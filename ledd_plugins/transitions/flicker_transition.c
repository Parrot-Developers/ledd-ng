/**
 * @file flicker_transition.c
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

#include <string.h>

#define ULOG_TAG flicker_transition
#include <ulog.h>
ULOG_DECLARE_TAG(flicker_transition);

#include <stdlib.h>

#include <ledd_plugin.h>

static uint8_t flicker_compute(uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value)
{
	float amplitude = abs(end_value - start_value);
	float alea = (amplitude * rand()) / RAND_MAX - amplitude / 2.f;
	int value;
	float t;

	t = (1.f * value_idx) / nb_values;
	value = transition_map_to(t, start_value, end_value);
	value += alea;

	return transition_clip_value(value);
}

static __attribute__((constructor)) void flicker_transition_init(void)
{
	int ret;

	ret = transition_register("flicker", flicker_compute);
	if (ret < 0)
		ULOGE("transition_register(flicker): %s", strerror(-ret));
}
