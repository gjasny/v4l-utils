/*
 *  ir-ctl.c - Program to send and receive IR using lirc interface
 *
 *  Copyright (C) 2016 Sean Young <sean@mess.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <argp.h>
#include <sysexits.h>

#include <config.h>

#include <linux/lirc.h>

#include "ir-encode.h"

#ifdef ENABLE_NLS
# define _(string) gettext(string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# include <string.h>
# define _(string) string
#endif

# define N_(string) string

/* taken from glibc unistd.h */
#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
    ({ long int __result;                                                     \
       do __result = (long int) (expression);                                 \
       while (__result == -1L && errno == EINTR);                             \
       __result; })
#endif

/* See drivers/media/rc/ir-lirc-codec.c line 23 */
#define LIRCBUF_SIZE	512
#define IR_DEFAULT_TIMEOUT 125000

const char *argp_program_version = "IR raw version " V4L_UTILS_VERSION;
const char *argp_program_bug_address = "Sean Young <sean@mess.org>";

/*
 * Since this program drives the lirc interface, use the same terminology
 */
struct file {
	struct file *next;
	const char *fname;
	bool is_scancode;
	union {
		struct {
			unsigned carrier;
			unsigned len;
			unsigned buf[LIRCBUF_SIZE];
		};
		struct {
			unsigned scancode;
			unsigned protocol;
		};
	};
};

struct arguments {
	char *device;
	bool features;
	bool receive;
	bool verbose;
	struct file *send;
	bool oneshot;
	char *savetofile;
	int wideband;
	unsigned carrier_low, carrier_high;
	unsigned timeout;
	unsigned gap;
	int carrier_reports;
	unsigned carrier;
	unsigned duty;
	unsigned emitters;
	bool work_to_do;
};

static const struct argp_option options[] = {
	{ "device",	'd',	N_("DEV"),	0,	N_("lirc device to use") },
	{ "features",	'f',	0,		0,	N_("list lirc device features") },
	{ "receive",	'r',	N_("FILE"),	OPTION_ARG_OPTIONAL,	N_("receive IR to stdout or file") },
	{ "send",	's',	N_("FILE"),	0,	N_("send IR pulse and space file") },
	{ "scancode", 'S',	N_("SCANCODE"),	0,	N_("send IR scancode in protocol specified") },
	{ "verbose",	'v',	0,		0,	N_("verbose output") },
		{ .doc = N_("Receiving options:") },
	{ "one-shot",	'1',	0,		0,	N_("end receiving after first message") },
	{ "wideband",	'w',	0,		0,	N_("use wideband receiver aka learning mode") },
	{ "no-wideband",'n',	0,		0,	N_("use normal narrowband receiver, disable learning mode") },
	{ "carrier-range", 'R', N_("RANGE"),	0,	N_("set receiver carrier range") },
	{ "measure-carrier", 'm', 0,		0,	N_("report carrier frequency") },
	{ "no-measure-carrier", 'M', 0,		0,	N_("disable reporting carrier frequency") },
	{ "timeout",	't',	N_("TIMEOUT"),	0,	N_("set receiving timeout") },
		{ .doc = N_("Sending options:") },
	{ "carrier",	'c',	N_("CARRIER"),	0,	N_("set send carrier") },
	{ "duty-cycle",	'D',	N_("DUTY"),	0,	N_("set duty cycle") },
	{ "emitters",	'e',	N_("EMITTERS"),	0,	N_("set send emitters") },
	{ "gap",	'g',	N_("GAP"),	0,	N_("set gap between files or scancodes") },
	{ }
};

static const char args_doc[] = N_(
	"--features\n"
	"--receive [save to file]\n"
	"--send [file to send]\n"
	"--scancode [scancode to send]\n"
	"[to set lirc option]");

