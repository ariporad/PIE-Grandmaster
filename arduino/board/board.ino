#include <Wire.h>

//
// Constants
//

// We use the built-in LED as a status indicator: we turn it on if the program panics
#define STATUS_LED_PIN 13

#define MAGNET_PIN 2

// There are 4 buttons, which sensors/LEDs are 4 or 9 +0, 1, 2, 3
#define BUTTON_PIN_START 9
#define BUTTON_LED_START 4
#define NUM_BUTTONS 4

#define CMD_LEDS 0b11
#define CMD_BUTTON_LIGHT 0b01
#define CMD_MAGNET 0b10

#define UPDATE_INTERVAL_MS 100

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
	for (int i = 0; i < NUM_BUTTONS; i++) {
		pinMode(BUTTON_PIN_START + i, INPUT);
		pinMode(BUTTON_LED_START + i, OUTPUT);
		digitalWrite(BUTTON_LED_START + i, LOW);
	}

	pinMode(MAGNET_PIN, OUTPUT);
    digitalWrite(MAGNET_PIN, LOW);
	
	led_setup();

	send_status();
}

unsigned long last_update_time = 0;

void loop()
{
	// Check for Serial communication
	if (Serial.available()) read_command();

	check_buttons();
	led_update();

	if (millis() - last_update_time >= UPDATE_INTERVAL_MS) {
		send_status();
		last_update_time = millis();
	}
}

void read_command() {
	uint16_t raw_cmd = Serial.parseInt();
		
	if (raw_cmd == 0) return;

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
	case CMD_LEDS:
		cmd_leds(data);
		break;
	default:
		send_invalid_command();	
		break;
	}
	send_status();
}

bool last_button_values[NUM_BUTTONS];

void check_buttons() {
	bool need_to_send_update = false;
	for (int i = 0; i < NUM_BUTTONS; i++) {
		bool cur_button_value = digitalRead(BUTTON_PIN_START + i);
		if (cur_button_value != last_button_values[i]) {
			last_button_values[i] = cur_button_value;
			need_to_send_update = true;
		}
	}
	if (need_to_send_update) send_status();
}

void send_status() {
	// 0b100M1234
	// Where M is magnet state, and 1234 is the state of each button
	uint8_t update = 0b10000000;
	for (int i = 0; i < NUM_BUTTONS; i++) {
		update |= (last_button_values[i] << i);
	}
	update |= digitalRead(MAGNET_PIN) << 4;
	Serial.write(update);
}

void send_invalid_command() {
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
			return;
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
  digitalWrite(BUTTON_LED_START + idx, enabled ? HIGH : LOW);
}

/**
 * Fancy LEDs
 */
#include <FastLED.h>

#define LED_PIN 3
#define NUM_LEDS 50
#define LED_BRIGHTNESS 64
#define LED_TYPE WS2811
#define LED_COLOR_ORDER RGB
CRGB leds[NUM_LEDS];

#define LED_UPDATE_INTERVAL_MS 10

CRGBPalette16 led_palette;
TBlendType led_blending = LINEARBLEND;
uint8_t led_speed = 1;
uint8_t led_max_brightness = 255;
uint8_t led_blink_time = 100;
uint8_t led_blink_brightness = 150;
uint8_t led_time_to_next_blink = led_blink_time;

const uint32_t LED_COMPUTER_COLOR = 0x6666FF;
const uint32_t LED_HUMAN_COLOR = CRGB::HotPink;

extern const TProgmemPalette16 led_palette_both_players PROGMEM;

void led_setup()
{
	delay(3000); // power-up safety delay
	FastLED.addLeds<LED_TYPE, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
	FastLED.setBrightness(LED_BRIGHTNESS);
	led_set_pallete_bootup();
	led_apply_palette(0, led_max_brightness);
	FastLED.show();
}

void cmd_leds(uint8_t pallete_id)
{
	led_set_pallete(pallete_id);
	led_update();
}

