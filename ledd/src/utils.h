/**
 * @file utils.h
 * @brief
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
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
