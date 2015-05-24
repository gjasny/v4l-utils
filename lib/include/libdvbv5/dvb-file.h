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

/**
 * @file dvb-file.h
 * @ingroup file
 * @brief Provides interfaces to deal with DVB channel and program files.
 * @copyright GNU General Public License version 2 (GPLv2)
 * @author Mauro Carvalho Chehab
 *
 * There are basically two types of files used for DVB:
 * - files that describe the physical channels (also called as transponders);
 * - files that describe the several programs found on a MPEG-TS (also called
 *   as zap files).
 *
 * The libdvbv5 library defines an unified type for both types. Other
 * applications generally use different formats.
 *
 * The purpose of the functions and structures defined herein is to provide
 * support to read and write to those different formats.
 *
 * @par Bug Report
 * Please submit bug reports and patches to linux-media@vger.kernel.org
 */

/*
 * DVB structures used to represent all files opened by the libdvbv5 library.
 *
 * Those structs represents each individual entry on a file, and the file
 * as a whole.
 */

/**
 * @struct dvb_elementary_pid
 * @brief associates an elementary stream type with its PID
 * @ingroup file
 *
 * @param type	Elementary stream type
 * @param pid	Elementary stream Program ID
 */
struct dvb_elementary_pid {
	uint8_t  type;
	uint16_t pid;
};

/**
 * @struct dvb_entry
 * @brief  Represents one entry on a DTV file
 * @ingroup file
 *
 * @param props			A property key/value pair. The keys are the ones
 *				specified at the DVB API, plus the ones defined
 *				internally by libdvbv5, at the dvb-v5-std.h
 *				header file.
 * @param next			a pointer to the next entry. NULL if this is
 *				the last one.
 * @param service_id		Service ID associated with a program inside a
 *				transponder. Please note that pure "channel"
 *				files will have this field filled with 0.
 * @param video_pid		Array with the video program IDs inside a service
 * @param audio_pid		Array with the audio program IDs inside a service
 * @param other_el_pid		Array with all non-audio/video  program IDs
 *				inside a service
 * @param video_pid_len		Size of the video_pid array
 * @param audio_pid_len		Size of the audio_pid array
 * @param other_el_pid_len	Size of the other_el_pid array
 * @param channel		String containing the name of the channel
 * @param vchannel		String representing the Number of the channel
 * @param location		String representing the location of the channel
 * @param sat_number		For satellite streams, this represents the
 *				number of the satellite dish on a DiSeqC
 *				arrangement. Should be zero on arrangements
 *				without DiSeqC.
 * @param freq_bpf		SCR/Unicable band-pass filter frequency to
 *				use, in kHz.
 *				For non SRC/Unicable arrangements, it should
 *				be zero.
 * @param diseqc_wait		Extra time to wait for DiSeqC commands to
 *				complete, in ms. The library will use 15 ms
 *				as the minimal time,
 *				plus the time specified on this field.
 * @param lnb			String with the name of the LNBf to be used for
 *				satellite tuning. The names should match the
 *				names provided by dvb_sat_get_lnb() call
 *				(see dvb-sat.h).
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
 * @struct dvb_file
 * @brief  Describes an entire DVB file opened
 *
 * @param fname		name of the file
 * @param n_entries	number of the entries read
 * @param first_entry	entry for the first entry. NULL if the file is empty.
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

/**
 * @struct dvb_parse_table
 * @brief  Describes the fields to parse on a file
 * @ingroup file
 *
 * @param prop			Name of the DVBv5 or libdvbv5 property field
 * @param table			Name of a translation table for string to
 *				int conversion
 * @param size			Size of the translation table
 * @param mult_factor		Multiply factor - Used, for example, to
 *				multiply the symbol rate read from a DVB-S
 *				table by 1000.
 * @param has_default_value	It is different than zero when the property
 *				can be optional. In this case, the next field
 *				should be present
 * @param default_value		Default value for the optional field
 */
struct dvb_parse_table {
	unsigned int prop;
	const char **table;
	unsigned int size;
	int	mult_factor;
	int	has_default_value;
	int	default_value;
};
/**
 * @struct dvb_parse_struct
 * @brief  Describes the format to parse an specific delivery system
 * @ingroup file
 *
 * @param id		String that identifies the delivery system on the
 * 			file to be parsed
 * @param delsys	Delivery system
 * @param table		the struct dvb_parse_table used to parse for this
 *			specific delivery system
 * @param size		Size of the table
 */
struct dvb_parse_struct {
	char				*id;
	uint32_t			delsys;
	const struct dvb_parse_table	*table;
	unsigned int			size;
};

/**
 * @struct dvb_parse_file
 * @brief  Describes an entire file format
 *
 * @param has_delsys_id		A non-zero value indicates that the id field
 *				at the formats vector should be used
 * @param delimiter		Delimiters to split entries on the format
 * @param formats		A struct dvb_parse_struct vector with the
 *				per delivery system parsers. This table should
 *				terminate with an empty entry.
 */
