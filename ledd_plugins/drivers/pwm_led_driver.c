
/**
 * @file pwm_led_driver.c
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
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <argz.h>
#include <envz.h>

#include <ut_utils.h>
#include <ut_string.h>
#include <ut_file.h>

#define ULOG_TAG pwm_led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(pwm_led_driver);

#include <ledd_plugin.h>

#define PWM_DEFAULT_PERIOD_NS_VALUE 1000000
#define PWM_DEFAULT_MAX_DUTY_NS_VALUE 900000
#define LONG_TO_STRING_SIZE 20

struct pwm_led_channel {
	uint32_t max_duty_ns;
	struct led_channel channel;
	int duty_ns_fd;
	int run_fd;
};

struct pwm_parameters {
	char pwm_idx[LONG_TO_STRING_SIZE];
	uint32_t max_duty_ns;
	char period_ns[LONG_TO_STRING_SIZE];
};

#define to_pwm_channel(c) ut_container_of((c), struct pwm_led_channel, channel)

static int get_pwm_file_fd(const char *pwm_path, const char *file, int mode)
{
	int ret;
	char __attribute__((cleanup(ut_string_free))) *path = NULL;

	ret = asprintf(&path, "%s/%s", pwm_path, file);
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

static int init_channel_state(struct pwm_led_channel *channel,
		const char *pwm_path, const struct pwm_parameters *prms)
{
	int ret;
	const char *file;
	int __attribute__((cleanup(ut_file_fd_close))) period_fd = -1;
	ssize_t sret;

	file = "period_ns";
	period_fd = get_pwm_file_fd(pwm_path, file, O_RDWR);
	if (period_fd < 0) {
		ULOGE("get_pwm_file_fd(%s, %s): %s", pwm_path, file,
				strerror(-period_fd));
		return period_fd;
	}
	sret = write(period_fd, prms->period_ns,
			UT_ARRAY_SIZE(prms->period_ns));
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(%s[%d]): %m", file, period_fd);
		return ret;
	}
	sret = write(channel->duty_ns_fd, "0", 1);
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(duty_ns[%s]): %m", pwm_path);
		return ret;
	}
	sret = write(channel->run_fd, "1", 1);
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(run_ns[%d]): %m", channel->run_fd);
		return ret;
	}

	return 0;
}

static void cleanup_channel(struct pwm_led_channel *channel)
{
	ut_file_fd_close(&channel->run_fd);
	ut_file_fd_close(&channel->duty_ns_fd);
}

static int parse_channel_parameters(const char *parameters,
		struct pwm_parameters *prms)
{
	int ret;
	char __attribute__((cleanup(ut_string_free))) *envz = NULL;
	size_t envz_len;
	unsigned i;
	const char *value;
	long long_val;
	char *endptr;
	const char *config_key;

	envz = strdup(parameters);
	if (envz == NULL) {
		ret = -errno;
		ULOGE("strdup: %m");
		return ret;
	}
	envz_len = strlen(envz);

	/* transform parameters into a proper envz */
	for (i = 0; i < envz_len; i++)
		if (envz[i] == ' ')
			envz[i] = '\0';

	/* parse pwm_index */
	config_key = "pwm_index";
	value = envz_get(envz, envz_len, config_key);
	if (value == NULL) {
		ULOGE("missing required %s value", config_key);
		return -EINVAL;
	}
	long_val = strtol(value, &endptr, 0);
	if (*endptr != '\0' || long_val < 0) {
		ULOGE("invalid %s value %s %ld", config_key, value, long_val);
		return -EINVAL;
	}
	snprintf(prms->pwm_idx, LONG_TO_STRING_SIZE, "%ld", long_val);

	/* parse max_duty_ns */
	config_key = "max_duty_ns";
	value = envz_get(envz, envz_len, config_key);
	if (value == NULL) {
		prms->max_duty_ns = PWM_DEFAULT_MAX_DUTY_NS_VALUE;
	} else {
		long_val = strtol(value, &endptr, 0);
		if (*endptr != '\0' || long_val < 0 ||
				(uintmax_t)long_val > (uintmax_t)UINT32_MAX) {
			ULOGE("invalid %s value %s %ld", config_key, value,
					long_val);
			return -EINVAL;
		}
		prms->max_duty_ns = long_val;
	}

	/* parse period_ns */
	config_key = "period_ns";
	value = envz_get(envz, envz_len, config_key);
	if (value == NULL) {
		long_val = PWM_DEFAULT_PERIOD_NS_VALUE;
	} else {
		long_val = strtol(value, &endptr, 0);
		if (*endptr != '\0' || long_val < 0 ||
				(uintmax_t)long_val > (uintmax_t)UINT32_MAX) {
			ULOGE("invalid %s value %s %ld", config_key, value,
					long_val);
			return -EINVAL;
		}
	}
	snprintf(prms->period_ns, LONG_TO_STRING_SIZE, "%ld", long_val);

	/* long_val is still period_ns here */
	if (prms->max_duty_ns > (uint32_t)long_val) {
		ULOGE("max_duty_ns(%"PRIu32") > period_ns(%s)",
				prms->max_duty_ns, prms->period_ns);
		return -EINVAL;
	}

	return 0;
}

