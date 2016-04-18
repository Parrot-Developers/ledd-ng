/**
 * @file lua_globals.c
 * @brief
 *
 * @date 2 mai 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include <lauxlib.h>

#define ULOG_TAG ledd_lua_globals
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_lua_globals);

#include <ut_string.h>

#include "lua_globals_priv.h"

#define LUA_GLOBALS_MAX 200

enum lua_global_type {
	LUA_GLOBAL_VALUE_INVALID = 0,
	LUA_GLOBAL_VALUE_INT,
	LUA_GLOBAL_VALUE_FUNC,
};
struct lua_global {
	enum lua_global_type type;
	enum lua_globals_config_type config;
	char *name;
	union {
		int int_value;
		lua_CFunction func_value;
	};
};

static struct lua_global lua_globals[LUA_GLOBALS_MAX];

static bool is_valid(enum lua_global_type type)
{
	return type != LUA_GLOBAL_VALUE_INVALID;
}
static int first_free_slot(void)
{
	int i;

	for (i = 0; i < LUA_GLOBALS_MAX && is_valid(lua_globals[i].type); i++)
		;

	return i;
}

static int register_global(const char *name, struct lua_global *global)
{
	int free_slot;
	struct lua_global *g;

	ULOGD("%s(%s)", __func__, name);

	free_slot = first_free_slot();
	if (free_slot == LUA_GLOBALS_MAX)
		return -ENOMEM;

	g = lua_globals + free_slot;
	*g = *global;

	/* we want to own a copy of the name */
	g->name = strdup(name);
	if (g->name == NULL)
		return -errno;

	return 0;
}

int lua_globals_register_int(const char *name, int value,
		enum lua_globals_config_type config)
{
	struct lua_global global = {
			.type = LUA_GLOBAL_VALUE_INT,
			.int_value = value,
			.config = config,
	};

	return register_global(name, &global);
}

int lua_globals_register_cfunction(const char *name, lua_CFunction func,
		enum lua_globals_config_type config)
{
	struct lua_global global = {
			.type = LUA_GLOBAL_VALUE_FUNC,
			.func_value = func,
			.config = config,
	};

	return register_global(name, &global);
}

int lua_globals_register_all(lua_State *l)
{
	int i;
	struct lua_global *global;
	enum lua_globals_config_type config;

	ULOGD("%s", __func__);

	lua_getglobal(l, "config");
	if (lua_isnil(l, -1))
		luaL_error(l, "missing config type");
	config = luaL_checknumber(l, -1);
	lua_pop(l, 1);

	for (i = 0; i < LUA_GLOBALS_MAX && is_valid(lua_globals[i].type); i++) {
		global = lua_globals + i;
		if (global->config != config)
			continue;

		ULOGD("registering lua global %s", global->name);
		switch (global->type) {
		case LUA_GLOBAL_VALUE_INT:
			lua_pushinteger(l, global->int_value);
			break;

		case LUA_GLOBAL_VALUE_FUNC:
			lua_pushcfunction(l, global->func_value);
			break;

		default:
			lua_error(l);
		}
		lua_setglobal(l, global->name);
	}

	return 0;
}

static __attribute__((destructor)) void lua_globals_cleanup(void)
{
	int i;

	ULOGD("%s", __func__);

	for (i = 0; i < LUA_GLOBALS_MAX && is_valid(lua_globals[i].type); i++)
		ut_string_free(&lua_globals[i].name);
}
