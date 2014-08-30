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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or, point your browser to http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 *
 */
#ifndef _DVB_FE_H
#define _DVB_FE_H

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include "dvb-frontend.h"
#include "dvb-sat.h"
#include "dvb-log.h"

#define ARRAY_SIZE(x)	(sizeof(x)/sizeof((x)[0]))

/* Max number of delivery systems for a given frontend. */
#define MAX_DELIVERY_SYSTEMS	20

/*
 * The libdvbv5 API works with a set of key/value properties.
 * There are two types of properties:
 *
 *	- The ones defined at the Kernel's frontent API, that are found at
 *        /usr/include/linux/dvb/frontend.h (actually, it uses a local copy
 *	  of that file, stored at ./include/linux/dvb/frontend.h)
 *
 * 	- some extra properties used by libdvbv5. Those can be found at
 *	  lib/include/libdvbv5/dvb-v5-std.h and start at DTV_USER_COMMAND_START.
 */

/*
 * There are a few aliases for other properties. Those are needed just
 * to avoid breaking apps that depend on the library but shoudn't be used
 * anymore on newer apps.
 */
#define DTV_MAX_STATS			DTV_NUM_STATS_PROPS
#define DTV_SIGNAL_STRENGTH		DTV_STAT_SIGNAL_STRENGTH
#define DTV_SNR				DTV_STAT_CNR
#define DTV_UNCORRECTED_BLOCKS		DTV_STAT_ERROR_BLOCK_COUNT

/**
 * struct dvb_v5_fe_parms - Keeps data needed to handle the DVB frontend
 *
 * @info:		Contains the DVB info properties (RO)
 * @version:		Version of the Linux DVB API (RO)
 * @has_v5_stats:	a value different than 0 indicates that the frontend
 * 			supports DVBv5 stats (RO)
 * @current_sys:	currently selected delivery system (RO)
 * @num_systems:	number of delivery systems  (RO)
 * @systems:		delivery systems supported by the hardware (RO)
 * @legacy_fe:		a value different than 0 indicates a legacy Kernel
 *			driver using DVBv3 API only, or that DVBv3 only mode
 *			was forced by the client (RO)
 * @abort:		Client should set it to abort a pending operation
 *			like DTV scan (RW)
 * @lna:		sets the LNA mode: 0 disables; 1 enables, -1 uses
 *			auto mode (RW)
 * @lnb:		LNBf description (RW)
 * @sat_number:		number of the satellite (used by DISEqC setup) (RW)
 * @freq_bpf:		SCR/Unicable band-pass filter frequency to use, in kHz
 * @verbose:		Verbosity level of the library (RW)
 * @dvb_logfunc:	Function used to write log messages (RO)
 * @default_charset:	Name of the charset used by the DVB standard (RW)
 * @output_charset:	Name of the charset to output (system specific) (RW)
 *
 * The fields marked as RO should not be changed by the client, as otherwise
 * undesired effects may happen. The ones marked as RW are ok to either read
 * or write by the client.
 */
struct dvb_v5_fe_parms {
	/* Information visible to the client - don't override those values */
	struct dvb_frontend_info	info;
	uint32_t			version;
	int				has_v5_stats;
	fe_delivery_system_t		current_sys;
	int				num_systems;
	fe_delivery_system_t		systems[MAX_DELIVERY_SYSTEMS];
	int				legacy_fe;

	/* The values below are specified by the library client */

	/* Flags from the client to the library */
	int				abort;

	/* Linear Amplifier settings */
	int				lna;

	/* Satellite settings */
	const struct dvb_sat_lnb       	*lnb;
	int				sat_number;
	unsigned			freq_bpf;
	unsigned			diseqc_wait;

	/* Function to write DVB logs */
	unsigned			verbose;
	dvb_logfunc                     logfunc;

