/*
#             (C) 2008-2009 Elmar Kleijn <elmar_kleijn@hotmail.com>
#             (C) 2008-2009 Sjoerd Piepenbrink <need4weed@gmail.com>
#             (C) 2008-2009 Radjnies Bhansingh <radjnies@gmail.com>
#             (C) 2008-2009 Hans de Goede <hdegoede@redhat.com>
#             (C)      2009 Paul Sladen <ubuntu@paul.sladen.org>

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

#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include "libv4lcontrol.h"
#include "libv4lcontrol-priv.h"
#include "../libv4lsyscall-priv.h"
#include <linux/videodev2.h>

#define ARRAY_SIZE(x) ((int)sizeof(x)/(int)sizeof((x)[0]))

/* Workaround these potentially missing from videodev2.h */
#ifndef V4L2_IN_ST_HFLIP
#define V4L2_IN_ST_HFLIP       0x00000010 /* Frames are flipped horizontally */
#endif

#ifndef V4L2_IN_ST_VFLIP
#define V4L2_IN_ST_VFLIP       0x00000020 /* Frames are flipped vertically */
#endif


/* List of cams which need special flags */
static const struct v4lcontrol_flags_info v4lcontrol_flags[] = {
/* First: Upside down devices */
  /* Philips SPC200NC */
  { 0x0471, 0x0325, 0, NULL, NULL, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Philips SPC300NC */
  { 0x0471, 0x0326, 0, NULL, NULL, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Philips SPC210NC */
  { 0x0471, 0x032d, 0, NULL, NULL, V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Genius E-M 112 (also want whitebalance by default) */
  { 0x093a, 0x2476, 0, NULL, NULL,
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED|V4LCONTROL_WANTS_WB, 1500 },
  /* Laptops */
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "F7L       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "W7Sg      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "F7SR      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "G50VT     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "W7S       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "X55SR     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc.        ", "X55SV     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* note different white space: http://launchpad.net/bugs/413752 */
  { 0x04f2, 0xb012, 0, "ASUSTeK Computer Inc. ", "X71SL               ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* These 3 PACKARD BELL's seem to be Asus notebook in disguise */
  { 0x04f2, 0xb012, 0, "Packard Bell BV", "T32A      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "PACKARD BELL BV              ", "EasyNote_BG45",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb012, 0, "PACKARD BELL BV              ", "EasyNote_BG46",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb036, 0, "ASUSTeK Computer Inc.        ", "U6S       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb036, 0, "ASUSTeK Computer Inc.        ", "U6V       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "K40IJ     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "K50IJ     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "K50IN      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "K70AB     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "K70IO     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "N10J      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer Inc.        ", "N20A      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Note no whitespace padding for these 2 models, this is not a typo */
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer INC.", "K50AB",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb071, 0, "ASUSTeK Computer INC.", "N5051Tp",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb106, 0, "ASUSTeK Computer Inc.        ", "N50Vc      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb106, 0, "ASUSTeK Computer Inc.        ", "N50Vn      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb106, 0, "ASUSTeK Computer Inc.        ", "N51Vf      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb106, 0, "ASUSTeK Computer Inc.        ", "N51Vg      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb106, 0, "ASUSTeK Computer Inc.        ", "N51Vn     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x04f2, 0xb16b, 0, "ASUSTeK Computer Inc.        ", "U80A      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Note no whitespace padding for board vendor, this is not a typo */
  { 0x064e, 0xa111, 0, "ASUSTeK Computer Inc.", "F5RL      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  /* Another Asus laptop in disguise */
  { 0x064e, 0xa111, 0, "PEGATRON CORPORATION         ", "F5SR    ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x064e, 0xa116, 0, "ASUSTeK Computer Inc.        ", "N10Jb     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x064e, 0xa116, 0, "ASUSTeK Computer Inc.        ", "N10Jc     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x064e, 0xa116, 0, "ASUSTeK Computer Inc.        ", "N20A      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x064e, 0xa116, 0, "ASUSTeK Computer Inc.        ", "X58LE     ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x090c, 0xe370, 0, "ASUSTeK Computer Inc.        ", "U6S       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F3Ke      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F3Q       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F3Sa      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F3Sg      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F3Sr      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F5N       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "F5SL    ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "G1S       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "G1Sn      ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0x5a35, 0, "ASUSTeK Computer Inc.        ", "G2S       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x174f, 0xa311, 0, "ASUSTeK Computer Inc.        ", "A3F       ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED },
  { 0x5986, 0x0200, 0, "LENOVO", "SPEEDY    ",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0, NULL, NULL, NULL,
    "Lenovo IdeaPad Y510" },
  { 0x5986, 0x0205, 0, "LENOVO", "Base Board Product Name",
    V4LCONTROL_HFLIPPED|V4LCONTROL_VFLIPPED, 0, NULL, NULL, NULL,
    "Lenovo IdeaPad U330" },

/* Second: devices which should use some software processing by default */
  /* Pac207 based devices */
  { 0x041e, 0x4028, 0,    NULL, NULL, V4LCONTROL_WANTS_WB, 1500 },
  { 0x093a, 0x2460, 0x1f, NULL, NULL, V4LCONTROL_WANTS_WB, 1500 },
  { 0x145f, 0x013a, 0,    NULL, NULL, V4LCONTROL_WANTS_WB, 1500 },
  { 0x2001, 0xf115, 0,    NULL, NULL, V4LCONTROL_WANTS_WB, 1500 },
  /* Pac7302 based devices */
  { 0x093a, 0x2620, 0x0f, NULL, NULL,
    V4LCONTROL_ROTATED_90_JPEG|V4LCONTROL_WANTS_WB },
  { 0x06f8, 0x3009, 0,    NULL, NULL,
    V4LCONTROL_ROTATED_90_JPEG|V4LCONTROL_WANTS_WB },
  /* Pac7311 based devices */
  { 0x093a, 0x2600, 0x0f, NULL, NULL, V4LCONTROL_WANTS_WB },
  /* sq905 devices */
  { 0x2770, 0x9120, 0,    NULL, NULL, V4LCONTROL_WANTS_WB },
  /* spca561 revison 12a devices */
  { 0x041e, 0x403b, 0,    NULL, NULL, V4LCONTROL_WANTS_WB_AUTOGAIN },
  { 0x046d, 0x0928, 7,    NULL, NULL, V4LCONTROL_WANTS_WB_AUTOGAIN },
  /* logitech quickcam express stv06xx + pb0100 */
  { 0x046d, 0x0840, 0,    NULL, NULL, V4LCONTROL_WANTS_WB },
  /* logitech quickcam messenger variants, st6422 */
  { 0x046d, 0x08f0, 0,    NULL, NULL, V4LCONTROL_WANTS_AUTOGAIN },
  { 0x046d, 0x08f5, 0,    NULL, NULL, V4LCONTROL_WANTS_AUTOGAIN },
  { 0x046d, 0x08f6, 0,    NULL, NULL, V4LCONTROL_WANTS_AUTOGAIN },
  { 0x046d, 0x08da, 0,    NULL, NULL, V4LCONTROL_WANTS_AUTOGAIN },
};

static const struct v4l2_queryctrl fake_controls[];

static void v4lcontrol_get_dmi_string(const char *string, char *buf, int size)
{
  FILE *f;
  char *s, sysfs_name[512];

  snprintf(sysfs_name, sizeof(sysfs_name),
	   "/sys/class/dmi/id/%s", string);
  f = fopen(sysfs_name, "r");
  if (!f) {
    /* Try again with a different sysfs path, not sure if this is needed
       but we used to look under /sys/devices/virtual/dmi/id in older
       libv4l versions, but this did not work with some kernels */
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/devices/virtual/dmi/id/%s", string);
    f = fopen(sysfs_name, "r");
    if (!f) {
      buf[0] = 0;
      return;
    }
  }

  s = fgets(buf, size, f);
  if (s)
    s[strlen(s) - 1] = 0;
  fclose(f);
}

static void v4lcontrol_init_flags(struct v4lcontrol_data *data)
{
  struct stat st;
  FILE *f;
  char sysfs_name[512];
  unsigned short vendor_id = 0;
  unsigned short product_id = 0;
  char dmi_system_vendor[512], dmi_system_name[512], dmi_system_version[512];
  char dmi_board_vendor[512], dmi_board_name[512], dmi_board_version[512];
  int i, minor;
  char c, *s, buf[32];
  struct v4l2_input input;

  if ((SYS_IOCTL(data->fd, VIDIOC_G_INPUT, &input.index) == 0) &&
      (SYS_IOCTL(data->fd, VIDIOC_ENUMINPUT, &input) == 0)) {
    if (input.status & V4L2_IN_ST_HFLIP)
      data->flags |= V4LCONTROL_HFLIPPED;
    if (input.status & V4L2_IN_ST_VFLIP)
      data->flags |= V4LCONTROL_VFLIPPED;
  }

  if (fstat(data->fd, &st) || !S_ISCHR(st.st_mode)) {
    return; /* Should never happen */
  }

  /* <Sigh> find ourselve in sysfs */
  for (i = 0; i < 256; i++) {
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/dev", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      continue;

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (s && sscanf(buf, "%*d:%d%c", &minor, &c) == 2 && c == '\n' &&
	minor == minor(st.st_rdev))
      break;
  }
  if (i == 256)
    return; /* Not found, sysfs not mounted? */

  /* Get vendor and product ID */
  snprintf(sysfs_name, sizeof(sysfs_name),
	   "/sys/class/video4linux/video%d/device/modalias", i);
  f = fopen(sysfs_name, "r");
  if (f) {
    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s ||
	sscanf(s, "usb:v%4hxp%4hx%c", &vendor_id, &product_id, &c) != 3 ||
	c != 'd')
      return; /* Not an USB device */
  } else {
    /* Try again assuming the device link points to the usb
       device instead of the usb interface (bug in older versions
       of gspca) */

    /* Get product ID */
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/device/idVendor", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      return; /* Not an USB device (or no sysfs) */

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s || sscanf(s, "%04hx%c", &vendor_id, &c) != 2 || c != '\n')
      return; /* Should never happen */

    /* Get product ID */
    snprintf(sysfs_name, sizeof(sysfs_name),
	     "/sys/class/video4linux/video%d/device/idProduct", i);
    f = fopen(sysfs_name, "r");
    if (!f)
      return; /* Should never happen */

    s = fgets(buf, sizeof(buf), f);
    fclose(f);

    if (!s || sscanf(s, "%04hx%c", &product_id, &c) != 2 || c != '\n')
      return; /* Should never happen */
  }

  /* Get DMI board and system strings */
  v4lcontrol_get_dmi_string("sys_vendor", dmi_system_vendor,
			    sizeof(dmi_system_vendor));
  v4lcontrol_get_dmi_string("product_name", dmi_system_name,
			    sizeof(dmi_system_name));
  v4lcontrol_get_dmi_string("product_version", dmi_system_version,
			    sizeof(dmi_system_version));

  v4lcontrol_get_dmi_string("board_vendor", dmi_board_vendor,
			    sizeof(dmi_board_vendor));
  v4lcontrol_get_dmi_string("board_name", dmi_board_name,
			    sizeof(dmi_board_name));
  v4lcontrol_get_dmi_string("board_version", dmi_board_version,
			    sizeof(dmi_board_version));

  for (i = 0; i < ARRAY_SIZE(v4lcontrol_flags); i++)
    if (v4lcontrol_flags[i].vendor_id == vendor_id &&
	v4lcontrol_flags[i].product_id ==
	  (product_id & ~v4lcontrol_flags[i].product_mask) &&

	(v4lcontrol_flags[i].dmi_system_vendor == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_system_vendor, dmi_system_vendor)) &&
	(v4lcontrol_flags[i].dmi_system_name == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_system_name, dmi_system_name)) &&
	(v4lcontrol_flags[i].dmi_system_version == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_system_version, dmi_system_version)) &&

	(v4lcontrol_flags[i].dmi_board_vendor == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_board_vendor, dmi_board_vendor)) &&
	(v4lcontrol_flags[i].dmi_board_name == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_board_name, dmi_board_name)) &&
	(v4lcontrol_flags[i].dmi_board_version == NULL ||
	 !strcmp(v4lcontrol_flags[i].dmi_board_version, dmi_board_version))) {
      data->flags |= v4lcontrol_flags[i].flags;
      data->flags_info = &v4lcontrol_flags[i];
      break;
    }
}

