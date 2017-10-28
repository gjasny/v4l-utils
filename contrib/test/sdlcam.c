/*
   Digital still camera.

   SDL based, suitable for camera phone such as Nokia N900. In
   particular, we support focus, gain and exposure control, but not
   aperture control or lens zoom.

   Copyright 2017 Pavel Machek, LGPL

   Needs sdl2, sdl2_image libraries. sudo aptitude install libsdl2-dev
   libsdl2-image-dev on Debian systems.
*/

#include <time.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <jpeglib.h>

#include "libv4l2.h"
#include <linux/videodev2.h>
#include "libv4l-plugin.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#define PICDIR "."
#define SX 2592
#define SY 1968
#define SIZE SX*SY*3

static void fmt_print(struct v4l2_format *fmt)
{
	int f;
	printf("Format: %dx%d. ", fmt->fmt.pix.width, fmt->fmt.pix.height);
	printf("%x ", fmt->fmt.pix.pixelformat);
	f = fmt->fmt.pix.pixelformat;
	for (int i=0; i<4; i++) {
		printf("%c", f & 0xff);
		f >>= 8;
	}
	printf("\n");
}

static double dtime(void)
{
	static double start = 0.0;
	struct timeval now;

	gettimeofday(&now, NULL);

	double n = now.tv_sec + now.tv_usec / 1000000.;
	if (!start)
		start = n;
	return n - start;
}

static long v4l2_g_ctrl(int fd, long id)
{
	int res;
	struct v4l2_control ctrl;
	ctrl.id = id;
	ctrl.value = 0;
	res = v4l2_ioctl(fd, VIDIOC_G_CTRL, &ctrl);
	if (res < 0)
		printf("Get control %lx failed\n", id);
	return ctrl.value;
}

static int v4l2_s_ctrl(int fd, long id, long value)
{
	int res;
	struct v4l2_control ctrl;
	ctrl.id = id;
	ctrl.value = value;
	res = v4l2_ioctl(fd, VIDIOC_S_CTRL, &ctrl);
	if (res < 0)
		printf("Set control %lx %ld failed\n", id, value);
	return res;
}

static int v4l2_set_focus(int fd, int diopt)
{
	if (v4l2_s_ctrl(fd, V4L2_CID_FOCUS_ABSOLUTE, diopt) < 0) {
		printf("Could not set focus\n");
	}
	return 0;
}

struct dev_info {
	int fd;
	struct v4l2_format fmt;

	unsigned char buf[SIZE];
	int debug;
#define D_TIMING 1
};

struct sdl {
	SDL_Window *window;
	SDL_Surface *screen, *liveview;

	int wx, wy; /* Window size */
	int sx, sy; /* Live view size */
	int bx, by; /* Border size */
	int nx, ny; /* Number of buttons */
	float factor;

	/* These should go separately */
	int do_focus, do_exposure, do_flash, do_white, do_big, do_full;
	double zoom;
	double focus_min;

	int slider_mode;
#define M_BIAS 0
#define M_EXPOSURE 1
#define M_GAIN 2
#define M_FOCUS 3
#define M_NUM 4

	int fd;

	struct dev_info *dev;
};

typedef struct {
	uint8_t r, g, b, alpha;
} pixel;

#define d_raw 1

static void sfc_put_pixel(SDL_Surface* liveview, int x, int y, pixel *p)
{
	Uint32* p_liveview = (Uint32*)liveview->pixels;
	p_liveview += y*liveview->w+x;
	*p_liveview = SDL_MapRGBA(liveview->format, p->r, p->g, p->b, p->alpha);
}

static void sdl_begin_paint(struct sdl *m)
{
	/* Fill the surface white */
	SDL_FillRect(m->liveview, NULL, SDL_MapRGB( m->liveview->format, 0, 0, 0 ));

	SDL_LockSurface(m->liveview);
}

static void sdl_finish_paint(struct sdl *m) {
	SDL_UnlockSurface(m->liveview);
	SDL_Rect rcDest = { m->bx, m->by, m->sx, m->sy };

	SDL_BlitSurface(m->liveview, NULL, m->screen, &rcDest);
	SDL_UpdateWindowSurfaceRects(m->window, &rcDest, 1);
}

