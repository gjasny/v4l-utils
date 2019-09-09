/* SPDX-License-Identifier: GPL-2.0 */

// Read a toml keymap or plain text keymap (ir-keytable 1.14 or earlier
// format).

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include "keymap.h"
#include "toml.h"

#ifdef ENABLE_NLS
# define _(string) gettext(string)
# include "gettext.h"
# include <locale.h>
# include <langinfo.h>
# include <iconv.h>
#else
# define _(string) string
#endif

void free_keymap(struct keymap *map)
{
	struct scancode_entry *se;
	struct raw_entry *re;
	struct protocol_param *param;
	struct keymap *next;

	while (map) {
		re = map->raw;

		while (re) {
			struct raw_entry *next = re->next;
			free(re->keycode);
			free(re);
			re = next;
		}

		se = map->scancode;

		while (se) {
			struct scancode_entry *next = se->next;
			free(se->keycode);
			free(se);
			se = next;
		}

		param = map->param;

		while (param) {
			struct protocol_param *next = param->next;
			free(param->name);
			free(param);
			param = next;
		}

		next = map->next;

		free(map->name);
		free(map->protocol);
		free(map->variant);
		free(map);

		map = next;
	}
}

static error_t parse_plain_keymap(char *fname, struct keymap **keymap, bool verbose)
{
	FILE *fin;
	int line = 0;
	char *scancode, *keycode, s[2048];
	struct scancode_entry *se;
	struct keymap *map;

	map = calloc(1, sizeof(*map));
	if (!map) {
		perror("parse_keymap");
		return ENOMEM;
	}

	if (verbose)
		fprintf(stderr, _("Parsing %s keycode file as plain text\n"), fname);

	fin = fopen(fname, "r");
	if (!fin) {
		fprintf(stderr, _("%s: error: cannot open: %m\n"), fname);
		return EINVAL;
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
					map->name = strdup(p);
				} else if (!strcmp(p, "type")) {
					p = strtok(NULL, " ,\n");
					if (!p)
						goto err_einval;

					while (p) {
						if (!map->protocol) {
							map->protocol = strdup(p);
						} else {
							struct keymap *k;

							k = calloc(1, sizeof(*k));
							k->protocol = strdup(p);
							k->next = map->next;
							map->next = k;
						}

						p = strtok(NULL, " ,\n");
					}
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

		se = calloc(1, sizeof(*se));
		if (!se) {
			free_keymap(map);
			perror("parse_keymap");
			fclose(fin);
			return ENOMEM;
		}

		se->scancode = strtoul(scancode, NULL, 0);
		se->keycode = strdup(keycode);
		se->next = map->scancode;
		map->scancode = se;
	}
	fclose(fin);

	*keymap = map;

	return 0;

err_einval:
	free_keymap(map);
	fprintf(stderr, _("Invalid parameter on line %d of %s\n"),
		line, fname);
	return EINVAL;
}

static error_t parse_rawir_string(const char *fname, char *str, struct raw_entry **entry)
{
	struct raw_entry *re;
	const char sep[] = "\n\r\t\v ,";
	const char *p;
	char *copy;
	int i, size = 0;

	// First do one scan so that we know the length
	copy = strdup(str);
	p = strtok(copy, sep);
	while (p) {
		size++;
		p = strtok(NULL, sep);
	}

	re = calloc(1, sizeof(*re) + sizeof(re->raw[0]) * size);
	if (!re) {
		fprintf(stderr, _("Failed to allocate memory"));
		free(copy);
		return EINVAL;
	}

	// Second scan to extract values and validate
	strcpy(copy, str);
	p = strtok(copy, sep);
	i = 0;
	while (p) {
		long int value;
		char *q;

		value = strtoll(p, &q, 10);
		if (*q || value == 0 || errno) {
			fprintf(stderr, _("%s: incorrect raw value `%s'"),
				fname, p);
			free(copy);
			free(re);
			return EINVAL;
		}

		if (value < 0) {
			if (i % 2)
				value = -value;
			else {
				fprintf(stderr, _("%s: negative raw value `%ld` at position %d only allowed for gaps/spaces"),
					fname, value, i);
				free(copy);
				free(re);
				return EINVAL;
			}
		}

		if (value <= 0 || value > USHRT_MAX) {
			fprintf(stderr, _("%s: raw value %ld out of range"),
				fname, value);
			free(copy);
			free(re);
			return EINVAL;
		}

		re->raw[i++] = value;

		p = strtok(NULL, sep);
	}

	free(copy);
	re->raw_length = size;

	*entry = re;

	return 0;
}