struct v4lcontrol_data *v4lcontrol_create(int fd, int always_needs_conversion)
{
  int shm_fd;
  int i, rc, init = 0;
  char *s, shm_name[256], pwd_buf[1024];
  struct v4l2_capability cap;
  struct v4l2_queryctrl ctrl;
  struct passwd pwd, *pwd_p;

  struct v4lcontrol_data *data = calloc(1, sizeof(struct v4lcontrol_data));

  if (!data) {
    fprintf(stderr, "libv4lcontrol: error: out of memory!\n");
    return NULL;
  }

  data->fd = fd;

  v4lcontrol_init_flags(data);

  /* Allow overriding through environment */
  if ((s = getenv("LIBV4LCONTROL_FLAGS")))
    data->flags = strtol(s, NULL, 0);

  ctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
  if (SYS_IOCTL(data->fd, VIDIOC_QUERYCTRL, &ctrl) == 0)
    data->priv_flags |= V4LCONTROL_SUPPORTS_NEXT_CTRL;

  /* If the device always needs conversion, we can add fake controls at no cost
     (no cost when not activated by the user that is) */
  if (always_needs_conversion || v4lcontrol_needs_conversion(data)) {
    for (i = 0; i < V4LCONTROL_AUTO_ENABLE_COUNT; i++) {
      ctrl.id = fake_controls[i].id;
      rc = SYS_IOCTL(data->fd, VIDIOC_QUERYCTRL, &ctrl);
      if (rc == -1 || (rc == 0 && (ctrl.flags & V4L2_CTRL_FLAG_DISABLED)))
	data->controls |= 1 << i;
    }
  }

