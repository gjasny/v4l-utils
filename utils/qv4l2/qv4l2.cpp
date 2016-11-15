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

#ifdef ENABLE_ASLA
extern "C" {
#include "alsa_stream.h"
}
#endif

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
#include <QInputDialog>

#include <assert.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <dirent.h>

#include "qv4l2.h"
#include "general-tab.h"
#include "vbi-tab.h"
#include "capture-win.h"
#include "capture-win-qt.h"
#include "capture-win-gl.h"

#include <libv4lconvert.h>

#define SDR_WIDTH 1024
#define SDR_HEIGHT 512

static QAction *addSubMenuItem(QActionGroup *grp, QMenu *menu, const QString &text, int val)
{
	QAction *a = grp->addAction(menu->addAction(text));

	a->setData(QVariant(val));
	a->setCheckable(true);
	return a;
}

ApplicationWindow::ApplicationWindow() :
	m_capture(NULL),
	m_pxw(25),
	m_minWidth(175),
	m_vMargin(15),
	m_hMargin(5),
	m_genTab(NULL),
	m_sigMapper(NULL)
{
	setAttribute(Qt::WA_DeleteOnClose, true);

	m_capNotifier = NULL;
	m_outNotifier = NULL;
	m_ctrlNotifier = NULL;
	m_capImage = NULL;
	m_frameData = NULL;
	m_nbuffers = 0;
	m_makeSnapshot = false;
	m_singleStep = false;
	m_tpgColorspace = 0;
	m_tpgXferFunc = 0;
	m_tpgYCbCrEnc = 0;
	m_tpgQuantRange = 0;
	m_tpgLimRGBRange = NULL;
	for (unsigned b = 0; b < sizeof(m_clear); b++)
		m_clear[b] = false;

	QAction *openAct = new QAction(QIcon(":/fileopen.png"), "&Open Device", this);
	openAct->setStatusTip("Open a v4l device, use libv4l2 wrapper if possible");
	openAct->setShortcut(Qt::CTRL+Qt::Key_O);
	connect(openAct, SIGNAL(triggered()), this, SLOT(opendev()));

	QAction *openRawAct = new QAction(QIcon(":/fileopen.png"), "Open &Raw Device", this);
	openRawAct->setStatusTip("Open a v4l device without using the libv4l2 wrapper");
	openRawAct->setShortcut(Qt::CTRL+Qt::Key_R);
	connect(openRawAct, SIGNAL(triggered()), this, SLOT(openrawdev()));

	m_capStartAct = new QAction(QIcon(":/start.png"), "Start &Capturing", this);
	m_capStartAct->setStatusTip("Start capturing");
	m_capStartAct->setCheckable(true);
	m_capStartAct->setDisabled(true);
	m_capStartAct->setShortcut(Qt::CTRL+Qt::Key_V);
	connect(m_capStartAct, SIGNAL(toggled(bool)), this, SLOT(capStart(bool)));

	m_capStepAct = new QAction(QIcon(":/step.png"), "Single Step", this);
	m_capStepAct->setDisabled(true);
	connect(m_capStepAct, SIGNAL(triggered(bool)), this, SLOT(capStep(bool)));

	m_snapshotAct = new QAction(QIcon(":/snapshot.png"), "&Make Snapshot", this);
	m_snapshotAct->setStatusTip("Make snapshot");
	m_snapshotAct->setDisabled(true);
	connect(m_snapshotAct, SIGNAL(triggered()), this, SLOT(snapshot()));

	m_saveRawAct = new QAction(QIcon(":/saveraw.png"), "Save Raw Frames", this);
	m_saveRawAct->setStatusTip("Save raw frames to file.");
	m_saveRawAct->setCheckable(true);
	m_saveRawAct->setChecked(false);
	connect(m_saveRawAct, SIGNAL(toggled(bool)), this, SLOT(saveRaw(bool)));

	m_showFramesAct = new QAction(QIcon(":/video-television.png"), "&Show Frames", this);
	m_showFramesAct->setStatusTip("Only show captured frames if set.");
	m_showFramesAct->setCheckable(true);
	m_showFramesAct->setChecked(true);

	QAction *closeAct = new QAction(QIcon(":/fileclose.png"), "&Close Device", this);
	closeAct->setStatusTip("Close");
	closeAct->setShortcut(Qt::CTRL+Qt::Key_W);
	connect(closeAct, SIGNAL(triggered()), this, SLOT(closeDevice()));

	QAction *traceAct = new QAction("&Trace IOCTLs", this);
	traceAct->setStatusTip("All V4L2 IOCTLs are traced on the console");
	traceAct->setCheckable(true);
	connect(traceAct, SIGNAL(toggled(bool)), this, SLOT(traceIoctls(bool)));

	QAction *quitAct = new QAction(QIcon(":/exit.png"), "&Quit", this);
	quitAct->setStatusTip("Exit the application");
	quitAct->setShortcut(Qt::CTRL+Qt::Key_Q);
	connect(quitAct, SIGNAL(triggered()), this, SLOT(close()));

	QMenu *fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(openAct);
	fileMenu->addAction(openRawAct);
	fileMenu->addAction(closeAct);
	fileMenu->addAction(m_snapshotAct);
	fileMenu->addAction(m_saveRawAct);
	fileMenu->addSeparator();
	fileMenu->addAction(traceAct);
	fileMenu->addSeparator();
	fileMenu->addAction(quitAct);

	QToolBar *toolBar = addToolBar("File");
	toolBar->setObjectName("toolBar");
	toolBar->addAction(openAct);
	toolBar->addAction(m_capStartAct);
	toolBar->addAction(m_capStepAct);
	toolBar->addAction(m_snapshotAct);
	toolBar->addAction(m_saveRawAct);

	m_scalingAct = new QAction("&Enable Video Scaling", this);
	m_scalingAct->setStatusTip("Scale video frames to match window size if set");
	m_scalingAct->setCheckable(true);
	m_scalingAct->setChecked(true);
	connect(m_scalingAct, SIGNAL(toggled(bool)), this, SLOT(enableScaling(bool)));

	m_resetScalingAct = new QAction("Resize to &Frame Size", this);
	m_resetScalingAct->setStatusTip("Resizes the capture window to match frame size");
	m_resetScalingAct->setShortcut(Qt::CTRL+Qt::Key_F);

	m_overrideColorspace = -1;
	QMenu *menu = new QMenu("Override Colorspace");
	m_overrideColorspaceMenu = menu;
	QActionGroup *grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	addSubMenuItem(grp, menu, "SMPTE 170M", V4L2_COLORSPACE_SMPTE170M);
	addSubMenuItem(grp, menu, "Rec. 709", V4L2_COLORSPACE_REC709);
	addSubMenuItem(grp, menu, "sRGB", V4L2_COLORSPACE_SRGB);
	addSubMenuItem(grp, menu, "Adobe RGB", V4L2_COLORSPACE_ADOBERGB);
	addSubMenuItem(grp, menu, "BT.2020", V4L2_COLORSPACE_BT2020);
	addSubMenuItem(grp, menu, "DCI-P3", V4L2_COLORSPACE_DCI_P3);
	addSubMenuItem(grp, menu, "SMPTE 240M", V4L2_COLORSPACE_SMPTE240M);
	addSubMenuItem(grp, menu, "470 System M", V4L2_COLORSPACE_470_SYSTEM_M);
	addSubMenuItem(grp, menu, "470 System BG", V4L2_COLORSPACE_470_SYSTEM_BG);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(overrideColorspaceChanged(QAction *)));

	m_overrideXferFunc = -1;
	menu = new QMenu("Override Transfer Function");
	m_overrideXferFuncMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	addSubMenuItem(grp, menu, "Rec. 709", V4L2_XFER_FUNC_709);
	addSubMenuItem(grp, menu, "sRGB", V4L2_XFER_FUNC_SRGB);
	addSubMenuItem(grp, menu, "Adobe RGB", V4L2_XFER_FUNC_ADOBERGB);
	addSubMenuItem(grp, menu, "DCI-P3", V4L2_XFER_FUNC_DCI_P3);
	addSubMenuItem(grp, menu, "SMPTE 2084", V4L2_XFER_FUNC_SMPTE2084);
	addSubMenuItem(grp, menu, "SMPTE 240M", V4L2_XFER_FUNC_SMPTE240M);
	addSubMenuItem(grp, menu, "None", V4L2_XFER_FUNC_NONE);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(overrideXferFuncChanged(QAction *)));

	m_overrideYCbCrEnc = -1;
	menu = new QMenu("Override Y'CbCr/HSV Encoding");
	m_overrideYCbCrEncMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	addSubMenuItem(grp, menu, "ITU-R 601", V4L2_YCBCR_ENC_601);
	addSubMenuItem(grp, menu, "Rec. 709", V4L2_YCBCR_ENC_709);
	addSubMenuItem(grp, menu, "xvYCC 601", V4L2_YCBCR_ENC_XV601);
	addSubMenuItem(grp, menu, "xvYCC 709", V4L2_YCBCR_ENC_XV709);
	addSubMenuItem(grp, menu, "BT.2020", V4L2_YCBCR_ENC_BT2020);
	addSubMenuItem(grp, menu, "BT.2020 Constant Luminance", V4L2_YCBCR_ENC_BT2020_CONST_LUM);
	addSubMenuItem(grp, menu, "SMPTE 240M", V4L2_YCBCR_ENC_SMPTE240M);
	addSubMenuItem(grp, menu, "HSV with Hue 0-179", V4L2_HSV_ENC_180);
	addSubMenuItem(grp, menu, "HSV with Hue 0-255", V4L2_HSV_ENC_256);

	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(overrideYCbCrEncChanged(QAction *)));

	m_overrideQuantization = -1;
	menu = new QMenu("Override Quantization");
	m_overrideQuantizationMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	addSubMenuItem(grp, menu, "Full Range", V4L2_QUANTIZATION_FULL_RANGE);
	addSubMenuItem(grp, menu, "Limited Range", V4L2_QUANTIZATION_LIM_RANGE);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(overrideQuantChanged(QAction *)));

	m_capMenu = menuBar()->addMenu("&Capture");
	m_capMenu->addAction(m_capStartAct);
	m_capMenu->addAction(m_capStepAct);
	m_capMenu->addMenu(m_overrideColorspaceMenu);
	m_capMenu->addMenu(m_overrideXferFuncMenu);
	m_capMenu->addMenu(m_overrideYCbCrEncMenu);
	m_capMenu->addMenu(m_overrideQuantizationMenu);
	m_capMenu->addAction(m_showFramesAct);
	m_capMenu->addAction(m_scalingAct);

	if (CaptureWinGL::isSupported()) {
		m_renderMethod = QV4L2_RENDER_GL;

		m_useGLAct = new QAction("Use Open&GL Rendering", this);
		m_useGLAct->setStatusTip("Use GPU with OpenGL for video capture if set.");
		m_useGLAct->setCheckable(true);
		m_useGLAct->setChecked(true);
		connect(m_useGLAct, SIGNAL(toggled(bool)), this, SLOT(setRenderMethod(bool)));
		m_capMenu->addAction(m_useGLAct);

		m_useBlendingAct = new QAction("Enable &Blending", this);
		m_useBlendingAct->setStatusTip("Enable blending to test the alpha component in the image");
		m_useBlendingAct->setCheckable(true);
		m_useBlendingAct->setChecked(false);
		connect(m_useBlendingAct, SIGNAL(toggled(bool)), this, SLOT(setBlending(bool)));
		m_capMenu->addAction(m_useBlendingAct);

		m_useLinearAct = new QAction("Enable &Linear filter", this);
		m_useLinearAct->setStatusTip("Enable linear scaling filter");
		m_useLinearAct->setCheckable(true);
		m_useLinearAct->setChecked(false);
		connect(m_useLinearAct, SIGNAL(toggled(bool)), this, SLOT(setLinearFilter(bool)));
		m_capMenu->addAction(m_useLinearAct);

	} else {
		m_renderMethod = QV4L2_RENDER_QT;
	}
	m_capMenu->addAction(m_resetScalingAct);
	
	m_makeFullScreenAct = new QAction(QIcon(":/fullscreen.png"), "Show Fullscreen", this);
	m_makeFullScreenAct->setStatusTip("Capture in fullscreen mode");
	m_makeFullScreenAct->setCheckable(true);
	connect(m_makeFullScreenAct, SIGNAL(toggled(bool)), this, SLOT(makeFullScreen(bool)));
	m_capMenu->addAction(m_makeFullScreenAct);
	toolBar->addAction(m_makeFullScreenAct);

