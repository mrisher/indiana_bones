import asyncio
import os
import base64
import webrtcvad
from pydub import AudioSegment
from datetime import datetime

import pyaudio
from bleak import BleakClient, BleakScanner
from google import genai
from google.genai.types import (
    Blob,
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
    vad = webrtcvad.Vad(3)  # Set aggressiveness mode (0-3)

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
                                "text": "You are a helpful and witty animatronic skull named Indiana Bones. Respond to all prompts in English with humor and personality."
                            }
                        ],
                    }
                ]
            )

            model_is_speaking = asyncio.Event()

            async def send_audio(model_is_speaking_event):
                state = "LISTENING"
                silent_frames = 0
                while True:
                    data = await asyncio.to_thread(
                        lambda: input_stream.read(CHUNK, exception_on_overflow=False)
                    )

                    # Do not process audio if the model is speaking
                    if model_is_speaking_event.is_set():
                        # Reset state to ensure we are ready to listen after the model finishes
                        state = "LISTENING"
                        await asyncio.sleep(0.1)
                        continue

                    is_speech = vad.is_speech(data, RATE)

                    if state == "LISTENING":
                        if is_speech:
                            log("Speech detected, starting to send.")
                            state = "SENDING"
                            silent_frames = 0
                            await session.send_realtime_input(
                                media={
                                    "mime_type": f"audio/pcm;rate={RATE}",
                                    "data": base64.b64encode(data).decode("utf-8"),
                                }
                            )

                    elif state == "SENDING":
                        if not is_speech:
                            silent_frames += 1
                        else:
                            silent_frames = 0

                        if silent_frames >= SILENCE_FRAMES:
                            log("Detected end of speech, signaling to model.")
                            await session.send_client_content(turn_complete=True)
                            state = "WAITING"  # Enter a waiting state
                            continue

                        await session.send_realtime_input(
                            media={
                                "mime_type": f"audio/pcm;rate={RATE}",
                                "data": base64.b64encode(data).decode("utf-8"),
                            }
                        )
                    elif state == "WAITING":
                        # While waiting, do nothing but check if the model has finished
                        if not model_is_speaking_event.is_set():
                            log("Model finished speaking. Resuming listening.")
                            state = "LISTENING"

            async def receive_audio(model_is_speaking_event):
                received_audio_chunks = []
                async for message in session.receive():
                    if message.data is not None:
                        if not model_is_speaking_event.is_set():
                            log("Model started speaking.")
                            model_is_speaking_event.set()
                            await controller.send_command("talk start")
                        received_audio_chunks.append(message.data)

                    if (
                        message.server_content is not None
                        and message.server_content.turn_complete
                    ):
                        if model_is_speaking_event.is_set():
                            if received_audio_chunks:
                                full_response = b"".join(received_audio_chunks)
                                output_stream.write(full_response)
                                received_audio_chunks = []

                            await controller.send_command("talk stop")
                            log("Model finished speaking.")
                            model_is_speaking_event.clear()

            async with asyncio.TaskGroup() as tg:
                tg.create_task(send_audio(model_is_speaking))
                tg.create_task(receive_audio(model_is_speaking))

    except Exception as e:
        log(f"An error occurred: {e}")
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
