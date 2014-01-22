/*
 *  ALSA streaming support
 *
 *  Originally written by:
 *      Copyright (c) by Devin Heitmueller <dheitmueller@kernellabs.com>
 *	for usage at tvtime
 *  Derived from the alsa-driver test tool latency.c:
 *    Copyright (c) by Jaroslav Kysela <perex@perex.cz>
 *
 *  Copyright (c) 2011 - Mauro Carvalho Chehab
 *	Ported to xawtv, with bug fixes and improvements
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <config.h>

#ifdef HAVE_ALSA
#include "alsa_stream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

/* Private vars to control alsa thread status */
static int stop_alsa = 0;
static snd_htimestamp_t timestamp;

/* Error handlers */
snd_output_t *output = NULL;
FILE *error_fp;
int verbose = 0;

struct final_params {
    int bufsize;
    int rate;
    int latency;
    int channels;
};

static int setparams_stream(snd_pcm_t *handle,
			    snd_pcm_hw_params_t *params,
			    snd_pcm_format_t format,
			    int *channels,
			    const char *id)
{
    int err;

    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
	fprintf(error_fp,
		"alsa: Broken configuration for %s PCM: no configurations available: %s\n",
		snd_strerror(err), id);
	return err;
    }

    err = snd_pcm_hw_params_set_access(handle, params,
				       SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
	fprintf(error_fp, "alsa: Access type not available for %s: %s\n", id,
		snd_strerror(err));
	return err;
    }

    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
	fprintf(error_fp, "alsa: Sample format not available for %s: %s\n", id,
	       snd_strerror(err));
	return err;
    }

retry:
    err = snd_pcm_hw_params_set_channels(handle, params, *channels);
    if (err < 0) {
	if (strcmp(id, "capture") == 0 && *channels == 2) {
	    *channels = 1;
	    goto retry; /* Retry with mono capture */
	}
	fprintf(error_fp, "alsa: Channels count (%i) not available for %s: %s\n",
		*channels, id, snd_strerror(err));
	return err;
    }

    return 0;
}

static void getparams_periods(snd_pcm_t *handle,
		      snd_pcm_hw_params_t *params,
		      unsigned int *usecs,
		      unsigned int *count,
		      const char *id)
{
    unsigned min = 0, max = 0;

    snd_pcm_hw_params_get_periods_min(params, &min, 0);
    snd_pcm_hw_params_get_periods_max(params, &max, 0);
    if (min && max) {
	if (verbose)
	    fprintf(error_fp, "alsa: %s periods range between %u and %u. Want: %u\n",
		    id, min, max, *count);
	if (*count < min)
	    *count = min;
	if (*count > max)
	    *count = max;
    }

    min = max = 0;
    snd_pcm_hw_params_get_period_time_min(params, &min, 0);
    snd_pcm_hw_params_get_period_time_max(params, &max, 0);
    if (min && max) {
	if (verbose)
	    fprintf(error_fp, "alsa: %s period time range between %u and %u. Want: %u\n",
		    id, min, max, *usecs);
	if (*usecs < min)
	    *usecs = min;
	if (*usecs > max)
	    *usecs = max;
    }
}

static int setparams_periods(snd_pcm_t *handle,
		      snd_pcm_hw_params_t *params,
		      unsigned int *usecs,
		      unsigned int *count,
		      const char *id)
{
    int err;

    err = snd_pcm_hw_params_set_period_time_near(handle, params, usecs, 0);
    if (err < 0) {
	    fprintf(error_fp, "alsa: Unable to set period time %u for %s: %s\n",
		    *usecs, id, snd_strerror(err));
	    return err;
    }

    err = snd_pcm_hw_params_set_periods_near(handle, params, count, 0);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to set %u periods for %s: %s\n",
		*count, id, snd_strerror(err));
	return err;
    }

    if (verbose)
	fprintf(error_fp, "alsa: %s period set to %u periods of %u time\n",
		id, *count, *usecs);

    return 0;
}