static void sdl_paint_image(struct sdl *m, char **xpm, int x, int y) {
	SDL_Surface *image = IMG_ReadXPMFromArray(xpm);
	if (!image) {
		printf("IMG_Load: %s\n", IMG_GetError());
		exit(1);
	}

	int x_pos = x - image->w/2, y_pos = y - image->h/2;

	SDL_Rect rcDest = { x_pos, y_pos, image->w, image->h };
	int r = SDL_BlitSurface(image, NULL, m->screen, &rcDest);

	if (r) {
		printf("Error blitting: %s\n", SDL_GetError());
		exit(1);
	}
	SDL_FreeSurface(image);
}

static void cam_exposure_limits(struct sdl *m, struct v4l2_queryctrl *qctrl)
{
	qctrl->id = V4L2_CID_EXPOSURE_ABSOLUTE;

	if (v4l2_ioctl(m->fd, VIDIOC_QUERYCTRL, qctrl)) {
		printf("Exposure absolute limits failed\n");
		exit(1);
	}

	/* Minimum of 11300 gets approximately same range on ISO and
	 * exposure axis. */
	if (qctrl->minimum < 500)
		qctrl->minimum = 500;
}

static void cam_set_exposure(struct sdl *m, double v)
{
	int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
	double res;
	double range;
	struct v4l2_queryctrl qctrl = { .id = cid };
	struct v4l2_control ctrl = { .id = cid };

	cam_exposure_limits(m, &qctrl);

	if (v4l2_ioctl(m->fd, VIDIOC_G_CTRL, &ctrl)) {
		printf("Can't get exposure parameters\n");
		exit(1);
	}

	range = log2(qctrl.maximum) - log2(qctrl.minimum);
	res = log2(qctrl.minimum) + v*range;
	res = exp2(res);

	v4l2_s_ctrl(m->fd, V4L2_CID_EXPOSURE_ABSOLUTE, res);
}

static double cam_convert_exposure(struct sdl *m, int v)
{
	int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
	double res;
	struct v4l2_queryctrl qctrl = { .id = cid };

	cam_exposure_limits(m, &qctrl);
	res = (log2(v) - log2(qctrl.minimum)) / (log2(qctrl.maximum) - log2(qctrl.minimum));

	return res;
}

static double cam_get_exposure(struct sdl *m)
{
	int cid = V4L2_CID_EXPOSURE_ABSOLUTE;
	struct v4l2_control ctrl = { .id = cid };

	if (v4l2_ioctl(m->fd, VIDIOC_G_CTRL, &ctrl))
		return -1;

	return cam_convert_exposure(m, ctrl.value);
}

static int cam_get_gain_iso(struct dev_info *dev)
{
	double x;

	x = v4l2_g_ctrl(dev->fd, 0x00980913);
	x = (x / 10); /* EV */
	x = exp2(x);
	x = 100*x;
	return x;
}

static void cam_set_focus(struct sdl *m, double val)
{
	v4l2_s_ctrl(m->fd, V4L2_CID_FOCUS_ABSOLUTE, (val * m->focus_min) * (1023. / 20));
}

static double cam_convert_focus(struct sdl *m, double diopter)
{
	return diopter / m->focus_min;
}

static double cam_get_focus_diopter(struct sdl *m)
{
	return (v4l2_g_ctrl(m->fd, V4L2_CID_FOCUS_ABSOLUTE) * 20.) / 1023.;
}

static double cam_get_focus(struct sdl *m)
{
	return cam_convert_focus(m, cam_get_focus_diopter(m));
}

static void sdl_line_color(struct sdl *m, int x1, int y1, int x2, int y2, Uint32 color)
{
	SDL_Rect rcDest = { x1, y1, x2-x1+1, y2-y1+1};

	SDL_FillRect(m->screen, &rcDest, color);
}

static void sdl_line(struct sdl *m, int x1, int y1, int x2, int y2)
{
	sdl_line_color(m, x1, y1, x2, y2, SDL_MapRGB( m->liveview->format, 255, 255, 255 ));
}

static void sdl_digit(struct sdl *m, int x, int y, int s, int i)
{
	unsigned char gr[] = { 0x5f, 0x0a, 0x76, 0x7a, 0x2b,
			       0x79, 0x7d, 0x1a, 0x7f, 0x7b };
	unsigned char g = gr[i];
	/*
              10
	    01  02
              20
            04  08
	      40
	*/

	if (g & 1) sdl_line(m, x, y, x, y+s);
	if (g & 2) sdl_line(m, x+s, y, x+s, y+s);
	if (g & 4) sdl_line(m, x, y+s, x, y+s+s);
	if (g & 8) sdl_line(m, x+s, y+s, x+s, y+s+s);

	if (g & 0x10) sdl_line(m, x, y, x+s, y);
	if (g & 0x20) sdl_line(m, x, y+s, x+s, y+s);
	if (g & 0x40) sdl_line(m, x, y+s+s, x+s, y+s+s);
}