static const char doc[] = N_(
	"\nReceive IR, send IR and list features of lirc device\n"
	"You will need permission on /dev/lirc for the program to work\n"
	"\nOn the options below, the arguments are:\n"
	"  DEV	    - the /dev/lirc* device to use\n"
	"  FILE     - a text file containing pulses and spaces\n"
	"  CARRIER  - the carrier frequency to use for sending\n"
	"  DUTY     - the duty cycle to use for sending\n"
	"  EMITTERS - comma separated list of emitters to use for sending, e.g. 1,2\n"
	"  GAP      - gap between pulse and files or scancodes in microseconds\n"
	"  RANGE    - set range of accepted carrier frequencies, e.g. 20000-40000\n"
	"  TIMEOUT  - set length of space before receiving stops in microseconds\n"
	"  SCANCODE - protocol:scancode, e.g. nec:0xa814\n\n"
	"Note that most lirc setting have global state, i.e. the device will remain\n"
	"in this state until set otherwise.");

static int strtoint(const char *p, const char *unit)
{
	char *end;
	long arg = strtol(p, &end, 10);
	if (end == NULL || (end[0] != 0 && strcasecmp(end, unit) != 0))
		return 0;

	if (arg <= 0 || arg >= 0xffffff)
		return 0;

	return arg;
}

static bool strtoscancode(const char *p, unsigned *ret)
{
	char *end;
	long long arg = strtoll(p, &end, 0);
	if (end == NULL || end[0] != 0)
		return false;

	if (arg < 0 || arg > 0xffffffff)
		return false;

	*ret = arg;
	return true;
}

static unsigned parse_emitters(char *p)
{
	unsigned emit = 0;
	const char *sep = " ,;:";
	char *saveptr, *q;

	q = strtok_r(p, sep, &saveptr);
	while (q) {
		if (*q) {
			char *endptr;
			long e = strtol(q, &endptr, 10);
			if ((endptr && *endptr) || e <= 0 || e > 32)
				return 0;

			emit |= 1 << (e - 1);
		}
		q = strtok_r(NULL, sep, &saveptr);
	}

	return emit;
}

static struct file *read_file(struct arguments *args, const char *fname)
{
	bool expect_pulse = true;
	int lineno = 0, lastspace = 0;
	char line[1024];
	int len = 0;
	const char *whitespace = " \n\r\t";
	struct file *f;

	FILE *input = fopen(fname, "r");

	if (!input) {
		fprintf(stderr, _("%s: could not open: %m\n"), fname);
		return NULL;
	}

	f = malloc(sizeof(*f));
	if (f == NULL) {
		fprintf(stderr, _("Failed to allocate memory\n"));
		return NULL;
	}
	f->is_scancode = false;
	f->carrier = 0;
	f->fname = fname;