static int setparams_set(snd_pcm_t *handle,
			 snd_pcm_hw_params_t *params,
			 snd_pcm_sw_params_t *swparams,
			 snd_pcm_uframes_t start_treshold,
			 const char *id)
{
    int err;

    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to set hw params for %s: %s\n",
		id, snd_strerror(err));
	return err;
    }
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to determine current swparams for %s: %s\n",
		id, snd_strerror(err));
	return err;
    }
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams,
						start_treshold);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to set start threshold mode for %s: %s\n",
		id, snd_strerror(err));
	return err;
    }

    err = snd_pcm_sw_params_set_avail_min(handle, swparams, 4);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to set avail min for %s: %s\n",
		id, snd_strerror(err));
	return err;
    }

    err = snd_pcm_sw_params_set_tstamp_mode(handle, swparams, SND_PCM_TSTAMP_ENABLE);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to enable timestamps for %s: %s\n",
		id, snd_strerror(err));
    }

    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
	fprintf(error_fp, "alsa: Unable to set sw params for %s: %s\n",
		id, snd_strerror(err));
	return err;
    }
    return 0;
}

static int alsa_try_rate(snd_pcm_t *phandle, snd_pcm_t *chandle,
                        snd_pcm_hw_params_t *p_hwparams,
                        snd_pcm_hw_params_t *c_hwparams,
                        int allow_resample, unsigned *ratep, unsigned *ratec)
{
    int err;

    err = snd_pcm_hw_params_set_rate_near(chandle, c_hwparams, ratec, 0);
    if (err)
        return err;

    *ratep = *ratec;
    err = snd_pcm_hw_params_set_rate_near(phandle, p_hwparams, ratep, 0);
    if (err)
        return err;

    if (*ratep == *ratec)
        return 0;

    if (verbose)
        fprintf(error_fp,
                "alsa_try_rate: capture wanted %u, playback wanted %u%s\n",
                *ratec, *ratep, allow_resample ? " with resample enabled": "");

    return 1; /* No error, but also no match */
}

static int setparams(snd_pcm_t *phandle, snd_pcm_t *chandle,
		     snd_pcm_format_t format,
		     int latency, int allow_resample,
		     struct final_params *negotiated)
{
    int i;
    unsigned ratep, ratec = 0;
    unsigned ratemin = 32000, ratemax = 96000, val;
    int err, channels = 2;
    snd_pcm_hw_params_t *p_hwparams, *c_hwparams;
    snd_pcm_sw_params_t *p_swparams, *c_swparams;
    snd_pcm_uframes_t c_size, p_psize, c_psize;
    /* Our latency is 2 periods (in usecs) */
    unsigned int c_periods = 2, p_periods;
    unsigned int c_periodtime, p_periodtime;
    const unsigned int prefered_rates[] = { 44100, 48000, 32000 };

    snd_pcm_hw_params_alloca(&p_hwparams);
    snd_pcm_hw_params_alloca(&c_hwparams);
    snd_pcm_sw_params_alloca(&p_swparams);
    snd_pcm_sw_params_alloca(&c_swparams);

    if (setparams_stream(chandle, c_hwparams, format, &channels, "capture"))
	return 1;

    if (setparams_stream(phandle, p_hwparams, format, &channels, "playback"))
	return 1;

    if (allow_resample) {
	err = snd_pcm_hw_params_set_rate_resample(chandle, c_hwparams, 1);
	if (err < 0) {
	    fprintf(error_fp, "alsa: Resample setup failed: %s\n", snd_strerror(err));
	    return 1;
	} else if (verbose)
	   fprintf(error_fp, "alsa: Resample enabled.\n");
    }

    err = snd_pcm_hw_params_get_rate_min(c_hwparams, &ratemin, 0);
    if (err >= 0 && verbose)
	fprintf(error_fp, "alsa: Capture min rate is %d\n", ratemin);
    err = snd_pcm_hw_params_get_rate_max(c_hwparams, &ratemax, 0);
    if (err >= 0 && verbose)
	fprintf(error_fp, "alsa: Capture max rate is %u\n", ratemax);

