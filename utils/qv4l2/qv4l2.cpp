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

#include "qv4l2.h"
#include "general-tab.h"
#include "vbi-tab.h"
#include "capture-win.h"

#include <QToolBar>
#include <QToolButton>
#include <QMenuBar>
#include <QFileDialog>
#include <QStatusBar>
#include <QApplication>
#include <QMessageBox>
#include <QLineEdit>
#include <QValidator>
#include <QLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolTip>
#include <QImage>
#include <QWhatsThis>
#include <QThread>
#include <QCloseEvent>

#include <assert.h>
#include <sys/mman.h>
#include <errno.h>
#include <dirent.h>
#include <libv4l2.h>

ApplicationWindow::ApplicationWindow() :
	m_capture(NULL),
	m_sigMapper(NULL)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_capNotifier = NULL;
	m_ctrlNotifier = NULL;
	m_capImage = NULL;
	m_frameData = NULL;
	m_nbuffers = 0;
	m_buffers = NULL;
	m_makeSnapshot = false;

	QAction *openAct = new QAction(QIcon(":/fileopen.png"), "&Open Device", this);
	openAct->setStatusTip("Open a v4l device, use libv4l2 wrapper if possible");
	openAct->setShortcut(Qt::CTRL+Qt::Key_O);
	connect(openAct, SIGNAL(triggered()), this, SLOT(opendev()));

	QAction *openRawAct = new QAction(QIcon(":/fileopen.png"), "Open &Raw Device", this);
	openRawAct->setStatusTip("Open a v4l device without using the libv4l2 wrapper");
	openRawAct->setShortcut(Qt::CTRL+Qt::Key_R);
	connect(openRawAct, SIGNAL(triggered()), this, SLOT(openrawdev()));

	m_capStartAct = new QAction(QIcon(":/record.png"), "&Start Capturing", this);
	m_capStartAct->setStatusTip("Start capturing");
	m_capStartAct->setCheckable(true);
	m_capStartAct->setDisabled(true);
	connect(m_capStartAct, SIGNAL(toggled(bool)), this, SLOT(capStart(bool)));

	m_snapshotAct = new QAction(QIcon(":/snapshot.png"), "&Make Snapshot", this);
	m_snapshotAct->setStatusTip("Make snapshot");
	m_snapshotAct->setDisabled(true);
	connect(m_snapshotAct, SIGNAL(triggered()), this, SLOT(snapshot()));

	m_saveRawAct = new QAction(QIcon(":/saveraw.png"), "Save Raw Frames", this);
	m_saveRawAct->setStatusTip("Save raw frames to file.");
	m_saveRawAct->setCheckable(true);
	m_saveRawAct->setChecked(false);
	connect(m_saveRawAct, SIGNAL(toggled(bool)), this, SLOT(saveRaw(bool)));

	m_showFramesAct = new QAction(QIcon(":/video-television.png"), "Show &Frames", this);
	m_showFramesAct->setStatusTip("Only show captured frames if set.");
	m_showFramesAct->setCheckable(true);
	m_showFramesAct->setChecked(true);

	QAction *closeAct = new QAction(QIcon(":/fileclose.png"), "&Close", this);
	closeAct->setStatusTip("Close");
	closeAct->setShortcut(Qt::CTRL+Qt::Key_W);
	connect(closeAct, SIGNAL(triggered()), this, SLOT(closeDevice()));

	QAction *quitAct = new QAction(QIcon(":/exit.png"), "&Quit", this);
	quitAct->setStatusTip("Exit the application");
	quitAct->setShortcut(Qt::CTRL+Qt::Key_Q);
	connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

	QMenu *fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(openAct);
	fileMenu->addAction(openRawAct);
	fileMenu->addAction(closeAct);
	fileMenu->addAction(m_capStartAct);
	fileMenu->addAction(m_snapshotAct);
	fileMenu->addAction(m_saveRawAct);
	fileMenu->addAction(m_showFramesAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	QToolBar *toolBar = addToolBar("File");
	toolBar->setObjectName("toolBar");
	toolBar->addAction(openAct);
	toolBar->addAction(m_capStartAct);
	toolBar->addAction(m_snapshotAct);
	toolBar->addAction(m_saveRawAct);
	toolBar->addAction(m_showFramesAct);
	toolBar->addSeparator();
	toolBar->addAction(quitAct);

	QMenu *helpMenu = menuBar()->addMenu("&Help");
	helpMenu->addAction("&About", this, SLOT(about()), Qt::Key_F1);

	QAction *whatAct = QWhatsThis::createAction(this);
	helpMenu->addAction(whatAct);
	toolBar->addAction(whatAct);

	statusBar()->showMessage("Ready", 2000);

	m_tabs = new QTabWidget;
	m_tabs->setMinimumSize(300, 200);
	setCentralWidget(m_tabs);
}


