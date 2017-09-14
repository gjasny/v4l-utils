/*
   Xceive XC2028/3028 tuner module firmware manipulation tool

   Copyright (C) 2007 Michel Ludwig <michel.ludwig@gmail.com>

   Copyright (C) 2007, 2008 Mauro Carvalho Chehab <mchehab@infradead.org>
	- Improve --list command
	- Add --seek command

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
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <asm/byteorder.h>
#include <asm/types.h>

#include "tuner-xc2028-types.h"
#include "linux/videodev2.h"

#include "extract_head.h"
#include "standards.h"

#define LIST_ACTION		(1<<0)
#define ADD_ACTION		(1<<1)
#define DELETE_ACTION		(1<<2)
#define SET_TYPE_ACTION		(1<<3)
#define SET_ID_ACTION		(1<<4)
#define SEEK_FIRM_ACTION	(1<<5)

struct firmware_description {
	__u32 type;
	__u64 id;
	unsigned char *data;
	__u16 int_freq;
	__u32 size;
};

struct firmware {
	char* name;
	struct firmware_description* desc;
	__u16 version;
	__u16 nr_desc;
};

#if 0
static struct firmware_description* alloc_firmware_description(void) {
	struct firmware_description *d = malloc(sizeof(*d));
	d->type = 0;
	d->id = 0;
	d->data = NULL;
	d->size = 0;
	return d;
}

static void free_firmware_description(struct firmware_description *d) {
	free(d->data);
	free(d);
}
#endif

static struct firmware* alloc_firmware(void) {
	struct firmware *f = malloc(sizeof(*f));
	f->name = NULL;
	f->desc = NULL;
	f->nr_desc = 0;
	return f;
}

static void free_firmware(struct firmware *f) {
	free(f->name);
	if(f->desc) {
		unsigned int i = 0;
		for(i = 0; i < f->nr_desc; ++ i) {
			free(f->desc[i].data);
		}
	}
	free(f->desc);
	free(f);
}

static void add_firmware_description(struct firmware *f,
			      struct firmware_description *d) {
	struct firmware_description* new_desc;

	new_desc = malloc((f->nr_desc + 1) * sizeof(*new_desc));
	memcpy(new_desc, f->desc, f->nr_desc * sizeof(*new_desc));
	memcpy(new_desc + f->nr_desc, d, sizeof(*d));
	free(f->desc);
	f->desc = new_desc;
	++f->nr_desc;
}

static void delete_firmware_description(struct firmware *f, __u16 i) {
	struct firmware_description* new_desc;

	if(f->nr_desc == 0 || i >= f->nr_desc) {
		return;
	}

	new_desc = malloc((f->nr_desc - 1) * sizeof(*new_desc));
	memcpy(new_desc, f->desc, i * sizeof(*f->desc));
	memcpy(new_desc + i, f->desc + i + 1, (f->nr_desc - i - 1) * sizeof(*f->desc));
	free(f->desc);
	f->desc = new_desc;
	--f->nr_desc;
}

/* name[32] + version[2] + nr_desc[2] */
#define HEADER_LENGTH (32 + 2 + 2)
/* description header: 4 + 8 + 4.*/
#define DESC_HEADER_LENGTH (4 + 8 + 4)

static int read_firmware(unsigned char* data, off_t size, struct firmware** f_res) {
	char *name = malloc(33);
	unsigned char *p = data;
	struct firmware* f = alloc_firmware();
	unsigned int i;

	if(size < HEADER_LENGTH) {
		printf("Invalid firmware header length.\n");
		free_firmware(f);
		return -1;
	}
	name[32] = 0;
	memcpy(name, data, 32);
	f->name = name;
	p += 32;
	f->version = __le16_to_cpu(*(__u16*)p);
	p += sizeof(f->version);
	f->nr_desc = __le16_to_cpu(*(__u16*)p);
	p += sizeof(f->nr_desc);
	f->desc = malloc(f->nr_desc * sizeof(*(f->desc)));

	for(i = 0; i < f->nr_desc; ++i) {
		if(p + DESC_HEADER_LENGTH > data + size) {
			printf("Invalid description header length.\n");
			free_firmware(f);
			return -1;
		}
		f->desc[i].type = __le32_to_cpu(*(__u32*) p);
		p += sizeof(f->desc[i].type);
		f->desc[i].id = __le64_to_cpu(*(__u64*) p);
		p += sizeof(f->desc[i].id);

		if (f->desc[i].type & HAS_IF) {
			f->desc[i].int_freq = __le16_to_cpu(*(__u16 *) p);
			p += sizeof(f->desc[i].int_freq);
		}

		f->desc[i].size = __le32_to_cpu(*(__u32*) p);
		p += sizeof(f->desc[i].size);

		if(p + f->desc[i].size > data + size) {
			printf("Invalid firmware standard length.\n");
			f->nr_desc = (f->nr_desc == 0) ? 0 : f->nr_desc -1;
			free_firmware(f);
			return -1;
		}

		f->desc[i].data = malloc(f->desc[i].size);
		memcpy(f->desc[i].data, p, f->desc[i].size);

		p += f->desc[i].size;
	}

	*f_res = f;
	return 0;
}