static void sdl_number(struct sdl *m, int x, int y, int s, int digits, int i)
{
	int tot = s * 5;
	x += tot * digits;
	for (int j=0; j<digits; j++) {
		sdl_digit(m, x+2, y+4, s*3, i%10);
		i /= 10;
		x -= tot;
	}
}

static void sdl_icon_pos(struct sdl *m, int *x, int *y, double val)
{
	*x = m->wx - 10;
	*y = val * m->wy;
}

static void sdl_paint_ui_iso(struct sdl *m, double y_, int i)
{
	static char *iso_xpm[] = {
		"16 12 2 1",
		"x c #ffffff",
		". c #000000",
		"................",
		"................",
		"................",
		".x..xx..x.......",
		".x.x...x.x......",
		".x..x..x.x......",
		".x...x.x.x......",
		".x.xx...x.......",
		"................",
		"................",
		"................",
		"................",
	};

	int x, y;

	sdl_icon_pos(m, &x, &y, y_);
	sdl_number(m, x-35, y-10, 1, 3, i);
	sdl_paint_image(m, iso_xpm, x, y);
}

static void sdl_paint_ui_exposure(struct sdl *m, int t)
{
	static char *time_1_xpm[] = {
		"16 12 2 1",
		"x c #ffffff",
		". c #000000",
		"......x.........",
		".....x..........",
		"....x...........",
		"...x............",
		"................",
		".xxx.xxxx.xxxx..",
		"x....x....x.....",
		".xx..xxx..x.....",
		"...x.x....x.....",
		"...x.x....x.....",
		"xxx..xxxx.xxxx..",
		"................",
	};
	int x, y;

	sdl_icon_pos(m, &x, &y, cam_convert_exposure(m, 1000000/t));
	sdl_number(m, x-35, y-10, 1, 3, t);
	sdl_paint_image(m, time_1_xpm, x, y);
}

static void sdl_paint_boolean(struct sdl *m, char **image, int x, int y, int yes)
{
	static char *not_xpm[] = {
		"16 12 2 1",
		"x c #ffffff",
		". c #000000",
		"......xxxxx.....",
		"....xx.....xx...",
		"...x.........x..",
		"..x........xx.x.",
		"..x......xx...x.",
		".x.....xx......x",
		".x...xx........x",
		"..xxx.........x.",
		"..x...........x.",
		"...x.........x..",
		"....xx.....xx...",
		"......xxxxx.....",
	};

	sdl_paint_image(m, image, x, y);
	if (!yes)
		sdl_paint_image(m, not_xpm,  16+x, y);
}

static void sdl_paint_button(struct sdl *m, char **image, int x, int y, int yes)
{
	int bsx = m->wx/m->nx;
	int bsy = m->wy/m->ny;
	if (x < 0)
		x += m->nx;
	if (y < 0)
		y += m->ny;
	x = bsx/2 + x*bsx;
	y = bsy/2 + y*bsy;
	sdl_paint_boolean(m, image, x, y, yes);
}

static void sdl_paint_ui_focus(struct sdl *m, int f)
{
	static char *cm_xpm[] = {
		"16 9 2 1",
		"# c #ffffff",
		". c #000000",
		"................",
		"................",
		"................",
		"....###..#.#.##.",
		"...#.....##.#..#",
		"...#.....#..#..#",
		"...#.....#..#..#",
		"....###..#..#..#",
		"................",
	};
	double dioptr = 1/(f/100.);
	int x, y;

	if (dioptr > m->focus_min)
		return;

	sdl_icon_pos(m, &x, &y, cam_convert_focus(m, dioptr));
	sdl_paint_image(m, cm_xpm, x, y);
	sdl_number(m, x-12, y-15, 1, 3, f);
}

static void sdl_paint_ui_bias(struct sdl *m, double v)
{
	static char *bias_xpm[] = {
		"16 12 2 1",
		"# c #ffffff",
		". c #000000",
		"...#............",
		"...#.......#....",
		".#####....#.....",
		"...#.....#......",
		"...#....#.......",
		".......#........",
		"......#...#####.",
		".....#..........",
		"....#...........",
		"...#............",
		"................",
		"................",
	};
	int x, y;

	sdl_icon_pos(m, &x, &y, v);
	sdl_paint_image(m, bias_xpm, x, y);
}