	while (fgets(line, sizeof(line), input)) {
		char *p, *saveptr;
		lineno++;
		char *keyword = strtok_r(line, whitespace, &saveptr);

		if (keyword == NULL || *keyword == 0 || *keyword == '#' ||
				(keyword[0] == '/' && keyword[1] == '/'))
			continue;

		p = strtok_r(NULL, whitespace, &saveptr);
		if (p == NULL) {
			fprintf(stderr, _("warning: %s:%d: missing argument\n"), fname, lineno);
			continue;
		}

		if (strcmp(keyword, "scancode") == 0) {
			enum rc_proto proto;
			unsigned scancode, carrier;
			char *scancodestr;

			if (!expect_pulse) {
				f->buf[len++] = args->gap;
				expect_pulse = true;
			}

			scancodestr = strchr(p, ':');
			if (!scancodestr) {
				fprintf(stderr, _("error: %s:%d: scancode argument '%s' should in protocol:scancode format\n"), fname, lineno, p);
				return NULL;
			}

			*scancodestr++ = 0;

			if (!protocol_match(p, &proto)) {
				fprintf(stderr, _("error: %s:%d: protocol '%s' not found\n"), fname, lineno, p);
				return NULL;
			}

			if (!strtoscancode(scancodestr, &scancode)) {
				fprintf(stderr, _("error: %s:%d: invalid scancode '%s'\n"), fname, lineno, scancodestr);
				return NULL;
			}

			if (!protocol_encoder_available(proto)) {
				fprintf(stderr, _("error: %s:%d: no encoder available for `%s'\n"), fname, lineno, protocol_name(proto));
				return NULL;
			}

			if (!protocol_scancode_valid(proto, scancode)) {
				fprintf(stderr, _("error: %s:%d: invalid scancode '%s' for protocol '%s'\n"), fname, lineno, scancodestr, protocol_name(proto));
				return NULL;
			}

			if (len + protocol_max_size(proto) >= LIRCBUF_SIZE) {
				fprintf(stderr, _("error: %s:%d: too much IR for one transmit\n"), fname, lineno);
				return NULL;
			}

			carrier = protocol_carrier(proto);
			if (f->carrier && f->carrier != carrier)
				fprintf(stderr, _("error: %s:%d: carrier already specified\n"), fname, lineno);
			else
				f->carrier = carrier;

			len += protocol_encode(proto, scancode, f->buf + len);
			expect_pulse = false;
			continue;
		}

		int arg = strtoint(p, "");
		if (arg == 0) {
			fprintf(stderr, _("warning: %s:%d: invalid argument '%s'\n"), fname, lineno, p);
			continue;
		}

		p = strtok_r(NULL, whitespace, &saveptr);
		if (p && p[0] != '#' && !(p[0] == '/' && p[1] == '/')) {
			fprintf(stderr, _("warning: %s:%d: '%s' unexpected\n"), fname, lineno, p);
			continue;
		}

		if (strcmp(keyword, "space") == 0) {
			if (expect_pulse) {
				if (len == 0) {
					fprintf(stderr, _("warning: %s:%d: leading space ignored\n"),
						fname, lineno);
				} else {
					f->buf[len-1] += arg;
				}
			} else {
				f->buf[len++] = arg;
			}
			lastspace = lineno;
			expect_pulse = true;
		} else if (strcmp(keyword, "pulse") == 0) {
			if (!expect_pulse)
				f->buf[len-1] += arg;
			else
				f->buf[len++] = arg;
			expect_pulse = false;
		} else if (strcmp(keyword, "carrier") == 0) {
			if (f->carrier && f->carrier != arg) {
				fprintf(stderr, _("warning: %s:%d: carrier already specified\n"), fname, lineno);
			} else {
				f->carrier = arg;
			}
		} else {
			fprintf(stderr, _("warning: %s:%d: unknown keyword '%s' ignored\n"), fname, lineno, keyword);
			continue;
		}

		if (len >= LIRCBUF_SIZE) {
			fprintf(stderr, _("warning: %s:%d: IR cannot exceed %u edges\n"), fname, lineno, LIRCBUF_SIZE);
			break;
		}
	}

	fclose(input);

	if (len == 0) {
		fprintf(stderr, _("%s: no pulses or spaces found\n"), fname);
		free(f);
		return NULL;
	}

	if ((len % 2) == 0) {
		fprintf(stderr, _("warning: %s:%d: trailing space ignored\n"),
							fname, lastspace);
		len--;
	}

	f->len = len;

	return f;
}

static struct file *read_scancode(const char *name)
{
	enum rc_proto proto;
	struct file *f;
	unsigned scancode;
	char *pstr;
	char *p = strchr(name, ':');

	if (!p) {
		fprintf(stderr, _("error: scancode '%s' must be in protocol:scancode format\n"), name);
		return NULL;
	}

	pstr = strndup(name, p - name);

	if (!protocol_match(pstr, &proto)) {
		fprintf(stderr, _("error: protocol '%s' not found\n"), pstr);
		free(pstr);
		return NULL;
	}
	free(pstr);

	if (!strtoscancode(p + 1, &scancode)) {
		fprintf(stderr, _("error: invalid scancode '%s'\n"), p + 1);
		return NULL;
	}

	if (!protocol_scancode_valid(proto, scancode)) {
		fprintf(stderr, _("error: invalid scancode '%s' for protocol '%s'\n"), p + 1, protocol_name(proto));
		return NULL;
	}

	f = malloc(sizeof(*f));
	if (f == NULL) {
		fprintf(stderr, _("Failed to allocate memory\n"));
		return NULL;
	}

	f->is_scancode = true;
	f->scancode = scancode;
	f->protocol = proto;