struct dvb_parse_file {
	int has_delsys_id;
	char *delimiter;
	struct dvb_parse_struct formats[];
};

/**
 * @enum  dvb_file_formats
 * @brief Known file formats
 * @ingroup file
 *
 * @details
 * Please notice that the channel format defined here has a few optional
 * fields that aren't part of the dvb-apps format, for DVB-S2 and for DVB-T2.
 * They're there to match the formats found at dtv-scan-tables package up to
 * September, 5 2014.
 *
 * @var FILE_UNKNOWN
 *	@brief File format is unknown
 * @var FILE_ZAP
 *	@brief File is at the dvb-apps "dvbzap" format
 * @var FILE_CHANNEL
 *	@brief File is at the dvb-apps output format for dvb-zap
 * @var FILE_DVBV5
 *	@brief File is at libdvbv5 format
 * @var FILE_VDR
 *	@brief File is at DVR format (as supported on version 2.1.6).
 *	       Note: this is only supported as an output format.
 */
enum dvb_file_formats {
	FILE_UNKNOWN,
	FILE_ZAP,
	FILE_CHANNEL,
	FILE_DVBV5,
	FILE_VDR,
};

struct dvb_v5_descriptors;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Deallocates memory associated with a struct dvb_file
 * @ingroup file
 *
 * @param dvb_file	dvb_file struct to be deallocated
 *
 * This function assumes that several functions were dynamically allocated
 * by the library file functions.
 */
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

/*
 * File format description structures defined for the several formats that
 * the library can read natively.
 */

/**
 * @brief File format definitions for dvb-apps channel format
 * @ingroup file
 */
extern const struct dvb_parse_file channel_file_format;

/**
 * @brief File format definitions for dvb-apps zap format
 * @ingroup file
 */
extern const struct dvb_parse_file channel_file_zap_format;

/*
 * Prototypes for the several functions defined at dvb-file.c
 */

/**
 * @brief Read a file at libdvbv5 format
 * @ingroup file
 *
 * @param fname		file name
 *
 * @return It returns a pointer to struct dvb_file describing the entries that
 * were read from the file. If it fails, NULL is returned.
 */
struct dvb_file *dvb_read_file(const char *fname);

/**
 * @brief Write a file at libdvbv5 format
 * @ingroup file
 *
 * @param fname		file name
 * @param dvb_file	contents of the file to be written
 *
 * @return It returns zero if success, or a positive error number if it fails.
 */
int dvb_write_file(const char *fname, struct dvb_file *dvb_file);

/**
 * @brief Read a file on any format natively supported by
 *			    the library
 * @ingroup file
 *
 * @param fname		file name
 * @param delsys	Delivery system, as specified by enum fe_delivery_system
 * @param format	Name of the format to be read
 *
 * @return It returns a pointer to struct dvb_file describing the entries that
 * were read from the file. If it fails, NULL is returned.
 */
struct dvb_file *dvb_read_file_format(const char *fname,
					   uint32_t delsys,
					   enum dvb_file_formats format);

/**
 * @brief Write a file on any format natively supported by
 *			    the library
 * @ingroup file
 *
 * @param fname	file name
 * @param dvb_file	contents of the file to be written
 * @param delsys	Delivery system, as specified by enum fe_delivery_system
 * @param format	Name of the format to be read
 *
 * @return It a pointer to struct dvb_file on success, NULL otherwise.
 */
int dvb_write_file_format(const char *fname,
			  struct dvb_file *dvb_file,
			  uint32_t delsys,
			  enum dvb_file_formats format);


/**
 * @brief Stores a key/value pair on a DVB file entry
 * @ingroup file
 *
 * @param entry	entry to be filled
 * @param cmd	key for the property to be used. It be one of the DVBv5
 * 		properties, plus the libdvbv5 ones, as defined at dvb-v5-std.h
 * @param value	value for the property.
 *
 * This function seeks for a property with the name specified by cmd and
 * fills it with value. If the entry doesn't exist, it creates a new key.
 *
 * @return Returns 0 if success, or, if the entry has already DTV_MAX_COMMAND
 * properties, it returns -1.
 */
int dvb_store_entry_prop(struct dvb_entry *entry,
		     uint32_t cmd, uint32_t value);

/**
 * @brief Retrieves the value associated witha key on a DVB file entry
 * @ingroup file
 *
 * @param entry	entry to be used
 * @param cmd	key for the property to be found. It be one of the DVBv5
 * 		properties, plus the libdvbv5 ones, as defined at dvb-v5-std.h
 * @param value	pointer to store the value associated with the property.
 *
 * This function seeks for a property with the name specified by cmd and
 * fills value with its contents.
 *
 * @return Returns 0 if success, or, -1 if the entry doesn't exist.
 */
int dvb_retrieve_entry_prop(struct dvb_entry *entry,
			uint32_t cmd, uint32_t *value);

