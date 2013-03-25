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

#ifndef V4L2_API_H
#define V4L2_API_H

#include <QString>
#include <linux/videodev2.h>
#include <libv4lconvert.h>

class v4l2
{
public:
	v4l2() : m_fd(-1) {}
	v4l2(v4l2 &old) :
		m_fd(old.m_fd),
		m_device(old.m_device),
		m_useWrapper(old.m_useWrapper),
		m_capability(old.m_capability)
	{}

	bool open(const QString &device, bool useWrapper = true);
	void close();
	int read(unsigned char *p, int size);
	int ioctl(unsigned cmd, void *arg);
	bool ioctl(const QString &descr, unsigned cmd, void *arg);
	bool ioctl_exists(unsigned cmd, void *arg);
	void *mmap(size_t length, int64_t offset);
	int munmap(void *_start, size_t length);

	inline int fd() const { return m_fd; }
	inline bool useWrapper() const { return m_useWrapper; }
	inline __u32 caps() const {
		if (m_capability.capabilities & V4L2_CAP_DEVICE_CAPS)
			return m_capability.device_caps;
		return m_capability.capabilities;
	}
	inline const QString &device() const { return m_device; }
	static QString pixfmt2s(unsigned pixelformat);

	virtual void error(const QString &text);

	bool querycap(v4l2_capability &cap);
	bool queryctrl(v4l2_queryctrl &qc);
	bool querymenu(v4l2_querymenu &qm);
	bool g_tuner(v4l2_tuner &tuner);
	bool s_tuner(v4l2_tuner &tuner);
	bool g_modulator(v4l2_modulator &modulator);
	bool s_modulator(v4l2_modulator &modulator);
	bool g_input(int &input);
	bool s_input(int input);
	bool g_output(int &output);
	bool s_output(int output);
	bool g_audio(v4l2_audio &audio);
	bool s_audio(int input);
	bool g_audout(v4l2_audioout &audout);
	bool s_audout(int output);
	bool s_std(v4l2_std_id std);
	bool g_std(v4l2_std_id &std);
	bool query_std(v4l2_std_id &std);
	bool s_dv_timings(v4l2_dv_timings &timings);
	bool g_dv_timings(v4l2_dv_timings &timings);
	bool query_dv_timings(v4l2_dv_timings &timings);
	bool g_frequency(v4l2_frequency &freq);
	bool s_frequency(v4l2_frequency &freq);
	bool s_frequency(int freq, bool low = false);
	bool g_fmt_cap(v4l2_format &fmt);
	bool g_fmt_out(v4l2_format &fmt);
	bool g_fmt_vbi(v4l2_format &fmt);
	bool g_fmt_sliced_vbi(v4l2_format &fmt);
	bool try_fmt(v4l2_format &fmt);
	bool s_fmt(v4l2_format &fmt);
	bool enum_input(v4l2_input &in, bool init = false, int index = 0);
	bool enum_output(v4l2_output &out, bool init = false, int index = 0);
	bool enum_audio(v4l2_audio &audio, bool init = false);
	bool enum_audout(v4l2_audioout &audout, bool init = false);
	bool enum_std(v4l2_standard &std, bool init = false, int index = 0);
	bool enum_dv_timings(v4l2_enum_dv_timings &timings, bool init = false, int index = 0);
	bool enum_fmt_cap(v4l2_fmtdesc &std, bool init = false, int index = 0);
	bool enum_fmt_out(v4l2_fmtdesc &std, bool init = false, int index = 0);
	bool enum_framesizes(v4l2_frmsizeenum &frm, __u32 init_pixfmt = 0, int index = 0);
	bool enum_frameintervals(v4l2_frmivalenum &frm, __u32 init_pixfmt = 0, __u32 w = 0, __u32 h = 0, int index = 0);

	bool reqbufs_mmap(v4l2_requestbuffers &reqbuf, __u32 buftype, int count = 0);
	bool reqbufs_user(v4l2_requestbuffers &reqbuf, __u32 buftype, int count = 0);
	bool dqbuf_mmap(v4l2_buffer &buf, __u32 buftype, bool &again);
	bool dqbuf_user(v4l2_buffer &buf, __u32 buftype, bool &again);
	bool qbuf(v4l2_buffer &buf);
	bool qbuf_mmap(int index, __u32 buftype);
	bool qbuf_user(int index, __u32 buftype, void *ptr, int length);
	bool streamon(__u32 buftype);
	bool streamoff(__u32 buftype);

