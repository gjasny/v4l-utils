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

#include <config.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <dirent.h>
#include <argp.h>

#include "parse.h"

struct input_keymap_entry_v2 {
#define KEYMAP_BY_INDEX	(1 << 0)
	u_int8_t  flags;
	u_int8_t  len;
	u_int16_t index;
	u_int32_t keycode;
	u_int8_t  scancode[32];
};

#ifndef EVIOCGKEYCODE_V2
#define EVIOCGKEYCODE_V2	_IOR('E', 0x04, struct input_keymap_entry_v2)
#define EVIOCSKEYCODE_V2	_IOW('E', 0x04, struct input_keymap_entry_v2)
#endif

struct keytable {
	u_int32_t codes[2];
	struct input_keymap_entry_v2 keymap;
	struct keytable *next;
};

struct uevents {
	char		*key;
	char		*value;
	struct uevents	*next;
};

struct cfgfile {
	char		*driver;
	char		*table;
	char		*fname;
	struct cfgfile	*next;
};

struct sysfs_names  {
	char			*name;
	struct sysfs_names	*next;
};

enum rc_type {
	UNKNOWN_TYPE,
	SOFTWARE_DECODER,
	HARDWARE_DECODER,
};

enum sysfs_ver {
	VERSION_1,	/* has nodes protocol, enabled */
	VERSION_2,	/* has node protocols */
};

enum ir_protocols {
	RC_5		= 1 << 0,
	RC_6		= 1 << 1,
	NEC		= 1 << 2,
	JVC		= 1 << 3,
	SONY		= 1 << 4,
	LIRC		= 1 << 5,
	SANYO		= 1 << 6,
	RC_5_SZ		= 1 << 7,
	OTHER		= 1 << 31,
};

static int parse_code(char *string)
{
	struct parse_event *p;

	for (p = key_events; p->name != NULL; p++) {
		if (!strcasecmp(p->name, string))
			return p->value;
	}
	return -1;
}

const char *argp_program_version = "IR keytable control version "V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Mauro Carvalho Chehab <mchehab@redhat.com>";

static const char doc[] = "\nAllows get/set IR keycode/scancode tables\n"
	"You need to have read permissions on /dev/input for the program to work\n"
	"\nOn the options bellow, the arguments are:\n"
	"  DEV      - the /dev/input/event* device to control\n"
	"  SYSDEV   - the ir class as found at /sys/class/rc\n"
	"  TABLE    - a file with a set of scancode=keycode value pairs\n"
	"  SCANKEY  - a set of scancode1=keycode1,scancode2=keycode2.. value pairs\n"
	"  PROTOCOL - protocol name (nec, rc-5, rc-6, jvc, sony, sanyo, rc-5-sz, lirc, other) to be enabled\n"
	"  DELAY    - Delay before repeating a keystroke\n"
	"  PERIOD   - Period to repeat a keystroke\n"
	"  CFGFILE  - configuration file that associates a driver/table name with a keymap file\n"
	"\nOptions can be combined together.";

static const struct argp_option options[] = {
	{"verbose",	'v',	0,		0,	"enables debug messages", 0},
	{"clear",	'c',	0,		0,	"clears the old table", 0},
	{"sysdev",	's',	"SYSDEV",	0,	"ir class device to control", 0},
	{"test",	't',	0,		0,	"test if IR is generating events", 0},
	{"device",	'd',	"DEV",		0,	"ir device to control", 0},
	{"read",	'r',	0,		0,	"reads the current scancode/keycode table", 0},
	{"write",	'w',	"TABLE",	0,	"write (adds) the scancodes to the device scancode/keycode table from an specified file", 0},
	{"set-key",	'k',	"SCANKEY",	0,	"Change scan/key pairs", 0},
	{"protocol",	'p',	"PROTOCOL",	0,	"Protocol to enable (the other ones will be disabled). To enable more than one, use the option more than one time", 0},
	{"delay",	'D',	"DELAY",	0,	"Sets the delay before repeating a keystroke", 0},
	{"period",	'P',	"PERIOD",	0,	"Sets the period to repeat a keystroke", 0},
	{"auto-load",	'a',	"CFGFILE",	0,	"Auto-load a table, based on a configuration file. Only works with sysdev.", 0},
	{ 0, 0, 0, 0, 0, 0 }
};

static const char args_doc[] =
	"--device [/dev/input/event* device]\n"
	"--sysdev [ir class (f. ex. rc0)]\n"
	"[for using the rc0 sysdev]";

/* Static vars to store the parameters */
static char *devclass = "rc0";
static char *devicename = NULL;
static int readtable = 0;
static int clear = 0;
static int debug = 0;
static int test = 0;
static int delay = 0;
static int period = 0;
static enum ir_protocols ch_proto = 0;

struct keytable keys = {
	.codes = {0, 0},
	.next = NULL
};


struct cfgfile cfg = {
	NULL, NULL, NULL, NULL
};

/*
 * Stores the input layer protocol version
 */
static int input_protocol_version = 0;

/*
 * Values that are read only via sysfs node
 */
static int sysfs = 0;