static void write_firmware(struct firmware *f, unsigned char** r_data, off_t *r_size) {
	off_t size;
	unsigned int i = 0;
	unsigned char* data;
	unsigned char* p;

	size = HEADER_LENGTH + f->nr_desc * DESC_HEADER_LENGTH;
	for(i = 0; i < f->nr_desc; ++i) {
		size += f->desc[i].size;
	}

	data = malloc(size);
	p = data;

	memcpy(p, f->name, 32);
	p += 32;

	*(__u16*)p = __cpu_to_le16(f->version);
	p += sizeof(f->version);

	*(__u16*)p = __cpu_to_le16(f->nr_desc);
	p += sizeof(f->nr_desc);

	for(i = 0; i < f->nr_desc; ++i) {
		*(__u32*) p = __cpu_to_le32(f->desc[i].type);
		p += sizeof(f->desc[i].type);

		*(__u64*) p = __cpu_to_le64(f->desc[i].id);
		p += sizeof(f->desc[i].id);

		*(__u32*) p = __cpu_to_le32(f->desc[i].size);
		p += sizeof(f->desc[i].size);

		memcpy(p, f->desc[i].data, f->desc[i].size);
		p += f->desc[i].size;
	}

	*r_data = data;
	*r_size = size;
}

static struct firmware* read_firmware_file(const char* filename) {
	struct stat buf;
	unsigned char *ptr;
	struct firmware *f;
	int fd;

	if(stat(filename, &buf) < 0) {
		perror("Error during stat");
		return NULL;
	}

	fd = open(filename, O_RDONLY);
	if(fd < 0) {
		perror("Error while opening the firmware file");
		return NULL;
	}

	/* allocate firmware buffer*/
	ptr = malloc(buf.st_size);

	if(read(fd, ptr, buf.st_size) < 0) {
		perror("Error while reading the firmware file");
		free(ptr);
		close(fd);
		return NULL;
	}

	if(read_firmware(ptr, buf.st_size, &f) < 0) {
		printf("Invalid firmware file!\n");
		free(ptr);
		close(fd);
		return NULL;
	}

	close(fd);
	free(ptr);
	return f;
}

static void write_firmware_file(const char* filename, struct firmware *f) {
	int fd;
	unsigned char* data;
	off_t size = 0;

	fd = open(filename, O_WRONLY | O_CREAT, 0644);
	if(fd < 0) {
		perror("Error while opening the firmware file");
		return;
	}

	if(ftruncate(fd, 0) < 0) {
		perror("Error while deleting the firmware file");
		close(fd);
		return;
	}

	write_firmware(f, &data, &size);

	if(write(fd, data, size) < 0) {
		perror("Error while writing the firmware file");
		close(fd);
		return;
	}

	free(data);
	close(fd);
}

static void dump_firm_type(FILE *fp, unsigned int type)
{
	if (type & SCODE)
		fprintf(fp, "SCODE FW  ");
	else if (type & BASE)
		fprintf(fp, "BASE FW   ");
	else
		fprintf(fp, "STD FW    ");

	if (type & F8MHZ)
		fprintf(fp, "F8MHZ ");
	if (type & MTS)
		fprintf(fp, "MTS ");
	if (type & D2620)
		fprintf(fp, "D2620 ");
	if (type & D2633)
		fprintf(fp, "D2633 ");
	if (type & DTV6)
		fprintf(fp, "DTV6 ");
	if (type & QAM)
		fprintf(fp, "QAM ");
	if (type & DTV7)
		fprintf(fp, "DTV7 ");
	if (type & DTV78)
		fprintf(fp, "DTV78 ");
	if (type & DTV8)
		fprintf(fp, "DTV8 ");
	if (type & FM)
		fprintf(fp, "FM ");
	if (type & INPUT1)
		fprintf(fp, "INPUT1 ");
	if (type & LCD)
		fprintf(fp, "LCD ");
	if (type & NOGD)
		fprintf(fp, "NOGD ");
	if (type & MONO)
		fprintf(fp, "MONO ");
	if (type & ATSC)
		fprintf(fp, "ATSC ");
	if (type & IF)
		fprintf(fp, "IF ");
	if (type & LG60)
		fprintf(fp, "LG60 ");
	if (type & ATI638)
		fprintf(fp, "ATI638 ");
	if (type & OREN538)
		fprintf(fp, "OREN538 ");
	if (type & OREN36)
		fprintf(fp, "OREN36 ");
	if (type & TOYOTA388)
		fprintf(fp, "TOYOTA388 ");
	if (type & TOYOTA794)
		fprintf(fp, "TOYOTA794 ");
	if (type & DIBCOM52)
		fprintf(fp, "DIBCOM52 ");
	if (type & ZARLINK456)
		fprintf(fp, "ZARLINK456 ");
	if (type & CHINA)
		fprintf(fp, "CHINA ");
	if (type & F6MHZ)
		fprintf(fp, "F6MHZ ");
	if (type & INPUT2)
		fprintf(fp, "INPUT2 ");
	if (type & HAS_IF)
		fprintf(fp, "HAS IF ");
}