	return f;
}

static error_t parse_opt(int k, char *arg, struct argp_state *state)
{
	struct arguments *arguments = state->input;
	struct file *s;

	switch (k) {
	case 'f':
		if (arguments->receive || arguments->send)
			argp_error(state, _("features can not be combined with receive or send option"));
		arguments->features = true;
		break;
	// receiving
	case 'r':
		if (arguments->features || arguments->send)
			argp_error(state, _("receive can not be combined with features or send option"));

		arguments->receive = true;
		if (arg) {
			if (arguments->savetofile)
				argp_error(state, _("receive filename already set"));

			arguments->savetofile = arg;
		}
		break;
	case '1':
		arguments->oneshot = true;
		break;
	case 'v':
		arguments->verbose = true;
		break;
	case 'm':
		if (arguments->carrier_reports == 2)
			argp_error(state, _("cannot enable and disable carrier reports"));

		arguments->carrier_reports = 1;
		break;
	case 'M':
		if (arguments->carrier_reports == 1)
			argp_error(state, _("cannot enable and disable carrier reports"));

		arguments->carrier_reports = 2;
		break;
	case 'n':
		if (arguments->wideband)
			argp_error(state, _("cannot use narrowband and wideband receiver at once"));

		arguments->wideband = 2;
		break;
	case 'w':
		if (arguments->wideband)
			argp_error(state, _("cannot use narrowband and wideband receiver at once"));

		arguments->wideband = 1;
		break;
	case 'R': {
		long low, high;
		char *end;

		low = strtol(arg, &end, 10);
		if (end == NULL || end[0] != '-')
			argp_error(state, _("cannot parse carrier range `%s'"), arg);
		high = strtol(end + 1, &end, 10);
		if (end[0] != 0 || low <= 0 || low >= high || high > 1000000)
			argp_error(state, _("cannot parse carrier range `%s'"), arg);

		arguments->carrier_low = low;
		arguments->carrier_high = high;
		break;
	}
	case 't':
		arguments->timeout = strtoint(arg, "µs");
		if (arguments->timeout == 0)
			argp_error(state, _("cannot parse timeout `%s'"), arg);
		break;

	// sending
	case 'd':
		arguments->device = arg;
		break;
	case 'c':
		arguments->carrier = strtoint(arg, "Hz");
		if (arguments->carrier == 0)
			argp_error(state, _("cannot parse carrier `%s'"), arg);
		break;
	case 'e':
		arguments->emitters = parse_emitters(arg);
		if (arguments->emitters == 0)
			argp_error(state, _("cannot parse emitters `%s'"), arg);
		break;
	case 'g':
		arguments->gap = strtoint(arg, "");
		if (arguments->gap == 0)
			argp_error(state, _("cannot parse gap `%s'"), arg);
		break;
	case 'D':
		arguments->duty = strtoint(arg, "%");
		if (arguments->duty == 0 || arguments->duty >= 100)
			argp_error(state, _("invalid duty cycle `%s'"), arg);
		break;
	case 's':
		if (arguments->receive || arguments->features)
			argp_error(state, _("send can not be combined with receive or features option"));
		s = read_file(arguments, arg);
		if (s == NULL)
			exit(EX_DATAERR);

		s->next = NULL;
		if (arguments->send == NULL)
			arguments->send = s;
		else {
			struct file *p = arguments->send;
			while (p->next) p = p->next;
			p->next = s;
		}
		break;
	case 'S':
		if (arguments->receive || arguments->features)
			argp_error(state, _("send can not be combined with receive or features option"));
		s = read_scancode(arg);
		if (s == NULL)
			exit(EX_DATAERR);

		s->next = NULL;
		if (arguments->send == NULL)
			arguments->send = s;
		else {
			struct file *p = arguments->send;
			while (p->next) p = p->next;
			p->next = s;
		}
		break;

	case ARGP_KEY_END:
		if (!arguments->work_to_do)
			argp_usage(state);

		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}

	if (k != '1' && k != 'd' && k != 'v')
		arguments->work_to_do = true;

	return 0;
}

