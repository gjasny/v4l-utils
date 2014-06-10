#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <dirent.h>
#include <math.h>

#include "v4l2-ctl.h"

struct v4l2_decoder_cmd dec_cmd; /* (try_)decoder_cmd */
static struct v4l2_encoder_cmd enc_cmd; /* (try_)encoder_cmd */
static struct v4l2_jpegcompression jpegcomp; /* jpeg compression */
static struct v4l2_streamparm parm;	/* get/set parm */
static double fps = 0;			/* set framerate speed, in fps */
static double output_fps = 0;		/* set framerate speed, in fps */

void misc_usage(void)
{
	printf("\nMiscellaneous options:\n"
	       "  --wait-for-event=<event>\n"
	       "                     wait for an event [VIDIOC_DQEVENT]\n"
	       "                     <event> is the event number or one of:\n"
	       "                     eos, vsync, ctrl=<id>, frame_sync, source_change=<pad>,\n"
	       "                     motion_det\n"
	       "                     where <id> is the name of the control\n"
	       "                     and where <pad> is the index of the pad or input\n"
	       "  --poll-for-event=<event>\n"
	       "                     poll for an event [VIDIOC_DQEVENT]\n"
	       "                     see --wait-for-event for possible events\n"
	       "  -P, --get-parm     display video parameters [VIDIOC_G_PARM]\n"
	       "  -p, --set-parm=<fps>\n"
	       "                     set video framerate in <fps> [VIDIOC_S_PARM]\n"
	       "  --get-output-parm  display output video parameters [VIDIOC_G_PARM]\n"
	       "  --set-output-parm=<fps>\n"
	       "                     set output video framerate in <fps> [VIDIOC_S_PARM]\n"
	       "  --get-jpeg-comp    query the JPEG compression [VIDIOC_G_JPEGCOMP]\n"
	       "  --set-jpeg-comp=quality=<q>,markers=<markers>,comment=<c>,app<n>=<a>\n"
	       "                     set the JPEG compression [VIDIOC_S_JPEGCOMP]\n"
	       "                     <n> is the app segment: 0-9/a-f, <a> is the actual string.\n"
	       "                     <markers> is a colon separated list of:\n"
	       "                     dht:      Define Huffman Tables\n"
	       "                     dqt:      Define Quantization Tables\n"
	       "                     dri:      Define Restart Interval\n"
	       "  --encoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Send a command to the encoder [VIDIOC_ENCODER_CMD]\n"
	       "                     cmd=start|stop|pause|resume\n"
	       "                     flags=stop_at_gop_end\n"
	       "  --try-encoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Try an encoder command [VIDIOC_TRY_ENCODER_CMD]\n"
	       "                     See --encoder-cmd for the arguments.\n"
	       "  --decoder-cmd=cmd=<cmd>,flags=<flags>,stop_pts=<pts>,start_speed=<speed>,\n"
	       "                     start_format=<none|gop>\n"
	       "                     Send a command to the decoder [VIDIOC_DECODER_CMD]\n"
	       "                     cmd=start|stop|pause|resume\n"
	       "                     flags=start_mute_audio|pause_to_black|stop_to_black|\n"
	       "                           stop_immediately\n"
	       "  --try-decoder-cmd=cmd=<cmd>,flags=<flags>\n"
	       "                     Try a decoder command [VIDIOC_TRY_DECODER_CMD]\n"
	       "                     See --decoder-cmd for the arguments.\n"
	       );
}

static std::string markers2s(unsigned markers)
{
	std::string s;

	if (markers & V4L2_JPEG_MARKER_DHT)
		s += "\t\tDefine Huffman Tables\n";
	if (markers & V4L2_JPEG_MARKER_DQT)
		s += "\t\tDefine Quantization Tables\n";
	if (markers & V4L2_JPEG_MARKER_DRI)
		s += "\t\tDefine Restart Interval\n";
	if (markers & V4L2_JPEG_MARKER_COM)
		s += "\t\tDefine Comment\n";
	if (markers & V4L2_JPEG_MARKER_APP)
		s += "\t\tDefine APP segment\n";
	return s;
}

static void printjpegcomp(const struct v4l2_jpegcompression &jc)
{
	printf("JPEG compression:\n");
	printf("\tQuality: %d\n", jc.quality);
	if (jc.COM_len)
		printf("\tComment: '%s'\n", jc.COM_data);
	if (jc.APP_len)
		printf("\tAPP%x   : '%s'\n", jc.APPn, jc.APP_data);
	printf("\tMarkers: 0x%08x\n", jc.jpeg_markers);
	printf("%s", markers2s(jc.jpeg_markers).c_str());
}

static void print_enccmd(const struct v4l2_encoder_cmd &cmd)
{
	switch (cmd.cmd) {
	case V4L2_ENC_CMD_START:
		printf("\tstart\n");
		break;
	case V4L2_ENC_CMD_STOP:
		printf("\tstop%s\n",
			(cmd.flags & V4L2_ENC_CMD_STOP_AT_GOP_END) ? " at gop end" : "");
		break;
	case V4L2_ENC_CMD_PAUSE:
		printf("\tpause\n");
		break;
	case V4L2_ENC_CMD_RESUME:
		printf("\tresume\n");
		break;
	}
}

