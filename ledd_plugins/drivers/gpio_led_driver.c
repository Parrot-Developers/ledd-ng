
/**
 * @file gpio_led_driver.c
 * @brief
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#include <sys/types.h>
#include <sys/timerfd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <rs_node.h>
#include <rs_dll.h>

#include <ut_utils.h>
#include <ut_string.h>
#include <ut_file.h>

#define ULOG_TAG gpio_led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(gpio_led_driver);

#include <ledd_plugin.h>

#define GPIO_DIR "/sys/class/gpio/"

#define ONE_SECOND_IN_NS 1000000000

#ifndef GPIO_LED_DRIVER_FREQUENCY_HZ
#define GPIO_LED_DRIVER_FREQUENCY_HZ 1000
#endif /* GPIO_LED_DRIVER_FREQUENCY_HZ */

#define GPIO_LED_DRIVER_BIT_BANGING_PRECISION_ENV \
	"GPIO_LED_DRIVER_BIT_BANGING_PRECISION"

struct gpio_led_channel {
	struct led_channel channel;
	int value_fd;
	uint8_t value;
	struct rs_node node;
};

struct gpio_led_driver {
	struct rs_dll channels;
	struct led_driver driver;
	uint8_t precision;
	uint8_t tick;
};

#define to_gpio_channel(c) ut_container_of((c), struct gpio_led_channel, \
		channel)

#define to_gpio_driver(d) ut_container_of((d), struct gpio_led_driver, driver)

static int get_gpio_file_fd(const char *gpio_path, const char *file, int mode)
{
	int ret;
	char __attribute__((cleanup(ut_string_free))) *path = NULL;

	ret = asprintf(&path, "%s/%s", gpio_path, file);
	if (ret == -1) {
		path = NULL;
		ULOGE("asprintf error (%s)", file);
		return -ENOMEM;
	}
	ret = open(path, mode | O_CLOEXEC);
	if (ret == -1) {
		ret = -errno;
		ULOGE("open(%s): %m", path);
		return ret;
	}

	return ret;
}

static int export_gpio(const char *gpio)
{
	int ret;
	const char *file;
	int __attribute__((cleanup(ut_file_fd_close))) export_fd = -1;
	ssize_t sret;

	file = "export";
	export_fd = get_gpio_file_fd(GPIO_DIR, file, O_WRONLY);
	if (export_fd < 0) {
		ULOGE("get_gpio_file_fd(" GPIO_DIR ", %s): %s", file,
				strerror(-export_fd));
		return export_fd;
	}
	sret = write(export_fd, gpio, strlen(gpio) + 1);
	if (sret == -1 && errno != EBUSY) {
		ret = -errno;
		ULOGE("write(%s[%d]): %m", file, export_fd);
		return ret;
	}

	return 0;
}

static int set_gpio_direction(const char *gpio_path, const char *direction_val)
{
	int ret;
	const char *file;
	ssize_t sret;
	int __attribute__((cleanup(ut_file_fd_close))) direction_fd = -1;

	file = "direction";
	direction_fd = get_gpio_file_fd(gpio_path, file, O_RDWR);
	if (direction_fd < 0) {
		ULOGE("get_gpio_file_fd(%s, %s): %s", gpio_path, file,
				strerror(-direction_fd));
		return direction_fd;

	}
	sret = write(direction_fd, direction_val, strlen(direction_val) + 1);
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(direction[%d]): %m", direction_fd);
		return ret;
	}

	return 0;
}

static int init_channel_state(struct gpio_led_channel *channel,
		const char *gpio_path, const char *gpio)
{
	int ret;
	ssize_t sret;
	const char *file;
	char value_val[] = "0\n";

	/* export the gpio iif gpio isn't a directory */
	if (!ut_file_is_dir(gpio)) {
		ret = export_gpio(gpio);
		if (ret < 0) {
			ULOGE("export_gpio(%s): %s", gpio, strerror(-ret));
			return ret;
		}
	}

	/* get the "value" sysfs file desc */
	file = "value";
	ret = get_gpio_file_fd(gpio_path, file, O_RDWR);
	if (ret < 0) {
		ULOGE("get_gpio_file_fd(%s, %s): %s", gpio_path, file,
				strerror(-ret));
		return ret;
	}
	channel->value_fd = ret;

	if (ut_file_exists("%s/%s", gpio_path, file)) {
		ret = set_gpio_direction(gpio_path, "out\n");
		if (ret < 0) {
			ULOGE("set_gpio_direction(%s, out): %s", gpio_path,
					strerror(-ret));
			return ret;
		}
	}

	/* set to off by default */
	sret = write(channel->value_fd, value_val, UT_ARRAY_SIZE(value_val));
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(run_ns[%d]): %m", channel->value_fd);
		return ret;
	}

	return 0;
}

static void cleanup_channel(struct gpio_led_channel *channel)
{
	ut_file_fd_close(&channel->value_fd);
}

static int init_channel(struct gpio_led_channel *channel, const char *gpio)
{
	int ret;
	char __attribute__((cleanup(ut_string_free)))*gpio_path = NULL;

	if (!ut_file_is_dir(gpio)) {
		/*
		 * if the gpio string isn't a directory, we consider it's a
		 * number, to be written in the /sys/class/gpio/export file in
		 * order to gain access to that gpio
		 */
		ret = asprintf(&gpio_path, GPIO_DIR "gpio%s", gpio);
		if (ret == -1) {
			gpio_path = NULL;
			ULOGE("asprintf error");
			return -ENOMEM;
		}
	} else {
		/* otherwise, it's the path to the gpio's directory */
		gpio_path = strdup(gpio);
		if (gpio_path == NULL) {
			ULOGE("strdup: %m");
			return -errno;
		}
	}

	return init_channel_state(channel, gpio_path, gpio);
}

