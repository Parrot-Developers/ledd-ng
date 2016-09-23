/**
 * @file pattern.c
 *
 * Copyright (c) 2016 Parrot S.A.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/param.h> /* for MIN and MAX */

#include <string.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>

#define ULOG_TAG ledd_patterns
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_patterns);

#include <rs_dll.h>
#include <rs_node.h>

#include <ut_string.h>
#include <ut_file.h>
#include <ut_utils.h>

#include "pattern.h"
#include "global.h"
#include "utils.h"
#include "transitions_priv.h"
#include "led_driver_priv.h"

struct pattern_frame {
	uint16_t value;
	uint16_t duration;
};

#define MAX_CHANNELS_PER_PATTERN 20

struct pattern_channel {
	/* values read from patterns config file */
	char *led_id;
	char *channel_id;
	unsigned nb_frames;
	struct pattern_frame *frames;

	/* post-processed fields */
	uint32_t duration; /* in ms, multiple of granularity */
};

struct pattern_values {
	/* indexes x granularity = time in ms */
	uint8_t *values[MAX_CHANNELS_PER_PATTERN];
};

struct pattern {
	struct rs_node node;

	/* values read from patterns config file */
	char *name;
	struct pattern_channel *channels[MAX_CHANNELS_PER_PATTERN];
	/* leds with at least one channel modified by this pattern */
	const char *leds[MAX_CHANNELS_PER_PATTERN];
	uint8_t nb_channels;
	uint8_t default_value;
	uint8_t repetitions;
	uint32_t intro;
	uint32_t outro;

	/* post-processed fields */
	struct pattern_values v;
	/* max of channels durations */
	uint32_t total_duration; /* in ms, multiple of granularity */
};

static struct rs_dll patterns;

#define to_pattern(n) ut_container_of(n, struct pattern, node)

static int read_frame(lua_State *l, struct pattern_channel *channel, int index)
{
	uint16_t value;
	const struct transition *transition;

	/* beware, lua tables' indices start at 1, not 0 */
	lua_rawgeti(l, -1, 1);
	channel->frames[index - 1].value = value = luaL_checknumber(l, -1);
	lua_pop(l, 1);

	lua_rawgeti(l, -1, 2);
	channel->frames[index - 1].duration = luaL_checknumber(l, -1);
	lua_pop(l, 1);

	if (value < 0x100) {
		ULOGD("frame = {.value = %d, .duration = %d}", value,
				channel->frames[index - 1].duration);
	} else {
		transition = transition_get(value);
		ULOGD("frame = {.transition = %s, .duration = %d}",
				transition_get_name(transition),
				channel->frames[index - 1].duration);
	}

	return 0;
}

static int pattern_store_channel(struct pattern *pattern,
		struct pattern_channel *channel)
{
	if (pattern->nb_channels == MAX_CHANNELS_PER_PATTERN)
		return -ENOMEM;

	pattern->channels[pattern->nb_channels++] = channel;

	return 0;
}

static void pattern_destroy(struct pattern *pattern)
{
	int i;
	struct pattern_channel *channel;

	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern->channels[i];
		if (channel != NULL) {
			ut_string_free(&channel->channel_id);
			ut_string_free(&channel->led_id);
			free(channel->frames);
			channel->frames = NULL;
			memset(channel, 0, sizeof(*channel));
			free(channel);
		}
		if (pattern->v.values[i] != NULL)
			free(pattern->v.values[i]);
	}

	ut_string_free(&pattern->name);
	memset(pattern, 0, sizeof(*pattern));
	free(pattern);
}

