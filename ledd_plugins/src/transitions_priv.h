/*
 * transitions.h
 *
 *  Created on: Apr 19, 2016
 *      Author: ncarrier
 */
#ifndef LEDD_PLUGINS_SRC_TRANSITIONS_PRIV_H_
#define LEDD_PLUGINS_SRC_TRANSITIONS_PRIV_H_
#include <stdint.h>

struct transition;

const struct transition *transition_get(uint16_t id);

const char *transition_get_name(const struct transition *t);

uint16_t transition_get_id(const struct transition *t);

const struct transition *transition_next_from(const struct transition *t);

uint8_t transition_compute(const struct transition *transition,
		uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value);

#endif /* LEDD_PLUGINS_SRC_TRANSITIONS_PRIV_H_ */