#ifdef HAVE_ALSA
	m_capMenu->addSeparator();

	m_audioBufferAct = new QAction("Set Audio &Buffer Capacity...", this);
	m_audioBufferAct->setStatusTip("Set audio buffer capacity in amount of ms than can be stored");
	connect(m_audioBufferAct, SIGNAL(triggered()), this, SLOT(setAudioBufferSize()));
	m_capMenu->addAction(m_audioBufferAct);
#endif

	QMenu *helpMenu = menuBar()->addMenu("&Help");
	helpMenu->addAction("&About", this, SLOT(about()), Qt::Key_F1);

	QAction *whatAct = QWhatsThis::createAction(this);
	helpMenu->addAction(whatAct);
	toolBar->addAction(whatAct);

	statusBar()->showMessage("Ready", 2000);

	m_tabs = new QTabWidget;
	setCentralWidget(m_tabs);
}


ApplicationWindow::~ApplicationWindow()
{
	closeDevice();
}

void ApplicationWindow::updateColorspace()
{
	if (!m_capture)
		return;

	int colorspace = m_overrideColorspace;
	int xferFunc = m_overrideXferFunc;
	int ycbcrEnc = m_overrideYCbCrEnc;
	int quantRange = m_overrideQuantization;
	cv4l_fmt fmt(g_type());

	// don't use the wrapped ioctl since it doesn't
	// update colorspace correctly.
	::ioctl(g_fd(), VIDIOC_G_FMT, &fmt);

	if (colorspace == -1)
		colorspace = fmt.g_colorspace();
	if (xferFunc == -1)
		xferFunc = fmt.g_xfer_func();
	if (ycbcrEnc == -1)
		ycbcrEnc = fmt.g_ycbcr_enc();
	if (quantRange == -1)
		quantRange = fmt.g_quantization();
	m_capture->setColorspace(colorspace, xferFunc, ycbcrEnc, quantRange,
			m_genTab ? m_genTab->isSDTV() : true);
}

void ApplicationWindow::overrideColorspaceChanged(QAction *a)
{
	m_overrideColorspace = a->data().toInt();
	updateColorspace();
}

void ApplicationWindow::overrideXferFuncChanged(QAction *a)
{
	m_overrideXferFunc = a->data().toInt();
	updateColorspace();
}