static error_t parse_toml_raw_part(const char *fname, struct toml_array_t *raw, struct keymap *map, bool verbose)
{
	char *keycode, *raw_str;
	struct toml_table_t *t;
	struct raw_entry *re;
	const char *traw;
	int ind = 0;

	while ((t = toml_table_at(raw, ind++)) != NULL) {
		traw = toml_raw_in(t, "keycode");
		if (!traw) {
			fprintf(stderr, _("%s: invalid keycode for raw entry %d\n"),
				fname, ind);
			return EINVAL;
		}

		if (toml_rtos(traw, &keycode)) {
			fprintf(stderr, _("%s: bad value `%s' for keycode\n"),
				fname, traw);
			return EINVAL;
		}

		traw = toml_raw_in(t, "raw");
		if (!traw) {
			fprintf(stderr, _("%s: missing raw value for entry %d\n"),
				fname, ind);
			free(keycode);
			return EINVAL;
		}

		if (toml_rtos(traw, &raw_str)) {
			fprintf(stderr, _("%s: bad value `%s' for keycode\n"),
				fname, traw);
			free(keycode);
			return EINVAL;
		}

		if (parse_rawir_string(fname, raw_str, &re)) {
			free(keycode);
			free(raw_str);
			return EINVAL;
		}

		free(raw_str);
		re->keycode = keycode;
		re->next = map->raw;
		map->raw = re;
	}

	return 0;
}

static error_t parse_toml_protocol(const char *fname, struct toml_table_t *proot, struct keymap **keymap, bool verbose)
{
	struct toml_table_t *scancodes;
	struct toml_array_t *rawarray;
	const char *raw, *key;
	bool have_raw_protocol = false;
	struct keymap *map;
	char *p;
	int i = 0;

	map = calloc(1, sizeof(*map));
	if (!map) {
		perror("parse_toml_protocol");
		return ENOMEM;
	}
	*keymap = map;

	raw = toml_raw_in(proot, "protocol");
	if (!raw) {
		fprintf(stderr, _("%s: protocol missing\n"), fname);
		free_keymap(map);
		return EINVAL;
	}

	if (toml_rtos(raw, &p)) {
		fprintf(stderr, _("%s: bad value `%s' for protocol\n"), fname, raw);
		free_keymap(map);
		return EINVAL;
	}

	map->protocol = p;
	if (!strcmp(p, "raw"))
		have_raw_protocol = true;

	raw = toml_raw_in(proot, "variant");
	if (raw) {
		if (toml_rtos(raw, &p)) {
			fprintf(stderr, _("%s: bad value `%s' for variant\n"), fname, raw);
			free_keymap(map);
			return EINVAL;
		}

		map->variant = p;
	}

	raw = toml_raw_in(proot, "name");
	if (raw) {
		if (toml_rtos(raw, &p)) {
			fprintf(stderr, _("%s: bad value `%s' for name\n"), fname, raw);
			free_keymap(map);
			return EINVAL;
		}

		map->name = p;
	}

	rawarray = toml_array_in(proot, "raw");
	if (rawarray) {
		if (toml_raw_in(proot, "scancodes")) {
			fprintf(stderr, _("Cannot have both [raw] and [scancode] sections"));
			free_keymap(map);
			return EINVAL;
		}
		if (!have_raw_protocol) {
			fprintf(stderr, _("Keymap with raw entries must have raw protocol"));
			free_keymap(map);
			return EINVAL;
		}
		error_t err = parse_toml_raw_part(fname, rawarray, map, verbose);
		if (err != 0) {
			free_keymap(map);
			return err;
		}
	} else if (have_raw_protocol) {
		fprintf(stderr, _("Keymap with raw protocol must have raw entries"));
		free_keymap(map);
		return EINVAL;
	}

	scancodes = toml_table_in(proot, "scancodes");
	if (!scancodes) {
		if (verbose)
			fprintf(stderr, _("%s: no [protocols.scancodes] section\n"), fname);
		return 0;
	}

	for (i = 0; (key = toml_key_in(proot, i)) != NULL; i++) {
		int64_t value;

		raw = toml_raw_in(proot, key);
		if (!toml_rtoi(raw, &value)) {
			struct protocol_param *param;

			param = malloc(sizeof(*param));
			param->name = strdup(key);
			param->value = value;
			param->next = map->param;
			map->param = param;
			if (verbose)
				fprintf(stderr, _("%s: protocol parameter %s=%ld\n"), fname, param->name, param->value);
		}
	}

	struct scancode_entry **next = &map->scancode;
	i = 0;

	for (;;) {
		struct scancode_entry *se;
		const char *scancode;
		char *keycode;

		scancode = toml_key_in(scancodes, i++);
		if (!scancode)
			break;

		raw = toml_raw_in(scancodes, scancode);
		if (!raw) {
			fprintf(stderr, _("%s: invalid value `%s'\n"), fname, scancode);
			free_keymap(map);
			return EINVAL;
		}

		if (toml_rtos(raw, &keycode)) {
			fprintf(stderr, _("%s: bad value `%s' for keycode\n"),
				fname, keycode);
			free_keymap(map);
			return EINVAL;
		}

		se = calloc(1, sizeof(*se));
		if (!se) {
			perror("parse_keymap");
			free(keycode);
			free_keymap(map);
			return ENOMEM;
		}

		se->scancode = strtoul(scancode, NULL, 0);
		se->keycode = keycode;
		*next = se;
		next = &se->next;
	}

	return 0;
}

