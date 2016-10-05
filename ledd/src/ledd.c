/**
 * @file ledd.c
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include <signal.h>

#include <error.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define MSG_SET_PATTERN 0
#define MSG_QUIT 1
#define MSG_DUMP_CONFIG 2
#define MSG_SET_VALUE 3

#define ULOG_TAG ledd_lib
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_lib);

#include <libpomp.h>

#include <ut_string.h>

#include <ledd.h>

#include "led_driver_priv.h"

#include "utils.h"
#include "global.h"
#include "platform.h"
#include "pattern.h"
#include "player.h"
#include "plugins.h"

/* codecheck_ignore[VOLATILE] */
static volatile bool loop = true;

static struct pomp_ctx *pomp;
static struct pomp_timer *timer;

static const int exit_signals[] = {
		SIGINT,
		SIGTERM,
		SIGQUIT,
		0 /* sentinel */
};

static int command_set_value(const struct pomp_msg *msg)
{
	int ret;
	char __attribute__((cleanup(ut_string_free))) *led = NULL;
	char __attribute__((cleanup(ut_string_free))) *channel = NULL;
	unsigned value;

	ret = pomp_msg_read(msg, "%ms%ms%u", &led, &channel, &value);
	if (ret < 0) {
		ULOGE("pomp_msg_read: %s", strerror(-ret));
		return ret;
	}
	if (value > UINT8_MAX) {
		ULOGW("value %u above maximum %"PRIu8, value, UINT8_MAX);
		value = UINT8_MAX;
	}

	ULOGD("set_value(%s, %s, %"PRIu8")", led, channel, value);

	return led_driver_set_value(led, channel, value);
}

static int start_pattern(const char *pattern, bool resume)
{
	int ret;
	uint32_t granularity;

	ret = player_set_pattern(pattern, resume);
	if (ret < 0) {
		ULOGE("player_set_pattern: %s", strerror(-ret));
		return ret;
	}

	if (!player_is_playing())
		return 0;

	ULOGI("timer resumed");

	granularity = global_get_granularity();

	return pomp_timer_set_periodic(timer, granularity, granularity);
}

static void pomp_event_cb(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	int ret;
	uint32_t msgid;
	char __attribute__((cleanup(ut_string_free))) *pattern = NULL;
	char __attribute__((cleanup(ut_string_free))) *resume = NULL;
	char __attribute__((cleanup(ut_string_free))) *config = NULL;

	if (event != POMP_EVENT_MSG)
		return;

	msgid = pomp_msg_get_id(msg);
	switch (msgid) {
	case MSG_SET_PATTERN:
		ret = pomp_msg_read(msg, "%ms%ms", &pattern, &resume);
		if (ret < 0) {
			pattern = resume = NULL;
			ULOGE("pomp_msg_read: %s", strerror(-ret));
			return;
		}
		ret = start_pattern(pattern, ut_string_match(resume, "true"));
		if (ret < 0) {
			ULOGE("start_pattern: %s", strerror(-ret));
			return;
		}
		ULOGD("current pattern set to %s", pattern);
		break;

	case MSG_QUIT:
		loop = false;
		ULOGI("exit on user request");
		break;

	case MSG_DUMP_CONFIG:
		ret = pomp_msg_read(msg, "%ms", &config);
		if (ret < 0) {
			pattern = resume = NULL;
			ULOGE("pomp_msg_read: %s", strerror(-ret));
			return;
		}
		ULOGD("%s dump %s config", __func__, config);
		if (ut_string_match("patterns", config))
			patterns_dump_config();
		else if (ut_string_match("platform", config))
			led_drivers_dump_config();
		else if (ut_string_match("global", config))
			global_dump_config();
		else
			ULOGE("no such config: %s", config);
		break;

	case MSG_SET_VALUE:
		ret = command_set_value(msg);
		if (ret < 0)
			ULOGE("command_set_value: %s", strerror(-ret));
		break;
	}
}