static void sdl_paint_slider(struct sdl *m)
{
	switch (m->slider_mode) {
	case M_BIAS:
		sdl_paint_ui_bias(m, 0.5);
		return;
	case M_EXPOSURE:
		sdl_paint_ui_exposure(m, 10);
		sdl_paint_ui_exposure(m, 100);
		sdl_paint_ui_exposure(m, 999);
		return;
	case M_GAIN:
		sdl_paint_ui_iso(m, 0/4., 100);
		sdl_paint_ui_iso(m, 1/4., 200);
		sdl_paint_ui_iso(m, 2/4., 400);
		sdl_paint_ui_iso(m, 3/4., 800);
		return;
	case M_FOCUS:
		sdl_paint_ui_focus(m, 100);
		sdl_paint_ui_focus(m, 40);
		sdl_paint_ui_focus(m, 25);
		sdl_paint_ui_focus(m, 16);
		sdl_paint_ui_focus(m, 10);
		sdl_paint_ui_focus(m, 8);
		sdl_paint_ui_focus(m, 6);
		return;
	}
}

static void sdl_paint_ui(struct sdl *m)
{
	static char *wait_xpm[] = {
		"16 9 2 1",
		"# c #ffffff",
		". c #000000",
		"....########....",
		".....#....#.....",
		".....#....#.....",
		"......#..#......",
		".......##.......",
		"......#..#......",
		".....#....#.....",
		".....#....#.....",
		"....########....",
	};

	static char *ok_xpm[] = {
		"16 9 2 1",
		"# c #ffffff",
		". c #000000",
		"...............#",
		"............###.",
		"..........##....",
		"#.......##......",
		".#.....#........",
		"..#...#.........",
		"..#..#..........",
		"...##...........",
		"...#............",
	};

	static char *exit_xpm[] = {
		"16 9 2 1",
		"x c #ffffff",
		". c #000000",
		"....x......x....",
		".....x....x.....",
		"......x..x......",
		".......xx.......",
		".......xx.......",
		"......x..x......",
		".....x....x.....",
		"....x......x....",
		"................",
	};

	static char *af_xpm[] = {
		"16 12 2 1",
		"x c #ffffff",
		". c #000000",
		"................",
		"................",
		".....xxxxxxx....",
		".....x..........",
		".....x..........",
		".x...xxxxx......",
		"x.x..x..........",
		"xxx..x..........",
		"x.x..x..........",
		"x.x..x..........",
		"................",
		"................",
	};

	static char *ae_xpm[] = {
		"16 12 2 1",
		"x c #ffffff",
		". c #000000",
		"................",
		"................",
		".....xxxxxxx....",
		".....x..........",
		".....x..........",
		".x...xxxxx......",
		"x.x..x..........",
		"xxx..x..........",
		"x.x..x..........",
		"x.x..xxxxxxx....",
		"................",
		"................",
	};

	static char *focus_xpm[] = {
		"16 12 2 1",
		"# c #ffffff",
		". c #000000",
		"................",
		"................",
		"###..........###",
		"#..............#",
		"#.....####.....#",
		".....#....#.....",
		".....#....#.....",
		"#.....####.....#",
		"#..............#",
		"###..........###",
		"................",
		"................",
	};

	static char *flash_xpm[] = {
		"16 12 2 1",
		"# c #ffffff",
		". c #000000",
		"................",
		"..........#.....",
		"........##......",
		".......##.......",
		"......##........",
		".....########...",
		"..........##....",
		".......#.##.....",
		".......###......",
		".......####.....",
		"................",
		"................",
	};

	static char *wb_xpm[] = {
		"16 12 2 1",
		"# c #ffffff",
		". c #000000",
		"................",
		"................",
		"................",
		"#.....#..####...",
		"#.....#..#...#..",
		"#..#..#..####...",
		"#..#..#..#...#..",
		".##.##...####...",
		"................",
		"................",
		"................",
		"................",
	};
/* Template for more xpm's:
	static char *empty_xpm[] = {
		"16 12 2 1",
		"# c #ffffff",
		". c #000000",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
		"................",
	};
*/
	SDL_FillRect(m->screen, NULL, SDL_MapRGB( m->liveview->format, 0, 0, 0 ));

	{
		/* Paint grid */
		int x, y;
		int nx = m->nx;
		for (x=1; x<nx; x++) {
			int x_ = (x*m->wx)/nx;
			sdl_line_color(m, x_, 1, x_, m->wy-1, SDL_MapRGB( m->liveview->format, 40, 40, 40 ));
		}

		int ny = m->ny;
		for (y=1; y<nx; y++) {
			int y_ = (y*m->wy)/ny;
			sdl_line_color(m, 1, y_, m->wx-1, y_, SDL_MapRGB( m->liveview->format, 40, 40, 40 ));
		}

	}

	sdl_paint_image(m, wait_xpm,  m->wx/2,     m->wy/2);


	sdl_paint_button(m, af_xpm, 0,  0, m->do_focus);
	sdl_paint_button(m, ae_xpm, -1, 0, m->do_exposure);

	sdl_paint_button(m, exit_xpm,   0, -1, 1);
	sdl_paint_button(m, flash_xpm,  1, -1, m->do_flash);
	sdl_paint_button(m, wb_xpm,     2, -1, m->do_white);
	sdl_paint_button(m, focus_xpm, -1, -2, 1);
	sdl_paint_button(m, ok_xpm,    -1, -1, 1);

	sdl_paint_slider(m);
	SDL_UpdateWindowSurface(m->window);
}

