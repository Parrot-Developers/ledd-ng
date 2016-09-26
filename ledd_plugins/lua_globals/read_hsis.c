/**
 * @file read_hsis.c
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
