
import asyncio
from bleak import BleakScanner, BleakClient

# Use the name and address from your original script
ANIMATRONIC_NAME = "IndianaBones"
ANIMATRONIC_ADDRESS = "80:F3:DA:42:EF:36" # This might be a Classic BT address, scanning is more reliable

async def main():
    print(f"Scanning for '{ANIMATRONIC_NAME}'...")
    device = await BleakScanner.find_device_by_name(ANIMATRONIC_NAME, timeout=20.0)

    if not device:
        print(f"Could not find a device named '{ANIMATRONIC_NAME}'.")
        print("Trying to connect by address...")
        try:
            device = await BleakScanner.find_device_by_address(ANIMATRONIC_ADDRESS, timeout=20.0)
        except Exception as e:
            print(f"Finding by address failed: {e}")
            device = None

    if not device:
        print(f"Failed to find device by name or address. Please ensure it's on and discoverable.")
        return

    print(f"Found device: {device.name} ({device.address})")
    print("Connecting and discovering services...")

    async with BleakClient(device) as client:
        if client.is_connected:
            print("Successfully connected. Services and characteristics:")
            for service in client.services:
                print(f"\n[Service] {service.uuid}: {service.description}")
                for char in service.characteristics:
                    print(f"  [Characteristic] {char.uuid}: {char.description}")
                    print(f"    Properties: {', '.join(char.properties)}")
        else:
            print("Failed to connect.")

if __name__ == "__main__":
    asyncio.run(main())
