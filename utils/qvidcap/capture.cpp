// SPDX-License-Identifier: GPL-2.0-only
/*
 * The YUY2 shader code is based on face-responder. The code is under public domain:
 * https://bitbucket.org/nateharward/face-responder/src/0c3b4b957039d9f4bf1da09b9471371942de2601/yuv42201_laplace.frag?at=master
 *
 * All other OpenGL code:
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include "capture.h"

#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtCore/QSocketNotifier>
#include <QtMath>
#include <QTimer>
#include <QApplication>

#include <netinet/in.h>
#include "v4l2-info.h"

const __u32 formats[] = {
	V4L2_PIX_FMT_YUYV,
	V4L2_PIX_FMT_YVYU,
	V4L2_PIX_FMT_UYVY,
	V4L2_PIX_FMT_VYUY,
	V4L2_PIX_FMT_YUV422P,
	V4L2_PIX_FMT_YVU420,
	V4L2_PIX_FMT_YUV420,
	V4L2_PIX_FMT_NV12,
	V4L2_PIX_FMT_NV21,
	V4L2_PIX_FMT_NV16,
	V4L2_PIX_FMT_NV61,
	V4L2_PIX_FMT_NV24,
	V4L2_PIX_FMT_NV42,
	V4L2_PIX_FMT_NV16M,
	V4L2_PIX_FMT_NV61M,
	V4L2_PIX_FMT_YVU420M,
	V4L2_PIX_FMT_YUV420M,
	V4L2_PIX_FMT_YVU422M,
	V4L2_PIX_FMT_YUV422M,
	V4L2_PIX_FMT_YVU444M,
	V4L2_PIX_FMT_YUV444M,
	V4L2_PIX_FMT_NV12M,
	V4L2_PIX_FMT_NV21M,
	V4L2_PIX_FMT_YUV444,
	V4L2_PIX_FMT_YUV555,
	V4L2_PIX_FMT_YUV565,
	V4L2_PIX_FMT_YUV32,
	V4L2_PIX_FMT_RGB32,
	V4L2_PIX_FMT_XRGB32,
	V4L2_PIX_FMT_ARGB32,
	V4L2_PIX_FMT_BGR32,
	V4L2_PIX_FMT_XBGR32,
	V4L2_PIX_FMT_ABGR32,
	V4L2_PIX_FMT_RGB24,
	V4L2_PIX_FMT_BGR24,
	V4L2_PIX_FMT_RGB565,
	V4L2_PIX_FMT_RGB565X,
	V4L2_PIX_FMT_RGB444,
	V4L2_PIX_FMT_XRGB444,
	V4L2_PIX_FMT_ARGB444,
	V4L2_PIX_FMT_RGB555,
	V4L2_PIX_FMT_XRGB555,
	V4L2_PIX_FMT_ARGB555,
	V4L2_PIX_FMT_RGB555X,
	V4L2_PIX_FMT_XRGB555X,
	V4L2_PIX_FMT_ARGB555X,
	V4L2_PIX_FMT_RGB332,
	V4L2_PIX_FMT_BGR666,
	V4L2_PIX_FMT_SBGGR8,
	V4L2_PIX_FMT_SGBRG8,
	V4L2_PIX_FMT_SGRBG8,
	V4L2_PIX_FMT_SRGGB8,
	V4L2_PIX_FMT_SBGGR10,
	V4L2_PIX_FMT_SGBRG10,
	V4L2_PIX_FMT_SGRBG10,
	V4L2_PIX_FMT_SRGGB10,
	V4L2_PIX_FMT_SBGGR12,
	V4L2_PIX_FMT_SGBRG12,
	V4L2_PIX_FMT_SGRBG12,
	V4L2_PIX_FMT_SRGGB12,
	V4L2_PIX_FMT_SBGGR16,
	V4L2_PIX_FMT_SGBRG16,
	V4L2_PIX_FMT_SGRBG16,
	V4L2_PIX_FMT_SRGGB16,
	V4L2_PIX_FMT_HSV24,
	V4L2_PIX_FMT_HSV32,
	V4L2_PIX_FMT_GREY,
	V4L2_PIX_FMT_Y10,
	V4L2_PIX_FMT_Y12,
	V4L2_PIX_FMT_Y16,
	V4L2_PIX_FMT_Y16_BE,
	V4L2_PIX_FMT_Z16,
	0
};

const __u32 fields[] = {
	V4L2_FIELD_NONE,
	V4L2_FIELD_TOP,
	V4L2_FIELD_BOTTOM,
	V4L2_FIELD_INTERLACED,
	V4L2_FIELD_SEQ_TB,
	V4L2_FIELD_SEQ_BT,
	V4L2_FIELD_ALTERNATE,
	V4L2_FIELD_INTERLACED_TB,
	V4L2_FIELD_INTERLACED_BT,
	0
};

const __u32 colorspaces[] = {
	V4L2_COLORSPACE_SMPTE170M,
	V4L2_COLORSPACE_SMPTE240M,
	V4L2_COLORSPACE_REC709,
	V4L2_COLORSPACE_470_SYSTEM_M,
	V4L2_COLORSPACE_470_SYSTEM_BG,
	V4L2_COLORSPACE_SRGB,
	V4L2_COLORSPACE_OPRGB,
	V4L2_COLORSPACE_DCI_P3,
	V4L2_COLORSPACE_BT2020,
	0
};

const __u32 xfer_funcs[] = {
	V4L2_XFER_FUNC_709,
	V4L2_XFER_FUNC_SRGB,
	V4L2_XFER_FUNC_OPRGB,
	V4L2_XFER_FUNC_DCI_P3,
	V4L2_XFER_FUNC_SMPTE2084,
	V4L2_XFER_FUNC_SMPTE240M,
	V4L2_XFER_FUNC_NONE,
	0
};

const __u32 ycbcr_encs[] = {
	V4L2_YCBCR_ENC_601,
	V4L2_YCBCR_ENC_709,
	V4L2_YCBCR_ENC_XV601,
	V4L2_YCBCR_ENC_XV709,
	V4L2_YCBCR_ENC_BT2020,
	V4L2_YCBCR_ENC_BT2020_CONST_LUM,
	V4L2_YCBCR_ENC_SMPTE240M,
	0
};

const __u32 hsv_encs[] = {
	V4L2_HSV_ENC_180,
	V4L2_HSV_ENC_256,
	0
};

const __u32 quantizations[] = {
	V4L2_QUANTIZATION_FULL_RANGE,
	V4L2_QUANTIZATION_LIM_RANGE,
	0
};

enum {
	CAPTURE_GL_WIN_RESIZE,
	CAPTURE_GL_WIN_SCROLLBAR,
};

static void checkSubMenuItem(QMenu *menu, __u32 value)
{
	QList<QAction *> actions = menu->actions();
	QList<QAction *>::iterator iter;

	for (iter = actions.begin(); iter != actions.end(); ++iter)
		if ((*iter)->data() == value)
			break;
	if (iter != actions.end())
		(*iter)->setChecked(true);
}

static QAction *addSubMenuItem(QActionGroup *grp, QMenu *menu, const QString &text, int val)
{
	QAction *a = grp->addAction(menu->addAction(text));

	a->setData(QVariant(val));
	a->setCheckable(true);
	return a;
}

CaptureWin::CaptureWin(QScrollArea *sa, QWidget *parent) :
	QOpenGLWidget(parent),
	m_fd(0),
	m_sock(0),
	m_v4l_queue(0),
	m_frame(0),
	m_ctx(0),
	m_origPixelFormat(0),
	m_fps(0),
	m_singleStep(false),
	m_singleStepStart(0),
	m_singleStepNext(false),
	m_screenTextureCount(0),
	m_program(0),
	m_curSize(),
	m_curIndex(-1),
	m_nextIndex(-1),
	m_scrollArea(sa)
{
	m_canOverrideResolution = false;
	m_pixelaspect.numerator = 1;
	m_pixelaspect.denominator = 1;
	QMenu *menu = new QMenu("Override Pixel Format (P)");
	m_fmtMenu = menu;
	QActionGroup *grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", 0)->setChecked(true);
	for (unsigned i = 0; formats[i]; i++) {
		std::string fmt = "'" + fcc2s(formats[i]) + "' " + pixfmt2s(formats[i]);

		addSubMenuItem(grp, menu, fmt.c_str(), formats[i]);
	}
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(fmtChanged(QAction *)));

	menu = new QMenu("Override Field (I)");
	m_fieldMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; fields[i]; i++)
		addSubMenuItem(grp, menu,
			       field2s(fields[i]).c_str(), fields[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(fieldChanged(QAction *)));

	menu = new QMenu("Override Colorspace (C)");
	m_colorspaceMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; colorspaces[i]; i++)
		addSubMenuItem(grp, menu,
			       colorspace2s(colorspaces[i]).c_str(), colorspaces[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(colorspaceChanged(QAction *)));

	menu = new QMenu("Override Transfer Function (X)");
	m_xferFuncMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; xfer_funcs[i]; i++)
		addSubMenuItem(grp, menu,
			       xfer_func2s(xfer_funcs[i]).c_str(), xfer_funcs[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(xferFuncChanged(QAction *)));

	menu = new QMenu("Override Y'CbCr Encoding (Y)");
	m_ycbcrEncMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; ycbcr_encs[i]; i++)
		addSubMenuItem(grp, menu,
			       ycbcr_enc2s(ycbcr_encs[i]).c_str(), ycbcr_encs[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(ycbcrEncChanged(QAction *)));

	menu = new QMenu("Override HSV Encoding (H)");
	m_hsvEncMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; hsv_encs[i]; i++)
		addSubMenuItem(grp, menu,
			       ycbcr_enc2s(hsv_encs[i]).c_str(), hsv_encs[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(hsvEncChanged(QAction *)));

	menu = new QMenu("Override Quantization (R)");
	m_quantMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "No Override", -1)->setChecked(true);
	for (unsigned i = 0; quantizations[i]; i++)
		addSubMenuItem(grp, menu,
			       quantization2s(quantizations[i]).c_str(), quantizations[i]);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(quantChanged(QAction *)));

	menu = new QMenu("Display Options");
	m_displayMenu = menu;
	grp = new QActionGroup(menu);
	addSubMenuItem(grp, menu, "Frame Resize", CAPTURE_GL_WIN_RESIZE)->setChecked(true);
	addSubMenuItem(grp, menu, "Window Scrollbars", CAPTURE_GL_WIN_SCROLLBAR)->setChecked(false);
	connect(grp, SIGNAL(triggered(QAction *)), this, SLOT(windowScalingChanged(QAction *)));

	m_resolutionOverride = new QAction("Override resolution", this);
	m_resolutionOverride->setCheckable(true);
	connect(m_resolutionOverride, SIGNAL(triggered(bool)),
		this, SLOT(resolutionOverrideChanged(bool)));

	m_enterFullScreen = new QAction("Enter fullscreen (F)", this);
	connect(m_enterFullScreen, SIGNAL(triggered(bool)),
		this, SLOT(toggleFullScreen(bool)));

	m_exitFullScreen = new QAction("Exit fullscreen (F or Esc)", this);
	connect(m_exitFullScreen, SIGNAL(triggered(bool)),
		this, SLOT(toggleFullScreen(bool)));
}

CaptureWin::~CaptureWin()
{
	makeCurrent();
	delete m_program;
}

void CaptureWin::resizeEvent(QResizeEvent *event)
{
	QSize origSize = correctAspect(QSize(m_origWidth, m_origHeight));
	QSize window = size();
	qreal scale;

	if ((qreal)window.width() / (qreal)origSize.width() >
	    (qreal)window.height() / (qreal)origSize.height()) {
		// Horizontal scale factor > vert. scale factor
		scale = (qreal)window.height() / (qreal)origSize.height();
	} else {
		scale = (qreal)window.width() / (qreal)origSize.width();
	}
	m_viewSize = QSize((qreal)m_origWidth * scale, (qreal)m_origHeight * scale);
	QOpenGLWidget::resizeEvent(event);
}

void CaptureWin::updateShader()
{
	setV4LFormat(m_v4l_fmt);
	if (m_mode == AppModeTest || m_mode == AppModeTPG || m_mode == AppModeFile) {
		delete m_timer;
		m_timer = NULL;
		startTimer();
	}
	m_updateShader = true;
}

void CaptureWin::showCurrentOverrides()
{
	static bool firstTime = true;
	const char *prefix = firstTime ? "" : "New ";

	if (m_mode == AppModeTest)
		return;

	if (m_canOverrideResolution || firstTime) {
		printf("%sPixel Format: '%s' %s\n", prefix,
		       fcc2s(m_origPixelFormat).c_str(),
		       pixfmt2s(m_origPixelFormat).c_str());
		printf("%sField: %s\n", prefix, field2s(m_origField).c_str());
	}
	printf("%sColorspace: %s\n", prefix, colorspace2s(m_origColorspace).c_str());
	printf("%sTransfer Function: %s\n", prefix, xfer_func2s(m_origXferFunc).c_str());
	if (m_is_hsv)
		printf("%sHSV Encoding: %s\n", prefix, ycbcr_enc2s(m_origHSVEnc).c_str());
	else if (!m_is_rgb)
		printf("%sY'CbCr Encoding: %s\n", prefix, ycbcr_enc2s(m_origYCbCrEnc).c_str());
	printf("%sQuantization Range: %s\n", prefix, quantization2s(m_origQuantization).c_str());
	firstTime = false;
}

void CaptureWin::restoreAll(bool checked)
{
	m_overridePixelFormat = m_origPixelFormat;
	m_overrideField = m_origField;
	m_overrideColorspace = m_origColorspace;
	m_overrideXferFunc = m_origXferFunc;
	m_overrideYCbCrEnc = m_origYCbCrEnc;
	m_overrideHSVEnc = m_origHSVEnc;
	m_overrideQuantization = m_origQuantization;
	m_overrideWidth = m_overrideHeight = 0;
	showCurrentOverrides();
	restoreSize();
}

void CaptureWin::fmtChanged(QAction *a)
{
	m_overridePixelFormat = a->data().toInt();
	if (m_overridePixelFormat == 0)
		m_overridePixelFormat = m_origPixelFormat;
	printf("New Pixel Format: '%s' %s\n",
	       fcc2s(m_overridePixelFormat).c_str(),
	       pixfmt2s(m_overridePixelFormat).c_str());
	updateShader();
}

void CaptureWin::fieldChanged(QAction *a)
{
	m_overrideField = a->data().toInt();
	if (m_overrideField == 0xffffffff)
		m_overrideField = m_origField;
	printf("New Field: %s\n", field2s(m_overrideField).c_str());
	updateShader();
}

void CaptureWin::colorspaceChanged(QAction *a)
{
	m_overrideColorspace = a->data().toInt();
	if (m_overrideColorspace == 0xffffffff)
		m_overrideColorspace = m_origColorspace;
	printf("New Colorspace: %s\n", colorspace2s(m_overrideColorspace).c_str());
	updateShader();
}

void CaptureWin::xferFuncChanged(QAction *a)
{
	m_overrideXferFunc = a->data().toInt();
	if (m_overrideXferFunc == 0xffffffff)
		m_overrideXferFunc = m_origXferFunc;
	printf("New Transfer Function: %s\n", xfer_func2s(m_overrideXferFunc).c_str());
	updateShader();
}

void CaptureWin::ycbcrEncChanged(QAction *a)
{
	m_overrideYCbCrEnc = a->data().toInt();
	if (m_overrideYCbCrEnc == 0xffffffff)
		m_overrideYCbCrEnc = m_origYCbCrEnc;
	printf("New Y'CbCr Encoding: %s\n", ycbcr_enc2s(m_overrideYCbCrEnc).c_str());
	updateShader();
}

void CaptureWin::hsvEncChanged(QAction *a)
{
	m_overrideHSVEnc = a->data().toInt();
	if (m_overrideHSVEnc == 0xffffffff)
		m_overrideHSVEnc = m_origHSVEnc;
	printf("New HSV Encoding: %s\n", ycbcr_enc2s(m_overrideHSVEnc).c_str());
	updateShader();
}

void CaptureWin::quantChanged(QAction *a)
{
	m_overrideQuantization = a->data().toInt();
	if (m_overrideQuantization == 0xffffffff)
		m_overrideQuantization = m_origQuantization;
	printf("New Quantization Range: %s\n", quantization2s(m_overrideQuantization).c_str());
	updateShader();
}

void CaptureWin::restoreSize(bool)
{
	QSize s = correctAspect(QSize(m_origWidth, m_origHeight));

	m_scrollArea->resize(s);
	resize(s);
	updateShader();
}

QSize CaptureWin::correctAspect(const QSize &s) const
{
	qreal aspect
		= (qreal)m_pixelaspect.denominator
		/ (qreal)m_pixelaspect.numerator;

	qreal w = s.width();
	qreal h = s.height();

	if (aspect < 1.0)
		h *= 1.0 / aspect;
	else
		w *= aspect;
	return QSize(qFloor(w), qFloor(h));
}

void CaptureWin::windowScalingChanged(QAction *a)
{
	m_scrollArea->setWidgetResizable(a->data().toInt() == CAPTURE_GL_WIN_RESIZE);
	resize(correctAspect(QSize(m_origWidth, m_origHeight)));
	updateShader();
}

void CaptureWin::resolutionOverrideChanged(bool checked)
{
	if (m_overrideWidth)
		m_origWidth = m_overrideWidth;
	if (m_overrideHeight)
		m_origHeight = m_overrideHeight;

	restoreSize();
}

void CaptureWin::toggleFullScreen(bool)
{
	if (m_scrollArea->isFullScreen())
		m_scrollArea->showNormal();
	else
		m_scrollArea->showFullScreen();
}

void CaptureWin::contextMenuEvent(QContextMenuEvent *event)
{
	QMenu menu(this);

	QAction *act = menu.addAction("Restore All");
	connect(act, SIGNAL(triggered(bool)), this, SLOT(restoreAll(bool)));

	act = menu.addAction("Reset window");
	connect(act, SIGNAL(triggered(bool)), this, SLOT(restoreSize(bool)));

	if (m_scrollArea->isFullScreen())
		menu.addAction(m_exitFullScreen);
	else
		menu.addAction(m_enterFullScreen);

	if (m_canOverrideResolution) {
		menu.addAction(m_resolutionOverride);
		menu.addMenu(m_fmtMenu);
	}
	menu.addMenu(m_fieldMenu);
	menu.addMenu(m_colorspaceMenu);
	menu.addMenu(m_xferFuncMenu);
	if (m_is_hsv)
		menu.addMenu(m_hsvEncMenu);
	else if (!m_is_rgb)
		menu.addMenu(m_ycbcrEncMenu);
	menu.addMenu(m_quantMenu);
	menu.addMenu(m_displayMenu);

	menu.exec(event->globalPos());
}

void CaptureWin::mouseDoubleClickEvent(QMouseEvent * e)
{
	if (e->button() != Qt::LeftButton)
		return;

	toggleFullScreen();
}

void CaptureWin::cycleMenu(__u32 &overrideVal, __u32 &origVal,
			     const __u32 values[], bool hasShift, bool hasCtrl)
{
	unsigned i;

	if (overrideVal == 0xffffffff || hasCtrl)
		overrideVal = origVal;
	if (hasCtrl)
		return;
	for (i = 0; values[i] && values[i] != overrideVal; i++);
	if (!values[i])
		overrideVal = values[0];
	else if (hasShift) {
		if (i)
			overrideVal = values[i - 1];
		else for (i = 0; values[i]; i++)
			overrideVal = values[i];
	} else {
		if (!values[i + 1])
			overrideVal = values[0];
		else
			overrideVal = values[i + 1];
	}
}

void CaptureWin::keyPressEvent(QKeyEvent *event)
{
	unsigned w = m_v4l_fmt.g_width();
	unsigned h = m_v4l_fmt.g_frame_height();
	bool hasShift = event->modifiers() & Qt::ShiftModifier;
	bool hasCtrl = event->modifiers() & Qt::ControlModifier;
	bool scalingEnabled = m_canOverrideResolution &&
			      m_resolutionOverride->isChecked();

	switch (event->key()) {
	case Qt::Key_Space:
		if (m_mode == AppModeTest)
			m_cnt = 1;
		else if (m_singleStep && m_frame > m_singleStepStart)
			m_singleStepNext = true;
		return;
	case Qt::Key_Escape:
		if (!m_scrollArea->isFullScreen())
			return;
	case Qt::Key_Left:
		if (scalingEnabled && w > 16)
			w -= 2;
		break;
	case Qt::Key_Right:
		if (scalingEnabled && w < 10240)
			w += 2;
		break;
	case Qt::Key_Up:
		if (scalingEnabled && h > 16)
			h -= 2;
		break;
	case Qt::Key_Down:
		if (scalingEnabled && h < 10240)
			h += 2;
		break;

	case Qt::Key_C:
		cycleMenu(m_overrideColorspace, m_origColorspace,
			  colorspaces, hasShift, hasCtrl);
		printf("New Colorspace: %s\n", colorspace2s(m_overrideColorspace).c_str());
		checkSubMenuItem(m_colorspaceMenu, m_overrideColorspace);
		updateShader();
		return;
	case Qt::Key_F:
		toggleFullScreen();
		return;
	case Qt::Key_H:
		if (!m_is_hsv)
			return;
		cycleMenu(m_overrideHSVEnc, m_origHSVEnc,
			  hsv_encs, hasShift, hasCtrl);
		printf("New HSV Encoding: %s\n", ycbcr_enc2s(m_overrideHSVEnc).c_str());
		checkSubMenuItem(m_hsvEncMenu, m_overrideHSVEnc);
		updateShader();
		return;
	case Qt::Key_I:
		cycleMenu(m_overrideField, m_origField,
			  fields, hasShift, hasCtrl);
		printf("New Field: %s\n", field2s(m_overrideField).c_str());
		checkSubMenuItem(m_fieldMenu, m_overrideField);
		updateShader();
		return;
	case Qt::Key_P:
		if (!m_canOverrideResolution)
			return;
		cycleMenu(m_overridePixelFormat, m_origPixelFormat,
			  formats, hasShift, hasCtrl);
		printf("New Pixel Format: '%s' %s\n", fcc2s(m_overridePixelFormat).c_str(),
		       pixfmt2s(m_overridePixelFormat).c_str());
		checkSubMenuItem(m_fmtMenu, m_overridePixelFormat);
		updateShader();
		return;
	case Qt::Key_Q:
		QApplication::quit();
		return;
	case Qt::Key_R:
		cycleMenu(m_overrideQuantization, m_origQuantization,
			  quantizations, hasShift, hasCtrl);
		printf("New Quantization Range: %s\n", quantization2s(m_overrideQuantization).c_str());
		checkSubMenuItem(m_quantMenu, m_overrideQuantization);
		updateShader();
		return;
	case Qt::Key_X:
		cycleMenu(m_overrideXferFunc, m_origXferFunc,
			  xfer_funcs, hasShift, hasCtrl);
		printf("New Transfer Function: %s\n", xfer_func2s(m_overrideXferFunc).c_str());
		checkSubMenuItem(m_xferFuncMenu, m_overrideXferFunc);
		updateShader();
		return;
	case Qt::Key_Y:
		if (m_is_rgb || m_is_hsv)
			return;
		cycleMenu(m_overrideYCbCrEnc, m_origYCbCrEnc,
			  ycbcr_encs, hasShift, hasCtrl);
		printf("New Y'CbCr Encoding: %s\n", ycbcr_enc2s(m_overrideYCbCrEnc).c_str());
		checkSubMenuItem(m_ycbcrEncMenu, m_overrideYCbCrEnc);
		updateShader();
		return;
	default:
		QOpenGLWidget::keyPressEvent(event);
		return;
	}

	if (scalingEnabled) {
		if (m_scrollArea->widgetResizable())
			m_scrollArea->resize(w, h);
		else
			resize(w, h);
	}
}

bool CaptureWin::supportedFmt(__u32 fmt)
{
	switch (fmt) {
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_Y16_BE:
		return m_haveSwapBytes;

	/*
	 * Note for RGB555(X) formats:
	 * openGL ES doesn't support GL_UNSIGNED_SHORT_1_5_5_5_REV
	 */
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
		return !context()->isOpenGLES();
	}

	return true;
}

