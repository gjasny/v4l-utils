/* keytable.c - This program allows checking/replacing keys at IR

   Copyright (C) 2006-2010 Mauro Carvalho Chehab <mchehab@redhat.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <argp.h>

#include "parse.h"

struct keytable {
	int codes[2];
	struct keytable *next;
};

struct uevents {
	char		*key;
	char		*value;
	struct uevents	*next;
};

static int parse_code(char *string)
{
	struct parse_key *p;

	for (p = keynames; p->name != NULL; p++) {
		if (!strcasecmp(p->name, string))
			return p->value;
	}
	return -1;
}

const char *argp_program_version = "IR keytable control version 0.1.0";
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

static const char doc[] = "\nAllows get/set IR keycode/scancode tables\n"
	"You need to have read permissions on /dev/input for the program to work\n"
	"\nOn the options bellow, the arguments are:\n"
	"  DEV     - the /dev/input/event* device to control\n"
	"  SYSDEV  - the ir class as found at /sys/class/irrcv\n"
	"  TABLE   - a file wit a set of scancode=keycode value pairs\n"
	"  SCANKEY - a set of scancode1=keycode1,scancode2=keycode2.. value pairs\n"
	"\nOptions can be combined together.";

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	"enables debug messages", 0},
	{"clear",	'c',	0,		0,	"clears the old table", 0},
	{"sysdev",	's',	"SYSDEV",	0,	"ir class device to control", 0},
	{"device",	'd',	"DEV",		0,	"ir device to control", 0},
	{"read",	'r',	0,		0,	"reads the current scancode/keycode table", 0},
	{"write",	'w',	"TABLE",	0,	"write (adds) the scancodes to the device scancode/keycode table from an specified file", 0},
	{"set-key",	'k',	"SCANKEY",	0,	"Change scan/key pairs", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static const char args_doc[] =
	"--device [/dev/input/event* device]\n"
	"--sysdev [ir class (f. ex. irrcv0)]\n"
	"[for using the irrcv0 sysdev]";


static char *devclass = "irrcv0";
static char *devname = NULL;
static int read = 0;
static int clear = 0;
static int debug = 0;

struct keytable keys = {
	{0, 0}, NULL
};

/*
 * Values that are read only via sysfs node
 */
static int sysfs = 0;
static char *node_input = NULL, *node_event = NULL;
static char *drv_name = NULL,   *keytable_name = NULL;

struct keytable *nextkey = &keys;

static error_t parse_keyfile(char *fname, char **table, char **type)
{
	FILE *fin;
	int value, line = 0;
	char *scancode, *keycode, s[2048];

	if (debug)
		fprintf(stderr, "Parsing %s keycode file\n", fname);

	fin = fopen(fname, "r");
	if (!fin) {
		perror("opening keycode file");
		return errno;
	}

	while (fgets(s, sizeof(s), fin)) {
		char *p = s;
		while (*p == ' ' || *p == '\t')
			p++;
		if (!line && p[0] == '#') {
			p++;
			p = strtok(p, "\n\t =:");
			do {
				if (!strcmp(p, "table")) {
					p = strtok(NULL,"\n, ");
					*table = malloc(sizeof(p) + 1);
					strcpy(*table, p);
				} else if (!strcmp(p, "type")) {
					p = strtok(NULL,"\n, ");
					*type = malloc(sizeof(p) + 1);
					strcpy(*type, p);
				} else {
					goto err_einval;
				}
				p = strtok(NULL, "\n\t =:");
			} while (p);
			continue;
		}

		if (*p == '\n' || *p == '#')
			continue;

		scancode = strtok(p, "\n\t =:");
		if (!scancode)
			goto err_einval;
		if (!strcasecmp(scancode, "scancode")) {
			scancode = strtok(NULL, "\n\t =:");
			if (!scancode)
				goto err_einval;
		}

		keycode = strtok(NULL, "\n\t =:(");
		if (!keycode)
			goto err_einval;

		if (debug)
			fprintf(stderr, "parsing %s=%s:", scancode, keycode);
		value = parse_code(keycode);
		if (debug)
			fprintf(stderr, "\tvalue=%d\n", value);

		if (value == -1) {
			value = strtol(keycode, NULL, 0);
			if (errno)
				perror("value");
		}

		nextkey->codes[0] = (unsigned) strtol(scancode, NULL, 0);
		nextkey->codes[1] = (unsigned) value;
		nextkey->next = calloc(1, sizeof(keys));
		if (!nextkey->next) {
			perror("No memory");
			return ENOMEM;
		}
		nextkey = nextkey->next;
		line++;
	}
	fclose(fin);

	return 0;

err_einval:
	fprintf(stderr, "Invalid parameter on line %d of %s\n",
		line + 1, fname);
	return EINVAL;

}

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	char *p;
	long key;
	int rc;

	switch (k) {
	case 'v':
		debug++;
		break;
	case 'c':
		clear++;
		break;
	case 'd':
		devname = arg;
		break;
	case 's':
		devclass = arg;
		break;
	case 'r':
		read++;
		break;
	case 'w': {
		char *name = NULL, *type = NULL;

		rc = parse_keyfile(arg, &name, &type);
		if (rc < 0)
			goto err_inval;
		if (name && type)
			fprintf(stderr, "Read %s table, type %s\n", name, type);
		break;
	}
	case 'k':
		p = strtok(arg, ":=");
		do {
			if (!p)
				goto err_inval;
			nextkey->codes[0] = strtol(p, NULL, 0);
			if (errno)
				goto err_inval;

			p = strtok(NULL, ",;");
			if (!p)
				goto err_inval;
			key = parse_code(p);
			if (key == -1) {
				key = strtol(p, NULL, 0);
				if (errno)
					goto err_inval;
			}
			nextkey->codes[1] = key;

			if (debug)
				fprintf(stderr, "scancode %i=%i\n",
					nextkey->codes[0], nextkey->codes[1]);

			nextkey->next = calloc(1, sizeof(keys));
			if (!nextkey->next) {
				perror("No memory!\n");
				return ENOMEM;
			}
			nextkey = nextkey->next;

			p = strtok(NULL, ":=");
		} while (p);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;

err_inval:
	fprintf(stderr, "Invalid parameter(s)\n");
	return EINVAL;

}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc,
};

