/**
 * @file file_led_driver.c
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

#include <time.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define ULOG_TAG file_led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(file_led_driver);

#include <ut_utils.h>
#include <ut_string.h>

#include <rs_dll.h>
#include <rs_node.h>

#include <ledd_plugin.h>

#ifndef DEFAULT_BACKING_FILE_PATH
#define DEFAULT_BACKING_FILE_PATH "/tmp/file_led_driver"
#endif /* DEFAULT_BACKING_FILE_PATH */

#define BACKING_FILE_PATH_ENV "FILE_LED_DRIVER_BACKING_FILE_PATH"

struct file_led_driver {
	struct led_driver driver;
	FILE *file;
	struct rs_dll channels;
};

#define to_file_led_driver(d) ut_container_of((d), struct file_led_driver, \
		driver)

struct file_led_channel {
	struct rs_node node;
	struct led_channel channel;
	char *label;
};

#define to_file_led_channel_from_channel(c) ut_container_of((c), \
		struct file_led_channel, channel)
#define to_file_led_channel_from_node(c) ut_container_of((c), \
		struct file_led_channel, node)

static int file_set_value(struct led_channel *channel, uint8_t value)
{
	int ret;
	struct file_led_channel *file_channel =
			to_file_led_channel_from_channel(channel);
	struct file_led_driver *file_driver =
			to_file_led_driver(channel->led->driver);
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	ret = fprintf(file_driver->file, "%s %jd.%.9ld 0x%x\n",
			file_channel->label, (intmax_t)ts.tv_sec, ts.tv_nsec,
			value);

	fflush(file_driver->file);

	return ret < 0 ? -EIO : 0;
}

static void file_channel_destroy(struct led_channel *channel)
{
	struct file_led_channel *file_channel =
			to_file_led_channel_from_channel(channel);
	struct file_led_driver *file_driver;

	if (channel == NULL)
		return;
	file_driver = to_file_led_driver(channel->led->driver);

	rs_dll_remove(&file_driver->channels, &file_channel->node);
	free(file_channel->label);
	memset(file_channel, 0, sizeof(*file_channel));
	free(file_channel);
}

static struct led_channel *file_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters)
{
	int old_errno;
	struct file_led_channel *channel;
	struct file_led_driver *file_driver = to_file_led_driver(driver);

	if (ut_string_is_invalid(led_id) || ut_string_is_invalid(channel_id) ||
			ut_string_is_invalid(parameters) || driver == NULL) {
		errno = EINVAL;
		return NULL;
	}

	channel = calloc(1, sizeof(*channel));
	if (channel == NULL)
		return NULL;
	channel->label = strdup(parameters);
	if (channel->label == NULL)
		goto err;

	rs_dll_push(&file_driver->channels, &channel->node);

	return &channel->channel;
err:
	old_errno = errno;
	file_channel_destroy(&channel->channel);
	errno = old_errno;

	return NULL;
}

static struct file_led_driver file_led_driver = {
	.driver = {
		.name = "file",
		.ops = {
			.channel_new = file_channel_new,
			.channel_destroy = file_channel_destroy,
			.set_value = file_set_value,
			.process_events = NULL,
		},
	},
};

static __attribute__((constructor)) void file_led_driver_init(void)
{
	int ret;
	const char *env;

	ULOGD("%s", __func__);

	env = getenv(BACKING_FILE_PATH_ENV);
	if (env == NULL)
		env = DEFAULT_BACKING_FILE_PATH;
	file_led_driver.file = fopen(env, "we");
	if (file_led_driver.file == NULL) {
		ULOGE("fopen: %m");
		return;
	}
	ULOGI("file led driver backed by file %s", env);

	rs_dll_init(&file_led_driver.channels, NULL);
	ret = led_driver_register(&file_led_driver.driver);
	if (ret < 0)
		ULOGE("led_driver_register %s", strerror(-ret));
}

static __attribute__((destructor)) void file_led_driver_cleanup(void)
{
	ULOGD("%s", __func__);

	if (file_led_driver.file != NULL)
		fclose(file_led_driver.file);
	led_driver_unregister(&file_led_driver.driver);
}