void CaptureWin::checkError(const char *msg)
{
	int err;
	unsigned errNo = 0;

	while ((err = glGetError()) != GL_NO_ERROR)
		fprintf(stderr, "OpenGL Error (no: %u) code 0x%x: %s.\n", errNo++, err, msg);

	if (errNo)
		exit(errNo);
}

void CaptureWin::configureTexture(size_t idx)
{
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[idx]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void CaptureWin::setOverrideWidth(__u32 w)
{
	m_overrideWidth = w;

	if (!m_overrideWidth && m_canOverrideResolution)
		m_resolutionOverride->setChecked(true);
}

void CaptureWin::setOverrideHeight(__u32 h)
{
	m_overrideHeight = h;

	if (!m_overrideHeight && m_canOverrideResolution)
		m_resolutionOverride->setChecked(true);
}

void CaptureWin::setModeV4L2(cv4l_fd *fd)
{
	m_mode = AppModeV4L2;
	m_fd = fd;
	QSocketNotifier *readSock = new QSocketNotifier(fd->g_fd(),
		QSocketNotifier::Read, this);
	QSocketNotifier *excepSock = new QSocketNotifier(fd->g_fd(),
		QSocketNotifier::Exception, this);

	v4l2_event_subscription sub = { };

	sub.type = V4L2_EVENT_SOURCE_CHANGE;
	m_fd->subscribe_event(sub);
	connect(readSock, SIGNAL(activated(int)), this, SLOT(v4l2ReadEvent()));
	connect(excepSock, SIGNAL(activated(int)), this, SLOT(v4l2ExceptionEvent()));

	if (m_verbose && m_fd->g_direct())
		printf("using libv4l2\n");
}

void CaptureWin::setModeSocket(int socket, int port)
{
	m_mode = AppModeSocket;
	m_sock = socket;
	m_port = port;
	if (m_ctx)
		free(m_ctx);
	m_ctx = fwht_alloc(m_v4l_fmt.g_pixelformat(), m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			   m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			   m_v4l_fmt.g_field(), m_v4l_fmt.g_colorspace(), m_v4l_fmt.g_xfer_func(),
			   m_v4l_fmt.g_ycbcr_enc(), m_v4l_fmt.g_quantization());

	QSocketNotifier *readSock = new QSocketNotifier(m_sock,
		QSocketNotifier::Read, this);

	connect(readSock, SIGNAL(activated(int)), this, SLOT(sockReadEvent()));
}

void CaptureWin::setModeFile(const QString &filename)
{
	m_mode = AppModeFile;
	m_file.setFileName(filename);
	if (!m_file.open(QIODevice::ReadOnly)) {
		fprintf(stderr, "could not open %s\n", filename.toLatin1().data());
		exit(1);
	}
	m_canOverrideResolution = true;
}

void CaptureWin::setModeTPG()
{
	m_mode = AppModeTPG;
}

void CaptureWin::setModeTest(unsigned cnt)
{
	m_mode = AppModeTest;
	m_test = cnt;
}

void CaptureWin::setQueue(cv4l_queue *q)
{
	m_v4l_queue = q;
	if (m_origPixelFormat == 0)
		updateOrigValues();
}

bool CaptureWin::updateV4LFormat(const cv4l_fmt &fmt)
{
	m_is_rgb = true;
	m_is_hsv = false;
	m_uses_gl_red = false;
	m_accepts_srgb = true;
	m_is_bayer = false;

	switch (fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		m_uses_gl_red = true;
		/* fall through */
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
		m_is_rgb = false;
		m_accepts_srgb = false;
		break;
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
		m_is_rgb = false;
		m_is_hsv = true;
		m_accepts_srgb = false;
		break;
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		m_is_bayer = true;
		/* fall through */
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
		m_uses_gl_red = true;
		/* fall through */
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
		m_accepts_srgb = false;
		/* fall through */
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB332:
		break;
	default:
		return false;
	}
	return true;
}

bool CaptureWin::setV4LFormat(cv4l_fmt &fmt)
{
	m_is_sdtv = false;

	if (m_mode == AppModeFile && m_overridePixelFormat)
		fmt.s_pixelformat(m_overridePixelFormat);

	if (!updateV4LFormat(fmt))
		return false;

	if (m_is_bayer) {
		m_v4l_fmt.s_field(V4L2_FIELD_NONE);
		m_overrideField = 0;
	}
	if (m_mode == AppModeFile && m_overrideWidth)
		fmt.s_width(m_overrideWidth);
	if (m_mode == AppModeFile && m_overrideField != 0xffffffff)
		fmt.s_field(m_overrideField);
	if (m_mode == AppModeFile && m_overrideHeight)
		fmt.s_frame_height(m_overrideHeight);
	if (m_overrideColorspace != 0xffffffff)
		fmt.s_colorspace(m_overrideColorspace);
	if (m_is_hsv && m_overrideHSVEnc != 0xffffffff)
		fmt.s_ycbcr_enc(m_overrideHSVEnc);
	else if (!m_is_rgb && m_overrideYCbCrEnc != 0xffffffff)
		fmt.s_ycbcr_enc(m_overrideYCbCrEnc);
	if (m_overrideXferFunc != 0xffffffff)
		fmt.s_xfer_func(m_overrideXferFunc);
	if (m_overrideQuantization != 0xffffffff)
		fmt.s_quantization(m_overrideQuantization);

	m_v4l_fmt = fmt;

	v4l2_input in;

	m_std = 0;
	if (m_fd && !m_fd->g_input(in.index) && !m_fd->enum_input(in, true, in.index)) {
		if (in.capabilities & V4L2_IN_CAP_STD) {
			m_is_sdtv = true;
			if (m_fd->g_std(m_std))
				m_std = fmt.g_frame_height() <= 480 ?
					V4L2_STD_525_60 : V4L2_STD_625_50;
		} else if (in.capabilities & V4L2_IN_CAP_DV_TIMINGS) {
			v4l2_dv_timings timings;
			if (m_fd->g_dv_timings(timings) == 0) {
				m_is_sdtv = timings.bt.width <= 720 && timings.bt.height <= 576;
				if (m_is_sdtv)
					m_std = timings.bt.height <= 480 ?
						V4L2_STD_525_60 : V4L2_STD_625_50;
			}
		}
	}

	switch (fmt.g_colorspace()) {
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_SMPTE240M:
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
	case V4L2_COLORSPACE_SRGB:
	case V4L2_COLORSPACE_OPRGB:
	case V4L2_COLORSPACE_BT2020:
	case V4L2_COLORSPACE_DCI_P3:
		break;
	default:
		// If the colorspace was not specified, then guess
		// based on the pixel format.
		if (m_is_rgb)
			m_v4l_fmt.s_colorspace(V4L2_COLORSPACE_SRGB);
		else if (m_is_sdtv)
			m_v4l_fmt.s_colorspace(V4L2_COLORSPACE_SMPTE170M);
		else
			m_v4l_fmt.s_colorspace(V4L2_COLORSPACE_REC709);
		break;
	}
	if (fmt.g_xfer_func() == V4L2_XFER_FUNC_DEFAULT)
		m_v4l_fmt.s_xfer_func(V4L2_MAP_XFER_FUNC_DEFAULT(m_v4l_fmt.g_colorspace()));
	if (m_is_hsv)
		m_v4l_fmt.s_ycbcr_enc(fmt.g_hsv_enc());
	else if (fmt.g_ycbcr_enc() == V4L2_YCBCR_ENC_DEFAULT)
		m_v4l_fmt.s_ycbcr_enc(V4L2_MAP_YCBCR_ENC_DEFAULT(m_v4l_fmt.g_colorspace()));
	if (fmt.g_quantization() == V4L2_QUANTIZATION_DEFAULT)
		m_v4l_fmt.s_quantization(V4L2_MAP_QUANTIZATION_DEFAULT(m_is_rgb,
				m_v4l_fmt.g_colorspace(), m_v4l_fmt.g_ycbcr_enc()));

	if (m_accepts_srgb &&
	    (m_v4l_fmt.g_quantization() == V4L2_QUANTIZATION_LIM_RANGE ||
	    m_v4l_fmt.g_xfer_func() != V4L2_XFER_FUNC_SRGB)) {
		/* Can't let openGL convert from non-linear to linear */
		m_accepts_srgb = false;
	}

	if (m_verbose) {
		v4l2_fmtdesc fmt;

		strcpy((char *)fmt.description, fcc2s(m_v4l_fmt.g_pixelformat()).c_str());
		if (m_fd) {
			m_fd->enum_fmt(fmt, true);
			while (fmt.pixelformat != m_v4l_fmt.g_pixelformat()) {
				if (m_fd->enum_fmt(fmt))
					break;
			}
		}

		printf("\n");
		switch (m_mode) {
		case AppModeSocket:
			printf("Mode:              Capture from socket\n");
			break;
		case AppModeFile:
			printf("Mode:              Read from file\n");
			break;
		case AppModeTest:
			printf("Mode:              Test Formats\n");
			break;
		case AppModeTPG:
			printf("Mode:              Test Pattern Generator\n");
			break;
		case AppModeV4L2:
		default:
			printf("Mode:              Capture\n");
			break;
		}
		printf("Width x Height:    %ux%u\n", m_v4l_fmt.g_width(), m_v4l_fmt.g_height());
		printf("Field:             %s\n", field2s(m_v4l_fmt.g_field()).c_str());
		printf("Pixelformat:       %s ('%s')\n", fmt.description, fcc2s(m_v4l_fmt.g_pixelformat()).c_str());
		printf("Colorspace:        %s\n", colorspace2s(m_v4l_fmt.g_colorspace()).c_str());
		if (m_is_hsv)
			printf("HSV Encoding:      %s\n", ycbcr_enc2s(m_v4l_fmt.g_hsv_enc()).c_str());
		else if (!m_is_rgb)
			printf("Y'CbCr Encoding:   %s\n", ycbcr_enc2s(m_v4l_fmt.g_ycbcr_enc()).c_str());
		printf("Transfer Function: %s\n", xfer_func2s(m_v4l_fmt.g_xfer_func()).c_str());
		printf("Quantization:      %s\n", quantization2s(m_v4l_fmt.g_quantization()).c_str());
		for (unsigned i = 0; i < m_v4l_fmt.g_num_planes(); i++) {
			printf("Plane %d Image Size:     %u\n", i, m_v4l_fmt.g_sizeimage(i));
			printf("Plane %d Bytes per Line: %u\n", i, m_v4l_fmt.g_bytesperline(i));
		}
	}
	return true;
}

void CaptureWin::setPixelAspect(v4l2_fract &pixelaspect)
{
	m_pixelaspect = pixelaspect;
}

void CaptureWin::v4l2ReadEvent()
{
	cv4l_buffer buf(m_fd->g_type());

	if (m_singleStep && m_frame > m_singleStepStart && !m_singleStepNext)
		return;

	m_singleStepNext = false;
	if (m_fd->dqbuf(buf))
		return;

	for (unsigned i = 0; i < m_v4l_queue->g_num_planes(); i++) {
		m_nextData[i] = (__u8 *)m_v4l_queue->g_dataptr(buf.g_index(), i);
		m_nextSize[i] = buf.g_bytesused(i);
	}
	int next = m_nextIndex;
	m_nextIndex = buf.g_index();
	if (next != -1) {
		buf.s_index(next);
		m_fd->qbuf(buf);
	}
	m_frame++;
	update();
	if (m_cnt == 0)
		return;
	if (--m_cnt == 0)
		exit(0);
}

void CaptureWin::v4l2ExceptionEvent()
{
	v4l2_event ev;
	cv4l_fmt fmt;

	while (m_fd->dqevent(ev) == 0) {
		switch (ev.type) {
		case V4L2_EVENT_SOURCE_CHANGE:
			m_fd->g_fmt(fmt);
			if (!setV4LFormat(fmt)) {
				fprintf(stderr, "Unsupported format: '%s' %s\n",
					fcc2s(fmt.g_pixelformat()).c_str(),
					pixfmt2s(fmt.g_pixelformat()).c_str());
				exit(1);
			}
			updateOrigValues();
			m_updateShader = true;
			break;
		}
	}
}

void CaptureWin::listenForNewConnection()
{
	cv4l_fmt fmt;
	v4l2_fract pixelaspect = { 1, 1 };

	::close(m_sock);

	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
		m_curSize[p] = 0;
		delete [] m_curData[p];
		m_curData[p] = NULL;
	}

	int sock_fd;

	for (;;) {
		sock_fd = initSocket(m_port, fmt, pixelaspect);
		if (setV4LFormat(fmt))
			break;
		fprintf(stderr, "Unsupported format: '%s' %s\n",
			fcc2s(fmt.g_pixelformat()).c_str(),
			pixfmt2s(fmt.g_pixelformat()).c_str());
		::close(sock_fd);
	}
	if (m_ctx)
		free(m_ctx);
	m_ctx = fwht_alloc(fmt.g_pixelformat(), fmt.g_width(), fmt.g_height(),
			   fmt.g_width(), fmt.g_height(),
			   fmt.g_field(), fmt.g_colorspace(), fmt.g_xfer_func(),
			   fmt.g_ycbcr_enc(), fmt.g_quantization());
	setPixelAspect(pixelaspect);
	updateOrigValues();
	setModeSocket(sock_fd, m_port);
	restoreSize();
}

