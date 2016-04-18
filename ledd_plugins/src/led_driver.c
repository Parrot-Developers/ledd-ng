/**
 * @file led_driver.c
 * @brief
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>

#define ULOG_TAG led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(led_driver);

#include <ut_utils.h>
#include <ut_string.h>

#include <rs_dll.h>

#include <ledd_plugin.h>

#include "led_driver_priv.h"

static struct led_driver *led_drivers[LED_MAX_DRIVERS];
static unsigned nb_drivers;
static struct rs_dll leds;

static bool driver_is_invalid(const struct led_driver *driver)
{
	return driver == NULL ||
			driver->name == NULL ||
			*driver->name == '\0' ||
			driver->ops.channel_destroy == NULL ||
			driver->ops.channel_new == NULL ||
			driver->ops.set_value == NULL;
}

/* driver API */
void led_driver_init(void)
{
	rs_dll_init(&leds, NULL);
}

int led_driver_register(struct led_driver *driver)
{
	if (driver_is_invalid(driver))
		return -EINVAL;
	if (nb_drivers == LED_MAX_DRIVERS)
		return -ENOMEM;

	led_drivers[nb_drivers] = driver;
	nb_drivers++;

	return 0;
}

void led_driver_unregister(const struct led_driver *driver)
{
	unsigned i;

	for (i = 0; i < nb_drivers; i++) {
		if (led_drivers[i] != driver)
			continue;

		for (i++; i < nb_drivers; i++)
			led_drivers[i - 1] = led_drivers[i];
		nb_drivers--;
		return;
	}
}

/* private API */
static struct led_driver *get_driver_by_name(const char *name)
{
	unsigned i;

	for (i = 0; i < nb_drivers; i++)
		if (ut_string_match(led_drivers[i]->name, name))
			return led_drivers[i];

	errno = -ESRCH;

	return NULL;
}

static RS_NODE_MATCH_STR_MEMBER(led, id, node)

#define to_led(n) ut_container_of(n, struct led, node)

static struct led *get_led_by_id(const char *led_id)
{
	struct rs_node *node;

	node = rs_dll_find_match(&leds, led_match_str_id, led_id);
	if (node != NULL)
		return to_led(node);

	errno = ESRCH;

	return NULL;
}

static struct led_channel *get_channel_by_id(struct led *led,
		const char *channel_id)
{
	unsigned i;

	for (i = 0; i < led->nb_channels; i++)
		if (ut_string_match(channel_id, led->channels[i]->id))
			return led->channels[i];

	errno = ESRCH;

	return NULL;
}

static int led_add_channel(struct led *led, struct led_channel *channel)
{
	if (led->nb_channels == LED_MAX_CHANNELS_PER_LED)
		return -ENOMEM;

	led->channels[led->nb_channels] = channel;
	led->nb_channels++;

	return 0;
}

static void led_remove_channel(struct led *led, struct led_channel *channel)
{
	unsigned i;

	for (i = 0; i < led->nb_channels; i++)
		if (ut_string_match(channel->id, led->channels[i]->id)) {
			/* channel found, shift all the channels above */
			for (i++; i < led->nb_channels; i++)
				led->channels[i - 1] = led->channels[i];
			led->nb_channels--;
			return;
		}
}

static void destroy_channel(struct led_channel *channel)
{
	struct led_driver *driver;

	if (channel == NULL)
		return;
	driver = channel->led->driver;

	led_remove_channel(channel->led, channel);

	free(channel->id);
	driver->ops.channel_destroy(channel);
}

/* preserves errno */
static void led_destroy(struct led *led)
{
	int old_errno;

	if (led == NULL)
		return;

	while (led->nb_channels != 0)
		destroy_channel(led->channels[0]);
	rs_dll_remove(&leds, &led->node);
	old_errno = errno;
	free(led->id);
	memset(led, 0, sizeof(*led));
	free(led);
	errno = old_errno;
}

static void driver_pomp_cb(int fd, uint32_t revents, void *userdata)
{
	static const int in = 1;
	static const int out = 4;
	int events = 0;
	struct led_driver *driver = userdata;

	events = POMP_FD_EVENT_IN;
	if (driver->rw)
		events |= POMP_FD_EVENT_OUT;

	if (revents & POMP_FD_EVENT_IN)
		events |= in;
	if (revents & POMP_FD_EVENT_OUT)
		events |= out;

	driver->ops.process_events(driver, events);
}

int led_new(const char *driver_name, const char *led_id)
{
	struct led_driver *driver;
	struct led *led;

	ULOGD("%s(%s, %s)", __func__, driver_name, led_id);

	driver = get_driver_by_name(driver_name);
	if (driver == NULL) {
		ULOGE("no driver named %s found", driver_name);
		return -ESRCH;
	}
	led = calloc(1, sizeof(*led));
	if (led == NULL)
		return -errno;

	led->id = strdup(led_id);
	if (led->id == NULL)
		goto err;
	led->driver = driver;
	rs_dll_enqueue(&leds, &led->node);

	return 0;
err:
	led_destroy(led);

	return -errno;
}

