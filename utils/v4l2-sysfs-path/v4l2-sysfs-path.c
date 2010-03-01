/*
    Copyright (C) 2009 Mauro Carvalho Chehab <mchehab@redhat.com>
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation version 2 of the License.

    The sysfs logic were adapted from a C++/Boost snippet code sent by

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "../libv4l2util/v4l2_driver.h"
#include <sysfs/libsysfs.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define USB_ID "usb-"
#define PCI_ID "PCI:"
#define PCIe_ID "PCIe:"

static char *obtain_bus_sysfs_path(char *bus_info)
{
	struct sysfs_device *pcictl = NULL;
	struct sysfs_bus *bus = NULL;
	struct sysfs_device *busdev = NULL;
	struct dlist *busdevs = NULL;
	struct sysfs_device *pdev = NULL;
	char *tmp = NULL, *busname, *buspath;
	int pci;

	if (!strncmp(bus_info, USB_ID, strlen(USB_ID))) {
		bus_info += strlen(USB_ID);
		pci = 0;
	} else 	if (!strncmp(bus_info, PCI_ID, strlen(PCI_ID))) {
		bus_info += strlen(PCI_ID);
		pci = 1;
	} else 	if (!strncmp(bus_info, PCIe_ID, strlen(PCIe_ID))) {
		bus_info += strlen(PCIe_ID);
		pci = 1;
	} else
		return NULL;

	busname = strtok(bus_info, "-");
	if (!busname)
		return NULL;

	buspath = strtok(NULL, "-");
	if (!buspath && !pci)
		return NULL;

	/* open bus host controller */
	pcictl = sysfs_open_device("pci", busname);
	if (!pcictl)
		goto err;

	/* We have all we need for PCI devices */
	if (pci) {
		char *name;

		if (asprintf(&name, "%s", pcictl->path) == -1)
			goto err;
		return name;
	}

	/* find matching usb bus */
	bus = sysfs_open_bus("usb");
	if (!bus)
		goto err;

	busdevs = sysfs_get_bus_devices(bus);
	if (!busdevs)
		goto err;

	dlist_for_each_data(busdevs, busdev, struct sysfs_device) {
		/* compare pathes of bus host controller and
		   parent of enumerated bus devices */

		pdev = sysfs_get_device_parent(busdev);
		if (!pdev)
			continue;

		if (!strcmp(pcictl->path, pdev->path))
			break;
	}

	if (!pdev)
		goto err;

	sysfs_close_device(pcictl);
	pcictl = NULL;

	/* assemble bus device path */
	if (busdev) {
		struct sysfs_attribute *busnumattr;
		unsigned int busnum;
		char *name;

		busnumattr = sysfs_get_device_attr(busdev, "busnum");
		if (!busnumattr)
			goto err;

		tmp = malloc(busnumattr->len + 1);
		strncpy(tmp, busnumattr->value, busnumattr->len);
		tmp[busnumattr->len] = '\0';

		if (sscanf(tmp, "%u", &busnum) != 1)
			goto err;

		if (asprintf(&name, "%s/%d-%s", busdev->path,
			     busnum, buspath) == -1)
			goto err;

		free(tmp);
		sysfs_close_bus(bus);

		return name;
	}

err:
	if (tmp)
		free(tmp);
	if (bus)
		sysfs_close_bus(bus);
	if (pcictl)
		sysfs_close_device(pcictl);

	return NULL;
}

static char *seek_name(char *path, char *match)
{
	DIR             *dir;
	struct dirent   *entry;
	struct stat	st;
	char		*p;
	static char     name[1024];
	int		major, minor;

	dir = opendir(path);
	if (!dir)
		return NULL;

	strcpy(name, path);
	strcat(name, "/");
	p = name + strlen(name);

	entry = readdir(dir);
	while (entry) {
		if (!strncmp(entry->d_name, match, strlen(match))) {

			strcpy(name, entry->d_name);
			closedir(dir);
			return name;
		}
		entry = readdir(dir);
	}
	closedir(dir);
	return NULL;
}

static int get_dev(char *class, int *major, int *minor, char *extra)
{
	char            path[1024];
	char		*name;
	FILE		*fp;

	name = strchr(class,':');
	if (!name)
		return -1;
	*name = 0;
	name++;

	*extra = 0;

	if (!strcmp(class, "input")) {
		char *event;

		sprintf(path, "/sys/class/%s/%s/", class, name);
		event = seek_name(path, "event");
		if (!event)
			return -1;

		strcpy(extra, event);

		sprintf(path, "/sys/class/%s/%s/%s/dev", class, name, event);

	} else
		sprintf(path, "/sys/class/%s/%s/dev", class, name);

	fp = fopen(path, "r");
	if (!fp)
		return -1;

	if (fscanf(fp, "%d:%d", major, minor) != 2)
		return -1;

	return 0;
}

/*
 Examples of subdevs:
	sound:audio1
	sound:controlC1
	sound:dsp1
	sound:mixer1
	sound:pcmC1D0c
	dvb:dvb0.demux0
	dvb:dvb0.dvr0
	dvb:dvb0.frontend0
	dvb:dvb0.net0
	i2c-adapter:i2c-4
	input:input8
*/

static void get_subdevs(char *path)
{
	DIR             *dir;
	struct dirent   *entry;
	struct stat	st;
	char            *p, name[1024], extra[20];
	int		major, minor;

	dir = opendir(path);
	if (!dir)
		return;

	strcpy(name, path);
	strcat(name, "/");
	p = name + strlen(name);

	printf("Associated devices:\n");
	entry = readdir(dir);
	while (entry) {
		strcpy(p, entry->d_name);
		if ((lstat(name, &st) == 0) &&
			!S_ISDIR(st.st_mode)) {
			char *s = strchr(entry->d_name, ':');
			if (s) {
				printf("\t%s", entry->d_name);
				if (!get_dev(entry->d_name, &major, &minor, extra)) {
					if (*extra)
						printf(":%s (dev %d,%d)",
							extra, major, minor);
					else
						printf(" (dev %d,%d)",
							major, minor);
				}
				printf("\n");
			}
		}
		entry = readdir(dir);
	}
	closedir(dir);
}

static void get_sysfs(char *fname)
{
	struct v4l2_driver drv;
	char *path;
	if (v4l2_open(fname, 0, &drv) < 0) {
		perror(fname);
		return;
	}

	printf("device     = %s\n", fname);
	printf("bus info   = %s\n", drv.cap.bus_info);
	path = obtain_bus_sysfs_path((char *)drv.cap.bus_info);
	if (path) {
		printf("sysfs path = %s\n", path);
		get_subdevs(path);
		free(path);
	}
	printf("\n");

	v4l2_close(&drv);
}

static void read_dir(char *dirname)
{
	DIR             *dir;
	struct dirent   *entry;
	const char      *vid = "video";
	const char      *rad = "radio";
	char		*p, name[512];

	dir = opendir(dirname);
	if (!dir)
		return;

	strcpy(name, dirname);
	strcat(name, "/");
	p = name + strlen(name);

	entry = readdir(dir);
	while (entry) {
		if (!strncmp(entry->d_name, vid, strlen(vid)) ||
		    !strncmp(entry->d_name, rad, strlen(rad))) {
			strcpy(p, entry->d_name);

			get_sysfs(name);
		}
		entry = readdir(dir);
	}
	closedir(dir);
}

int main(void)
{
	read_dir("/dev");
	return 0;
}