static int init_channel(struct pwm_led_channel *channel, const char *parameters)
{
	ssize_t sret;
	int ret;
	char __attribute__((cleanup(ut_string_free))) *pwm_path = NULL;
	int __attribute__((cleanup(ut_file_fd_close))) export_fd = -1;
	const char *file;
	struct pwm_parameters prms;

	ret = parse_channel_parameters(parameters, &prms);
	if (ret < 0) {
		ULOGE("parse_channel_parameters: %s", strerror(-ret));
		return ret;
	}

	/* export the pwm */
	file = "export";
	export_fd = get_pwm_file_fd("/sys/class/pwm/", file, O_WRONLY);
	if (export_fd < 0) {
		ULOGE("get_gpio_file_fd(/sys/class/pwm/, %s): %s", file,
				strerror(-export_fd));
		return export_fd;

	}
	sret = write(export_fd, prms.pwm_idx, strlen(prms.pwm_idx) + 1);
	if (sret == -1 && errno != EBUSY) {
		ret = -errno;
		ULOGE("write(%s[%d]): %m", file, export_fd);
		return ret;
	}

	ret = asprintf(&pwm_path, "/sys/class/pwm/pwm_%s", prms.pwm_idx);
	if (ret == -1) {
		pwm_path = NULL;
		ULOGE("asprintf error");
		return -ENOMEM;
	}

	file = "duty_ns";
	ret = get_pwm_file_fd(pwm_path, file, O_RDWR);
	if (ret < 0) {
		ULOGE("get_pwm_file_fd(%s, %s): %s", pwm_path, file,
				strerror(-ret));
		return ret;

	}
	channel->duty_ns_fd = ret;

	file = "run";
	ret = get_pwm_file_fd(pwm_path, file, O_RDWR);
	if (ret < 0) {
		ULOGE("get_pwm_file_fd(%s, %s): %s", pwm_path, file,
				strerror(-ret));
		return ret;

	}
	channel->run_fd = ret;
	channel->max_duty_ns = prms.max_duty_ns;

	return init_channel_state(channel, pwm_path, &prms);
}

static void pwm_channel_destroy(struct led_channel *channel)
{
	struct pwm_led_channel *pwm_channel;

	if (channel == NULL)
		return;

	pwm_channel = to_pwm_channel(channel);
	cleanup_channel(pwm_channel);
	free(pwm_channel);
}

static struct led_channel *pwm_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters)
{
	int ret;
	int old_errno;
	struct pwm_led_channel *channel;

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

	return &channel->channel;
err:
	old_errno = errno;
	pwm_channel_destroy(&channel->channel);
	errno = old_errno;

	return NULL;
}

static int pwm_set_value(struct led_channel *channel, uint8_t value)
{
	int ret;
	ssize_t sret;
	char duty_ns_val[0x100];
	int value_ns;
	struct pwm_led_channel *pwm_channel;

	pwm_channel = to_pwm_channel(channel);

	value_ns = (value * pwm_channel->max_duty_ns) / LED_CHANNEL_MAX;
	snprintf(duty_ns_val, 0x100, "%d\n", value_ns);
	sret = write(pwm_channel->duty_ns_fd, duty_ns_val, strlen(duty_ns_val));
	if (sret == -1) {
		ret = -errno;
		ULOGE("write(duty_ns): %m");
		return ret;
	}

	return 0;
}

static struct led_driver pwm_led_driver = {
		.name = "pwm",
		.ops = {
			.channel_new = pwm_channel_new,
			.channel_destroy = pwm_channel_destroy,
			.set_value = pwm_set_value,
		},
};

static __attribute__((constructor)) void pwm_led_driver_init(void)
{
	int ret;

	ret = led_driver_register(&pwm_led_driver);
	if (ret < 0)
		ULOGE("led_driver_register %s", strerror(-ret));
}

static __attribute__((destructor)) void pwm_led_driver_cleanup(void)
{
	led_driver_unregister(&pwm_led_driver);
}
