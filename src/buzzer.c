#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

#include "buzzer.h"

// estratégia: quando o clock está a 125MHz, um divisor de 250 faz com que a
// frequência do PWM seja de 500KHz/(wrap+1), e já que o valor máximo para
// wrap+1 é 65536, o intervalo possível é entre 7Hz e 500KHz.
const float clock_div = 250.0f;

void buzzer_init(buzzer_t *bz, uint pin) {
	bz->pin = pin;

	gpio_set_function(bz->pin, GPIO_FUNC_PWM);
	bz->slice = pwm_gpio_to_slice_num(bz->pin);

	pwm_config config = pwm_get_default_config();
	pwm_config_set_clkdiv(&config, clock_div);
	pwm_init(bz->slice, &config, true);

	pwm_set_gpio_level(bz->pin, 0); // inicialmente "desligar" o PWM
}

void buzzer_deinit(buzzer_t *bz) {
	bz->pin = 0;
	bz->slice = 0;
}

void buzzer_play(buzzer_t *bz, float frequency, uint duration_ms) {
	buzzer_start(bz, frequency);
	sleep_ms(duration_ms);
	buzzer_stop(bz);
}

void buzzer_start(buzzer_t *bz, float frequency) {
	uint32_t clock_freq = clock_get_hz(clk_sys);
	uint32_t wrap = (uint32_t)(clock_freq / (clock_div * frequency) - 1.0f);

	pwm_set_wrap(bz->slice, wrap);
	pwm_set_gpio_level(bz->pin, wrap * 0.5f);
}

void buzzer_stop(buzzer_t *bz) {
	pwm_set_gpio_level(bz->pin, 0);
}