int CaptureWin::read_u32(__u32 &v)
{
	int n;

	v = 0;
	n = read(m_sock, &v, sizeof(v));
	if (n != sizeof(v)) {
		fprintf(stderr, "could not read __u32\n");
		return -1;
	}
	v = ntohl(v);
	return 0;
}

void CaptureWin::sockReadEvent()
{
	int n;

	if (m_singleStep && m_frame > m_singleStepStart && !m_singleStepNext)
		return;
	m_singleStepNext = false;

	if (m_origPixelFormat == 0)
		updateOrigValues();

	if (m_curSize[0] == 0) {
		for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
			m_curSize[p] = m_v4l_fmt.g_sizeimage(p);
			m_curData[p] = new __u8[m_curSize[p]];
		}
	}

	unsigned packet, sz;
	bool is_fwht;

	if (read_u32(packet))
		goto new_conn;

	if (packet == V4L_STREAM_PACKET_END) {
		fprintf(stderr, "END packet read\n");
		goto new_conn;
	}

	if (read_u32(sz))
		goto new_conn;

	if (packet != V4L_STREAM_PACKET_FRAME_VIDEO_RLE &&
	    packet != V4L_STREAM_PACKET_FRAME_VIDEO_FWHT) {
		char buf[1024];

		fprintf(stderr, "expected FRAME_VIDEO, got 0x%08x\n", packet);
		while (sz) {
			unsigned rdsize = sz > sizeof(buf) ? sizeof(buf) : sz;

			n = read(m_sock, buf, rdsize);
			if (n < 0) {
				fprintf(stderr, "error reading %d bytes\n", sz);
				goto new_conn;
			}
			sz -= n;
		}
		return;
	}

	is_fwht = m_ctx && packet == V4L_STREAM_PACKET_FRAME_VIDEO_FWHT;

	if (read_u32(sz))
		goto new_conn;

	if (sz != V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_HDR) {
		fprintf(stderr, "unsupported FRAME_VIDEO size\n");
		goto new_conn;
	}
	if (read_u32(sz) ||  // ignore field
	    read_u32(sz))    // ignore flags
		goto new_conn;

	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
		__u32 max_size = is_fwht ? m_ctx->comp_max_size : m_curSize[p];
		__u8 *dst = is_fwht ? m_ctx->state.compressed_frame : m_curData[p];
		__u32 data_size;
		__u32 offset;
		__u32 size;

		if (read_u32(sz))
			goto new_conn;
		if (sz != V4L_STREAM_PACKET_FRAME_VIDEO_SIZE_PLANE_HDR) {
			fprintf(stderr, "unsupported FRAME_VIDEO plane size\n");
			goto new_conn;
		}
		if (read_u32(size) || read_u32(data_size))
			goto new_conn;
		offset = is_fwht ? 0 : size - data_size;
		sz = data_size;

		if (data_size > max_size) {
			fprintf(stderr, "data size is too large (%u > %u)\n",
				data_size, max_size);
			goto new_conn;
		}
		while (sz) {
			n = read(m_sock, dst + offset, sz);
			if (n < 0) {
				fprintf(stderr, "error reading %d bytes\n", sz);
				goto new_conn;
			}
			if ((__u32)n == sz)
				break;
			offset += n;
			sz -= n;
		}
		if (is_fwht)
			fwht_decompress(m_ctx, dst, data_size, m_curData[p], m_curSize[p]);
		else
			rle_decompress(dst, size, data_size,
				       rle_calc_bpl(m_v4l_fmt.g_bytesperline(p), m_v4l_fmt.g_pixelformat()));
	}
	m_frame++;
	update();
	if (m_cnt == 0)
		return;
	if (--m_cnt == 0)
		exit(0);
	return;

