/**
 * @file ledd_client.h
 * @brief library for connecting to a ledd server and setting it's patterns
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

#ifndef LEDD_CLIENT_INCLUDE_LEDD_CLIENT_H_
#define LEDD_CLIENT_INCLUDE_LEDD_CLIENT_H_
#include <stdbool.h>

/**
 * @def LEDD_DEFAULT_ADDRESS
 * @brief default address ledd listens to when not configured otherwise
 */
#define LEDD_DEFAULT_ADDRESS "unix:@ledd.socket"

/**
 * @struct ledd_client
 * @brief ledd client context, opaque structure
 */
struct ledd_client;

/**
 * @typedef ledd_client_connection_cb
 * @brief type of the callback to pass to ledd_client_new to be notified when
 * the client gets connected to ledd
 * @param userdata userdata passed to ledd_client_new() along with the callback
 * @param connected true if the client got connected, false if it got
 * disconnected
 */
typedef void (*ledd_client_connection_cb)(void *userdata, bool connected);

/**
 * @struct ledd_client_ops
 * @brief structure holding the callbacks to pass to ledd_client_new()
 */
struct ledd_client_ops {
	/** callback called when the client gets connected */
	ledd_client_connection_cb connection_cb;
};

/**
 * Retrieves the libpomp address ledd is listening to, by reading ledd's
 * global.conf configuration file. Returns "unix:@ledd.socket" by default if for
 * any reason, no address could be found. If global_conf_path, tries to read the
 * address in /etc/ledd/global.conf by default.
 * @param global_conf_path path to the global.conf configuration file used b
 * ledd, can be NULL
 * @return address used by ledd, defaulting to "unix:@ledd.socket", must be
 * freed after usage, on error, returns NULL with errno set
 */
char *ledd_client_get_ledd_address(const char *global_conf_path);

/**
 * Allocates and initializes a ledd client context
 * @param address address of the ledd server to contact, can be NULL, in this
 * case, defaults to "unix:@ledd.socket"
 * @param ops client callbacks
 * @param userdata userdata pointer passed to the callbacks when called
 * @return NULL on error, with errno set, ledd client context newly allocated
 * and initialized on success
 */
struct ledd_client *ledd_client_new(const char *address,
		const struct ledd_client_ops *ops, void *userdata);

/**
 * Connects the ledd client to the server
 * @param client ledd client to connect
 * @return 0 on success, errno-compatible negative value on error
 */
int ledd_client_connect(struct ledd_client *client);

/**
 * Gets the underlying file descriptor of a ledd client context. The file
 * descriptor must be waited for read events in an event loop, and
 * ledd_client_process_events() must be called when it triggers a read event.
 * @param client ledd client context
 * @return positive file descriptor on success, negative errno-compatible value
 * on error
 */
int ledd_client_get_fd(struct ledd_client *client);

/**
 * Processes read events detected on a ledd client's file descriptor
 * @param client ledd client context
 * @return errno-compatible negative value on error, 0 on success
 */
int ledd_client_process_events(struct ledd_client *client);


/**
 * Sets a ledd pattern to be played by ledd. Multiple led patterns can be played
 * at the same time, provided they control different leds.
 * @param client ledd client context
 * @param pattern name of the pattern, as defined in the platform.conf ledd
 * configuration file
 * @param resume_previous if true and the patterns doesn't cycle an infinite of
 * time, pause the previous pattern and resume it when the new pattern ends
 * @return errno-compatible negative value on error, 0 on success
 */
int ledd_client_set_pattern(struct ledd_client *client, const char *pattern,
		bool resume_previous);

/**
 * Destroys a ledd client context.
 * @param client ledd client context to destroy, set to NULL on output
 */
void ledd_client_destroy(struct ledd_client **client);

#endif /* LEDD_CLIENT_INCLUDE_LEDD_CLIENT_H_ */
