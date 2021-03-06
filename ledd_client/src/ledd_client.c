/**
 * @file ledd_client.c
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

#include <sys/socket.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lualib.h>
#include <lauxlib.h>

#include <libpomp.h>

#include <ledd_client.h>

#define LEDD_MSG_SET_PATTERN 0
#define LEDD_MSG_QUIT 1
#define LEDD_MSG_DUMP_CONFIG 2
#define LEDD_MSG_SET_VALUE 3

struct ledd_client {
	struct pomp_ctx *pomp;
	struct ledd_client_ops ops;
	char *address;
	void *userdata;
};

static void pomp_event_cb(struct pomp_ctx *ctx, enum pomp_event event,
		struct pomp_conn *conn, const struct pomp_msg *msg,
		void *userdata)
{
	struct ledd_client *client = userdata;

	if (event == POMP_EVENT_MSG)
		return;

	client->ops.connection_cb(client->userdata,
			event == POMP_EVENT_CONNECTED);
}

char *ledd_client_get_ledd_address(const char *global_conf_path)
{
	int ret;
	lua_State *l = NULL;
	char *address = NULL;

	if (global_conf_path == NULL || *global_conf_path == '\0')
		goto err;
	l = luaL_newstate();
	if (l == NULL)
		goto err;

	/* allow access to lua's standard library */
	luaL_openlibs(l);

	ret = luaL_dofile(l, global_conf_path);
	if (ret != LUA_OK)
		goto err;
	lua_getglobal(l, "address");
	if (lua_isnil(l, -1))
		address = strdup(luaL_checkstring(l, -1));
	if (address == NULL)
		goto err;
	lua_pop(l, 1);

	lua_close(l);

	return address;
err:
	if (l != NULL)
		lua_close(l);

	return strdup(LEDD_DEFAULT_ADDRESS);
}

struct ledd_client *ledd_client_new(const char *address,
		const struct ledd_client_ops *ops, void *userdata)
{
	int old_errno;
	struct ledd_client *client;

	client = calloc(1, sizeof(*client));
	if (client == NULL)
		return NULL;

	client->pomp = pomp_ctx_new(pomp_event_cb, client);
	if (client->pomp == NULL) {
		old_errno = errno;
		goto err;
	}
	if (address == NULL)
		address = LEDD_DEFAULT_ADDRESS;
	client->address = strdup(address);
	if (client->address == NULL) {
		old_errno = errno;
		goto err;
	}
	/* must be done before connect because it can notify */
	client->ops = *ops;
	client->userdata = userdata;

	return client;
err:
	ledd_client_destroy(&client);
	errno = old_errno;

	return NULL;
}

int ledd_client_connect(struct ledd_client *client)
{
	int ret;
	union {
		struct sockaddr_storage addr_str;
		struct sockaddr addr_sock;
	} addr;
	uint32_t addrlen = sizeof(addr.addr_str);

	/* coverity[overrun-buffer-val] */
	ret = pomp_addr_parse(client->address, &addr.addr_sock, &addrlen);
	if (ret < 0)
		return ret;

	return pomp_ctx_connect(client->pomp, &addr.addr_sock, addrlen);
}

int ledd_client_get_fd(struct ledd_client *client)
{
	if (client == NULL)
		return -EINVAL;

	return pomp_ctx_get_fd(client->pomp);
}

int ledd_client_process_events(struct ledd_client *client)
{
	if (client == NULL)
		return -EINVAL;

	return pomp_ctx_process_fd(client->pomp);
}


int ledd_client_set_pattern(struct ledd_client *client, const char *pattern,
		bool resume)
{
	return pomp_ctx_send(client->pomp, LEDD_MSG_SET_PATTERN, "%s%s",
			pattern, resume ? "true" : "false");
}

void ledd_client_destroy(struct ledd_client **client)
{
	struct ledd_client *c;

	if (client == NULL || *client == NULL)
		return;
	c = *client;

	if (c->address != NULL)
		free(c->address);
	if (c->pomp != NULL) {
		pomp_ctx_stop(c->pomp);
		pomp_ctx_destroy(c->pomp);
	}
	memset(c, 0, sizeof(*c));
	free(c);

	*client = NULL;
}
