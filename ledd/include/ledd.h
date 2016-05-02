/**
 * @file ledd.h
 * @brief Library for implementing a ledd daemon
 *
 * @date 2 mai 2016
 * @author ncarrier
 * @copyright Copyright (C) 2016 Parrot S.A.
 */

#ifndef LEDD_INCLUDE_LEDD_H_
#define LEDD_INCLUDE_LEDD_H_

#ifndef DEFAULT_GLOBAL_CONF_PATH
/**
 * @def DEFAULT_GLOBAL_CONF_PATH
 * @brief default value for ledd's global configuration file, can be overridden
 * at compilation time
 */
#define DEFAULT_GLOBAL_CONF_PATH "/etc/ledd/global.conf"
#endif /* DEFAULT_GLOBAL_CONF_PATH */

/**
 * @def ledd_init
 * @brief thin wrapper around ledd_init_impl, specifying skip_plugins as the
 * value defined by the LEDD_SKIP_PLUGINS C macro. ledd_cleanup() must be called
 * to free the resources after usage
 * @see ledd_init_impl()
 */
#define ledd_init(config) ledd_init_impl((config), LEDD_SKIP_PLUGINS)

/**
 * Initializes the ledd server.
 * @param global_config Path to the global configuration file, usually
 * /etc/ledd/global.conf
 * @param skip_plugins if false, the plugins dir (defined in global.conf,
 * usually set to "/usr/lib/ledd-plugins/"), will be scanned and the plug-ins
 * found will be loaded, if false, no plug-in wil be loaded. This is useful if
 * the plug-ins you want to use are already linked into your program, like when
 * using the libledd-static version of libledd
 * @return errno-compatible negative value on error, 0 on success
 * @note in most case, one should rather use directly ledd_init() directly
 */
int ledd_init_impl(const char *global_config, bool skip_plugins);

/**
 * @brief Retrieves the underlying file descriptor of the ledd plugin, which
 * will report read events when the ledd_process_events() function must be
 * called
 * @return errno-compatible negative value on error, 0 on success
 */
int ledd_get_fd(void);

/**
 * Processes the events reported on the file descriptor returnedby ledd_get_fd()
 * @return 1 if quit has been requested for the ledd server, 0 if it must
 * continue or an errno-compatible negative value on error
 */
int ledd_process_events(void);

/**
 * Cleans up the internal state of the ledd server
 */
void ledd_cleanup(void);

#endif /* LEDD_INCLUDE_LEDD_H_ */