new_conn:
	listenForNewConnection();
}

void CaptureWin::resizeGL(int w, int h)
{
	if (!m_canOverrideResolution || !m_resolutionOverride->isChecked())
		return;
	w &= ~1;
	h &= ~1;
	m_overrideWidth = w;
	m_overrideHeight = h;
	m_viewSize = QSize(m_overrideWidth, m_overrideHeight);
	updateShader();
	printf("New resolution: %ux%u\n", w, h);
}

void CaptureWin::updateOrigValues()
{
	m_origWidth = m_v4l_fmt.g_width();
	m_origHeight = m_v4l_fmt.g_frame_height();
	m_overrideWidth = m_overrideHeight = 0;
	m_origPixelFormat = m_v4l_fmt.g_pixelformat();
	m_origField = m_v4l_fmt.g_field();
	m_origColorspace = m_v4l_fmt.g_colorspace();
	m_origXferFunc = m_v4l_fmt.g_xfer_func();
	if (m_is_rgb)
		m_origXferFunc = V4L2_YCBCR_ENC_601;
	else if (m_is_hsv)
		m_origHSVEnc = m_v4l_fmt.g_hsv_enc();
	else
		m_origYCbCrEnc = m_v4l_fmt.g_ycbcr_enc();
	m_origQuantization = m_v4l_fmt.g_quantization();
	showCurrentOverrides();
	m_viewSize = QSize(m_origWidth, m_origHeight);
}