  if (data->flags & V4LCONTROL_WANTS_AUTOGAIN)
    data->controls |= 1 << V4LCONTROL_AUTOGAIN |
		      1 << V4LCONTROL_AUTOGAIN_TARGET;

  /* Allow overriding through environment */
  if ((s = getenv("LIBV4LCONTROL_CONTROLS")))
    data->controls = strtol(s, NULL, 0);

  if (data->controls == 0)
    return data; /* No need to create a shared memory segment */

  if (SYS_IOCTL(fd, VIDIOC_QUERYCAP, &cap)) {
    perror("libv4lcontrol: error querying device capabilities");
    goto error;
  }

  if (getpwuid_r(geteuid(), &pwd, pwd_buf, sizeof(pwd_buf), &pwd_p) == 0) {
    snprintf(shm_name, 256, "/libv4l-%s:%s:%s", pwd.pw_name,
	     cap.bus_info, cap.card);
  } else {
    perror("libv4lcontrol: error getting username using uid instead");
    snprintf(shm_name, 256, "/libv4l-%lu:%s:%s", (unsigned long)geteuid(),
	     cap.bus_info, cap.card);
  }

  /* / is not allowed inside shm names */
  for (i = 1; shm_name[i]; i++)
    if (shm_name[i] == '/')
      shm_name[i] = '-';

