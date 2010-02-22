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

ApplicationWindow::ApplicationWindow() :
	m_capture(NULL),
	m_sigMapper(NULL)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_capNotifier = NULL;
	m_capImage = NULL;
	m_frameData = NULL;
	m_nbuffers = 0;
	m_buffers = NULL;

	QAction *openAct = new QAction(QIcon(":/fileopen.png"), "&Open device", this);
	openAct->setStatusTip("Open a v4l device, use libv4l2 wrapper if possible");
	openAct->setShortcut(Qt::CTRL+Qt::Key_O);
	connect(openAct, SIGNAL(triggered()), this, SLOT(opendev()));

	QAction *openRawAct = new QAction(QIcon(":/fileopen.png"), "Open &raw device", this);
	openRawAct->setStatusTip("Open a v4l device without using the libv4l2 wrapper");
	openRawAct->setShortcut(Qt::CTRL+Qt::Key_R);
	connect(openRawAct, SIGNAL(triggered()), this, SLOT(openrawdev()));

	m_capStartAct = new QAction(QIcon(":/record.png"), "&Start capturing", this);
	m_capStartAct->setStatusTip("Start capturing");
	m_capStartAct->setCheckable(true);
	m_capStartAct->setDisabled(true);
	connect(m_capStartAct, SIGNAL(toggled(bool)), this, SLOT(capStart(bool)));

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
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	QToolBar *toolBar = addToolBar("File");
	toolBar->setObjectName("toolBar");
	toolBar->addAction(openAct);
	toolBar->addAction(m_capStartAct);
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
	if (QWidget *current = m_tabs->currentWidget()) {
		current->show();
	}
	m_tabs->show();
	m_tabs->setFocus();
	m_convertData = v4lconvert_create(fd());
	m_capStartAct->setEnabled(fd() >= 0);
}

void ApplicationWindow::opendev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio*)");

	d.setFilter(QDir::Dirs | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), false);
}

void ApplicationWindow::openrawdev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio*)");

	d.setFilter(QDir::Dirs | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), true);
}

void ApplicationWindow::capFrame()
{
	v4l2_buffer buf;
	unsigned i;
	int s = 0;
	int err = 0;

	switch (m_capMethod) {
	case methodRead:
		s = read(m_frameData, m_capSrcFormat.fmt.pix.sizeimage);
		err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
				m_frameData, m_capSrcFormat.fmt.pix.sizeimage,
				m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);
		break;

	case methodMmap:
		if (!dqbuf_mmap_cap(buf)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}

		err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
				(unsigned char *)m_buffers[buf.index].start, buf.bytesused,
				m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);

		qbuf(buf);
		break;

	case methodUser:
		if (!dqbuf_user_cap(buf)) {
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}

		for (i = 0; i < m_nbuffers; ++i)
			if (buf.m.userptr == (unsigned long)m_buffers[i].start
					&& buf.length == m_buffers[i].length)
				break;

		err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
				(unsigned char *)buf.m.userptr, buf.bytesused,
				m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);

		qbuf(buf);
		break;
	}
	m_capture->setImage(*m_capImage);
}

bool ApplicationWindow::startCapture(unsigned buffer_size)
{
	unsigned int i;
	v4l2_requestbuffers req;

	memset(&req, 0, sizeof(req));

	switch (m_capMethod) {
	case methodRead:
		/* Nothing to do. */
		return true;

	case methodMmap:
		if (!reqbufs_mmap_cap(req, 3)) {
			error("Cannot capture");
			break;
		}

		if (req.count < 2) {
			error("Too few buffers");
			reqbufs_mmap_cap(req);
			break;
		}

		m_buffers = (buffer *)calloc(req.count, sizeof(*m_buffers));

		if (!m_buffers) {
			error("Out of memory");
			reqbufs_mmap_cap(req);
			break;
		}

		for (m_nbuffers = 0; m_nbuffers < req.count; ++m_nbuffers) {
			v4l2_buffer buf;

			memset(&buf, 0, sizeof(buf));

			buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
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
			if (!qbuf_mmap_cap(i)) {
				perror("VIDIOC_QBUF");
				goto error;
			}
		}
		if (!streamon_cap()) {
			perror("VIDIOC_STREAMON");
			goto error;
		}
		return true;

	case methodUser:
		if (!reqbufs_user_cap(req, 4)) {
			error("Cannot capture");
			break;
		}

		if (req.count < 4) {
			error("Too few buffers");
			reqbufs_user_cap(req);
			break;
		}

		m_buffers = (buffer *)calloc(4, sizeof(*m_buffers));

		if (!m_buffers) {
			error("Out of memory");
			break;
		}

		for (m_nbuffers = 0; m_nbuffers < 4; ++m_nbuffers) {
			m_buffers[m_nbuffers].length = buffer_size;
			m_buffers[m_nbuffers].start = malloc(buffer_size);

			if (!m_buffers[m_nbuffers].start) {
				error("Out of memory");
				goto error;
			}
		}
		for (i = 0; i < m_nbuffers; ++i)
			if (!qbuf_user_cap(i, m_buffers[i].start, m_buffers[i].length)) {
				perror("VIDIOC_QBUF");
				goto error;
			}
		if (!streamon_cap()) {
			perror("VIDIOC_STREAMON");
			goto error;
		}
		return true;
	}

error:
	m_capStartAct->setChecked(false);
	return false;
}

