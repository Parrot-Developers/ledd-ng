/**
 * @file ledd.h
 * @brief Library for implementing a ledd daemon
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
 *   * Neither the name of the Parrot Company nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE PARROT COMPANY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
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