  /* Open the shared memory object identified by shm_name */
  if ((shm_fd = shm_open(shm_name, (O_CREAT | O_EXCL | O_RDWR),
			 (S_IREAD | S_IWRITE))) >= 0)
    init = 1;
  else
    shm_fd = shm_open(shm_name, O_RDWR, (S_IREAD | S_IWRITE));

  if (shm_fd >= 0) {
    /* Set the shared memory size */
    ftruncate(shm_fd, V4LCONTROL_SHM_SIZE);

    /* Retreive a pointer to the shm object */
    data->shm_values = mmap(NULL, V4LCONTROL_SHM_SIZE, (PROT_READ | PROT_WRITE),
			    MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    if (data->shm_values == MAP_FAILED) {
      perror("libv4lcontrol: error shm mmap failed");
      data->shm_values = NULL;
    }
  } else
    perror("libv4lcontrol: error creating shm segment failed");

  /* Fall back to malloc */
  if (data->shm_values == NULL) {
    fprintf(stderr,
	    "libv4lcontrol: falling back to malloc-ed memory for controls\n");
    data->shm_values = malloc(V4LCONTROL_SHM_SIZE);
    if (!data->shm_values) {
      fprintf(stderr, "libv4lcontrol: error: out of memory!\n");
      goto error;
    }
    init = 1;
    data->priv_flags |= V4LCONTROL_MEMORY_IS_MALLOCED;
  }

  if (init) {
    /* Initialize the new shm object we created */
    memset(data->shm_values, 0, V4LCONTROL_SHM_SIZE);

    for (i = 0; i < V4LCONTROL_COUNT; i++)
      data->shm_values[i] = fake_controls[i].default_value;

    if (data->flags & V4LCONTROL_WANTS_WB)
      data->shm_values[V4LCONTROL_WHITEBALANCE] = 1;

    if (data->flags_info && data->flags_info->default_gamma)
      data->shm_values[V4LCONTROL_GAMMA] = data->flags_info->default_gamma;
  }

  return data;

error:
  free(data);
  return NULL;
}

void v4lcontrol_destroy(struct v4lcontrol_data *data)
{
  if (data->controls) {
    if (data->priv_flags & V4LCONTROL_MEMORY_IS_MALLOCED)
      free(data->shm_values);
    else
      munmap(data->shm_values, V4LCONTROL_SHM_SIZE);
  }
  free(data);
}

static const struct v4l2_queryctrl fake_controls[V4LCONTROL_COUNT] = {
{
  .id = V4L2_CID_AUTO_WHITE_BALANCE,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Whitebalance (software)",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 0,
  .flags = 0
},
{
  .id = V4L2_CID_HFLIP,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Horizontal flip (sw)",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 0,
  .flags = 0
},
{
  .id = V4L2_CID_VFLIP,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Vertical flip (sw)",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 0,
  .flags = 0
},
{
  .id = V4L2_CID_GAMMA,
  .type = V4L2_CTRL_TYPE_INTEGER,
  .name =  "Gamma (software)",
  .minimum = 500,  /* == 0.5 */
  .maximum = 3000, /* == 3.0 */
  .step = 1,
  .default_value = 1000, /* == 1.0 */
  .flags = 0
},
{}, /* Dummy place holder for V4LCONTROL_AUTO_ENABLE_COUNT */
{
  .id = V4L2_CID_AUTOGAIN,
  .type = V4L2_CTRL_TYPE_BOOLEAN,
  .name =  "Auto Gain (software)",
  .minimum = 0,
  .maximum = 1,
  .step = 1,
  .default_value = 1,
  .flags = 0
},
{
  .id = V4L2_CTRL_CLASS_USER + 0x2000, /* FIXME */
  .type = V4L2_CTRL_TYPE_INTEGER,
  .name =  "Auto Gain target",
  .minimum = 0,
  .maximum = 255,
  .step = 1,
  .default_value = 100,
  .flags = 0
},
};

static void v4lcontrol_copy_queryctrl(struct v4lcontrol_data *data,
  struct v4l2_queryctrl *ctrl, int i)
{
  memcpy(ctrl, &fake_controls[i], sizeof(struct v4l2_queryctrl));

