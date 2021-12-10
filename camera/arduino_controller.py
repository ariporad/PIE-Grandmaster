from typing import *
from serial import Serial
from enum import IntEnum
import serial.tools.list_ports

GANTRY_ARDUINO_SERIAL_NUMBER = "85033313237351301221"
BOARD_ARDUINO_SERIAL_NUMBER = "8503331323735140D1D0"

class Arduino:
	name: str
	serial: Serial

	def __init__(self, name: str, serial_number: str,  baudrate=115200):
		self.name = name.upper()
		
		found_arduino = False
		for device in serial.tools.list_ports.comports():
			if device.serial_number is not None and device.serial_number.upper() == serial_number.upper():
				self.serial = Serial(device.device, baudrate=baudrate, timeout=0.1)
		
		if not found_arduino:
			raise IOError(f"Couldn't find Arduino! (Name: {name}, SN: {serial_number}")

	def write(self, data: int):
		self.serial.write(bytes(str(data), 'utf-8'))
		self.serial.flush()

	def get_messages(self, max=None) -> Iterator[Dict]:
		"""
		Read, parse, and return any pending messages from the serial port.
		"""
		while max is None or max > 0:
			line = self.serial.readline()
			if line is None:
				break
			line = line.strip()
			if line == "":
				break
			msg = self.parse_message(line)
			if msg['TYPE'] == 'ANNOUNCEMENT':
				if msg['NAME'].upper() != self.name.upper():
					print(f"WARNING: Arduino '{self.name}' got announcement for name '{msg['NAME']}")
			else:
				if max is not None:
					max -= 1
				yield msg

	def parse_message(self, msg: str):
		"""
		Messages from the Arduino are ascii-encoded, key-value pairs.
		Format:
		> KEY:VALUE;KEY:VALUE
		Over the serial connection, they are newline-deliminated.
		Keys may only contain uppercase letters. Values may contain either (but not both):
		a) uppercase letters and underscores, OR
		b) digits 0-9
		"""
		data = {}
		for pair in msg.upper().split(' '):
			key, value = pair.split(':')
			# attempt to convert value to an int
			try:
				value = int(value)
			except ValueError:
				pass # just use the string value
			data[key] = value
		if 'TYPE' not in data:
			print(f"WARNING(Arduino:{self.name}): Illegal message received:", data)
		return data


class LightMode(IntEnum):
	OFF = 0
	DEFAULT = 1

class Button(IntEnum):
	COMPUTER = 0
	PLAYER = 1
	START = 2
	FUN = 3

class ArduinoController:
	gantry: Arduino
	primary: Arduino

	buttons: Dict[Button, bool]
	gantry_pos: Tuple[int, int] = (0, 0)
	electromagnet_enabled: bool = False

	def __init__(self):
		self.gantry = Arduino("GANTRY", GANTRY_ARDUINO_SERIAL_NUMBER)
		self.board = Arduino("BOARD", BOARD_ARDUINO_SERIAL_NUMBER)
		self.buttons = {button: False for button in Button}
	
	def move_to_square(self, rank: int, file: int):
		"""
		File is accepted as an integer for simplicity, and to allow accessing the graveyard: The
		normal files (A-H) are 0-7, respectively, and the graveyard is files 8-9.
		"""
		self.gantry.write((file << 2) | rank)
	
	def set_electromagnet(self, enabled: bool):
		self.board.write(0b110 if enabled else 0b010)

	def set_light_mode(self, mode: LightMode):
		print("Light Modes Not Implemented, set to:", mode)

	def set_button_light(self, button: Button, enabled: bool):
		self.board.write((int(enabled) << 4) | (button << 2) | 0b01)

	def tick(self):
		for message in self.board.get_messages():
			if message['TYPE'] == 'BUTTON_PRESS':
				self.buttons[Button[message['BUTTON']]] = bool(message['PRESSED'])
			elif message['TYPE'] == 'MAGNET_STATUS':
				self.electromagnet_enabled = bool(message['ENABLED'])
			elif message['TYPE'] == 'STATUS':
				self.electromagnet_enabled = bool(message['MAGNET'])
				for button in Button:
					self.buttons[button] = bool(message[f"BUTTON{button}"]) or self.buttons[button]
			else:
				print("Got unknown message from board:", message)

		for message in self.gantry.get_messages():
			if message['TYPE'] == 'POSITION':
				self.gantry = (message['X'], message['Y'])
			else:
				print("Got unknown message from gantry:", message)





