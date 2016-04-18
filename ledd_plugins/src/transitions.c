/*
 * transitions_priv.h
 *
 *  Created on: Apr 19, 2016
 *      Author: ncarrier
 */
#include <sys/param.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ledd_plugin.h>
#include <math.h>

#define ULOG_TAG ledd_transitions
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_transitions);

#include <ut_utils.h>
#include <ut_string.h>

#include <ledd_plugin.h>

#include "transitions_priv.h"
#include "lua_globals_priv.h"


struct transition {
	char *name;
	uint16_t id;
	uint8_t (*compute)(uint32_t nb_values, uint32_t value_idx,
			uint8_t start_value, uint8_t end_value);
};

static struct transition transitions[TRANSITION_MAX];

uint8_t transition_clip_value(int value)
{
	return MAX(MIN(value, LED_CHANNEL_MAX), 0.);
}

float transition_clip_float(float t)
{
	return MAX(MIN(t, 1.), 0.);
}

uint8_t transition_map_to(float t, uint8_t start_value, uint8_t end_value)
{
	return transition_clip_float(t) * (end_value - start_value) +
			start_value;
}

static uint8_t ramp_compute(uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value)
{
	float t;

	t = (1.f * value_idx) / nb_values;

	return transition_map_to(t , start_value, end_value);
}

#define PI 3.1415f
static uint8_t cosine_compute(uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value)
{
	float a;
	float t;

	/* go back to [-PI, 0] */
	a = value_idx * PI / nb_values - PI;

	t = (cos(a) + 1.f) / 2.f;

	return transition_map_to(t, start_value, end_value);
}

const struct transition *transition_get(uint16_t id)
{
	unsigned i;

	for (i = 0; i < UT_ARRAY_SIZE(transitions)
			|| transitions[i].compute == NULL; i++)
		if (id == transitions[i].id)
			return transitions + i;

	errno = ESRCH;

	return NULL;
}

const char *transition_get_name(const struct transition *t)
{
	return t == NULL ? "(none)" : t->name;
}

uint16_t transition_get_id(const struct transition *t)
{
	return t == NULL ? 0 : t->id;
}

const struct transition *transition_next_from(const struct transition *t)
{
	ptrdiff_t offset;
	unsigned index;

	if (t == NULL)
		return transitions + 0;

	offset = t - transitions;
	if (offset < 0) {
		errno = EINVAL;
		return NULL;
	}
	index = offset + 1;
	if (transitions[index].compute == NULL
			|| index >= UT_ARRAY_SIZE(transitions))
		return NULL;

	return transitions + index;
}

uint8_t transition_compute(const struct transition *transition,
		uint32_t nb_val, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value)
{
	if (transition == NULL)
		return 0;

	return transition->compute(nb_val, value_idx, start_value, end_value);
}

int transition_register(const char *name, transition_function compute)
{
	unsigned i;

	if (ut_string_is_invalid(name) || compute == NULL)
		return -EINVAL;

	/* find first free slot */
	for (i = 0; i < UT_ARRAY_SIZE(transitions); i++)
		if (transitions[i].compute == NULL)
			break;

	if (i >= UT_ARRAY_SIZE(transitions))
		return -ENOMEM;

	transitions[i].compute = compute;
	transitions[i].name = strdup(name);
	transitions[i].id = i >= 1 ? transitions[i - 1].id + 1 :  0x100;

	return lua_globals_register_int(name, transitions[i].id,
			LUA_GLOBALS_CONFIG_PATTERNS);
}

static __attribute__((constructor)) void transitions_init(void)
{
	int ret;

	ULOGD("register ramp and cosine transitions");

	ret = transition_register("ramp", ramp_compute);
	if (ret < 0)
		ULOGW("transition_register: %s", strerror(-ret));
	ret = transition_register("cosine", cosine_compute);
	if (ret < 0)
		ULOGW("transition_register: %s", strerror(-ret));
}

static __attribute__((destructor)) void transitions_cleanup(void)
{
	unsigned i;

	for (i = 0; i < UT_ARRAY_SIZE(transitions) &&
			transitions[i].compute != NULL; i++)
		ut_string_free(&transitions[i].name);

	memset(transitions, 0, sizeof(transitions));
}