  /* Hmm, not pretty */
  if (ctrl->id == V4L2_CID_AUTO_WHITE_BALANCE &&
      (data->flags & V4LCONTROL_WANTS_WB))
    ctrl->default_value = 1;

  if (ctrl->id == V4L2_CID_GAMMA && data->flags_info &&
      data->flags_info->default_gamma)
    ctrl->default_value = data->flags_info->default_gamma;
}

int v4lcontrol_vidioc_queryctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_queryctrl *ctrl = arg;
  int retval;
  __u32 orig_id=ctrl->id;

  /* if we have an exact match return it */
  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      v4lcontrol_copy_queryctrl(data, ctrl, i);
      return 0;
    }

  /* find out what the kernel driver would respond. */
  retval = SYS_IOCTL(data->fd, VIDIOC_QUERYCTRL, arg);

  if ((data->priv_flags & V4LCONTROL_SUPPORTS_NEXT_CTRL) &&
      (orig_id & V4L2_CTRL_FLAG_NEXT_CTRL)) {
    /* If the hardware has no more controls check if we still have any
       fake controls with a higher id then the hardware's highest */
    if (retval)
      ctrl->id = V4L2_CTRL_FLAG_NEXT_CTRL;

    /* If any of our controls have an id > orig_id but less than
       ctrl->id then return that control instead. Note we do not
       break when we have a match, but keep iterating, so that
       we end up with the fake ctrl with the lowest CID > orig_id. */
    for (i = 0; i < V4LCONTROL_COUNT; i++)
      if ((data->controls & (1 << i)) &&
	  (fake_controls[i].id > (orig_id & ~V4L2_CTRL_FLAG_NEXT_CTRL)) &&
	  (fake_controls[i].id <= ctrl->id)) {
	v4lcontrol_copy_queryctrl(data, ctrl, i);
	retval = 0;
      }
  }

  return retval;
}

