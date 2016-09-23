/**
 * @file plugins.c
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
#include <dirent.h>

#include <dlfcn.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <fnmatch.h>

#define ULOG_TAG ledd_plugins
#include <ulog.h>
ULOG_DECLARE_TAG(ledd_plugins);

#include <ledd_plugin.h>

#include "utils.h"
#include "plugins.h"

#define PLUGINS_MATCHING_PATTERN "*.so"

static int pattern_filter(const struct dirent *d)
{
	return fnmatch(PLUGINS_MATCHING_PATTERN, d->d_name, 0) == 0;
}

static void *plugins[LEDD_PLUGINS_MAX];
static bool plugins_initialized;

int plugins_init(const char *plugins_dir)
{
	int ret;
	int n;
	int i;
	struct dirent **namelist;
	char *path;
	void **current_plugin;

	/* don't register plugins twice */
	if (plugins_initialized)
		return 0;
	plugins_initialized = true;

	ULOGI("scanning plugins dir %s", plugins_dir);

	n = scandir(plugins_dir, &namelist, pattern_filter, alphasort);
	if (n == -1) {
		ret = -errno;
		ULOGD("%s scandir(%s): %m", __func__, plugins_dir);
		return ret;
	}

	current_plugin = plugins;
	for (i = 0; i < n; i++) {
		if (i >= LEDD_PLUGINS_MAX) {
			ULOGW("More than %d plugins, plugin %s skipped",
					LEDD_PLUGINS_MAX, namelist[i]->d_name);
			free(namelist[i]);
			continue;
		}
		ret = asprintf(&path, "%s/%s", plugins_dir,
				namelist[i]->d_name);
		if (ret == -1) {
			ULOGE("%s asprintf error", __func__);
			return -ENOMEM;
		}
		ULOGD("loading plugin %s", path);
		*current_plugin = dlopen(path, RTLD_NOW);
		free(path);
		if (!*current_plugin)
			ULOGW("%s dlopen: %s", __func__, dlerror());
		else
			current_plugin++;
		free(namelist[i]);
	}
	free(namelist);

	return 0;
}

void plugins_cleanup(void)
{
	int i = LEDD_PLUGINS_MAX;

	if (!plugins_initialized)
		return;
	plugins_initialized = false;

	while (i--)
		pdlclose(plugins + i);
}

