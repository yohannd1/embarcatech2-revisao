#include <stdio.h>

#include "pico/stdlib.h"
#include "hardware/timer.h"

// velocidade de atualização do loop principal
#define UPDATE_RATE_HZ 30
#define UPDATE_TIME_MS (1000.0f / UPDATE_RATE_HZ)

static volatile int val = 0;

static void die(const char *msg);
static bool main_loop(struct repeating_timer *_);

int main(void) {
	stdio_init_all();

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
