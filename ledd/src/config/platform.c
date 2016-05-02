/*
 * platform.c
 *
 *  Created on: Apr 18, 2016
 *      Author: ncarrier
 */
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <errno.h>

#include <lua.h>
#include <lauxlib.h>

#define ULOG_TAG ledd_platform
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_platform);

#include <ut_string.h>

#include <ledd_plugin.h>

#include "led_driver_priv.h"

#include "platform.h"
#include "utils.h"

#define print_top do {int top = lua_gettop(l); ULOGC("stack_top = %d", top);\
	} while (0)

static int read_channel(lua_State *l, const char *led_id,
		const char *channel_id)
{
	int ret;
	char __attribute__((cleanup(ut_string_free)))*parameters = NULL;

	lua_pushstring(l, "parameters");
	lua_gettable(l, -2);
	if (!lua_isnil(l, -1)) {
		if (!lua_isstring(l, -1))
			luaL_error(l, "string expected for parameters, got %s",
					lua_typename(l, lua_type(l, -1)));

		parameters = strdup(lua_tostring(l, -1));
		if (parameters == NULL)
			config_error(l, errno, "strdup");
	}
	lua_pop(l, 1);

	ret = led_channel_new(led_id, channel_id, parameters);
	if (ret != 0)
		config_error(l, -ret, "led_channel_new");

	return 0;
}

static int read_led(lua_State *l, const char *led_id)
{
	int ret;
	const char *key;
	char __attribute__((cleanup(ut_string_free)))*driver = NULL;

	lua_pushstring(l, "driver");
	lua_gettable(l, -2);
	if (!lua_isstring(l, -1))
		luaL_error(l, "string expected for driver name, got %s",
				lua_typename(l, lua_type(l, -1)));
	driver = strdup(lua_tostring(l, -1));
	if (driver == NULL)
		config_error(l, errno, "strdup");
	lua_pop(l, 1);

	ret = led_new(driver, led_id);
	if (ret < 0)
		config_error(l, -ret, "led_new");

	/* iterate over the "channels" table to fetch all the channels */
	lua_pushstring(l, "channels");
	lua_gettable(l, -2);
	lua_pushnil(l);
	while (lua_next(l, -2) != 0) {
		if (!lua_isstring(l, -2))
			luaL_error(l, "string expected for chan. name, got %s",
					lua_typename(l, lua_type(l, -2)));
		if (!lua_istable(l, -1))
			luaL_error(l, "table expected for chan. desc., got %s",
					lua_typename(l, lua_type(l, -1)));

		key = lua_tostring(l, -2);
		ret = read_channel(l, led_id, key);
		if (ret != 0)
			config_error(l, -ret, "read_channel");
		lua_pop(l, 1);
	}
	lua_pop(l, 1); /* pop table channels */

	return 0;
}

static int read_platform(lua_State *l)
{
	int ret;
	const char *key;

	ULOGD("%s", __func__);

	lua_getglobal(l, "leds");
	if (!lua_istable(l, -1))
		luaL_error(l, "'leds' is not a table");

	/* iterate over the "leds" table to fetch all the leds */
	lua_pushnil(l); /* first key */
	while (lua_next(l, -2) != 0) {
		if (!lua_isstring(l, -2))
			luaL_error(l, "string expected for led name, got %s",
					lua_typename(l, lua_type(l, -2)));
		if (!lua_istable(l, -1))
			luaL_error(l, "table expected for led desc., got %s",
					lua_typename(l, lua_type(l, -1)));

		key = lua_tostring(l, -2);
		ret = read_led(l, key);
		if (ret != 0)
			config_error(l, -ret, "read_led");
		lua_pop(l, 1);
	}
	/* pop the last key */
	lua_pop(l, 1);

	/* pop the "leds" table */
	lua_pop(l, 1);

	return 0;
}

int platform_init(const char *path)
{
	int ret;

	ULOGD("%s(%s)", __func__, path);

	led_driver_init();
	ret = read_config(path, read_platform, LUA_GLOBALS_CONFIG_PLATFORM);
	if (ret != 0)
		platform_cleanup();

	return ret;
}

void platform_cleanup(void)
{
	ULOGD("%s", __func__);

	led_driver_cleanup();
}