static int read_channel(lua_State *l, struct pattern *pattern)
{
	int ret;
	struct pattern_channel *channel;
	int index;
	const char *key;

	ULOGD("%s", __func__);

	channel = calloc(1, sizeof(*channel));
	if (channel == NULL)
		config_error(l, errno, "calloc");
	channel->nb_frames = luaL_len(l, -1);
	channel->frames = calloc(channel->nb_frames, sizeof(*channel->frames));
	if (channel->frames == NULL)
		config_error(l, errno, "calloc");

	/* iterate over the channel's content */
	lua_pushnil(l);
	while (lua_next(l, -2) != 0) {
		if (lua_isnumber(l, -2)) {
			index = luaL_checknumber(l, -2);
			ret = read_frame(l, channel, index);
			if (ret != 0)
				config_error(l, -ret, "read_frame");
		} else if (lua_isstring(l, -2)) {
			key = lua_tostring(l, -2);
			if (ut_string_match(key, "led_id")) {
				channel->led_id = strdup(
						luaL_checkstring(l, -1));
				if (channel->led_id == NULL)
					config_error(l, errno, "strdup");
			} else if (ut_string_match(key, "channel_id")) {
				channel->channel_id = strdup(
						luaL_checkstring(l, -1));
				if (channel->channel_id == NULL)
					config_error(l, errno, "strdup");
			} else {
				luaL_error(l, "unknown pattern key '%s'", key);
			}
		} else {
			luaL_error(l, "expected string or number key, got %s",
					lua_typename(l, lua_type(l, -2)));
		}
		lua_pop(l, 1);
	}
	ret = pattern_store_channel(pattern, channel);
	if (ret < 0) {
		ULOGE("pattern_store_channel: %s", strerror(-ret));
		pattern_destroy(pattern);
	}

	return ret;
}

static int read_pattern(lua_State *l, const char *pattern_name)
{
	int ret;
	const char *key = "";
	struct pattern *p;

	ULOGD("%s(%s)", __func__, pattern_name);

	p = calloc(1, sizeof(*p));
	if (p == NULL)
		config_error(l, errno, "calloc");
	p->name = strdup(pattern_name);
	if (p->name == NULL)
		config_error(l, errno, "strdup");

	p->repetitions = 1;
	rs_dll_enqueue(&patterns, &p->node);

	/* iterate over the pattern's content */
	lua_pushnil(l);
	while (lua_next(l, -2) != 0) {
		if (lua_isnumber(l, -2)) {
			ret = read_channel(l, p);
			if (ret != 0)
				config_error(l, -ret, "read_channel");
		} else if (lua_isstring(l, -2)) {
			key = lua_tostring(l, -2);
			if (ut_string_match(key, "repetitions"))
				p->repetitions = luaL_checkunsigned(l, -1);
			else if (ut_string_match(key, "default_value"))
				p->default_value = luaL_checkunsigned(l, -1);
			else if (ut_string_match(key, "intro"))
				p->intro = luaL_checkunsigned(l, -1);
			else if (ut_string_match(key, "outro"))
				p->outro = luaL_checkunsigned(l, -1);
			else
				luaL_error(l, "unknown pattern key '%s'", key);
		} else {
			luaL_error(l, "expected string or number key, got %s",
					lua_typename(l, lua_type(l, -2)));
		}
		lua_pop(l, 1);
	}

	return 0;
}

static int read_patterns(lua_State *l)
{
	int ret;
	const char *key;

	ULOGD("%s", __func__);

	lua_getglobal(l, "patterns");
	if (!lua_istable(l, -1))
		luaL_error(l, "'patterns' is not a table");

	/* iterate over the "patterns" */
	lua_pushnil(l); /* first key */
	while (lua_next(l, -2) != 0) {
		if (!lua_isstring(l, -2))
			luaL_error(l, "string expected for pat. name, got %s",
					lua_typename(l, lua_type(l, -2)));
		if (!lua_istable(l, -1))
			luaL_error(l, "table expected for pat. desc., got %s",
					lua_typename(l, lua_type(l, -1)));

		key = lua_tostring(l, -2);
		ret = read_pattern(l, key);
		if (ret != 0)
			config_error(l, -ret, "read_pattern");
		lua_pop(l, 1);
	}
	/* pop the last key */
	lua_pop(l, 1);

	/* pop the "pattern" table */
	lua_pop(l, 1);

	return 0;
}

