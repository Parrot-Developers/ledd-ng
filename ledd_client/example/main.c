/**
 * @file main.c
 * @brief example client demonstating the usage of the ledd_client library.
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
 * At connection with ledd, asks to play the blinking blue pattern (which
 * loops), after 3 seconds, plays the fast_yellow_blink pattern the waits
 * another 2 seconds and quits.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include <error.h>

#include <ut_string.h>
#include <ut_utils.h>

#include <io_mon.h>
#include <io_src.h>
#include <io_src_tmr.h>

#include <ledd_client.h>

struct client {
	struct ledd_client *ledd;
	struct io_mon mon;
	struct io_src src;
	struct io_src_tmr tmr;
	bool notification_played;
	bool connected;
	bool loop;
};

#define from_tmr(tmr) ut_container_of((tmr), struct client, tmr)
#define from_src(src) ut_container_of((src), struct client, src)

static void client_connection_cb(void *userdata, bool connected)
{
	int ret;
	struct client *client = userdata;

	client->connected = connected;
	printf("%sconnected\n", connected ? "" : "dis");
	if (!connected)
		return;

	printf("play periodic pattern\n");
	ret = ledd_client_set_pattern(client->ledd, "blinking_blue", false);
	if (ret < 0)
		error(0, -ret, "ledd_client_set_pattern");
}

static void client_tmr_cb(struct io_src_tmr *tmr, uint64_t *nbexpired)
{
	int ret;
	struct client *client = from_tmr(tmr);

	if (!client->connected) {
		printf("timeout !\n");
		client->loop = false;
		return;
	}
	if (client->notification_played) {
		printf("quit !\n");
		client->loop = false;
		return;
	}

	printf("play notification pattern\n");
	ret = ledd_client_set_pattern(client->ledd, "fast_yello_blink", true);
	if (ret < 0)
		error(0, -ret, "ledd_client_set_pattern");
	ret = io_src_tmr_set(&client->tmr, 2000);
	if (ret < 0)
		error(0, -ret, "io_src_tmr_set");
	client->notification_played = true;
}

static void client_src_cb(struct io_src *src)
{
	int ret;
	struct client *client = from_tmr(src);

	ret = ledd_client_process_events(client->ledd);
	if (ret < 0)
		error(0, -ret, "io_mon_process_event");
}

static const struct ledd_client_ops client_ops = {
	.connection_cb = client_connection_cb,
};

static void client_cleanup(struct client *client)
{
	io_mon_remove_sources(&client->mon, &client->src,
			io_src_tmr_get_source(&client->tmr), NULL);
	io_src_tmr_clean(&client->tmr);
	io_src_clean(&client->src);
	io_mon_clean(&client->mon);
	ledd_client_destroy(&client->ledd);
}

static int client_init(struct client *client)
{
	int ret;
	const char *global_conf_path;
	char *address = NULL;

	global_conf_path = getenv("LEDD_CLIENT_GLOBAL_CONF");
	address = ledd_client_get_ledd_address(global_conf_path);
	printf("connecting to ledd using address %s\n", address);
	client->ledd = ledd_client_new(address, &client_ops, client);
	ut_string_free(&address);
	if (client->ledd == NULL) {
		ret = -errno;
		error(0, errno, "ledd_client_new");
		goto err;
	}

	ret = io_mon_init(&client->mon);
	if (ret < 0) {
		error(0, -ret, "io_mon_init");
		goto err;
	}
	ret = io_src_init(&client->src, ledd_client_get_fd(client->ledd),
			IO_IN, client_src_cb);
	if (ret < 0) {
		error(0, -ret, "io_src_init");
		goto err;
	}
	ret = io_src_tmr_init(&client->tmr, client_tmr_cb);
	if (ret < 0) {
		error(0, -ret, "io_src_tmr_init");
		goto err;
	}

	ret = io_src_tmr_set(&client->tmr, 3000);
	if (ret < 0) {
		error(0, -ret, "io_src_tmr_init");
		goto err;
	}
	ret = io_mon_add_sources(&client->mon, &client->src,
			io_src_tmr_get_source(&client->tmr), NULL);
	if (ret < 0) {
		error(0, -ret, "io_src_tmr_init");
		goto err;
	}
	ret = ledd_client_connect(client->ledd);
	if (ret < 0) {
		error(0, -ret, "ledd_client_connect");
		goto err;
	}

	client->loop = true;

	return 0;
err:
	client_cleanup(client);

	return ret;
}

/*
 * returns 1 if the loop must continue, 0 if not and a negative errno-compatible
 * value on error
 */
static int client_poll(struct client *client)
{
	int ret;

	ret = io_mon_poll(&client->mon, -1);
	if (ret < 0)
		return ret;

	return client->loop;
}

static struct client client;

int main(void)
{
	int ret;

	ret = client_init(&client);
	if (ret < 0)
		error(EXIT_FAILURE, -ret, "client_init");

	do {
		ret = client_poll(&client);
		if (ret < 0)
			error(EXIT_FAILURE, -ret, "client_poll");
	} while (ret == 1);

	client_cleanup(&client);

	return EXIT_SUCCESS;
}