static void print_deccmd(const struct v4l2_decoder_cmd &cmd)
{
	__s32 speed;

	switch (cmd.cmd) {
	case V4L2_DEC_CMD_START:
		speed = cmd.start.speed;
		if (speed == 0)
			speed = 1000;
		printf("\tstart%s%s, ",
			cmd.start.format == V4L2_DEC_START_FMT_GOP ? " (GOP aligned)" : "",
			(speed != 1000 &&
			 (cmd.flags & V4L2_DEC_CMD_START_MUTE_AUDIO)) ? " (mute audio)" : "");
		if (speed == 1 || speed == -1)
			printf("single step %s\n",
				speed == 1 ? "forward" : "backward");
		else
			printf("speed %.3fx\n", speed / 1000.0);
		break;
	case V4L2_DEC_CMD_STOP:
		printf("\tstop%s%s\n",
			(cmd.flags & V4L2_DEC_CMD_STOP_TO_BLACK) ? " to black" : "",
			(cmd.flags & V4L2_DEC_CMD_STOP_IMMEDIATELY) ? " immediately" : "");
		break;
	case V4L2_DEC_CMD_PAUSE:
		printf("\tpause%s\n",
			(cmd.flags & V4L2_DEC_CMD_PAUSE_TO_BLACK) ? " to black" : "");
		break;
	case V4L2_DEC_CMD_RESUME:
		printf("\tresume\n");
		break;
	}
}

/* Used for both encoder and decoder commands since they are the same
   at the moment. */
static int parse_cmd(const char *s)
{
	if (!strcmp(s, "start")) return V4L2_ENC_CMD_START;
	if (!strcmp(s, "stop")) return V4L2_ENC_CMD_STOP;
	if (!strcmp(s, "pause")) return V4L2_ENC_CMD_PAUSE;
	if (!strcmp(s, "resume")) return V4L2_ENC_CMD_RESUME;
	return 0;
}

static int parse_encflags(const char *s)
{
	if (!strcmp(s, "stop_at_gop_end")) return V4L2_ENC_CMD_STOP_AT_GOP_END;
	return 0;
}

static int parse_decflags(const char *s)
{
	if (!strcmp(s, "start_mute_audio")) return V4L2_DEC_CMD_START_MUTE_AUDIO;
	if (!strcmp(s, "pause_to_black")) return V4L2_DEC_CMD_PAUSE_TO_BLACK;
	if (!strcmp(s, "stop_to_black")) return V4L2_DEC_CMD_STOP_TO_BLACK;
	if (!strcmp(s, "stop_immediately")) return V4L2_DEC_CMD_STOP_IMMEDIATELY;
	return 0;
}

void misc_cmd(int ch, char *optarg)
{
	char *value, *subs;

	switch (ch) {
	case OptSetParm:
		fps = strtod(optarg, NULL);
		break;
	case OptSetOutputParm:
		output_fps = strtod(optarg, NULL);
		break;
	case OptSetJpegComp:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"app0", "app1", "app2", "app3",
				"app4", "app5", "app6", "app7",
				"app8", "app9", "appa", "appb",
				"appc", "appd", "appe", "appf",
				"quality",
				"markers",
				"comment",
				NULL
			};
			size_t len;
			int opt = parse_subopt(&subs, subopts, &value);

			switch (opt) {
			case 16:
				jpegcomp.quality = strtol(value, 0L, 0);
				break;
			case 17:
				if (strstr(value, "dht"))
					jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DHT;
				if (strstr(value, "dqt"))
					jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DQT;
				if (strstr(value, "dri"))
					jpegcomp.jpeg_markers |= V4L2_JPEG_MARKER_DRI;
				break;
			case 18:
				len = strlen(value);
				if (len > sizeof(jpegcomp.COM_data) - 1)
					len = sizeof(jpegcomp.COM_data) - 1;
				jpegcomp.COM_len = len;
				memcpy(jpegcomp.COM_data, value, len);
				jpegcomp.COM_data[len] = '\0';
				break;
			default:
				if (opt < 0 || opt > 15) {
					misc_usage();
					exit(1);
				}
				len = strlen(value);
				if (len > sizeof(jpegcomp.APP_data) - 1)
					len = sizeof(jpegcomp.APP_data) - 1;
				if (jpegcomp.APP_len) {
					fprintf(stderr, "Only one APP segment can be set\n");
					break;
				}
				jpegcomp.APP_len = len;
				memcpy(jpegcomp.APP_data, value, len);
				jpegcomp.APP_data[len] = '\0';
				jpegcomp.APPn = opt;
				break;
			}
		}
		break;
	case OptEncoderCmd:
	case OptTryEncoderCmd:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"cmd",
				"flags",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				enc_cmd.cmd = parse_cmd(value);
				break;
			case 1:
				enc_cmd.flags = parse_encflags(value);
				break;
			default:
				misc_usage();
				exit(1);
			}
		}
		break;
	case OptDecoderCmd:
	case OptTryDecoderCmd:
		subs = optarg;
		while (*subs != '\0') {
			static const char *const subopts[] = {
				"cmd",
				"flags",
				"stop_pts",
				"start_speed",
				"start_format",
				NULL
			};

			switch (parse_subopt(&subs, subopts, &value)) {
			case 0:
				dec_cmd.cmd = parse_cmd(value);
				break;
			case 1:
				dec_cmd.flags = parse_decflags(value);
				break;
			case 2:
				dec_cmd.stop.pts = strtoull(value, 0, 0);
				break;
			case 3:
				dec_cmd.start.speed = strtol(value, 0, 0);
				break;
			case 4:
				if (!strcmp(value, "gop"))
					dec_cmd.start.format = V4L2_DEC_START_FMT_GOP;
				else if (!strcmp(value, "none"))
					dec_cmd.start.format = V4L2_DEC_START_FMT_NONE;
				break;
			default:
				misc_usage();
				exit(1);
			}
		}
		break;
	}
}

