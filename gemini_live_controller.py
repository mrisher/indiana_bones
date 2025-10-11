import asyncio
import os
import base64
from datetime import datetime

import librosa
import numpy as np
import pyaudio
from bleak import BleakClient, BleakScanner
from google import genai
from google.genai.types import (
    LiveConnectConfig,
    Modality,
    PrebuiltVoiceConfig,
    SpeechConfig,
    VoiceConfig,
)

def log(message):
    """Prints a message with a timestamp."""
    print(f"[{datetime.now().isoformat()}] {message}")


# --- Animatronic Controller Configuration ---
DEVICE_NAME = "IndianaBones"
SERVICE_UUID = "0b3a2666-6f1a-4262-9d6d-563a3d6a5867"
COMMAND_CHARACTERISTIC_UUID = "a5228043-8350-4d13-9842-11a050d7896c"
RESPONSE_CHARACTERISTIC_UUID = "1ea38cd0-6856-4f15-970a-3931b3b4a83d"

# --- Gemini Live and Audio Configuration ---
# Make sure to set your GOOGLE_API_key environment variable
# pip install google-generativeai pyaudio webrtcvad pydub
MODEL_ID = "gemini-live-2.5-flash-preview"  # Correct model ID
AUDIO_FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 16000  # Sample rate for input audio (must be 8000, 16000, 32000, or 48000 for webrtcvad)
FRAME_DURATION_MS = 30  # Duration of each audio frame in ms
CHUNK = int(RATE * FRAME_DURATION_MS / 1000)  # Number of bytes in each audio frame
SILENCE_FRAMES = 50  # Number of consecutive silent frames to detect end of speech


class AnimatronicController:
    def __init__(self):
        self.client = None
        self.is_connected = False

    async def connect(self):
        """
        Scans for the animatronic device and establishes a BLE connection.
        """
        log(f"Scanning for '{DEVICE_NAME}'...")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME)
        if device is None:
            log(f"Could not find device with name '{DEVICE_NAME}'.")
            return False

        log(f"Connecting to '{device.name}' ({device.address})...")
        self.client = BleakClient(device)
        try:
            await self.client.connect()
            self.is_connected = self.client.is_connected
            if self.is_connected:
                log("Successfully connected.")
                await self.client.start_notify(
                    RESPONSE_CHARACTERISTIC_UUID, self._handle_data_received
                )
                return True
            else:
                log("Failed to connect.")
                return False
        except Exception as e:
            log(f"Failed to connect: {e}")
            return False

    async def disconnect(self):
        """
        Closes the BLE connection.
        """
        if self.client and self.client.is_connected:
            self.is_connected = False
            await self.client.disconnect()
            log("Connection closed.")
        self.client = None

    async def send_command(self, command):
        """
        Sends a command to the animatronic via BLE.
        """
        if self.client and self.client.is_connected:
            log(f"Sending: '{command}'")
            await self.client.write_gatt_char(
                COMMAND_CHARACTERISTIC_UUID, (command + "\n").encode()
            )
        else:
            log("Not connected. Cannot send command.")

    def _handle_data_received(self, sender, data):
        """
        Callback function to handle data received from the animatronic.
        """
        log(f"Received: '{data.decode().strip()}'")


async def gemini_live_interaction(controller):
    """
    Handles the Gemini Live API interaction, including sending and receiving audio,
    and controlling the animatronic based on voice activity.
    """
    api_key = os.getenv("GEMINI_API_KEY")
    if not api_key:
        log("Error: GEMINI_API_KEY environment variable not set.")
        return

    client = genai.Client(api_key=api_key)
    audio = pyaudio.PyAudio()

    input_stream = audio.open(
        format=AUDIO_FORMAT,
        channels=CHANNELS,
        rate=RATE,
        input=True,
        frames_per_buffer=CHUNK,
    )

    output_stream = audio.open(
        format=AUDIO_FORMAT, channels=CHANNELS, rate=24000, output=True
    )

    try:
        log("Connecting to Gemini Live...")
        async with client.aio.live.connect(
            model=MODEL_ID,
            config=LiveConnectConfig(
                response_modalities=[Modality.AUDIO],
                speech_config=SpeechConfig(
                    language_code="en-US",
                    voice_config=VoiceConfig(
                        prebuilt_voice_config=PrebuiltVoiceConfig(voice_name="Charon")
                    ),
                ),
            ),
        ) as session:
            log("Successfully connected to Gemini Live. Speak now...")

            # Send the initial context message
            await session.send_client_content(
                turns=[
                    {
                        "role": "user",
                        "parts": [
                            {
                                "text": "You are a spooky animatronic skull named Indiana Bones. Add menacing laughs -- 'HaHaHa' -- after your responses."
                            }
                        ],
                    }
                ]
            )

            model_is_speaking = asyncio.Event()
            audio_in_queue = asyncio.Queue()

            async def send_audio():
                while True:
                    data = await asyncio.to_thread(
                        lambda: input_stream.read(CHUNK, exception_on_overflow=False)
                    )
                    await session.send_realtime_input(
                        media={
                            "mime_type": f"audio/pcm;rate={RATE}",
                            "data": base64.b64encode(data).decode("utf-8"),
                        }
                    )

            async def receive_audio(model_is_speaking_event, queue):
                while True:
                    async for message in session.receive():
                        if message.data is not None:
                            if not model_is_speaking_event.is_set():
                                log("Model started speaking.")
                                model_is_speaking_event.set()
                                await controller.send_command("talk start")
                            queue.put_nowait(message.data)

                        if (
                            message.server_content is not None
                            and message.server_content.turn_complete
                        ):
                            if model_is_speaking_event.is_set():
                                # Clear the queue on interruption or completion
                                while not queue.empty():
                                    queue.get_nowait()
                                await controller.send_command("talk stop")
                                log("Model finished speaking.")
                                model_is_speaking_event.clear()

            async def play_audio(queue):
                while True:
                    chunk = await queue.get()
                    # Convert raw audio bytes to numpy array
                    audio_int16 = np.frombuffer(chunk, dtype=np.int16)

                    # Convert to float32 for librosa
                    audio_float = audio_int16.astype(np.float32) / 32768.0

                    # Pitch shift down by 5 semitones
                    y_shifted = librosa.effects.pitch_shift(
                        y=audio_float, sr=24000, n_steps=-5
                    )

                    # # Slow down by 10%
#                     y_slowed = librosa.effects.time_stretch(y=y_shifted, rate=0.9)

                    # Convert back to int16
                    audio_shifted_int16 = (y_shifted * 32767).astype(np.int16)

                    # Convert back to bytes
                    modified_chunk = audio_shifted_int16.tobytes()

                    await asyncio.to_thread(output_stream.write, modified_chunk)

            async with asyncio.TaskGroup() as tg:
                tg.create_task(send_audio())
                tg.create_task(receive_audio(model_is_speaking, audio_in_queue))
                tg.create_task(play_audio(audio_in_queue))

    except Exception as e:
        log(f"An error occurred: {e}")
        import traceback
        traceback.print_exc()
    finally:
        log("Closing streams.")
        if input_stream.is_active():
            input_stream.stop_stream()
        input_stream.close()
        if output_stream.is_active():
            output_stream.stop_stream()
        output_stream.close()
        audio.terminate()


async def main():
    """
    Main asynchronous function to run the controller and Gemini Live interaction.
    """
    controller = AnimatronicController()
    if await controller.connect():
        try:
            await gemini_live_interaction(controller)
        finally:
            await controller.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Exiting.")