ApplicationWindow::~ApplicationWindow()
{
	closeDevice();
}


void ApplicationWindow::setDevice(const QString &device, bool rawOpen)
{
	closeDevice();
	m_sigMapper = new QSignalMapper(this);
	connect(m_sigMapper, SIGNAL(mapped(int)), this, SLOT(ctrlAction(int)));

	if (!open(device, !rawOpen))
		return;

	m_capture = new CaptureWin;
	m_capture->setMinimumSize(150, 50);
	connect(m_capture, SIGNAL(close()), this, SLOT(closeCaptureWin()));

	QWidget *w = new QWidget(m_tabs);
	m_genTab = new GeneralTab(device, *this, 4, w);
	m_tabs->addTab(w, "General");
	addTabs();
	if (caps() & (V4L2_CAP_VBI_CAPTURE | V4L2_CAP_SLICED_VBI_CAPTURE)) {
		w = new QWidget(m_tabs);
		m_vbiTab = new VbiTab(w);
		m_tabs->addTab(w, "VBI");
	}
	if (QWidget *current = m_tabs->currentWidget()) {
		current->show();
	}
	statusBar()->clearMessage();
	m_tabs->show();
	m_tabs->setFocus();
	m_convertData = v4lconvert_create(fd());
	m_capStartAct->setEnabled(fd() >= 0 && !m_genTab->isRadio());
	m_ctrlNotifier = new QSocketNotifier(fd(), QSocketNotifier::Exception, m_tabs);
	connect(m_ctrlNotifier, SIGNAL(activated(int)), this, SLOT(ctrlEvent()));
}

void ApplicationWindow::opendev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio*)");

	d.setFilter(QDir::AllDirs | QDir::Files | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), false);
}

void ApplicationWindow::openrawdev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio*)");

	d.setFilter(QDir::AllDirs | QDir::Files | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), true);
}