static void dump_firm_std(FILE *fp, v4l2_std_id id)
{
	v4l2_std_id old=-1, curr_id;

	/* Dumps video standards */
	while (old!=id) {
		old=id;
		if ( (id & V4L2_STD_PAL) == V4L2_STD_PAL) {
			fprintf (fp, "PAL ");
			curr_id = V4L2_STD_PAL;
		} else if ( (id & V4L2_STD_MN) == V4L2_STD_MN) {
			fprintf (fp, "NTSC PAL/M PAL/N ");
			curr_id = V4L2_STD_PAL;
		} else if ( (id & V4L2_STD_PAL_BG) == V4L2_STD_PAL_BG) {
			fprintf (fp, "PAL/BG ");
			curr_id = V4L2_STD_PAL_BG;
		} else if ( (id & V4L2_STD_PAL_DK) == V4L2_STD_PAL_DK) {
			fprintf (fp, "PAL/DK ");
			curr_id = V4L2_STD_PAL_DK;
		} else if ( (id & V4L2_STD_PAL_B) == V4L2_STD_PAL_B) {
			fprintf (fp, "PAL/B ");
			curr_id = V4L2_STD_PAL_B;
		} else if ( (id & V4L2_STD_PAL_B1) == V4L2_STD_PAL_B1) {
			fprintf (fp, "PAL/B1 ");
			curr_id = V4L2_STD_PAL_B1;
		} else if ( (id & V4L2_STD_PAL_G) == V4L2_STD_PAL_G) {
			fprintf (fp, "PAL/G ");
			curr_id = V4L2_STD_PAL_G;
		} else if ( (id & V4L2_STD_PAL_H) == V4L2_STD_PAL_H) {
			fprintf (fp, "PAL/H ");
			curr_id = V4L2_STD_PAL_H;
		} else if ( (id & V4L2_STD_PAL_I) == V4L2_STD_PAL_I) {
			fprintf (fp, "PAL/I ");
			curr_id = V4L2_STD_PAL_I;
		} else if ( (id & V4L2_STD_PAL_D) == V4L2_STD_PAL_D) {
			fprintf (fp, "PAL/D ");
			curr_id = V4L2_STD_PAL_D;
		} else if ( (id & V4L2_STD_PAL_D1) == V4L2_STD_PAL_D1) {
			fprintf (fp, "PAL/D1 ");
			curr_id = V4L2_STD_PAL_D1;
		} else if ( (id & V4L2_STD_PAL_K) == V4L2_STD_PAL_K) {
			fprintf (fp, "PAL/K ");
			curr_id = V4L2_STD_PAL_K;
		} else if ( (id & V4L2_STD_PAL_M) == V4L2_STD_PAL_M) {
			fprintf (fp, "PAL/M ");
			curr_id = V4L2_STD_PAL_M;
		} else if ( (id & V4L2_STD_PAL_N) == V4L2_STD_PAL_N) {
			fprintf (fp, "PAL/N ");
			curr_id = V4L2_STD_PAL_N;
		} else if ( (id & V4L2_STD_PAL_Nc) == V4L2_STD_PAL_Nc) {
			fprintf (fp, "PAL/Nc ");
			curr_id = V4L2_STD_PAL_Nc;
		} else if ( (id & V4L2_STD_PAL_60) == V4L2_STD_PAL_60) {
			fprintf (fp, "PAL/60 ");
			curr_id = V4L2_STD_PAL_60;
		} else if ( (id & V4L2_STD_NTSC) == V4L2_STD_NTSC) {
			fprintf (fp, "NTSC ");
			curr_id = V4L2_STD_NTSC;
		} else if ( (id & V4L2_STD_NTSC_M) == V4L2_STD_NTSC_M) {
			fprintf (fp, "NTSC/M ");
			curr_id = V4L2_STD_NTSC_M;
		} else if ( (id & V4L2_STD_NTSC_M_JP) == V4L2_STD_NTSC_M_JP) {
			fprintf (fp, "NTSC/M Jp ");
			curr_id = V4L2_STD_NTSC_M_JP;
		} else if ( (id & V4L2_STD_NTSC_443) == V4L2_STD_NTSC_443) {
			fprintf (fp, "NTSC 443 ");
			curr_id = V4L2_STD_NTSC_443;
		} else if ( (id & V4L2_STD_NTSC_M_KR) == V4L2_STD_NTSC_M_KR) {
			fprintf (fp, "NTSC/M Kr ");
			curr_id = V4L2_STD_NTSC_M_KR;
		} else if ( (id & V4L2_STD_SECAM) == V4L2_STD_SECAM) {
			fprintf (fp, "SECAM ");
			curr_id = V4L2_STD_SECAM;
		} else if ( (id & V4L2_STD_SECAM_DK) == V4L2_STD_SECAM_DK) {
			fprintf (fp, "SECAM/DK ");
			curr_id = V4L2_STD_SECAM_DK;
		} else if ( (id & V4L2_STD_SECAM_B) == V4L2_STD_SECAM_B) {
			fprintf (fp, "SECAM/B ");
			curr_id = V4L2_STD_SECAM_B;
		} else if ( (id & V4L2_STD_SECAM_D) == V4L2_STD_SECAM_D) {
			fprintf (fp, "SECAM/D ");
			curr_id = V4L2_STD_SECAM_D;
		} else if ( (id & V4L2_STD_SECAM_G) == V4L2_STD_SECAM_G) {
			fprintf (fp, "SECAM/G ");
			curr_id = V4L2_STD_SECAM_G;
		} else if ( (id & V4L2_STD_SECAM_H) == V4L2_STD_SECAM_H) {
			fprintf (fp, "SECAM/H ");
			curr_id = V4L2_STD_SECAM_H;
		} else if ( (id & V4L2_STD_SECAM_K) == V4L2_STD_SECAM_K) {
			fprintf (fp, "SECAM/K ");
			curr_id = V4L2_STD_SECAM_K;
		} else if ( (id & V4L2_STD_SECAM_K1) == V4L2_STD_SECAM_K1) {
			fprintf (fp, "SECAM/K1 ");
			curr_id = V4L2_STD_SECAM_K1;
		} else if ( (id & V4L2_STD_SECAM_K3) == V4L2_STD_SECAM_K3) {
			fprintf (fp, "SECAM/K3 ");
			curr_id = V4L2_STD_SECAM_K3;
		} else if ( (id & V4L2_STD_SECAM_L) == V4L2_STD_SECAM_L) {
			fprintf (fp, "SECAM/L ");
			curr_id = V4L2_STD_SECAM_L;
		} else if ( (id & V4L2_STD_SECAM_LC) == V4L2_STD_SECAM_LC) {
			fprintf (fp, "SECAM/Lc ");
			curr_id = V4L2_STD_SECAM_LC;
		} else if ( (id & V4L2_STD_A2) == V4L2_STD_A2) {
			fprintf (fp, "A2 ");
			curr_id = V4L2_STD_A2;
		} else if ( (id & V4L2_STD_A2_A) == V4L2_STD_A2_A) {
			fprintf (fp, "A2/A ");
			curr_id = V4L2_STD_A2_A;
		} else if ( (id & V4L2_STD_A2_B) == V4L2_STD_A2_B) {
			fprintf (fp, "A2/B ");
			curr_id = V4L2_STD_A2_B;
		} else if ( (id & V4L2_STD_NICAM) == V4L2_STD_NICAM) {
			fprintf (fp, "NICAM ");
			curr_id = V4L2_STD_NICAM;
		} else if ( (id & V4L2_STD_NICAM_A) == V4L2_STD_NICAM_A) {
			fprintf (fp, "NICAM/A ");
			curr_id = V4L2_STD_NICAM_A;
		} else if ( (id & V4L2_STD_NICAM_B) == V4L2_STD_NICAM_B) {
			fprintf (fp, "NICAM/B ");
			curr_id = V4L2_STD_NICAM_B;
		} else if ( (id & V4L2_STD_AM) == V4L2_STD_AM) {
			fprintf (fp, "AM ");
			curr_id = V4L2_STD_AM;
		} else if ( (id & V4L2_STD_BTSC) == V4L2_STD_BTSC) {
			fprintf (fp, "BTSC ");
			curr_id = V4L2_STD_BTSC;
		} else if ( (id & V4L2_STD_EIAJ) == V4L2_STD_EIAJ) {
			fprintf (fp, "EIAJ ");
			curr_id = V4L2_STD_EIAJ;
		} else {
			curr_id = 0;
			break;
		}
		id &= ~curr_id;
	}
}

