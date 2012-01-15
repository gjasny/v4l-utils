#include <stdint.h>
#include <linux/dvb/dmx.h>

/* According with ISO/IEC 13818-1:2007 */

struct pmt_table {
	uint16_t program_number, pcr_pid;
	unsigned char version;
};

struct el_pid {
	uint8_t  type;
	uint16_t pid;
};

struct pid_table {
	uint16_t service_id;
	uint16_t pid;
	struct pmt_table pmt_table;
	unsigned video_pid_len, audio_pid_len, other_el_pid_len;
	uint16_t *video_pid;
	uint16_t *audio_pid;
	struct el_pid *other_el_pid;
};

struct pat_table {
	uint16_t  ts_id;
	unsigned char version;
	struct pid_table *pid_table;
	unsigned pid_table_len;
};

struct transport_table {
	uint16_t tr_id;
};

struct lcn_table {
	uint16_t service_id;
	uint16_t lcn;
};

struct nit_table {
	uint16_t network_id;
	unsigned char version;
	char *network_name, *network_alias;
	struct transport_table *tr_table;
	unsigned tr_table_len;
	unsigned virtual_channel;
	unsigned area_code;

	/* Network Parameters */
	uint32_t delivery_system;
	uint32_t guard_interval;
	uint32_t fec_inner, fec_outer;
	uint32_t pol;
	uint32_t modulation;
	uint32_t rolloff;
	uint32_t symbol_rate;
	uint32_t bandwidth;
	uint32_t code_rate_hp;
	uint32_t code_rate_lp;
	uint32_t transmission_mode;
	uint32_t hierarchy;
	uint32_t plp_id;
	uint32_t system_id;

	unsigned has_dvbt:1;
	unsigned is_hp:1;
	unsigned has_time_slicing:1;
	unsigned has_mpe_fec:1;
	unsigned has_other_frequency:1;
	unsigned is_in_depth_interleaver:1;

	char *orbit;
	uint32_t *frequency;
	unsigned frequency_len;

	uint32_t *other_frequency;
	unsigned other_frequency_len;

	uint16_t *partial_reception;
	unsigned partial_reception_len;

	struct lcn_table *lcn;
	unsigned lcn_len;
};

struct service_table {
	uint16_t service_id;
	char running;
	char scrambled;
	unsigned char type;
	char *service_name, *service_alias;
	char *provider_name, *provider_alias;
};

struct sdt_table {
	unsigned char version;
	uint16_t ts_id;
	struct service_table *service_table;
	unsigned service_table_len;
};

struct dvb_descriptors {
	int verbose;
	uint32_t delivery_system;

	struct pat_table pat_table;
	struct nit_table nit_table;
	struct sdt_table sdt_table;

	/* Used by descriptors to know where to update a PMT/Service/TS */
	unsigned cur_pmt;
	unsigned cur_service;
	unsigned cur_ts;
};

struct dvb_descriptors *get_dvb_ts_tables(char *dmxdev,
					  uint32_t delivery_system,
					  unsigned timeout_multiply,
					  int verbose);
void free_dvb_ts_tables(struct dvb_descriptors *dvb_desc);
