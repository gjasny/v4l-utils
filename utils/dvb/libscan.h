#include <stdint.h>
#include <linux/dvb/dmx.h>

/* According with ISO/IEC 13818-1:2007 */

struct pmt_table {
	uint16_t program_number, pcr_pid;
	unsigned char version;
};

struct pid_table {
	uint16_t program_number;
	uint16_t pid;
	struct pmt_table pmt_table;
	unsigned video_pid_len, audio_pid_len;
	uint16_t *video_pid;
	uint16_t *audio_pid;
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

struct nit_table {
	uint16_t network_id;
	unsigned char version;
	char *network_name, *network_alias;
	struct transport_table *tr_table;
	unsigned tr_table_len;
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
	struct pat_table pat_table;
	struct nit_table nit_table;
	struct sdt_table sdt_table;
};

struct dvb_descriptors *get_dvb_ts_tables(char *dmxdev);
void free_dvb_ts_tables(struct dvb_descriptors *dvb_desc);