static void list_firmware_desc(FILE *fp, struct firmware_description *desc)
{
	fprintf(fp, "type: ");
	dump_firm_type(fp, desc->type);
	fprintf(fp, "(0x%08x), ", desc->type);
	if (desc->type & HAS_IF)
		fprintf(fp, "IF = %.2f MHz ", desc->int_freq/1000.0);
	fprintf(fp, "id: ");
	dump_firm_std(fp, desc->id);
	fprintf(fp, "(%016llx), ", desc->id);
	fprintf(fp, "size: %u\n", desc->size);
}

static void list_firmware(struct firmware *f, unsigned int dump, char *binfile)
{
	unsigned int i = 0;

	printf("firmware name:\t%s\n", f->name);
	printf("version:\t%d.%d (%u)\n", f->version >> 8, f->version & 0xff,
					  f->version);
	printf("standards:\t%u\n", f->nr_desc);
	for(i = 0; i < f->nr_desc; ++i) {
		printf("Firmware %2u, ", i);
		list_firmware_desc(stdout, &f->desc[i]);
		if (dump) {
			printf("\t");
			unsigned j, k = 0;
			for (j = 0; j < f->desc[i].size; j++) {
				printf("%02x", f->desc[i].data[j]);

				k++;
				if (k >= 32) {
					printf("\n\t");
					k = 0;
				} else if (!(k % 2))
					printf(" ");
			}
			printf("\n");
		}
		if (binfile) {
			char name[strlen(binfile)+4], *p;
			p = strrchr(binfile,'.');
			if (p) {
				int n = p - binfile;
				strncpy(name, binfile, n);
				sprintf(name + n, "%03i", i);
				strcat(name, p);
			} else {
				strcpy(name, binfile);
				sprintf(name + strlen(name), "%03i", i);
			}
			FILE *fp;

			fp = fopen(name,"w");
			if (!fp) {
				perror("Opening file to write");
				return;
			}
			fwrite(f->desc[i].data, f->desc[i].size, 1, fp);
			fclose(fp);
		}
	}
}