void ApplicationWindow::overrideYCbCrEncChanged(QAction *a)
{
	m_overrideYCbCrEnc = a->data().toInt();
	updateColorspace();
}

void ApplicationWindow::overrideQuantChanged(QAction *a)
{
	m_overrideQuantization = a->data().toInt();
	updateColorspace();
}

void ApplicationWindow::setDevice(const QString &device, bool rawOpen)
{
	closeDevice();
	m_sigMapper = new QSignalMapper(this);
	connect(m_sigMapper, SIGNAL(mapped(int)), this, SLOT(ctrlAction(int)));

	s_direct(rawOpen);

	if (open(device.toLatin1(), true) < 0) {
#ifdef HAVE_ALSA
		m_audioBufferAct->setEnabled(false);
#endif
		return;
	}

	newCaptureWin();

	QWidget *w = new QWidget(m_tabs);
	m_genTab = new GeneralTab(device, this, 4, w);
	int m_winWidth = m_genTab->getWidth();

#ifdef HAVE_ALSA
	if (m_genTab->hasAlsaAudio()) {
		connect(m_genTab, SIGNAL(audioDeviceChanged()), this, SLOT(changeAudioDevice()));
		m_audioBufferAct->setEnabled(true);
	} else {
		m_audioBufferAct->setEnabled(false);
	}
#endif
	connect(m_genTab, SIGNAL(pixelAspectRatioChanged()), this, SLOT(updatePixelAspectRatio()));
	connect(m_genTab, SIGNAL(croppingChanged()), this, SLOT(updateCropping()));
	connect(m_genTab, SIGNAL(clearBuffers()), this, SLOT(clearBuffers()));
	m_tabs->addTab(w, "General Settings");

	if (has_vid_out()) {
		addTpgTab(m_minWidth);
		tpg_init(&m_tpg, 640, 360);
		updateLimRGBRange();
	}

	addTabs(m_winWidth);
	m_vbiTab = NULL;
	if (has_vbi_cap()) {
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
	m_convertData = v4lconvert_create(g_fd());
	bool canStream = has_rw() || has_streaming();
	bool isCapture = v4l_type_is_capture(g_type());
	m_capStartAct->setEnabled(canStream);
	m_capStepAct->setEnabled(canStream && isCapture);
	m_saveRawAct->setEnabled(canStream && has_vid_cap());
	m_snapshotAct->setEnabled(canStream && has_vid_cap());
	m_capMenu->setEnabled(canStream && isCapture && !has_radio_rx());
#ifdef HAVE_QTGL
	m_useGLAct->setEnabled(CaptureWinGL::isSupported());
#endif
	m_genTab->sourceChangeSubscribe();
	subscribeCtrlEvents();
	m_ctrlNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Exception, m_tabs);
	connect(m_ctrlNotifier, SIGNAL(activated(int)), this, SLOT(ctrlEvent()));
}

void ApplicationWindow::opendev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio* swradio*)");

	d.setFilter(QDir::AllDirs | QDir::Files | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), false);
}

void ApplicationWindow::openrawdev()
{
	QFileDialog d(this, "Select v4l device", "/dev", "V4L Devices (video* vbi* radio* swradio*)");

	d.setFilter(QDir::AllDirs | QDir::Files | QDir::System);
	d.setFileMode(QFileDialog::ExistingFile);
	if (d.exec())
		setDevice(d.selectedFiles().first(), true);
}

void ApplicationWindow::setRenderMethod(bool checked)
{
	if (m_capStartAct->isChecked()) {
		m_useGLAct->setChecked(m_renderMethod == QV4L2_RENDER_GL);
		return;
	}

	m_renderMethod = checked ? QV4L2_RENDER_GL : QV4L2_RENDER_QT;
	m_useBlendingAct->setEnabled(m_renderMethod == QV4L2_RENDER_GL);

	newCaptureWin();
}

void ApplicationWindow::setBlending(bool checked)
{
	if (m_capture)
		m_capture->setBlending(checked);
}

void ApplicationWindow::setLinearFilter(bool checked)
{
	if (m_capture)
		m_capture->setLinearFilter(checked);
}

void ApplicationWindow::setAudioBufferSize()
{
	bool ok;
	int buffer = QInputDialog::getInt(this, "Audio Device Buffer Size", "Capacity in ms:",
					   m_genTab->getAudioDeviceBufferSize(), 1, 65535, 1, &ok);

	if (ok) {
		m_genTab->setAudioDeviceBufferSize(buffer);
		changeAudioDevice();
	}
}


void ApplicationWindow::ctrlEvent()
{
	v4l2_event ev;

	while (dqevent(ev) == 0) {
		if (ev.type == V4L2_EVENT_SOURCE_CHANGE) {
			m_genTab->sourceChange(ev);
			continue;
		}
		if (ev.type != V4L2_EVENT_CTRL)
			continue;
		m_ctrlMap[ev.id].flags = ev.u.ctrl.flags;
		m_ctrlMap[ev.id].minimum = ev.u.ctrl.minimum;
		m_ctrlMap[ev.id].maximum = ev.u.ctrl.maximum;
		m_ctrlMap[ev.id].step = ev.u.ctrl.step;
		m_ctrlMap[ev.id].default_value = ev.u.ctrl.default_value;

		bool disabled = m_ctrlMap[ev.id].flags & CTRL_FLAG_DISABLED;

		if (qobject_cast<QLineEdit *>(m_widgetMap[ev.id]))
			static_cast<QLineEdit *>(m_widgetMap[ev.id])->setReadOnly(disabled);
		else
			m_widgetMap[ev.id]->setDisabled(disabled);
		if (m_sliderMap.find(ev.id) != m_sliderMap.end())
			m_sliderMap[ev.id]->setDisabled(disabled);
		if (ev.u.ctrl.changes & V4L2_EVENT_CTRL_CH_RANGE)
			updateCtrlRange(ev.id, ev.u.ctrl.value);
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
		query_ext_ctrl(m_ctrlMap[ev.id]);

		struct v4l2_ext_control c;
		struct v4l2_ext_controls ctrls;

		c.id = ev.id;
		c.size = m_ctrlMap[ev.id].maximum + 1;
		c.string = (char *)malloc(c.size);
		memset(&ctrls, 0, sizeof(ctrls));
		ctrls.count = 1;
		ctrls.which = 0;
		ctrls.controls = &c;
		if (!g_ext_ctrls(ctrls))
			setString(ev.id, c.string);
		free(c.string);
	}
}

void ApplicationWindow::newCaptureWin()
{
	if (m_capture != NULL) {
		m_capture->stop();
		delete m_capture;
	}

	switch (m_renderMethod) {
	case QV4L2_RENDER_GL:
		m_capture = new CaptureWinGL(this);
		break;
	default:
		m_capture = new CaptureWinQt(this);
		break;
	}

	m_capture->setPixelAspectRatio(1.0);
	m_capture->enableScaling(m_scalingAct->isChecked());
        connect(m_capture, SIGNAL(close()), this, SLOT(closeCaptureWin()));
}

