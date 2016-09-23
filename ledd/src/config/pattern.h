/**
 * @file pattern.h
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
 *   * Neither the name of the <organization> nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