static void add_standard(struct firmware* f, char* firmware_file, char* standard_file) {
	unsigned char* standard_data;
	unsigned int len;
	struct firmware_description desc;

	create_standard_data(standard_file, &standard_data, &len);
	if(!standard_data) {
		fprintf(stderr, "Couldn't create the firmware standard data.\n");
		return;
	}
	desc.id = 0;
	desc.type = 0;
	desc.size = len;
	desc.data = standard_data;
	add_firmware_description(f, &desc);
	write_firmware_file(firmware_file, f);
}

static void delete_standard(struct firmware* f, char* firmware_file, __u16 i) {
	delete_firmware_description(f, i);
	write_firmware_file(firmware_file, f);
}

static void set_standard_type(struct firmware* f, char* firmware_file, __u16 i, __u32 type) {
	if(i > f->nr_desc) {
		return;
	}
	f->desc[i].type = type;
	write_firmware_file(firmware_file, f);
}

static void set_standard_id(struct firmware* f, char* firmware_file, __u16 i, __u32 id) {
	if(i > f->nr_desc) {
		return;
	}
	f->desc[i].id = id;
	write_firmware_file(firmware_file, f);
}

struct chunk_hunk;

struct chunk_hunk {
	unsigned char *data;
	long pos;
	int size;
	int need_fix_endian;
	int hint_method;
	struct chunk_hunk *next;
};

