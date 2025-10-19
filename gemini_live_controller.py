import argparse
import asyncio
import base64
import os
import queue
from datetime import datetime
from functools import partial

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
from stftpitchshift import StftPitchShift


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
OUTPUT_RATE = 24000  # Gemini Live API output sample rate
FRAME_DURATION_MS = 30  # Duration of each audio frame in ms
CHUNK = int(RATE * FRAME_DURATION_MS / 1000)  # Number of bytes in each audio frame
SILENCE_FRAMES = 50  # Number of consecutive silent frames to detect end of speech
PITCH_SHIFT_RATIO = 0.76
TIMBRE_SHIFT_RATIO = 0.9
QUEFRENCY_SECONDS = 0.001
TIME_STRETCH_FACTOR = 0.8
SILENCE_THRESHOLD = 0.01  # RMS threshold for silence detection on float32 audio
PAUSE_FRAMES_THRESHOLD = 20  # Number of consecutive silent frames to trigger a pause


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


class DummyAnimatronicController:
    """A mock controller that simulates the AnimatronicController for testing without a BLE device."""

    def __init__(self):
        self.is_connected = False

    async def connect(self):
        log("Running in --no-ble mode. Skipping BLE connection.")
        self.is_connected = True
        return True

    async def disconnect(self):
        log("Running in --no-ble mode. Skipping BLE disconnection.")
        self.is_connected = False

    async def send_command(self, command):
        log(f"Running in --no-ble mode. Mock sending command: '{command}'")


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
        format=AUDIO_FORMAT,
        channels=CHANNELS,
        rate=OUTPUT_RATE,
        output=True,
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
            first_audio_played = asyncio.Event()
            audio_in_queue = queue.Queue()
            animatronic_state_queue = asyncio.Queue()
            loop = asyncio.get_running_loop()

            # Initialize the pitch shifter
            pitch_shifter = StftPitchShift(
                framesize=1024, hopsize=256, samplerate=OUTPUT_RATE
            )

            async def send_audio(model_is_speaking_event):
                while True:
                    data = await asyncio.to_thread(
                        lambda: input_stream.read(CHUNK, exception_on_overflow=False)
                    )
                    if not model_is_speaking_event.is_set():
                        await session.send_realtime_input(
                            media={
                                "mime_type": f"audio/pcm;rate={RATE}",
                                "data": base64.b64encode(data).decode("utf-8"),
                            }
                        )

            async def receive_audio(
                model_is_speaking_event,
                queue,
                first_audio_played_event,
                anim_queue,
            ):
                while True:
                    async for message in session.receive():
                        if message.data is not None:
                            if not model_is_speaking_event.is_set():
                                log("Model started speaking.")
                                model_is_speaking_event.set()
                            queue.put(message.data)

                        if (
                            message.server_content is not None
                            and message.server_content.turn_complete
                        ):
                            if model_is_speaking_event.is_set():
                                # Clear the queue on interruption or completion
                                while not queue.empty():
                                    queue.get_nowait()
                                while not anim_queue.empty():
                                    anim_queue.get_nowait()
                                await controller.send_command("talk stop")
                                log("Model finished speaking.")
                                model_is_speaking_event.clear()
                                first_audio_played_event.clear()

            def play_audio(
                queue,
                stream,
                pitch_shifter,
                first_audio_played_event,
                loop,
                anim_queue,
            ):
                """
                Plays audio chunks from a queue, applying pitch shifting and time stretching
                using the correct Overlap-Add (OLA) method for smooth, glitch-free audio.
                Note: Time stretching with this method might introduce some audible artifacts
                as it doesn't perform phase locking.
                """
                # Buffers for OLA processing, using float32 for audio processing
                input_buffer = np.array([], dtype=np.float32)
                output_buffer = np.zeros(pitch_shifter.framesize, dtype=np.float32)
                is_currently_silent = False
                silence_frames_count = 0

                analysis_hopsize = pitch_shifter.hopsize
                # To slow down, synthesis hopsize must be smaller than analysis hopsize.
                synthesis_hopsize = int(analysis_hopsize / TIME_STRETCH_FACTOR)

                while True:
                    chunk_bytes = queue.get(block=True)
                    if chunk_bytes is None:
                        break

                    # 1. Accumulate new audio data (convert to float32)
                    new_chunk = (
                        np.frombuffer(chunk_bytes, dtype=np.int16).astype(np.float32)
                        / 32768.0
                    )
                    input_buffer = np.concatenate((input_buffer, new_chunk))

                    # 2. Process all available full frames
                    while len(input_buffer) >= pitch_shifter.framesize:
                        # a. Get the next frame
                        current_frame = input_buffer[: pitch_shifter.framesize]

                        # b. Perform pitch shifting (windowing is handled internally)
                        processed_frame = pitch_shifter.shiftpitch(
                            current_frame,
                            factors=PITCH_SHIFT_RATIO,
                            quefrency=QUEFRENCY_SECONDS,
                            distortion=TIMBRE_SHIFT_RATIO,
                            normalization=False,
                        )

                        # c. Add the processed frame to the output buffer (Overlap-Add)
                        output_buffer += processed_frame

                        # d. Send the first hop_size of the result to the speaker
                        output_segment_float = output_buffer[:synthesis_hopsize]

                        # Silence detection
                        rms = np.sqrt(np.mean(output_segment_float**2))
                        if rms < SILENCE_THRESHOLD:
                            silence_frames_count += 1
                            if (
                                silence_frames_count > PAUSE_FRAMES_THRESHOLD
                                and not is_currently_silent
                            ):
                                is_currently_silent = True
                                loop.call_soon_threadsafe(
                                    anim_queue.put_nowait, "PAUSE"
                                )
                        else:
                            if is_currently_silent:
                                is_currently_silent = False
                                loop.call_soon_threadsafe(
                                    anim_queue.put_nowait, "RESUME"
                                )
                            silence_frames_count = 0

                        output_segment_int = (
                            output_segment_float * 32768.0
                        ).astype(np.int16)
                        stream.write(output_segment_int.tobytes())
                        if not first_audio_played_event.is_set():
                            loop.call_soon_threadsafe(first_audio_played_event.set)

                        # e. Slide the output buffer left
                        output_buffer = np.roll(output_buffer, -synthesis_hopsize)
                        # Clear the end of the buffer that was just slid over
                        output_buffer[-synthesis_hopsize:] = 0.0

                        # f. Slide the input buffer left
                        input_buffer = input_buffer[analysis_hopsize:]

            async def manage_animatronic_talking(
                model_is_speaking_event,
                first_audio_played_event,
                anim_queue,
                controller,
            ):
                while True:
                    # Wait for the start of a new utterance
                    await model_is_speaking_event.wait()
                    await first_audio_played_event.wait()

                    # Check if the utterance is still active before starting
                    if model_is_speaking_event.is_set():
                        log("Starting animatronic for utterance.")
                        await controller.send_command("talk start")

                    # Manage pauses during the utterance
                    while model_is_speaking_event.is_set():
                        try:
                            # Wait for a state change command from play_audio
                            state = await asyncio.wait_for(
                                anim_queue.get(), timeout=0.1
                            )
                            if state == "PAUSE":
                                log("Pausing animatronic due to silence.")
                                await controller.send_command("talk stop")
                            elif state == "RESUME":
                                log("Resuming animatronic.")
                                await controller.send_command("talk start")
                            anim_queue.task_done()
                        except asyncio.TimeoutError:
                            # Timeout is fine, just means no state change
                            continue

            async with asyncio.TaskGroup() as tg:
                tg.create_task(send_audio(model_is_speaking))
                tg.create_task(
                    receive_audio(
                        model_is_speaking,
                        audio_in_queue,
                        first_audio_played,
                        animatronic_state_queue,
                    )
                )
                tg.create_task(
                    asyncio.to_thread(
                        partial(
                            play_audio,
                            audio_in_queue,
                            output_stream,
                            pitch_shifter,
                            first_audio_played,
                            loop,
                            animatronic_state_queue,
                        )
                    )
                )
                tg.create_task(
                    manage_animatronic_talking(
                        model_is_speaking,
                        first_audio_played,
                        animatronic_state_queue,
                        controller,
                    )
                )

    except Exception as e:
        log(f"An error occurred: {e}")
        import traceback

        traceback.print_exc()
    finally:
        log("Closing streams.")
        if "audio_in_queue" in locals():
            audio_in_queue.put(None)  # Signal the play_audio thread to exit
        if input_stream.is_active():
            input_stream.stop_stream()
        input_stream.close()
        if "output_stream" in locals() and output_stream.is_active():
            output_stream.stop_stream()
            output_stream.close()
        audio.terminate()


async def main():
    """
    Main asynchronous function to run the controller and Gemini Live interaction.
    """
    parser = argparse.ArgumentParser(description="Gemini Live Animatronic Controller")
    parser.add_argument(
        "--no-ble",
        action="store_true",
        help="Run without connecting to the BLE device.",
    )
    args = parser.parse_args()

    if args.no_ble:
        controller = DummyAnimatronicController()
    else:
        controller = AnimatronicController()

    if await controller.connect():
        try:
            await controller.send_command("start")
            await gemini_live_interaction(controller)
        finally:
            log("Exiting program.")
            await controller.send_command("stop")
            await controller.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Exiting.")