static const struct argp argp = {
	.options = options,
	.parser = parse_opt,
	.args_doc = args_doc,
	.doc = doc
};

static int open_lirc(const char *fname, unsigned *features)
{
	int fd;

	fd = TEMP_FAILURE_RETRY(open(fname, O_RDWR | O_CLOEXEC));
	if (fd == -1) {
		fprintf(stderr, _("%s: cannot open: %m\n"), fname);
		return -1;
	}

	struct stat st;
	int rc = TEMP_FAILURE_RETRY(fstat(fd, &st));
	if (rc) {
		fprintf(stderr, _("%s: cannot stat: %m\n"), fname);
		close(fd);
		return -1;
	}

	if ((st.st_mode & S_IFMT) != S_IFCHR) {
		fprintf(stderr, _("%s: not character device\n"), fname);
		close(fd);
		return -1;
	}

	rc = ioctl(fd, LIRC_GET_FEATURES, features);
	if (rc) {
		fprintf(stderr, _("%s: failed to get lirc features: %m\n"), fname);
		close(fd);
		return -1;
	}

	return fd;
}

static void lirc_set_send_carrier(int fd, const char *devname, unsigned features, unsigned carrier)
{
	if (features & LIRC_CAN_SET_SEND_CARRIER) {
		int rc = ioctl(fd, LIRC_SET_SEND_CARRIER, &carrier);
		if (rc < 0)
			fprintf(stderr, _("warning: %s: failed to set carrier: %m\n"), devname);
		if (rc != 0)
			fprintf(stderr, _("warning: %s: set send carrier returned %d, should return 0\n"), devname, rc);
	} else
		fprintf(stderr, _("warning: %s: does not support setting send carrier\n"), devname);
}

static int lirc_options(struct arguments *args, int fd, unsigned features)
{
	const char *dev = args->device;
	int rc;

	if (args->timeout) {
		if (features & LIRC_CAN_SET_REC_TIMEOUT) {
			rc = ioctl(fd, LIRC_SET_REC_TIMEOUT, &args->timeout);
			if (rc)
				fprintf(stderr, _("%s: failed to set receiving timeout\n"), dev);
		} else
			fprintf(stderr, _("%s: device does not support setting timeout\n"), dev);
	}

	if (args->wideband) {
		unsigned on = args->wideband == 1;
		if (features & LIRC_CAN_USE_WIDEBAND_RECEIVER) {
			rc = ioctl(fd, LIRC_SET_WIDEBAND_RECEIVER, &on);
			if (rc)
				fprintf(stderr, _("%s: failed to set wideband receiver %s\n"), dev, on ? _("on") : _("off"));
		} else
			fprintf(stderr, _("%s: device does not have wideband receiver\n"), dev);
	}

	if (args->carrier_reports) {
		unsigned on = args->carrier_reports == 1;
		if (features & LIRC_CAN_MEASURE_CARRIER) {
			rc = ioctl(fd, LIRC_SET_MEASURE_CARRIER_MODE, &on);
			if (rc)
				fprintf(stderr, _("%s: failed to set carrier reports %s\n"), dev, on ? _("on") : _("off"));
		} else
			fprintf(stderr, _("%s: device cannot measure carrier\n"), dev);
	}

	if (features & LIRC_CAN_REC_MODE2) {
		unsigned on = 1;
		rc = ioctl(fd, LIRC_SET_REC_TIMEOUT_REPORTS, &on);
		if (rc)
			fprintf(stderr, _("%s: failed to enable timeout reports: %m\n"), dev);
	}

	if (args->carrier_low) {
		if (features & LIRC_CAN_SET_REC_CARRIER_RANGE) {
			rc = ioctl(fd, LIRC_SET_REC_CARRIER_RANGE, &args->carrier_low);
			if (rc)
				fprintf(stderr, _("%s: failed to set low carrier range: %m\n"), dev);
			rc = ioctl(fd, LIRC_SET_REC_CARRIER, &args->carrier_high);
			if (rc)
				fprintf(stderr, _("%s: failed to set high carrier range: %m\n"), dev);
		} else
			fprintf(stderr, _("%s: device does not support setting receiver carrier range\n"), dev);
	}

	if (args->carrier)
		lirc_set_send_carrier(fd, dev, features, args->carrier);

	if (args->duty) {
		if (features & LIRC_CAN_SET_SEND_DUTY_CYCLE) {
			rc = ioctl(fd, LIRC_SET_SEND_DUTY_CYCLE, &args->duty);
			if (rc)
				fprintf(stderr, _("warning: %s: failed to set duty cycle: %m\n"), dev);
		} else
			fprintf(stderr, _("warning: %s: does not support setting send duty cycle\n"), dev);
	}

	if (args->emitters) {
		if (features & LIRC_CAN_SET_TRANSMITTER_MASK) {
			rc = ioctl(fd, LIRC_SET_TRANSMITTER_MASK, &args->emitters);
			if (rc > 0)
				fprintf(stderr, _("warning: %s: failed to set send transmitters: only %d available\n"), dev, rc);
			else if (rc < 0)
				fprintf(stderr, _("warning: %s: failed to set send transmitters: %m\n"), dev);
		} else
			fprintf(stderr, _("warning: %s: does not support setting send transmitters\n"), dev);
	}


	return 0;
}

