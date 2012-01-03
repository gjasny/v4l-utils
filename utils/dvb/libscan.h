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
	int video_pid_len, audio_pid_len;
	uint16_t *video_pid;
	uint16_t *audio_pid;
};

struct program_association_section {
	uint16_t  ts_id;
	unsigned char version;
	struct pid_table *pid_table;
	unsigned pid_table_len;
};

struct dvb_descriptors {
	struct program_association_section pat_table;
};

struct dvb_descriptors *get_dvb_ts_tables(char *dmxdev);