bool ApplicationWindow::startStreaming()
{
	startAudio();

	if (!m_genTab->isSDR() && m_genTab->isRadio()) {
		s_priority(m_genTab->usePrio());
		return true;
	}

	m_queue.init(g_type(), m_capMethod);

#ifdef HAVE_QTGL
	m_useGLAct->setEnabled(false);
#endif

	switch (m_capMethod) {
	case methodRead:
		m_snapshotAct->setEnabled(true);
		m_genTab->setHaveBuffers(true);
		s_priority(m_genTab->usePrio());
		/* Nothing to do. */
		return true;

	case methodMmap:
	case methodUser:
		if (m_queue.reqbufs(this, m_genTab->getNumBuffers())) {
			error("Cannot capture");
			break;
		}

		if (m_queue.g_buffers() < 2) {
			error("Too few buffers");
			break;
		}

		if (m_queue.obtain_bufs(this)) {
			error("Get buffers");
			break;
		}

		if (v4l_type_is_capture(g_type())) {
			for (unsigned i = 0; i < m_queue.g_buffers(); i++) {
				cv4l_buffer buf;

				m_queue.buffer_init(buf, i);
				if (qbuf(buf)) {
					error("Couldn't queue buffer\n");
					goto error;
				}
			}
		} else {
			for (unsigned i = 0; i < m_queue.g_buffers(); i++) {
				cv4l_buffer buf;

				m_queue.buffer_init(buf, i);
				buf.s_field(m_tpgField);
				tpg_s_field(&m_tpg, m_tpgField, m_tpgFieldAlt);
				if (m_tpgField == V4L2_FIELD_TOP)
					m_tpgField = V4L2_FIELD_BOTTOM;
				else if (m_tpgField == V4L2_FIELD_BOTTOM)
					m_tpgField = V4L2_FIELD_TOP;
				for (unsigned p = 0; p < m_queue.g_num_planes(); p++) {
					tpg_fillbuffer(&m_tpg, m_tpgStd, p, (u8 *)m_queue.g_dataptr(i, p));
					buf.s_bytesused(buf.g_length(p), p);
				}
				if (qbuf(buf)) {
					error("Couldn't queue buffer\n");
					goto error;
				}
				tpg_update_mv_count(&m_tpg, V4L2_FIELD_HAS_T_OR_B(m_tpgField));
			}
		}

		if (streamon()) {
			perror("VIDIOC_STREAMON");
			break;
		}
		m_snapshotAct->setEnabled(true);
		m_genTab->setHaveBuffers(true);
		s_priority(m_genTab->usePrio());
		return true;
	}

error:
	m_queue.free(this);
	delete m_ctrlNotifier;
	reopen(true);
	m_genTab->sourceChangeSubscribe();
	subscribeCtrlEvents();
	m_ctrlNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Exception, m_tabs);
	connect(m_ctrlNotifier, SIGNAL(activated(int)), this, SLOT(ctrlEvent()));
	m_capStartAct->setChecked(false);
#ifdef HAVE_QTGL
	m_useGLAct->setEnabled(CaptureWinGL::isSupported());
#endif
	return false;
}

void ApplicationWindow::calculateFps()
{
	static unsigned last_sec;

	if (m_frame == 0) {
		clock_gettime(CLOCK_MONOTONIC, &m_startTimestamp);
		last_sec = 0;
	} else {
		struct timespec ts_cur, res;

		clock_gettime(CLOCK_MONOTONIC, &ts_cur);
		res.tv_sec = ts_cur.tv_sec - m_startTimestamp.tv_sec;
		res.tv_nsec = ts_cur.tv_nsec - m_startTimestamp.tv_nsec;
		if (res.tv_nsec < 0) {
			res.tv_sec--;
			res.tv_nsec += 1000000000;
		}
		if (res.tv_sec > last_sec) {
			m_fps = (10000 * m_frame) /
				(res.tv_sec * 100 + res.tv_nsec / 10000000);
			m_fps /= 100.0;
			last_sec = res.tv_sec;
		}
	}
}

