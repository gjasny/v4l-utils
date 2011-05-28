/*
 * Copyright © 2011 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:
 * 	Mauro Carvalho Chehab <mchehab@redhat.com>
 */

#include "../libmedia_dev/get_media_devices.h"
#include <stdio.h>

static void print_all_associated_devices(void *md, char *vid,
					 enum device_type type)
{
	char *devname = NULL;
	int first = 1;

	do {
		devname = get_associated_device(md, devname, type,
						vid, V4L_VIDEO);
		if (devname) {
			if (first) {
				printf("\t%s: ", media_device_type(type));
				first = 0;
			}

			printf("%s ", devname);
		}
	} while (devname);
	if (!first)
		printf("\n");
}

static void print_all_alsa_independent_playback(void *md)
{
	char *devname = NULL;
	int first = 1;

	do {
		devname = get_not_associated_device(md, devname,
						    SND_OUT, V4L_VIDEO);
		if (devname) {
			if (first) {
				printf("Alsa playback device(s): ");
				first = 0;
			}

			printf("%s ", devname);
		}
	} while (devname);
	if (!first)
		printf("\n");
}

int main(void)
{
	void *md;
	char *vid, *alsa;
	int i;

	md = discover_media_devices();

	vid = NULL;
	do {
		vid = get_associated_device(md, vid, V4L_VIDEO,
						NULL, NONE);
		if (!vid)
			break;
		printf("Video device: %s\n", vid);

		for (i = 0; i <= SND_HW; i++)
			print_all_associated_devices(md, vid, i);
	} while (vid);

	print_all_alsa_independent_playback(md);

	free_media_devices(md);

	return 0;
}