    err = snd_pcm_hw_params_get_rate_min(p_hwparams, &val, 0);
    if (err >= 0) {
	if (verbose)
	    fprintf(error_fp, "alsa: Playback min rate is %u\n", val);
	if (val > ratemin)
		ratemin = val;
    }
    err = snd_pcm_hw_params_get_rate_max(p_hwparams, &val, 0);
    if (err >= 0) {
	if (verbose)
	    fprintf(error_fp, "alsa: Playback max rate is %u\n", val);
	if (val < ratemax)
		ratemax = val;
    }

    if (verbose)
	fprintf(error_fp,
	        "alsa: Will search a common rate between %u and %u\n",
		ratemin, ratemax);

    /* First try a set of common rates */
    err = -1;
    for (i = 0; i < ARRAY_SIZE(prefered_rates); i++) {
        if (prefered_rates[i] < ratemin || prefered_rates[i] > ratemax)
            continue;
        ratep = ratec = prefered_rates[i];
        err = alsa_try_rate(phandle, chandle, p_hwparams, c_hwparams,
                            allow_resample, &ratep, &ratec);
        if (err == 0)
            break;
    }

    if (err != 0) {
        if (ratemin >= 44100) {
            for (i = ratemin; i <= ratemax; i += 100) {
                ratep = ratec = i;
                err = alsa_try_rate(phandle, chandle, p_hwparams, c_hwparams,
                                    allow_resample, &ratep, &ratec);
                if (err == 0)
                    break;
            }
        } else {
            for (i = ratemax; i >= ratemin; i -= 100) {
                ratep = ratec = i;
                err = alsa_try_rate(phandle, chandle, p_hwparams, c_hwparams,
                                    allow_resample, &ratep, &ratec);
                if (err == 0)
                    break;
            }
        }
    }

    if (err < 0) {
	fprintf(error_fp, "alsa: Failed to set a supported rate: %s\n",
		snd_strerror(err));
	return 1;
    }
    if (ratep != ratec) {
	if (verbose || allow_resample)
	    fprintf(error_fp,
		    "alsa: Couldn't find a rate that it is supported by both playback and capture\n");
	return 2;
    }
    if (verbose)
	fprintf(error_fp, "alsa: Using Rate %d\n", ratec);

    /* Negotiate period parameters */

    c_periodtime = latency * 1000 / c_periods;
    getparams_periods(chandle, c_hwparams, &c_periodtime, &c_periods, "capture");
    p_periods = c_periods * 2;
    p_periodtime = c_periodtime;
    getparams_periods(phandle, p_hwparams, &p_periodtime, &p_periods, "playback");
    c_periods = p_periods / 2;

    /*
     * Some playback devices support a very limited periodtime range. If the user needs to
     * use a higher latency to avoid overrun/underrun, use an alternate algorithm of incresing
     * the number of periods, to archive the needed latency
     */
    if (p_periodtime < c_periodtime) {
	c_periodtime = p_periodtime;
	c_periods = round (latency * 1000.0 / c_periodtime + 0.5);
	getparams_periods(chandle, c_hwparams, &c_periodtime, &c_periods, "capture");
	p_periods = c_periods * 2;
	p_periodtime = c_periodtime;
	getparams_periods(phandle, p_hwparams, &p_periodtime, &p_periods, "playback");
	c_periods = p_periods / 2;
    }

    if (setparams_periods(chandle, c_hwparams, &c_periodtime, &c_periods, "capture"))
	return 1;

    /* Note we use twice as much periods for the playback buffer, since we
       will get a period size near the requested time and we don't want it to
       end up smaller then the capture buffer as then we could end up blocking
       on writing to it. Note we will configure the playback dev to start
       playing as soon as it has 2 capture periods worth of data, so this
       won't influence latency */
    if (setparams_periods(phandle, p_hwparams, &p_periodtime, &p_periods, "playback"))
	return 1;

    snd_pcm_hw_params_get_period_size(p_hwparams, &p_psize, NULL);
    snd_pcm_hw_params_get_period_size(c_hwparams, &c_psize, NULL);
    snd_pcm_hw_params_get_buffer_size(c_hwparams, &c_size);

    latency = c_periods * c_psize;
    if (setparams_set(phandle, p_hwparams, p_swparams, latency, "playback"))
	return 1;

