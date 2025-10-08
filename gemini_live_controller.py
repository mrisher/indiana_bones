import asyncio
import audioop
import os
import base64

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

# --- Animatronic Controller Configuration ---
DEVICE_NAME = "IndianaBones"
SERVICE_UUID = "0b3a2666-6f1a-4262-9d6d-563a3d6a5867"
COMMAND_CHARACTERISTIC_UUID = "a5228043-8350-4d13-9842-11a050d7896c"
RESPONSE_CHARACTERISTIC_UUID = "1ea38cd0-6856-4f15-970a-3931b3b4a83d"

# --- Gemini Live and Audio Configuration ---
# Make sure to set your GOOGLE_API_key environment variable
# pip install google-generativeai pyaudio
MODEL_ID = "gemini-live-2.5-flash-preview"  # Correct model ID
AUDIO_FORMAT = pyaudio.paInt16
CHANNELS = 1
RATE = 16000  # Sample rate for input audio
CHUNK = 1024 * 2
SILENCE_CHUNKS = 20  # Number of consecutive silent chunks to detect end of speech


class AnimatronicController:
    def __init__(self):
        self.client = None
        self.is_connected = False

    async def connect(self):
        """
        Scans for the animatronic device and establishes a BLE connection.
        """
        print(f"Scanning for '{DEVICE_NAME}'...")
        device = await BleakScanner.find_device_by_name(DEVICE_NAME)
        if device is None:
            print(f"Could not find device with name '{DEVICE_NAME}'.")
            return False

        print(f"Connecting to '{device.name}' ({device.address})...")
        self.client = BleakClient(device)
        try:
            await self.client.connect()
            self.is_connected = self.client.is_connected
            if self.is_connected:
                print("Successfully connected.")
                await self.client.start_notify(
                    RESPONSE_CHARACTERISTIC_UUID, self._handle_data_received
                )
                return True
            else:
                print("Failed to connect.")
                return False
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False

    async def disconnect(self):
        """
        Closes the BLE connection.
        """
        if self.client and self.client.is_connected:
            self.is_connected = False
            await self.client.disconnect()
            print("Connection closed.")
        self.client = None

    async def send_command(self, command):
        """
        Sends a command to the animatronic via BLE.
        """
        if self.client and self.client.is_connected:
            print(f"Sending: '{command}'")
            await self.client.write_gatt_char(
                COMMAND_CHARACTERISTIC_UUID, (command + "\n").encode()
            )
        else:
            print("Not connected. Cannot send command.")

    def _handle_data_received(self, sender, data):
        """
        Callback function to handle data received from the animatronic.
        """
        print(f"Received: '{data.decode().strip()}'")


async def gemini_live_interaction(controller):
    """
    Handles the Gemini Live API interaction, including sending and receiving audio,
    and controlling the animatronic based on voice activity.
    """
    api_key = os.getenv("GEMINI_API_KEY")
    if not api_key:
        print("Error: GEMINI_API_KEY environment variable not set.")
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
        print("Connecting to Gemini Live...")
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
            print("Successfully connected to Gemini Live. Speak now...")

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

            print(f"Session methods: {dir(session)}")

            async def send_audio():
                silent_chunks = 0
                while True:
                    data = await asyncio.to_thread(
                        lambda: input_stream.read(CHUNK, exception_on_overflow=False)
                    )

                    rms = audioop.rms(data, 2)
                    if rms < 500:
                        silent_chunks += 1
                    else:
                        silent_chunks = 0

                    if silent_chunks >= SILENCE_CHUNKS:
                        print("Detected end of speech, signaling to model.")
                        await session.send_client_content(turn_complete=True)
                        silent_chunks = 0  # Reset after sending
                        continue

                    print(f"Sending {len(data)} bytes of audio...")
                    await session.send_realtime_input(
                        media={
                            "mime_type": f"audio/pcm;rate={RATE}",
                            "data": base64.b64encode(data).decode("utf-8"),
                        }
                    )

            async def receive_audio():
                is_talking_this_turn = False
                async for message in session.receive():
                    print(f"Received message: {message}")
                    if message.data is not None:
                        if not is_talking_this_turn:
                            is_talking_this_turn = True
                            await controller.send_command("talk start")
                        output_stream.write(message.data)

                    if (
                        message.server_content is not None
                        and message.server_content.turn_complete
                    ):
                        if is_talking_this_turn:
                            await controller.send_command("talk stop")
                            is_talking_this_turn = False

            async with asyncio.TaskGroup() as tg:
                tg.create_task(send_audio())
                tg.create_task(receive_audio())

    except Exception as e:
        print(f"An error occurred: {e}")
        import traceback
        traceback.print_exc()
    finally:
        print("Closing streams.")
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
