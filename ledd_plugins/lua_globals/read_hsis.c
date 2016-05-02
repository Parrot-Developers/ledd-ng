/**
 * @file read_hsis.c
 * @brief
 *
 * @date 2 mai 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#include <stdlib.h>
#include <string.h>

#define ULOG_TAG ledd_read_hsis
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_read_hsis);

#include <lua.h>
#include <lauxlib.h>

#include <ut_string.h>
#include <ut_file.h>

#include <ledd_plugin.h>

static int read_hsis_int_l(lua_State *l)
{
	int ret;
	char __attribute__((cleanup(ut_string_free)))*value = NULL;
	const char *file;

	file = luaL_checkstring(l, -1);

	ret = ut_file_to_string("/sys/kernel/hsis/%s", &value, file);
	if (ret < 0)
		luaL_error(l, "ut_file_to_string: %s", strerror(-ret));

	ut_string_rstrip(value);
	lua_pushstring(l, value);

	return 1;
}

static __attribute__((constructor)) void read_hsis_init(void)
{
	int ret;

	ret = lua_globals_register_cfunction("read_hsis_int", read_hsis_int_l,
			LUA_GLOBALS_CONFIG_PLATFORM);
	if (ret < 0)
		ULOGW("lua_globals_register_cfunction: %s", strerror(-ret));
}
