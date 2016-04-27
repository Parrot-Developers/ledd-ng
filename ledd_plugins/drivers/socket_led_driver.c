/*
 * socket_led_driver.c
 *
 *  Created on: Apr 20, 2016
 *      Author: ncarrier
 */
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <string.h>
#include <errno.h>

#include <libpomp.h>

#define ULOG_TAG socket_led_driver
#include <ulog.h>
ULOG_DECLARE_TAG(socket_led_driver);

#include <ut_utils.h>
#include <ut_bits.h>
#include <ut_file.h>

#include <rs_dll.h>
#include <rs_node.h>

#include <ledd_plugin.h>

#define SOCKET_LED_DRIVER_ADDRESS_ENV "SOCKET_LED_DRIVER_ADDRESS"
#ifndef SOCKET_LED_DRIVER_ADDRESS_DEFAULT
#define SOCKET_LED_DRIVER_ADDRESS_DEFAULT "unix:/tmp/socket_led_driver.sock"
#endif /* SOCKET_LED_DRIVER_ADDRESS_DEFAULT */

struct socket_led_driver {
	struct led_driver driver;
	struct pomp_ctx *pomp;
};

#define to_socket_led_driver(d) ut_container_of((d), struct socket_led_driver, \
		driver)

static int socket_set_value(struct led_channel *channel, uint8_t value)
{
	int ret;
	struct socket_led_driver *driver =
			to_socket_led_driver(channel->led->driver);

	ret = pomp_ctx_send(driver->pomp, 0, "%s%s%"PRIu8, channel->led->id,
			channel->id, value);
	if (ret < 0)
		ULOGW("pomp_ctx_send: %s", strerror(-ret));

	return ret;
}

static void socket_channel_destroy(struct led_channel *channel)
{
	free(channel);
}

static struct led_channel *socket_channel_new(struct led_driver *driver,
		const char *led_id, const char *channel_id,
		const char *parameters)
{
	return calloc(1, sizeof(struct led_channel));
}

static void socket_process_events(struct led_driver *driver, int events)
{
	int ret;
	struct socket_led_driver *sock_driver = to_socket_led_driver(driver);

	ret = pomp_ctx_process_fd(sock_driver->pomp);
	if (ret < 0)
		ULOGE("pomp_ctx_wait_and_process: %s", strerror(-ret));
}

static void socket_led_driver_pomp_cb(struct pomp_ctx *ctx,
		enum pomp_event event, struct pomp_conn *conn,
		const struct pomp_msg *msg, void *userdata)
{
	switch (event) {
	case POMP_EVENT_CONNECTED:
		ULOGD("A client is connected");
		break;

	case POMP_EVENT_DISCONNECTED:
		ULOGD("A client got disconnected");
		break;
	default:
		ULOGW("Unknown event : %d", event);
		break;
	}
}

static struct socket_led_driver socket_led_driver = {
	.driver = {
		.name = "socket",
		.ops = {
			.channel_new = socket_channel_new,
			.channel_destroy = socket_channel_destroy,
			.set_value = socket_set_value,
			.process_events = socket_process_events,
		},
	},
};

static int init_pomp_server(struct socket_led_driver *driver,
		const char *address)
{
	int ret;
	union {
		struct sockaddr_storage addr_str;
		struct sockaddr addr_sock;
	} addr;
	uint32_t addrlen = sizeof(addr.addr_str);

	driver->pomp = pomp_ctx_new(socket_led_driver_pomp_cb, NULL);
	if (driver->pomp == NULL) {
		ret = -errno;
		ULOGE("pomp_ctx_new: %m");
		return ret;
	}

	ret = pomp_addr_parse(address, &addr.addr_sock, &addrlen);
	if (ret < 0) {
		ULOGE("pomp_addr_parse(%s): %s", address, strerror(-ret));
		return ret;
	}
	ret = pomp_ctx_listen(driver->pomp, &addr.addr_sock, addrlen);
	if (ret < 0) {
		ULOGE("pomp_ctx_listen: %s", strerror(-ret));
		return ret;
	}
	driver->driver.fd = pomp_ctx_get_fd(driver->pomp);
	driver->driver.rw = false;

	return 0;
}

static __attribute__((constructor)) void socket_led_driver_init(void)
{
	int ret;
	const char *address;

	ULOGD("%s", __func__);


	address = getenv(SOCKET_LED_DRIVER_ADDRESS_ENV);
	if (address == NULL)
		address = SOCKET_LED_DRIVER_ADDRESS_DEFAULT;
	ret = init_pomp_server(&socket_led_driver, address);
	if (ret < 0) {
		ULOGE("init_pomp_server: %s", strerror(-ret));
		return;
	}
	ULOGI("socket led driver listens on socket %s", address);

	ret = led_driver_register(&socket_led_driver.driver);
	if (ret < 0)
		ULOGE("led_driver_register %s", strerror(-ret));
}

static __attribute__((destructor)) void socket_led_driver_cleanup(void)
{
	ULOGD("%s", __func__);

	led_driver_unregister(&socket_led_driver.driver);
}