static void prtcode(int *codes)
{
	struct parse_key *p;

	for (p = keynames; p->name != NULL; p++) {
		if (p->value == (unsigned)codes[1]) {
			printf("scancode 0x%04x = %s (0x%02x)\n", codes[0], p->name, codes[1]);
			return;
		}
	}

	if (isprint (codes[1]))
		printf("scancode %d = '%c' (0x%02x)\n", codes[0], codes[1], codes[1]);
	else
		printf("scancode %d = 0x%02x\n", codes[0], codes[1]);
}

static int seek_sysfs_dir(char *dname, char *node_name, char **node_entry)
{
	DIR             *dir;
	struct dirent   *entry;
	int		rc;

	dir = opendir(dname);
	if (!dir) {
		perror(dname);
		return errno;
	}
	entry = readdir(dir);
	while (entry) {
		if (!strncmp(entry->d_name, node_name, strlen(node_name))) {
			*node_entry = malloc(strlen(dname) + strlen(entry->d_name) + 2);
			strcpy(*node_entry, dname);
			strcat(*node_entry, entry->d_name);
			strcat(*node_entry, "/");
			rc = 1;
			break;
		}
		entry = readdir(dir);
	}
	closedir(dir);

	return rc;
}

static void free_uevent(struct uevents *uevent)
{
	struct uevents *old;
	do {
		old = uevent;
		uevent = uevent->next;
		if (old->key)
			free(old->key);
		if (old->value)
			free(old->value);
		free(old);
	} while (uevent);
}

struct uevents *read_sysfs_uevents(char *dname)
{
	FILE		*fp;
	struct uevents	*next, *uevent;
	char		*event = "uevent", *file, s[4096];

	next = uevent = calloc(1, sizeof(*uevent));

	file = malloc(strlen(dname) + strlen(event) + 1);
	strcpy(file, dname);
	strcat(file, event);

	if (debug)
		fprintf(stderr, "Parsing uevent %s\n", file);


	fp = fopen(file, "r");
	if (!fp) {
		perror(file);
		free(file);
		return NULL;
	}
	while (fgets(s, sizeof(s), fp)) {
		char *p = strtok(s, "=");
		if (!p)
			continue;
		next->key = malloc(strlen(p) + 1);
		if (!next->key) {
			perror("next->key");
			free(file);
			free_uevent(uevent);
			return NULL;
		}
		strcpy(next->key, p);

		p = strtok(NULL, "\n");
		if (!p) {
			fprintf(stderr, "Error on uevent information\n");
			fclose(fp);
			free(file);
			free_uevent(uevent);
			return NULL;
		}
		next->value = malloc(strlen(p) + 1);
		if (!next->value) {
			perror("next->value");
			free(file);
			free_uevent(uevent);
			return NULL;
		}
		strcpy(next->value, p);

		if (debug)
			fprintf(stderr, "%s uevent %s=%s\n", file, next->key, next->value);

		next->next = calloc(1, sizeof(*next));
		if (!next->next) {
			perror("next->next");
			free(file);
			free_uevent(uevent);
			return NULL;
		}
		next = next->next;
	}
	fclose(fp);
	free(file);

