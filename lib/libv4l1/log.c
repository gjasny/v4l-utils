/*
#             (C) 2008 Hans de Goede <hdegoede@redhat.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdio.h>
#include <stdlib.h>
#include "../libv4lconvert/libv4lsyscall-priv.h"
#include <linux/videodev.h>
#include "libv4l1-priv.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

FILE *v4l1_log_file = NULL;

static const char *v4l1_ioctls[] = {
	[_IOC_NR(VIDIOCGCAP)]       = "VIDIOCGCAP",
	[_IOC_NR(VIDIOCGCHAN)]      = "VIDIOCGCHAN",
	[_IOC_NR(VIDIOCSCHAN)]      = "VIDIOCSCHAN",
	[_IOC_NR(VIDIOCGTUNER)]     = "VIDIOCGTUNER",
	[_IOC_NR(VIDIOCSTUNER)]     = "VIDIOCSTUNER",
	[_IOC_NR(VIDIOCGPICT)]      = "VIDIOCGPICT",
	[_IOC_NR(VIDIOCSPICT)]      = "VIDIOCSPICT",
	[_IOC_NR(VIDIOCCAPTURE)]    = "VIDIOCCAPTURE",
	[_IOC_NR(VIDIOCGWIN)]       = "VIDIOCGWIN",
	[_IOC_NR(VIDIOCSWIN)]       = "VIDIOCSWIN",
	[_IOC_NR(VIDIOCGFBUF)]      = "VIDIOCGFBUF",
	[_IOC_NR(VIDIOCSFBUF)]      = "VIDIOCSFBUF",
	[_IOC_NR(VIDIOCKEY)]        = "VIDIOCKEY",
	[_IOC_NR(VIDIOCGFREQ)]      = "VIDIOCGFREQ",
	[_IOC_NR(VIDIOCSFREQ)]      = "VIDIOCSFREQ",
	[_IOC_NR(VIDIOCGAUDIO)]     = "VIDIOCGAUDIO",
	[_IOC_NR(VIDIOCSAUDIO)]     = "VIDIOCSAUDIO",
	[_IOC_NR(VIDIOCSYNC)]       = "VIDIOCSYNC",
	[_IOC_NR(VIDIOCMCAPTURE)]   = "VIDIOCMCAPTURE",
	[_IOC_NR(VIDIOCGMBUF)]      = "VIDIOCGMBUF",
	[_IOC_NR(VIDIOCGUNIT)]      = "VIDIOCGUNIT",
	[_IOC_NR(VIDIOCGCAPTURE)]   = "VIDIOCGCAPTURE",
	[_IOC_NR(VIDIOCSCAPTURE)]   = "VIDIOCSCAPTURE",
	[_IOC_NR(VIDIOCSPLAYMODE)]  = "VIDIOCSPLAYMODE",
	[_IOC_NR(VIDIOCSWRITEMODE)] = "VIDIOCSWRITEMODE",
	[_IOC_NR(VIDIOCGPLAYINFO)]  = "VIDIOCGPLAYINFO",
	[_IOC_NR(VIDIOCSMICROCODE)] = "VIDIOCSMICROCODE",
	[_IOC_NR(VIDIOCGVBIFMT)]    = "VIDIOCGVBIFMT",
	[_IOC_NR(VIDIOCSVBIFMT)]    = "VIDIOCSVBIFMT",
};

void v4l1_log_ioctl(unsigned long int request, void *arg, int result)
{
  const char *ioctl_str = "unknown";
  char buf[40];

  if (!v4l1_log_file)
    return;

  /* Don't log v4l2 ioctl's as unknown we pass them to libv4l2 which will
     log them for us */
  if (_IOC_TYPE(request) == 'V')
    return;

  if (_IOC_TYPE(request) == 'v' && _IOC_NR(request) < ARRAY_SIZE(v4l1_ioctls))
    ioctl_str = v4l1_ioctls[_IOC_NR(request)];
  else {
    snprintf(buf, sizeof(buf), "unknown request: %c %d\n",
      (int)_IOC_TYPE(request), (int)_IOC_NR(request));
    ioctl_str = buf;
  }

  fprintf(v4l1_log_file, "request == %s\n", ioctl_str);

  switch(request)
  {
	  case VIDIOCGCAP:fprintf(v4l1_log_file,"name 	%s\n",(	(struct video_capability*)arg)->name		);
			  fprintf(v4l1_log_file,"type 	%d\n",(	(struct video_capability*)arg)->type		);
			  fprintf(v4l1_log_file,"channels 	%d\n",(	(struct video_capability*)arg)->channels	);
			  fprintf(v4l1_log_file,"audios 	%d\n",(	(struct video_capability*)arg)->audios		);
			  fprintf(v4l1_log_file,"maxwidth 	%d\n",(	(struct video_capability*)arg)->maxwidth	);
			  fprintf(v4l1_log_file,"maxheight 	%d\n",(	(struct video_capability*)arg)->maxheight	);
			  fprintf(v4l1_log_file,"minwidth 	%d\n",(	(struct video_capability*)arg)->minwidth	);
			  fprintf(v4l1_log_file,"minheight 	%d\n",(	(struct video_capability*)arg)->minheight	);
			  break;
    case VIDIOCGWIN:
    case VIDIOCSWIN:
      fprintf(v4l1_log_file,"width\t%u\n",
	((struct video_window *)arg)->width);
      fprintf(v4l1_log_file,"height\t%u\n",
	((struct video_window *)arg)->height);
      break;

	  case VIDIOCGCHAN:
	  case VIDIOCSCHAN:
			  fprintf(v4l1_log_file,"channel 	%d\n",(	(struct video_channel*)arg)->channel		);
			  fprintf(v4l1_log_file,"name 	%s\n",(	(struct video_channel*)arg)->name		);
			  break;

	  case VIDIOCGPICT:
	  case VIDIOCSPICT:
			  fprintf(v4l1_log_file,"brightness 	%d\n",(	(int)((struct video_picture*)arg)->brightness)	);
			  fprintf(v4l1_log_file,"hue 	%d\n",(	(int)((struct video_picture*)arg)->hue)		);
			  fprintf(v4l1_log_file,"colour 	%d\n",(	(int)((struct video_picture*)arg)->colour)	);
			  fprintf(v4l1_log_file,"contrast 	%d\n",(	(int)((struct video_picture*)arg)->contrast)	);
			  fprintf(v4l1_log_file,"whiteness 	%d\n",(	(int)((struct video_picture*)arg)->whiteness)	);
			  fprintf(v4l1_log_file,"depth 	%d\n",(	(int)((struct video_picture*)arg)->depth)	);
			  fprintf(v4l1_log_file,"palette 	%d\n",(	(int)((struct video_picture*)arg)->palette)	);
			  break;

	  case VIDIOCCAPTURE: fprintf(v4l1_log_file,"on/off? 	%d\n",	*((int *)arg) 					);
			  break;

	  case VIDIOCSYNC: fprintf(v4l1_log_file,"sync 	%d\n",	*((int *)arg)					);
			  break;

	  case VIDIOCMCAPTURE:
			  fprintf(v4l1_log_file,"frame 	%u\n",(	(struct video_mmap*)arg)->frame			);
			  fprintf(v4l1_log_file,"width 	%d\n",(	(struct video_mmap*)arg)->width			);
			  fprintf(v4l1_log_file,"height 	%d\n",(	(struct video_mmap*)arg)->height		);
			  fprintf(v4l1_log_file,"format 	%u\n",(	(struct video_mmap*)arg)->format		);
			  break;

	  case VIDIOCGMBUF:
			  fprintf(v4l1_log_file,"size 	%d\n",(	(struct video_mbuf*)arg)->size			);
			  fprintf(v4l1_log_file,"frames 	%d\n",(	(struct video_mbuf*)arg)->frames		);
			  break;
  }
  fprintf(v4l1_log_file, "result == %d\n", result);
  fflush(v4l1_log_file);
}