void ApplicationWindow::capVbiFrame()
{
	cv4l_buffer buf(m_queue);
	__u8 *data = NULL;
	int s = 0;

	if (m_singleStep)
		m_capNotifier->setEnabled(false);

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
	case methodUser:
		if (dqbuf(buf)) {
			if (errno == EAGAIN)
				return;
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (buf.g_flags() & V4L2_BUF_FLAG_ERROR) {
			if (qbuf(buf)) {
				error("Couldn't queue buffer\n");
				m_capStartAct->setChecked(false);
			}
			return;
		}
		data = (__u8 *)m_queue.g_dataptr(buf.g_index(), 0);
		s = buf.g_bytesused();
		break;
	}
	if (g_type() == V4L2_BUF_TYPE_VBI_CAPTURE && s != m_vbiSize) {
		error("incorrect vbi size");
		m_capStartAct->setChecked(false);
		return;
	}
	if (showFrames() && g_type() == V4L2_BUF_TYPE_VBI_CAPTURE) {
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

	if (g_type() == V4L2_BUF_TYPE_SLICED_VBI_CAPTURE) {
		p = (struct v4l2_sliced_vbi_data *)data;
	} else {
		vbi_parse(&m_vbiHandle, data, &sfmt, sdata);
		s = sizeof(sdata);
		p = sdata;
	}

	if (m_capMethod != methodRead) {
		if (qbuf(buf)) {
			error("Couldn't queue buffer\n");
			m_capStartAct->setChecked(false);
			return;
		}
	}

	m_vbiTab->slicedData(p, s / sizeof(p[0]));

	QString status, curStatus;

	calculateFps();
	status = QString("Frame: %1 Fps: %2").arg(++m_frame).arg(m_fps, 0, 'f', 2, '0');
	if (showFrames() && g_type() == V4L2_BUF_TYPE_VBI_CAPTURE)
		m_capture->setFrame(m_capImage->width(), m_capImage->height(),
				    m_capDestFormat.fmt.pix.pixelformat, m_capImage->bits(),
				    NULL, NULL);

	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: "))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

void ApplicationWindow::capSdrFrame()
{
	cv4l_buffer buf(m_queue);
	__u8 *data = NULL;
	int s = 0;

	if (m_singleStep)
		m_capNotifier->setEnabled(false);

	switch (m_capMethod) {
	case methodRead:
		s = read(m_frameData, m_sdrSize);
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
	case methodUser:
		if (dqbuf(buf)) {
			if (errno == EAGAIN)
				return;
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (buf.g_flags() & V4L2_BUF_FLAG_ERROR) {
			if (qbuf(buf)) {
				error("Couldn't queue buffer\n");
				m_capStartAct->setChecked(false);
			}
			return;
		}
		data = (__u8 *)m_queue.g_dataptr(buf.g_index(), 0);
		s = buf.g_bytesused();
		break;
	}
	if (s != m_sdrSize) {
		error("incorrect sdr size");
		m_capStartAct->setChecked(false);
		return;
	}
	if (showFrames()) {
		unsigned width = m_sdrSize / 2 - 1;

		if (SDR_WIDTH < width)
			width = SDR_WIDTH;

		m_capImage->fill(0);
		/*
		 * Draw two waveforms, each consisting of the first 'width + 1' samples
		 * of the buffer, the top is for the I, the bottom is for the Q values.
		 */
		for (unsigned i = 0; i < 2; i++) {
			unsigned start = 255 - data[i];

			for (unsigned x = 0; x < width; x++) {
				unsigned next = 255 - data[2 + 2 * x + i];
				unsigned low = start < next ? start : next;
				unsigned high = start > next ? start : next;
				__u8 *q = m_capImage->bits() + x * 3 +
					(i * 256 + low) * m_capImage->bytesPerLine();

				while (low++ <= high) {
					q[0] = 255;
					q[1] = 255;
					q[2] = 255;
					q += m_capImage->bytesPerLine();
				}
				start = next;
			}
		}
	}

	if (m_capMethod != methodRead) {
		if (qbuf(buf)) {
			error("Couldn't queue buffer\n");
			m_capStartAct->setChecked(false);
			return;
		}
	}

	QString status, curStatus;

	calculateFps();
	status = QString("Frame: %1 Fps: %2").arg(++m_frame).arg(m_fps, 0, 'f', 2, '0');
	if (showFrames())
		m_capture->setFrame(m_capImage->width(), m_capImage->height(),
				    m_capDestFormat.fmt.pix.pixelformat, m_capImage->bits(),
				    NULL, NULL);

	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: "))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

void ApplicationWindow::outFrame()
{
	cv4l_buffer buf(m_queue);
	int s = 0;

	switch (m_capMethod) {
	case methodRead:
		tpg_fillbuffer(&m_tpg, m_tpgStd, 0, (u8 *)m_frameData);
		s = write(m_frameData, m_tpgSizeImage);
		tpg_update_mv_count(&m_tpg, V4L2_FIELD_HAS_T_OR_B(m_tpgField));

		if (s < 0) {
			if (errno != EAGAIN) {
				error("write");
				m_capStartAct->setChecked(false);
			}
			return;
		}
		break;

	case methodMmap:
	case methodUser:
		if (dqbuf(buf)) {
			if (errno == EAGAIN)
				return;
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		m_queue.buffer_init(buf, buf.g_index());
		buf.s_field(m_tpgField);
		tpg_s_field(&m_tpg, m_tpgField, m_tpgFieldAlt);
		if (m_tpgField == V4L2_FIELD_TOP)
			m_tpgField = V4L2_FIELD_BOTTOM;
		else if (m_tpgField == V4L2_FIELD_BOTTOM)
			m_tpgField = V4L2_FIELD_TOP;
		for (unsigned p = 0; p < m_queue.g_num_planes(); p++) {
			tpg_fillbuffer(&m_tpg, m_tpgStd, p, (u8 *)m_queue.g_dataptr(buf.g_index(), p));
			buf.s_bytesused(buf.g_length(p), p);
		}
		tpg_update_mv_count(&m_tpg, V4L2_FIELD_HAS_T_OR_B(m_tpgField));
		break;
	}

	QString status, curStatus;

	calculateFps();
	status = QString("Frame: %1 Fps: %2").arg(++m_frame).arg(m_fps, 0, 'f', 2, '0');

	if (m_capMethod == methodMmap || m_capMethod == methodUser) {
		if (m_clear[buf.g_index()]) {
			for (unsigned p = 0; p < m_queue.g_num_planes(); p++)
				memset(m_queue.g_dataptr(buf.g_index(), p), 0, buf.g_length(p));
			m_clear[buf.g_index()] = false;
		}
			
		if (qbuf(buf)) {
			error("Couldn't queue buffer\n");
			m_capStartAct->setChecked(false);
			return;
		}
	}

	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: "))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

void ApplicationWindow::capFrame()
{
	cv4l_buffer buf(m_queue);
	unsigned char *plane[3];
	unsigned bytesused[3];
	int s = 0;
	int err = 0;
#ifdef HAVE_ALSA
	struct timeval tv_alsa;
#endif

	if (m_singleStep)
		m_capNotifier->setEnabled(false);

	plane[0] = plane[1] = plane[2] = NULL;
	switch (m_capMethod) {
	case methodRead:
		s = read(m_frameData, m_capSrcFormat.g_sizeimage(0));
#ifdef HAVE_ALSA
		alsa_thread_timestamp(&tv_alsa);
#endif

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

		plane[0] = m_frameData;
		if (showFrames() && m_mustConvert) {
			err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
						 m_frameData, s,
						 m_capImage->bits(), m_capDestFormat.fmt.pix.sizeimage);
			if (err != -1)
				plane[0] = m_capImage->bits();
		}
		break;

	case methodMmap:
	case methodUser:
		if (dqbuf(buf)) {
			if (errno == EAGAIN)
				return;
			error("dqbuf");
			m_capStartAct->setChecked(false);
			return;
		}
		if (buf.g_flags() & V4L2_BUF_FLAG_ERROR) {
			printf("error\n");
			if (qbuf(buf)) {
				error("Couldn't queue buffer\n");
				m_capStartAct->setChecked(false);
			}
			return;
		}

#ifdef HAVE_ALSA
		alsa_thread_timestamp(&tv_alsa);
#endif

		plane[0] = (__u8 *)m_queue.g_dataptr(buf.g_index(), 0);
		plane[1] = (__u8 *)m_queue.g_dataptr(buf.g_index(), 1);
		plane[2] = (__u8 *)m_queue.g_dataptr(buf.g_index(), 2);
		plane[0] += buf.g_data_offset(0);
		bytesused[0] = buf.g_bytesused(0) - buf.g_data_offset(0);
		if (plane[1]) {
			plane[1] += buf.g_data_offset(1);
			bytesused[1] = buf.g_bytesused(1) - buf.g_data_offset(1);
		}
		if (plane[2]) {
			plane[2] += buf.g_data_offset(2);
			bytesused[2] = buf.g_bytesused(2) - buf.g_data_offset(2);
		}
		if (showFrames() && m_mustConvert) {
			err = v4lconvert_convert(m_convertData, &m_capSrcFormat, &m_capDestFormat,
						 plane[0], bytesused[0], m_capImage->bits(),
						 m_capDestFormat.fmt.pix.sizeimage);
			if (err != -1) {
				plane[0] = m_capImage->bits();
				bytesused[0] = m_capDestFormat.fmt.pix.sizeimage;
			}
		}
		if (m_makeSnapshot)
			makeSnapshot(plane[0], bytesused[0]);
		if (m_saveRaw.openMode())
			m_saveRaw.write((const char *)plane[0], bytesused[0]);

		break;
	}
	if (err == -1 && m_frame == 0)
		error(v4lconvert_get_error_message(m_convertData));

	QString status, curStatus;

	calculateFps();

	float wscale = m_capture->getHorScaleFactor();
	float hscale = m_capture->getVertScaleFactor();
	status = QString("Frame: %1 Fps: %2 Scale Factors: %3x%4").arg(++m_frame)
			 .arg(m_fps, 0, 'f', 2, '0').arg(wscale).arg(hscale);
	if (m_capMethod != methodRead)
		status.append(QString(" SeqNr: %1").arg(buf.g_sequence()));
#ifdef HAVE_ALSA
	if (m_capMethod != methodRead && alsa_thread_is_running()) {
		if (tv_alsa.tv_sec || tv_alsa.tv_usec) {
			m_totalAudioLatency.tv_sec += buf.g_timestamp().tv_sec - tv_alsa.tv_sec;
			m_totalAudioLatency.tv_usec += buf.g_timestamp().tv_usec - tv_alsa.tv_usec;
		}
		status.append(QString(" Average A-V: %1 ms")
			      .arg((m_totalAudioLatency.tv_sec * 1000 + m_totalAudioLatency.tv_usec / 1000) / m_frame));
	}
#endif
	if (plane[0] == NULL && showFrames())
		status.append(" Error: Unsupported format.");

	if (showFrames())
		m_capture->setFrame(m_capImage->width(), m_capImage->height(),
				    m_capDestFormat.g_pixelformat(),
				    plane[0], plane[1], plane[2]);

	if (m_capMethod == methodMmap || m_capMethod == methodUser) {
		if (m_clear[buf.g_index()]) {
			memset(m_queue.g_dataptr(buf.g_index(), 0), 0, buf.g_length());
			if (V4L2_TYPE_IS_MULTIPLANAR(buf.g_type())) {
				memset(m_queue.g_dataptr(buf.g_index(), 1), 0, buf.g_length(1));
				if (m_queue.g_dataptr(buf.g_index(), 2))
					memset(m_queue.g_dataptr(buf.g_index(), 2), 0, buf.g_length(2));
			}
			m_clear[buf.g_index()] = false;
		}
			
		if (qbuf(buf)) {
			error("Couldn't queue buffer\n");
			m_capStartAct->setChecked(false);
			return;
		}
	}

	curStatus = statusBar()->currentMessage();
	if (curStatus.isEmpty() || curStatus.startsWith("Frame: ") || curStatus.startsWith("No frame"))
		statusBar()->showMessage(status);
	if (m_frame == 1)
		refresh();
}

void ApplicationWindow::stopStreaming()
{
	bool canStream = g_fd() >= 0 && (v4l_type_is_capture(g_type()) || has_vid_out()) &&
					 !has_radio_tx();
	v4l2_encoder_cmd cmd;

	m_singleStep = false;
	m_capStepAct->setEnabled(canStream && v4l_type_is_capture(g_type()));
	stopAudio();

	s_priority(V4L2_PRIORITY_DEFAULT);

	if (!m_genTab->isSDR() && m_genTab->isRadio())
		return;

	if (v4l_type_is_capture(g_type()))
		m_capture->stop();

	m_snapshotAct->setDisabled(true);
#ifdef HAVE_QTGL
	m_useGLAct->setEnabled(CaptureWinGL::isSupported());
#endif
	switch (m_capMethod) {
	case methodRead:
		if (v4l_type_is_capture(g_type())) {
			memset(&cmd, 0, sizeof(cmd));
			cmd.cmd = V4L2_ENC_CMD_STOP;
			encoder_cmd(cmd);
		}
		break;

	case methodMmap:
	case methodUser:
		m_queue.free(this);
		break;
	}
	delete m_ctrlNotifier;
	reopen(true);
	m_genTab->sourceChangeSubscribe();
	subscribeCtrlEvents();
	m_ctrlNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Exception, m_tabs);
	connect(m_ctrlNotifier, SIGNAL(activated(int)), this, SLOT(ctrlEvent()));
	m_genTab->setHaveBuffers(false);
	refresh();
}

bool ApplicationWindow::showFrames()
{
	if (m_showFramesAct->isChecked() && !m_capture->isVisible() &&
	    !m_genTab->isSlicedVbi())
		m_capture->show();
	if ((!m_showFramesAct->isChecked() && m_capture->isVisible()) ||
	    m_genTab->isSlicedVbi())
		m_capture->hide();
	return m_showFramesAct->isChecked();
}

void ApplicationWindow::traceIoctls(bool enable)
{
	s_trace(enable);
}

void ApplicationWindow::enableScaling(bool enable)
{
	if (m_capture != NULL)
		m_capture->enableScaling(enable);
}

void ApplicationWindow::updatePixelAspectRatio()
{
	if (m_capture != NULL && m_genTab != NULL)
		m_capture->setPixelAspectRatio(m_genTab->getPixelAspectRatio());
}

void ApplicationWindow::updateCropping()
{
	if (m_capture != NULL)
		m_capture->setCropMethod(m_genTab->getCropMethod());
}

void ApplicationWindow::clearBuffers()
{
	if (m_capture)
		for (unsigned b = 0; b < sizeof(m_clear); b++)
			m_clear[b] = true;
}

void ApplicationWindow::startAudio()
{
#ifdef HAVE_ALSA
	m_totalAudioLatency.tv_sec = 0;
	m_totalAudioLatency.tv_usec = 0;

	QString audIn = m_genTab->getAudioInDevice();
	QString audOut = m_genTab->getAudioOutDevice();

	if (audIn != NULL && audOut != NULL && audIn.compare("None") && audIn.compare(audOut) != 0) {
		alsa_thread_startup(audOut.toLatin1().data(), audIn.toLatin1().data(),
				    m_genTab->getAudioDeviceBufferSize(), NULL, 0);

		if (m_genTab->isRadio())
			statusBar()->showMessage("Capturing audio");
	}
#endif
}

void ApplicationWindow::stopAudio()
{
#ifdef HAVE_ALSA
	if (m_genTab != NULL && m_genTab->isRadio())
		statusBar()->showMessage("");
	alsa_thread_stop();
#endif
}

void ApplicationWindow::changeAudioDevice()
{
	stopAudio();
	if (m_capStartAct->isChecked()) {
		startAudio();
	}
}

void ApplicationWindow::closeCaptureWin()
{
	m_capStartAct->setChecked(false);
}

void ApplicationWindow::outStart(bool start)
{
	if (start) {
		cv4l_fmt fmt;
		v4l2_output out;
		v4l2_control ctrl = { V4L2_CID_DV_TX_RGB_RANGE };
		unsigned p;
		int factor = 1;

		g_output(out.index);
		enum_output(out, true, out.index);
		m_frame = m_fps = 0;
		m_capMethod = m_genTab->capMethod();
		g_fmt(fmt);
		fmt.s_flags(0);
		if (out.capabilities & V4L2_OUT_CAP_STD)
			g_std(m_tpgStd);
		else
			m_tpgStd = 0;
		m_tpgField = fmt.g_first_field(m_tpgStd);
		m_tpgFieldAlt = fmt.g_field() == V4L2_FIELD_ALTERNATE;
		m_tpgSizeImage = fmt.g_sizeimage(0);
		tpg_alloc(&m_tpg, fmt.g_width());
		m_useTpg = tpg_s_fourcc(&m_tpg, fmt.g_pixelformat());
		if (V4L2_FIELD_HAS_T_OR_B(fmt.g_field()))
			factor = 2;
		tpg_reset_source(&m_tpg, fmt.g_width(), fmt.g_height() * factor, fmt.g_field());
		tpg_update_mv_step(&m_tpg);
		tpg_init_mv_count(&m_tpg);
		if (g_ctrl(ctrl))
			tpg_s_rgb_range(&m_tpg, V4L2_DV_RGB_RANGE_AUTO);
		else
			tpg_s_rgb_range(&m_tpg, ctrl.value);
		tpgColorspaceChanged();
		s_fmt(fmt);

		if (out.capabilities & V4L2_OUT_CAP_STD) {
			tpg_s_pixel_aspect(&m_tpg, (m_tpgStd & V4L2_STD_525_60) ?
					TPG_PIXEL_ASPECT_NTSC : TPG_PIXEL_ASPECT_PAL);
		} else if (out.capabilities & V4L2_OUT_CAP_DV_TIMINGS) {
			v4l2_dv_timings timings;

			g_dv_timings(timings);
			if (timings.bt.width == 720 && timings.bt.height <= 576)
				tpg_s_pixel_aspect(&m_tpg, timings.bt.height == 480 ?
					TPG_PIXEL_ASPECT_NTSC : TPG_PIXEL_ASPECT_PAL);
			else
				tpg_s_pixel_aspect(&m_tpg, TPG_PIXEL_ASPECT_SQUARE);
		} else {
			tpg_s_pixel_aspect(&m_tpg, TPG_PIXEL_ASPECT_SQUARE);
		}

		for (p = 0; p < fmt.g_num_planes(); p++)
			tpg_s_bytesperline(&m_tpg, p, fmt.g_bytesperline(p));
		if (m_capMethod == methodRead)
			m_frameData = new unsigned char[fmt.g_sizeimage(0)];
		if (startStreaming()) {
			m_outNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Write, m_tabs);
			connect(m_outNotifier, SIGNAL(activated(int)), this, SLOT(outFrame()));
		}
	} else {
		stopStreaming();
		tpg_free(&m_tpg);
		delete m_frameData;
		m_frameData = NULL;
		delete m_outNotifier;
		m_outNotifier = NULL;
	}
}

void ApplicationWindow::capStep(bool checked)
{
	if (!m_capStartAct->isChecked()) {
		m_singleStep = true;
		m_capStartAct->setChecked(true);
	} else if (m_singleStep) {
		m_capNotifier->setEnabled(true);
	}
}

void ApplicationWindow::capStart(bool start)
{
	if (!m_singleStep)
		m_capStepAct->setDisabled(true);
	if (m_genTab->isRadio() && !m_genTab->isSDR()) {
		if (start)
			startAudio();
		else
			stopAudio();
		return;
	}
	if (has_vid_out()) {
		outStart(start);
		return;
	}

	if (!m_genTab->isSDR() && m_genTab->isRadio()) {
		if (start)
			startStreaming();
		else
			stopStreaming();

		return;
	}

	QImage::Format dstFmt = QImage::Format_RGB888;
	struct v4l2_fract interval;
	__u32 width, height, pixfmt;
	unsigned field;

	if (!start) {
		stopStreaming();
		delete m_capNotifier;
		m_capNotifier = NULL;
		delete m_capImage;
		m_capImage = NULL;
		return;
	}
	m_frame = m_fps = 0;
	m_capMethod = m_genTab->capMethod();

	if (m_genTab->isSlicedVbi()) {
		cv4l_fmt fmt;
		v4l2_std_id std;

		s_type(V4L2_BUF_TYPE_SLICED_VBI_CAPTURE);
		if (g_std(std)) {
			error("this input isn't suitable for VBI\n");
			return;
		}
		if (g_fmt(fmt)) {
			error("could not obtain an VBI format\n");
			return;
		}
		fmt.fmt.sliced.service_set = (std & V4L2_STD_525_60) ?
			V4L2_SLICED_VBI_525 : V4L2_SLICED_VBI_625;
		s_fmt(fmt);
		memset(&m_vbiHandle, 0, sizeof(m_vbiHandle));
		m_vbiTab->slicedFormat(fmt.fmt.sliced);
		m_vbiSize = fmt.fmt.sliced.io_size;
		m_frameData = new unsigned char[m_vbiSize];
		if (startStreaming()) {
			m_capNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Read, m_tabs);
			connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capVbiFrame()));
		}
		return;
	}
	if (m_genTab->isVbi()) {
		cv4l_fmt fmt;
		v4l2_std_id std;

		s_type(V4L2_BUF_TYPE_VBI_CAPTURE);
		if (g_std(std)) {
			error("this input isn't suitable for VBI\n");
			return;
		}
		if (g_fmt(fmt)) {
			error("could not obtain an VBI format\n");
			return;
		}
		if (fmt.fmt.vbi.sample_format != V4L2_PIX_FMT_GREY) {
			error("non-grey pixelformat not supported for VBI\n");
			return;
		}
		s_fmt(fmt);
		if (!vbi_prepare(&m_vbiHandle, &fmt.fmt.vbi, std)) {
			error("no services possible\n");
			return;
		}
		m_capDestFormat.s_type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
		m_capDestFormat.s_pixelformat(V4L2_PIX_FMT_RGB24);
		m_vbiTab->rawFormat(fmt.fmt.vbi);
		m_vbiWidth = fmt.fmt.vbi.samples_per_line;
		m_vbiHeight = fmt.fmt.vbi.count[0] + fmt.fmt.vbi.count[1];
		m_vbiSize = m_vbiWidth * m_vbiHeight;
		m_frameData = new unsigned char[m_vbiSize];
		m_capImage = new QImage(m_vbiWidth, m_vbiHeight, dstFmt);
		m_capImage->fill(0);
		m_capture->setWindowSize(QSize(m_vbiWidth, m_vbiHeight));
		m_capture->setFrame(m_capImage->width(), m_capImage->height(),
				    m_capDestFormat.fmt.pix.pixelformat, m_capImage->bits(), NULL, NULL);
		if (showFrames())
			m_capture->show();

		statusBar()->showMessage("No frame");
		if (startStreaming()) {
			m_capNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Read, m_tabs);
			connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capVbiFrame()));
		}
		return;
	}

	if (m_genTab->isSDR()) {
		cv4l_fmt fmt;

		if (g_fmt(fmt)) {
			error("could not obtain an VBI format\n");
			return;
		}
		if (fmt.fmt.sdr.pixelformat != V4L2_SDR_FMT_CU8) {
			error("only CU8 is supported for SDR\n");
			return;
		}
		m_sdrSize = fmt.fmt.sdr.buffersize;
		m_capDestFormat.s_type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
		m_capDestFormat.s_pixelformat(V4L2_PIX_FMT_RGB24);
		m_frameData = new unsigned char[m_sdrSize];
		m_capImage = new QImage(SDR_WIDTH, SDR_HEIGHT, dstFmt);
		m_capImage->fill(0);
		m_capture->setWindowSize(QSize(SDR_WIDTH, SDR_HEIGHT));
		m_capture->setFrame(m_capImage->width(), m_capImage->height(),
				    m_capDestFormat.fmt.pix.pixelformat, m_capImage->bits(), NULL, NULL);
		if (showFrames())
			m_capture->show();

		statusBar()->showMessage("No frame");
		if (startStreaming()) {
			m_capNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Read, m_tabs);
			connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capSdrFrame()));
		}
		return;
	}

	m_capSrcFormat.s_type(g_type());
	if (g_fmt(m_capSrcFormat)) {
		error("could not obtain a source format\n");
		return;
	}
	s_fmt(m_capSrcFormat);
	if (m_genTab->get_interval(interval))
		set_interval(interval);

	m_frameData = new unsigned char[m_capSrcFormat.g_sizeimage(0) +
					m_capSrcFormat.g_sizeimage(1)];
	m_capDestFormat = m_capSrcFormat;

	if (m_capture->hasNativeFormat(m_capSrcFormat.g_pixelformat())) {
		width = m_capSrcFormat.g_width();
		height = m_capSrcFormat.g_height();
		pixfmt = m_capSrcFormat.g_pixelformat();
		field = m_capSrcFormat.g_field();
		m_mustConvert = false;
	} else {
		m_mustConvert = true;

		m_capDestFormat.s_pixelformat(V4L2_PIX_FMT_RGB24);
		// Make sure sizeimage is large enough. This is necessary if the mplane
		// plugin is in use since v4lconvert_try_format() bypasses the plugin.
		m_capDestFormat.s_sizeimage(m_capDestFormat.g_width() *
					    m_capDestFormat.g_height() * 3);
		cv4l_fmt copy = m_capSrcFormat;
		v4lconvert_try_format(m_convertData, &m_capDestFormat, &m_capSrcFormat);
		// v4lconvert_try_format sometimes modifies the source format if it thinks
		// that there is a better format available. Restore our selected source
		// format since we do not want that happening.
		m_capSrcFormat = copy;
		width = m_capDestFormat.g_width();
		height = m_capDestFormat.g_height();
		pixfmt = m_capDestFormat.g_pixelformat();
		field = m_capDestFormat.g_field();
	}

	// Ensure that the initial image is large enough for native 32 bit per pixel formats
	switch (pixfmt) {
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
		dstFmt = QImage::Format_RGB32;
		break;
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_YUV32:
		dstFmt = QImage::Format_ARGB32;
		break;
	}
	m_capImage = new QImage(width, height, dstFmt);
	m_capImage->fill(0);
	
	updatePixelAspectRatio();
	m_capture->setField(field);

	m_capture->setWindowSize(QSize(width, height));
	m_capture->setFrame(m_capImage->width(), m_capImage->height(),
			    pixfmt, m_capImage->bits(), NULL, NULL);
	m_capture->makeFullScreen(m_makeFullScreenAct->isChecked());
	updateColorspace();
	if (showFrames())
		m_capture->show();

	statusBar()->showMessage("No frame");
	if (startStreaming()) {
		m_capNotifier = new QSocketNotifier(g_fd(), QSocketNotifier::Read, m_tabs);
		connect(m_capNotifier, SIGNAL(activated(int)), this, SLOT(capFrame()));
	}
}

