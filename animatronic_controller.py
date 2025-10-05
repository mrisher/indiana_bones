import threading
import time

import serial

ANIMATRONIC_PORT = "/dev/tty.IndianaBones"


class AnimatronicController:
    def __init__(self, port, baudrate=9600):
        self.port = port
        self.baudrate = baudrate
        self.serial_connection = None
        self.is_connected = False
        self.read_thread = None

    def connect(self):
        """
        Establishes a serial connection to the animatronic.
        """
        print(f"Attempting to connect to '{self.port}'...")
        try:
            self.serial_connection = serial.Serial(self.port, self.baudrate, timeout=1)
            self.is_connected = True
            # Give the connection a moment to establish
            time.sleep(2)
            self.read_thread = threading.Thread(target=self._read_data)
            self.read_thread.daemon = True
            self.read_thread.start()
            print(f"Successfully connected to '{self.port}'.")
            return True
        except serial.SerialException as e:
            print(f"Failed to connect to '{self.port}'. Is it paired and in range?")
            print(f"Error: {e}")
            self.serial_connection = None
            return False

    def disconnect(self):
        """
        Closes the serial connection.
        """
        if self.serial_connection and self.serial_connection.is_open:
            self.is_connected = False
            self.serial_connection.close()
            print("Connection closed.")
        self.serial_connection = None

    def send_command(self, command):
        """
        Sends a command to the animatronic.
        """
        if self.serial_connection and self.serial_connection.is_open:
            print(f"Sending: '{command}'")
            self.serial_connection.write((command + "\n").encode())
        else:
            print("Not connected. Cannot send command.")

    def _read_data(self):
        """
        Reads data from the serial port in a separate thread.
        """
        while self.is_connected:
            try:
                raw_data = self.serial_connection.readline()
                if raw_data:
                    print(f"Received raw: {raw_data}")
                    data = raw_data.decode().strip()
                    if data:
                        self._handle_data_received(data)
            except serial.SerialException:
                # Connection lost
                self.is_connected = False
                print("Connection lost.")
                break
            except Exception as e:
                print(f"An error occurred while reading data: {e}")
                break

    def _handle_data_received(self, data):
        """
        Callback function to handle data received from the animatronic.
        """
        print(f"Received: '{data.strip()}'")


if __name__ == "__main__":
    controller = AnimatronicController(ANIMATRONIC_PORT)
    if controller.connect():
        try:
            controller.send_command("start")
            time.sleep(5)
            controller.send_command("mode scripted")
            time.sleep(5)
            controller.send_command("foo")
            time.sleep(5)
            controller.send_command("stop")
            time.sleep(5)
        finally:
            controller.disconnect()
