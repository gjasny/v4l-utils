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

#include "../lib/v4l2_driver.h"
#include <sysfs/libsysfs.h>
#include <errno.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>

#define USB_ID "usb-"
#define PCI_ID "PCI:"
#define PCIe_ID "PCIe:"

char *obtain_bus_sysfs_path(char *bus_info)
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

		asprintf(&name, "%s", pcictl->path);
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

		asprintf(&name, "%s/%d-%s", busdev->path,
			 busnum, buspath);

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

void get_sysfs(char *fname)
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
		printf("sysfs path = %s\n\n", path);
		free(path);
	}

	v4l2_close(&drv);
}

void read_dir(char *dirname)
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
