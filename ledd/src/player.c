/**
 * @file player.c
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
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <errno.h>
#include <string.h>
#include <inttypes.h>

#define ULOG_TAG ledd_player
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_player);

#include <rs_node.h>
#include <rs_dll.h>

#include <ut_utils.h>
#include <ut_string.h>

#include <ledd_plugin.h>

#include "player.h"
#include "pattern.h"
#include "global.h"

struct player {
	struct rs_dll streams;
	bool playing;
};

static struct player player;

struct player_stream {
	struct rs_node node;
	const struct pattern *pattern;
	uint32_t total_duration;
	uint32_t cursor;
	uint8_t repetitions;
	uint8_t repetition;
	struct player_stream *previous;
};

#define to_stream(n) ut_container_of((n), struct player_stream, node)

int player_init(void)
{
	ULOGD("%s", __func__);

	return rs_dll_init(&player.streams, NULL);
}

static void player_stream_init(struct player_stream *stream,
		const struct pattern *pattern, uint32_t total_duration,
		uint8_t repetitions, struct player_stream *previous)
{
	memset(stream, 0, sizeof(*stream));
	stream->pattern = pattern;
	stream->total_duration = total_duration;
	stream->repetitions = repetitions;
	stream->previous = previous;
}

static void player_stream_destroy(struct player_stream *stream)
{
	memset(stream, 0, sizeof(*stream));
	free(stream);
}

static struct player_stream *player_stream_new(
		const struct pattern *pattern, uint32_t total_duration,
		uint8_t repetitions, struct player_stream *previous)
{
	int old_errno;
	struct player_stream *stream;

	stream = calloc(1, sizeof(*stream));
	if (stream == NULL) {
		old_errno = errno;
		ULOGE("calloc: %m");
		errno = old_errno;
		return NULL;
	}
	player_stream_init(stream, pattern, total_duration, repetitions,
			previous);

	return stream;
}

int player_set_pattern(const char *new_pattern_name, bool resume)
{
	int ret;
	struct rs_node *node = NULL;
	struct player_stream *os; /* old_stream */
	struct player_stream *ns; /* new steam */
	const char *old_pattern_name;
	const struct pattern *pattern;
	const struct pattern *old_pattern;
	uint32_t total_duration;
	uint8_t repetitions;

	ULOGD("%s(%s, %d)", __func__, new_pattern_name, resume);

	if (!player.playing) {
		player.playing = true;
		ULOGI("player started");
	}

	pattern = pattern_get(new_pattern_name);
	if (pattern == NULL) {
		ret = -errno;
		ULOGE("pattern_get:%m");
		return ret;
	}
	total_duration = pattern_get_total_duration(pattern);
	repetitions = pattern_get_repetitions(pattern);

	while ((node = rs_dll_next_from(&player.streams, node))) {
		os = to_stream(node);
		old_pattern = os->pattern;
		old_pattern_name = pattern_get_name(old_pattern);
		/* if the pattern is already playing, we do nothing */
		if (ut_string_match(new_pattern_name, old_pattern_name))
			return 0;

		if (patterns_intersect(pattern, old_pattern)) {
			if (!patterns_have_same_support(pattern, old_pattern)) {
				ULOGE("patterns %s and %s have some leds in "
						"common, but not all. This is "
						"not supported.",
						old_pattern_name,
						new_pattern_name);
				return -EINVAL;
			}
			ret = pattern_apply_values(pattern, 0, true);
			if (ret < 0)
				ULOGW("pattern_apply_values: %s",
						strerror(-ret));
			if (!resume) {
				/* replace all the "old" streams */
				if (os->previous != NULL)
					player_stream_destroy(os->previous);
				player_stream_init(os, pattern, total_duration,
						repetitions, os->previous);
				return 0;
			}
			/* here we want to resume after */
			if (os->previous != NULL) {
				/* a previous stream exist, replace both */
				memcpy(os->previous, os, sizeof(*os));
				os->previous->previous = NULL;
				player_stream_init(os, pattern, total_duration,
						repetitions, os->previous);
				return 0;
			}
			/* here os->previous == NULL */
			/* store a new stream and link it with the old one */
			rs_dll_remove(&player.streams, &os->node);
			ns = player_stream_new(pattern, total_duration,
					repetitions, os);
			if (ns == NULL)
				return -errno;
			rs_dll_push(&player.streams, &ns->node);
			return 0;
		}
	}

	/* here, it is guaranteed that there is no intersection */
	ret = pattern_apply_values(pattern, 0, true);
	if (ret < 0)
		ULOGW("pattern_apply_values: %s", strerror(-ret));
	ns = player_stream_new(pattern, total_duration, repetitions, NULL);
	if (ns == NULL)
		return -errno;
	rs_dll_push(&player.streams, &ns->node);

	return 0;
}

bool player_is_playing(void)
{
	return player.playing;
}

int player_update(void)
{
	int ret;
	struct rs_node *node = NULL;
	struct player_stream *stream;
	struct player_stream *previous_stream;
	uint32_t granularity = global_get_granularity();
	uint32_t intro;
	uint32_t outro;
	struct rs_dll trash;

	rs_dll_init(&trash, NULL);

	while ((node = rs_dll_next_from(&player.streams, node))) {
		stream = to_stream(node);
		/* apply values */
		ret = pattern_apply_values(stream->pattern, stream->cursor,
				false);
		if (ret < 0)
			ULOGW("pattern_apply_values: %s", strerror(-ret));

		/* advance reading heads */
		stream->cursor++;
		outro = pattern_get_outro(stream->pattern) / granularity;
		if (stream->cursor >= stream->total_duration / granularity
						- outro) {
			if (outro == 0 || stream->repetition
					< stream->repetitions - 1 ||
					stream->cursor == stream->total_duration
					/ granularity)
				stream->repetition++;
			else
				continue;
			/*
			 * the stream is finished, drop it and replace it by
			 * its previous, if any
			 */
			if (stream->repetitions != 0 &&
					stream->repetition ==
							stream->repetitions) {
				previous_stream = stream->previous;
				rs_dll_remove(&player.streams, &stream->node);
				/*
				 * we can't destroy the stream while traversing
				 * the linked list, so we store it into trash
				 * for removal afterwards
				 */
				rs_dll_push(&trash, &stream->node);
				stream = previous_stream;
				if (previous_stream != NULL) {
					/*
					 * if present, the previous becomes the
					 * current stream again
					 */
					rs_dll_push(&player.streams,
							&stream->node);
					/* reset state of the previous stream */
					ret = pattern_apply_values(
							stream->pattern,
							stream->cursor, true);
					if (ret < 0)
						ULOGW("pattern_apply_values: "
								"%s",
								strerror(-ret));
				}
			} else {
				/* the patterns repeate, jump at intro's end */
				intro = pattern_get_intro(stream->pattern) /
						granularity;
				stream->cursor = intro;
			}
		}
	}

	while (rs_dll_get_count(&trash) != 0) {
		stream = to_stream(rs_dll_pop(&trash));
		player_stream_destroy(stream);
	}

	if (rs_dll_is_empty(&player.streams)) {
		player.playing = false;
		ULOGI("player stopped");
	}

	led_driver_tick_all_drivers();

	return 0;
}

void player_cleanup(void)
{
	struct player_stream *stream;

	ULOGD("%s", __func__);

	while (rs_dll_get_count(&player.streams) != 0) {
		stream = to_stream(rs_dll_pop(&player.streams));
		player_stream_destroy(stream);
	}

	rs_dll_init(&player.streams, NULL);
	player.playing = false;
}
