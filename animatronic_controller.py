import asyncio
from bleak import BleakClient, BleakScanner

# UUIDs from the Arduino sketch
DEVICE_NAME = "IndianaBones"
SERVICE_UUID = "0b3a2666-6f1a-4262-9d6d-563a3d6a5867"
COMMAND_CHARACTERISTIC_UUID = "a5228043-8350-4d13-9842-11a050d7896c"
RESPONSE_CHARACTERISTIC_UUID = "1ea38cd0-6856-4f15-970a-3931b3b4a83d"


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
                # Set up notification handler for responses
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


async def main():
    """
    Main asynchronous function to run the controller.
    """
    controller = AnimatronicController()
    if await controller.connect():
        try:
            await controller.send_command("start")
            await asyncio.sleep(5)
            await controller.send_command("mode scripted")
            await asyncio.sleep(5)
            await controller.send_command("foo")
            await asyncio.sleep(5)
            await controller.send_command("stop")
            await asyncio.sleep(5)
        finally:
            await controller.disconnect()


if __name__ == "__main__":
    asyncio.run(main())