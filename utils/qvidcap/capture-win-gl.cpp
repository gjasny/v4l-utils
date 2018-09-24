// SPDX-License-Identifier: GPL-2.0-only
/*
 * The YUY2 shader code is based on face-responder. The code is under public domain:
 * https://bitbucket.org/nateharward/face-responder/src/0c3b4b957039d9f4bf1da09b9471371942de2601/yuv42201_laplace.frag?at=master
 *
 * All other OpenGL code:
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include "capture-win-gl.h"

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

CaptureGLWin::CaptureGLWin(QScrollArea *sa, QWidget *parent) :
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

	m_resolutionOverride = new QAction("Override resolution");
	m_resolutionOverride->setCheckable(true);
	connect(m_resolutionOverride, SIGNAL(triggered(bool)),
		this, SLOT(resolutionOverrideChanged(bool)));

	m_enterFullScreen = new QAction("Enter fullscreen (F)");
	connect(m_enterFullScreen, SIGNAL(triggered(bool)),
		this, SLOT(toggleFullScreen(bool)));

	m_exitFullScreen = new QAction("Exit fullscreen (F or Esc)");
	connect(m_exitFullScreen, SIGNAL(triggered(bool)),
		this, SLOT(toggleFullScreen(bool)));
}

CaptureGLWin::~CaptureGLWin()
{
	makeCurrent();
	delete m_program;
}

void CaptureGLWin::resizeEvent(QResizeEvent *event)
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

void CaptureGLWin::updateShader()
{
	setV4LFormat(m_v4l_fmt);
	if (m_mode == AppModeTest || m_mode == AppModeTPG || m_mode == AppModeFile) {
		delete m_timer;
		m_timer = NULL;
		startTimer();
	}
	m_updateShader = true;
}

void CaptureGLWin::showCurrentOverrides()
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

void CaptureGLWin::restoreAll(bool checked)
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

void CaptureGLWin::fmtChanged(QAction *a)
{
	m_overridePixelFormat = a->data().toInt();
	if (m_overridePixelFormat == 0)
		m_overridePixelFormat = m_origPixelFormat;
	printf("New Pixel Format: '%s' %s\n",
	       fcc2s(m_overridePixelFormat).c_str(),
	       pixfmt2s(m_overridePixelFormat).c_str());
	updateShader();
}

void CaptureGLWin::fieldChanged(QAction *a)
{
	m_overrideField = a->data().toInt();
	if (m_overrideField == 0xffffffff)
		m_overrideField = m_origField;
	printf("New Field: %s\n", field2s(m_overrideField).c_str());
	updateShader();
}

void CaptureGLWin::colorspaceChanged(QAction *a)
{
	m_overrideColorspace = a->data().toInt();
	if (m_overrideColorspace == 0xffffffff)
		m_overrideColorspace = m_origColorspace;
	printf("New Colorspace: %s\n", colorspace2s(m_overrideColorspace).c_str());
	updateShader();
}

void CaptureGLWin::xferFuncChanged(QAction *a)
{
	m_overrideXferFunc = a->data().toInt();
	if (m_overrideXferFunc == 0xffffffff)
		m_overrideXferFunc = m_origXferFunc;
	printf("New Transfer Function: %s\n", xfer_func2s(m_overrideXferFunc).c_str());
	updateShader();
}

void CaptureGLWin::ycbcrEncChanged(QAction *a)
{
	m_overrideYCbCrEnc = a->data().toInt();
	if (m_overrideYCbCrEnc == 0xffffffff)
		m_overrideYCbCrEnc = m_origYCbCrEnc;
	printf("New Y'CbCr Encoding: %s\n", ycbcr_enc2s(m_overrideYCbCrEnc).c_str());
	updateShader();
}

void CaptureGLWin::hsvEncChanged(QAction *a)
{
	m_overrideHSVEnc = a->data().toInt();
	if (m_overrideHSVEnc == 0xffffffff)
		m_overrideHSVEnc = m_origHSVEnc;
	printf("New HSV Encoding: %s\n", ycbcr_enc2s(m_overrideHSVEnc).c_str());
	updateShader();
}

void CaptureGLWin::quantChanged(QAction *a)
{
	m_overrideQuantization = a->data().toInt();
	if (m_overrideQuantization == 0xffffffff)
		m_overrideQuantization = m_origQuantization;
	printf("New Quantization Range: %s\n", quantization2s(m_overrideQuantization).c_str());
	updateShader();
}

void CaptureGLWin::restoreSize(bool)
{
	QSize s = correctAspect(QSize(m_origWidth, m_origHeight));

	m_scrollArea->resize(s);
	resize(s);
	updateShader();
}

QSize CaptureGLWin::correctAspect(const QSize &s) const
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

void CaptureGLWin::windowScalingChanged(QAction *a)
{
	m_scrollArea->setWidgetResizable(a->data().toInt() == CAPTURE_GL_WIN_RESIZE);
	resize(correctAspect(QSize(m_origWidth, m_origHeight)));
	updateShader();
}

void CaptureGLWin::resolutionOverrideChanged(bool checked)
{
	if (m_overrideWidth)
		m_origWidth = m_overrideWidth;
	if (m_overrideHeight)
		m_origHeight = m_overrideHeight;

	restoreSize();
}

void CaptureGLWin::toggleFullScreen(bool)
{
	if (m_scrollArea->isFullScreen())
		m_scrollArea->showNormal();
	else
		m_scrollArea->showFullScreen();
}

void CaptureGLWin::contextMenuEvent(QContextMenuEvent *event)
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

void CaptureGLWin::mouseDoubleClickEvent(QMouseEvent * e)
{
	if (e->button() != Qt::LeftButton)
		return;

	toggleFullScreen();
}

void CaptureGLWin::cycleMenu(__u32 &overrideVal, __u32 &origVal,
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

void CaptureGLWin::keyPressEvent(QKeyEvent *event)
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

bool CaptureGLWin::supportedFmt(__u32 fmt)
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

void CaptureGLWin::checkError(const char *msg)
{
	int err;
	unsigned errNo = 0;

	while ((err = glGetError()) != GL_NO_ERROR)
		fprintf(stderr, "OpenGL Error (no: %u) code 0x%x: %s.\n", errNo++, err, msg);

	if (errNo)
		exit(errNo);
}

void CaptureGLWin::configureTexture(size_t idx)
{
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[idx]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

void CaptureGLWin::setOverrideWidth(__u32 w)
{
	m_overrideWidth = w;

	if (!m_overrideWidth && m_canOverrideResolution)
		m_resolutionOverride->setChecked(true);
}

void CaptureGLWin::setOverrideHeight(__u32 h)
{
	m_overrideHeight = h;

	if (!m_overrideHeight && m_canOverrideResolution)
		m_resolutionOverride->setChecked(true);
}

void CaptureGLWin::setModeV4L2(cv4l_fd *fd)
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

void CaptureGLWin::setModeSocket(int socket, int port)
{
	m_mode = AppModeSocket;
	m_sock = socket;
	m_port = port;
	if (m_ctx)
		free(m_ctx);
	m_ctx = fwht_alloc(m_v4l_fmt.g_pixelformat(), m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			   m_v4l_fmt.g_field(), m_v4l_fmt.g_colorspace(), m_v4l_fmt.g_xfer_func(),
			   m_v4l_fmt.g_ycbcr_enc(), m_v4l_fmt.g_quantization());

	QSocketNotifier *readSock = new QSocketNotifier(m_sock,
		QSocketNotifier::Read, this);

	connect(readSock, SIGNAL(activated(int)), this, SLOT(sockReadEvent()));
}

void CaptureGLWin::setModeFile(const QString &filename)
{
	m_mode = AppModeFile;
	m_file.setFileName(filename);
	if (!m_file.open(QIODevice::ReadOnly)) {
		fprintf(stderr, "could not open %s\n", filename.toLatin1().data());
		exit(1);
	}
	m_canOverrideResolution = true;
}

void CaptureGLWin::setModeTPG()
{
	m_mode = AppModeTPG;
}

void CaptureGLWin::setModeTest(unsigned cnt)
{
	m_mode = AppModeTest;
	m_test = cnt;
}

void CaptureGLWin::setQueue(cv4l_queue *q)
{
	m_v4l_queue = q;
	if (m_origPixelFormat == 0)
		updateOrigValues();
}

bool CaptureGLWin::updateV4LFormat(const cv4l_fmt &fmt)
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

bool CaptureGLWin::setV4LFormat(cv4l_fmt &fmt)
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

void CaptureGLWin::setPixelAspect(v4l2_fract &pixelaspect)
{
	m_pixelaspect = pixelaspect;
}

void CaptureGLWin::v4l2ReadEvent()
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

void CaptureGLWin::v4l2ExceptionEvent()
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

void CaptureGLWin::listenForNewConnection()
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
			   fmt.g_field(), fmt.g_colorspace(), fmt.g_xfer_func(),
			   fmt.g_ycbcr_enc(), fmt.g_quantization());
	setPixelAspect(pixelaspect);
	updateOrigValues();
	setModeSocket(sock_fd, m_port);
	restoreSize();
}

int CaptureGLWin::read_u32(__u32 &v)
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

void CaptureGLWin::sockReadEvent()
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

void CaptureGLWin::resizeGL(int w, int h)
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

void CaptureGLWin::updateOrigValues()
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

void CaptureGLWin::initImageFormat()
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

void CaptureGLWin::startTimer()
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

void CaptureGLWin::tpgUpdateFrame()
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

void CaptureGLWin::initializeGL()
{
	initializeOpenGLFunctions();
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	// Check if the the GL_FRAMEBUFFER_SRGB feature is available.
	// If it is, then the GPU can perform the SRGB transfer function
	// for us.
	GLint res = 0;
	if (context()->hasExtension("GL_EXT_framebuffer_sRGB"))
		glGetIntegerv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &res);
	checkError("InitializeGL Part 1");

	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	m_haveSwapBytes = glGetError() == GL_NO_ERROR;

	if (m_verbose) {
		printf("OpenGL %sdoes%s support GL_UNPACK_SWAP_BYTES\n",
		       context()->isOpenGLES() ? "ES " : "",
		       m_haveSwapBytes ? "" : " not");
	}
	if (m_uses_gl_red && glGetString(GL_VERSION)[0] < '3') {
		fprintf(stderr, "The openGL implementation does not support GL_RED/GL_RG\n");
		exit(1);
	}

	QColor bg = QWidget::palette().color(QWidget::backgroundRole());
	glClearColor(bg.red() / 255.0f,
		     bg.green() / 255.0f,
		     bg.blue() / 255.0f,
		     0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	checkError("InitializeGL Part 2");
	m_program = new QOpenGLShaderProgram(this);
	m_updateShader = true;
}


void CaptureGLWin::paintGL()
{
	if (m_v4l_fmt.g_width() < 16 || m_v4l_fmt.g_frame_height() < 16)
		return;

	if (m_mode == AppModeV4L2) {
		if (m_v4l_queue == NULL || m_nextIndex == -1)
			return;

		cv4l_buffer buf(*m_v4l_queue, m_curIndex);

		m_fd->qbuf(buf);
		for (unsigned i = 0; i < m_v4l_queue->g_num_planes(); i++) {
			m_curData[i] = m_nextData[i];
			m_curSize[i] = m_nextSize[i];
			m_curIndex = m_nextIndex;
			m_nextIndex = -1;
			m_nextData[i] = 0;
			m_nextSize[i] = 0;
		}
	}


	if (m_curData[0] == NULL) {
		// No data, just clear display
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		checkError("paintGL - no data");
		return;
	}

	if (m_updateShader) {
		m_updateShader = false;
		if (!supportedFmt(m_v4l_fmt.g_pixelformat())) {
			fprintf(stderr, "OpenGL ES unsupported format 0x%08x ('%s').\n",
				m_v4l_fmt.g_pixelformat(), fcc2s(m_v4l_fmt.g_pixelformat()).c_str());

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			checkError("paintGL - no data");
			return;
		}

		changeShader();
	}

	if (!supportedFmt(m_v4l_fmt.g_pixelformat()))
		return;

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		render_YUY2(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		render_NV16(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		render_NV12(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		render_NV24(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		render_YUV(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
		render_YUV_packed(m_v4l_fmt.g_pixelformat());
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
		render_Bayer(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		render_RGB(m_v4l_fmt.g_pixelformat());
		break;
	}

	static unsigned long long tot_t;
	static unsigned cnt;
	GLuint query;
	QSize s = correctAspect(m_viewSize);
	bool scale = m_scrollArea->widgetResizable();

	glViewport(scale ? (size().width() - s.width()) / 2 : 0,
		   scale ? (size().height() - s.height()) / 2 : 0,
		   s.width(), s.height());

	if (m_reportTimings) {
		glGenQueries(1, &query);
		glBeginQuery(GL_TIME_ELAPSED, query);
	}

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Define Quad surface to draw to, vertices sequenced for GL_TRIANGLE_FAN
	// Adjusting these will make the screen to draw to smaller or larger
	const GLfloat quadVertex[] = {
		-1.0f, -1.0f, // Left Bottom
		+1.0f, -1.0f, // Right Bottom
		+1.0f, +1.0f, // Rigth Top
		-1.0f, +1.0f  // Left Top
	};

	// Normalized texture coords to be aligned to draw quad corners, same sequence but 0,0 is Left Top
	const GLuint texCoords[] = {
		0, 1,
		1, 1,
		1, 0,
		0, 0
	};

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertex), quadVertex, GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);

	// Attach Quad positions to vertex shader "position" attribute (location = 0)
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Attach texture corner positions to vertex shader "texCoord" attribute (location = 1)
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE, 0, (void*)0);

	// Draw quad with texture
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Disable attrib arrays
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vertexbuffer);

	checkError("paintGL");

	if (m_reportTimings) {
		glEndQuery(GL_TIME_ELAPSED);
		GLuint t;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &t);
		glDeleteQueries(1, &query);

		cnt++;
		tot_t += t;
		unsigned ave = tot_t / cnt;
		printf("Average render time: %09u ns, frame %d render time: %09u ns\n",
		       ave, cnt, t);
	}
}

static const char *prog =
#include "v4l2-convert.h"
;

struct define {
	const char *name;
	unsigned id;
};

#define DEF(c) { .name = #c, .id = c }

static const struct define defines[] = {
	DEF(V4L2_PIX_FMT_YUYV),
	DEF(V4L2_PIX_FMT_YVYU),
	DEF(V4L2_PIX_FMT_UYVY),
	DEF(V4L2_PIX_FMT_VYUY),
	DEF(V4L2_PIX_FMT_YUV422P),
	DEF(V4L2_PIX_FMT_YVU420),
	DEF(V4L2_PIX_FMT_YUV420),
	DEF(V4L2_PIX_FMT_NV12),
	DEF(V4L2_PIX_FMT_NV21),
	DEF(V4L2_PIX_FMT_NV16),
	DEF(V4L2_PIX_FMT_NV61),
	DEF(V4L2_PIX_FMT_NV24),
	DEF(V4L2_PIX_FMT_NV42),
	DEF(V4L2_PIX_FMT_NV16M),
	DEF(V4L2_PIX_FMT_NV61M),
	DEF(V4L2_PIX_FMT_YVU420M),
	DEF(V4L2_PIX_FMT_YUV420M),
	DEF(V4L2_PIX_FMT_YVU422M),
	DEF(V4L2_PIX_FMT_YUV422M),
	DEF(V4L2_PIX_FMT_YVU444M),
	DEF(V4L2_PIX_FMT_YUV444M),
	DEF(V4L2_PIX_FMT_NV12M),
	DEF(V4L2_PIX_FMT_NV21M),
	DEF(V4L2_PIX_FMT_YUV444),
	DEF(V4L2_PIX_FMT_YUV555),
	DEF(V4L2_PIX_FMT_YUV565),
	DEF(V4L2_PIX_FMT_YUV32),
	DEF(V4L2_PIX_FMT_RGB32),
	DEF(V4L2_PIX_FMT_XRGB32),
	DEF(V4L2_PIX_FMT_ARGB32),
	DEF(V4L2_PIX_FMT_BGR32),
	DEF(V4L2_PIX_FMT_XBGR32),
	DEF(V4L2_PIX_FMT_ABGR32),
	DEF(V4L2_PIX_FMT_RGB24),
	DEF(V4L2_PIX_FMT_BGR24),
	DEF(V4L2_PIX_FMT_RGB565),
	DEF(V4L2_PIX_FMT_RGB565X),
	DEF(V4L2_PIX_FMT_RGB444),
	DEF(V4L2_PIX_FMT_XRGB444),
	DEF(V4L2_PIX_FMT_ARGB444),
	DEF(V4L2_PIX_FMT_RGB555),
	DEF(V4L2_PIX_FMT_XRGB555),
	DEF(V4L2_PIX_FMT_ARGB555),
	DEF(V4L2_PIX_FMT_RGB555X),
	DEF(V4L2_PIX_FMT_XRGB555X),
	DEF(V4L2_PIX_FMT_ARGB555X),
	DEF(V4L2_PIX_FMT_RGB332),
	DEF(V4L2_PIX_FMT_BGR666),
	DEF(V4L2_PIX_FMT_SBGGR8),
	DEF(V4L2_PIX_FMT_SGBRG8),
	DEF(V4L2_PIX_FMT_SGRBG8),
	DEF(V4L2_PIX_FMT_SRGGB8),
	DEF(V4L2_PIX_FMT_SBGGR10),
	DEF(V4L2_PIX_FMT_SGBRG10),
	DEF(V4L2_PIX_FMT_SGRBG10),
	DEF(V4L2_PIX_FMT_SRGGB10),
	DEF(V4L2_PIX_FMT_SBGGR12),
	DEF(V4L2_PIX_FMT_SGBRG12),
	DEF(V4L2_PIX_FMT_SGRBG12),
	DEF(V4L2_PIX_FMT_SRGGB12),
	DEF(V4L2_PIX_FMT_HSV24),
	DEF(V4L2_PIX_FMT_HSV32),
	DEF(V4L2_PIX_FMT_GREY),
	DEF(V4L2_PIX_FMT_Y10),
	DEF(V4L2_PIX_FMT_Y12),
	DEF(V4L2_PIX_FMT_Y16),
	DEF(V4L2_PIX_FMT_Y16_BE),
	DEF(V4L2_PIX_FMT_Z16),

	DEF(V4L2_FIELD_ANY),
	DEF(V4L2_FIELD_NONE),
	DEF(V4L2_FIELD_TOP),
	DEF(V4L2_FIELD_BOTTOM),
	DEF(V4L2_FIELD_INTERLACED),
	DEF(V4L2_FIELD_SEQ_TB),
	DEF(V4L2_FIELD_SEQ_BT),
	DEF(V4L2_FIELD_ALTERNATE),
	DEF(V4L2_FIELD_INTERLACED_TB),
	DEF(V4L2_FIELD_INTERLACED_BT),

	DEF(V4L2_COLORSPACE_DEFAULT),
	DEF(V4L2_COLORSPACE_SMPTE170M),
	DEF(V4L2_COLORSPACE_SMPTE240M),
	DEF(V4L2_COLORSPACE_REC709),
	DEF(V4L2_COLORSPACE_470_SYSTEM_M),
	DEF(V4L2_COLORSPACE_470_SYSTEM_BG),
	DEF(V4L2_COLORSPACE_SRGB),
	DEF(V4L2_COLORSPACE_OPRGB),
	DEF(V4L2_COLORSPACE_BT2020),
	DEF(V4L2_COLORSPACE_RAW),
	DEF(V4L2_COLORSPACE_DCI_P3),

	DEF(V4L2_XFER_FUNC_DEFAULT),
	DEF(V4L2_XFER_FUNC_709),
	DEF(V4L2_XFER_FUNC_SRGB),
	DEF(V4L2_XFER_FUNC_OPRGB),
	DEF(V4L2_XFER_FUNC_SMPTE240M),
	DEF(V4L2_XFER_FUNC_NONE),
	DEF(V4L2_XFER_FUNC_DCI_P3),
	DEF(V4L2_XFER_FUNC_SMPTE2084),

	DEF(V4L2_YCBCR_ENC_DEFAULT),
	DEF(V4L2_YCBCR_ENC_601),
	DEF(V4L2_YCBCR_ENC_709),
	DEF(V4L2_YCBCR_ENC_XV601),
	DEF(V4L2_YCBCR_ENC_XV709),
	DEF(V4L2_YCBCR_ENC_BT2020),
	DEF(V4L2_YCBCR_ENC_BT2020_CONST_LUM),
	DEF(V4L2_YCBCR_ENC_SMPTE240M),
	DEF(V4L2_HSV_ENC_180),
	DEF(V4L2_HSV_ENC_256),

	DEF(V4L2_QUANTIZATION_DEFAULT),
	DEF(V4L2_QUANTIZATION_FULL_RANGE),
	DEF(V4L2_QUANTIZATION_LIM_RANGE),
	{ NULL, 0 }
};

void CaptureGLWin::changeShader()
{
	if (m_screenTextureCount)
		glDeleteTextures(m_screenTextureCount, m_screenTexture);
	m_program->removeAllShaders();
	checkError("Render settings.\n");

	QString code;

	if (context()->isOpenGLES())
		code = "#version 300 es\n"
			"precision mediump float;\n";
	else
		code = "#version 330\n";
	code += QString(
		"const float tex_w = %1.0;\n"
		"const float tex_h = %2.0;\n"
		"#define FIELD %3\n"
		"#define IS_RGB %4\n"
		"#define PIXFMT %5u\n"
		"#define COLSP %6\n"
		"#define XFERFUNC %7\n"
		"#define YCBCRENC %8\n"
		"#define QUANT %9\n\n"
		"#define IS_HSV %10\n"
		"#define HSVENC %11\n")
		.arg(m_v4l_fmt.g_width())
		.arg(m_v4l_fmt.g_height())
		.arg(m_v4l_fmt.g_field())
		.arg(m_is_rgb)
		.arg(m_v4l_fmt.g_pixelformat())
		.arg(m_v4l_fmt.g_colorspace())
		.arg(m_accepts_srgb ? V4L2_XFER_FUNC_NONE : m_v4l_fmt.g_xfer_func())
		.arg(m_v4l_fmt.g_ycbcr_enc())
		.arg(m_v4l_fmt.g_quantization())
		.arg(m_is_hsv)
		.arg(m_v4l_fmt.g_hsv_enc());

	for (unsigned i = 0; defines[i].name; i++)
		code += QString("#define ") + defines[i].name + " " + QString("%1").arg(defines[i].id) + "u\n";
	code += "#line 1\n";

	code += prog;

	bool src_ok = m_program->addShaderFromSourceCode(
		QOpenGLShader::Fragment, code);

	if (!src_ok) {
		fprintf(stderr, "OpenGL Error: fragment shader compilation failed.\n");
		exit(1);
	}

	// Mandatory vertex shader replaces fixed pipeline in GLES 2.0. In this case just a feedthrough shader.
	QString vertexShaderSrc;

	if (context()->isOpenGLES())
		vertexShaderSrc = "#version 300 es\n"
			"precision mediump float;\n";
	else
		vertexShaderSrc = "#version 330\n";

	vertexShaderSrc +=
		"layout(location = 0) in vec2 position;\n"
		"layout(location = 1) in vec2 texCoord;\n"
		"out vec2 vs_TexCoord;\n"
		"void main() {\n"
		"       gl_Position = vec4(position, 0.0, 1.0);\n"
		"       vs_TexCoord = texCoord;\n"
		"}\n";

	src_ok = m_program->addShaderFromSourceCode(
		QOpenGLShader::Vertex, vertexShaderSrc);

	if (!src_ok) {
		fprintf(stderr, "OpenGL Error: vertex shader compilation failed.\n");
		exit(1);
	}

	if (!m_program->bind()) {
		fprintf(stderr, "OpenGL Error: shader bind failed.\n");
		exit(1);
	}

	GLint loc = m_program->uniformLocation("uvtex");

	if (loc >= 0)
		m_program->setUniformValue(loc, 1);
	loc = m_program->uniformLocation("utex");
	if (loc >= 0)
		m_program->setUniformValue(loc, 1);
	loc = m_program->uniformLocation("vtex");
	if (loc >= 0)
		m_program->setUniformValue(loc, 2);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		shader_YUY2();
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		shader_NV16();
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		shader_NV12();
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		shader_NV24();
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
		shader_YUV_packed();
		break;

	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		shader_YUV();
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
		shader_Bayer();
		break;

	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		shader_RGB();
		break;
	}
}

void CaptureGLWin::shader_YUV()
{
	unsigned vdiv = 2, hdiv = 2;

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		vdiv = hdiv = 1;
		break;
	}

	m_screenTextureCount = 3;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 1");

	glActiveTexture(GL_TEXTURE2);
	configureTexture(2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 2");
}

void CaptureGLWin::shader_NV12()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 1");
}

void CaptureGLWin::shader_NV24()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RG, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 1");
}

void CaptureGLWin::shader_NV16()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 1");
}

void CaptureGLWin::shader_YUY2()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width() / 2, m_v4l_fmt.g_height(), 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	checkError("YUY2 shader");
}

void CaptureGLWin::shader_RGB()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	GLint internalFmt = m_accepts_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
		break;

	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_BGR666:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;

	case V4L2_PIX_FMT_RGB332:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_BYTE_3_3_2, NULL);
		break;

	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_HSV32:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_GREY:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glTexImage2D(GL_TEXTURE_2D, 0, m_accepts_srgb ? GL_SRGB8 : GL_RGB8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGB, GL_UNSIGNED_BYTE, NULL);
		break;
	}

	checkError("RGB shader");
}

void CaptureGLWin::shader_Bayer()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
		break;
	}

	checkError("Bayer shader");
}

void CaptureGLWin::shader_YUV_packed()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV555:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_YUV444:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_YUV32:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		break;
	}

	checkError("Packed YUV shader");
}

void CaptureGLWin::render_YUV(__u32 format)
{
	unsigned vdiv = 2, hdiv = 2;
	int idxU = 0;
	int idxV = 0;

	switch (format) {
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		vdiv = hdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV422P:
		idxU = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxV = idxU + (idxU / 2);
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV420:
		idxU = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxV = idxU + (idxU / 4);
		break;
	case V4L2_PIX_FMT_YVU420:
		idxV = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxU = idxV + (idxV / 4);
		break;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("YUV paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0] == NULL ? NULL : &m_curData[0][idxU]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[2]);
		break;
	}
	checkError("YUV paint utex");

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[2]);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0] == NULL ? NULL : &m_curData[0][idxV]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[2]);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	checkError("YUV paint vtex");
}

void CaptureGLWin::render_NV12(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV12 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2,
				GL_RED, GL_UNSIGNED_BYTE,
				m_curData[0] ? m_curData[0] + m_v4l_fmt.g_width() * m_v4l_fmt.g_height() : NULL);
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2,
				GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	checkError("NV12 paint uvtex");
}

void CaptureGLWin::render_NV24(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV24 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RG, GL_UNSIGNED_BYTE,
			m_curData[0] ? m_curData[0] + m_v4l_fmt.g_sizeimage() / 3 : NULL);
	checkError("NV24 paint uvtex");
}

void CaptureGLWin::render_NV16(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV16 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED, GL_UNSIGNED_BYTE,
				m_curData[0] ? m_curData[0] + m_v4l_fmt.g_sizeimage() / 2 : NULL);
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("NV16 paint uvtex");
}

void CaptureGLWin::render_YUY2(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / 2, m_v4l_fmt.g_height(),
			GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("YUY2 paint");
}

void CaptureGLWin::render_RGB(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_RGB332:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_BYTE_3_3_2, m_curData[0]);
		break;
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, m_curData[0]);
		break;

	case V4L2_PIX_FMT_GREY:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, m_curData[0]);
		break;

	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Z16:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;
	case V4L2_PIX_FMT_Y16_BE:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB555 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB565:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB565X:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB565 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_HSV32:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	case V4L2_PIX_FMT_BGR666:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 3);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("RGB paint");
}

void CaptureGLWin::render_Bayer(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;
	}
	checkError("Bayer paint");
}

void CaptureGLWin::render_YUV_packed(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_YUV555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV444:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	}
	checkError("Packed YUV paint");
}
