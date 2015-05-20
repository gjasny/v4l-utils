#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "s-record.h"


#define S0  0x5330
#define S1  0x5331
#define S2  0x5332
#define S3  0x5333


static void show_performance(struct timeval *start, struct timeval *end, int size)
{
	double ssec = start->tv_sec + start->tv_usec / 1000000.0;
	double esec = end->tv_sec + end->tv_usec / 1000000.0;
	double bytes = (double)size;
	double bsec = bytes / (esec - ssec);

	printf("Time used: %.2lf seconds ==> ", esec - ssec);
	if (bsec / 1000000 > 1.0)
		printf("%.3lf MB/s\n", bsec/1000000);
	else if (bsec / 1000 > 1.0)
		printf("%.3lf kB/s\n", bsec/1000);
	else
		printf("%.3lf B/s\n", bsec);
}

static unsigned char digit_to_val(char *digit)
{
	if (*digit <= 0x39)
		return *digit - 0x30;
	else if (*digit <= 70)
		return *digit - 55;

	return *digit - 87;
}

static unsigned char char_to_val(char *str)
{
	return digit_to_val(&str[0]) << 4 | digit_to_val(&str[1]);
}

static void str_to_char(char *str, unsigned char *buf, int length)
{
	int i;

	for (i = 0;  i < length/2;  i++, str += 2, buf++)
		*buf = (unsigned char)char_to_val(str);
}

int s_record_to_bin(char *file_buffer, unsigned char *flash_buffer,
		    int file_size, int flash_size)
{
	unsigned char *flash = flash_buffer;
	char *sline = file_buffer;
	struct timeval start, end;
	int left_to_convert = file_size;
	int bin_size = 0;
	int newline = 0;
	char *is_dos;

	/* Check for dos format CR/LN */
	is_dos = strstr(sline + 2, "S") - 2;
	if (*is_dos == 0x0d)
		newline = 2;
	else
		newline = 1;

	printf("Convert %u bytes of S-record data to binary flash format\n", file_size);
	gettimeofday(&start, NULL);
	while (left_to_convert) {
		unsigned short stype = (sline[0] << 8) | sline[1];
		unsigned char num_of_char = 2 + char_to_val(&sline[2]);
		int sline_length = 2*num_of_char + newline;
		int payload_size;
		char *next = sline + sline_length;
		char *data;
		unsigned int offset;

		switch (stype) {
		case S0:
			data = sline + 8;
			payload_size = sline_length - 11;
			goto next_line;
		case S3:
			data = sline + 12;
			payload_size = sline_length - 14 - newline;
			offset = (char_to_val(sline + 4) << 24 |
				  char_to_val(sline + 6) << 16 |
				  char_to_val(sline + 8) << 8 |
				  char_to_val(sline + 10));
			break;
		default:
			printf("%c%c line is not supported\n",
			       char_to_val(&sline[0]), char_to_val(&sline[2]));
			return -1;
		}

		if (offset + payload_size > flash_size) {
			printf("S-record try to write outside flash area!\n");
			return -1;
		}

		/* Copy Sx data to binary buffer */
		str_to_char(data, &flash[offset], payload_size);

next_line:
		bin_size += payload_size / 2;
		sline = next;
		left_to_convert -= sline_length;
	}

	gettimeofday(&end, NULL);
	show_performance(&start, &end, file_size);

	return bin_size;
}