    if (setparams_set(chandle, c_hwparams, c_swparams, c_psize, "capture"))
	return 1;

    if ((err = snd_pcm_prepare(phandle)) < 0) {
	fprintf(error_fp, "alsa: Prepare error: %s\n", snd_strerror(err));
	return 1;
    }

    if (verbose) {
	fprintf(error_fp, "alsa: Negociated configuration:\n");
	snd_pcm_dump_setup(phandle, output);
	snd_pcm_dump_setup(chandle, output);
	fprintf(error_fp, "alsa: Parameters are %iHz, %s, %i channels\n",
		ratep, snd_pcm_format_name(format), channels);
	fprintf(error_fp, "alsa: Set bitrate to %u%s, buffer size is %u\n", ratec,
		allow_resample ? " with resample enabled at playback": "",
		(unsigned int)c_size);
    }

    negotiated->bufsize = c_size;
    negotiated->rate = ratep;
    negotiated->channels = channels;
    negotiated->latency = latency;
    return 0;
}

/* Read up to len frames */
static snd_pcm_sframes_t readbuf(snd_pcm_t *handle, char *buf, long len)
{
    snd_pcm_sframes_t r;
    snd_pcm_uframes_t frames;
    snd_pcm_htimestamp(handle, &frames, &timestamp);
    r = snd_pcm_readi(handle, buf, len);
    if (r < 0 && r != -EAGAIN) {
	r = snd_pcm_recover(handle, r, 0);
	if (r < 0)
	    fprintf(error_fp, "alsa: overrun recover error: %s\n", snd_strerror(r));
    }
    return r;
}

/* Write len frames (note not up to len, but all of len!) */
static snd_pcm_sframes_t writebuf(snd_pcm_t *handle, char *buf, long len)
{
    snd_pcm_sframes_t r;

    while (!stop_alsa) {
	r = snd_pcm_writei(handle, buf, len);
	if (r == len)
	    return 0;
	if (r < 0) {
	    r = snd_pcm_recover(handle, r, 0);
	    if (r < 0) {
		fprintf(error_fp, "alsa: underrun recover error: %s\n",
			snd_strerror(r));
		return r;
	    }
	}
	buf += r * 4;
	len -= r;
	snd_pcm_wait(handle, 100);
    }
    return -1;
}

static int alsa_stream(const char *pdevice, const char *cdevice, int latency)
{
    snd_pcm_t *phandle, *chandle;
    char *buffer;
    int err;
    ssize_t r;
    struct final_params negotiated;
    snd_pcm_format_t format = SND_PCM_FORMAT_S16_LE;
    char pdevice_new[32];

    err = snd_output_stdio_attach(&output, error_fp, 0);
    if (err < 0) {
	fprintf(error_fp, "alsa: Output failed: %s\n", snd_strerror(err));
	return 0;
    }

    /* Open the devices */
    if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK,
			    0)) < 0) {
	fprintf(error_fp, "alsa: Cannot open playback device %s: %s\n",
		pdevice, snd_strerror(err));
	return 0;
    }
    if ((err = snd_pcm_open(&chandle, cdevice, SND_PCM_STREAM_CAPTURE,
			    SND_PCM_NONBLOCK)) < 0) {
	fprintf(error_fp, "alsa: Cannot open capture device %s: %s\n",
		cdevice, snd_strerror(err));
	snd_pcm_close(phandle);
	return 0;
    }

    err = setparams(phandle, chandle, format, latency, 0, &negotiated);

    /* Try to use plughw instead, as it allows emulating speed */
    if (err == 2 && strncmp(pdevice, "hw", 2) == 0) {

	snd_pcm_close(phandle);

	sprintf(pdevice_new, "plug%s", pdevice);
	pdevice = pdevice_new;
	if (verbose)
	    fprintf(error_fp, "alsa: Trying %s for playback\n", pdevice);
	if ((err = snd_pcm_open(&phandle, pdevice, SND_PCM_STREAM_PLAYBACK,
				0)) < 0) {
	    fprintf(error_fp, "alsa: Cannot open playback device %s: %s\n",
		    pdevice, snd_strerror(err));
	    snd_pcm_close(chandle);
	    return 0;
	}

	err = setparams(phandle, chandle, format, latency, 1, &negotiated);
    }

    if (err != 0) {
	fprintf(error_fp, "alsa: setparams failed\n");
	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	return 1;
    }

    buffer = malloc((negotiated.bufsize * snd_pcm_format_width(format) / 8)
		    * negotiated.channels);
    if (buffer == NULL) {
	fprintf(error_fp, "alsa: Failed allocating buffer for audio\n");
	snd_pcm_close(phandle);
	snd_pcm_close(chandle);
	return 0;
    }

    if (verbose)
        fprintf(error_fp,
	    "alsa: stream started from %s to %s (%i Hz, buffer delay = %.2f ms)\n",
	    cdevice, pdevice, negotiated.rate,
	    negotiated.latency * 1000.0 / negotiated.rate);

    while (!stop_alsa) {
	/* We start with a read and not a wait to auto(re)start the capture */
	r = readbuf(chandle, buffer, negotiated.bufsize);
	if (r == 0)   /* Succesfully recovered from an overrun? */
	    continue; /* Force restart of capture stream */
	if (r > 0)
	    writebuf(phandle, buffer, r);
	/* use poll to wait for next event */
	while (!stop_alsa && !snd_pcm_wait(chandle, 50))
	    ;
    }

    snd_pcm_drop(chandle);
    snd_pcm_drop(phandle);

    snd_pcm_unlink(chandle);
    snd_pcm_hw_free(phandle);
    snd_pcm_hw_free(chandle);

    snd_pcm_close(phandle);
    snd_pcm_close(chandle);

    return 0;
}

