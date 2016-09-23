/**
 * @file main.c
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif /* _GNU_SOURCE */
#include <sys/select.h>

#include <libgen.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define ULOG_TAG ledd
#include <ulog.h>
ULOG_DECLARE_TAG(ledd);

#include <ledd.h>

static int usage(bool success, const char *prog)
{
	puts("Daemon handling leds over libpomp");
	printf("usage : %s [config_file]\n"
			"\tdefault config_file is "DEFAULT_GLOBAL_CONF_PATH"\n",
			prog);

	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

static bool is_help_argument(const char *arg)
{
	return strcmp(arg, "-h") == 0 || strcmp(arg, "-?") == 0 ||
				strcmp(arg, "--help") == 0;
}

int main(int argc, char *argv[])
{
	int ret;
	const char *global_config = DEFAULT_GLOBAL_CONF_PATH;
	const char *prog = basename(argv[0]);
	int fd;
	fd_set rfds;

	if (argc > 2)
		return usage(false, prog);
	if (argc == 2) {
		if (is_help_argument(argv[1]))
			return usage(true, prog);
		else
			global_config = argv[1];
	}

	ret = ledd_init(global_config);
	if (ret < 0) {
		ULOGE("ledd_init: %s", strerror(-ret));
		return EXIT_FAILURE;
	}
	atexit(ledd_cleanup);

	fd = ledd_get_fd();
	if (fd < 0) {
		ULOGE("ledd_get_fd: %s", strerror(-fd));
		return EXIT_FAILURE;
	}

	ULOGI("started");
	do {
		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);

		ret = select(fd + 1, &rfds, NULL, NULL, NULL);
		if (ret < 0 && errno != EINTR) {
			ULOGE("select: %m");
			return EXIT_FAILURE;
		}
		ret = ledd_process_events();
		if (ret < 0) {
			ULOGE("ledd_process_events: %s", strerror(-ret));
			return EXIT_FAILURE;
		}
	} while (ret != 1);

	ULOGI("bye bye");

	return EXIT_SUCCESS;
}
