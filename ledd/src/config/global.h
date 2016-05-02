/**
 * @file global.h
 * @brief
 *
 * @date 19 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef SRC_GLOBAL_H_
#define SRC_GLOBAL_H_

#ifndef PLUGINS_DIR_ENV
#define PLUGINS_DIR_ENV "LEDD_PLUGINS_DIR"
#endif

int global_init(const char *path);

void global_dump_config(void);

uint32_t global_get_granularity(void);

const char *global_get_platform_config(void);

const char *global_get_patterns_config(void);

const char *global_get_startup_pattern(void);

const char *global_get_plugins_dir(void);

const char *global_get_address(void);

void global_cleanup(void);

#endif /* SRC_GLOBAL_H_ */