void ApplicationWindow::capVbiFrame()
{
	__u32 buftype = m_genTab->bufType();
	v4l2_buffer buf;
	__u8 *data = NULL;
	int s = 0;
	bool again;

	switch (m_capMethod) {
	case methodRead:
		s = read(m_frameData, m_vbiSize);
		if (s < 0) {
			if (errno != EAGAIN) {
				error("read");
				m_capStartAct->setChecked(false);
			}
			return;
		}
		data = m_frameData;
		break;

	case methodMmap:
		if (!dqbuf_mmap(buf, buftype, again)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (again)
			return;
		data = (__u8 *)m_buffers[buf.index].start;
		s = buf.bytesused;
		break;

	case methodUser:
		if (!dqbuf_user(buf, buftype, again)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (again)
			return;
		data = (__u8 *)buf.m.userptr;
		s = buf.bytesused;
		break;
	}
	if (buftype == V4L2_BUF_TYPE_VBI_CAPTURE && s != m_vbiSize) {
		error("incorrect vbi size");
		m_capStartAct->setChecked(false);
		return;
	}
	if (m_showFrames) {
		for (unsigned y = 0; y < m_vbiHeight; y++) {
			__u8 *p = data + y * m_vbiWidth;
			__u8 *q = m_capImage->bits() + y * m_capImage->bytesPerLine();

			for (unsigned x = 0; x < m_vbiWidth; x++) {
				*q++ = *p;
				*q++ = *p;
				*q++ = *p++;
			}
		}
	}

	struct v4l2_sliced_vbi_format sfmt;
	struct v4l2_sliced_vbi_data sdata[m_vbiHandle.count[0] + m_vbiHandle.count[1]];
	struct v4l2_sliced_vbi_data *p;

	if (buftype == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE) {
		p = (struct v4l2_sliced_vbi_data *)data;
	} else {
		vbi_parse(&m_vbiHandle, data, &sfmt, sdata);
		s = sizeof(sdata);
		p = sdata;
	}

	if (m_capMethod != methodRead)
		qbuf(buf);

	m_vbiTab->slicedData(p, s / sizeof(p[0]));

	QString status, curStatus;
	struct timeval tv, res;

	if (m_frame == 0)
		gettimeofday(&m_tv, NULL);
	gettimeofday(&tv, NULL);
	timersub(&tv, &m_tv, &res);
	if (res.tv_sec) {
		m_fps = (100 * (m_frame - m_lastFrame)) /
			(res.tv_sec * 100 + res.tv_usec / 10000);
		m_lastFrame = m_frame;
		m_tv = tv;
	}
	status = QString("Frame: %1 Fps: %2").arg(++m_frame).arg(m_fps);
	if (m_showFrames)
		m_capture->setImage(*m_capImage, status);
	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: "))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

void ApplicationWindow::ctrlEvent()
{
	v4l2_event ev;

	while (dqevent(ev)) {
		if (ev.type != V4L2_EVENT_CTRL)
			continue;
		m_ctrlMap[ev.id].flags = ev.u.ctrl.flags;
		m_ctrlMap[ev.id].minimum = ev.u.ctrl.minimum;
		m_ctrlMap[ev.id].maximum = ev.u.ctrl.maximum;
		m_ctrlMap[ev.id].step = ev.u.ctrl.step;
		m_ctrlMap[ev.id].default_value = ev.u.ctrl.default_value;
		m_widgetMap[ev.id]->setDisabled(m_ctrlMap[ev.id].flags & CTRL_FLAG_DISABLED);
		switch (m_ctrlMap[ev.id].type) {
		case V4L2_CTRL_TYPE_INTEGER:
		case V4L2_CTRL_TYPE_INTEGER_MENU:
		case V4L2_CTRL_TYPE_MENU:
		case V4L2_CTRL_TYPE_BOOLEAN:
		case V4L2_CTRL_TYPE_BITMASK:
			setVal(ev.id, ev.u.ctrl.value);
			break;
		case V4L2_CTRL_TYPE_INTEGER64:
			setVal64(ev.id, ev.u.ctrl.value64);
			break;
		default:
			break;
		}
		if (m_ctrlMap[ev.id].type != V4L2_CTRL_TYPE_STRING)
			continue;
		queryctrl(m_ctrlMap[ev.id]);

		struct v4l2_ext_control c;
		struct v4l2_ext_controls ctrls;

		c.id = ev.id;
		c.size = m_ctrlMap[ev.id].maximum + 1;
		c.string = (char *)malloc(c.size);
		memset(&ctrls, 0, sizeof(ctrls));
		ctrls.count = 1;
		ctrls.ctrl_class = 0;
		ctrls.controls = &c;
		if (!ioctl(VIDIOC_G_EXT_CTRLS, &ctrls))
			setString(ev.id, c.string);
		free(c.string);
	}
}

void ApplicationWindow::capFrame()
{
	__u32 buftype = m_genTab->bufType();
	v4l2_buffer buf;
	int s = 0;
	int err = 0;
	bool again;

	switch (m_capMethod) {
	case methodRead:
		s = read(m_frameData, m_capSrcFormat.fmt.pix.sizeimage);
		if (s < 0) {
			if (errno != EAGAIN) {
				error("read");
				m_capStartAct->setChecked(false);
			}
			return;
		}
		if (m_makeSnapshot)
			makeSnapshot((unsigned char *)m_frameData, s);
		if (m_saveRaw.openMode())
			m_saveRaw.write((const char *)m_frameData, s);

		if (!m_showFrames)
			break;
		if (m_mustConvert)
			err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
				m_frameData, s,
				m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);
		if (!m_mustConvert || err < 0)
			memcpy(m_capImage->bits(), m_frameData, std::min(s, m_capImage->numBytes()));
		break;

	case methodMmap:
		if (!dqbuf_mmap(buf, buftype, again)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (again)
			return;

		if (m_showFrames) {
			if (m_mustConvert)
				err = v4lconvert_convert(m_convertData,
					&m_capSrcFormat, &m_capDestFormat,
					(unsigned char *)m_buffers[buf.index].start, buf.bytesused,
					m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);
			if (!m_mustConvert || err < 0)
				memcpy(m_capImage->bits(),
				       (unsigned char *)m_buffers[buf.index].start,
				       std::min(buf.bytesused, (unsigned)m_capImage->numBytes()));
		}
		if (m_makeSnapshot)
			makeSnapshot((unsigned char *)m_buffers[buf.index].start, buf.bytesused);
		if (m_saveRaw.openMode())
			m_saveRaw.write((const char *)m_buffers[buf.index].start, buf.bytesused);

		qbuf(buf);
		break;

	case methodUser:
		if (!dqbuf_user(buf, buftype, again)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (again)
			return;

		if (m_showFrames) {
			if (m_mustConvert)
				err = v4lconvert_convert(m_convertData,
					&m_capSrcFormat, &m_capDestFormat,
					(unsigned char *)buf.m.userptr, buf.bytesused,
					m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);
			if (!m_mustConvert || err < 0)
				memcpy(m_capImage->bits(), (unsigned char *)buf.m.userptr,
				       std::min(buf.bytesused, (unsigned)m_capImage->numBytes()));
		}
		if (m_makeSnapshot)
			makeSnapshot((unsigned char *)buf.m.userptr, buf.bytesused);
		if (m_saveRaw.openMode())
			m_saveRaw.write((const char *)buf.m.userptr, buf.bytesused);

		qbuf(buf);
		break;
	}
	if (err == -1 && m_frame == 0)
		error(v4lconvert_get_error_message(m_convertData));

	QString status, curStatus;
	struct timeval tv, res;

	if (m_frame == 0)
		gettimeofday(&m_tv, NULL);
	gettimeofday(&tv, NULL);
	timersub(&tv, &m_tv, &res);
	if (res.tv_sec) {
		m_fps = (100 * (m_frame - m_lastFrame)) /
			(res.tv_sec * 100 + res.tv_usec / 10000);
		m_lastFrame = m_frame;
		m_tv = tv;
	}
	status = QString("Frame: %1 Fps: %2").arg(++m_frame).arg(m_fps);
	if (m_showFrames)
		m_capture->setImage(*m_capImage, status);
	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: "))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

bool ApplicationWindow::startCapture(unsigned buffer_size)
{
	__u32 buftype = m_genTab->bufType();
	v4l2_requestbuffers req;
	unsigned int i;

	memset(&req, 0, sizeof(req));

	switch (m_capMethod) {
	case methodRead:
		m_snapshotAct->setEnabled(true);
		/* Nothing to do. */
		return true;

	case methodMmap:
		if (!reqbufs_mmap(req, buftype, 3)) {
			error("Cannot capture");
			break;
		}

		if (req.count < 2) {
			error("Too few buffers");
			reqbufs_mmap(req, buftype);
			break;
		}

		m_buffers = (buffer *)calloc(req.count, sizeof(*m_buffers));

		if (!m_buffers) {
			error("Out of memory");
			reqbufs_mmap(req, buftype);
			break;
		}

		for (m_nbuffers = 0; m_nbuffers < req.count; ++m_nbuffers) {
			v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));

			buf.type        = buftype;
			buf.memory      = V4L2_MEMORY_MMAP;
			buf.index       = m_nbuffers;

			if (-1 == ioctl(VIDIOC_QUERYBUF, &buf)) {
				perror("VIDIOC_QUERYBUF");
				goto error;
			}

			m_buffers[m_nbuffers].length = buf.length;
			m_buffers[m_nbuffers].start = mmap(buf.length, buf.m.offset);

			if (MAP_FAILED == m_buffers[m_nbuffers].start) {
				perror("mmap");
				goto error;
			}
		}
		for (i = 0; i < m_nbuffers; ++i) {
			if (!qbuf_mmap(i, buftype)) {
				perror("VIDIOC_QBUF");
				goto error;
			}
		}
		if (!streamon(buftype)) {
			perror("VIDIOC_STREAMON");
			goto error;
		}
		m_snapshotAct->setEnabled(true);
		return true;

	case methodUser:
		if (!reqbufs_user(req, buftype, 3)) {
			error("Cannot capture");
			break;
		}

		if (req.count < 2) {
			error("Too few buffers");
			reqbufs_user(req, buftype);
			break;
		}

		m_buffers = (buffer *)calloc(req.count, sizeof(*m_buffers));

		if (!m_buffers) {
			error("Out of memory");
			break;
		}

		for (m_nbuffers = 0; m_nbuffers < req.count; ++m_nbuffers) {
			m_buffers[m_nbuffers].length = buffer_size;
			m_buffers[m_nbuffers].start = malloc(buffer_size);

			if (!m_buffers[m_nbuffers].start) {
				error("Out of memory");
				goto error;
			}
		}
		for (i = 0; i < m_nbuffers; ++i)
			if (!qbuf_user(i, buftype, m_buffers[i].start, m_buffers[i].length)) {
				perror("VIDIOC_QBUF");
				goto error;
			}
		if (!streamon(buftype)) {
			perror("VIDIOC_STREAMON");
			goto error;
		}
		m_snapshotAct->setEnabled(true);
		return true;
	}

error:
	m_capStartAct->setChecked(false);
	return false;
}

void ApplicationWindow::stopCapture()
{
	__u32 buftype = m_genTab->bufType();
	v4l2_requestbuffers reqbufs;
	v4l2_encoder_cmd cmd;
	unsigned i;

	m_snapshotAct->setDisabled(true);
	switch (m_capMethod) {
	case methodRead:
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = V4L2_ENC_CMD_STOP;
		ioctl(VIDIOC_ENCODER_CMD, &cmd);
		break;

	case methodMmap:
		if (m_buffers == NULL)
			break;
		if (!streamoff(buftype))
			perror("VIDIOC_STREAMOFF");
		for (i = 0; i < m_nbuffers; ++i)
			if (-1 == munmap(m_buffers[i].start, m_buffers[i].length))
				perror("munmap");
		// Free all buffers.
		reqbufs_mmap(reqbufs, buftype, 1);  // videobuf workaround
		reqbufs_mmap(reqbufs, buftype, 0);
		break;

	case methodUser:
		if (!streamoff(buftype))
			perror("VIDIOC_STREAMOFF");
		// Free all buffers.
		reqbufs_user(reqbufs, buftype, 1);  // videobuf workaround
		reqbufs_user(reqbufs, buftype, 0);
		for (i = 0; i < m_nbuffers; ++i)
			free(m_buffers[i].start);
		break;
	}
	free(m_buffers);
	m_buffers = NULL;
	refresh();
}

void ApplicationWindow::startOutput(unsigned)
{
}

void ApplicationWindow::stopOutput()
{
}

void ApplicationWindow::closeCaptureWin()
{
	m_capStartAct->setChecked(false);
}

void ApplicationWindow::capStart(bool start)
{
	static const struct {
		__u32 v4l2_pixfmt;
		QImage::Format qt_pixfmt;
	} supported_fmts[] = {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
		{ V4L2_PIX_FMT_RGB32, QImage::Format_RGB32 },
		{ V4L2_PIX_FMT_RGB24, QImage::Format_RGB888 },
		{ V4L2_PIX_FMT_RGB565X, QImage::Format_RGB16 },
		{ V4L2_PIX_FMT_RGB555X, QImage::Format_RGB555 },
#else
		{ V4L2_PIX_FMT_BGR32, QImage::Format_RGB32 },
		{ V4L2_PIX_FMT_RGB24, QImage::Format_RGB888 },
		{ V4L2_PIX_FMT_RGB565, QImage::Format_RGB16 },
		{ V4L2_PIX_FMT_RGB555, QImage::Format_RGB555 },
		{ V4L2_PIX_FMT_RGB444, QImage::Format_RGB444 },
#endif
		{ 0, QImage::Format_Invalid }
	};
	QImage::Format dstFmt = QImage::Format_RGB888;
	struct v4l2_fract interval;
	v4l2_pix_format &srcPix = m_capSrcFormat.fmt.pix;
	v4l2_pix_format &dstPix = m_capDestFormat.fmt.pix;

	if (!start) {
		stopCapture();
		delete m_capNotifier;
		m_capNotifier = NULL;
		delete m_capImage;
		m_capImage = NULL;
		return;
	}
	m_showFrames = m_showFramesAct->isChecked();
	m_frame = m_lastFrame = m_fps = 0;
	m_capMethod = m_genTab->capMethod();

	if (m_genTab->isSlicedVbi()) {
		v4l2_format fmt;
		v4l2_std_id std;

		m_showFrames = false;
		g_fmt_sliced_vbi(fmt);
		g_std(std);
		fmt.fmt.sliced.service_set = (std & V4L2_STD_625_50) ?
			V4L2_SLICED_VBI_625 : V4L2_SLICED_VBI_525;
		s_fmt(fmt);
		memset(&m_vbiHandle, 0, sizeof(m_vbiHandle));
		m_vbiTab->slicedFormat(fmt.fmt.sliced);
		m_vbiSize = fmt.fmt.sliced.io_size;
		m_frameData = new unsigned char[m_vbiSize];
		if (startCapture(m_vbiSize)) {
			m_capNotifier = new QSocketNotifier(fd(), QSocketNotifier::Read, m_tabs);
			connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capVbiFrame()));
		}
		return;
	}
	if (m_genTab->isVbi()) {
		v4l2_format fmt;
		v4l2_std_id std;

		g_fmt_vbi(fmt);
		if (fmt.fmt.vbi.sample_format != V4L2_PIX_FMT_GREY) {
			error("non-grey pixelformat not supported for VBI\n");
			return;
		}
		s_fmt(fmt);
		g_std(std);
		if (!vbi_prepare(&m_vbiHandle, &fmt.fmt.vbi, std)) {
			error("no services possible\n");
			return;
		}
		m_vbiTab->rawFormat(fmt.fmt.vbi);
		m_vbiWidth = fmt.fmt.vbi.samples_per_line;
		if (fmt.fmt.vbi.flags & V4L2_VBI_INTERLACED)
			m_vbiHeight = fmt.fmt.vbi.count[0];
		else
			m_vbiHeight = fmt.fmt.vbi.count[0] + fmt.fmt.vbi.count[1];
		m_vbiSize = m_vbiWidth * m_vbiHeight;
		m_frameData = new unsigned char[m_vbiSize];
		if (m_showFrames) {
			m_capture->setMinimumSize(m_vbiWidth, m_vbiHeight);
			m_capImage = new QImage(m_vbiWidth, m_vbiHeight, dstFmt);
			m_capImage->fill(0);
			m_capture->setImage(*m_capImage, "No frame");
			m_capture->show();
		}
		statusBar()->showMessage("No frame");
		if (startCapture(m_vbiSize)) {
			m_capNotifier = new QSocketNotifier(fd(), QSocketNotifier::Read, m_tabs);
			connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capVbiFrame()));
		}
		return;
	}

	g_fmt_cap(m_capSrcFormat);
	s_fmt(m_capSrcFormat);
	if (m_genTab->get_interval(interval))
		set_interval(interval);

	m_mustConvert = m_showFrames;
	m_frameData = new unsigned char[srcPix.sizeimage];
	if (m_showFrames) {
		m_capDestFormat = m_capSrcFormat;
		dstPix.pixelformat = V4L2_PIX_FMT_RGB24;

		for (int i = 0; supported_fmts[i].v4l2_pixfmt; i++) {
			if (supported_fmts[i].v4l2_pixfmt == srcPix.pixelformat) {
				dstPix.pixelformat = supported_fmts[i].v4l2_pixfmt;
				dstFmt = supported_fmts[i].qt_pixfmt;
				m_mustConvert = false;
				break;
			}
		}
		if (m_mustConvert) {
			v4l2_format copy = m_capSrcFormat;

			v4lconvert_try_format(m_convertData, &m_capDestFormat, &m_capSrcFormat);
			// v4lconvert_try_format sometimes modifies the source format if it thinks
			// that there is a better format available. Restore our selected source
			// format since we do not want that happening.
			m_capSrcFormat = copy;
		}

		m_capture->setMinimumSize(dstPix.width, dstPix.height);
		m_capImage = new QImage(dstPix.width, dstPix.height, dstFmt);
		m_capImage->fill(0);
		m_capture->setImage(*m_capImage, "No frame");
		m_capture->show();
	}

	statusBar()->showMessage("No frame");
	if (startCapture(srcPix.sizeimage)) {
		m_capNotifier = new QSocketNotifier(fd(), QSocketNotifier::Read, m_tabs);
		connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capFrame()));
	}
}

