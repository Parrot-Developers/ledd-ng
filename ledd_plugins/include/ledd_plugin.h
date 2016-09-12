/**
 * @file ledd_plugin.h
 * @brief Public API header for the implementation of ledd plugins
 *
 * @date 13 avr. 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef LEDD_PLUGIN_H_
#define LEDD_PLUGIN_H_
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>

#include <lua.h>

#include <rs_node.h>

/**
 * @def LEDD_PLUGINS_MAX
 * @brief maximum number of plug-ins that ledd can register
 */
#ifndef LEDD_PLUGINS_MAX
#define LEDD_PLUGINS_MAX 20
#endif

/***************************** led drivers API ********************************/

/**
 * @def LED_MAX_CHANNELS_PER_LED
 * @brief maximum number of channels a led can have
 */
#define LED_MAX_CHANNELS_PER_LED 8

/**
 * @def LED_CHANNEL_MAX
 * @brief maximum value a channel can be set to
 */
#define LED_CHANNEL_MAX UINT8_MAX

/**
 * @def LED_MAX_DRIVERS
 * @brief maximum number of led drivers which can be registered
 */
#define LED_MAX_DRIVERS 10

struct led;
struct led_driver;

/**
 * @struct led_channel
 * @brief channel of a led, e.g. red, green or blue channel of an RGB led
 */
struct led_channel {
	/** current value of the channel */
	uint8_t value;
	/** reference to the led this channel belongs to */
	struct led *led;
	/** name (unique) of the channel */
	char *id;
};

struct led {
	/** led driver responsible of this led's channels */
	struct led_driver *driver;
	/** name (unique) of the led */
	char *id;
	/** arrow of channels of this led */
	struct led_channel *channels[LED_MAX_CHANNELS_PER_LED];
	/** number of channels of the led */
	uint8_t nb_channels;
	/** node for storage in a container */
	struct rs_node node;
};

/**
 * @struct led_driver_ops
 * @brief operations for implementing a led driver
 */
struct led_driver_ops {
	/* mandatory callbacks */
	/**
	 * @fn channel_new
	 * @brief channel constructor, mandatory
	 * @param driver driver for this driver_ops structure
	 * @param led_id name of the led to create
	 * @param channel_id name of the channel to create
	 * @param parameters parameters for the channel's instantiation, can be
	 * NULL if not required
	 * @return channel allocated and configured on success, NULL on error
	 * with errno set
	 */
	struct led_channel *(*channel_new)(struct led_driver *driver,
			const char *led_id, const char *channel_id,
			const char *parameters);
	/**
	 * @fn channel_destroy
	 * @brief channel destructor, must release the ressources allocated to
	 * the channel, mandatory
	 * @param channel channel to destroy
	 */
	void (*channel_destroy)(struct led_channel *channel);
	/**
	 * @fn set_value
	 * @brief callback responsible of setting a led's channel to a given
	 * value
	 * @param channel channel to set the current value of
	 * @param value value to set to the channel, in [0,255]
	 * @return 0 on success, errno-compatible negative value on error
	 */
	int (*set_value)(struct led_channel *channel, uint8_t value);

	/* optional callbacks */
	/**
	 * @fn process_events
	 * @brief if the driver want to register himself in ledd's fd event
	 * loop, this callback must not NULL and will be triggered each time a
	 * read (or write, according to led_driver.rw) event is reported on
	 * led_driver.fd
	 * @param driver driver for this driver_ops structure
	 * @param events bit-mask of the events reported (R, W or both)
	 */
	void (*process_events)(struct led_driver *driver, int events);
	/**
	 * @fn tick
	 * @brief callback notified when all the set_value callbacks have been
	 * called for all the leds channels registered in ledd for any driver.
	 * Allows drivers to pack their commands to limit writes.
	 * @param driver led driver notified of the end of the current tick
	 */
	void (*tick)(struct led_driver *driver);
};

/**
 * @struct led_driver
 * @brief main structure of a led driver
 */
struct led_driver {
	/** name of the driver, used to instantiate it in platform.conf */
	const char *name;
	/** driver operations */
	struct led_driver_ops ops;

	/**
	 * file descriptor of registration in ledd's event loop, ignored if
	 * process_events is NULL.<br />
	 * registered for reads at least
	 */
	int fd;
	/** if true, fd will be registered for write events too */
	bool rw;
};

/**
 * @brief registers a led driver in ledd
 * @param driver driver to register
 * @return 0 on success, errno-compatible negative value on error
 */