	inline bool reqbufs_mmap_cap(v4l2_requestbuffers &reqbuf, int count = 0) {
		return reqbufs_mmap(reqbuf, V4L2_BUF_TYPE_VIDEO_CAPTURE, count);
	}
	inline bool reqbufs_user_cap(v4l2_requestbuffers &reqbuf, int count = 0) {
		return reqbufs_user(reqbuf, V4L2_BUF_TYPE_VIDEO_CAPTURE, count);
	}
	inline bool dqbuf_mmap_cap(v4l2_buffer &buf, bool &again) {
		return dqbuf_mmap(buf, V4L2_BUF_TYPE_VIDEO_CAPTURE, again);
	}
	inline bool dqbuf_user_cap(v4l2_buffer &buf, bool &again) {
		return dqbuf_user(buf, V4L2_BUF_TYPE_VIDEO_CAPTURE, again);
	}
	inline bool qbuf_mmap_cap(int index) {
		return qbuf_mmap(index, V4L2_BUF_TYPE_VIDEO_CAPTURE);
	}
	inline bool qbuf_user_cap(int index, void *ptr, int length) {
		return qbuf_user(index, V4L2_BUF_TYPE_VIDEO_CAPTURE, ptr, length);
	}
	inline bool streamon_cap() { return streamon(V4L2_BUF_TYPE_VIDEO_CAPTURE); }
	inline bool streamoff_cap() { return streamoff(V4L2_BUF_TYPE_VIDEO_CAPTURE); }

	inline bool reqbufs_mmap_vbi(v4l2_requestbuffers &reqbuf, int count = 0) {
		return reqbufs_mmap(reqbuf, V4L2_BUF_TYPE_VBI_CAPTURE, count);
	}
	inline bool reqbufs_user_vbi(v4l2_requestbuffers &reqbuf, int count = 0) {
		return reqbufs_user(reqbuf, V4L2_BUF_TYPE_VBI_CAPTURE, count);
	}
	inline bool dqbuf_mmap_vbi(v4l2_buffer &buf, bool &again) {
		return dqbuf_mmap(buf, V4L2_BUF_TYPE_VBI_CAPTURE, again);
	}
	inline bool dqbuf_user_vbi(v4l2_buffer &buf, bool &again) {
		return dqbuf_user(buf, V4L2_BUF_TYPE_VBI_CAPTURE, again);
	}
	inline bool qbuf_mmap_vbi(int index) {
		return qbuf_mmap(index, V4L2_BUF_TYPE_VBI_CAPTURE);
	}
	inline bool qbuf_user_vbi(int index, void *ptr, int length) {
		return qbuf_user(index, V4L2_BUF_TYPE_VBI_CAPTURE, ptr, length);
	}
	inline bool streamon_vbi() { return streamon(V4L2_BUF_TYPE_VBI_CAPTURE); }
	inline bool streamoff_vbi() { return streamoff(V4L2_BUF_TYPE_VBI_CAPTURE); }

	bool reqbufs_mmap_out(v4l2_requestbuffers &reqbuf, int count = 0);
	bool reqbufs_user_out(v4l2_requestbuffers &reqbuf);
	bool dqbuf_mmap_out(v4l2_buffer &buf);
	bool dqbuf_user_out(v4l2_buffer &buf);
	bool qbuf_mmap_out(int index, int bytesused);
	bool qbuf_user_out(void *ptr, int length);
	bool streamon_out();
	bool streamoff_out();
	bool subscribe_event(v4l2_event_subscription &sub);
	bool dqevent(v4l2_event &ev);

	bool set_interval(v4l2_fract interval);
	bool get_interval(v4l2_fract &interval);
private:
	void clear() { error(QString()); }

private:
	int 		m_fd;
	QString 	m_device;
	bool 		m_useWrapper;		// true if using the libv4l2 wrappers
	v4l2_capability m_capability;
};

#endif