static int compute_channel_duration(struct pattern_channel *channel)
{
	unsigned i;
	struct pattern_frame *frame;
	uint32_t granularity = global_get_granularity();

	for (i = 0; i < channel->nb_frames; i++) {
		frame = channel->frames + i;
		if ((frame->duration % granularity) != 0) {
			ULOGW("duration of frame %s_%s[%"PRIu32"] isn't a "
					"multiple of granularity",
					channel->led_id, channel->channel_id,
					i);
			return -EINVAL;
		}
		channel->duration += channel->frames[i].duration;
	}

	return 0;
}

static bool frame_is_transition(const struct pattern_frame *frame)
{
	return frame->value >= 0x100;
}

static int compute_channel_values(const struct pattern_channel *channel,
		uint8_t **values, uint32_t total_duration)
{
	uint32_t i;
	uint32_t frame_index = 0;
	int ret;
	struct pattern_frame *frame;
	struct pattern_frame *next_frame;
	uint32_t nb_values;
	uint32_t value_idx = 0;
	uint32_t granularity = global_get_granularity();
	uint8_t start_value = 0;
	uint8_t end_value = 0;
	const struct transition *transition = NULL;

	if (frame_is_transition(channel->frames)) {
		ULOGE("a pattern can't start with a transition");
		return -EINVAL;
	}

	*values = calloc(total_duration / granularity, sizeof(**values));
	if (*values == NULL) {
		ret = -errno;
		ULOGE("calloc: %m");
		return ret;
	}

	frame = channel->frames;
	nb_values = frame->duration / granularity;
	for (i = 0; i < total_duration / granularity; i++) {
		if (!frame_is_transition(frame))
			(*values)[i] = frame->value;
		else
			(*values)[i] = transition_compute(transition, nb_values,
					value_idx, start_value, end_value);
		value_idx++;

		if (value_idx == nb_values) {
			/* when end of frame reached, jump to next if any */
			frame_index++;
			if (frame_index >= channel->nb_frames)
				return 0;

			frame = channel->frames + frame_index;
			value_idx = 0;
			nb_values = frame->duration / granularity;
			if (!frame_is_transition(frame))
				continue;

			/* transition: need to get start and end values */
			transition = transition_get(frame->value);
			if (transition == NULL) {
				ret = -errno;
				ULOGE("transition %"PRIu8" missing: %m",
						frame->value);
				return ret;
			}
			start_value = (frame - 1)->value;
			next_frame = channel->frames;
			next_frame += (frame_index + 1) % channel->nb_frames;
			end_value = next_frame->value;
		}
	}

	return 0;
}

static struct pattern_channel *pattern_get_channel(
		const struct pattern *pattern, unsigned i)
{
	return pattern->channels[i];
}


static int add_modified_led(struct pattern *pattern, const char *led_id)
{
	unsigned i;

	for (i = 0; i < MAX_CHANNELS_PER_PATTERN; i++)
		if (ut_string_match(pattern->leds[i], led_id))
			return 0;
		else if (pattern->leds[i] == NULL)
			break;

	if (i == MAX_CHANNELS_PER_PATTERN)
		return -ENOMEM;

	pattern->leds[i] = led_id;

	return 0;
}

static int post_process_pattern(struct pattern *pattern)
{
	int ret;
	unsigned i;
	struct pattern_channel *channel;
	uint32_t old_total_duration;

	/* compute total duration */
	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern_get_channel(pattern, i);
		ret = compute_channel_duration(channel);
		if (ret < 0) {
			ULOGE("compute_channel_duration: %s", strerror(-ret));
			return ret;
		}
		old_total_duration = pattern->total_duration;
		pattern->total_duration = MAX(pattern->total_duration,
				channel->duration);
		if (i != 0 && pattern->total_duration != old_total_duration)
			ULOGW("channel durations of pattern %s are not equal. "
					"the shorter patterns will completed "
					"with zeroes, old duration %"PRIu32
					", new duration %"PRIu32,
					pattern->name, old_total_duration,
					pattern->total_duration);

		/* some sanity checks */
		if (pattern->intro > pattern->total_duration) {
			ULOGE("intro time (%"PRIu32"is longer than  duration",
					pattern->intro);
			return -EINVAL;
		}
		if (pattern->outro > pattern->total_duration) {
			ULOGE("outro time (%"PRIu32"is longer than  duration",
					pattern->intro);
			return -EINVAL;
		}
	}

	/* compute all channel values and list the leds modified */
	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern_get_channel(pattern, i);
		ret = compute_channel_values(channel, &(pattern->v.values[i]),
				pattern->total_duration);
		if (ret < 0) {
			ULOGE("compute_channel_values: %s", strerror(-ret));
			return ret;
		}
		ret = add_modified_led(pattern, channel->led_id);
		if (ret < 0)
			ULOGW("add_modified_led");
	}

	return 0;
}