void CaptureWin::initImageFormat()
{
	updateV4LFormat(m_v4l_fmt);
	tpg_s_fourcc(&m_tpg, m_v4l_fmt.g_pixelformat());
	bool is_alt = m_v4l_fmt.g_field() == V4L2_FIELD_ALTERNATE;

	tpg_reset_source(&m_tpg, m_v4l_fmt.g_width(),
			 m_v4l_fmt.g_frame_height(), m_v4l_fmt.g_field());
	tpg_s_field(&m_tpg, m_v4l_fmt.g_first_field(m_std), is_alt);
	tpg_s_colorspace(&m_tpg, m_v4l_fmt.g_colorspace());
	tpg_s_xfer_func(&m_tpg, m_v4l_fmt.g_xfer_func());
	if (m_is_hsv)
		tpg_s_hsv_enc(&m_tpg, m_v4l_fmt.g_hsv_enc());
	else if (!m_is_rgb)
		tpg_s_ycbcr_enc(&m_tpg, m_v4l_fmt.g_ycbcr_enc());
	tpg_s_quantization(&m_tpg, m_v4l_fmt.g_quantization());
	m_v4l_fmt.s_num_planes(tpg_g_buffers(&m_tpg));
	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
		m_v4l_fmt.s_bytesperline(tpg_g_bytesperline(&m_tpg, p), p);
		m_v4l_fmt.s_sizeimage(tpg_calc_plane_size(&m_tpg, p), p);
	}
	if (tpg_g_buffers(&m_tpg) == 1) {
		unsigned size = 0;

		for (unsigned p = 0; p < tpg_g_planes(&m_tpg); p++)
			size += tpg_calc_plane_size(&m_tpg, p);
		m_v4l_fmt.s_sizeimage(size, 0);
	}
	if (m_verbose)
		tpg_log_status(&m_tpg);
}