struct input_params {
    char *pdevice;
    char *cdevice;
    int latency;
};

static void *alsa_thread_entry(void *whatever)
{
    struct input_params *inputs = (struct input_params *) whatever;

    if (verbose)
	fprintf(error_fp, "alsa: starting copying alsa stream from %s to %s\n",
		inputs->cdevice, inputs->pdevice);
    alsa_stream(inputs->pdevice, inputs->cdevice, inputs->latency);
    if (verbose)
        fprintf(error_fp, "alsa: stream stopped\n");

    free(inputs->pdevice);
    free(inputs->cdevice);
    free(inputs);

    return NULL;
}

/*************************************************************************
 Public functions
 *************************************************************************/

static int alsa_is_running = 0;
static pthread_t alsa_thread;

int alsa_thread_startup(const char *pdevice, const char *cdevice, int latency,
			FILE *__error_fp, int __verbose)
{
    int ret;
    struct input_params *inputs;

    if ((strcasecmp(pdevice, "disabled") == 0) ||
	(strcasecmp(cdevice, "disabled") == 0))
	return 0;

    if (__error_fp)
	error_fp = __error_fp;
    else
	error_fp = stderr;

    verbose = __verbose;

    if (alsa_is_running) {
        fprintf(error_fp, "alsa: Already running\n");
        return EBUSY;
    }

    inputs = malloc(sizeof(struct input_params));
    if (inputs == NULL) {
	fprintf(error_fp, "alsa: failed allocating memory for inputs\n");
	return ENOMEM;
    }

    inputs->pdevice = strdup(pdevice);
    inputs->cdevice = strdup(cdevice);
    inputs->latency = latency;

    stop_alsa = 0;
    ret = pthread_create(&alsa_thread, NULL,
			 &alsa_thread_entry, (void *) inputs);
    if (ret == 0)
        alsa_is_running = 1;

    return ret;
}

void alsa_thread_stop(void)
{
    if (!alsa_is_running)
        return;

    stop_alsa = 1;
    pthread_join(alsa_thread, NULL);
    alsa_is_running = 0;
}

int alsa_thread_is_running(void)
{
    return alsa_is_running;
}

void alsa_thread_timestamp(struct timeval *tv)
{
	if (alsa_thread_is_running()) {
		tv->tv_sec = timestamp.tv_sec;
		tv->tv_usec = timestamp.tv_nsec / 1000;
	} else {
		tv->tv_sec = 0;
		tv->tv_usec = 0;
	}
}
#endif