static void lirc_features(struct arguments *args, int fd, unsigned features)
{
	const char *dev = args->device;
	unsigned resolution = 0;
	int rc;

	if (features & LIRC_CAN_GET_REC_RESOLUTION) {
		rc = ioctl(fd, LIRC_GET_REC_RESOLUTION, &resolution);
		if (rc == 0 && resolution == 0)
			fprintf(stderr, _("warning: %s: device returned resolution of 0\n"), dev);
		else if (rc)
			fprintf(stderr, _("warning: %s: unexpected error while retrieving resolution: %m\n"), dev);
	}

	bool can_receive = false;
	printf(_("Receive features %s:\n"), dev);
	if (features & LIRC_CAN_REC_SCANCODE) {
		printf(_(" - Device can receive scancodes\n"));
		can_receive = true;
	}
	if (features & LIRC_CAN_REC_MODE2) {
		printf(_(" - Device can receive raw IR\n"));
		if (resolution)
			printf(_(" - Resolution %u microseconds\n"), resolution);
		if (features & LIRC_CAN_SET_REC_CARRIER)
			printf(_(" - Set receive carrier\n"));
		if (features & LIRC_CAN_USE_WIDEBAND_RECEIVER)
			printf(_(" - Use wideband receiver\n"));
		if (features & LIRC_CAN_MEASURE_CARRIER)
			printf(_(" - Can measure carrier\n"));
		if (features & LIRC_CAN_SET_REC_TIMEOUT) {
			unsigned min_timeout, max_timeout;
			int rc = ioctl(fd, LIRC_GET_MIN_TIMEOUT, &min_timeout);
			if (rc) {
				fprintf(stderr, _("warning: %s: device supports setting receiving timeout but LIRC_GET_MIN_TIMEOUT returns: %m\n"), dev);
				min_timeout = 0;
			} else if (min_timeout == 0)
				fprintf(stderr, _("warning: %s: device supports setting receiving timeout but min timeout is 0\n"), dev);
			rc = ioctl(fd, LIRC_GET_MAX_TIMEOUT, &max_timeout);
			if (rc) {
				fprintf(stderr, _("warning: %s: device supports setting receiving timeout but LIRC_GET_MAX_TIMEOUT returns: %m\n"), dev);
				max_timeout = 0;
			} else if (max_timeout == 0) {
				fprintf(stderr, _("warning: %s: device supports setting receiving timeout but max timeout is 0\n"), dev);
			}

			if (min_timeout || max_timeout)
				printf(_(" - Can set receiving timeout min:%u microseconds max:%u microseconds\n"), min_timeout, max_timeout);
		}
		can_receive = true;
	}
	if (features & LIRC_CAN_REC_LIRCCODE) {
		printf(_(" - Device can receive using device dependent LIRCCODE mode (not supported)\n"));
		can_receive = true;
	}

	if (!can_receive)
		printf(_(" - Device cannot receive\n"));

	bool can_send = false;
	printf(_("Send features %s:\n"), dev);
	if (features & LIRC_CAN_SEND_SCANCODE) {
		printf(_(" - Device can send scancodes\n"));
		can_send = true;
	}
	if (features & LIRC_CAN_SEND_PULSE) {
		printf(_(" - Device can send raw IR\n"));
		if (features & LIRC_CAN_SET_SEND_CARRIER)
			printf(_(" - Set carrier\n"));
		if (features & LIRC_CAN_SET_SEND_DUTY_CYCLE)
			printf(_(" - Set duty cycle\n"));
		if (features & LIRC_CAN_SET_TRANSMITTER_MASK) {
			unsigned mask = ~0;
			rc = ioctl(fd, LIRC_SET_TRANSMITTER_MASK, &mask);
			if (rc == 0)
				fprintf(stderr, _("warning: %s: device supports setting transmitter mask but returns 0 as number of transmitters\n"), dev);
			else if (rc < 0)
				fprintf(stderr, _("warning: %s: device supports setting transmitter mask but returns: %m\n"), dev);
			else
				printf(_(" - Set transmitter (%d available)\n"), rc);
		}
		can_send = true;
	}
	if (features & LIRC_CAN_SEND_LIRCCODE) {
		printf(_(" - Device can send using device dependent LIRCCODE mode (not supported)\n"));
		can_send = true;
	}

	if (!can_send)
		printf(_(" - Device cannot send\n"));
}

