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

#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <unistd.h>

#include <asm/byteorder.h>
#include <asm/types.h>

#include "../../../linux/drivers/media/video/tuner-xc2028-types.h"
#include "../../../linux/include/linux/videodev2.h"

#include "standards.h"

#define LIST_ACTION		(1<<0)
#define ADD_ACTION		(1<<1)
#define DELETE_ACTION		(1<<2)
#define SET_TYPE_ACTION		(1<<3)
#define SET_ID_ACTION		(1<<4)

struct firmware_description {
	__u32 type;
	__u64 id;
	unsigned char *data;
	__u32 size;
};

struct firmware {
	char* name;
	struct firmware_description* desc;
	__u16 version;
	__u16 nr_desc;
};

struct firmware_description* alloc_firmware_description(void) {
	struct firmware_description *d = malloc(sizeof(*d));
	d->type = 0;
	d->id = 0;
	d->data = NULL;
	d->size = 0;
	return d;
}

void free_firmware_description(struct firmware_description *d) {
	free(d->data);
	free(d);
}

struct firmware* alloc_firmware(void) {
	struct firmware *f = malloc(sizeof(*f));
	f->name = NULL;
	f->desc = NULL;
	f->nr_desc = 0;
	return f;
}

void free_firmware(struct firmware *f) {
	free(f->name);
	free(f->desc);
	if(f->desc) {
		unsigned int i = 0;
		for(i = 0; i < f->nr_desc; ++ i) {
			free(f->desc[i].data);
		}
	}
	free(f);
}

void add_firmware_description(struct firmware *f,
			      struct firmware_description *d) {
	struct firmware_description* new_desc;

	new_desc = malloc((f->nr_desc + 1) * sizeof(*new_desc));
	memcpy(new_desc, f->desc, f->nr_desc * sizeof(*new_desc));
	memcpy(new_desc + f->nr_desc, d, sizeof(*d));
	free(f->desc);
	f->desc = new_desc;
	++f->nr_desc;
}

