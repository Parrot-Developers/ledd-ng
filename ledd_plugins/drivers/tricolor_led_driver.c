/*
 * tricolor_led_driver.c
 *
 *  Created on: Apr 27, 2016
 *      Author: ncarrier
 */
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <errno.h>

#define ULOG_TAG tricolor_led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(tricolor_led_driver);

#include <rs_dll.h>

#include <ut_utils.h>
#include <ut_string.h>

#include <ledd_plugin.h>

enum tricolor_channel_name {
	TRICOLOR_CHANNEL_HUE,
	TRICOLOR_CHANNEL_SATURATION,
	TRICOLOR_CHANNEL_VALUE,
};

struct tricolor_led_driver {
	struct led_driver driver;
	struct rs_dll leds;
};

#define to_tricolor_led_driver(d) ut_container_of((d), \
		struct tricolor_led_driver, driver)

struct tricolor_led_channel {
	struct led_channel channel;
	uint8_t value;
	enum tricolor_channel_name name;
	bool exposed;
};

#define to_tricolor_led_channel(c) ut_container_of((c), \
		struct tricolor_led_channel, channel)

struct tricolor_led {
	char *led_id;
	struct rs_node node;
	struct tricolor_led_channel hue;
	struct tricolor_led_channel saturation;
	struct tricolor_led_channel value;
	int ref;
};

#define to_tricolor_led_from_node(n) ut_container_of((n), \
		struct tricolor_led, node)
#define to_tricolor_led_from_hue(c) ut_container_of((c), \
		struct tricolor_led, hue)
#define to_tricolor_led_from_saturation(c) ut_container_of((c), \
		struct tricolor_led, saturation)
#define to_tricolor_led_from_value(c) ut_container_of((c), \
		struct tricolor_led, value)

struct tricolor_color {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

static struct tricolor_led *to_tricolor_led(struct tricolor_led_channel *c)
{
	switch (c->name) {
	case TRICOLOR_CHANNEL_HUE:
		return to_tricolor_led_from_hue(c);

	case TRICOLOR_CHANNEL_SATURATION:
		return to_tricolor_led_from_saturation(c);

	case TRICOLOR_CHANNEL_VALUE:
		return to_tricolor_led_from_value(c);
	}

	return NULL;
}

static void tricolor_color_from_hsv(struct tricolor_color *color,
		uint8_t hue, uint8_t saturation, uint8_t value)
{
	float h; /* in [0, 360) */
	float s; /* in [0, 1] */
	float v; /* in [0, 1] */
	float c; /* chroma */
	float hp; /* h' */
	float k;
	float x;
	float r1;
	float g1;
	float b1;
	float m;

	/* source: https://en.wikipedia.org/wiki/HSL_and_HSV */

	h = 360.f * hue / 256.f;
	s = saturation / 256.f;
	v = value / 256.f;


	c = v * s;
	hp = h / 60.f;

	k = 2.f * ((hp / 2.f) - floor(hp / 2.f));
	k -= 1.f;
	k = fabsf(k);
	x = c * (1.f - k);

	if (hp < 1) {
		r1 = c;
		g1 = x;
		b1 = 0;
	} else if (hp < 2) {
		r1 = x;
		g1 = c;
		b1 = 0;
	} else if (hp < 3) {
		r1 = 0;
		g1 = c;
		b1 = x;
	} else if (hp < 4) {
		r1 = 0;
		g1 = x;
		b1 = c;
	} else if (hp < 5) {
		r1 = x;
		g1 = 0;
		b1 = c;
	} else {
		r1 = c;
		g1 = 0;
		b1 = x;
	}

	m = v - c;

	color->red = floor((r1 + m) * 256.f);
	color->green = floor((g1 + m) * 256.f);
	color->blue = floor((b1 + m) * 256.f);
}

static int tricolor_set_value(struct led_channel *c, uint8_t value)
{
	int ret;
	struct tricolor_led_channel *channel = to_tricolor_led_channel(c);
	struct tricolor_led *led = to_tricolor_led(channel);
	struct tricolor_color color;
	char rgb_led_id[0x100];

	channel->value = value;

	tricolor_color_from_hsv(&color, led->hue.value, led->saturation.value,
			led->value.value);

	snprintf(rgb_led_id, 0x100, "%s_rgb", c->led->id);
	ret = led_driver_set_value(rgb_led_id, "red", color.red);
	if (ret < 0) {
		ULOGE("led_driver_set_value(red, %"PRIu8, color.red);
		return ret;
	}
	ret = led_driver_set_value(rgb_led_id, "green", color.green);
	if (ret < 0) {
		ULOGE("led_driver_set_value(green, %"PRIu8, color.green);
		return ret;
	}

	return led_driver_set_value(rgb_led_id, "blue", color.blue);
}

static void tricolor_led_destroy(struct tricolor_led *led)
{
	if (led == NULL)
		return;

	ut_string_free(&led->led_id);
	memset(led, 0, sizeof(*led));
	free(led);
}

static void tricolor_channel_destroy(struct led_channel *c)
{
	struct tricolor_led_channel *channel = to_tricolor_led_channel(c);
	struct tricolor_led *led = to_tricolor_led(channel);

	channel->exposed = false;
	if (!led->hue.exposed && !led->saturation.exposed
			&& !led->value.exposed)
		tricolor_led_destroy(led);
}

static struct led_channel *tricolor_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters);