static int lirc_send(struct arguments *args, int fd, unsigned features, struct file *f)
{
	const char *dev = args->device;
	int rc, mode;
	ssize_t ret;

	if (f->is_scancode && (features & LIRC_CAN_SEND_SCANCODE)) {
		mode = LIRC_MODE_SCANCODE;
		rc = ioctl(fd, LIRC_SET_SEND_MODE, &mode);
		if (rc == 0) {
			struct lirc_scancode sc = {
				.scancode = f->scancode,
				.rc_proto = f->protocol,
				.flags = 0
			};
			ret = TEMP_FAILURE_RETRY(write(fd, &sc, sizeof sc));
			if (ret > 0)
				return 0;
		}
	}

	if (!(features & LIRC_CAN_SEND_PULSE)) {
		fprintf(stderr, _("%s: device cannot send raw ir\n"), dev);
		return EX_UNAVAILABLE;
	}

	mode = LIRC_MODE_PULSE;
	rc = ioctl(fd, LIRC_SET_SEND_MODE, &mode);
	if (rc) {
		fprintf(stderr, _("%s: cannot set send mode\n"), dev);
		return EX_UNAVAILABLE;
	}

	if (f->is_scancode) {
		// encode scancode
		enum rc_proto proto = f->protocol;
		if (!protocol_encoder_available(proto)) {
			fprintf(stderr, _("%s: no encoder available for `%s'\n"),
				dev, protocol_name(proto));
			return EX_UNAVAILABLE;
		}
		f->len = protocol_encode(f->protocol, f->scancode, f->buf);
		f->carrier = protocol_carrier(proto);
	}

	if (args->carrier && f->carrier)
		fprintf(stderr, _("warning: %s: carrier specified but overwritten on command line\n"), f->fname);
	else if (f->carrier && args->carrier == 0)
		lirc_set_send_carrier(fd, dev, features, f->carrier);

	size_t size = f->len * sizeof(unsigned);
	if (args->verbose) {
		int i;
		printf("Sending:\n");
		for (i=0; i<f->len; i++)
			printf("%s %u\n", i & 1 ? "space" : "pulse", f->buf[i]);
	}
	ret = TEMP_FAILURE_RETRY(write(fd, f->buf, size));
	if (ret < 0) {
		fprintf(stderr, _("%s: failed to send: %m\n"), dev);
		return EX_IOERR;
	}

	if (size < ret) {
		fprintf(stderr, _("warning: %s: sent %zd out %zd edges\n"),
				dev,
				ret / sizeof(unsigned),
				size / sizeof(unsigned));
		return EX_IOERR;
	}
	if (args->verbose)
		printf("Successfully sent\n");

	return 0;
}

