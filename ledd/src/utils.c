/**
 * @file utils.c
 * @brief
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#include <unistd.h>
#include <dlfcn.h>

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lualib.h>
#include <lauxlib.h>

#define ULOG_TAG ledd_utils
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_utils);

#include "utils.h"

/* approximate mapping from lua error codes to errno values */
static int lua_error_to_errno(int error)
{
	switch (error) {
	case LUA_OK:
		return 0;

	case LUA_ERRRUN:
		return -ENOEXEC;

	case LUA_ERRMEM:
		return -ENOMEM;

	case LUA_ERRFILE:
		return -EIO;

	case LUA_ERRGCMM:
	case LUA_ERRERR:
	case LUA_YIELD:
	case LUA_ERRSYNTAX:
	default:
		return -EINVAL;
	}
}

void stackdump(lua_State *l)
{
	int i;
	int top = lua_gettop(l);

	printf("total in stack %d\n", top);

	for (i = 1; i <= top; i++) { /* repeat for each level */
		int t = lua_type(l, i);
		switch (t) {
		case LUA_TSTRING: /* strings */
			printf("string: '%s'\n", lua_tostring(l, i));
			break;
		case LUA_TBOOLEAN: /* booleans */
			printf("boolean %s\n",
					lua_toboolean(l, i) ? "true" : "false");
			break;
		case LUA_TNUMBER: /* numbers */
			printf("number: %g\n", lua_tonumber(l, i));
			break;
		default: /* other values */
			printf("%s\n", lua_typename(l, t));
			break;
		}
		printf("  "); /* put a separator */
	}
	printf("\n"); /* end the listing */
}

void pdlclose(void **dl)
{
	if (dl == NULL || *dl == NULL)
		return;

	dlclose(*dl);
	*dl = NULL;
}

float linear_map(float value, float range1_min, float range1_max,
		float range2_min, float range2_max)
{
	float result;

	/* go back to [0, 1] */
	result = (value - range1_min) / (range1_max - range1_min);

	return result * (range2_max - range2_min) + range2_min;
}

static void plua_close(lua_State **l)
{
	if (l == NULL || *l == NULL)
		return;

	lua_close(*l);
	*l = NULL;
}

int read_config(const char *path, lua_CFunction config_reader,
		enum lua_globals_config_type config)
{
	int ret;
	lua_State __attribute__((cleanup(plua_close)))*l = NULL;
	bool is_number;

	ULOGD("%s", __func__);

	l = luaL_newstate();
	if (l == NULL) {
		ULOGE("luaL_newstate() failed");
		return -ENOMEM;
	}

	/* allow access to lua's standard library */
	luaL_openlibs(l);

	/* push all the registered lua globals */
	lua_pushinteger(l, config);
	lua_setglobal(l, "config");
	lua_pushcfunction(l, lua_globals_register_all);
	ret = lua_pcall(l, 0, 0, 0);
	if (ret != LUA_OK) {
		is_number = lua_isnumber(l, -1);
		ret = -(is_number ? lua_tonumber(l, -1) : EINVAL);
		ULOGE("read_config pl: %s", is_number ? strerror(-ret) :
						lua_tostring(l, -1));
		return ret;
	}

	/* load the config file */
	if (path != NULL) {
		ULOGI("loading configuration file \"%s\"", path);
		ret = luaL_dofile(l, path);
		if (ret != LUA_OK) {
			ULOGE("reading config file: %s", lua_tostring(l, -1));
			return lua_error_to_errno(ret);
		}
	}

	/* parse the config file */
	lua_pushcfunction(l, config_reader);
	ret = lua_pcall(l, 0, 0, 0);
	if (ret != LUA_OK) {
		is_number = lua_isnumber(l, -1);
		ret = -(is_number ? lua_tonumber(l, -1) : EINVAL);
		ULOGE("read_config: %s", is_number ? strerror(-ret) :
						lua_tostring(l, -1));
		return ret;
	}

	return 0;
}

void config_error(lua_State *l, int errnum, const char *fmt, ...)
{
	va_list args;
	int abs_err;

	if (errnum > 0) {
		abs_err = errnum;
		ULOGE("%s: %s", fmt, strerror(errnum));
	} else {
		abs_err = -errnum;
		va_start(args, fmt);
		ULOG_PRI_VA(ULOG_ERR, fmt, args);
		va_end(args);
	}
	lua_pushnumber(l, abs_err);

	lua_error(l);
}
