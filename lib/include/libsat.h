/*
 * Copyright (c) 2011-2012 - Mauro Carvalho Chehab <mchehab@redhat.com>
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
 */

enum polarization {
	POLARIZATION_OFF	= 0,
	POLARIZATION_H		= 1,
	POLARIZATION_V		= 2,
	POLARIZATION_L		= 3,
	POLARIZATION_R		= 4,
};

struct dvb_satellite_freqrange {
	unsigned low, high;
};

struct dvb_satellite_lnb {
	char *name;
	char *alias;
	unsigned lowfreq, highfreq;

	unsigned rangeswitch;

	struct dvb_satellite_freqrange freqrange[2];
};

struct dvb_v5_fe_parms *parms;

/* From libsat.c */
int search_lnb(char *name);
int print_lnb(int i);
void print_all_lnb(void);
struct dvb_satellite_lnb *get_lnb(int i);
int dvb_satellite_set_parms(struct dvb_v5_fe_parms *parms);
int dvb_satellite_get_parms(struct dvb_v5_fe_parms *parms);