static struct tricolor_led_driver tricolor_led_driver = {
	.driver = {
		.name = "tricolor",
		.ops = {
			.channel_new = tricolor_channel_new,
			.channel_destroy = tricolor_channel_destroy,
			.set_value = tricolor_set_value,
		},
	},
};

static struct tricolor_led *tricolor_led_find_by_id(const char *led_id)
{
	struct rs_node *node = NULL;
	struct tricolor_led *led;

	while ((node = rs_dll_next_from(&tricolor_led_driver.leds, node))) {
		led = to_tricolor_led_from_node(node);
		if (ut_string_match(led->led_id, led_id))
			return led;
	}

	errno = ENOENT;

	return NULL;
}

struct rgb_prms {
	char *red_prms;
	char *green_prms;
	char *blue_prms;
};

static int split_rgb_prms(char *parameters, struct rgb_prms *prms)
{
	prms->red_prms = strchr(parameters, '|');
	if (prms->red_prms == NULL)
		return -EINVAL;
	*(prms->red_prms++) = '\0';

	prms->green_prms = strchr(prms->red_prms, '|');
	if (prms->green_prms == NULL)
		return -EINVAL;
	*(prms->green_prms++) = '\0';

	prms->blue_prms = strchr(prms->green_prms, '|');
	if (prms->blue_prms == NULL)
		return -EINVAL;
	*(prms->blue_prms++) = '\0';

	return 0;
}

static struct tricolor_led *tricolor_led_new(const char *led_id,
		const char *parameters)
{
	int ret;
	struct tricolor_led *led = NULL;
	char rgb_led_id[0x100];
	char __attribute__((cleanup(ut_string_free)))*parameters_dup;
	struct rgb_prms prms = {
			.red_prms = "",
			.green_prms = "",
			.blue_prms = "",
	};

	parameters_dup = strdup(parameters);
	if (parameters_dup == NULL)
		return NULL;

	if (strchr(parameters, '|') != NULL) {
		ret = split_rgb_prms(parameters_dup, &prms);
		if (ret < 0) {
			ULOGE("split_rgb_prms: %s", strerror(-ret));
			goto err;
		}
	}

	led = calloc(1, sizeof(*led));
	if (led == NULL)
		return NULL;
	led->hue.name = TRICOLOR_CHANNEL_HUE;
	led->saturation.name = TRICOLOR_CHANNEL_SATURATION;
	led->value.name = TRICOLOR_CHANNEL_VALUE;

	led->led_id = strdup(led_id);
	if (led->led_id == NULL) {
		ret = -errno;
		goto err;
	}
	snprintf(rgb_led_id, 0x100, "%s_rgb", led_id);
	ret = led_new(parameters_dup, rgb_led_id);
	if (ret < 0) {
		ULOGE("led_new(%s)", rgb_led_id);
		goto err;
	}
	ret = led_channel_new(rgb_led_id, "red", prms.red_prms);
	if (ret < 0) {
		ULOGE("led_channel_new(%s, red, %s)", led_id, prms.red_prms);
		goto err;
	}
	ret = led_channel_new(rgb_led_id, "green", prms.green_prms);
	if (ret < 0) {
		ULOGE("led_channel_new(%s, green, %s)", led_id,
				prms.green_prms);
		goto err;
	}
	ret = led_channel_new(rgb_led_id, "blue",  prms.blue_prms);
	if (ret < 0) {
		ULOGE("led_channel_new(%s, blue, %s)", led_id,  prms.blue_prms);
		goto err;
	}

	return led;
err:
	tricolor_led_destroy(led);

	errno = -ret;

	return NULL;
}