void ApplicationWindow::closeDevice()
{
	delete m_sigMapper;
	m_sigMapper = NULL;
	m_capStartAct->setEnabled(false);
	m_capStartAct->setChecked(false);
	if (fd() >= 0) {
		if (m_capNotifier) {
			delete m_capNotifier;
			delete m_capImage;
			m_capNotifier = NULL;
			m_capImage = NULL;
		}
		if (m_ctrlNotifier) {
			delete m_ctrlNotifier;
			m_ctrlNotifier = NULL;
		}
		delete m_frameData;
		m_frameData = NULL;
		v4lconvert_destroy(m_convertData);
		v4l2::close();
		delete m_capture;
		m_capture = NULL;
	}
	while (QWidget *page = m_tabs->widget(0)) {
		m_tabs->removeTab(0);
		delete page;
	}
	m_ctrlMap.clear();
	m_widgetMap.clear();
	m_classMap.clear();
}

bool SaveDialog::setBuffer(unsigned char *buf, unsigned size)
{
	m_buf = new unsigned char[size];
	m_size = size;
	if (m_buf == NULL)
		return false;
	memcpy(m_buf, buf, size);
	return true;
}

void SaveDialog::selected(const QString &s)
{
	if (!s.isEmpty()) {
		QFile file(s);
		file.open(QIODevice::WriteOnly | QIODevice::Truncate);
		file.write((const char *)m_buf, m_size);
		file.close();
	}
	delete [] m_buf;
}