static void signal_handler(int signum)
{
	ULOGI("terminating on signal %s(%d)", strsignal(signum), signum);
	loop = false;
	pomp_ctx_wakeup(pomp);
}

static void timer_cb(struct pomp_timer *timer, void *userdata)
{
	int ret;

	ret = player_update();
	if (ret < 0)
		ULOGW("player_update: %s", strerror(-ret));
	if (!player_is_playing()) {
		ret = pomp_timer_clear(timer);
		if (ret < 0)
			ULOGW("pomp_timer_clear: %s", strerror(-ret));
		ULOGI("timer stopped");
	}
}

int ledd_init_impl(const char *global_config, bool skip_plugins)
{
	int i;
	int ret;
	union {
		struct sockaddr_storage addr_str;
		struct sockaddr addr_sock;
	} addr;
	uint32_t addrlen = sizeof(addr.addr_str);
	const char *address;
	struct pomp_loop *loop;

	ret = global_init(global_config);
	if (ret != 0) {
		ULOGE("global_init(%s): %s", global_config, strerror(-ret));
		return ret;
	}

	if (!skip_plugins) {
		ret = plugins_init(global_get_plugins_dir());
		if (ret != 0) {
			ULOGE("drivers_init: %s", strerror(-ret));
			return ret;
		}
	}

	ret = platform_init(global_get_platform_config());
	if (ret != 0) {
		ULOGE("platform_init(%s): %s", global_get_platform_config(),
				strerror(-ret));
		return ret;
	}
	ret = patterns_init(global_get_patterns_config());
	if (ret != 0) {
		ULOGE("patterns_init(%s): %s", global_get_patterns_config(),
				strerror(-ret));
		return ret;
	}

	ret = player_init();
	if (ret != 0) {
		ULOGE("player_init: %s", strerror(-ret));
		return ret;
	}
	pomp = pomp_ctx_new(pomp_event_cb, NULL);
	if (pomp == NULL) {
		ret = -errno;
		ULOGE("pomp_ctx_new: %m");
		return ret;
	}
	loop = pomp_ctx_get_loop(pomp);
	ret = led_driver_register_drivers_in_pomp_loop(loop);
	if (ret < 0) {
		ULOGE("drivers_register_in_pomp_loop: %s", strerror(-ret));
		return ret;
	}
	address = global_get_address();
	ULOGI("ledd listening on address %s", address);
	/* coverity[overrun-buffer-val] */
	ret = pomp_addr_parse(address, &addr.addr_sock, &addrlen);
	if (ret < 0) {
		ULOGE("pomp_addr_parse(%s): %s", address, strerror(-ret));
		return ret;
	}
	ret = pomp_ctx_listen(pomp, &addr.addr_sock, addrlen);
	if (ret < 0) {
		ULOGE("pomp_ctx_listen: %s", strerror(-ret));
		return ret;
	}
	timer = pomp_timer_new(loop, timer_cb, NULL);
	if (timer == NULL) {
		ret = -errno;
		ULOGE("pomp_timer_new: %m");
		return ret;
	}

	for (i = 0; exit_signals[i] != 0; i++)
		signal(exit_signals[i], signal_handler);

	if (global_get_startup_pattern() != NULL)
		start_pattern(global_get_startup_pattern(), false);
	else
		led_driver_paint_it_black();

	return 0;
}

int ledd_get_fd(void)
{
	return pomp_ctx_get_fd(pomp);
}

int ledd_process_events(void)
{
	int ret = 0;

	ret = pomp_ctx_process_fd(pomp);
	if (loop == false)
		return 1;

	return ret;
}

void ledd_cleanup(void)
{
	if (timer != NULL)
		pomp_timer_destroy(timer);
	if (pomp != NULL) {
		pomp_ctx_stop(pomp);
		led_driver_unregister_drivers_from_pomp_loop(
				pomp_ctx_get_loop(pomp));
		pomp_ctx_destroy(pomp);
	}
	player_cleanup();
	patterns_cleanup();
	platform_cleanup();
	plugins_cleanup();
	global_cleanup();
}