void CaptureWin::startTimer()
{
	if (m_origPixelFormat == 0)
		updateOrigValues();
	initImageFormat();

	m_timer = new QTimer(this);
	connect(m_timer, SIGNAL(timeout()), this, SLOT(tpgUpdateFrame()));

	m_imageSize = 0;
	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++)
		m_imageSize += m_v4l_fmt.g_sizeimage(p);

	if (m_file.isOpen())
		m_file.seek(0);

	if (m_file.isOpen() && m_imageSize > m_file.size()) {
		fprintf(stderr, "the file size is too small (expect at least %u, got %llu)\n",
			m_imageSize, m_file.size());
	}

	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
		m_curSize[p] = m_v4l_fmt.g_sizeimage(p);
		delete [] m_curData[p];
		if (m_canOverrideResolution)
			m_curData[p] = new __u8[4096 * 2160 * (p ? 2 : 4)];
		else
			m_curData[p] = new __u8[m_curSize[p]];
		if (m_file.isOpen())
			m_file.read((char *)m_curData[p], m_curSize[p]);
		else
			tpg_fillbuffer(&m_tpg, 0, p, m_curData[p]);
	}
	bool is_alt = m_v4l_fmt.g_field() == V4L2_FIELD_ALTERNATE;
	tpg_update_mv_count(&m_tpg, is_alt);
	m_timer->setTimerType(Qt::PreciseTimer);
	m_timer->setSingleShot(false);
	m_timer->setInterval(1000.0 / (m_fps * (is_alt ? 2 : 1)));
	m_timer->start(1000.0 / (m_fps * (is_alt ? 2 : 1)));
	if (is_alt && m_cnt)
		m_cnt *= 2;
	if (m_mode == AppModeTest) {
		fprintf(stderr, "test %s ('%s'), %s, %s, %s, ",
			pixfmt2s(formats[m_testState.fmt_idx]).c_str(),
			fcc2s(formats[m_testState.fmt_idx]).c_str(),
			field2s(fields[m_testState.field_idx]).c_str(),
			colorspace2s(colorspaces[m_testState.colorspace_idx]).c_str(),
			xfer_func2s(xfer_funcs[m_testState.xfer_func_idx]).c_str());
		if (m_is_rgb)
			fprintf(stderr, "%s\n",
				quantization2s(quantizations[m_testState.quant_idx]).c_str());
		else if (m_is_hsv)
			fprintf(stderr, "%s, %s\n",
				ycbcr_enc2s(ycbcr_encs[m_testState.hsv_enc_idx]).c_str(),
				quantization2s(quantizations[m_testState.quant_idx]).c_str());
		else
			fprintf(stderr, "%s, %s\n",
				ycbcr_enc2s(ycbcr_encs[m_testState.ycbcr_enc_idx]).c_str(),
				quantization2s(quantizations[m_testState.quant_idx]).c_str());
	}
}