void ApplicationWindow::makeSnapshot(unsigned char *buf, unsigned size)
{
	m_makeSnapshot = false;
	SaveDialog *dlg = new SaveDialog(this, "Save Snapshot");
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setFileMode(QFileDialog::AnyFile);
	dlg->setAcceptMode(QFileDialog::AcceptSave);
	dlg->setModal(false);
	if (!dlg->setBuffer(buf, size)) {
		delete dlg;
		error("No memory to make snapshot\n");
		return;
	}
	connect(dlg, SIGNAL(fileSelected(const QString &)), dlg, SLOT(selected(const QString &)));
	dlg->show();
}

void ApplicationWindow::snapshot()
{
	m_makeSnapshot = true;
}

void ApplicationWindow::rejectedRawFile()
{
	m_saveRawAct->setChecked(false);
}

void ApplicationWindow::openRawFile(const QString &s)
{
	if (s.isEmpty())
		return;

	if (m_saveRaw.openMode())
		m_saveRaw.close();
	m_saveRaw.setFileName(s);
	m_saveRaw.open(QIODevice::WriteOnly | QIODevice::Truncate);
	m_saveRawAct->setChecked(true);
}

void ApplicationWindow::saveRaw(bool checked)
{
	if (!checked) {
		if (m_saveRaw.openMode())
			m_saveRaw.close();
		return;
	}

	SaveDialog *dlg = new SaveDialog(this, "Save Raw Frames");
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setFileMode(QFileDialog::AnyFile);
	dlg->setAcceptMode(QFileDialog::AcceptSave);
	dlg->setModal(false);
	connect(dlg, SIGNAL(fileSelected(const QString &)), this, SLOT(openRawFile(const QString &)));
	connect(dlg, SIGNAL(rejected()), this, SLOT(rejectedRawFile()));
	dlg->show();
}