static int patterns_post_process(void)
{
	int ret;
	struct rs_node *node = NULL;
	struct pattern *pattern;

	while ((node = rs_dll_next_from(&patterns, node))) {
		pattern = to_pattern(node);
		ret = post_process_pattern(pattern);
		if (ret < 0) {
			ULOGE("post_process_pattern: %s", strerror(-ret));
			return ret;
		}
	}

	return 0;
}

static void pattern_print(struct rs_node *node)
{
	struct pattern *pattern = to_pattern(node);
#ifdef LEDD_VERBOSE_PATTERN_DUMP
	uint8_t i;
	uint32_t j;
	struct pattern_values *values;
	uint32_t granularity = global_get_granularity();
#endif /* LEDD_VERBOSE_PATTERN_DUMP */

	ULOGI("\t%s:", pattern->name);
	ULOGI("\t\tnb_channels = %"PRIu8, pattern->nb_channels);
	ULOGI("\t\trepetitions = %"PRIu8, pattern->repetitions);
	ULOGI("\t\tdefault_value = %"PRIu8, pattern->default_value);
	ULOGI("\t\ttotal_duration = %"PRIu32, pattern->total_duration);
	ULOGI("\t\tintro = %"PRIu32, pattern->intro);
	ULOGI("\t\toutro = %"PRIu32, pattern->outro);
#ifdef LEDD_VERBOSE_PATTERN_DUMP
	for (i = 0; i < pattern->nb_channels; i++) {
		values = &pattern->v;
		ULOGI("\t\tchannel[%"PRIu8"]", i);
		for (j = 0; j < pattern->total_duration / granularity; j++)
			ULOGI("\t\t\tvalue = %"PRIu8, values->values[i][j]);
	}
#endif /* LEDD_VERBOSE_PATTERN_DUMP */
}

static const struct rs_dll_vtable patterns_vtable = {
		.print = pattern_print,
};

int patterns_init(const char *path)
{
	int ret;

	rs_dll_init(&patterns, &patterns_vtable);
	ret = read_config(path, read_patterns, LUA_GLOBALS_CONFIG_PATTERNS);
	if (ret < 0) {
		ULOGE("read_config(%s): %s", path, strerror(-ret));
		return ret;
	}

	ret = patterns_post_process();
	if (ret < 0) {
		ULOGE("patterns_post_process: %s", strerror(-ret));
		return ret;
	}

	return ret;
}

static RS_NODE_MATCH_STR_MEMBER(pattern, name, node);

static struct pattern *get_pattern(const char *name)
{
	struct rs_node *node;

	node = rs_dll_find_match(&patterns, pattern_match_str_name, name);
	if (node == NULL) {
		errno = ESRCH;
		return NULL;
	}

	return to_pattern(node);
}

const struct pattern *pattern_get(const char *name)
{
	if (ut_string_is_invalid(name)) {
		errno = -EINVAL;
		return NULL;
	}

	return get_pattern(name);
}

uint32_t pattern_get_total_duration(const struct pattern *pattern)
{
	if (pattern == NULL) {
		errno = -EINVAL;
		return 0;
	}

	return pattern->total_duration;
}

uint32_t pattern_get_repetitions(const struct pattern *pattern)
{
	if (pattern == NULL) {
		errno = -EINVAL;
		return 0;
	}

	return pattern->repetitions;
}

const char *pattern_get_name(const struct pattern *pattern)
{
	if (pattern == NULL) {
		errno = -EINVAL;
		return NULL;
	}

	return pattern->name;
}