	return uevent;
}

static char *find_device(void)
{
	struct uevents  *uevent;
	char		dname[256], *name = NULL;
	char		*input = "input", *event = "event";
	int		rc;
	char		*DEV = "/dev/";

	/*
	 * Get event sysfs node
	 */
	snprintf(dname, sizeof(dname), "/sys/class/irrcv/%s/", devclass);

	rc = seek_sysfs_dir(dname, input, &node_input);
	if (rc == 0)
		fprintf(stderr, "Couldn't find input device. Old driver?");
	if (rc <= 0)
		return NULL;
	if (debug)
		fprintf(stderr, "Input sysfs node is %s\n", node_input);

	rc = seek_sysfs_dir(node_input, event, &node_event);
	if (rc == 0)
		fprintf(stderr, "Couldn't find input device. Old driver?");
	if (rc <= 0)
		return NULL;
	if (debug)
		fprintf(stderr, "Event sysfs node is %s\n", node_event);

	uevent = read_sysfs_uevents(node_event);
	if (!uevent)
		return NULL;

	while (uevent->next) {
		if (!strcmp(uevent->key, "DEVNAME")) {
			name = malloc(strlen(uevent->value) + strlen(DEV) + 1);
			strcpy(name, DEV);
			strcat(name, uevent->value);
			break;
		}
		uevent = uevent->next;
	}
	free_uevent(uevent);

	uevent = read_sysfs_uevents(dname);
	if (!uevent)
		return NULL;
	while (uevent->next) {
		if (!strcmp(uevent->key, "DRV_NAME")) {
			drv_name = malloc(strlen(uevent->value) + 1);
			strcpy(drv_name, uevent->value);
		}
		if (!strcmp(uevent->key, "NAME")) {
			keytable_name = malloc(strlen(uevent->value) + 1);
			strcpy(keytable_name, uevent->value);
		}
		uevent = uevent->next;
	}
	free_uevent(uevent);

	if (debug)
		fprintf(stderr, "input device is %s\n", name);

	sysfs++;

	return name;
}

int main(int argc, char *argv[])
{
	int fd;
	unsigned int i, j;
	int dev_from_class = 0, write_cnt = 0;

	argp_parse(&argp, argc, argv, 0, 0, 0);

	if (!devname) {
		devname = find_device();
		if (!devname)
			return -1;
		dev_from_class++;
	}
	if (sysfs)
		fprintf(stderr, "Kernel IR driver for %s is %s (using table %s)\n",
			devclass, drv_name, keytable_name);

	if (!clear && !read && !keys.next) {
		argp_help(&argp, stderr, ARGP_HELP_SHORT_USAGE, argv[0]);
		return -1;
	}
	if (debug)
		fprintf(stderr, "Opening %s\n", devname);
	fd = open(devname, O_RDONLY);
	if (fd < 0) {
		perror(devname);
		return -1;
	}
	if (dev_from_class)
		free(devname);

	/*
	 * First step: clear, if --clear is specified
	 */
	if (clear) {
		int codes[2];

		/* Clears old table */
		for (j = 0; j < 256; j++) {
			for (i = 0; i < 256; i++) {
				codes[0] = (j << 8) | i;
				codes[1] = KEY_RESERVED;
				ioctl(fd, EVIOCSKEYCODE, codes);
			}
		}
		fprintf(stderr, "Old keytable cleared\n");
	}

	/*
	 * Second step: stores key tables from file or from commandline
	 */
	nextkey = &keys;
	while (nextkey->next) {
		int value;
		struct keytable *old;

		write_cnt++;
		if (debug)
			fprintf(stderr, "\t%04x=%04x\n",
			       nextkey->codes[0], nextkey->codes[1]);

		if (ioctl(fd, EVIOCSKEYCODE, nextkey->codes)) {
			fprintf(stderr,
				"Setting scancode 0x%04x with 0x%04x via ",
				nextkey->codes[0], nextkey->codes[1]);
			perror("EVIOCSKEYCODE");
		}
		old = nextkey;
		nextkey = nextkey->next;
		if (old != &keys)
			free(old);
	}
	if (write_cnt)
		fprintf(stderr, "Wrote %d keycode(s) to driver\n", write_cnt);

	/*
	 * Third step: display current keytable
	 */
	if (read) {
		for (j = 0; j < 256; j++) {
			for (i = 0; i < 256; i++) {
				int codes[2];

				codes[0] = (j << 8) | i;
				if (!ioctl(fd, EVIOCGKEYCODE, codes) && codes[1] != KEY_RESERVED)
					prtcode(codes);
			}
		}
	}

	return 0;
}
