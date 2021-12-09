#include <Wire.h>

//
// Constants
//

// We use the built-in LED as a status indicator: we turn it on if the program panics
#define STATUS_LED_PIN 13

#define MAGNET_PIN 2

#define START_BUTTON_PIN 4
#define FUN_BUTTON_PIN 5
#define PLAYER_BUTTON_PIN 6
#define COMPUTER_BUTTON_PIN 7

#define CMD_LIGHTS 0b00
#define CMD_BUTTON_LIGHT 0b01
#define CMD_MAGNET 0b10

#define LOOPS_PER_UPDATE 500

//
// State Variables
//

bool magnet_enabled = false;
void setup()
{
	// Configure and turn off the panic LED, so we can see if we panic later on
	pinMode(STATUS_LED_PIN, OUTPUT);
	digitalWrite(STATUS_LED_PIN, LOW);

	// Setup the Serial interface
	Serial.begin(115200);
	Serial.setTimeout(50); // Make sure we don't spend too much time waiting for serial input

	// Configure pins
	pinMode(MAGNET_PIN, OUTPUT);
  	pinMode(START_BUTTON_PIN, INPUT);
  	pinMode(FUN_BUTTON_PIN, INPUT);
  	pinMode(PLAYER_BUTTON_PIN, INPUT);
  	pinMode(COMPUTER_BUTTON_PIN, INPUT);
}

int loops_since_update = 0;

void loop()
{
	// Check for Serial communication
	if (Serial.available())
	{
		uint16_t raw_cmd = Serial.parseInt();
		uint16_t cmd_type = raw_cmd & 0b11;
		uint16_t data = raw_cmd >> 2;

		switch (cmd_type)
		{
		case CMD_MAGNET:
			cmd_magnet(data);
			break;
		case CMD_BUTTON_LIGHT:
			cmd_button_light(data);
			break;
		case CMD_LIGHTS:
			cmd_lights(data);
			break;
		default:
			send_invalid_command();	
			break;
		}
		send_status();
	}

	loops_since_update++;
	if (loops_since_update >= LOOPS_PER_UPDATE) {
		loops_since_update = 0;
		send_status();
	}
}

void send_status() {
	Serial.print("GRANDMASTER:BOARD;START:");
	Serial.print(String(digitalRead(START_BUTTON_PIN)));
	Serial.print(";FUN:");
	Serial.print(String(digitalRead(FUN_BUTTON_PIN)));
	Serial.print(";COMPUTER:");
	Serial.print(String(digitalRead(COMPUTER_BUTTON_PIN)));
	Serial.print(";PLAYER:");
	Serial.print(String(digitalRead(PLAYER_BUTTON_PIN)));
	Serial.println("");
}

void send_invalid_command() {
	Serial.println("ERROR:INVALID_COMMAND");
}

/**
 * Magnet Command Format:
 * 0bA10
 * Where A is 1 to turn the magnet on, or 0 to turn it off.
 */
void cmd_magnet(uint16_t data) {
	switch (data) {
		case 0:
			digitalWrite(MAGNET_PIN, LOW);
			break;
		case 1:
			digitalWrite(MAGNET_PIN, HIGH);
			break;
		default:
			send_invalid_command();
	}
}

/**
 * Button Light Command Format:
 * 0bABB01
 * Where A is the action you want (1 for on, 0 for off), and BB is the offset of the LED (9 + BB) is
 * the pin to use.
 */
void cmd_button_light(uint16_t data) {
  uint8_t idx = data & 0b011;
  bool enabled = data & 0b100;
  digitalWrite(9 + idx, enabled ? HIGH : LOW);
}

void cmd_lights(uint16_t data) {

}
