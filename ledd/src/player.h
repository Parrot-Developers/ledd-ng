/**
 * @file player.h
 * @brief
 *
 * @date 19 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef LEDD_SRC_PLAYER_H_
#define LEDD_SRC_PLAYER_H_
#include <stdbool.h>

int player_init(void);

/*
 * resume: if true, if a pattern A was interrupted by pattern B, pattern A will
 * resume after B has finished, otherwise, no pattern will be played.
 * only one previous pattern is stored, though
 */
int player_set_pattern(const char *pattern, bool resume);

bool player_is_playing(void);

int player_update(void);

void player_cleanup(void);

#endif /* LEDD_SRC_PLAYER_H_ */