void misc_set(int fd)
{
	if (options[OptSetParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.timeperframe.numerator = 1000;
		parm.parm.capture.timeperframe.denominator =
			fps * parm.parm.capture.timeperframe.numerator;

		if (doioctl(fd, VIDIOC_S_PARM, &parm) == 0) {
			struct v4l2_fract *tf = &parm.parm.capture.timeperframe;

			if (!tf->denominator || !tf->numerator)
				printf("Invalid frame rate\n");
			else
				printf("Frame rate set to %.3f fps\n",
					1.0 * tf->denominator / tf->numerator);
		}
	}

	if (options[OptSetOutputParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		parm.parm.output.timeperframe.numerator = 1000;
		parm.parm.output.timeperframe.denominator =
			output_fps * parm.parm.output.timeperframe.numerator;

		if (doioctl(fd, VIDIOC_S_PARM, &parm) == 0) {
			struct v4l2_fract *tf = &parm.parm.output.timeperframe;

			if (!tf->denominator || !tf->numerator)
				printf("Invalid frame rate\n");
			else
				printf("Frame rate set to %.3f fps\n",
					1.0 * tf->denominator / tf->numerator);
		}
	}

	if (options[OptSetJpegComp]) {
		doioctl(fd, VIDIOC_S_JPEGCOMP, &jpegcomp);
	}
	
	if (options[OptEncoderCmd])
		doioctl(fd, VIDIOC_ENCODER_CMD, &enc_cmd);
	if (options[OptTryEncoderCmd])
		if (doioctl(fd, VIDIOC_TRY_ENCODER_CMD, &enc_cmd) == 0)
			print_enccmd(enc_cmd);
	if (options[OptDecoderCmd])
		doioctl(fd, VIDIOC_DECODER_CMD, &dec_cmd);
	if (options[OptTryDecoderCmd])
		if (doioctl(fd, VIDIOC_TRY_DECODER_CMD, &dec_cmd) == 0)
			print_deccmd(dec_cmd);
}

void misc_get(int fd)
{
	if (options[OptGetJpegComp]) {
		struct v4l2_jpegcompression jc;
		if (doioctl(fd, VIDIOC_G_JPEGCOMP, &jc) == 0)
			printjpegcomp(jc);
	}

        if (options[OptGetParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (doioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
			const struct v4l2_fract &tf = parm.parm.capture.timeperframe;

			printf("Streaming Parameters %s:\n", buftype2s(parm.type).c_str());
			if (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
				printf("\tCapabilities     : timeperframe\n");
			if (parm.parm.capture.capturemode & V4L2_MODE_HIGHQUALITY)
				printf("\tCapture mode     : high quality\n");
			if (!tf.denominator || !tf.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
						tf.denominator, tf.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
						(1.0 * tf.denominator) / tf.numerator,
						tf.denominator, tf.numerator);
			printf("\tRead buffers     : %d\n", parm.parm.capture.readbuffers);
		}
	}

	if (options[OptGetOutputParm]) {
		memset(&parm, 0, sizeof(parm));
		parm.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		if (doioctl(fd, VIDIOC_G_PARM, &parm) == 0) {
			const struct v4l2_fract &tf = parm.parm.output.timeperframe;

			printf("Streaming Parameters %s:\n", buftype2s(parm.type).c_str());
			if (parm.parm.output.capability & V4L2_CAP_TIMEPERFRAME)
				printf("\tCapabilities     : timeperframe\n");
			if (parm.parm.output.outputmode & V4L2_MODE_HIGHQUALITY)
				printf("\tOutput mode      : high quality\n");
			if (!tf.denominator || !tf.numerator)
				printf("\tFrames per second: invalid (%d/%d)\n",
						tf.denominator, tf.numerator);
			else
				printf("\tFrames per second: %.3f (%d/%d)\n",
						(1.0 * tf.denominator) / tf.numerator,
						tf.denominator, tf.numerator);
			printf("\tWrite buffers    : %d\n", parm.parm.output.writebuffers);
		}
	}
}
