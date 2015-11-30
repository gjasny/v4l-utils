/* qv4l2: a control panel controlling v4l2 devices.
 *
 * Copyright (C) 2006 Hans Verkuil <hverkuil@xs4all.nl>
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


#ifndef GENERAL_TAB_H
#define GENERAL_TAB_H

#include <config.h>

#include <QSpinBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QStackedWidget>
#include <sys/time.h>
#include <linux/videodev2.h>
#include <map>

#include "qv4l2.h"
#include "capture-win.h"

#ifdef HAVE_ALSA
extern "C" {
#include "../libmedia_dev/get_media_devices.h"
#include "alsa_stream.h"
}
#include <alsa/asoundlib.h>
#endif

class QComboBox;
class QCheckBox;
class QSpinBox;
class QToolButton;
class QSlider;

class GeneralTab: public QGridLayout
{
	Q_OBJECT

public:
	GeneralTab(const QString &device, cv4l_fd *fd, int n, QWidget *parent = 0);
	virtual ~GeneralTab() {}

	CapMethod capMethod();
	QString getAudioInDevice();
	QString getAudioOutDevice();
	void setAudioDeviceBufferSize(int size);
	int getAudioDeviceBufferSize();
	bool hasAlsaAudio();
	double getPixelAspectRatio();
	CropMethod getCropMethod();
	bool get_interval(struct v4l2_fract &interval);
	int width() const { return m_width; }
	int height() const { return m_height; }
	bool isRadio() const { return m_isRadio; }
	bool isSDR() const { return m_isSDR; }
	bool isVbi() const { return m_isVbi; }
	bool isSlicedVbi() const;
	bool isPlanar() const { return m_isPlanar; }
	bool isSDTV() const { return m_isSDTV; }
	__u32 usePrio() const
	{
		return m_recordPrio->isChecked() ?
			V4L2_PRIORITY_RECORD : V4L2_PRIORITY_DEFAULT;
	}
	void setHaveBuffers(bool haveBuffers);
	unsigned getNumBuffers()
	{
		return m_numBuffers->value();
	}
	void sourceChange(const v4l2_event &ev);
	void sourceChangeSubscribe();
	int getWidth();
	unsigned getNumBuffers() const;
	QComboBox *m_tpgComboColorspace;
	QComboBox *m_tpgComboXferFunc;
	QComboBox *m_tpgComboYCbCrEnc;
	QComboBox *m_tpgComboQuantRange;

signals:
	void audioDeviceChanged();
	void pixelAspectRatioChanged();
	void croppingChanged();
	void clearBuffers();

private slots:
	void inputChanged(int);
	void outputChanged(int);
	void inputAudioChanged(int);
	void outputAudioChanged(int);
	void standardChanged(int);
	void qryStdClicked();
	void timingsChanged(int);
	void qryTimingsClicked();
	void freqTableChanged(int);
	void freqChannelChanged(int);
	void freqChanged(double);
	void freqRfChanged(double);
	void audioModeChanged(int);
	void detectSubchansClicked();
	void stereoModeChanged();
	void rdsModeChanged();
	void vidCapFormatChanged(int);
	void vidFieldChanged(int);
	void frameWidthChanged();
	void frameHeightChanged();
	void frameSizeChanged(int);
	void frameIntervalChanged(int);
	void vidOutFormatChanged(int);
	void vbiMethodsChanged(int);
	void changeAudioDevice();
	void changePixelAspectRatio();
	void cropChanged();
	void composeChanged();
	void colorspaceChanged(int);
	void xferFuncChanged(int);
	void ycbcrEncChanged(int);
	void quantRangeChanged(int);

private:
	void inputSection(v4l2_input vin);
	void outputSection(v4l2_output vout);
	void audioSection(v4l2_audio vaudio, v4l2_audioout vaudout);
	void formatSection(v4l2_fmtdesc fmt); 
	void cropSection(); 
	void fixWidth();
	void updateGUIInput(__u32);
	void updateGUIOutput(__u32);
	void updateVideoInput();
	void updateVideoOutput();
	void updateAudioInput();
	void updateAudioOutput();
	void refreshStandards();
	void updateStandard();
	void refreshTimings();
	void updateTimings();
	void updateFreq();
	void updateFreqChannel();
	void updateFreqRf();
	void updateColorspace();
	void clearColorspace(cv4l_fmt &fmt);
	void updateVidCapFormat();
	void updateVidFields();
	void updateFrameSize();
	void updateFrameInterval();
	void updateVidOutFormat();
	void updateCrop();
	void updateCompose();
	void updateVidFormat()
	{
		if (m_isOutput)
			updateVidOutFormat();
		else
			updateVidCapFormat();
	}
	int addAudioDevice(void *hint, int deviceNum);
	bool filterAudioDevice(QString &deviceName);
	bool createAudioDeviceList();
#ifdef HAVE_ALSA
	int matchAudioDevice();
	int checkMatchAudioDevice(void *md, const char *vid, enum device_type type);
#endif

	void addWidget(QWidget *w, Qt::Alignment align = Qt::AlignLeft);
	void addTitle(const QString &titlename);
	void addLabel(const QString &text, Qt::Alignment align = Qt::AlignLeft)
	{
		addWidget(new QLabel(text, parentWidget()), align);
	}
	void info(const QString &info)
	{
		g_mw->info(info);
	}
	virtual void error(const QString &error)
	{
		g_mw->error(error);
	}
	virtual void error(int error)
	{
		g_mw->error(error);
	}
	v4l_fd *g_v4l_fd() { return m_fd->g_v4l_fd(); }

	__u32 g_type() const { return m_fd->g_type(); }
	void s_type(__u32 type) { m_fd->s_type(type); }
	__u32 g_selection_type() const { return m_fd->g_selection_type(); }
	__u32 g_caps() const { return m_fd->g_caps(); }

	bool has_vid_cap() const { return m_fd->has_vid_cap(); }
	bool has_vid_out() const { return m_fd->has_vid_out(); }
	bool has_vid_m2m() const { return m_fd->has_vid_m2m(); }
	bool has_vid_mplane() const { return m_fd->has_vid_mplane(); }
	bool has_overlay_cap() const { return m_fd->has_overlay_cap(); }
	bool has_overlay_out() const { return m_fd->has_overlay_out(); }
	bool has_raw_vbi_cap() const { return m_fd->has_raw_vbi_cap(); }
	bool has_sliced_vbi_cap() const { return m_fd->has_sliced_vbi_cap(); }
	bool has_vbi_cap() const { return m_fd->has_vbi_cap(); }
	bool has_raw_vbi_out() const { return m_fd->has_raw_vbi_out(); }
	bool has_sliced_vbi_out() const { return m_fd->has_sliced_vbi_out(); }
	bool has_vbi_out() const { return m_fd->has_vbi_out(); }
	bool has_vbi() const { return m_fd->has_vbi(); }
	bool has_radio_rx() const { return m_fd->has_radio_rx(); }
	bool has_radio_tx() const { return m_fd->has_radio_tx(); }
	bool has_rds_cap() const { return m_fd->has_rds_cap(); }
	bool has_rds_out() const { return m_fd->has_rds_out(); }
	bool has_sdr_cap() const { return m_fd->has_sdr_cap(); }
	bool has_hwseek() const { return m_fd->has_hwseek(); }
	bool has_rw() const { return m_fd->has_rw(); }
	bool has_streaming() const { return m_fd->has_streaming(); }
	bool has_ext_pix_format() const { return m_fd->has_ext_pix_format(); }

	bool g_direct() const { return m_fd->g_direct(); }
	void s_direct(bool direct) { m_fd->s_direct(direct); }
	void querycap(v4l2_capability &cap) { return m_fd->querycap(cap); }
	int queryctrl(v4l2_queryctrl &qc) { return m_fd->queryctrl(qc); }
	int querymenu(v4l2_querymenu &qm) { return m_fd->querymenu(qm); }
	int g_fmt(v4l2_format &fmt, unsigned type = 0) { return m_fd->g_fmt(fmt); }
	int try_fmt(v4l2_format &fmt) { return m_fd->try_fmt(fmt); }
	int s_fmt(v4l2_format &fmt) { return m_fd->s_fmt(fmt); }
	int g_selection(v4l2_selection &sel) { return m_fd->g_selection(sel); }
	int s_selection(v4l2_selection &sel) { return m_fd->s_selection(sel); }
	int g_tuner(v4l2_tuner &tuner, unsigned index = 0) { return m_fd->g_tuner(tuner, index); }
	int s_tuner(v4l2_tuner &tuner) { return m_fd->s_tuner(tuner); }
	int g_modulator(v4l2_modulator &modulator) { return m_fd->g_modulator(modulator); }
	int s_modulator(v4l2_modulator &modulator) { return m_fd->s_modulator(modulator); }
	int enum_input(v4l2_input &in, bool init = false, int index = 0) { return m_fd->enum_input(in, init, index); }
	int enum_output(v4l2_output &out, bool init = false, int index = 0) { return m_fd->enum_output(out, init, index); }
	int enum_audio(v4l2_audio &audio, bool init = false, int index = 0) { return m_fd->enum_audio(audio, init, index); }
	int enum_audout(v4l2_audioout &audout, bool init = false, int index = 0) { return m_fd->enum_audout(audout, init, index); }
	int subscribe_event(v4l2_event_subscription &sub) { return m_fd->subscribe_event(sub); }
	int dqevent(v4l2_event &ev) { return m_fd->dqevent(ev); }
	int g_input(__u32 &input) { return m_fd->g_input(input); }
	int s_input(__u32 input) { return m_fd->s_input(input); }
	int g_output(__u32 &output) { return m_fd->g_output(output); }
	int s_output(__u32 output) { return m_fd->s_output(output); }
	int g_audio(v4l2_audio &audio) { return m_fd->g_audio(audio); }
	int s_audio(__u32 input) { return m_fd->s_audio(input); }
	int g_audout(v4l2_audioout &audout) { return m_fd->g_audout(audout); }
	int s_audout(__u32 output) { return m_fd->s_audout(output); }
	int g_std(v4l2_std_id &std) { return m_fd->g_std(std); }
	int s_std(v4l2_std_id std) { return m_fd->s_std(std); }
	int query_std(v4l2_std_id &std) { return m_fd->query_std(std); }
	int g_dv_timings(v4l2_dv_timings &timings) { return m_fd->g_dv_timings(timings); }
	int s_dv_timings(v4l2_dv_timings &timings) { return m_fd->s_dv_timings(timings); }
	int query_dv_timings(v4l2_dv_timings &timings) { return m_fd->query_dv_timings(timings); }
	int g_frequency(v4l2_frequency &freq, unsigned index = 0) { return m_fd->g_frequency(freq, index); }
	int s_frequency(v4l2_frequency &freq) { return m_fd->s_frequency(freq); }
	int g_priority(__u32 &prio) { return m_fd->g_priority(prio); }
	int s_priority(__u32 prio = V4L2_PRIORITY_DEFAULT) { return m_fd->s_priority(prio); }
	int streamon(__u32 type = 0) { return m_fd->streamon(type); }
	int streamoff(__u32 type = 0) { return m_fd->streamoff(type); }
	int querybuf(v4l_buffer &buf, unsigned index) { return m_fd->querybuf(buf, index); }
	int dqbuf(v4l_buffer &buf) { return m_fd->dqbuf(buf); }
	int qbuf(v4l_buffer &buf) { return m_fd->qbuf(buf); }
	int prepare_buf(v4l_buffer &buf) { return m_fd->prepare_buf(buf); }
	int enum_std(v4l2_standard &std, bool init = false, int index = 0) { return m_fd->enum_std(std, init, index); }
	int enum_dv_timings(v4l2_enum_dv_timings &timings, bool init = false, int index = 0) { return m_fd->enum_dv_timings(timings, init, index); }
	int enum_fmt(v4l2_fmtdesc &fmt, bool init = false, int index = 0, unsigned type = 0) { return m_fd->enum_fmt(fmt, init, index, type); }
	int enum_framesizes(v4l2_frmsizeenum &frm, __u32 init_pixfmt = 0, int index = 0) { return m_fd->enum_framesizes(frm, init_pixfmt, index); }
	int enum_frameintervals(v4l2_frmivalenum &frm, __u32 init_pixfmt = 0, __u32 w = 0, __u32 h = 0, int index = 0) { return m_fd->enum_frameintervals(frm, init_pixfmt, w, h, index); }
	int set_interval(v4l2_fract interval, unsigned type = 0) { return m_fd->set_interval(interval, type); }
	v4l2_fract g_pixel_aspect(unsigned &width, unsigned &height, unsigned type = 0)
	{
		return m_fd->g_pixel_aspect(width, height, type);
	}
	bool has_crop() { return m_fd->has_crop(); }
	bool has_compose() { return m_fd->has_compose(); }
	bool cur_io_has_crop() { return m_fd->cur_io_has_crop(); }
	bool cur_io_has_compose() { return m_fd->cur_io_has_compose(); }
	bool ioctl_exists(int ret)
	{
		return ret == 0 || errno != ENOTTY;
	}


	cv4l_fd *m_fd;
	int m_row;
	int m_col;
	int m_cols;
	const int m_minWidth;
	const double m_pxw;
	const int m_vMargin;
	const int m_hMargin;
	int m_increment;
	int m_maxw[4];
	int m_maxh;
	bool m_isRadio;
	bool m_isSDR;
	bool m_isVbi;
	bool m_isOutput;
	bool m_isSDTV;
	double m_freqFac;
	double m_freqRfFac;
	bool m_isPlanar;
	bool m_haveBuffers;
	bool m_discreteSizes;
	__u32 m_audioModes[5];
	QString m_device;
	struct v4l2_tuner m_tuner;
	struct v4l2_tuner m_tuner_rf;
	struct v4l2_modulator m_modulator;
	struct v4l2_capability m_querycap;
	__u32 m_pixelformat;
	__u32 m_width, m_height;
	struct v4l2_fract m_interval;
	bool m_has_interval;
	int m_audioDeviceBufferSize;
	static bool m_fullAudioName;
	std::map<int, QString> m_audioInDeviceMap;
	std::map<int, QString> m_audioOutDeviceMap;

	// General tab
	QList<QGridLayout *> m_grids;
	QStackedWidget *m_stackedStandards;
	QStackedWidget *m_stackedFrameSettings;
	QStackedWidget *m_stackedFrequency;
	QComboBox *m_videoInput;
	QComboBox *m_videoOutput;
	QComboBox *m_audioInput;
	QComboBox *m_audioOutput;
	QComboBox *m_tvStandard;
	QToolButton *m_qryStandard;
	QComboBox *m_videoTimings;
	QComboBox *m_pixelAspectRatio;
	QComboBox *m_colorspace;
	QComboBox *m_xferFunc;
	QComboBox *m_ycbcrEnc;
	QComboBox *m_quantRange;
	QComboBox *m_cropping;
	QToolButton *m_qryTimings;
	QDoubleSpinBox *m_freq;
	QComboBox *m_freqTable;
	QComboBox *m_freqChannel;
	QComboBox *m_audioMode;
	QLabel *m_subchannels;
	QDoubleSpinBox *m_freqRf;
	QCheckBox *m_stereoMode;
	QCheckBox *m_rdsMode;
	QToolButton *m_detectSubchans;
	QComboBox *m_vidCapFormats;
	QComboBox *m_vidFields;
	QComboBox *m_frameSize;
	QSpinBox *m_frameWidth;
	QSpinBox *m_frameHeight;
	QComboBox *m_frameInterval;
	QComboBox *m_vidOutFormats;
	QComboBox *m_capMethods;
	QSpinBox *m_numBuffers;
	QCheckBox *m_recordPrio;
	QComboBox *m_vbiMethods;
	QComboBox *m_audioInDevice;
	QComboBox *m_audioOutDevice;
	QSlider *m_cropWidth;
	QSlider *m_cropLeft;
	QSlider *m_cropHeight;
	QSlider *m_cropTop;
	QSlider *m_composeWidth;
	QSlider *m_composeLeft;
	QSlider *m_composeHeight;
	QSlider *m_composeTop;
};

#endif