int lirc_receive(struct arguments *args, int fd, unsigned features)
{
	char *dev = args->device;
	FILE *out = stdout;
	int rc = EX_IOERR;
	int mode = LIRC_MODE_MODE2;

	if (!(features & LIRC_CAN_REC_MODE2)) {
		fprintf(stderr, _("%s: device cannot receive raw ir\n"), dev);
		return EX_UNAVAILABLE;
	}

	// kernel v4.8 and v4.9 return ENOTTY
	if (ioctl(fd, LIRC_SET_REC_MODE, &mode) && errno != ENOTTY) {
		fprintf(stderr, _("%s: failed to set receive mode: %m\n"), dev);
		return EX_IOERR;
	}

	if (args->savetofile) {
		out = fopen(args->savetofile, "w");
		if (!out) {
			fprintf(stderr, _("%s: failed to open for writing: %m\n"), args->savetofile);
			return EX_CANTCREAT;
		}
	}
	unsigned buf[LIRCBUF_SIZE];

	bool keep_reading = true;
	bool leading_space = true;

	while (keep_reading) {
		ssize_t ret = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
		if (ret < 0) {
			fprintf(stderr, _("%s: failed read: %m\n"), dev);
			goto err;
		}

		if (ret == 0 || ret % sizeof(unsigned)) {
			fprintf(stderr, _("%s: read returned %zd bytes\n"),
								dev, ret);
			goto err;
		}

		for (int i=0; i<ret / sizeof(unsigned); i++) {
			unsigned val = buf[i] & LIRC_VALUE_MASK;
			unsigned msg = buf[i] & LIRC_MODE2_MASK;

			// FIXME: the kernel often send us a space after
			// the IR receiver comes out of idle mode. This
			// is meaningless, maybe fix the kernel?
			if (leading_space && msg == LIRC_MODE2_SPACE)
				continue;
			else
				leading_space = false;

			if (args->oneshot &&
				(msg == LIRC_MODE2_TIMEOUT ||
				(msg == LIRC_MODE2_SPACE && val > 19000))) {
				keep_reading = false;
				break;
			}

			switch (msg) {
			case LIRC_MODE2_TIMEOUT:
				fprintf(out, "timeout %u\n", val);
				leading_space = true;
				break;
			case LIRC_MODE2_PULSE:
				fprintf(out, "pulse %u\n", val);
				break;
			case LIRC_MODE2_SPACE:
				fprintf(out, "space %u\n", val);
				break;
			case LIRC_MODE2_FREQUENCY:
				fprintf(out, "carrier %u\n", val);
				break;
			}

			fflush(out);
		}
	}

	rc = 0;
err:
	if (args->savetofile)
		fclose(out);

	return rc;
}

int main(int argc, char *argv[])
{
	struct arguments args = { .gap = IR_DEFAULT_TIMEOUT };

#ifdef ENABLE_NLS
        setlocale (LC_ALL, "");
        bindtextdomain (PACKAGE, LOCALEDIR);
        textdomain (PACKAGE);
#endif

	argp_parse(&argp, argc, argv, 0, 0, &args);

	if (args.device == NULL)
		args.device = "/dev/lirc0";

	int rc, fd;
	unsigned features;

	fd = open_lirc(args.device, &features);
	if (fd < 0)
		exit(EX_NOINPUT);

	rc = lirc_options(&args, fd, features);
	if (rc)
		exit(EX_IOERR);

	struct file *s = args.send;
	while (s) {
		struct file *next = s->next;
		if (s != args.send)
			usleep(args.gap);

		rc = lirc_send(&args, fd, features, s);
		if (rc) {
			close(fd);
			exit(rc);
		}

		free(s);
		s = next;
	}

	if (args.receive) {
		rc = lirc_receive(&args, fd, features);
		if (rc) {
			close(fd);
			exit(rc);
		}
	}

	if (args.features)
		lirc_features(&args, fd, features);

	close(fd);

	return 0;
}
