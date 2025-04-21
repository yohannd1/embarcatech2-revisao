#ifndef _BUZZER_H
#define _BUZZER_H

#include <stdint.h>

typedef struct buzzer_t {
	uint pin;
	uint slice;
} buzzer_t;

void buzzer_init(buzzer_t *bz, uint pin);
void buzzer_deinit(buzzer_t *bz);
void buzzer_play(buzzer_t *bz, float frequency, uint duration_ms);

void buzzer_start(buzzer_t *bz, float frequency);
void buzzer_stop(buzzer_t *bz);

#endif
