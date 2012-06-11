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

#include <config.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <glob.h>
#include <sys/stat.h>
#include <sys/mman.h>
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

#define PLUGINS_PATTERN LIBV4L2_PLUGIN_DIR "/*.so"

void v4l2_plugin_init(int fd, void **plugin_lib_ret, void **plugin_priv_ret,
		      const struct libv4l_dev_ops **dev_ops_ret)
{
	char *error;
	int glob_ret, i;
	void *plugin_library = NULL;
	const struct libv4l_dev_ops *libv4l2_plugin = NULL;
	glob_t globbuf;

	*dev_ops_ret = v4lconvert_get_default_dev_ops();
	*plugin_lib_ret = NULL;
	*plugin_priv_ret = NULL;

	glob_ret = glob(PLUGINS_PATTERN, 0, NULL, &globbuf);

	if (glob_ret == GLOB_NOSPACE)
		return;

	if (glob_ret == GLOB_ABORTED || glob_ret == GLOB_NOMATCH)
		goto leave;

	for (i = 0; i < globbuf.gl_pathc; i++) {
		V4L2_LOG("PLUGIN: dlopen(%s);\n", globbuf.gl_pathv[i]);

		plugin_library = dlopen(globbuf.gl_pathv[i], RTLD_LAZY);
		if (!plugin_library)
			continue;

		dlerror(); /* Clear any existing error */
		libv4l2_plugin = (struct libv4l_dev_ops *)
			dlsym(plugin_library, "libv4l2_plugin");
		error = dlerror();
		if (error != NULL)  {
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

		*plugin_lib_ret = plugin_library;
		*dev_ops_ret = libv4l2_plugin;
		break;
	}

leave:
	globfree(&globbuf);
}

void v4l2_plugin_cleanup(void *plugin_lib, void *plugin_priv,
			 const struct libv4l_dev_ops *dev_ops)
{
	if (plugin_lib) {
		dev_ops->close(plugin_priv);
		dlclose(plugin_lib);
	}
}
