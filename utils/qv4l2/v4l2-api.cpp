/* v4l2-api: low-level wrapper around v4l2 devices
 *
 * Copyright (C) 2009 Hans Verkuil <hverkuil@xs4all.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <limits.h>
#include <libv4l2.h>
#include "v4l2-api.h"

bool v4l2::open(const QString &device, bool useWrapper)
{
	m_device = device;
	m_useWrapper = useWrapper;
	m_fd = ::open(device.toAscii(), O_RDWR | O_NONBLOCK);
	if (m_fd < 0) {
		error("Cannot open " + device);
		return false;
	}
	if (!querycap(m_capability)) {
		::close(m_fd);
		m_fd = -1;
		error(device + " is not a V4L2 device");
		return false;
	}

	if (m_useWrapper) {
		int fd = ::v4l2_fd_open(m_fd, V4L2_ENABLE_ENUM_FMT_EMULATION);

		if (fd < 0) {
			m_useWrapper = false;
			error("Cannot use libv4l2 wrapper for " + device);
		}
	}
	return true;
}

void v4l2::close()
{
	if (useWrapper())
		::v4l2_close(m_fd);
	else
		::close(m_fd);
	m_fd = -1;
}

int v4l2::ioctl(unsigned cmd, void *arg)
{
	if (useWrapper())
		return v4l2_ioctl(m_fd, cmd, arg);
	return ::ioctl(m_fd, cmd, arg);
}

bool v4l2::ioctl(const QString &descr, unsigned cmd, void *arg)
{
	clear();
	int err = ioctl(cmd, arg);

	if (err < 0) {
		QString s = strerror(errno);
		error(descr + ": " + s);
	}
	return err >= 0;
}

bool v4l2::ioctl_exists(unsigned cmd, void *arg)
{
	int err;

	if (useWrapper())
		err = v4l2_ioctl(m_fd, cmd, arg);
	else
		err = ::ioctl(m_fd, cmd, arg);
	return !err || errno != ENOTTY;
}

int v4l2::read(unsigned char *p, int size)
{
	if (useWrapper())
		return v4l2_read(m_fd, p, size);
	return ::read(m_fd, p, size);
}

void *v4l2::mmap(size_t length, int64_t offset)
{
	if (useWrapper())
		return v4l2_mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, offset);
	return ::mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, offset);
}

int v4l2::munmap(void *start, size_t length)
{
	if (useWrapper())
		return v4l2_munmap(start, length);
	return ::munmap(start, length);
}

void v4l2::error(const QString &error)
{
	if (!error.isEmpty())
		fprintf(stderr, "%s\n", error.toAscii().data());
}

QString v4l2::pixfmt2s(unsigned id)
{
	QString pixfmt;

	pixfmt += (char)(id & 0xff);
	pixfmt += (char)((id >> 8) & 0xff);
	pixfmt += (char)((id >> 16) & 0xff);
	pixfmt += (char)((id >> 24) & 0xff);
	return pixfmt;
}

bool v4l2::querycap(v4l2_capability &cap)
{
	memset(&cap, 0, sizeof(cap));
	return ioctl(VIDIOC_QUERYCAP, &cap) >= 0;
}

bool v4l2::queryctrl(v4l2_queryctrl &qc)
{
	return ioctl(VIDIOC_QUERYCTRL, &qc) >= 0;
}

bool v4l2::querymenu(v4l2_querymenu &qm)
{
	return ioctl(VIDIOC_QUERYMENU, &qm) >= 0;
}

bool v4l2::g_tuner(v4l2_tuner &tuner)
{
	memset(&tuner, 0, sizeof(tuner));
	if (ioctl(VIDIOC_G_TUNER, &tuner) < 0)
		return false;
	if (tuner.rangehigh > INT_MAX)
		tuner.rangehigh = INT_MAX;
	return true;
}

bool v4l2::s_tuner(v4l2_tuner &tuner)
{
	return ioctl("Set Tuner", VIDIOC_S_TUNER, &tuner);
}

bool v4l2::g_modulator(v4l2_modulator &modulator)
{
	memset(&modulator, 0, sizeof(modulator));
	return ioctl(VIDIOC_G_MODULATOR, &modulator) >= 0;
}

bool v4l2::s_modulator(v4l2_modulator &modulator)
{
	return ioctl("Set Modulator", VIDIOC_S_MODULATOR, &modulator);
}

bool v4l2::g_input(int &input)
{
	return ioctl(VIDIOC_G_INPUT, &input) >= 0;
}

bool v4l2::s_input(int input)
{
	return ioctl("Set Input", VIDIOC_S_INPUT, &input);
}

bool v4l2::g_output(int &output)
{
	return ioctl(VIDIOC_G_OUTPUT, &output) >= 0;
}

bool v4l2::s_output(int output)
{
	return ioctl("Set Output", VIDIOC_S_OUTPUT, &output);
}

bool v4l2::g_audio(v4l2_audio &audio)
{
	memset(&audio, 0, sizeof(audio));
	return ioctl(VIDIOC_G_AUDIO, &audio) >= 0;
}

bool v4l2::s_audio(int input)
{
	v4l2_audio audio;

	memset(&audio, 0, sizeof(audio));
	audio.index = input;
	return ioctl("Set Audio Input", VIDIOC_S_AUDIO, &audio);
}

bool v4l2::g_audout(v4l2_audioout &audout)
{
	memset(&audout, 0, sizeof(audout));
	return ioctl(VIDIOC_G_AUDOUT, &audout) >= 0;
}

bool v4l2::s_audout(int output)
{
	v4l2_audioout audout;

	memset(&audout, 0, sizeof(audout));
	audout.index = output;
	return ioctl("Set Audio Output", VIDIOC_S_AUDOUT, &audout);
}

bool v4l2::g_std(v4l2_std_id &std)
{
	return ioctl(VIDIOC_G_STD, &std) >= 0;
}

bool v4l2::s_std(v4l2_std_id std)
{
	return ioctl("Set TV Standard", VIDIOC_S_STD, &std);
}

bool v4l2::query_std(v4l2_std_id &std)
{
	return ioctl("Query TV Standard", VIDIOC_QUERYSTD, &std);
}

bool v4l2::g_dv_timings(v4l2_dv_timings &timings)
{
	int err = ioctl(VIDIOC_G_DV_TIMINGS, &timings);
	return err >= 0;
}

bool v4l2::s_dv_timings(v4l2_dv_timings &timings)
{
	return ioctl("Set Timings", VIDIOC_S_DV_TIMINGS, &timings);
}

bool v4l2::query_dv_timings(v4l2_dv_timings &timings)
{
	return ioctl("Query Timings", VIDIOC_QUERY_DV_TIMINGS, &timings);
}


bool v4l2::g_frequency(v4l2_frequency &freq)
{
	memset(&freq, 0, sizeof(freq));
	freq.type = V4L2_TUNER_ANALOG_TV;
	return ioctl(VIDIOC_G_FREQUENCY, &freq) >= 0;
}

bool v4l2::s_frequency(v4l2_frequency &freq)
{
	return ioctl("Set Frequency", VIDIOC_S_FREQUENCY, &freq);
}

bool v4l2::s_frequency(int val, bool low)
{
	v4l2_frequency f;

	memset(&f, 0, sizeof(f));
	f.type = low ? V4L2_TUNER_RADIO : V4L2_TUNER_ANALOG_TV;
	f.frequency = val;
	return s_frequency(f);
}

bool v4l2::g_fmt_cap(v4l2_format &fmt)
{
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return ioctl(VIDIOC_G_FMT, &fmt) >= 0;
}

bool v4l2::g_fmt_out(v4l2_format &fmt)
{
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	return ioctl(VIDIOC_G_FMT, &fmt) >= 0;
}

bool v4l2::g_fmt_vbi(v4l2_format &fmt)
{
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VBI_CAPTURE;
	return ioctl(VIDIOC_G_FMT, &fmt) >= 0;
}

bool v4l2::g_fmt_sliced_vbi(v4l2_format &fmt)
{
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_SLICED_VBI_CAPTURE;
	return ioctl(VIDIOC_G_FMT, &fmt) >= 0;
}

bool v4l2::try_fmt(v4l2_format &fmt)
{
	fmt.fmt.pix.field = V4L2_FIELD_ANY;
	fmt.fmt.pix.bytesperline = 0;
	return ioctl("Try Capture Format", VIDIOC_TRY_FMT, &fmt);
}

bool v4l2::s_fmt(v4l2_format &fmt)
{
	fmt.fmt.pix.bytesperline = 0;
	return ioctl("Set Capture Format", VIDIOC_S_FMT, &fmt);
}

bool v4l2::enum_input(v4l2_input &in, bool init, int index)
{
	if (init) {
		memset(&in, 0, sizeof(in));
		in.index = index;
	} else {
		in.index++;
	}
	return ioctl(VIDIOC_ENUMINPUT, &in) >= 0;
}

bool v4l2::enum_output(v4l2_output &out, bool init, int index)
{
	if (init) {
		memset(&out, 0, sizeof(out));
		out.index = index;
	} else {
		out.index++;
	}
	return ioctl(VIDIOC_ENUMOUTPUT, &out) >= 0;
}

bool v4l2::enum_audio(v4l2_audio &audio, bool init)
{
	if (init)
		memset(&audio, 0, sizeof(audio));
	else
		audio.index++;
	return ioctl(VIDIOC_ENUMAUDIO, &audio) >= 0;
}

bool v4l2::enum_audout(v4l2_audioout &audout, bool init)
{
	if (init)
		memset(&audout, 0, sizeof(audout));
	else
		audout.index++;
	return ioctl(VIDIOC_ENUMAUDOUT, &audout) >= 0;
}

bool v4l2::enum_std(v4l2_standard &std, bool init, int index)
{
	if (init) {
		memset(&std, 0, sizeof(std));
		std.index = index;
	} else {
		std.index++;
	}
	return ioctl(VIDIOC_ENUMSTD, &std) >= 0;
}

bool v4l2::enum_dv_timings(v4l2_enum_dv_timings &timings, bool init, int index)
{
	if (init) {
		memset(&timings, 0, sizeof(timings));
		timings.index = index;
	} else {
		timings.index++;
	}
	return ioctl(VIDIOC_ENUM_DV_TIMINGS, &timings) >= 0;
}

bool v4l2::enum_fmt_cap(v4l2_fmtdesc &fmt, bool init, int index)
{
	if (init) {
		memset(&fmt, 0, sizeof(fmt));
		fmt.index = index;
	} else {
		fmt.index++;
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return ioctl(VIDIOC_ENUM_FMT, &fmt) >= 0;
}

bool v4l2::enum_fmt_out(v4l2_fmtdesc &fmt, bool init, int index)
{
	if (init) {
		memset(&fmt, 0, sizeof(fmt));
		fmt.index = index;
	} else {
		fmt.index++;
	}
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	return ioctl(VIDIOC_ENUM_FMT, &fmt) >= 0;
}

bool v4l2::enum_framesizes(v4l2_frmsizeenum &frm, __u32 init_pixfmt, int index)
{
	if (init_pixfmt) {
		memset(&frm, 0, sizeof(frm));
		frm.pixel_format = init_pixfmt;
		frm.index = index;
	} else {
		frm.index++;
	}
	return ioctl(VIDIOC_ENUM_FRAMESIZES, &frm) >= 0;
}

bool v4l2::enum_frameintervals(v4l2_frmivalenum &frm, __u32 init_pixfmt, __u32 w, __u32 h, int index)
{
	if (init_pixfmt) {
		memset(&frm, 0, sizeof(frm));
		frm.pixel_format = init_pixfmt;
		frm.width = w;
		frm.height = h;
		frm.index = index;
	} else {
		frm.index++;
	}
	return ioctl(VIDIOC_ENUM_FRAMEINTERVALS, &frm) >= 0;
}

bool v4l2::reqbufs_user(v4l2_requestbuffers &reqbuf, __u32 buftype, int count)
{
	memset(&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = buftype;
	reqbuf.memory = V4L2_MEMORY_USERPTR;
	reqbuf.count = count;

	return ioctl(VIDIOC_REQBUFS, &reqbuf) >= 0;
}

bool v4l2::reqbufs_mmap(v4l2_requestbuffers &reqbuf, __u32 buftype, int count)
{
	memset(&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = buftype;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = count;

	return ioctl(VIDIOC_REQBUFS, &reqbuf) >= 0;
}

bool v4l2::dqbuf_mmap(v4l2_buffer &buf, __u32 buftype, bool &again)
{
	int res;

	memset(&buf, 0, sizeof(buf));
	buf.type = buftype;
	buf.memory = V4L2_MEMORY_MMAP;
	res = ioctl(VIDIOC_DQBUF, &buf);
	again = res < 0 && errno == EAGAIN;
	return res >= 0 || again;
}

bool v4l2::dqbuf_user(v4l2_buffer &buf, __u32 buftype, bool &again)
{
	int res;

	memset(&buf, 0, sizeof(buf));
	buf.type = buftype;
	buf.memory = V4L2_MEMORY_USERPTR;
	res = ioctl(VIDIOC_DQBUF, &buf);
	again = res < 0 && errno == EAGAIN;
	return res >= 0 || again;
}

bool v4l2::qbuf(v4l2_buffer &buf)
{
	return ioctl(VIDIOC_QBUF, &buf) >= 0;
}

bool v4l2::qbuf_mmap(int index, __u32 buftype)
{
	v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = buftype;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = index;
	return qbuf(buf);
}

bool v4l2::qbuf_user(int index, __u32 buftype, void *ptr, int length)
{
	v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = buftype;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.userptr = (unsigned long)ptr;
	buf.length = length;
	buf.index = index;
	return qbuf(buf);
}

bool v4l2::streamon(__u32 buftype)
{
	return ioctl("Start Streaming", VIDIOC_STREAMON, &buftype);
}

bool v4l2::streamoff(__u32 buftype)
{
	return ioctl("Stop Streaming", VIDIOC_STREAMOFF, &buftype);
}

bool v4l2::reqbufs_user_out(v4l2_requestbuffers &reqbuf)
{
	memset(&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_USERPTR;

	return ioctl(VIDIOC_REQBUFS, &reqbuf) >= 0;
}

bool v4l2::reqbufs_mmap_out(v4l2_requestbuffers &reqbuf, int count)
{
	memset(&reqbuf, 0, sizeof (reqbuf));
	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = count;

	return ioctl(VIDIOC_REQBUFS, &reqbuf) >= 0;
}

bool v4l2::dqbuf_mmap_out(v4l2_buffer &buf)
{
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
	return ioctl("dqbuf", VIDIOC_DQBUF, &buf);
}

bool v4l2::dqbuf_user_out(v4l2_buffer &buf)
{
	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_USERPTR;
	return ioctl(VIDIOC_DQBUF, &buf) >= 0;
}

bool v4l2::qbuf_mmap_out(int index, int bytesused)
{
	v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
	buf.index = index;
	buf.bytesused = bytesused;
	return qbuf(buf);
}

bool v4l2::qbuf_user_out(void *ptr, int length)
{
	v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_USERPTR;
	buf.m.userptr = (unsigned long)ptr;
	buf.length = length;
	return qbuf(buf);
}

bool v4l2::streamon_out()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	return ioctl("Start Output", VIDIOC_STREAMON, &type);
}

bool v4l2::streamoff_out()
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	return ioctl("Stop Output", VIDIOC_STREAMOFF, &type);
}

bool v4l2::subscribe_event(v4l2_event_subscription &sub)
{
	return ioctl(VIDIOC_SUBSCRIBE_EVENT, &sub);
}

bool v4l2::dqevent(v4l2_event &ev)
{
	return ioctl(VIDIOC_DQEVENT, &ev) >= 0;
}

bool v4l2::set_interval(v4l2_fract interval)
{
	v4l2_streamparm parm;

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(VIDIOC_G_PARM, &parm) < 0)
		return false;

	if (!(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
		return false;

	parm.parm.capture.timeperframe = interval;

	return ioctl("Set FPS", VIDIOC_S_PARM, &parm);
}

bool v4l2::get_interval(v4l2_fract &interval)
{
	v4l2_streamparm parm;

	parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(VIDIOC_G_PARM, &parm) >= 0 &&
	    (parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)) {
		interval = parm.parm.capture.timeperframe;
		return true;
        }

	return false;
}