static double usec_to_time(double v)
{
	return 1/(v*.000001);
}

static void sdl_status(struct sdl *m, double avg)
{
	int ox = 0;
	int oy = m->wy/2;
	int s = 3;
	SDL_Rect rcDest = { ox, oy, s*25, s*40 };

	SDL_FillRect(m->screen, &rcDest, SDL_MapRGB( m->liveview->format, 30, 30, 30 ));

	sdl_number(m, ox, oy, s, 3, avg*1000);
	oy += s*10;

	{
		double focus, gain, exposure;

		exposure = v4l2_g_ctrl(m->fd, V4L2_CID_EXPOSURE_ABSOLUTE);
		gain = cam_get_gain_iso(m->dev);
		focus = cam_get_focus_diopter(m);

		double x = usec_to_time(exposure);
		if (x > 999) x = 999;
		sdl_number(m, ox, oy, s, 3, x);

		oy += s*10;
		if (gain > 999)
			gain = 999;
		sdl_number(m, ox, oy, s, 3, gain);

		oy += s*10;
		x = focus; /* diopters */
		if (x == 0)
			x = 999;
		else
			x = 100/x; /* centimeters */
		sdl_number(m, ox, oy, s, 3, x);
	}

	SDL_UpdateWindowSurfaceRects(m->window, &rcDest, 1);
}
static pixel buf_pixel(struct v4l2_format *fmt, unsigned char *buf, int x, int y)
{
	pixel p = { 0, 0, 0, 0 };
	int pos = x + y*fmt->fmt.pix.width;

	p.alpha = 128;

	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_SGRBG10:
		{
			short *b2 = (void *)buf;
			x &= ~1;
			y &= ~1;
			p.g = b2[x + y*fmt->fmt.pix.width] /4;
			p.r = b2[x + y*fmt->fmt.pix.width+1] /4;
			p.b = b2[x + (y+1)*fmt->fmt.pix.width] /4;
		}
		break;

	case V4L2_PIX_FMT_RGB24:
		pos *= 3;
		p.r = buf[pos];
		p.g = buf[pos+1];
		p.b = buf[pos+2];
		break;

	default:
		printf("Wrong pixel format!\n");
		fmt_print(fmt);
		exit(1);
	}

	return p;
}

static char *fmt_name(struct v4l2_format *fmt)
{
	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_SGRBG10:
		return "GRBG10";
	case V4L2_PIX_FMT_RGB24:
		return "RGB24";
	default:
		return "unknown";
	}
}

static void sdl_handle_focus(struct sdl *m, float how)
{
	v4l2_set_control(m->fd, V4L2_CID_FOCUS_ABSOLUTE, 65535. * how);
}

static void sdl_key(struct sdl *m, int c)
{
	switch (c) {
	case 27: exit(1); break;
	case 'q': sdl_handle_focus(m, 0.); break;
	case 'w': sdl_handle_focus(m, 1/6.); break;
	case 'e': sdl_handle_focus(m, 1/3.); break;
	case 'r': sdl_handle_focus(m, 1/2.); break;
	case 't': sdl_handle_focus(m, 1/1); break;
	case 'y': sdl_handle_focus(m, 1/.8); break;
	case 'u': sdl_handle_focus(m, 1/.5); break;
	case 'i': sdl_handle_focus(m, 1/.2); break;
	case 'o': sdl_handle_focus(m, 1/.1); break;
	case 'p': sdl_handle_focus(m, 1/.05); break;
	default: printf("Unknown key %d / %c", c, c);
	}
}