void ApplicationWindow::makeFullScreen(bool checked)
{
	if (m_capture && m_capStartAct->isChecked())
		m_capture->makeFullScreen(checked);
}

void ApplicationWindow::closeDevice()
{
	stopAudio();
	delete m_sigMapper;
	m_sigMapper = NULL;
	m_capStartAct->setEnabled(false);
	m_capStartAct->setChecked(false);
	m_capStepAct->setEnabled(false);
	m_saveRawAct->setEnabled(false);
	if (g_fd() >= 0) {
		if (m_capNotifier) {
			delete m_capNotifier;
			delete m_capImage;
			m_capNotifier = NULL;
			m_capImage = NULL;
		}
		if (m_outNotifier) {
			delete m_outNotifier;
			m_outNotifier = NULL;
		}
		if (m_ctrlNotifier) {
			delete m_ctrlNotifier;
			m_ctrlNotifier = NULL;
		}
		delete [] m_frameData;
		m_frameData = NULL;
		v4lconvert_destroy(m_convertData);
		cv4l_fd::close();
		delete m_capture;
		m_capture = NULL;
	}
	while (QWidget *page = m_tabs->widget(0)) {
		m_tabs->removeTab(0);
		delete page;
	}
	m_genTab = NULL;
	m_ctrlMap.clear();
	m_widgetMap.clear();
	m_sliderMap.clear();
	m_classMap.clear();
	m_tpgLimRGBRange = NULL;
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
#ifdef HAVE_ALSA
	bool alsa = true;
#else
	bool alsa = false;
#endif
#ifdef HAVE_QTGL
	bool gl = true;
#else
	bool gl = false;
#endif

	QMessageBox::about(this, "V4L2 Test Bench",
			   QString("This program allows easy experimenting with video4linux devices.\n"
				   "v. %1\n\nALSA support : %2\nOpenGL support : %3")
			   .arg(V4L_UTILS_VERSION)
			   .arg(alsa ? "Present" : "Not Available")
			   .arg(gl ? "Present" : "Not Available")
			   );
}

