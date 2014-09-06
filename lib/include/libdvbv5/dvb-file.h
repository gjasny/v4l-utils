/*
 * Copyright (c) 2011-2014 - Mauro Carvalho Chehab
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation version 2
 * of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef _DVB_FILE_H
#define _DVB_FILE_H

#include "dvb-fe.h"

/*
 * DVB structures used to represent all files opened by the libdvbv5 library.
 *
 * Those structs represents each individual entry on a file, and the file
 * as a hole.
 */

/**
 * struct dvb_elementary_pid - associates an elementary stream type with its PID
 *
 * @type:	Elementary stream type
 * @pid:	Elementary stream Program ID
 */
struct dvb_elementary_pid {
	uint8_t  type;
	uint16_t pid;
};

/**
 * struct dvb_entry - Represents one entry on a DTV file.
 *
 * @props:		a property key/value pair. The keys are the ones
 *			specified at the DVB API, plus the ones defined
 *			internally by libdvbv5, at the dvb-v5-std.h header file.
 * @next:		a pointer to the next entry. NULL if this is the last
 *			one.
 * @service_id:		Service ID associated with a program inside a
 *			transponder. Please note that pure "channel" files
 *			will have this field filled with 0.
 * @video_pid:		Array with the video program IDs inside a service
 * @audio_pid:		Array with the audio program IDs inside a service
 * @other_el_pid:	Array with all non-audio/video  program IDs inside a
 *			service
 * @video_pid_len:	Size of the video_pid array
 * @audio_pid_len:	Size of the audio_pid array
 * @other_el_pid_len:	Size of the other_el_pid array
 * @channel:		String containing the name of the channel
 * @vchannel:		String representing the Number of the channel
 * @location:		String representing the location of the channel
 * @sat_number:		For satellite streams, this represents the number of
 *			the satellite dish on a DiSeqC arrangement. Should be
 *			zero on arrangements without DiSeqC.
 * @freq_bpf:		SCR/Unicable band-pass filter frequency to use, in kHz.
 *			For non SRC/Unicable arrangements, it should be zero.
 * @diseqc_wait:	Extra time to wait for DiSeqC commands to complete,
 *			in ms. The library will use 15 ms as the minimal time,
 *			plus the time specified on this field.
 * @lnb:		String with the name of the LNBf to be used for
 *			satellite tuning. The names should match the names
 *			provided by dvb_sat_get_lnb() call (see dvb-sat.h).
 */
struct dvb_entry {
	struct dtv_property props[DTV_MAX_COMMAND];
	unsigned int n_props;
	struct dvb_entry *next;
	uint16_t service_id;
	uint16_t *video_pid, *audio_pid;
	struct dvb_elementary_pid *other_el_pid;
	unsigned video_pid_len, audio_pid_len, other_el_pid_len;
	char *channel;
	char *vchannel;

	char *location;

	int sat_number;
	unsigned freq_bpf;
	unsigned diseqc_wait;
	char *lnb;
};

/**
 * struct dvb_file - Describes an entire DVB file opened
 *
 * @fname:		name of the file
 * @n_entries:		number of the entries read
 * @first_entry:	entry for the first entry. NULL if the file is empty.
 */
struct dvb_file {
	char *fname;
	int n_entries;
	struct dvb_entry *first_entry;
};

/*
 * DVB file format tables
 *
 * The structs below are used to represent oneline formats like the ones
 * commonly found on DVB legacy applications.
 */

struct parse_table {
	unsigned int prop;
	const char **table;
	unsigned int size;
	int	mult_factor;	/* Factor to muliply from file parsing POV */
	int	has_default_value;	/* Mark an optional integer field - should be the last fields */
	int	default_value;		/* default for the optional field */
};

struct parse_struct {
	char				*id;
	uint32_t			delsys;
	const struct parse_table	*table;
	unsigned int			size;
};

struct parse_file {
	int has_delsys_id;
	char *delimiter;
	struct parse_struct formats[];
};

/* Known file formats */
enum file_formats {
	FILE_UNKNOWN,
	FILE_ZAP,
	FILE_CHANNEL,
	FILE_DVBV5,
};

#define PTABLE(a) .table = a, .size=ARRAY_SIZE(a)


struct dvb_v5_descriptors;

#ifdef __cplusplus
extern "C" {
#endif

static inline void dvb_file_free(struct dvb_file *dvb_file)
{
	struct dvb_entry *entry = dvb_file->first_entry, *next;
	while (entry) {
		next = entry->next;
		if (entry->channel)
			free(entry->channel);
		if (entry->vchannel)
			free(entry->vchannel);
		if (entry->location)
			free(entry->location);
		if (entry->video_pid)
			free(entry->video_pid);
		if (entry->audio_pid)
			free(entry->audio_pid);
		if (entry->other_el_pid)
			free(entry->other_el_pid);
		if (entry->lnb)
			free(entry->lnb);
		free(entry);
		entry = next;
	}
	free(dvb_file);
}

/* From dvb-legacy-channel-format.c */
extern const struct parse_file channel_file_format;

/* From dvb-zap-format.c */
extern const struct parse_file channel_file_zap_format;

/* From dvb-file.c */
struct dvb_file *dvb_parse_format_oneline(const char *fname,
					  uint32_t delsys,
					  const struct parse_file *parse_file);
int dvb_write_format_oneline(const char *fname,
			     struct dvb_file *dvb_file,
			     uint32_t delsys,
			     const struct parse_file *parse_file);

struct dvb_file *dvb_read_file(const char *fname);

int dvb_write_file(const char *fname, struct dvb_file *dvb_file);

int dvb_store_entry_prop(struct dvb_entry *entry,
		     uint32_t cmd, uint32_t value);
int dvb_retrieve_entry_prop(struct dvb_entry *entry,
			uint32_t cmd, uint32_t *value);

int dvb_store_channel(struct dvb_file **dvb_file,
		      struct dvb_v5_fe_parms *parms,
		      struct dvb_v5_descriptors *dvb_desc,
		      int get_detected, int get_nit);
int dvb_parse_delsys(const char *name);
enum file_formats dvb_parse_format(const char *name);
struct dvb_file *dvb_read_file_format(const char *fname,
					   uint32_t delsys,
					   enum file_formats format);
int dvb_write_file_format(const char *fname,
			  struct dvb_file *dvb_file,
			  uint32_t delsys,
			  enum file_formats format);

#ifdef __cplusplus
}
#endif

#endif // _DVB_FILE_H