static struct led_channel *tricolor_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters)
{
	struct tricolor_led *led;
	bool allocated = false;

	led = tricolor_led_find_by_id(led_id);
	if (led == NULL) {
		allocated = true;
		led = tricolor_led_new(led_id, parameters);
		if (led == NULL)
			return NULL;
		rs_dll_push(&tricolor_led_driver.leds, &led->node);
	}

	if (ut_string_match(channel_id, "hue")) {
		if (led->hue.exposed) {
			ULOGE("multiple hue channels for '%s'", led_id);
			goto err;
		}
		led->hue.exposed = true;
		return &led->hue.channel;
	}
	if (ut_string_match(channel_id, "saturation")) {
		if (led->saturation.exposed) {
			ULOGE("multiple saturation channels for '%s'", led_id);
			goto err;
		}
		led->saturation.exposed = true;
		return &led->saturation.channel;
	}
	if (ut_string_match(channel_id, "value")) {
		if (led->value.exposed) {
			ULOGE("multiple value channels for '%s'", led_id);
			goto err;
		}
		led->value.exposed = true;
		return &led->value.channel;
	}

	ULOGE("invalid tricolor channel name '%s', should be one of 'hue', "
			"'saturation' and 'value'", channel_id);
err:
	if (allocated) {
		rs_dll_remove(&tricolor_led_driver.leds, &led->node);
		tricolor_led_destroy(led);
	}
	errno = EINVAL;

	return NULL;
}

struct color_constant {
	const char *name;
	uint8_t value;
};
static struct color_constant color_constants[] = {
		{
				.name = "red",
				.value = 0,
		},
		{
				.name = "yellow",
				.value = 43,
		},
		{
				.name = "green",
				.value = 85,
		},
		{
				.name = "cyan",
				.value = 128,
		},
		{
				.name = "blue",
				.value = 171,
		},
		{
				.name = "magenta",
				.value = 213,
		},
};

static __attribute__((constructor)) void tricolor_led_driver_init(void)
{
	int ret;
	unsigned i;

	ULOGD("%s", __func__);

	rs_dll_init(&tricolor_led_driver.leds, NULL);

	ret = led_driver_register(&tricolor_led_driver.driver);
	if (ret < 0)
		ULOGE("led_driver_register %s", strerror(-ret));

	for (i = 0; i < UT_ARRAY_SIZE(color_constants); i++) {
		ret = lua_globals_register_int(color_constants[i].name,
				color_constants[i].value,
				LUA_GLOBALS_CONFIG_PATTERNS);
		if (ret < 0)
			ULOGE("lua_globals_register_int(%s) %s",
					color_constants[i].name,
					strerror(-ret));
	}
}

static __attribute__((destructor)) void tricolor_led_driver_cleanup(void)
{
	ULOGD("%s", __func__);

	led_driver_unregister(&tricolor_led_driver.driver);
}