uint8_t led_position = 0;
unsigned long last_led_update = 0;
void led_update()
{
	if (millis() - last_led_update < LED_UPDATE_INTERVAL_MS) return;

	last_led_update = millis();
	led_position = led_position + led_speed;

	uint8_t brightness = led_max_brightness;

	if (led_time_to_next_blink == 0) {
		led_time_to_next_blink = led_blink_time;
	}
	if (led_time_to_next_blink-- >= (led_blink_time / 2)) {
		brightness -= led_blink_brightness;
	}

	led_apply_palette(led_position, brightness);

	FastLED.show();
}

void led_apply_palette(uint8_t colorIndex, uint8_t brightness)
{
	for (int i = 0; i < NUM_LEDS; ++i)
	{
		leds[i] = ColorFromPalette(led_palette, colorIndex, brightness, led_blending);
		colorIndex += 3;
	}
}

void led_set_animation(int speed, int new_blink_time, int blink_intensity)
{
	led_speed = speed;
	led_max_brightness = 255;
	led_blink_time = new_blink_time;
	led_blink_brightness = blink_intensity;
	led_time_to_next_blink = new_blink_time;
}

void led_set_animation_blink()
{
	led_set_animation(0, 100, 100);
}

void led_set_animation_spin()
{
	led_set_animation(1, 0, 0);
}

void led_set_animation_blink_and_spin()
{
	led_set_animation(1, 100, 100);
}

void led_set_pallete(uint8_t id) {
	switch (id)
	{
	case 0: return led_set_pallete_fail();
	case 1: return led_set_pallete_bootup();
	case 2: return led_set_pallete_getting_ready();
	case 3: return led_set_pallete_ready();
	case 4: return led_set_pallete_human_turn();
	case 5: return led_set_pallete_expo_human_think();
	case 6: return led_set_pallete_computer_move();
	case 7: return led_set_pallete_computer_think();
	default:
		break;
	}
}

void led_set_pallete_fail() // ID: 0
{
	led_set_animation_blink();
	fill_solid(led_palette, 16, CRGB::Red);
}

void led_set_pallete_bootup() // ID: 1
{
	led_set_animation_blink_and_spin();
	led_palette = RainbowColors_p;
}

void led_set_pallete_getting_ready() // ID: 2
{
	led_set_animation_spin();
	led_palette = RainbowStripeColors_p;
}

void led_set_pallete_ready() // ID: 3
{
	led_set_animation_spin();
	led_palette = led_palette_both_players;
}

void led_set_pallete_human_turn() // ID: 4
{
	led_set_animation_spin();
	fill_solid(led_palette, 16, LED_HUMAN_COLOR);
	led_palette[0] = CRGB::Black;
	led_palette[1] = CRGB::Black;
	led_palette[4] = CRGB::Black;
	led_palette[5] = CRGB::Black;
	led_palette[8] = CRGB::Black;
	led_palette[9] = CRGB::Black;
	led_palette[12] = CRGB::Black;
	led_palette[13] = CRGB::Black;
}

void led_set_pallete_expo_human_think() // ID: 5
{
	led_set_animation_blink();
	fill_solid(led_palette, 16, LED_HUMAN_COLOR);
}

void led_set_pallete_computer_move() // ID: 6
{
	led_set_animation_spin();
	fill_solid(led_palette, 16, LED_COMPUTER_COLOR);
	led_palette[0] = CRGB::Black;
	led_palette[1] = CRGB::Black;
	led_palette[4] = CRGB::Black;
	led_palette[5] = CRGB::Black;
	led_palette[8] = CRGB::Black;
	led_palette[9] = CRGB::Black;
	led_palette[12] = CRGB::Black;
	led_palette[13] = CRGB::Black;
}

void led_set_pallete_computer_think() // ID: 7
{
	led_set_animation_blink();
	fill_solid(led_palette, 16, LED_COMPUTER_COLOR);
}

const TProgmemPalette16 led_palette_both_players PROGMEM =
	{
		LED_COMPUTER_COLOR,
		LED_COMPUTER_COLOR,
		CRGB::Black,
		CRGB::Black,
		LED_HUMAN_COLOR,
		LED_HUMAN_COLOR,
		CRGB::Black,
		CRGB::Black,

		LED_COMPUTER_COLOR,
		LED_COMPUTER_COLOR,
		CRGB::Black,
		CRGB::Black,
		LED_HUMAN_COLOR,
		LED_HUMAN_COLOR,
		CRGB::Black,
		CRGB::Black
	};