static int seek_chunks(struct chunk_hunk *fhunk,
		unsigned char *seek, unsigned char *endp,	/* File to seek */
		unsigned char *fdata, unsigned char *endf)	/* Firmware */
{
	unsigned char *fpos, *p, *p2;
	int fsize;
	unsigned char *temp_data;
	struct chunk_hunk *hunk = fhunk;
	/* Method 3 vars */
	static unsigned char *base_start = 0;
	int ini_sig = 8, sig_len = 14, end_sig = 8;

	/* Method 1a: Seek for a complete firmware */
	for (p = seek; p < endp; p++) {
		fpos = p;
		for (p2 = fdata; p2 < endf; p2++, fpos++) {
			if (*fpos != *p2)
				break;
		}
		if (p2 == endf) {
			hunk->data = NULL;
			hunk->pos = p - seek;
			hunk->size = endf - fdata;
			hunk->next = NULL;
			hunk->need_fix_endian = 0;
			hunk->hint_method = 0;
			return 1;
		}
	}

	fsize = endf - fdata;
	temp_data = malloc(fsize);
	memcpy(temp_data, fdata, fsize);

	/* Try again, changing endian */
	for (p2 = temp_data; p2 < temp_data + fsize;) {
		unsigned char c;
		int size = *p2 + (*(p2 + 1) << 8);
		c = *p2;
		*p2 = *(p2 + 1);
		*(p2 + 1) = c;
		p2+=2;
		if ((size > 0) && (size < 0x8000))
			p2 += size;
	}

	/* Method 1b: Seek for a complete firmware with changed endians */
	for (p = seek; p < endp; p++) {
		fpos = p;
		for (p2 = temp_data; p2 < temp_data + fsize; p2++, fpos++) {
			if (*fpos != *p2)
				break;
		}
		if (p2 == temp_data + fsize) {
			hunk->data = NULL;
			hunk->pos = p - seek;
			hunk->size = endf - fdata;
			hunk->next = NULL;
			hunk->need_fix_endian = 1;
			hunk->hint_method = 0;
			return 1;
		}
	}

	free(temp_data);

	/* Method 2: seek for base firmware */
	if (!base_start)
		base_start = seek;

	/* Skip if firmware is not a base firmware */
	if (endf - fdata < 1000)
		goto method3;

	for (p = base_start; p < endp; p++) {
		fpos = p;
		for (p2 = fdata + ini_sig;
		     p2 < fdata + ini_sig + sig_len; p2++,
		     fpos++) {
			if (*fpos != *p2)
				break;
		}
		if (p2 == fdata + ini_sig + sig_len) {
			base_start = p - ini_sig;

			p = memmem (base_start, endp-base_start,
				temp_data + fsize - end_sig, end_sig);

			if (p)
				p = memmem (p + end_sig, endp-base_start,
					temp_data + fsize - end_sig, end_sig);

			if (!p) {
				printf("Found something that looks like a firmware start at %lx\n",
					(long)(base_start - seek));

				base_start += ini_sig + sig_len;
				goto method3;
			}

			p += end_sig;

			printf("Found firmware at %lx, size = %ld\n",
				(long)(base_start - seek),
				(long)(p - base_start));

			hunk->data = NULL;
			hunk->pos = base_start - seek;
			hunk->size = p - base_start;
			hunk->next = NULL;
			hunk->need_fix_endian = 1;
			hunk->hint_method = 3;

			base_start = p;

			return 2;
		}
	}

method3:
#if 0
	/* Method 3: Seek for each firmware chunk */
	p = seek;
	for (p2 = fdata; p2 < endf;) {
		int size = *p2 + (*(p2 + 1) << 8);

		/* Encode size/reset/sleep directly */
		hunk->size = 2;
		hunk->data = malloc(hunk->size);
		memcpy(hunk->data, p2, hunk->size);
		hunk->pos = -1;
		hunk->next = calloc(1, sizeof(hunk));
		hunk->need_fix_endian = 0;
		hunk->hint_method = 0;

		hunk = hunk->next;
		p2 += 2;

		if ((size > 0) && (size < 0x8000)) {
			unsigned char *ep;
			int	found = 0;
			ep = p2 + size;
			///////////////////
			for (; p < endp; p++) {
				unsigned char *p3;
				fpos = p;
				for (p3 = p2; p3 < ep; p3++, fpos++)
					if (*fpos != *p3)
						break;
				if (p3 == ep) {
					found = 1;
					hunk->pos = p - seek;
					hunk->size = size;
					hunk->next = calloc(1, sizeof(hunk));
					hunk->need_fix_endian = 0;
					hunk->hint_method = 0;

					hunk = hunk->next;
					break;
				}
			}
			if (!found) {
				goto not_found;
			}
			p2 += size;
		}
	}
	return 3;
not_found:
#endif
	memset(fhunk, 0, sizeof(struct chunk_hunk));
	printf("Couldn't find firmware\n");
	return 0;

	/* Method 4: Seek for first firmware chunks */
#if 0
seek_next:
	for (p = seek; p < endp; p++) {
		fpos = p;
		for (p2 = fdata; p2 < endf; p2++, fpos++) {
			if (*fpos != *p2)
				break;
		}
		if (p2 > fdata + 3) {
			int i = 0;
			unsigned char *lastp;
			printf("Found %ld equal bytes at %06x:\n",
				p2 - fdata, p - seek);
			fpos = p;
			lastp = fpos;
			for (p2 = fdata; p2 < endf; p2++, fpos++) {
				if (*fpos != *p2)
					break;
				printf("%02x ",*p2);
			}
			for (i=0; p2 < endf && i <5 ; p2++, fpos++, i++) {
				printf("%02x(%02x) ",*p2 , *fpos);
			}
			printf("\n");
			/* Seek for the next chunk */
			fdata = p2;

			if (fdata == endf) {
				printf ("Found all chunks.\n");
				return 4;
			}
		}
	}

	printf ("NOT FOUND: %02x\n", *fdata);
	fdata++;
	goto seek_next;
#endif
}

