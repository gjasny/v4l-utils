#ifndef ALSA_STREAM_H
#define ALSA_STREAM_H

#include <stdio.h>
#include <sys/time.h>

int alsa_thread_startup(const char *pdevice, const char *cdevice,
			int latency, FILE *__error_fp, int __verbose);
void alsa_thread_stop(void);
int alsa_thread_is_running(void);
void alsa_thread_timestamp(struct timeval *tv);
#endif