int led_driver_register(struct led_driver *driver);

/**
 * @brief notifies all the drivers that all the set_value calls have been
 * performed during this tick
 */
void led_driver_tick_all_drivers(void);

/**
 * @brief un-registers a led driver from ledd
 * @param driver driver to un-register
 */
void led_driver_unregister(const struct led_driver *driver);

/******** API for drivers depending on another driver (e.g., tricolor) ********/

/**
 * @brief builds a new led
 * @param driver name of the driver driving the led's channel
 * @param led_id name (unique) of the led
 * @return 0 on success, errno-compatible negative value on error
 */
int led_new(const char *driver, const char *led_id);

/**
 * @brief build a new led channel
 * @param led_id led name (unique)
 * @param channel_id led channel name (unique)
 * @param parameters parameters string for configuring the channel, can be NULL
 * @return 0 on success, errno-compatible negative value on error
 */
int led_channel_new(const char *led_id, const char *channel_id,
		const char *parameters);

/**
 * @brief sets the value of a led channel
 * @param led_id name of the led
 * @param channel_id name of the led channel to set the value of
 * @param value value to set
 * @return 0 on success, errno-compatible negative value on error
 */
int led_driver_set_value(const char *led_id, const char *channel_id,
		uint8_t value);

/***************************** transitions API ********************************/

/**
 * @def TRANSITION_MAX
 * @brief maximum number of different transitions that can be registered into
 * ledd
 * @note 2 are already registered by ledd, cosine and ramp, hence this number is
 * reduced by 2
 */
#define TRANSITION_MAX 10

/**
 * @typedef transition_function
 * @brief type of the functions used to implement a transition, concretely, must
 * implement a mapping of [0,nb_value] onto [start_value,end_value]
 * @param nb_values maximum value of the initial interval
 * @param value_idx number in [0,nb_value]
 * @param start_value initial value of the target interval
 * @param end_value end value of the target interval
 * @return value in [start_value,end_value]
 */
typedef uint8_t (*transition_function)(uint32_t nb_values, uint32_t value_idx,
		uint8_t start_value, uint8_t end_value);

/**
 * @param name name of the transition to register
 * @param compute function used to compute the transition's value
 * @return 0 on success, errno-compatible negative value on error
 */
int transition_register(const char *name, transition_function compute);

/* some helpers for the implementation of transitions */
/**
 * @brief Clip an integer value into the [0,255] interval
 * @param value value to clip
 * @return value clipped into [0,255]
 */
uint8_t transition_clip_value(int value);

/**
 * @brief Clip a float value into the [0,1] interval
 * @param t float to clip
 * @return clipped value in [0,1]
 */
float transition_clip_float(float t);

/**
 * @brief clips t in [0,1] and maps it linearly to [start_value,end_value]
 * @param t value to clip and map
 * @param start_value start value of target interval
 * @param end_value end value of target interval
 * @return value mapped into [start_value,end_value]
 */
uint8_t transition_map_to(float t, uint8_t start_value, uint8_t end_value);

/************************* lua constants registration API *********************/
/*
 * plug-ins can register global values, numbers or functions, which will be
 * preloaded in the lua state for the parsing of global.conf, platform.conf or
 * pattern.conf
 */
/**
 * @enum lua_globals_config_type
 * @brief used to indicate to which configuration file a lua global must be
 * available
 */
enum lua_globals_config_type {
	/** lua global is available in global.conf config file */
	LUA_GLOBALS_CONFIG_GLOBAL,
	/** lua global is available in platform.conf config file */
	LUA_GLOBALS_CONFIG_PLATFORM,
	/** lua global is available in patterns.conf config file */
	LUA_GLOBALS_CONFIG_PATTERNS,
};

/**
 * @brief registers an integer lua global variable
 * @param name name of the variable to register
 * @param value value to put in it
 * @param config config file this global will be available into
 * @return 0 on success, errno-compatible negative value on error
 */
int lua_globals_register_int(const char *name, int value,
		enum lua_globals_config_type config);

/**
 * @brief registers an C function lua global variable
 * @param name name of the variable to register
 * @param func function to put in it
 * @param config config file this global will be available into
 * @return 0 on success, errno-compatible negative value on error
 */
int lua_globals_register_cfunction(const char *name, lua_CFunction func,
		enum lua_globals_config_type config);

#endif /* LEDD_PLUGIN_H_ */
