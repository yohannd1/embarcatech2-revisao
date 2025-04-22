#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"

#include "ssd1306.h"
#include "ws2812b_matrix.h"
#include "buzzer.h"

#define LED_STRIP_PIN 7
#define BUZZER_PIN 21

#define BUTTON_A_PIN 5
#define BUTTON_B_PIN 6

#define MID_LED_B_PIN 12

#define JOYSTICK_X_PIN 27
#define JOYSTICK_X_INPUT 1

#define JOYSTICK_Y_PIN 26
#define JOYSTICK_Y_INPUT 0

#define JOYSTICK_CORRECTION -0.1

#define DISPLAY_SDA_PIN 14
#define DISPLAY_SCL_PIN 15
#define DISPLAY_I2C_PORT i2c1
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define DEBOUNCING_TIME_US 150000

// velocidade de atualização do loop principal
#define UPDATE_RATE_HZ 20
#define UPDATE_TIME_MS (1000.0f / UPDATE_RATE_HZ)

static ssd1306_t display;

static volatile _Atomic float joystick_x_axis = 0.0f;
static volatile _Atomic float joystick_y_axis = 0.0f;

static ws2812b_matrix_t matrix;
static ws2812b_buffer_t buffer;
static buzzer_t bz;
static volatile _Atomic int screen_updates_queued = 1;
static volatile _Atomic int cursor_x = 2;
static volatile _Atomic int cursor_y = 2;

static void on_press(uint gpio, uint32_t events);
static bool main_loop(struct repeating_timer *_);
static void die(const char *msg);
static float get_axis_normalized(uint16_t axis_value);
static void handle_toggle_cell(void);
static void handle_move_cursor(void);

const float LED_ON_INTENSITY = 0.02f;

int main(void) {
	stdio_init_all();

	for (int i = 0; i < 25; i++)
		buffer[i] = (ws2812b_color_t) { 0.0f, 0.0f, 0.0f };
	buffer[5*cursor_y + cursor_x].g = LED_ON_INTENSITY;

	if (!ws2812b_matrix_init(&matrix, pio0, LED_STRIP_PIN))
		die("falha ao inicializar a matriz de LEDs");

	adc_init();
	adc_gpio_init(JOYSTICK_X_PIN);
	adc_gpio_init(JOYSTICK_Y_PIN);

	gpio_init(BUTTON_A_PIN);
	gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_A_PIN);

	gpio_init(BUTTON_B_PIN);
	gpio_set_dir(BUTTON_B_PIN, GPIO_IN);
	gpio_pull_up(BUTTON_B_PIN);

	gpio_init(MID_LED_B_PIN);
	gpio_set_dir(MID_LED_B_PIN, GPIO_OUT);

	gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &on_press);
	gpio_set_irq_enabled_with_callback(BUTTON_B_PIN, GPIO_IRQ_EDGE_FALL, true, &on_press);

	i2c_init(DISPLAY_I2C_PORT, 400000); // 400KHz
	gpio_set_function(DISPLAY_SDA_PIN, GPIO_FUNC_I2C);
	gpio_set_function(DISPLAY_SCL_PIN, GPIO_FUNC_I2C);
	gpio_pull_up(DISPLAY_SDA_PIN);
	gpio_pull_up(DISPLAY_SCL_PIN);

	if (!ssd1306_init(&display, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, DISPLAY_I2C_PORT))
		die("falha ao inicializar o display OLED");

	buzzer_init(&bz, BUZZER_PIN);

	struct repeating_timer timer;
	add_repeating_timer_ms(UPDATE_TIME_MS, main_loop, NULL, &timer);

	while (true) {
		sleep_ms(10000);
	}

	return 0;
}

static bool main_loop(struct repeating_timer *_) {
	buzzer_stop(&bz);
	gpio_put(MID_LED_B_PIN, 0);

	adc_select_input(JOYSTICK_X_INPUT);
	joystick_x_axis = get_axis_normalized(adc_read());

	adc_select_input(JOYSTICK_Y_INPUT);
	joystick_y_axis = get_axis_normalized(adc_read());

	uint16_t x_mid = DISPLAY_WIDTH / 2 + joystick_x_axis * 15;
	uint16_t y_mid = DISPLAY_HEIGHT / 2 - joystick_y_axis * 15;

	ssd1306_fill(&display, 0);
	ssd1306_rect(&display, y_mid - 4, x_mid - 4, 8, 8, 1, true);
	ssd1306_send_data(&display);

	if (screen_updates_queued > 0) {
		buzzer_start(&bz, 400.0f);
		gpio_put(MID_LED_B_PIN, 1);
		ws2812b_matrix_draw(&matrix, &buffer);
		screen_updates_queued--;
	}

	return true;
}

static void on_press(uint gpio, uint32_t events) {
	static volatile uint32_t last_time_a = 0;
	static volatile uint32_t last_time_b = 0;
	uint32_t current_time = to_us_since_boot(get_absolute_time());

	if (gpio == BUTTON_A_PIN) {
		if (current_time - last_time_a > DEBOUNCING_TIME_US) {
			bool button_a_pressed = !gpio_get(BUTTON_A_PIN);
			if (button_a_pressed) {
				handle_toggle_cell();
			}
			last_time_a = current_time;
		}
	} else if (gpio == BUTTON_B_PIN) {
		if (current_time - last_time_b > DEBOUNCING_TIME_US) {
			bool button_b_pressed = !gpio_get(BUTTON_B_PIN);
			if (button_b_pressed) {
				handle_move_cursor();
			}
			last_time_b = current_time;
		}
	}
}

static void handle_toggle_cell(void) {
	float *val = &buffer[5*cursor_y + cursor_x].r;
	*val = (*val == 0.0f) ? LED_ON_INTENSITY : 0.0f;

	printf("Trocando casa atual de cor\n");

	// avisar que o display mudou
	screen_updates_queued++;
}

static void handle_move_cursor(void) {
	int old_x = cursor_x;
	int old_y = cursor_y;
	float abs_x = fabsf(joystick_x_axis);
	float abs_y = fabsf(joystick_y_axis);

	if (abs_x > abs_y) {
		if (abs_x < 0.1) {}
		else if (joystick_x_axis < 0) {
			// joystick predominantemente para a esquerda
			if (cursor_x > 0) cursor_x--;
		} else {
			// joystick predominantemente para a direita
			if (cursor_x < 4) cursor_x++;
		}
	} else {
		if (abs_y < 0.1) {}
		else if (joystick_y_axis < 0) {
			// joystick predominantemente para cima
			if (cursor_y < 4) cursor_y++;
		} else {
			// joystick predominantemente para baixo
			if (cursor_y > 0) cursor_y--;
		}
	}

	if (old_x != cursor_x || old_y != cursor_y) {
		printf("Posição alterada de (%d, %d) to (%d, %d)\n", old_x, old_y, cursor_x, cursor_y);

		// atualizar marcação do cursor se ele mudou de lugar
		buffer[5*old_y + old_x].g = 0.0f;
		buffer[5*cursor_y + cursor_x].g = LED_ON_INTENSITY;

		// avisar que o display mudou
		screen_updates_queued++;
	}
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
