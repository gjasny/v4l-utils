/*
* Copyright (C) 2010 Nokia Corporation <multimedia@maemo.org>

* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 2.1 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef ANDROID
#include <android-config.h>
#else
#include <config.h>
#endif
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "libv4l2.h"
#include "libv4l2-priv.h"
#include "libv4l-plugin.h"

/* libv4l plugin support:
   it is provided by functions v4l2_plugin_[open,close,etc].

   When open() is called libv4l dlopens files in /usr/lib[64]/libv4l/plugins
   1 at a time and call open callback passing through the applications
   parameters unmodified.

   If a plugin is relevant for the specified device node, it can indicate so
   by returning a value other then -1 (the actual file descriptor).
   As soon as a plugin returns another value then -1 plugin loading stops and
   information about it (fd and corresponding library handle) is stored. For
   each function v4l2_[ioctl,read,close,etc] is called corresponding
   v4l2_plugin_* function which looks if there is loaded plugin for that file
   and call it's callbacks.

   v4l2_plugin_* function indicates by it's first argument if plugin was used,
   and if it was not then v4l2_* functions proceed with their usual behavior.
*/

/* list of plugin search paths */
static const char *g_plugin_search_paths[] = {
	"/system/lib/libv4l/plugins",
	"/vendor/lib/libv4l/plugins",
	NULL /* list terminator */
};

void v4l2_plugin_init(int fd, void **plugin_lib_ret, void **plugin_priv_ret,
			  const struct libv4l_dev_ops **dev_ops_ret)
{
	char *error;
	void *plugin_library = NULL;
	const struct libv4l_dev_ops *libv4l2_plugin = NULL;
	DIR *plugin_dir = NULL;
	struct dirent *entry;
	char *suffix = NULL;
	int length, i;
	char filename[256];

	/* initialize output params */
	*dev_ops_ret = v4lconvert_get_default_dev_ops();
	*plugin_lib_ret = NULL;
	*plugin_priv_ret = NULL;

	/* read the plugin directory for "*.so" files */
	for (i = 0; g_plugin_search_paths[i] != NULL; i++) {
		plugin_dir = opendir(g_plugin_search_paths[i]);
		if (plugin_dir == NULL) {
			V4L2_LOG_ERR("PLUGIN: opening plugin directory (%s) failed\n",
				g_plugin_search_paths[i]);
			continue;
		}

		while (entry = readdir(plugin_dir)) {
			/* get last 3 letter suffix from the filename */
			length = strlen(entry->d_name);
			if (length > 3)
				suffix = entry->d_name + (length - 3);

			if (!suffix || strcmp(suffix, ".so")) {
				suffix = NULL; /* reset for next iteration */
				continue;
			}

			/* load library and get desired symbol */
			sprintf(filename, "%s/%s", g_plugin_search_paths[i], entry->d_name);
			V4L2_LOG("PLUGIN: dlopen(%s);\n", filename);
			plugin_library = dlopen(filename, RTLD_LAZY);
			if (!plugin_library)
				continue;

			dlerror(); /* Clear any existing error */
			libv4l2_plugin = (struct libv4l_dev_ops *)
				dlsym(plugin_library, "libv4l2_plugin");
			error = dlerror();
			if (error != NULL) {
				V4L2_LOG_ERR("PLUGIN: dlsym failed: %s\n", error);
				dlclose(plugin_library);
				continue;
			}

			if (!libv4l2_plugin->init ||
				!libv4l2_plugin->close ||
				!libv4l2_plugin->ioctl) {
				V4L2_LOG("PLUGIN: does not have all mandatory ops\n");
				dlclose(plugin_library);
				continue;
			}

			*plugin_priv_ret = libv4l2_plugin->init(fd);
			if (!*plugin_priv_ret) {
				V4L2_LOG("PLUGIN: plugin open() returned NULL\n");
				dlclose(plugin_library);
				continue;
			}

			/* exit loop when a suitable plugin is found */
			*plugin_lib_ret = plugin_library;
			*dev_ops_ret = libv4l2_plugin;
			break;
		}
		closedir(plugin_dir);

		/* exit loop when a suitable plugin is found */
		if (*plugin_lib_ret && *plugin_priv_ret && *dev_ops_ret)
			break;
	}
}

void v4l2_plugin_cleanup(void *plugin_lib, void *plugin_priv,
			 const struct libv4l_dev_ops *dev_ops)
{
	if (plugin_lib) {
		dev_ops->close(plugin_priv);
		dlclose(plugin_lib);
	}
}