void ApplicationWindow::error(const QString &error)
{
	statusBar()->showMessage(error, 20000);
	if (!error.isEmpty())
		fprintf(stderr, "%s\n", error.toLatin1().data());
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

static void usage()
{
	printf("  Usage:\n"
	       "  qv4l2 [-R] [-h] [-d <dev>] [-r <dev>] [-V <dev>] [-S <dev>]\n"
	       "\n  -d, --device=<dev> use device <dev> as the video device\n"
	       "                     if <dev> is a number, then /dev/video<dev> is used\n"
	       "  -V, --vbi-device=<dev> use device <dev> as the vbi device\n"
	       "                     if <dev> is a number, then /dev/vbi<dev> is used\n"
	       "  -r, --radio-device=<dev> use device <dev> as the radio device\n"
	       "                     if <dev> is a number, then /dev/radio<dev> is used\n"
	       "  -S, --sdr-device=<dev> use device <dev> as the SDR device\n"
	       "                     if <dev> is a number, then /dev/swradio<dev> is used\n"
	       "  -h, --help         display this help message\n"
	       "  -R, --raw          open device in raw mode.\n");
}

static void usageError(const char *msg)
{
	printf("Missing parameter for %s\n", msg);
	usage();
}

static QString getDeviceName(QString dev, QString &name)
{
	bool ok;
	name.toInt(&ok);
	return ok ? QString("%1%2").arg(dev).arg(name) : name;
}

static bool processShortOption(const QStringList &args, int &i, QString &dev)
{
	if (args[i].length() < 2)
		return false;
	if (args[i].length() == 2) {
		if (i + 1 >= args.size()) {
			usageError(args[i].toLatin1());
			return false;
		}
		dev = args[++i];
		return true;
	}
	dev = args[i].mid(2);
	return true;
}

static bool processLongOption(const QStringList &args, int &i, QString &dev)
{
	int index = args[i].indexOf('=');

	if (index >= 0) {
		dev = args[i].mid(index + 1);
		if (dev.length() == 0) {
			usageError("--device");
			return false;
		}
		return true;
	}
	if (i + 1 >= args.size()) {
		usageError(args[i].toLatin1());
		return false;
	}
	dev = args[++i];
	return true;
}

int main(int argc, char **argv)
{
	QApplication a(argc, argv);
	bool raw = false;
	QString device;
	QString video_device;
	QString vbi_device;
	QString radio_device;
	QString sdr_device;

	a.setWindowIcon(QIcon(":/qv4l2.png"));
	g_mw = new ApplicationWindow();
	g_mw->setWindowTitle("V4L2 Test Bench");

	QStringList args = a.arguments();
	for (int i = 1; i < args.size(); i++) {
		if (args[i].startsWith("-d")) {
			if (!processShortOption(args, i, video_device))
				return 0;
		} else if (args[i].startsWith("--device")) {
			if (!processLongOption(args, i, video_device))
				return 0;
		} else if (args[i].startsWith("-V")) {
			if (!processShortOption(args, i, vbi_device))
				return 0;
		} else if (args[i].startsWith("--vbi-device")) {
			if (!processLongOption(args, i, vbi_device))
				return 0;
		} else if (args[i].startsWith("-r")) {
			if (!processShortOption(args, i, radio_device))
				return 0;
		} else if (args[i].startsWith("--radio-device")) {
			if (!processLongOption(args, i, radio_device))
				return 0;
		} else if (args[i].startsWith("-S")) {
			if (!processShortOption(args, i, sdr_device))
				return 0;
		} else if (args[i].startsWith("--sdr-device")) {
			if (!processLongOption(args, i, sdr_device))
				return 0;
		} else if (args[i] == "-h" || args[i] == "--help") {
			usage();
			return 0;

		} else if (args[i] == "-R" || args[i] == "--raw") {
			raw = true;
		} else {
			printf("Invalid argument %s\n", args[i].toLatin1().data());
			return 0;
		}
	}

	if (video_device != NULL)
		device = getDeviceName("/dev/video", video_device);
	else if (vbi_device != NULL)
		device = getDeviceName("/dev/vbi", vbi_device);
	else if (radio_device != NULL)
		device = getDeviceName("/dev/radio", radio_device);
	else if (sdr_device != NULL)
		device = getDeviceName("/dev/swradio", sdr_device);
	else
		device = "/dev/video0";

	g_mw->setDevice(device, raw);
	g_mw->show();
	a.connect(&a, SIGNAL(lastWindowClosed()), &a, SLOT(quit()));
	return a.exec();
}