static int apply_default_values(const struct pattern *pattern)
{
	int ret;
	uint8_t i;
	const char *led_id;

	for (i = 0; i < MAX_CHANNELS_PER_PATTERN && pattern->leds[i] != NULL;
			i++) {
		led_id = pattern->leds[i];
		if (led_id == NULL)
			return 0;

		ret = led_driver_apply_default_value(led_id,
				pattern->default_value);
		if (ret < 0)
			ULOGW("led_driver_apply_default_value: %s",
					strerror(-ret));
	}

	return 0;
}

int pattern_apply_values(const struct pattern *pattern, uint32_t cursor,
		bool apply_default)
{
	int ret;
	struct pattern_channel *channel;
	uint8_t i;
	uint8_t value;

	if (apply_default) {
		ret = apply_default_values(pattern);
		if (ret < 0)
			ULOGW("apply_default_values(%s): %s", pattern->name,
					strerror(-ret));
	}

	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern_get_channel(pattern, i);
		value = pattern->v.values[i][cursor];
		ret = led_driver_set_value(channel->led_id, channel->channel_id,
				value);
		if (ret < 0)
			ULOGW("led_driver_set_value(%s, %s, %"PRIu8"): %s",
					channel->led_id, channel->channel_id,
					value, strerror(-ret));
	}

	return 0;
}

int pattern_switch_off(const struct pattern *pattern)
{
	int ret;
	struct pattern_channel *channel;
	uint8_t i;

	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern_get_channel(pattern, i);
		ret = led_driver_set_value(channel->led_id, channel->channel_id,
				0);
		if (ret < 0)
			ULOGW("led_driver_set_value(%s, %s, %"PRIu8"): %s",
					channel->led_id, channel->channel_id,
					0, strerror(-ret));
	}

	return 0;
}

static bool channel_have_same_led(const struct pattern_channel *channel1,
		const struct pattern_channel *channel2)
{
	if (channel1 == NULL || channel2 == NULL)
		return false;

	return ut_string_match(channel1->led_id, channel2->led_id);
}

uint32_t pattern_get_intro(const struct pattern *pattern)
{
	return pattern->intro;
}

uint32_t pattern_get_outro(const struct pattern *pattern)
{
	return pattern->outro;
}

bool patterns_intersect(const struct pattern *pat1, const struct pattern *pat2)
{
	uint8_t i;
	uint8_t j;
	struct pattern_channel *channel1;
	struct pattern_channel *channel2;

	for (i = 0; i < pat1->nb_channels; i++)
		for (j = 0; j < pat2->nb_channels; j++) {
			channel1 = pattern_get_channel(pat1, i);
			channel2 = pattern_get_channel(pat2, i);
			if (channel_have_same_led(channel1, channel2))
				return true;
		}

	return false;
}

static bool pattern_contains_led(const struct pattern *pattern,
		const char *led_id)
{
	struct pattern_channel *channel;
	uint8_t i;

	for (i = 0; i < pattern->nb_channels; i++) {
		channel = pattern->channels[i];
		if (ut_string_match(channel->led_id, led_id))
			return true;
	}

	return false;
}

bool patterns_have_same_support(const struct pattern *pat1,
		const struct pattern *pat2)
{
	struct pattern_channel *channel;
	uint8_t i;

	/* check that all leds controlled by pat1 are controlled by pat2 */
	for (i = 0; i < pat1->nb_channels; i++) {
		channel = pat1->channels[i];
		if (!pattern_contains_led(pat2, channel->led_id))
			return false;
	}
	/* check that all leds controlled by pat1 are controlled by pat2 */
	for (i = 0; i < pat2->nb_channels; i++) {
		channel = pat2->channels[i];
		if (!pattern_contains_led(pat1, channel->led_id))
			return false;
	}

	return true;
}

void patterns_dump_config(void)
{
	rs_dll_dump(&patterns);
}

void patterns_cleanup(void)
{
	struct pattern *pattern;
	struct rs_node *node;

	while (rs_dll_get_count(&patterns) != 0) {
		node = rs_dll_pop(&patterns);
		pattern = to_pattern(node);
		pattern_destroy(pattern);
	}
}
