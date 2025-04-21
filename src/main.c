#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

#include "ssd1306.h"

#define JOYSTICK_X_PIN 27
#define JOYSTICK_X_INPUT 1

#define JOYSTICK_Y_PIN 26
#define JOYSTICK_Y_INPUT 0

#define JOYSTICK_CORRECTION -0.1 // evitar flicker na luz quando o joystick não está sendo usado
#define JOYSTICK_WRAP 4096

// velocidade de atualização do loop principal
#define UPDATE_RATE_HZ 20
#define UPDATE_TIME_MS (1000.0f / UPDATE_RATE_HZ)

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

static volatile int val = 0;
static ssd1306_t display;

static bool main_loop(struct repeating_timer *_);
static void die(const char *msg);
static float get_axis_normalized(uint16_t axis_value);
static float calc_duty_cycle(uint16_t axis_value);

int main(void) {
	stdio_init_all();

	adc_init();
	adc_gpio_init(JOYSTICK_X_PIN);
	adc_gpio_init(JOYSTICK_Y_PIN);

	i2c_init(DISPLAY_I2C_PORT, 400000); // 400KHz
	gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(DISPLAY_SDA_PIN);
	gpio_pull_up(DISPLAY_SCL_PIN);

	if (!ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, DISPLAY_I2C_PORT))
		die("falha ao inicializar o display OLED");

	struct repeating_timer timer;
	add_repeating_timer_ms(UPDATE_TIME_MS, main_loop, NULL, &timer);

	while (true) {
		sleep_ms(10000);
	}

	return 0;
}

static bool main_loop(struct repeating_timer *_) {
	ssd1306_fill(&display, 0);

	adc_select_input(JOYSTICK_X_INPUT);
	float x_axis = get_axis_normalized(adc_read());

	adc_select_input(JOYSTICK_Y_INPUT);
	float y_axis = get_axis_normalized(adc_read());

	uint16_t x_mid = DISPLAY_WIDTH / 2 + x_axis * 15;
	uint16_t y_mid = DISPLAY_HEIGHT / 2 - y_axis * 15;
	ssd1306_rect(&display, y_mid - 4, x_mid - 4, 8, 8, 1, true);
	ssd1306_send_data(&display);

	return true;
}

static void die(const char *msg) {
	while (true) {
		printf("ERRO FATAL: %s\n", msg);
		sleep_ms(2000);
	}
}

static float get_axis_normalized(uint16_t axis_value) {
	int32_t middle_centered = (int32_t)axis_value - 2048;
	return (float)middle_centered / 2048.0f;
}

static float calc_duty_cycle(uint16_t axis_value) {
	float normalized = get_axis_normalized(axis_value);
	float absolute = fabsf(normalized);
	float corrected = (absolute + JOYSTICK_CORRECTION) / (1.0f + JOYSTICK_CORRECTION);
	return fmaxf(corrected, 0.0f);
}
