/**
 * @file led_driver_priv.h
 * @brief
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef LED_DRIVER_PRIV_H_
#define LED_DRIVER_PRIV_H_
#include <inttypes.h>

#include <libpomp.h>

struct led;
struct led_channel;

void led_driver_init(void);

void led_driver_cleanup(void);

void led_channel_destroy(const char *led_id, const char *channel_id);

int led_driver_register_drivers_in_pomp_loop(struct pomp_loop *loop);

void led_driver_unregister_drivers_from_pomp_loop(struct pomp_loop *loop);

/* put all channels to black, returns the last set_value error */
int led_driver_paint_it_black(void);

/* put all channels of a led to a default value */
int led_driver_apply_default_value(const char *led_id, uint8_t value);

void led_drivers_dump_config(void);

#endif /* LED_DRIVER_PRIV_H_ */
