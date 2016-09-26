/**
 * @file global.c
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

#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>

#define ULOG_TAG ledd_global
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_global);

#include <ut_string.h>

#include "global.h"
#include "utils.h"

static const int default_granularity = 10;
static char * const default_platform_config = "/etc/ledd/platform.conf";
static char * const default_patterns_conf = "/etc/ledd/patterns.conf";
static char * const default_plugins_dir = "/usr/lib/ledd-plugins/";
static char * const default_address = "unix:@ledd.socket";

/* in ms */
static uint32_t granularity;
static char *platform_config;
static char *patterns_config;
static char *startup_pattern;
static char *plugins_dir;
static char *address;

static int read_global(lua_State *l)
{
	char *env;

	ULOGD("%s", __func__);

	granularity = default_granularity;
	platform_config = default_platform_config;
	patterns_config = default_patterns_conf;
	plugins_dir = default_plugins_dir;
	address = default_address;

	lua_getglobal(l, "granularity");
	if (!lua_isnil(l, -1))
		granularity = luaL_checknumber(l, -1);
	lua_pop(l, 1);

	lua_getglobal(l, "platform_config");
	if (!lua_isnil(l, -1)) {
		platform_config = strdup(luaL_checkstring(l, -1));
		if (platform_config == NULL)
			config_error(l, errno, "strdup");
	}
	lua_pop(l, 1);

	lua_getglobal(l, "patterns_config");
	if (!lua_isnil(l, -1)) {
		patterns_config = strdup(luaL_checkstring(l, -1));
		if (patterns_config == NULL)
			config_error(l, errno, "strdup");
	}
	lua_pop(l, 1);

	lua_getglobal(l, "startup_pattern");
	if (!lua_isnil(l, -1)) {
		startup_pattern = strdup(luaL_checkstring(l, -1));
		if (startup_pattern == NULL)
			config_error(l, errno, "strdup");
	}
	lua_pop(l, 1);

	env = getenv(PLUGINS_DIR_ENV);
	if (env != NULL) {
		plugins_dir = env;
	} else {
		lua_getglobal(l, "plugins_dir");
		if (!lua_isnil(l, -1)) {
			plugins_dir = strdup(luaL_checkstring(l, -1));
			if (plugins_dir == NULL)
				config_error(l, errno, "strdup");
		}
		lua_pop(l, 1);
	}

	lua_getglobal(l, "address");
	if (!lua_isnil(l, -1)) {
		address = strdup(luaL_checkstring(l, -1));
		if (address == NULL)
			config_error(l, errno, "strdup");
	}
	lua_pop(l, 1);

	return 0;
}

int global_init(const char *path)
{
	return read_config(path, read_global, LUA_GLOBALS_CONFIG_GLOBAL);
}

void global_dump_config(void)
{
	ULOGI("granularity = %"PRIu32, granularity);
	ULOGI("platform config directory = %s", platform_config);
	ULOGI("patterns config directory = %s", patterns_config);
	ULOGI("startup pattern = %s", startup_pattern);
	ULOGI("plugins directory = %s", plugins_dir);
}

uint32_t global_get_granularity(void)
{
	return granularity;
}

const char *global_get_platform_config(void)
{
	return platform_config;
}

const char *global_get_patterns_config(void)
{
	return patterns_config;
}

const char *global_get_startup_pattern(void)
{
	return startup_pattern;
}

const char *global_get_plugins_dir(void)
{
	return plugins_dir;
}
const char *global_get_address()
{
	return address;
}

void global_cleanup(void)
{
	ULOGD("%s", __func__);

	if (address != default_address)
		ut_string_free(&address);
	if (plugins_dir != default_plugins_dir)
		ut_string_free(&plugins_dir);
	if (startup_pattern != NULL)
		ut_string_free(&startup_pattern);
	if (patterns_config != default_patterns_conf)
		ut_string_free(&patterns_config);
	if (platform_config != default_platform_config)
		ut_string_free(&platform_config);
}