static int sdl_render_statistics(struct sdl *m)
{
	pixel white;
	double focus, gain, exposure;

	white.r = (Uint8)0xff;
	white.g = (Uint8)0xff;
	white.b = (Uint8)0xff;
	white.alpha = (Uint8)128;

	exposure = cam_get_exposure(m);
	gain = v4l2_get_control(m->fd, 0x00980913) / 65535.;
	focus = cam_get_focus(m);

	for (int x=0; x<m->sx && x<m->sx*focus; x++)
		sfc_put_pixel(m->liveview, x, 0, &white);

	for (int y=0; y<m->sy && y<m->sy*gain; y++)
		sfc_put_pixel(m->liveview, 0, y, &white);

	for (int y=0; y<m->sy && y<m->sy*exposure; y++)
		sfc_put_pixel(m->liveview, m->sx-1, y, &white);

	for (int x=0; x<m->sx; x++)
		sfc_put_pixel(m->liveview, x, m->sy-1, &white);

	return 0;
}

static void sdl_render(struct sdl *m, unsigned char *buf, struct v4l2_format *fmt)
{
	float zoom = m->zoom;
	if (!m->window)
		return;

	sdl_begin_paint(m);
	int x0 = (fmt->fmt.pix.width - m->sx*m->factor/zoom)/2;
	int y0 = (fmt->fmt.pix.height - m->sy*m->factor/zoom)/2;

	for (int y = 0; y < m->sy; y++)
		for (int x = 0; x < m->sx; x++) {
			int x1 = x0+x*m->factor/zoom;
			int y1 = y0+y*m->factor/zoom;
			pixel p = buf_pixel(fmt, buf, x1, y1);
			p.alpha = 128;
			sfc_put_pixel(m->liveview, x, y, &p);
		}

	sdl_render_statistics(m);
	sdl_finish_paint(m);
}

static void sdl_sync_settings(struct sdl *m)
{
	printf("Autofocus: "); v4l2_s_ctrl(m->fd, V4L2_CID_FOCUS_AUTO, m->do_focus);
	printf("Autogain: " ); v4l2_s_ctrl(m->fd, V4L2_CID_AUTOGAIN, m->do_exposure);
	printf("Autowhite: "); v4l2_s_ctrl(m->fd, V4L2_CID_AUTO_WHITE_BALANCE, m->do_white);
	v4l2_s_ctrl(m->fd, 0x009c0901, m->do_flash ? 2 : 0);
}

static void sdl_init_window(struct sdl *m)
{
	if (m->do_full) {
		m->wx = 800;
		m->wy = 480;
	} else {
		m->wx = 800;
		m->wy = 429;
	}

	SDL_SetWindowFullscreen(m->window, m->do_full*SDL_WINDOW_FULLSCREEN_DESKTOP);

	m->screen = SDL_GetWindowSurface(m->window);
	if (!m->screen) {
		printf("Couldn't create screen\n");
		exit(1);
	}
}

static void sdl_init(struct sdl *m, struct dev_info *dev)
{
	m->fd = dev->fd;
	m->dev = dev;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("Could not init SDL\n");
		exit(1);
	}

	atexit(SDL_Quit);

	m->nx = 6;
	m->ny = 4;

	m->do_flash = 1;
	m->do_focus = 0;
	m->do_exposure = 1;
	m->focus_min = 5;
	m->do_white = 0;
	m->do_big = 0;
	m->do_full = 0;
	m->zoom = 1;

	m->window = SDL_CreateWindow("Camera", SDL_WINDOWPOS_UNDEFINED,
				     SDL_WINDOWPOS_UNDEFINED, 800, 429,
				     SDL_WINDOW_SHOWN |
				     m->do_full * SDL_WINDOW_FULLSCREEN_DESKTOP);
	if (m->window == NULL) {
		printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
		exit(1);
	}

	sdl_init_window(m);
}

static void sdl_set_size(struct sdl *m, int _sx)
{
	m->sx = _sx;
	m->factor = (float) m->dev->fmt.fmt.pix.width / m->sx;
	m->sy = m->dev->fmt.fmt.pix.height / m->factor;

	m->bx = (m->wx-m->sx)/2;
	m->by = (m->wy-m->sy)/2;

	m->liveview = SDL_CreateRGBSurface(0,m->sx,m->sy,32,0,0,0,0);
	if (!m->liveview) {
		printf("Couldn't create liveview\n");
		exit(1);
	}

	sdl_paint_ui(m);
	sdl_sync_settings(m);
}

