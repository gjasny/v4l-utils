/*
   Xceive XC2028/3028 tuner module firmware manipulation tool

   Copyright (C) 2007 Michel Ludwig <michel.ludwig@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation version 2

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "standards.h"

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <asm/byteorder.h>
#include <asm/types.h>

#define MAX(a,b) ((a) >= (b) ? (a) : (b))

struct vector {
	unsigned char* data;
	unsigned int size;
};

static struct vector* alloc_vector(unsigned int size) {
	struct vector *v = malloc(sizeof(*v));
	v->data = malloc(size);
	v->size = size;
	return v;
}

static void free_vector(struct vector* v) {
	free(v->data);
	free(v);
}

static void enlarge_vector(struct vector* v, unsigned int new_size) {
	unsigned char *n_data;
	unsigned int old_size = v->size;

	v->size = MAX(v->size, new_size);
	n_data = malloc(v->size);
	memcpy(n_data, v->data, old_size);
	free(v->data);
	v->data = n_data;
}

static void copy_vector(struct vector *v, unsigned int i,
		 unsigned char* ptr, unsigned int len) {
	if(i + len > v->size) {
		enlarge_vector(v, MAX(2 * v->size, i + len));
	}
	memcpy(v->data + i, ptr, len);
}

static void write_vector8(struct vector *v, unsigned int i, __u8 value) {
	__u8 buf[1];

	buf[0] = value;
	copy_vector(v, i, buf, 1);
}

static void write_vector16(struct vector *v, unsigned int i, __u16 value) {
	__u8 buf[2];

	buf[0] = value & 0xff;
	buf[1] = value >> 8;
	copy_vector(v, i, buf, 2);
}

static const char reset_tuner_str[] = "RESET_TUNER";
static const char reset_clk_str[] = "RESET_CLK";

void create_standard_data(char* filename, unsigned char** data, unsigned int *r_len) {
	FILE *file;
	char* line = NULL;
	ssize_t r = 0;
	size_t len = 0;
	struct vector *v = alloc_vector(1);
	unsigned int v_i = 0;

	if (!(file = fopen(filename, "r"))) {
		perror("Cannot open the firmware standard file.\n");
		*data = NULL;
	}

	while ((r = getline(&line, &len, file)) != -1) {
		unsigned int i = 0;
		unsigned int val, count = 0;
		unsigned int values[len];

		printf("read line \"%s\"\n", line);

		if(len >= 9 && memcmp(reset_clk_str, line, strlen(reset_clk_str) - 1) == 0) {
			printf("adding RESET_CLK\n");
			write_vector16(v, v_i, (__u16) 0xff00);
			v_i += 2;
			continue;
		}
		else if(len >= 11 && memcmp(reset_tuner_str, line, strlen(reset_tuner_str) - 1) == 0) {
			printf("adding RESET_TUNER\n");
			write_vector16(v, v_i, (__u16) 0x0000);
			v_i += 2;
			continue;
		}

		while(i < len && sscanf(line + i*sizeof(char), "%2x", &val) == 1) {
			printf("%2x ", val);
			values[count] = val;
			++count;
			i += 2;
			while(line[i] == ' ') {
				++i;
			}
		}

		write_vector16(v, v_i, __cpu_to_le16((__u16) count));
		v_i += 2;


		for(i = 0; i < count; ++i) {
			write_vector8(v, v_i, (__u8) values[i]);
			++v_i;
		}

		printf("\n");
	}
	write_vector16(v, v_i, 0xffff);
	v_i += 2;

	free(line);
	fclose(file);
	*data = malloc(v_i);
	memcpy(*data, v->data, v_i);
	free_vector(v);
	*r_len = v_i;
}