static void gpio_channel_destroy(struct led_channel *channel)
{
	struct gpio_led_channel *gpio_channel;
	struct gpio_led_driver *gpio_driver;

	if (channel == NULL)
		return;

	gpio_driver = to_gpio_driver(channel->led->driver);
	gpio_channel = to_gpio_channel(channel);
	rs_dll_remove(&gpio_driver->channels, &gpio_channel->node);
	cleanup_channel(gpio_channel);
	free(gpio_channel);
}

static struct led_channel *gpio_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters)
{
	int ret;
	int old_errno;
	struct gpio_led_channel *channel;
	struct gpio_led_driver *gpio_driver = to_gpio_driver(driver);

	if (ut_string_is_invalid(led_id) || ut_string_is_invalid(channel_id) ||
			ut_string_is_invalid(parameters) || driver == NULL) {
		errno = EINVAL;
		return NULL;
	}

	channel = calloc(1, sizeof(*channel));
	if (channel == NULL)
		return NULL;

	ret = init_channel(channel, parameters);
	if (ret < 0) {
		ULOGE("init_channel: %s", strerror(-ret));
		errno = -ret;
		goto err;
	}

	rs_dll_push(&gpio_driver->channels, &channel->node);

	return &channel->channel;
err:
	old_errno = errno;
	gpio_channel_destroy(&channel->channel);
	errno = old_errno;

	return NULL;
}

static int switch_gpio_on(struct gpio_led_channel *channel, bool on)
{
	int ret;
	ssize_t sret;
	char value_val[0x10];

	snprintf(value_val, 0x10, "%d\n", on);
	sret = write(channel->value_fd, value_val, strlen(value_val));
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(value): %m");
		return ret;
	}

	return 0;
}

static bool bit_banging_enabled(struct gpio_led_driver *driver)
{
	return driver->precision > 0;
}

static int gpio_set_value(struct led_channel *channel, uint8_t value)
{
	int ret;
	struct gpio_led_channel *gpio_channel;
	struct gpio_led_driver *driver = to_gpio_driver(channel->led->driver);

	gpio_channel = to_gpio_channel(channel);

	/*
	 * if bit banging isn't enabled, we switch on the light iif the value is
	 * in the upper half, otherwise, the timerfd callback will take care of
	 * switching on the channel according to gpio_channel->vvalue
	 */
	if (!bit_banging_enabled(driver)) {
		ret = switch_gpio_on(gpio_channel, value > 127);
		if (ret < 0)
			ULOGE("switch_gpio_on: %s", strerror(-ret));
	}
	gpio_channel->value = value;

	return 0;
}

static void gpio_led_driver_process_events(struct led_driver *driver,
		int events)
{
	int ret;
	struct rs_node *node = NULL;
	ssize_t sret;
	uint64_t expired;
	struct gpio_led_channel *channel;
	struct gpio_led_driver *d = to_gpio_driver(driver);
	uint8_t value;
	float fvalue;

	sret = read(driver->fd, &expired, sizeof(expired));
	if (sret == -1)
		ULOGE("read: %m");

	d->tick++;
	d->tick %= d->precision;
	while ((node = rs_dll_next_from(&d->channels, node)) != NULL) {
		channel = ut_container_of(node, struct gpio_led_channel, node);
		/* in [0, d->precision] */
		fvalue = (1.f * channel->value * d->precision / 255.f);
		value = round(fvalue);
		ret = switch_gpio_on(channel, value > (d->tick % d->precision));
		if (ret < 0)
			ULOGE("switch_gpio_on: %s", strerror(-ret));
	}
}

static struct gpio_led_driver gpio_led_driver = {
		.driver = {
			.name = "gpio",
			.fd = -1,
			.ops = {
				.channel_new = gpio_channel_new,
				.channel_destroy = gpio_channel_destroy,
				.set_value = gpio_set_value,
			},
		},
		.tick = 0,
		.precision = 0,
};

static void setup_bit_banging(struct gpio_led_driver *driver)
{
	int ret;
	struct itimerspec its = {
			.it_interval = {
					.tv_sec = 0,
					.tv_nsec = ONE_SECOND_IN_NS /
						GPIO_LED_DRIVER_FREQUENCY_HZ,
			},
			.it_value = {
					.tv_sec = 0,
					.tv_nsec = ONE_SECOND_IN_NS /
						GPIO_LED_DRIVER_FREQUENCY_HZ,
			},
	};

	ULOGI("bit banging enabled with precision %"PRIu8, driver->precision);
	driver->driver.fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (driver->driver.fd == -1) {
		ULOGE("timerfd_create: %m");
		return;
	}
	ret = timerfd_settime(driver->driver.fd, 0, &its, NULL);
	if (ret == -1) {
		ULOGE("timerfd_settime: %m");
		return;
	}

	driver->driver.ops.process_events = gpio_led_driver_process_events;
}

static __attribute__((constructor)) void gpio_led_driver_init(void)
{
	int ret;
	const char *env;

	env = getenv(GPIO_LED_DRIVER_BIT_BANGING_PRECISION_ENV);
	if (env != NULL)
		gpio_led_driver.precision = atoi(env);
	if (bit_banging_enabled(&gpio_led_driver))
		setup_bit_banging(&gpio_led_driver);

	rs_dll_init(&gpio_led_driver.channels, NULL);

	ret = led_driver_register(&gpio_led_driver.driver);
	if (ret < 0)
		ULOGE("led_driver_register %s", strerror(-ret));
}

static __attribute__((destructor)) void gpio_led_driver_cleanup(void)
{
	led_driver_unregister(&gpio_led_driver.driver);
	ut_file_fd_close(&gpio_led_driver.driver.fd);
}