void delete_firmware_description(struct firmware *f, __u16 i) {
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

int read_firmware(unsigned char* data, off_t size, struct firmware** f_res) {
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

void write_firmware(struct firmware *f, unsigned char** r_data, off_t *r_size) {
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

struct firmware* read_firmware_file(const char* filename) {
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
		free_firmware(f);
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

void write_firmware_file(const char* filename, struct firmware *f) {
	int fd;
	unsigned char* data;
	off_t size = 0;

	fd = open(filename, O_WRONLY | O_CREAT);
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

void dump_firm_type(unsigned int type)
{
	 if (type & BASE)
		printf("BASE ");
	 if (type & F8MHZ)
		printf("F8MHZ ");
	 if (type & MTS)
		printf("MTS ");
	 if (type & D2620)
		printf("D2620 ");
	 if (type & D2633)
		printf("D2633 ");
	 if (type & DTV6)
		printf("DTV6 ");
	 if (type & QAM)
		printf("QAM ");
	 if (type & DTV7)
		printf("DTV7 ");
	 if (type & DTV78)
		printf("DTV78 ");
	 if (type & DTV8)
		printf("DTV8 ");
	 if (type & FM)
		printf("FM ");
	 if (type & INPUT1)
		printf("INPUT1 ");
	 if (type & LCD)
		printf("LCD ");
	 if (type & NOGD)
		printf("NOGD ");
	 if (type & MONO)
		printf("MONO ");
	 if (type & ATSC)
		printf("ATSC ");
	 if (type & IF)
		printf("IF ");
	 if (type & LG60)
		printf("LG60 ");
	 if (type & ATI638)
		printf("ATI638 ");
	 if (type & OREN538)
		printf("OREN538 ");
	 if (type & OREN36)
		printf("OREN36 ");
	 if (type & TOYOTA388)
		printf("TOYOTA388 ");
	 if (type & TOYOTA794)
		printf("TOYOTA794 ");
	 if (type & DIBCOM52)
		printf("DIBCOM52 ");
	 if (type & ZARLINK456)
		printf("ZARLINK456 ");
	 if (type & CHINA)
		printf("CHINA ");
	 if (type & F6MHZ)
		printf("F6MHZ ");
	 if (type & INPUT2)
		printf("INPUT2 ");
	 if (type & SCODE)
		printf("SCODE ");
}

void dump_firm_std(v4l2_std_id id)
{
	v4l2_std_id old=-1, curr_id;

	/* Dumps video standards */
	while (old!=id) {
		old=id;
		if ( (id & V4L2_STD_PAL) == V4L2_STD_PAL) {
			printf ("PAL ");
			curr_id = V4L2_STD_PAL;
		} else if ( (id & V4L2_STD_MN) == V4L2_STD_MN) {
			printf ("NTSC PAL/M PAL/N ");
			curr_id = V4L2_STD_PAL;
		} else if ( (id & V4L2_STD_PAL_BG) == V4L2_STD_PAL_BG) {
			printf ("PAL/BG ");
			curr_id = V4L2_STD_PAL_BG;
		} else if ( (id & V4L2_STD_PAL_DK) == V4L2_STD_PAL_DK) {
			printf ("PAL/DK ");
			curr_id = V4L2_STD_PAL_DK;
		} else if ( (id & V4L2_STD_PAL_B) == V4L2_STD_PAL_B) {
			printf ("PAL/B ");
			curr_id = V4L2_STD_PAL_B;
		} else if ( (id & V4L2_STD_PAL_B1) == V4L2_STD_PAL_B1) {
			printf ("PAL/B1 ");
			curr_id = V4L2_STD_PAL_B1;
		} else if ( (id & V4L2_STD_PAL_G) == V4L2_STD_PAL_G) {
			printf ("PAL/G ");
			curr_id = V4L2_STD_PAL_G;
		} else if ( (id & V4L2_STD_PAL_H) == V4L2_STD_PAL_H) {
			printf ("PAL/H ");
			curr_id = V4L2_STD_PAL_H;
		} else if ( (id & V4L2_STD_PAL_I) == V4L2_STD_PAL_I) {
			printf ("PAL/I ");
			curr_id = V4L2_STD_PAL_I;
		} else if ( (id & V4L2_STD_PAL_D) == V4L2_STD_PAL_D) {
			printf ("PAL/D ");
			curr_id = V4L2_STD_PAL_D;
		} else if ( (id & V4L2_STD_PAL_D1) == V4L2_STD_PAL_D1) {
			printf ("PAL/D1 ");
			curr_id = V4L2_STD_PAL_D1;
		} else if ( (id & V4L2_STD_PAL_K) == V4L2_STD_PAL_K) {
			printf ("PAL/K ");
			curr_id = V4L2_STD_PAL_K;
		} else if ( (id & V4L2_STD_PAL_M) == V4L2_STD_PAL_M) {
			printf ("PAL/M ");
			curr_id = V4L2_STD_PAL_M;
		} else if ( (id & V4L2_STD_PAL_N) == V4L2_STD_PAL_N) {
			printf ("PAL/N ");
			curr_id = V4L2_STD_PAL_N;
		} else if ( (id & V4L2_STD_PAL_Nc) == V4L2_STD_PAL_Nc) {
			printf ("PAL/Nc ");
			curr_id = V4L2_STD_PAL_Nc;
		} else if ( (id & V4L2_STD_PAL_60) == V4L2_STD_PAL_60) {
			printf ("PAL/60 ");
			curr_id = V4L2_STD_PAL_60;
		} else if ( (id & V4L2_STD_NTSC) == V4L2_STD_NTSC) {
			printf ("NTSC ");
			curr_id = V4L2_STD_NTSC;
		} else if ( (id & V4L2_STD_NTSC_M) == V4L2_STD_NTSC_M) {
			printf ("NTSC/M ");
			curr_id = V4L2_STD_NTSC_M;
		} else if ( (id & V4L2_STD_NTSC_M_JP) == V4L2_STD_NTSC_M_JP) {
			printf ("NTSC/M Jp ");
			curr_id = V4L2_STD_NTSC_M_JP;
		} else if ( (id & V4L2_STD_NTSC_443) == V4L2_STD_NTSC_443) {
			printf ("NTSC 443 ");
			curr_id = V4L2_STD_NTSC_443;
		} else if ( (id & V4L2_STD_NTSC_M_KR) == V4L2_STD_NTSC_M_KR) {
			printf ("NTSC/M Kr ");
			curr_id = V4L2_STD_NTSC_M_KR;
		} else if ( (id & V4L2_STD_SECAM) == V4L2_STD_SECAM) {
			printf ("SECAM ");
			curr_id = V4L2_STD_SECAM;
		} else if ( (id & V4L2_STD_SECAM_DK) == V4L2_STD_SECAM_DK) {
			printf ("SECAM/DK ");
			curr_id = V4L2_STD_SECAM_DK;
		} else if ( (id & V4L2_STD_SECAM_B) == V4L2_STD_SECAM_B) {
			printf ("SECAM/B ");
			curr_id = V4L2_STD_SECAM_B;
		} else if ( (id & V4L2_STD_SECAM_D) == V4L2_STD_SECAM_D) {
			printf ("SECAM/D ");
			curr_id = V4L2_STD_SECAM_D;
		} else if ( (id & V4L2_STD_SECAM_G) == V4L2_STD_SECAM_G) {
			printf ("SECAM/G ");
			curr_id = V4L2_STD_SECAM_G;
		} else if ( (id & V4L2_STD_SECAM_H) == V4L2_STD_SECAM_H) {
			printf ("SECAM/H ");
			curr_id = V4L2_STD_SECAM_H;
		} else if ( (id & V4L2_STD_SECAM_K) == V4L2_STD_SECAM_K) {
			printf ("SECAM/K ");
			curr_id = V4L2_STD_SECAM_K;
		} else if ( (id & V4L2_STD_SECAM_K1) == V4L2_STD_SECAM_K1) {
			printf ("SECAM/K1 ");
			curr_id = V4L2_STD_SECAM_K1;
		} else if ( (id & V4L2_STD_SECAM_K3) == V4L2_STD_SECAM_K3) {
			printf ("SECAM/K3 ");
			curr_id = V4L2_STD_SECAM_K3;
		} else if ( (id & V4L2_STD_SECAM_L) == V4L2_STD_SECAM_L) {
			printf ("SECAM/L ");
			curr_id = V4L2_STD_SECAM_L;
		} else if ( (id & V4L2_STD_SECAM_LC) == V4L2_STD_SECAM_LC) {
			printf ("SECAM/Lc ");
			curr_id = V4L2_STD_SECAM_LC;
		} else if ( (id & V4L2_STD_A2) == V4L2_STD_A2) {
			printf ("A2 ");
			curr_id = V4L2_STD_A2;
		} else if ( (id & V4L2_STD_A2_A) == V4L2_STD_A2_A) {
			printf ("A2/A ");
			curr_id = V4L2_STD_A2_A;
		} else if ( (id & V4L2_STD_A2_B) == V4L2_STD_A2_B) {
			printf ("A2/A ");
			curr_id = V4L2_STD_A2_B;
		} else if ( (id & V4L2_STD_NICAM) == V4L2_STD_NICAM) {
			printf ("NICAM ");
			curr_id = V4L2_STD_NICAM;
		} else if ( (id & V4L2_STD_NICAM_A) == V4L2_STD_NICAM_A) {
			printf ("NICAM/A ");
			curr_id = V4L2_STD_NICAM_A;
		} else if ( (id & V4L2_STD_NICAM_B) == V4L2_STD_NICAM_B) {
			printf ("NICAM/B ");
			curr_id = V4L2_STD_NICAM_B;
		} else if ( (id & V4L2_STD_AM) == V4L2_STD_AM) {
			printf ("AM ");
			curr_id = V4L2_STD_AM;
		} else if ( (id & V4L2_STD_BTSC) == V4L2_STD_BTSC) {
			printf ("BTSC ");
			curr_id = V4L2_STD_BTSC;
		} else if ( (id & V4L2_STD_EIAJ) == V4L2_STD_EIAJ) {
			printf ("EIAJ ");
			curr_id = V4L2_STD_EIAJ;
		} else {
			curr_id = 0;
			break;
		}
		id &= ~curr_id;
	}
}

void list_firmware(struct firmware *f) {
	unsigned int i = 0;

	printf("firmware name:\t%s\n", f->name);
	printf("version:\t%d.%d (%u)\n", f->version >> 8, f->version & 0xff,
					  f->version);
	printf("standards:\t%u\n", f->nr_desc);
	for(i = 0; i < f->nr_desc; ++i) {
		printf("Firmware %u, ", i);
		printf("type: ");
		dump_firm_type(f->desc[i].type);
		printf("(0x%08x), ", f->desc[i].type);
		printf("id: ");
		dump_firm_std(f->desc[i].id);
		printf("(%016llx), ", f->desc[i].id);
		printf("size: %u\n", f->desc[i].size);
	}
}

void add_standard(struct firmware* f, char* firmware_file, char* standard_file) {
	unsigned char* standard_data;
	unsigned int len, i;
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

void delete_standard(struct firmware* f, char* firmware_file, __u16 i) {
	delete_firmware_description(f, i);
	write_firmware_file(firmware_file, f);
}

void set_standard_type(struct firmware* f, char* firmware_file, __u16 i, __u32 type) {
	if(i > f->nr_desc) {
		return;
	}
	f->desc[i].type = type;
	write_firmware_file(firmware_file, f);
}

void set_standard_id(struct firmware* f, char* firmware_file, __u16 i, __u32 id) {
	if(i > f->nr_desc) {
		return;
	}
	f->desc[i].id = id;
	write_firmware_file(firmware_file, f);
}

void print_usage(void)
{
	printf("firmware-tool usage:\n");
	printf("\t firmware-tool --list <firmware-file>\n");
	printf("\t firmware-tool --add <firmware-dump> <firmware-file>\n");
	printf("\t firmware-tool --delete <index> <firmware-file>\n");
	printf("\t firmware-tool --type <type> --index <i> <firmware-file>\n");
	printf("\t firmware-tool --id <type> --index <i> <firmware-file>\n");
}

int main(int argc, char* argv[])
{
	int c;
	int nr_args;
	unsigned int action = 0;
	char* firmware_file, *file = NULL, *nr_str = NULL, *index_str = NULL;
	struct firmware *f;
	__u64 nr;

	while(1) {
		static struct option long_options[] = {
			{"list",     no_argument,      0, 'l'},
			{"add",     required_argument, 0, 'a'},
			{"delete",  required_argument, 0, 'd'},
			{"type",  required_argument, 0, 't'},
			{"id",  required_argument, 0, 's'},
			{"index",  required_argument, 0, 'i'},
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
			list_firmware(f);
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
		break;
	}
	return 0;
}
