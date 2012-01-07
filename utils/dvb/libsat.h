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