void ApplicationWindow::stopCapture()
{
	v4l2_requestbuffers reqbufs;
	v4l2_encoder_cmd cmd;
	unsigned i;

	switch (m_capMethod) {
	case methodRead:
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = V4L2_ENC_CMD_STOP;
		ioctl(VIDIOC_ENCODER_CMD, &cmd);
		break;

	case methodMmap:
		if (m_buffers == NULL)
			break;
		if (!streamoff_cap())
			perror("VIDIOC_STREAMOFF");
		for (i = 0; i < m_nbuffers; ++i)
			if (-1 == munmap(m_buffers[i].start, m_buffers[i].length))
				perror("munmap");
		// Free all buffers.
		reqbufs_mmap_out(reqbufs, 0);
		break;

	case methodUser:
		if (!streamoff_cap())
			perror("VIDIOC_STREAMOFF");
		for (i = 0; i < m_nbuffers; ++i)
			free(m_buffers[i].start);
		break;
	}
	free(m_buffers);
	m_buffers = NULL;
	m_capture->stop();
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
		{ V4L2_PIX_FMT_RGB32, QImage::Format_ARGB32 },
		{ V4L2_PIX_FMT_RGB24, QImage::Format_RGB888 },
		{ V4L2_PIX_FMT_RGB565X, QImage::Format_RGB16 },
		{ V4L2_PIX_FMT_RGB555X, QImage::Format_RGB555 },
		{ V4L2_PIX_FMT_RGB444, QImage::Format_RGB444 },
		{ 0, QImage::Format_Invalid }
	};
	QImage::Format dstFmt = QImage::Format_RGB888;

	if (!start) {
		stopCapture();
		delete m_capNotifier;
		m_capNotifier = NULL;
		delete m_capImage;
		return;
	}
	m_capMethod = m_genTab->capMethod();
	g_fmt_cap(m_capSrcFormat);
	m_frameData = new unsigned char[m_capSrcFormat.fmt.pix.sizeimage];
	m_capDestFormat = m_capSrcFormat;
	m_capDestFormat.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;

	for (int i = 0; supported_fmts[i].v4l2_pixfmt; i++) {
		if (supported_fmts[i].v4l2_pixfmt == m_capSrcFormat.fmt.pix.pixelformat) {
			m_capDestFormat.fmt.pix.pixelformat = supported_fmts[i].v4l2_pixfmt;
			dstFmt = supported_fmts[i].qt_pixfmt;
			break;
		}
	}
	v4lconvert_try_format(m_convertData, &m_capDestFormat, &m_capSrcFormat);
	// v4lconvert_try_format sometimes modifies the source format if it thinks
	// that there is a better format available. Restore our selected source
	// format since we do not want that happening.
	g_fmt_cap(m_capSrcFormat);
	m_capture->setMinimumSize(m_capDestFormat.fmt.pix.width, m_capDestFormat.fmt.pix.height);
	m_capImage = new QImage(m_capDestFormat.fmt.pix.width, m_capDestFormat.fmt.pix.height, dstFmt);
	m_capImage->fill(0);
	m_capture->setImage(*m_capImage, true);
	m_capture->show();
	if (startCapture(m_capSrcFormat.fmt.pix.sizeimage)) {
		m_capNotifier = new QSocketNotifier(fd(), QSocketNotifier::Read, m_tabs);
		connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capFrame()));
	}
}

void ApplicationWindow::closeDevice()
{
	m_capStartAct->setEnabled(false);
	m_capStartAct->setChecked(false);
	if (fd() >= 0) {
		if (m_capNotifier) {
			delete m_capNotifier;
			delete m_capImage;
			m_capNotifier = NULL;
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
	delete m_sigMapper;
	m_sigMapper = NULL;
	m_ctrlMap.clear();
	m_widgetMap.clear();
	m_classMap.clear();
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

	a.setWindowIcon(QIcon(":/qv4l2.png"));
	g_mw = new ApplicationWindow();
	g_mw->setWindowTitle("V4L2 Test Bench");
	g_mw->setDevice(a.argc() > 1 ? a.argv()[1] : "/dev/video0", true);
	g_mw->show();
	a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
	return a.exec();
}