void CaptureWin::tpgUpdateFrame()
{
	bool is_alt = m_v4l_fmt.g_field() == V4L2_FIELD_ALTERNATE;

	if (m_mode != AppModeTest && m_singleStep && m_frame > m_singleStepStart &&
	    !m_singleStepNext)
		return;
	m_singleStepNext = false;

	if (m_mode == AppModeFile && m_file.pos() + m_imageSize > m_file.size())
		m_file.seek(0);

	if (m_mode != AppModeFile && is_alt) {
		if (m_tpg.field == V4L2_FIELD_TOP)
			tpg_s_field(&m_tpg, V4L2_FIELD_BOTTOM, true);
		else
			tpg_s_field(&m_tpg, V4L2_FIELD_TOP, true);
	}

	for (unsigned p = 0; p < m_v4l_fmt.g_num_planes(); p++) {
		if (m_mode == AppModeFile)
			m_file.read((char *)m_curData[p], m_curSize[p]);
		else
			tpg_fillbuffer(&m_tpg, 0, p, m_curData[p]);
	}
	m_frame++;
	update();
	if (m_cnt != 1)
		tpg_update_mv_count(&m_tpg, is_alt);

	if (m_cnt == 0)
		return;
	if (--m_cnt)
		return;

	delete m_timer;
	m_timer = NULL;
	if (!m_test)
		exit(0);

	m_cnt = m_test;

	bool mask_quant = m_testState.mask & QUANT_MASK;
	bool mask_ycbcr_enc = m_is_rgb || m_is_hsv || (m_testState.mask & YCBCR_HSV_ENC_MASK);
	bool mask_hsv_enc = !m_is_hsv || (m_testState.mask & YCBCR_HSV_ENC_MASK);
	bool mask_xfer_func = m_testState.mask & XFER_FUNC_MASK;
	bool mask_colorspace = m_testState.mask & COLORSPACE_MASK;
	bool mask_field = m_is_bayer || (m_testState.mask & FIELD_MASK);
	bool mask_fmt = m_testState.mask & FMT_MASK;

	if (mask_quant ||
	    quantizations[++m_testState.quant_idx] == 0) {
		if (!mask_quant)
			m_testState.quant_idx = 0;
		if (mask_ycbcr_enc ||
		    ycbcr_encs[++m_testState.ycbcr_enc_idx] == 0) {
			if (!mask_ycbcr_enc)
				m_testState.ycbcr_enc_idx = 0;
			if (mask_hsv_enc ||
			    hsv_encs[++m_testState.hsv_enc_idx] == 0) {
				if (!mask_hsv_enc)
					m_testState.hsv_enc_idx = 0;
				if (mask_xfer_func ||
				    xfer_funcs[++m_testState.xfer_func_idx] == 0) {
					if (!mask_xfer_func)
						m_testState.xfer_func_idx = 0;
					if (mask_colorspace ||
					    colorspaces[++m_testState.colorspace_idx] == 0) {
						if (!mask_colorspace)
							m_testState.colorspace_idx = 0;
						if (mask_field ||
						    fields[++m_testState.field_idx] == 0) {
							if (!mask_field)
								m_testState.field_idx = 0;
							if (mask_fmt ||
							    formats[++m_testState.fmt_idx] == 0)
								exit(0);
						}
					}
				}
			}
		}
	}

	while (!supportedFmt(formats[m_testState.fmt_idx]))
		if (formats[++m_testState.fmt_idx] == 0)
			exit(0);

	m_v4l_fmt.s_pixelformat(formats[m_testState.fmt_idx]);
	updateV4LFormat(m_v4l_fmt);
	m_v4l_fmt.s_field(fields[m_testState.field_idx]);
	m_v4l_fmt.s_colorspace(colorspaces[m_testState.colorspace_idx]);
	m_v4l_fmt.s_xfer_func(xfer_funcs[m_testState.xfer_func_idx]);
	if (m_is_hsv)
		m_v4l_fmt.s_ycbcr_enc(hsv_encs[m_testState.hsv_enc_idx]);
	else
		m_v4l_fmt.s_ycbcr_enc(ycbcr_encs[m_testState.ycbcr_enc_idx]);
	m_v4l_fmt.s_quantization(quantizations[m_testState.quant_idx]);

	if (m_accepts_srgb &&
	    (m_v4l_fmt.g_quantization() == V4L2_QUANTIZATION_LIM_RANGE ||
	    m_v4l_fmt.g_xfer_func() != V4L2_XFER_FUNC_SRGB)) {
		/* Can't let openGL convert from non-linear to linear */
		m_accepts_srgb = false;
	}
	startTimer();
	m_updateShader = true;
}