struct rc_device {
	char *sysfs_name;	/* Device sysfs node name */
	char *input_name;	/* Input device file name */
	char *drv_name;		/* Kernel driver that implements it */
	char *keytable_name;	/* Keycode table name */

	enum sysfs_ver version; /* sysfs version */
	enum rc_type type;	/* Software (raw) or hardware decoder */
	enum ir_protocols supported, current; /* Current and supported IR protocols */
};

struct keytable *nextkey = &keys;

static error_t parse_keyfile(char *fname, char **table)
{
	FILE *fin;
	int value, line = 0;
	char *scancode, *keycode, s[2048];

	*table = NULL;

	if (debug)
		fprintf(stderr, "Parsing %s keycode file\n", fname);

	fin = fopen(fname, "r");
	if (!fin) {
		return errno;
	}

	while (fgets(s, sizeof(s), fin)) {
		char *p = s;

		line++;
		while (*p == ' ' || *p == '\t')
			p++;
		if (line==1 && p[0] == '#') {
			p++;
			p = strtok(p, "\n\t =:");
			do {
				if (!p)
					goto err_einval;
				if (!strcmp(p, "table")) {
					p = strtok(NULL,"\n, ");
					if (!p)
						goto err_einval;
					*table = malloc(strlen(p) + 1);
					strcpy(*table, p);
				} else if (!strcmp(p, "type")) {
					p = strtok(NULL, " ,\n");
					do {
						if (!p)
							goto err_einval;
						if (!strcasecmp(p,"rc5") || !strcasecmp(p,"rc-5"))
							ch_proto |= RC_5;
						else if (!strcasecmp(p,"rc6") || !strcasecmp(p,"rc-6"))
							ch_proto |= RC_6;
						else if (!strcasecmp(p,"nec"))
							ch_proto |= NEC;
						else if (!strcasecmp(p,"jvc"))
							ch_proto |= JVC;
						else if (!strcasecmp(p,"sony"))
							ch_proto |= SONY;
						else if (!strcasecmp(p,"sanyo"))
							ch_proto |= SANYO;
						else if (!strcasecmp(p,"rc-5-sz"))
							ch_proto |= RC_5_SZ;
						else if (!strcasecmp(p,"other") || !strcasecmp(p,"unknown"))
							ch_proto |= OTHER;
						else {
							fprintf(stderr, "Protocol %s invalid\n", p);
							goto err_einval;
						}
						p = strtok(NULL, " ,\n");
					} while (p);
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

		nextkey->codes[0] = (unsigned) strtoul(scancode, NULL, 0);
		nextkey->codes[1] = (unsigned) value;
		nextkey->next = calloc(1, sizeof(*nextkey));
		if (!nextkey->next) {
			perror("parse_keyfile");
			return ENOMEM;
		}
		nextkey = nextkey->next;
	}
	fclose(fin);

	return 0;

err_einval:
	fprintf(stderr, "Invalid parameter on line %d of %s\n",
		line, fname);
	return EINVAL;

}

struct cfgfile *nextcfg = &cfg;

static error_t parse_cfgfile(char *fname)
{
	FILE *fin;
	int line = 0;
	char s[2048];
	char *driver, *table, *filename;

	if (debug)
		fprintf(stderr, "Parsing %s config file\n", fname);

	fin = fopen(fname, "r");
	if (!fin) {
		perror("opening keycode file");
		return errno;
	}

	while (fgets(s, sizeof(s), fin)) {
		char *p = s;

		line++;
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\n' || *p == '#')
			continue;

		driver = strtok(p, "\t ");
		if (!driver)
			goto err_einval;

		table = strtok(NULL, "\t ");
		if (!table)
			goto err_einval;

		filename = strtok(NULL, "\t #\n");
		if (!filename)
			goto err_einval;

		if (debug)
			fprintf(stderr, "Driver %s, Table %s => file %s\n",
				driver, table, filename);

		nextcfg->driver = malloc(strlen(driver) + 1);
		strcpy(nextcfg->driver, driver);

		nextcfg->table = malloc(strlen(table) + 1);
		strcpy(nextcfg->table, table);

		nextcfg->fname = malloc(strlen(filename) + 1);
		strcpy(nextcfg->fname, filename);

		nextcfg->next = calloc(1, sizeof(*nextcfg));
		if (!nextcfg->next) {
			perror("parse_cfgfile");
			return ENOMEM;
		}
		nextcfg = nextcfg->next;
	}
	fclose(fin);

	return 0;

err_einval:
	fprintf(stderr, "Invalid parameter on line %d of %s\n",
		line, fname);
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
	case 't':
		test++;
		break;
	case 'c':
		clear++;
		break;
	case 'D':
		delay = atoi(arg);
		break;
	case 'P':
		period = atoi(arg);
		break;
	case 'd':
		devicename = arg;
		break;
	case 's':
		devclass = arg;
		break;
	case 'r':
		readtable++;
		break;
	case 'w': {
		char *name = NULL;

		rc = parse_keyfile(arg, &name);
		if (rc)
			goto err_inval;
		if (name)
			fprintf(stderr, "Read %s table\n", name);
		break;
	}
	case 'a': {
		rc = parse_cfgfile(arg);
		if (rc)
			goto err_inval;
		break;
	}
	case 'k':
		p = strtok(arg, ":=");
		do {
			if (!p)
				goto err_inval;
			nextkey->codes[0] = strtoul(p, NULL, 0);
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
				fprintf(stderr, "scancode 0x%04x=%u\n",
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
	case 'p':
		p = strtok(arg, ",;");
		do {
			if (!p)
				goto err_inval;
			if (!strcasecmp(p,"rc5") || !strcasecmp(p,"rc-5"))
				ch_proto |= RC_5;
			else if (!strcasecmp(p,"rc6") || !strcasecmp(p,"rc-6"))
				ch_proto |= RC_6;
			else if (!strcasecmp(p,"nec"))
				ch_proto |= NEC;
			else if (!strcasecmp(p,"jvc"))
				ch_proto |= JVC;
			else if (!strcasecmp(p,"sony"))
				ch_proto |= SONY;
			else if (!strcasecmp(p,"sanyo"))
				ch_proto |= SANYO;
			else if (!strcasecmp(p,"lirc"))
				ch_proto |= LIRC;
			else if (!strcasecmp(p,"rc-5-sz"))
				ch_proto |= RC_5_SZ;
			else
				goto err_inval;
			p = strtok(NULL, ",;");
		} while (p);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;

err_inval:
	fprintf(stderr, "Invalid parameter(s)\n");
	return ARGP_ERR_UNKNOWN;

}

static struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc,
};

static void prtcode(int *codes)
{
	struct parse_event *p;

	for (p = key_events; p->name != NULL; p++) {
		if (p->value == (unsigned)codes[1]) {
			printf("scancode 0x%04x = %s (0x%02x)\n", codes[0], p->name, codes[1]);
			return;
		}
	}

	if (isprint (codes[1]))
		printf("scancode 0x%04x = '%c' (0x%02x)\n", codes[0], codes[1], codes[1]);
	else
		printf("scancode 0x%04x = 0x%02x\n", codes[0], codes[1]);
}

static void free_names(struct sysfs_names *names)
{
	struct sysfs_names *old;
	do {
		old = names;
		names = names->next;
		if (old->name)
			free(old->name);
		free(old);
	} while (names);
}

static struct sysfs_names *seek_sysfs_dir(char *dname, char *node_name)
{
	DIR             	*dir;
	struct dirent   	*entry;
	struct sysfs_names	*names, *cur_name;

	names = calloc(sizeof(*names), 1);

	cur_name = names;

	dir = opendir(dname);
	if (!dir) {
		perror(dname);
		return NULL;
	}
	entry = readdir(dir);
	while (entry) {
		if (!node_name || !strncmp(entry->d_name, node_name, strlen(node_name))) {
			cur_name->name = malloc(strlen(dname) + strlen(entry->d_name) + 2);
			if (!cur_name->name)
				goto err;
			strcpy(cur_name->name, dname);
			strcat(cur_name->name, entry->d_name);
			if (node_name)
				strcat(cur_name->name, "/");
			cur_name->next = calloc(sizeof(*cur_name), 1);
			if (!cur_name->next)
				goto err;
			cur_name = cur_name->next;
		}
		entry = readdir(dir);
	}
	closedir(dir);

	if (names == cur_name) {
		fprintf(stderr, "Couldn't find any node at %s%s*.\n",
			dname, node_name);
		free (names);
		names = NULL;
	}
	return names;

err:
	perror("Seek dir");
	free_names(names);
	return NULL;
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

static struct uevents *read_sysfs_uevents(char *dname)
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

static struct sysfs_names *find_device(char *name)
{
	char		dname[256];
	char		*input = "rc";
	static struct sysfs_names *names, *cur;
	/*
	 * Get event sysfs node
	 */
	snprintf(dname, sizeof(dname), "/sys/class/rc/");

	names = seek_sysfs_dir(dname, input);
	if (!names)
		return NULL;

	if (debug) {
		for (cur = names; cur->next; cur = cur->next) {
			fprintf(stderr, "Found device %s\n", cur->name);
		}
	}

	if (name) {
		static struct sysfs_names *tmp;
		char *p, *n;
		int found = 0;

		n = malloc(strlen(name) + 2);
		strcpy(n, name);
		strcat(n,"/");
		for (cur = names; cur->next; cur = cur->next) {
			if (cur->name) {
				p = cur->name + strlen(dname);
				if (p && !strcmp(p, n)) {
					found = 1;
					break;
				}
			}
		}
		free(n);
		if (!found) {
			free_names(names);
			fprintf(stderr, "Not found device %s\n", name);
			return NULL;
		}
		tmp = calloc(sizeof(*names), 1);
		tmp->name = cur->name;
		cur->name = NULL;
		free_names(names);
		return tmp;
	}

	return names;
}

static enum ir_protocols v1_get_hw_protocols(char *name)
{
	FILE *fp;
	char *p, buf[4096];
	enum ir_protocols proto = 0;

	fp = fopen(name, "r");
	if (!fp) {
		perror(name);
		return 0;
	}

	if (!fgets(buf, sizeof(buf), fp)) {
		perror(name);
		fclose(fp);
		return 0;
	}

	p = strtok(buf, " \n");
	while (p) {
		if (debug)
			fprintf(stderr, "%s protocol %s\n", name, p);
		if (!strcmp(p, "rc-5"))
			proto |= RC_5;
		else if (!strcmp(p, "rc-6"))
			proto |= RC_6;
		else if (!strcmp(p, "nec"))
			proto |= NEC;
		else if (!strcmp(p, "jvc"))
			proto |= JVC;
		else if (!strcmp(p, "sony"))
			proto |= SONY;
		else if (!strcmp(p, "sanyo"))
			proto |= SANYO;
		else if (!strcmp(p, "rc-5-sz"))
			proto |= RC_5_SZ;
		else
			proto |= OTHER;

		p = strtok(NULL, " \n");
	}

	fclose(fp);

	return proto;
}

static int v1_set_hw_protocols(struct rc_device *rc_dev)
{
	FILE *fp;
	char name[4096];

	strcpy(name, rc_dev->sysfs_name);
	strcat(name, "/protocol");

	fp = fopen(name, "w");
	if (!fp) {
		perror(name);
		return errno;
	}

	if (rc_dev->current & RC_5)
		fprintf(fp, "rc-5 ");

	if (rc_dev->current & RC_6)
		fprintf(fp, "rc-6 ");

	if (rc_dev->current & NEC)
		fprintf(fp, "nec ");

	if (rc_dev->current & JVC)
		fprintf(fp, "jvc ");

	if (rc_dev->current & SONY)
		fprintf(fp, "sony ");

	if (rc_dev->current & SANYO)
		fprintf(fp, "sanyo ");

	if (rc_dev->current & RC_5_SZ)
		fprintf(fp, "rc-5-sz ");

	if (rc_dev->current & OTHER)
		fprintf(fp, "unknown ");

	fprintf(fp, "\n");

	fclose(fp);

	return 0;
}

static int v1_get_sw_enabled_protocol(char *dirname)
{
	FILE *fp;
	char *p, buf[4096], name[512];
	int rc;

	strcpy(name, dirname);
	strcat(name, "/enabled");

	fp = fopen(name, "r");
	if (!fp) {
		perror(name);
		return 0;
	}

	if (!fgets(buf, sizeof(buf), fp)) {
		perror(name);
		fclose(fp);
		return 0;
	}

	if (fclose(fp)) {
		perror(name);
		return errno;
	}

	p = strtok(buf, " \n");
	if (!p) {
		fprintf(stderr, "%s has invalid content: '%s'\n", name, buf);
		return 0;
	}

	rc = atoi(p);

	if (debug)
		fprintf(stderr, "protocol %s is %s\n",
			name, rc? "enabled" : "disabled");

	if (atoi(p) == 1)
		return 1;

	return 0;
}

static int v1_set_sw_enabled_protocol(struct rc_device *rc_dev,
				   char *dirname, int enabled)
{
	FILE *fp;
	char name[512];

	strcpy(name, rc_dev->sysfs_name);
	strcat(name, dirname);
	strcat(name, "/enabled");

	fp = fopen(name, "w");
	if (!fp) {
		perror(name);
		return errno;
	}

	if (enabled)
		fprintf(fp, "1");
	else
		fprintf(fp, "0");

	if (fclose(fp)) {
		perror(name);
		return errno;
	}

	return 0;
}

static enum ir_protocols v2_get_protocols(struct rc_device *rc_dev, char *name)
{
	FILE *fp;
	char *p, buf[4096];
	int enabled;
	enum ir_protocols proto;

	fp = fopen(name, "r");
	if (!fp) {
		perror(name);
		return 0;
	}

	if (!fgets(buf, sizeof(buf), fp)) {
		perror(name);
		fclose(fp);
		return 0;
	}

	p = strtok(buf, " \n");
	while (p) {
		if (*p == '[') {
			enabled = 1;
			p++;
			p[strlen(p)-1] = '\0';
		} else
			enabled = 0;

		if (debug)
			fprintf(stderr, "%s protocol %s (%s)\n", name, p,
				enabled? "enabled" : "disabled");

		if (!strcmp(p, "rc-5"))
			proto = RC_5;
		else if (!strcmp(p, "rc-6"))
			proto = RC_6;
		else if (!strcmp(p, "nec"))
			proto = NEC;
		else if (!strcmp(p, "jvc"))
			proto = JVC;
		else if (!strcmp(p, "sony"))
			proto = SONY;
		else if (!strcmp(p, "sanyo"))
			proto = SANYO;
		else if (!strcmp(p, "lirc"))	/* Only V2 has LIRC support */
			proto = LIRC;
		else if (!strcmp(p, "rc-5-sz"))
			proto = RC_5_SZ;
		else
			proto = OTHER;

		rc_dev->supported |= proto;
		if (enabled)
			rc_dev->current |= proto;

		p = strtok(NULL, " \n");
	}

	fclose(fp);

	return 0;
}

static int v2_set_protocols(struct rc_device *rc_dev)
{
	FILE *fp;
	char name[4096];

	strcpy(name, rc_dev->sysfs_name);
	strcat(name, "/protocols");

	fp = fopen(name, "w");
	if (!fp) {
		perror(name);
		return errno;
	}

	/* Disable all protocols */
	fprintf(fp, "none\n");

	if (rc_dev->current & RC_5)
		fprintf(fp, "+rc-5\n");

	if (rc_dev->current & RC_6)
		fprintf(fp, "+rc-6\n");

	if (rc_dev->current & NEC)
		fprintf(fp, "+nec\n");

	if (rc_dev->current & JVC)
		fprintf(fp, "+jvc\n");

	if (rc_dev->current & SONY)
		fprintf(fp, "+sony\n");

	if (rc_dev->current & SANYO)
		fprintf(fp, "+sanyo\n");

	if (rc_dev->current & LIRC)
		fprintf(fp, "+lirc\n");

	if (rc_dev->current & RC_5_SZ)
		fprintf(fp, "+rc-5-sz\n");

	if (rc_dev->current & OTHER)
		fprintf(fp, "+unknown\n");

	if (fclose(fp)) {
		perror(name);
		return errno;
	}

	return 0;
}

static void show_proto(	enum ir_protocols proto)
{
	if (proto & NEC)
		fprintf (stderr, "NEC ");
	if (proto & RC_5)
		fprintf (stderr, "RC-5 ");
	if (proto & RC_6)
		fprintf (stderr, "RC-6 ");
	if (proto & JVC)
		fprintf (stderr, "JVC ");
	if (proto & SONY)
		fprintf (stderr, "SONY ");
	if (proto & SANYO)
		fprintf (stderr, "SANYO ");
	if (proto & LIRC)
		fprintf (stderr, "LIRC ");
	if (proto & RC_5_SZ)
		fprintf (stderr, "RC-5-SZ ");
	if (proto & OTHER)
		fprintf (stderr, "other ");
}

static int get_attribs(struct rc_device *rc_dev, char *sysfs_name)
{
	struct uevents  *uevent;
	char		*input = "input", *event = "event";
	char		*DEV = "/dev/";
	static struct sysfs_names *input_names, *event_names, *attribs, *cur;

	/* Clean the attributes */
	memset(rc_dev, 0, sizeof(*rc_dev));

	rc_dev->sysfs_name = sysfs_name;

	input_names = seek_sysfs_dir(rc_dev->sysfs_name, input);
	if (!input_names)
		return EINVAL;
	if (input_names->next->next) {
		fprintf(stderr, "Found more than one input interface."
				"This is currently unsupported\n");
		return EINVAL;
	}
	if (debug)
		fprintf(stderr, "Input sysfs node is %s\n", input_names->name);

	event_names = seek_sysfs_dir(input_names->name, event);
	free_names(input_names);
	if (!event_names) {
		free_names(event_names);
		return EINVAL;
	}
	if (event_names->next->next) {
		free_names(event_names);
		fprintf(stderr, "Found more than one event interface."
				"This is currently unsupported\n");
		return EINVAL;
	}
	if (debug)
		fprintf(stderr, "Event sysfs node is %s\n", event_names->name);

	uevent = read_sysfs_uevents(event_names->name);
	free_names(event_names);
	if (!uevent)
		return EINVAL;

	while (uevent->next) {
		if (!strcmp(uevent->key, "DEVNAME")) {
			rc_dev->input_name = malloc(strlen(uevent->value) + strlen(DEV) + 1);
			strcpy(rc_dev->input_name, DEV);
			strcat(rc_dev->input_name, uevent->value);
			break;
		}
		uevent = uevent->next;
	}
	free_uevent(uevent);

	if (!rc_dev->input_name) {
		fprintf(stderr, "Input device name not found.\n");
		return EINVAL;
	}

	uevent = read_sysfs_uevents(rc_dev->sysfs_name);
	if (!uevent)
		return EINVAL;
	while (uevent->next) {
		if (!strcmp(uevent->key, "DRV_NAME")) {
			rc_dev->drv_name = malloc(strlen(uevent->value) + 1);
			strcpy(rc_dev->drv_name, uevent->value);
		}
		if (!strcmp(uevent->key, "NAME")) {
			rc_dev->keytable_name = malloc(strlen(uevent->value) + 1);
			strcpy(rc_dev->keytable_name, uevent->value);
		}
		uevent = uevent->next;
	}
	free_uevent(uevent);

	if (debug)
		fprintf(stderr, "input device is %s\n", rc_dev->input_name);

	sysfs++;

	rc_dev->type = SOFTWARE_DECODER;

	/* Get the other attribs - basically IR decoders */
	attribs = seek_sysfs_dir(rc_dev->sysfs_name, NULL);
	for (cur = attribs; cur->next; cur = cur->next) {
		if (!cur->name)
			continue;
		if (strstr(cur->name, "/protocols")) {
			rc_dev->version = VERSION_2;
			rc_dev->type = UNKNOWN_TYPE;
			v2_get_protocols(rc_dev, cur->name);
		} else if (strstr(cur->name, "/protocol")) {
			rc_dev->version = VERSION_1;
			rc_dev->type = HARDWARE_DECODER;
			rc_dev->current = v1_get_hw_protocols(cur->name);
		} else if (strstr(cur->name, "/supported_protocols")) {
			rc_dev->version = VERSION_1;
			rc_dev->supported = v1_get_hw_protocols(cur->name);
		} else if (strstr(cur->name, "/nec_decoder")) {
			rc_dev->supported |= NEC;
			if (v1_get_sw_enabled_protocol(cur->name))
				rc_dev->current |= NEC;
		} else if (strstr(cur->name, "/rc5_decoder")) {
			rc_dev->supported |= RC_5;
			if (v1_get_sw_enabled_protocol(cur->name))
				rc_dev->current |= RC_5;
		} else if (strstr(cur->name, "/rc6_decoder")) {
			rc_dev->supported |= RC_6;
			if (v1_get_sw_enabled_protocol(cur->name))
				rc_dev->current |= RC_6;
		} else if (strstr(cur->name, "/jvc_decoder")) {
			rc_dev->supported |= JVC;
			if (v1_get_sw_enabled_protocol(cur->name))
				rc_dev->current |= JVC;
		} else if (strstr(cur->name, "/sony_decoder")) {
			rc_dev->supported |= SONY;
			if (v1_get_sw_enabled_protocol(cur->name))
				rc_dev->current |= SONY;
		}
	}

	return 0;
}

static int set_proto(struct rc_device *rc_dev)
{
	int rc = 0;

	if (rc_dev->version == VERSION_2) {
		rc = v2_set_protocols(rc_dev);
		return rc;
	}

	if (rc_dev->type == SOFTWARE_DECODER) {
		if (rc_dev->supported & NEC)
			rc += v1_set_sw_enabled_protocol(rc_dev, "/nec_decoder",
						      rc_dev->current & NEC);
		if (rc_dev->supported & RC_5)
			rc += v1_set_sw_enabled_protocol(rc_dev, "/rc5_decoder",
						      rc_dev->current & RC_5);
		if (rc_dev->supported & RC_6)
			rc += v1_set_sw_enabled_protocol(rc_dev, "/rc6_decoder",
						      rc_dev->current & RC_6);
		if (rc_dev->supported & JVC)
			rc += v1_set_sw_enabled_protocol(rc_dev, "/jvc_decoder",
						      rc_dev->current & JVC);
		if (rc_dev->supported & SONY)
			rc += v1_set_sw_enabled_protocol(rc_dev, "/sony_decoder",
						      rc_dev->current & SONY);
	} else {
		rc = v1_set_hw_protocols(rc_dev);
	}

	return rc;
}

static int get_input_protocol_version(int fd)
{
	if (ioctl(fd, EVIOCGVERSION, &input_protocol_version) < 0) {
		fprintf(stderr,
			"Unable to query evdev protocol version: %s\n",
			strerror(errno));
		return errno;
	}
	if (debug)
		fprintf(stderr, "Input Protocol version: 0x%08x\n",
			input_protocol_version);

	return 0;
}

static void clear_table(int fd)
{
	int i, j;
	u_int32_t codes[2];
	struct input_keymap_entry_v2 entry;

	/* Clears old table */
	if (input_protocol_version < 0x10001) {
		for (j = 0; j < 256; j++) {
			for (i = 0; i < 256; i++) {
				codes[0] = (j << 8) | i;
				codes[1] = KEY_RESERVED;
				ioctl(fd, EVIOCSKEYCODE, codes);
			}
		}
	} else {
		memset(&entry, '\0', sizeof(entry));
		i = 0;
		do {
			entry.flags = KEYMAP_BY_INDEX;
			entry.keycode = KEY_RESERVED;
			entry.index = 0;

			i++;
			if (debug)
				fprintf(stderr, "Deleting entry %d\n", i);
		} while (ioctl(fd, EVIOCSKEYCODE_V2, &entry) == 0);
	}
}

static int add_keys(int fd)
{
	int write_cnt = 0;

	nextkey = &keys;
	while (nextkey->next) {
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

	return write_cnt;
}

static void display_proto(struct rc_device *rc_dev)
{
	if (rc_dev->type == HARDWARE_DECODER)
		fprintf(stderr, "Current protocols: ");
	else
		fprintf(stderr, "Enabled protocols: ");
	show_proto(rc_dev->current);
	fprintf(stderr, "\n");
}


static char *get_event_name(struct parse_event *event, u_int16_t code)
{
	struct parse_event *p;

	for (p = event; p->name != NULL; p++) {
		if (p->value == code)
			return p->name;
	}
	return "";
}

static void test_event(int fd)
{
	struct input_event ev[64];
	int rd, i;

	printf ("Testing events. Please, press CTRL-C to abort.\n");
	while (1) {
		rd = read(fd, ev, sizeof(ev));

		if (rd < (int) sizeof(struct input_event)) {
			perror("Error reading event");
			return;
		}

		for (i = 0; i < rd / sizeof(struct input_event); i++) {
			printf("%ld.%06ld: event type %s(0x%02x)",
				ev[i].time.tv_sec, ev[i].time.tv_usec,
				get_event_name(events_type, ev[i].type), ev[i].type);

			switch (ev[i].type) {
			case EV_SYN:
				printf(".\n");
				break;
			case EV_KEY:
				printf(" key_%s: %s(0x%04x)\n",
					(ev[i].value == 0) ? "up" : "down",
					get_event_name(key_events, ev[i].code),
					ev[i].type);
				break;
			case EV_REL:
				printf(": %s (0x%04x) value=%d\n",
					get_event_name(rel_events, ev[i].code),
					ev[i].type,
					ev[i].value);
				break;
			case EV_ABS:
				printf(": %s (0x%04x) value=%d\n",
					get_event_name(abs_events, ev[i].code),
					ev[i].type,
					ev[i].value);
				break;
			case EV_MSC:
				if (ev[i].code == MSC_SCAN)
					printf(": scancode = 0x%02x\n", ev[i].value);
				else
					printf(": code = %s(0x%02x), value = %d\n",
						get_event_name(msc_events, ev[i].code),
						ev[i].code, ev[i].value);
				break;
			case EV_REP:
				printf(": value = %d\n", ev[i].value);
				break;
			case EV_SW:
			case EV_LED:
			case EV_SND:
			case EV_FF:
			case EV_PWR:
			case EV_FF_STATUS:
			default:
				printf(": code = 0x%02x, value = %d\n",
					ev[i].code, ev[i].value);
				break;
			}
		}
	}
}

static void display_table_v1(struct rc_device *rc_dev, int fd)
{
	unsigned int i, j;

	for (j = 0; j < 256; j++) {
		for (i = 0; i < 256; i++) {
			int codes[2];

			codes[0] = (j << 8) | i;
			if (ioctl(fd, EVIOCGKEYCODE, codes) == -1)
				perror("EVIOCGKEYCODE");
			else if (codes[1] != KEY_RESERVED)
				prtcode(codes);
		}
	}
	display_proto(rc_dev);
}

static void display_table_v2(struct rc_device *rc_dev, int fd)
{
	int i;
	struct input_keymap_entry_v2 entry;
	int codes[2];

	memset(&entry, '\0', sizeof(entry));
	i = 0;
	do {
		entry.flags = KEYMAP_BY_INDEX;
		entry.index = i;
		entry.len = sizeof(u_int32_t);

		if (ioctl(fd, EVIOCGKEYCODE_V2, &entry) == -1)
			break;

		/* FIXME: Extend it to support scancodes > 32 bits */
		memcpy(&codes[0], entry.scancode, sizeof(codes[0]));
		codes[1] = entry.keycode;

		prtcode(codes);
		i++;
	} while (1);
	display_proto(rc_dev);
}

static void display_table(struct rc_device *rc_dev, int fd)
{
	if (input_protocol_version < 0x10001)
		display_table_v1(rc_dev, fd);
	else
		display_table_v2(rc_dev, fd);
}

static int set_rate(int fd, unsigned int delay, unsigned int period)
{
	unsigned int rep[2] = { delay, period };

	if (ioctl(fd, EVIOCSREP, rep) < 0) {
		perror("evdev ioctl");
		return -1;
	}

	printf("Changed Repeat delay to %d ms and repeat period to %d ms\n", delay, period);
	return 0;
}

static int get_rate(int fd, unsigned int *delay, unsigned int *period)
{
	unsigned int rep[2];

	if (ioctl(fd, EVIOCGREP, rep) < 0) {
		perror("evdev ioctl");
		return -1;
	}
	*delay = rep[0];
	*period = rep[1];
	printf("Repeat delay = %d ms, repeat period = %d ms\n", *delay, *period);
	return 0;
}

static void show_evdev_attribs(int fd)
{
	unsigned int delay, period;

	printf("\t");
	get_rate(fd, &delay, &period);
}

static void device_info(int fd, char *prepend)
{
	struct input_id id;
	char buf[32];
	int rc;

	rc = ioctl(fd, EVIOCGNAME(sizeof(buf)), buf);
	if (rc >= 0)
		fprintf(stderr,"%sName: %.*s\n",prepend, rc, buf);
	else
		perror ("EVIOCGNAME");

	rc = ioctl(fd, EVIOCGID, &id);
	if (rc >= 0)
		fprintf(stderr,
			"%sbus: %d, vendor/product: %04x:%04x, version: 0x%04x\n",
			prepend, id.bustype, id.vendor, id.product, id.version);
	else
		perror ("EVIOCGID");
}

static int show_sysfs_attribs(struct rc_device *rc_dev)
{
	static struct sysfs_names *names, *cur;
	int fd;

	names = find_device(NULL);
	if (!names)
		return -1;
	for (cur = names; cur->next; cur = cur->next) {
		if (cur->name) {
			if (get_attribs(rc_dev, cur->name))
				return -1;
			fprintf(stderr, "Found %s (%s) with:\n",
				rc_dev->sysfs_name,
				rc_dev->input_name);
			fprintf(stderr, "\tDriver %s, table %s\n",
				rc_dev->drv_name,
				rc_dev->keytable_name);
			fprintf(stderr, "\tSupported protocols: ");
			show_proto(rc_dev->supported);
			fprintf(stderr, "\n\t");
			display_proto(rc_dev);
			fd = open(rc_dev->input_name, O_RDONLY);
			if (fd > 0) {
				device_info(fd, "\t");
				show_evdev_attribs(fd);
				close(fd);
			} else {
				printf("\tExtra capabilities: <access denied>\n");
			}
		}
	}
	return 0;
}

int main(int argc, char *argv[])
{
	int dev_from_class = 0, write_cnt;
	int fd;
	static struct sysfs_names *names;
	struct rc_device	  rc_dev;

	argp_parse(&argp, argc, argv, 0, 0, 0);

	/* Just list all devices */
	if (!clear && !readtable && !keys.next && !ch_proto && !cfg.next && !test && !delay && !period) {
		if (devicename) {
			fd = open(devicename, O_RDONLY);
			if (fd < 0) {
				perror("Can't open device");
				return -1;
			}
			device_info(fd, "");
			close(fd);
			return 0;
		}
		if (show_sysfs_attribs(&rc_dev))
			return -1;

		return 0;
	}

	if (cfg.next && (clear || keys.next || ch_proto || devicename)) {
		fprintf (stderr, "Auto-mode can be used only with --read, --debug and --sysdev options\n");
		return -1;
	}
	if (!devicename) {
		names = find_device(devclass);
		if (!names)
			return -1;
		rc_dev.sysfs_name = names->name;
		if (get_attribs(&rc_dev, names->name)) {
			free_names(names);
			return -1;
		}
		names->name = NULL;
		free_names(names);

		devicename = rc_dev.input_name;
		dev_from_class++;
	}

	if (cfg.next) {
		struct cfgfile *cur;
		char *fname, *name;
		int rc;

		for (cur = &cfg; cur->next; cur = cur->next) {
			if ((!rc_dev.drv_name || strcasecmp(cur->driver, rc_dev.drv_name)) && strcasecmp(cur->driver, "*"))
				continue;
			if ((!rc_dev.keytable_name || strcasecmp(cur->table, rc_dev.keytable_name)) && strcasecmp(cur->table, "*"))
				continue;
			break;
		}

		if (!cur->next) {
			if (debug)
				fprintf(stderr, "Table for %s, %s not found. Keep as-is\n",
				       rc_dev.drv_name, rc_dev.keytable_name);
			return 0;
		}
		if (debug)
			fprintf(stderr, "Table for %s, %s is on %s file.\n",
				rc_dev.drv_name, rc_dev.keytable_name,
				cur->fname);
		if (cur->fname[0] == '/' || ((cur->fname[0] == '.') && strchr(cur->fname, '/'))) {
			fname = cur->fname;
			rc = parse_keyfile(fname, &name);
			if (rc < 0) {
				fprintf(stderr, "Can't load %s table\n", fname);
				return -1;
			}
		} else {
			fname = malloc(strlen(cur->fname) + strlen(IR_KEYTABLE_USER_DIR) + 2);
			strcpy(fname, IR_KEYTABLE_USER_DIR);
			strcat(fname, "/");
			strcat(fname, cur->fname);
			rc = parse_keyfile(fname, &name);
			if (rc != 0) {
				fname = malloc(strlen(cur->fname) + strlen(IR_KEYTABLE_SYSTEM_DIR) + 2);
				strcpy(fname, IR_KEYTABLE_SYSTEM_DIR);
				strcat(fname, "/");
				strcat(fname, cur->fname);
				rc = parse_keyfile(fname, &name);
			}
			if (rc != 0) {
				fprintf(stderr, "Can't load %s table from %s or %s\n", cur->fname, IR_KEYTABLE_USER_DIR, IR_KEYTABLE_SYSTEM_DIR);
				return -1;
			}
		}
		if (!keys.next) {
			fprintf(stderr, "Empty table %s\n", fname);
			return -1;
		}
		clear = 1;
	}

	if (debug)
		fprintf(stderr, "Opening %s\n", devicename);
	fd = open(devicename, O_RDONLY);
	if (fd < 0) {
		perror(devicename);
		return -1;
	}
	if (dev_from_class)
		free(devicename);
	if (get_input_protocol_version(fd))
		return -1;

	/*
	 * First step: clear, if --clear is specified
	 */
	if (clear) {
		clear_table(fd);
		fprintf(stderr, "Old keytable cleared\n");
	}

	/*
	 * Second step: stores key tables from file or from commandline
	 */
	write_cnt = add_keys(fd);
	if (write_cnt)
		fprintf(stderr, "Wrote %d keycode(s) to driver\n", write_cnt);

	/*
	 * Third step: change protocol
	 */
	if (ch_proto) {
		rc_dev.current = ch_proto;
		if (set_proto(&rc_dev))
			fprintf(stderr, "Couldn't change the IR protocols\n");
		else {
			fprintf(stderr, "Protocols changed to ");
			show_proto(rc_dev.current);
			fprintf(stderr, "\n");
		}
	}

	/*
	 * Fourth step: display current keytable
	 */
	if (readtable)
		display_table(&rc_dev, fd);

	/*
	 * Fiveth step: change repeat rate/delay
	 */
	if (delay || period) {
		unsigned int new_delay, new_period;
		get_rate(fd, &new_delay, &new_period);
		if (delay)
			new_delay = delay;
		if (period)
			new_period = period;
		set_rate(fd, new_delay, new_period);
	}

	if (test)
		test_event(fd);

	return 0;
}