void ApplicationWindow::about()
{
	QMessageBox::about(this, "V4L2 Test Bench",
			"This program allows easy experimenting with video4linux devices.");
}

void ApplicationWindow::error(const QString &error)
{
	statusBar()->showMessage(error, 20000);
	if (!error.isEmpty())
		fprintf(stderr, "%s\n", error.toAscii().data());
}

void ApplicationWindow::error(int err)
{
	error(QString("Error: %1").arg(strerror(err)));
}

void ApplicationWindow::errorCtrl(unsigned id, int err)
{
	error(QString("Error %1: %2")
		.arg((const char *)m_ctrlMap[id].name).arg(strerror(err)));
}

void ApplicationWindow::errorCtrl(unsigned id, int err, const QString &v)
{
	error(QString("Error %1 (%2): %3")
		.arg((const char *)m_ctrlMap[id].name).arg(v).arg(strerror(err)));
}

void ApplicationWindow::errorCtrl(unsigned id, int err, long long v)
{
	error(QString("Error %1 (%2): %3")
		.arg((const char *)m_ctrlMap[id].name).arg(v).arg(strerror(err)));
}

void ApplicationWindow::info(const QString &info)
{
	statusBar()->showMessage(info, 5000);
}

void ApplicationWindow::closeEvent(QCloseEvent *event)
{
	closeDevice();
	delete m_capture;
	event->accept();
}

ApplicationWindow *g_mw;

int main(int argc, char **argv)
{
	QApplication a(argc, argv);
	QString device = "/dev/video0";
	bool raw = false;
	bool help = false;
	int i;

	a.setWindowIcon(QIcon(":/qv4l2.png"));
	g_mw = new ApplicationWindow();
	g_mw->setWindowTitle("V4L2 Test Bench");
	for (i = 1; i < argc; i++) {
		const char *arg = a.argv()[i];

		if (!strcmp(arg, "-r"))
			raw = true;
		else if (!strcmp(arg, "-h"))
			help = true;
		else if (arg[0] != '-')
			device = arg;
	}
	if (help) {
		printf("qv4l2 [-r] [-h] [device node]\n\n"
		       "-h\tthis help message\n"
		       "-r\topen device node in raw mode\n");
		return 0;
	}
	g_mw->setDevice(device, raw);
	g_mw->show();
	a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
	return a.exec();
}