/**
 * @brief stored a new scanned channel into a dvb_file struct
 * @ingroup file
 *
 * @param dvb_file	file struct to be filled
 * @param parms		struct dvb_v5_fe_parms used by libdvbv5 frontend
 * @param dvb_desc		struct dvb_desc as described at descriptors.h, filled
 *			with the descriptors associated with a DVB channel.
 *			those descriptors can be filled by calling one of the
 *			scan functions defined at dvb-sat.h.
 * @param get_detected	if different than zero, uses the frontend parameters
 *			obtained from the device driver (such as modulation,
 *			FEC, etc)
 * @param get_nit		if true, uses the parameters obtained from the MPEG-TS
 *			NIT table to add newly detected transponders.
 *
 * This function should be used to store the services found on a scanned
 * transponder. Initially, it copies the same parameters used to set the
 * frontend, that came from a file where the Service ID and Elementary Stream
 * PIDs are unknown. At tuning time, it is common to set the device to tune
 * on auto-detection mode (e. g. using QAM/AUTO, for example, to autodetect
 * the QAM modulation). The libdvbv5's logic will be to check the detected
 * values. So, the modulation might, for example, have changed to QAM/256.
 * In such case, if get_detected is 0, it will store QAM/AUTO at the struct.
 * If get_detected is different than zero, it will store QAM/256.
 * If get_nit is different than zero, and if the MPEG-TS has info about other
 * physical channels/transponders, this function will add newer entries to
 * dvb_file, for it to seek for new transponders. This is very useful especially
 * for DVB-C, where all transponders belong to the same operator. Knowing one
 * frequency is generally enough to get all DVB-C transponders.
 *
 * @return Returns 0 if success, or, -1 if error.
 */
int dvb_store_channel(struct dvb_file **dvb_file,
		      struct dvb_v5_fe_parms *parms,
		      struct dvb_v5_descriptors *dvb_desc,
		      int get_detected, int get_nit);

/**
 * @brief Ancillary function that seeks for a delivery system
 * @ingroup file
 *
 * @param name	string containing the name of the Delivery System to seek
 *
 * If the name is found, this function returns the DVBv5 property that
 * corresponds to the string given. The function is case-insensitive, and
 * it can check for alternate ways to write the name of a Delivery System.
 * Currently, it supports: DVB-C, DVB-H, DVB-S, DVB-S2, DVB-T, DVB-T2,
 * ISDB-C, ISDB-S, ISDB-T, ATSC-MH, DVBC/ANNEX_A, DVBC/ANNEX_B, DVBT, DSS,
 * DVBS, DVBS2, DVBH, ISDBT, ISDBS, ISDBC, ATSC, ATSCMH, DTMB, CMMB, DAB,
 * DVBT2, TURBO, DVBC/ANNEX_C.
 * Please notice that this doesn't mean that all those standards are properly
 * supported by the library.
 *
 * @return Returns the Delivery System property number if success, -1 if error.
 */
int dvb_parse_delsys(const char *name);

/**
 * @brief Ancillary function that parses the name of a file format
 * @ingroup file
 *
 * @param name	string containing the name of the format
 *		Current valid names are: ZAP, CHANNEL, VDR and DVBV5.
 *		The name is case-insensitive.
 *
 * @return It returns FILE_ZAP, FILE_CHANNEL, FILE_VDR or  FILE_DVBV5
 * if the name was translated. FILE_UNKNOWN otherwise.
 */
enum dvb_file_formats dvb_parse_format(const char *name);

/*
 * Routines to read a non-libdvbv5 format. They're called by
 * dvb_read_file_format() or dvb_write_file_format()
 */

/**
 * @brief Read and parses a one line file format
 * @ingroup file
 *
 * @param fname		file name
 * @param delsys	delivery system
 * @param parse_file	pointer struct dvb_parse_file
 *
 * @return It a pointer to struct dvb_file on success, NULL otherwise.
 *
 * This function is called internally by dvb_read_file_format.
 */
struct dvb_file *dvb_parse_format_oneline(const char *fname,
					  uint32_t delsys,
					  const struct dvb_parse_file *parse_file);

/**
 * @brief Writes a file into an one line file format
 * @ingroup file
 *
 * @param fname		file name
 * @param dvb_file	contents of the file to be written
 * @param delsys	delivery system
 * @param parse_file	pointer struct dvb_parse_file
 *
 * @return It returns zero if success, or a positive error number if it fails.
 *
 * This function is called internally by dvb_write_file_format.
 */
int dvb_write_format_oneline(const char *fname,
			     struct dvb_file *dvb_file,
			     uint32_t delsys,
			     const struct dvb_parse_file *parse_file);

/**
 * @brief Writes a file into vdr format (compatible up to version 2.1)
 * @ingroup file
 *
 * @param fname		file name
 * @param dvb_file	contents of the file to be written
 *
 * @return It returns zero if success, or a positive error number if it fails.
 *
 * This function is called internally by dvb_write_file_format.
 */
int dvb_write_format_vdr(const char *fname,
			 struct dvb_file *dvb_file);

#ifdef __cplusplus
}
#endif

#endif // _DVB_FILE_H