int v4lcontrol_vidioc_g_ctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_control *ctrl = arg;

  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      ctrl->value = data->shm_values[i];
      return 0;
    }

  return SYS_IOCTL(data->fd, VIDIOC_G_CTRL, arg);
}

int v4lcontrol_vidioc_s_ctrl(struct v4lcontrol_data *data, void *arg)
{
  int i;
  struct v4l2_control *ctrl = arg;

  for (i = 0; i < V4LCONTROL_COUNT; i++)
    if ((data->controls & (1 << i)) &&
	ctrl->id == fake_controls[i].id) {
      if (ctrl->value > fake_controls[i].maximum ||
	  ctrl->value < fake_controls[i].minimum) {
	errno = EINVAL;
	return -1;
      }

      data->shm_values[i] = ctrl->value;
      return 0;
    }

  return SYS_IOCTL(data->fd, VIDIOC_S_CTRL, arg);
}

int v4lcontrol_get_flags(struct v4lcontrol_data *data)
{
  return data->flags;
}

int v4lcontrol_get_ctrl(struct v4lcontrol_data *data, int ctrl)
{
  if (data->controls & (1 << ctrl)) {
    /* Special case for devices with flipped input */
    if ((ctrl == V4LCONTROL_HFLIP && (data->flags & V4LCONTROL_HFLIPPED)) ||
	(ctrl == V4LCONTROL_VFLIP && (data->flags & V4LCONTROL_VFLIPPED)))
      return !data->shm_values[ctrl];

    return data->shm_values[ctrl];
  }

  return 0;
}

int v4lcontrol_controls_changed(struct v4lcontrol_data *data)
{
  int res;

  if (!data->controls)
    return 0;

  res = memcmp(data->shm_values, data->old_values,
	       V4LCONTROL_COUNT * sizeof(unsigned int));

  memcpy(data->old_values, data->shm_values,
	 V4LCONTROL_COUNT * sizeof(unsigned int));

  return res;
}

/* See the comment about this in libv4lconvert.h */
int v4lcontrol_needs_conversion(struct v4lcontrol_data *data) {
  return data->flags || data->controls;
}
