/**
 * @file utils.h
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

#ifndef UTILS_H_
#define UTILS_H_
#include <stdbool.h>

#include <lua.h>
#include "lua_globals_priv.h"

void stackdump(lua_State *l);

/* closes a lib handle obtained with dlopen */
void pdlclose(void **dl);

/*
 * maps a value linearly from [range1_min, range1_max] to
 * [range2_min, range2_max]
 */
float linear_map(float value, float range1_min, float range1_max,
		float range2_min, float range2_max);


int read_config(const char *path, lua_CFunction config_reader,
		enum lua_globals_config_type config);

/*
 * calls lua_error()
 * returns abs(errnum) in the lua stack, if errnum > 0, prints
 *    "[fmt]: [strerror(errnum)]"
 * otherwise, prints the message described by the printf format fmt
 */
void config_error(lua_State *l, int errnum, const char *fmt, ...);

#endif /* UTILS_H_ */