	/* Charsets to be used by the conversion utilities */
	char				*default_charset;
	char				*output_charset;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * dvb_fe_dummy() - Allocates a dummy frontend structure
 *
 * This is useful for some applications that may want to just use the
 * frontend structure internally, without associating it with a real hardware
 */
struct dvb_v5_fe_parms *dvb_fe_dummy();

/**
 * dvb_fe_open() - Opens a frontend and allocates a structure to work with
 *
 * @adapter:		Number of the adapter to open
 * @frontend:		Number of the frontend to open
 * @verbose:		Verbosity level of the messages that will be printed
 * @use_legacy_call:	Force to use the DVBv3 calls, instead of using the
 * 			DVBv5 API
 *
 * This function should be called before using any other function at the
 * frontend library (or the other alternatives: dvb_fe_open2() or
 * dvb_fe_dummy().
 *
 * Returns NULL on error.
 */
struct dvb_v5_fe_parms *dvb_fe_open(int adapter, int frontend,
				    unsigned verbose, unsigned use_legacy_call);

/**
 * dvb_fe_open2() - Opens a frontend and allocates a structure to work with
 *
 * @adapter:		Number of the adapter to open
 * @frontend:		Number of the frontend to open
 * @verbose:		Verbosity level of the messages that will be printed
 * @use_legacy_call:	Force to use the DVBv3 calls, instead of using the
 *			DVBv5 API
 * @logfunc:		Callback function to be called when a log event
 *			happens. Can either store the event into a file or to
 *			print it at the TUI/GUI.
 *
 * This function should be called before using any other function at the
 * frontend library (or the other alternatives: dvb_fe_open() or
 * dvb_fe_dummy().
 *
 * Returns NULL on error.
 */
struct dvb_v5_fe_parms *dvb_fe_open2(int adapter, int frontend,
				    unsigned verbose, unsigned use_legacy_call,
				    dvb_logfunc logfunc);

/**
 * dvb_fe_close() - Closes the frontend and frees allocated resources
 */
void dvb_fe_close(struct dvb_v5_fe_parms *parms);

/**
 * dvb_cmd_name() - Returns the string name associated with a DVBv5 command
 * @cmd:	DVBv5 or libdvbv5 property
 *
 * This function gets an integer argument (cmd) and returns a string that
 * corresponds to the name of that property.
 * For example:
 * 	dvb_cmd_name(DTV_GUARD_INTERVAL) would return "GUARD_INTERVAL"
 * It also returns names for the properties used internally by libdvbv5.
 */
const char *dvb_cmd_name(int cmd);

/**
 * dvb_attr_names() - Returns an string array with the valid string values
 * 		      associated with a DVBv5 command
 * @cmd:	DVBv5 or libdvbv5 property
 *
 * This function gets an integer argument (cmd) and returns a string array
 * that  corresponds to the names associated with the possible values for
 * that property, when available.
 * For example:
 * 	dvb_cmd_name(DTV_CODE_RATE_HP) would return an array with the
 * possible values for the code rates:
 *	{ "1/2", "2/3", ... NULL }
 * The array always ends with NULL.
 */
const char *const *dvb_attr_names(int cmd);

/* Get/set delivery system parameters */

/**
 * dvb_fe_retrieve_parm() - retrieves the value of a DVBv5/libdvbv5 property
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @cmd:	DVBv5 or libdvbv5 property
 * @value:	Pointer to an uint32_t where the value will be stored.
 *
 * This reads the value of a property stored at the cache. Before using it,
 * a dvb_fe_get_parms() is likely required.
 *
 * Return 0 if success, EINVAL otherwise.
 */
int dvb_fe_retrieve_parm(const struct dvb_v5_fe_parms *parms,
			unsigned cmd, uint32_t *value);

/**
 * dvb_fe_store_parm() - stores the value of a DVBv5/libdvbv5 property
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @cmd:	DVBv5 or libdvbv5 property
 * @value:	Pointer to an uint32_t where the value will be stored.
 *
 * This stores the value of a property at the cache. The value will only
 * be send to the hardware after calling dvb_fe_set_parms().
 *
 * Return 0 if success, EINVAL otherwise.
 */
int dvb_fe_store_parm(struct dvb_v5_fe_parms *parms,
		      unsigned cmd, uint32_t value);

/**
 * dvb_set_sys() - Sets the delivery system
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @sys:	delivery system to be selected
 *
 * This function changes the delivery system of the frontend. By default,
 * the libdvbv5 will use the first available delivery system. If another
 * delivery system is desirable, this function should be called before being
 * able to store the properties for the new delivery system via
 * dvb_fe_store_parm().
 */
int dvb_set_sys(struct dvb_v5_fe_parms *parms,
		   fe_delivery_system_t sys);

/**
 * dvb_add_parms_for_sys() - Make dvb properties reflect the current standard
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @sys:	delivery system to be selected
 *
 * This function prepares the properties cache for a given delivery system.
 *
 * It is automatically called by dvb_set_sys(), and should not be normally
 * called, except when dvb_fe_dummy() is used.
 */
int dvb_add_parms_for_sys(struct dvb_v5_fe_parms *parms,
			  fe_delivery_system_t sys);

/**
 * dvb_set_compat_delivery_system() - Sets the delivery system
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @sys:	delivery system to be selected
 *
 * This function changes the delivery system of the frontend. By default,
 * the libdvbv5 will use the first available delivery system. If another
 * delivery system is desirable, this function should be called before being
 * able to store the properties for the new delivery system via
 * dvb_fe_store_parm().
 *
 * This function is an enhanced version of dvb_set_sys(): it has an special
 * logic inside to work with Kernels that supports onld DVBv3.
 */
int dvb_set_compat_delivery_system(struct dvb_v5_fe_parms *parms,
				   uint32_t desired_system);

/**
 * dvb_fe_prt_parms() - Prints all the properties at the cache.
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 *
 * Used mostly for debugging issues.
 */
void dvb_fe_prt_parms(const struct dvb_v5_fe_parms *parms);

/**
 * dvb_fe_set_parms() - Prints all the properties at the cache.
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 *
 * Writes the properties stored at the DVB cache at the DVB hardware. At
 * return, some properties could have a different value, as the frontend
 * may not support the values set.
 */
int dvb_fe_set_parms(struct dvb_v5_fe_parms *parms);

/**
 * dvb_fe_get_parms() - Prints all the properties at the cache.
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 *
 * Gets the properties from the DVB hardware. The values will only reflect
 * what's set at the hardware if the frontend is locked.
 */
int dvb_fe_get_parms(struct dvb_v5_fe_parms *parms);

/*
 * Get statistics
 *
 * Just like the DTV properties, the stats are cached. That warrants that
 * all stats are got at the same time, when dvb_fe_get_stats() is called.
 *
 */

/**
 * dvb_fe_retrieve_stats_layer() Retrieve the stats for a DTV layer from cache
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @cmd:	DVBv5 or libdvbv5 property
 * @layer:	DTV layer
 *
 * Gets the value for one stats cache, on a given layer. Layer 0 is
 * always present. On DTV standards that doesn't have layers, it returns
 * the same value as dvb_fe_retrieve_stats() for layer = 0.
 *
 * For DTV standards with multiple layers, like ISDB, layer=1 is layer 'A',
 * layer=2 is layer 'B' and layer=3 is layer 'C'. Please notice that not all
 * frontends support per-layer stats. Also, the layer value is only valid if
 * the layer exists at the original stream.
 * Also, on such standards, layer 0 is typically a mean value of the layers,
 * or a sum of events (if FE_SCALE_COUNTER).
 *
 * For it to be valid, dvb_fe_get_stats() should be called first.
 *
 * It returns a struct dtv_stats if succeed or NULL otherwise.
 */
struct dtv_stats *dvb_fe_retrieve_stats_layer(struct dvb_v5_fe_parms *parms,
                                              unsigned cmd, unsigned layer);

/**
 * dvb_fe_retrieve_stats() Retrieve the stats for a DTV layer from cache
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @cmd:	DVBv5 or libdvbv5 property
 * @layer:	DTV layer
 *
 * Gets the value for one stats property for layer = 0.
 *
 * For it to be valid, dvb_fe_get_stats() should be called first.
 *
 * The returned value is 0 if success, EINVAL otherwise.
 */
int dvb_fe_retrieve_stats(struct dvb_v5_fe_parms *parms,
			  unsigned cmd, uint32_t *value);

/**
 * dvb_fe_get_stats() Retrieve the stats from the Kernel
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 *
 * Updates the stats cache from the available stats at the Kernel.
 *
 * The returned value is 0 if success, EINVAL otherwise.
 */
int dvb_fe_get_stats(struct dvb_v5_fe_parms *parms);

/**
 * dvb_fe_retrieve_ber() Retrieve the BER stats from cache
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @layer:	DTV layer
 * @scale:	retrieves the scale
 *
 * Gets the value for BER stats from stats cache, on a given layer. Layer 0 is
 * always present. On DTV standards that doesn't have layers, it returns
 * the same value as dvb_fe_retrieve_stats() for layer = 0.
 *
 * For DTV standards with multiple layers, like ISDB, layer=1 is layer 'A',
 * layer=2 is layer 'B' and layer=3 is layer 'C'. Please notice that not all
 * frontends support per-layer stats. Also, the layer value is only valid if
 * the layer exists at the original stream.
 * Also, on such standards, layer 0 is typically a mean value of the layers,
 * or a sum of events (if FE_SCALE_COUNTER).
 *
 * For it to be valid, dvb_fe_get_stats() should be called first.
 *
 * a scale equal to FE_SCALE_NOT_AVAILABLE means that BER is not available.
 *
 * It returns a float for the BER value.
 */
float dvb_fe_retrieve_ber(struct dvb_v5_fe_parms *parms, unsigned layer,
                          enum fecap_scale_params *scale);

/**
 * dvb_fe_retrieve_per() Retrieve the PER stats from cache
 *
 * @parms:	struct dvb_v5_fe_parms pointer to the opened device
 * @layer:	DTV layer
 * @scale:
 *
 * Gets the value for BER stats from stats cache, on a given layer. Layer 0 is
 * always present. On DTV standards that doesn't have layers, it returns
 * the same value as dvb_fe_retrieve_stats() for layer = 0.
 *
 * For DTV standards with multiple layers, like ISDB, layer=1 is layer 'A',
 * layer=2 is layer 'B' and layer=3 is layer 'C'. Please notice that not all
 * frontends support per-layer stats. Also, the layer value is only valid if
 * the layer exists at the original stream.
 * Also, on such standards, layer 0 is typically a mean value of the layers,
 * or a sum of events (if FE_SCALE_COUNTER).
 *
 * For it to be valid, dvb_fe_get_stats() should be called first.
 *
 * A negative value indicates error.
 */
float dvb_fe_retrieve_per(struct dvb_v5_fe_parms *parms, unsigned layer);

/**
 * dvb_fe_snprintf_eng() - Ancillary function to sprintf on ENG format
 *
 * @buf:	buffer to store the value
 * @len:	buffer length
 * @val:	value to be printed
 *
 * On ENG notation, the exponential value should be multiple of 3. This is
 * good to display some values, like BER.
 *
 * At return, it shows the actual size of the print.
 */
int dvb_fe_snprintf_eng(char *buf, int len, float val);


/**
 * dvb_fe_snprintf_eng() - Ancillary function to sprintf on ENG format
 *
 * @parms:		struct dvb_v5_fe_parms pointer to the opened device
 * @cmd:		DVBv5 or libdvbv5 property
 * @display_name:	String with the name of the property to be shown
 * @layer:		DTV Layer
 * @buf:		buffer to store the value
 * @len:		buffer length
 * @show_layer_name:	a value different than zero shows the layer name, if
 * 			the layer is bigger than zero.
 *
 * This function calls internally dvb_fe_retrieve_stats_layer(). It allows to
 * print a DVBv5 statistics value into a string. An extra property is available
 * (DTV_QUALITY) with prints either one of the values: Poor, Ok or Good,
 * depending on the overall measures.
 */
 int dvb_fe_snprintf_stat(struct dvb_v5_fe_parms *parms, uint32_t cmd,
			  char *display_name, int layer,
		          char **buf, int *len, int *show_layer_name);

/**
 * dvb_fe_get_event() - Get both status statistics and dvb parameters
 *
 * @parms:		struct dvb_v5_fe_parms pointer to the opened device
 *
 * That's similar of calling both dvb_fe_get_parms() and dvb_fe_get_stats().
 *
 * It returns 0 if success or an errorno otherwise.
 */
int dvb_fe_get_event(struct dvb_v5_fe_parms *parms);

/*
 * Other functions, associated to SEC/LNB/DISEqC
 *
 * The functions bellow are just wrappers for the Kernel calls, in order to
 * manually control satellite systems.
 *
 * Instead of using them, the best is to set the LNBf parameters, and let
 * the libdvbv5 to automatically handle the calls.
 *
 * NOTE: It currently lacks support for two ioctl's:
 * FE_DISEQC_RESET_OVERLOAD	used only on av7110.
 * Spec says:
 *   If the bus has been automatically powered off due to power overload,
 *   this ioctl call restores the power to the bus. The call requires read/write
 *   access to the device. This call has no effect if the device is manually
 *   powered off. Not all DVB adapters support this ioctl.
 *
 * FE_DISHNETWORK_SEND_LEGACY_CMD is used on av7110, budget, gp8psk and stv0299
 * Spec says:
 *   WARNING: This is a very obscure legacy command, used only at stv0299
 *   driver. Should not be used on newer drivers.
 *   It provides a non-standard method for selecting Diseqc voltage on the
 *   frontend, for Dish Network legacy switches.
 *   As support for this ioctl were added in 2004, this means that such dishes
 *   were already legacy in 2004.
 *
 * So, it doesn't make much sense on implementing support for them.
 */

int dvb_fe_sec_voltage(struct dvb_v5_fe_parms *parms, int on, int v18);
int dvb_fe_sec_tone(struct dvb_v5_fe_parms *parms, fe_sec_tone_mode_t tone);
int dvb_fe_lnb_high_voltage(struct dvb_v5_fe_parms *parms, int on);
int dvb_fe_diseqc_burst(struct dvb_v5_fe_parms *parms, int mini_b);
int dvb_fe_diseqc_cmd(struct dvb_v5_fe_parms *parms, const unsigned len,
		      const unsigned char *buf);
int dvb_fe_diseqc_reply(struct dvb_v5_fe_parms *parms, unsigned *len, char *buf,
		       int timeout);
int dvb_fe_is_satellite(uint32_t delivery_system);

#ifdef __cplusplus
}
#endif

/*
 * Arrays from dvb-v5.h
 *
 * Those arrays can be used to translate from a DVB property into a name.
 *
 * No need to directly access them from userspace, as dvb_attr_names()
 * already handles them into a more standard way.
 */

extern const unsigned fe_bandwidth_name[8];
extern const char *dvb_v5_name[71];
extern const void *dvb_v5_attr_names[];
extern const char *delivery_system_name[20];
extern const char *fe_code_rate_name[14];
extern const char *fe_modulation_name[15];
extern const char *fe_transmission_mode_name[10];
extern const unsigned fe_bandwidth_name[8];
extern const char *fe_guard_interval_name[12];
extern const char *fe_hierarchy_name[6];
extern const char *fe_voltage_name[4];
extern const char *fe_tone_name[3];
extern const char *fe_inversion_name[4];
extern const char *fe_pilot_name[4];
extern const char *fe_rolloff_name[5];

#endif