void led_driver_cleanup(void)
{
	struct rs_node *node;
	struct led *led;

	while (rs_dll_get_count(&leds) != 0) {
		node = rs_dll_pop(&leds);
		led = to_led(node);
		led_destroy(led);
	}
}

int led_channel_new(const char *led_id, const char *channel_id,
		const char *parameters)
{
	int ret;
	struct led *led;
	struct led_channel *channel;
	struct led_driver *driver;

	ULOGD("%s(%s, %s, %s)", __func__, led_id, channel_id, parameters);

	led = get_led_by_id(led_id);
	if (led == NULL)
		return -ESRCH;
	if (led->nb_channels == LED_MAX_CHANNELS_PER_LED)
		return -ENOMEM;
	driver = led->driver;

	channel = driver->ops.channel_new(driver, led_id, channel_id,
			parameters);
	if (channel == NULL)
		return -errno;
	channel->id = strdup(channel_id);
	if (channel->id == NULL) {
		ret = -errno;
		goto err;
	}
	channel->led = led;
	ret = led_add_channel(led, channel);
	if (ret < 0)
		goto err;

	return 0;
err:
	destroy_channel(channel);
	return ret;
}

void led_channel_destroy(const char *led_id, const char *channel_id)
{
	unsigned i;
	struct led *led;
	struct led_channel *channel;

	led = get_led_by_id(led_id);
	if (led == NULL)
		return;

	for (i = 0; i < led->nb_channels; i++) {
		channel = led->channels[i];
		if (ut_string_match(channel->id, channel_id))
			destroy_channel(channel);
	}
}

int led_driver_set_value(const char *led_id, const char *channel_id,
		uint8_t value)
{
	struct led *led;
	struct led_channel *channel;

	led = get_led_by_id(led_id);
	if (led == NULL)
		return -ESRCH;
	channel = get_channel_by_id(led, channel_id);
	if (channel == NULL)
		return -ESRCH;

	/* don't call the driver if the value hasn't changed */
	if (channel->value == value)
		return 0;

	channel->value = value;

	return led->driver->ops.set_value(channel, value);
}

int led_driver_register_drivers_in_pomp_loop(struct pomp_loop *loop)
{
	int ret;
	unsigned i = nb_drivers;
	struct led_driver *driver;
	uint32_t events;

	while (i--) {
		driver = led_drivers[i];
		if (driver->ops.process_events != NULL) {
			ULOGD("register driver %s in pomp loop", driver->name);
			events = POMP_FD_EVENT_IN;
			if (driver->rw)
				events |= POMP_FD_EVENT_OUT;
			ret = pomp_loop_add(loop, driver->fd, events,
					driver_pomp_cb, driver);
			if (ret < 0) {
				ULOGE("pomp_loop_add: %s", strerror(-ret));
				return ret;
			}
		}
	}

	return 0;
}

void led_driver_unregister_drivers_from_pomp_loop(struct pomp_loop *loop)
{
	int ret;
	unsigned i = nb_drivers;
	struct led_driver *driver;

	while (i--) {
		driver = led_drivers[i];
		if (driver->ops.process_events != NULL) {
			ULOGD("unregister driver %s from pomp loop",
					driver->name);
			ret = pomp_loop_remove(loop, driver->fd);
			if (ret < 0)
				ULOGW("pomp_loop_add: %s", strerror(-ret));
		}
	}
}

static int apply_value_to_led(struct led *led, uint8_t value)
{
	int ret;
	int result = 0;
	int i;
	struct led_channel *channel;

	for (i = 0; i < led->nb_channels; i++) {
		channel = led->channels[i];
		ret = led_driver_set_value(led->id, channel->id, value);
		if (ret < 0) {
			ULOGW("led_driver_set_value(%s, %"PRIu8"): %s",
					channel->id, value, strerror(-ret));
			result = ret;
		}
	}

	return result;
}

int led_driver_paint_it_black(void)
{
	int ret;
	int result = 0;
	struct rs_node *node = NULL;
	struct led *led;

	while ((node = rs_dll_next_from(&leds, node))) {
		led = to_led(node);
		ret = apply_value_to_led(led, 0);
		if (ret < 0) {
			ULOGW("apply_value_to_led(%s, 0): %s", led->id,
					strerror(-ret));
			result = ret;
		}
	}

	return result;
}

int led_driver_apply_default_value(const char *led_id, uint8_t value)
{
	struct rs_node *node = NULL;

	node = rs_dll_find_match(&leds, led_match_str_id, led_id);
	if (node == NULL)
		return -ESRCH;

	return apply_value_to_led(to_led(node), value);
}

void led_drivers_dump_config(void)
{
	const struct led *led;
	const struct led_channel *channel;
	struct rs_node *node = NULL;
	uint8_t i;

	while ((node = rs_dll_next_from(&leds, node))) {
		led = to_led(node);
		ULOGI("led %s (driver \"%s\"):", led->id, led->driver->name);
		for (i = 0; i < led->nb_channels; i++) {
			channel = led->channels[i];
			ULOGI("\tchannel[%"PRIu8"] %s", i, channel->id);
		}
	}
}