static void seek_firmware(struct firmware *f, char *seek_file, char *write_file) {
	unsigned int i = 0, j, nfound = 0;
	long size, rd = 0;
	unsigned char *seek, *p, *endp, *endp2;
	/*FIXME: Calculate it, instead of using a hardcode value */
	char *md5 = "0e44dbf63bb0169d57446aec21881ff2";
	FILE *fp;

	struct chunk_hunk hunks[f->nr_desc];
	memset (hunks, 0, sizeof(struct chunk_hunk) * f->nr_desc);

	fp=fopen(seek_file, "r");
	if (!fp) {
		perror("Opening seek file");
		exit(-1);
	}
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	rewind(fp);
	seek = malloc(size);
	p = seek;

	do {
		i = fread(p, 1, 16768, fp);
		if (i > 0) {
			rd += i;
			p += i;
		}
	} while (i > 0);

	fclose(fp);

	if (rd != size) {
		fprintf(stderr, "Error while reading the seek file: "
				"should read %ld, instead of %ld ", size, rd);
		exit (-1);
	}
	endp = p;

	printf("firmware name:\t%s\n", f->name);
	printf("version:\t%d.%d (%u)\n", f->version >> 8, f->version & 0xff,
					  f->version);
	printf("number of standards:\t%u\n", f->nr_desc);
	for(i = 0; i < f->nr_desc; ++i) {
		int found;

		endp2 = f->desc[i].data + f->desc[i].size;

		found = seek_chunks (&hunks[i],
			     seek, endp, f->desc[i].data, endp2);

		if (!found) {
			printf("NOT FOUND: Firmware %d ", i);
			list_firmware_desc(stdout, &f->desc[i]);
		} else {
			nfound++;
			printf("Found with method %d: Firmware %d ", found, i);
			if (found == 2)
				f->desc[i].size = hunks[i].size;
			list_firmware_desc(stdout, &f->desc[i]);
		}
	}
	printf ("Found %d complete firmwares\n", nfound);

	if (!write_file)
		return;

	fp = fopen(write_file, "w");
	if (!fp) {
		perror("Writing firmware file");
		exit(-1);
	}

	fprintf(fp, "%s", extract_header);
	for (i = 0, j = -1; i < f->nr_desc; i++) {
		struct chunk_hunk *hunk = &hunks[i];

		if (!hunk->size)
			continue;
		j++;

		if (hunk->hint_method)
			fprintf(fp, "\n\t#\n\t# Guessed format ");

		fprintf(fp, "\n\t#\n\t# Firmware %d, ", j);
		list_firmware_desc(fp, &f->desc[i]);
		fprintf(fp, "\t#\n\n");

		fprintf(fp, "\twrite_le32(0x%08x);\t\t\t# Type\n",
			f->desc[i].type);
		fprintf(fp, "\twrite_le64(0x%08Lx, 0x%08Lx);\t# ID\n",
			f->desc[i].id>>32, f->desc[i].id & 0xffffffff);
		if (f->desc[i].type & HAS_IF)
			fprintf(fp, "\twrite_le16(%d);\t\t\t# IF\n",
				f->desc[i].int_freq);
		fprintf(fp, "\twrite_le32(%d);\t\t\t# Size\n",
			f->desc[i].size);
		while (hunk) {
			if (hunk->data) {
				int k;
				fprintf(fp, "\tsyswrite(OUTFILE, ");
				for (k = 0; k < hunk->size; k++) {
					fprintf(fp, "chr(%d)", hunk->data[k]);
					if (k < hunk->size-1)
						fprintf(fp,".");
				}
				fprintf(fp,");\n");
			} else {
				if (!hunk->size)
					break;

				if (hunk->need_fix_endian)
					fprintf(fp, write_hunk_fix_endian,
						hunk->pos, hunk->size);
				else
					fprintf(fp, write_hunk,
						hunk->pos, hunk->size);
			}
			hunk = hunk->next;
		}
	}

	fprintf(fp, end_extract, seek_file, md5, "xc3028-v27.fw",
		f->name, f->version, nfound);
}

