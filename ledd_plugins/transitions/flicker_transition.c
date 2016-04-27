/*
 * flicker_transition.c
 *
 *  Created on: Apr 27, 2016
 *      Author: ncarrier
 */
#include <string.h>

#define ULOG_TAG flicker_transition
#include <ulog.h>
ULOG_DECLARE_TAG(flicker_transition);

#include <stdlib.h>

#include <ledd_plugin.h>

static uint8_t flicker_compute(uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value)
{
	float amplitude = abs(end_value - start_value);
	float alea = (amplitude * rand()) / RAND_MAX - amplitude / 2.f;
	int value;
	float t;

	t = (1.f * value_idx) / nb_values;
	value = transition_map_to(t, start_value, end_value);
	value += alea;

	return transition_clip_value(value);
}

static __attribute__((constructor)) void flicker_transition_init(void)
{
	int ret;

	ret = transition_register("flicker", flicker_compute);
	if (ret < 0)
		ULOGE("transition_register(flicker): %s", strerror(-ret));
}
