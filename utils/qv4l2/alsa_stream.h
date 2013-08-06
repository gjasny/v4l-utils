int alsa_thread_startup(const char *pdevice, const char *cdevice, int latency,
			FILE *__error_fp,
			int __verbose);
void alsa_thread_stop(void);
int alsa_thread_is_running(void);