static void print_usage(void)
{
	printf("firmware-tool usage:\n");
	printf("\t firmware-tool --list [--dump] [--write <bin-file>] <firmware-file>\n");
	printf("\t firmware-tool --add <firmware-dump> <firmware-file>\n");
	printf("\t firmware-tool --delete <index> <firmware-file>\n");
	printf("\t firmware-tool --type <type> --index <i> <firmware-file>\n");
	printf("\t firmware-tool --id <type> --index <i> <firmware-file>\n");
	printf("\t firmware-tool --seek <seek-file> [--write <write-file>] <firmware-file>\n");
}

int main(int argc, char* argv[])
{
	int c;
	int nr_args;
	unsigned int action = 0, dump = 0;
	char* firmware_file, *file = NULL, *nr_str = NULL, *index_str = NULL;
	char *seek_file = NULL, *write_file = NULL;
	struct firmware *f;

	while(1) {
		static struct option long_options[] = {
			{"list",     no_argument,      0, 'l'},
			{"add",     required_argument, 0, 'a'},
			{"delete",  required_argument, 0, 'd'},
			{"type",  required_argument, 0, 't'},
			{"id",  required_argument, 0, 's'},
			{"index",  required_argument, 0, 'i'},
			{"seek", required_argument, 0, 'k'},
			{"write", required_argument , 0, 'w'},
			{"dump", no_argument, 0, 'm'},
			{0, 0, 0, 0}
		};
		int option_index = 0;

		c = getopt_long(argc, argv, "", long_options, &option_index);

		if (c == -1) {
			break;
		}

		switch(c) {
			case 'l':
				puts("list action\n");
				if(action != 0) {
					printf("Please specify only one action.\n");
				}
				action |= LIST_ACTION;
				break;
			case 'm':
				dump = 1;
				break;
			case 'a':
				puts("add action\n");
				if(action != 0) {
					printf("Please specify only one action.\n");
				}
				action |= ADD_ACTION;
				file = optarg;
				break;
			case 'd':
				puts("delete action\n");
				if(action != 0) {
					printf("Please specify only one action.\n");
				}
				action |= DELETE_ACTION;
				nr_str = optarg;
				break;
			case 't':
				puts("set-type action\n");
				if(action != 0) {
					printf("Please specify only one action.\n");
				}
				action |= SET_TYPE_ACTION;
				nr_str = optarg;
				break;
			case 's':
				puts("set-id action\n");
				if(action != 0) {
					printf("Please specify only one action.\n");
				}
				action |= SET_ID_ACTION;
				nr_str = optarg;
				break;
			case 'i':
				index_str = optarg;
				break;
			case 'k':
				puts("seek firmwares\n");
				action = SEEK_FIRM_ACTION;
				seek_file = optarg;
				break;
			case 'w':
				write_file = optarg;
				break;
			default:
				print_usage();
				return 0;
		}
	}

	nr_args = (action == LIST_ACTION) ? 1 : 1;
	if(!(optind + nr_args == argc)) {
		printf("Wrong number of arguments!\n\n");
		print_usage();
		return -1;
	}

	if(!action) {
		printf("Please specify an action!\n\n");
		print_usage();
		return -1;
	}

	firmware_file = argv[optind];

	printf("firmware file name: %s\n", firmware_file);

	f = read_firmware_file(firmware_file);
	if(!f) {
		printf("Couldn't read the firmware file!\n");
		return -1;
	}

	switch(action) {
		case LIST_ACTION:
			list_firmware(f, dump, write_file);
		break;

		case ADD_ACTION:
			add_standard(f, firmware_file, file);
		break;

		case DELETE_ACTION:
			delete_standard(f, firmware_file, strtoul(nr_str, NULL, 10));
		break;

		case SET_TYPE_ACTION:
			set_standard_type(f, firmware_file, strtoul(index_str, NULL, 10), strtoul(nr_str, NULL, 10));
		break;

		case SET_ID_ACTION:
			set_standard_id(f, firmware_file, strtoul(index_str, NULL, 10), strtoul(nr_str, NULL, 10));

		case SEEK_FIRM_ACTION:
			seek_firmware(f, seek_file, write_file);
		break;
	}
	return 0;
}
