#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"

#include "ssd1306.h"

// velocidade de atualização do loop principal
#define UPDATE_RATE_HZ 30
#define UPDATE_TIME_MS (1000.0f / UPDATE_RATE_HZ)

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

static volatile int val = 0;
static ssd1306_t display;

static void die(const char *msg);
static bool main_loop(struct repeating_timer *_);

int main(void) {
	stdio_init_all();

	i2c_init(DISPLAY_I2C_PORT, 400000); // 400KHz
	gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(DISPLAY_SDA_PIN);
	gpio_pull_up(DISPLAY_SCL_PIN);

	if (!ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, DISPLAY_I2C_PORT))
		die("falha ao inicializar o display OLED");

	// limpar a tela
	ssd1306_fill(&display, 0);
	ssd1306_send_data(&display);

	struct repeating_timer timer;
	add_repeating_timer_ms(UPDATE_TIME_MS, main_loop, NULL, &timer);

	while (true) {
		sleep_ms(10000);
	}

	return 0;
}

static void die(const char *msg) {
	while (true) {
		printf("ERRO FATAL: %s\n", msg);
		sleep_ms(2000);
	}
}

static bool main_loop(struct repeating_timer *_) {
	printf("HELLO %d\n", val++);
}