static error_t parse_toml_keymap(char *fname, struct keymap **keymap, bool verbose)
{
	struct toml_table_t *root, *proot;
	struct toml_array_t *arr;
	int ret, i = 0;
	char buf[200];
	FILE *fin;

	if (verbose)
		fprintf(stderr, _("Parsing %s keycode file as toml\n"), fname);

	fin = fopen(fname, "r");
	if (!fin) {
		fprintf(stderr, _("%s: error: cannot open: %m\n"), fname);
		return EINVAL;
	}

	root = toml_parse_file(fin, buf, sizeof(buf));
	fclose(fin);
	if (!root) {
		fprintf(stderr, _("%s: error: %s\n"), fname, buf);
		return EINVAL;
	}

	arr = toml_array_in(root, "protocols");
	if (!arr) {
		fprintf(stderr, _("%s: missing [protocols] section\n"), fname);
		goto out;
	}

	struct keymap *map = NULL;

	for (;;) {
		struct keymap *cur_map;

		proot = toml_table_at(arr, i);
		if (!proot)
			break;

		ret = parse_toml_protocol(fname, proot, &cur_map, verbose);
		if (ret)
			goto out;

		if (!map) {
			map = cur_map;
		} else {
			cur_map->next = map->next;
			map->next = cur_map;
		}
		i++;
	}

	if (i == 0) {
		fprintf(stderr, _("%s: no protocols found\n"), fname);
		goto out;
	}

	toml_free(root);
	*keymap = map;
	return 0;
out:
	toml_free(root);
	return EINVAL;
}

error_t parse_keymap(char *fname, struct keymap **keymap, bool verbose)
{
	size_t len = strlen(fname);

	if (len >= 5 && strcasecmp(fname + len - 5, ".toml") == 0)
		return parse_toml_keymap(fname, keymap, verbose);
	else
		return parse_plain_keymap(fname, keymap, verbose);
}

int keymap_param(struct keymap *map, const char *name, int fallback)
{
	struct protocol_param *param;

	for (param = map->param; param; param = param->next) {
		if (!strcmp(param->name, name))
			return param->value;
	}

	return fallback;
}