static struct sdl sdl;

static void ppm_write(struct v4l2_format *fmt, unsigned char *img, char *out_name)
{
	FILE *fout;
	fout = fopen(out_name, "w");
	if (!fout) {
		perror("Cannot open image");
		exit(EXIT_FAILURE);
	}
	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_RGB24:
		printf("ok\n");
		break;
	default:
		printf("Bad pixel format\n");
		exit(1);
	}

	fprintf(fout, "P6\n%d %d 255\n",
		fmt->fmt.pix.width, fmt->fmt.pix.height);
	fwrite(img, fmt->fmt.pix.width, 3*fmt->fmt.pix.height, fout);
	fclose(fout);
}

/**
   Write image to jpeg file.
   \param img image to write
*/
static void jpeg_write(struct v4l2_format *fmt, unsigned char *img, char *jpegFilename)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	JSAMPROW row_pointer[1];
	FILE *outfile = fopen(jpegFilename, "wb");
	if (!outfile) {
		printf("Can't open jpeg for writing.\n");
		exit(2);
	}

	/* create jpeg data */
	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_stdio_dest(&cinfo, outfile);

	/* set image parameters */
	cinfo.image_width = fmt->fmt.pix.width;
	cinfo.image_height = fmt->fmt.pix.height;
	cinfo.input_components = 3;
	switch (fmt->fmt.pix.pixelformat) {
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_RGB24:
		cinfo.in_color_space = JCS_RGB;
		break;
	default:
		printf("Need to specify colorspace!\n");
		exit(1);
	}

	/* set jpeg compression parameters to default */
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 90, TRUE);

	jpeg_start_compress(&cinfo, TRUE);

	/* feed data */
	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = &img[cinfo.next_scanline * cinfo.image_width *  cinfo.input_components];
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);
	fclose(outfile);
}

static void any_write(struct dev_info *dev)
{
	char name[1024];
	unsigned char *buf;
	time_t t = time(NULL);
	int ppm = 0;

	buf = dev->buf;

	sprintf(name, PICDIR "/delme_%d.%s", (int) t, ppm ? "ppm" : "jpg");
	if (dev->fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB24) {
		printf("Wrong pixel format\n");
		exit(1);
	}
	if (ppm)
		ppm_write(&dev->fmt, buf, name);
	else
		jpeg_write(&dev->fmt, buf, name);
}

static void sdl_mouse(struct sdl *m, SDL_Event event)
{
	int ax = 0, ay = 0;
	int nx = event.button.x / (m->wx/m->nx), ny = event.button.y / (m->wy/m->ny);
	if (nx >= m->nx/2)
		nx -= m->nx;
	if (ny >= m->ny/2)
		ny -= m->ny;

	printf("Button %d %d\n", nx, ny);

	/* Virtual slider */
	if (nx == -2) {
		double value = (double) event.button.y / m->wy;
		switch (m->slider_mode) {
		case M_BIAS: {
			double ev = value - .5; /* -.5 .. +.5 */
			ev *= 3000;
			printf("Exposure bias: %f mEV %d\n", ev, (int) ev);
			if (v4l2_s_ctrl(m->fd, V4L2_CID_AUTO_EXPOSURE_BIAS, ev) < 0) {
				printf("Could not set exposure bias\n");
			}
		}
			return;
		case M_EXPOSURE:
			m->do_exposure = 0;
			cam_set_exposure(m, value);
			sdl_sync_settings(m);
			return;
		case M_GAIN:
			m->do_exposure = 0;
			v4l2_set_control(m->fd, 0x00980913, value * 65535);
			sdl_sync_settings(m);
			return;
		case M_FOCUS:
			cam_set_focus(m, value);
			return;
		}
	}

	switch (ny) {
	case 0:
		switch (nx) {
		case 0:
			m->do_focus ^= 1;
			sdl_paint_ui(m);
			sdl_sync_settings(m);
			return;
		case -1:
			m->do_exposure ^= 1;
			sdl_paint_ui(m);
			sdl_sync_settings(m);
			return;
		}
		break;
	case 1:
		switch (nx) {
		case -1:
			m->slider_mode = (m->slider_mode + 1) % M_NUM;
			sdl_paint_ui(m);
		}
		break;
	case -2:
		switch (nx) {
		case -1:
			v4l2_s_ctrl(m->fd, V4L2_CID_AUTO_FOCUS_STATUS, 1);
			return;
		break;
		}
	case -1:
		switch (nx) {
		case 0:
			exit(0);
		case 1:
			m->do_flash ^= 1;
			sdl_paint_ui(m);
			sdl_sync_settings(m);
			return;
		case 2:
			m->do_white ^= 1;
			sdl_paint_ui(m);
			sdl_sync_settings(m);
			return;
		case -3:
			m->do_big ^= 1;
			if (m->do_big)
				sdl_set_size(m, m->do_full ? 630:512);
			else
				sdl_set_size(m, 256);
			return;
		case -1:
			sdl_paint_ui(m);
			any_write(m->dev);
			return;
		}
		break;
	}

	if (event.button.x > m->wx-m->bx)
		ax = 1;
	if (event.button.x < m->bx)
		ax = -1;

	if (event.button.y > m->wy-m->by)
		ay = 1;
	if (event.button.y < m->by)
		ay = -1;

	printf("mouse button at...%d, %d area %d, %d\n", event.button.x, event.button.y,
	       ax, ay);
}

