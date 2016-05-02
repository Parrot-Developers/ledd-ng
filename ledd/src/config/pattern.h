/**
 * @file pattern.h
 * @brief
 *
 * @date 18 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef SRC_PATTERN_H_
#define SRC_PATTERN_H_
#include <stdbool.h>

#include <rs_node.h>

#include <ledd_plugin.h>

struct pattern;

int patterns_init(const char *path);

const struct pattern *pattern_get(const char *name);

uint32_t pattern_get_total_duration(const struct pattern *pattern);

uint32_t pattern_get_repetitions(const struct pattern *pattern);

/* cursor is the value index */
int pattern_apply_values(const struct pattern *pattern, uint32_t cursor,
		bool apply_default);

int pattern_switch_off(const struct pattern *pattern);

const char *pattern_get_name(const struct pattern *pattern);

uint32_t pattern_get_intro(const struct pattern *pattern);

uint32_t pattern_get_outro(const struct pattern *pattern);

/*
 * a pair of patterns intersect iif among the leds they control, at least one
 * is in common, even if only different channels are controlled
 */
bool patterns_intersect(const struct pattern *pat1, const struct pattern *pat2);

bool patterns_have_same_support(const struct pattern *pat1,
		const struct pattern *pat2);

void patterns_dump_config(void);

void patterns_cleanup(void);

#endif /* SRC_PATTERN_H_ */