static void sdl_iteration(struct sdl *m)
{
	SDL_Event event;

	while(SDL_PollEvent(&event)) {
		switch(event.type) {
		case SDL_QUIT:
			exit(1);
			break;
		case SDL_KEYDOWN:
			printf("key pressed... %c\n", event.key.keysym.sym);
			/* SDLK_A, SDLK_LEFT, SDLK_RETURN, SDLK_BACKSPACE, SDLK_SPACE */
			switch (event.key.keysym.sym) {
			default: sdl_key(m, event.key.keysym.sym);
			}
			break;
		case SDL_WINDOWEVENT:
			if (event.window.event == SDL_WINDOWEVENT_EXPOSED)
				sdl_paint_ui(m);
			break;
		case SDL_MOUSEBUTTONDOWN:
			sdl_mouse(m, event);
			break;
		}
	}
}

static void cam_open(struct dev_info *dev, char *name)
{
	struct v4l2_format *fmt = &dev->fmt;

	dev->fd = v4l2_open(name, O_RDWR);
	if (dev->fd < 0) {
		printf("video %s open failed: %m\n", name);
		exit(1);
	}

	fmt->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
	fmt->fmt.pix.field = V4L2_FIELD_NONE;
	fmt->fmt.pix.width = SX;
	fmt->fmt.pix.height = SY;

	v4l2_s_ctrl(dev->fd, V4L2_CID_AUTO_FOCUS_STATUS, 0);
	v4l2_set_focus(dev->fd, 50);

	printf("ioctl = %d\n", v4l2_ioctl(dev->fd, VIDIOC_S_FMT, fmt));

	printf("capture is %lx %s %d x %d\n", (unsigned long) fmt->fmt.pix.pixelformat, fmt_name(fmt), fmt->fmt.pix.width, fmt->fmt.pix.height);
}

/* ------------------------------------------------------------------ */


static struct dev_info dev;

int main(void)
{
	int i;
	struct v4l2_format *fmt = &dev.fmt;

	dtime();
	cam_open(&dev, "/dev/video0");

	sdl_init(&sdl, &dev);
	sdl_set_size(&sdl, 256);

	double loop = dtime(), max = 0, avg = .200;
	if (dev.debug & D_TIMING)
		printf("startup took %f\n", loop);

	for (i=0; i<500000; i++) {
		int num = v4l2_read(dev.fd, dev.buf, SIZE);
		if (num < 0) {
			printf("Could not read frame\n");
			return 1;
		}
		{
			double d = dtime();
			sdl_render(&sdl, dev.buf, fmt);
			if (dev.debug & D_TIMING)
				printf("Render took %f\n", dtime() - d);
		}
		{
			double d = dtime();
			for (int i = 0; i<1; i++)
				sdl_status(&sdl, avg);
			if (dev.debug & D_TIMING)
				printf("Status took %f\n", dtime() - d);
		}

		sdl_iteration(&sdl);
		double now = dtime();
		if (now - loop > max)
			max = now - loop;
		double c = 0.03;
		avg = (now - loop) * c + avg * (1-c);
		if (dev.debug & D_TIMING)
			printf("Iteration %f, maximum %f, average %f\n", now-loop, max, avg);
		loop = now;
	}
	return 0;
}
