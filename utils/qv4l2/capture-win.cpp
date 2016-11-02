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

#include "capture-win.h"

#undef min
#undef max

#include <QCloseEvent>
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <QApplication>
#include <QDesktopWidget>

#include <math.h>

#define MIN_WIN_SIZE_WIDTH 16
#define MIN_WIN_SIZE_HEIGHT 12

bool CaptureWin::m_enableScaling = true;
double CaptureWin::m_pixelAspectRatio = 1.0;
CropMethod CaptureWin::m_cropMethod = QV4L2_CROP_NONE;

CaptureWin::CaptureWin(ApplicationWindow *aw) :
	m_appWin(aw)
{
	setWindowTitle("V4L2 Capture");
	m_hotkeyClose = new QShortcut(Qt::CTRL+Qt::Key_W, this);
	connect(m_hotkeyClose, SIGNAL(activated()), this, SLOT(close()));
	connect(new QShortcut(Qt::Key_Q, this), SIGNAL(activated()), this, SLOT(close()));
	m_hotkeyScaleReset = new QShortcut(Qt::CTRL+Qt::Key_F, this);
	connect(m_hotkeyScaleReset, SIGNAL(activated()), this, SLOT(resetSize()));
	connect(aw->m_resetScalingAct, SIGNAL(triggered()), this, SLOT(resetSize()));
	m_hotkeyExitFullscreen = new QShortcut(Qt::Key_Escape, this);
	connect(m_hotkeyExitFullscreen, SIGNAL(activated()), this, SLOT(escape()));
	m_hotkeyToggleFullscreen = new QShortcut(Qt::Key_F, this);
	connect(m_hotkeyToggleFullscreen, SIGNAL(activated()), aw->m_makeFullScreenAct, SLOT(toggle()));
	m_exitFullScreen = new QAction(QIcon(":/fullscreenexit.png"), "Exit Fullscreen", this);
	connect(m_exitFullScreen, SIGNAL(triggered()), this, SLOT(escape()));
	m_enterFullScreen = new QAction(QIcon(":/fullscreen.png"), "Show Fullscreen", this);
	connect(m_enterFullScreen, SIGNAL(triggered()), this, SLOT(fullScreen()));
	m_frame.format = 0;
	m_frame.size.setWidth(0);
	m_frame.size.setHeight(0);
	m_frame.planeData[0] = NULL;
	m_frame.planeData[1] = NULL;
	m_frame.planeData[2] = NULL;
	m_crop.delta.setWidth(0);
	m_crop.delta.setHeight(0);
	m_crop.size.setWidth(0);
	m_crop.size.setHeight(0);
	m_crop.updated = 0;
	m_scaledSize.setWidth(0);
	m_scaledSize.setHeight(0);
	m_origFrameSize.setWidth(0);
	m_origFrameSize.setHeight(0);
	m_windowSize.setWidth(0);
	m_windowSize.setHeight(0);
}

CaptureWin::~CaptureWin()
{
	if (layout() == NULL)
		return;

	layout()->removeWidget(this);
	delete layout();
	delete m_hotkeyClose;
	delete m_hotkeyScaleReset;
}

void CaptureWin::setFrame(int width, int height, __u32 format,
	  unsigned char *data, unsigned char *data2, unsigned char *data3)
{
	m_frame.planeData[0] = data;
	m_frame.planeData[1] = data2;
	m_frame.planeData[2] = data3;

	m_frame.updated = false;
	if (width != m_frame.size.width() || height != m_frame.size.height()
	    || format != m_frame.format) {
		m_frame.size.setHeight(height);
		m_frame.size.setWidth(width);
		m_frame.format       = format;
		m_frame.updated      = true;
		updateSize();
	}

	setRenderFrame();
}

void CaptureWin::buildWindow(QWidget *videoSurface)
{
	int l, t, r, b;
	m_vboxLayout = new QVBoxLayout(this);
	m_vboxLayout->getContentsMargins(&l, &t, &r, &b);
	m_vboxLayout->setMargin(0);
	m_vboxLayout->addWidget(videoSurface, 1000, Qt::AlignCenter);

	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customMenuRequested(QPoint)));
}

void CaptureWin::resetSize()
{
        // Force resize even if no size change
	QSize resetFrameSize = m_origFrameSize;
	m_origFrameSize.setWidth(0);
	m_origFrameSize.setHeight(0);

	setWindowSize(resetFrameSize);
}

QSize CaptureWin::cropSize(QSize size)
{
	QSize croppedSize = size;
	double realWidth = size.width() * m_pixelAspectRatio;
	double realHeight = size.height() / m_pixelAspectRatio;
	double aspectRatio = 1;

	switch (m_cropMethod) {
	case QV4L2_CROP_P43:
		aspectRatio = 4.0 / 3.0;
		break;
	case QV4L2_CROP_W149:
		aspectRatio = 14.0 / 9.0;
		break;
	case QV4L2_CROP_W169:
		aspectRatio = 16.0 / 9.0;
		break;
	case QV4L2_CROP_C185:
		aspectRatio = 1.85;
		break;
	case QV4L2_CROP_C239:
		aspectRatio = 2.39;
		break;
	case QV4L2_CROP_TB:
		croppedSize.setHeight(size.height() - 2);
		break;
	default:
		break;  // No cropping
	}

	if ((m_cropMethod != QV4L2_CROP_TB) && (m_cropMethod != QV4L2_CROP_NONE)) {
		if (realWidth / size.height() < aspectRatio)
			croppedSize.setHeight(realWidth / aspectRatio);
		else
			croppedSize.setWidth(realHeight * aspectRatio);
	}

	if (croppedSize.width() >= size.width())
		croppedSize.setWidth(size.width());
	if (croppedSize.width() < MIN_WIN_SIZE_WIDTH)
		croppedSize.setWidth(MIN_WIN_SIZE_WIDTH);
	if (croppedSize.height() >= size.height())
		croppedSize.setHeight(size.height());
	if (croppedSize.height() < MIN_WIN_SIZE_HEIGHT)
		croppedSize.setHeight(MIN_WIN_SIZE_HEIGHT);

	return croppedSize;
}

void CaptureWin::updateSize()
{
	m_crop.updated = false;
	if (m_frame.updated) {
		m_scaledSize = scaleFrameSize(m_windowSize, m_frame.size);
		m_crop.size = cropSize(m_frame.size);
		m_crop.delta = (m_frame.size - m_crop.size) / 2;
		m_crop.updated = true;
	}
}

void CaptureWin::setCropMethod(CropMethod crop)
{
	m_cropMethod = crop;
	resetSize();
}

QSize CaptureWin::pixelAspectFrameSize(QSize size)
{
	if (!m_enableScaling)
		return size;

	if (m_pixelAspectRatio > 1)
		size.rwidth() *= m_pixelAspectRatio;

	if (m_pixelAspectRatio < 1)
		size.rheight() /= m_pixelAspectRatio;

	return size;
}

QSize CaptureWin::getMargins()
{
	int l, t, r, b;
	layout()->getContentsMargins(&l, &t, &r, &b);
	return QSize(l + r, t + b);
}

void CaptureWin::enableScaling(bool enable)
{
	if (!enable) {
		QSize margins = getMargins();
		QWidget::setMinimumSize(m_origFrameSize.width() + margins.width(),
					m_origFrameSize.height() + margins.height());
	} else {
		QWidget::setMinimumSize(MIN_WIN_SIZE_WIDTH, MIN_WIN_SIZE_HEIGHT);
	}
	m_enableScaling = enable;
	resetSize();
}

void CaptureWin::setWindowSize(QSize frameSize)
{
	// Dont resize window if the frame size is the same in
	// the event the window has been paused when beeing resized.
	if (frameSize == m_origFrameSize)
		return;

	m_origFrameSize = frameSize;

	QSize margins = getMargins();
	QDesktopWidget *screen = QApplication::desktop();
	QRect resolution = screen->screenGeometry();

	QSize windowSize =  pixelAspectFrameSize(cropSize(frameSize)) + margins;

	if (windowSize.width() > resolution.width())
		windowSize.setWidth(resolution.width());
	if (windowSize.width() < MIN_WIN_SIZE_WIDTH)
		windowSize.setWidth(MIN_WIN_SIZE_WIDTH);

	if (windowSize.height() > resolution.height())
		windowSize.setHeight(resolution.height());
	if (windowSize.height() < MIN_WIN_SIZE_HEIGHT)
		windowSize.setHeight(MIN_WIN_SIZE_HEIGHT);

	QWidget::setMinimumSize(MIN_WIN_SIZE_WIDTH, MIN_WIN_SIZE_HEIGHT);

	m_frame.size = frameSize;

	QWidget::resize(windowSize);
}

QSize CaptureWin::scaleFrameSize(QSize window, QSize frame)
{
	QSize actualSize = pixelAspectFrameSize(cropSize(frame));

	if (!m_enableScaling) {
		window.setWidth(actualSize.width());
		window.setHeight(actualSize.height());
	}

	qreal newW, newH;
	if (window.width() >= window.height()) {
		newW = (qreal)window.width() / actualSize.width();
		newH = (qreal)window.height() / actualSize.height();
	} else {
		newH = (qreal)window.width() / actualSize.width();
		newW = (qreal)window.height() / actualSize.height();
	}
	qreal resized = (qreal)std::min(newW, newH);

	return (actualSize * resized);
}

void CaptureWin::setPixelAspectRatio(double ratio)
{
	m_pixelAspectRatio = ratio;
	resetSize();
}

float CaptureWin::getHorScaleFactor()
{
	float ow, sw, wscale;

	sw = m_scaledSize.width();
	ow = m_origFrameSize.width();
	wscale = floor(100 * (sw / ow)) / 100.0;

	return wscale;
}

float CaptureWin::getVertScaleFactor()
{
	float oh, sh, hscale;

	sh = m_scaledSize.height();
	oh = m_origFrameSize.height();
	hscale = floor(100 * (sh / oh)) / 100.0;

	return hscale;
}

void CaptureWin::mouseDoubleClickEvent(QMouseEvent *e)
{
	m_appWin->m_makeFullScreenAct->toggle();
}

void CaptureWin::escape()
{
	m_appWin->m_makeFullScreenAct->setChecked(false);
}

void CaptureWin::fullScreen()
{
	m_appWin->m_makeFullScreenAct->setChecked(true);
}

void CaptureWin::makeFullScreen(bool enable)
{
	if (enable) {
		showFullScreen();
		setStyleSheet("background-color:#000000;");
	} else {
		showNormal();
		setStyleSheet("background-color:none;");
	}
	QSize resetFrameSize = m_origFrameSize;
	m_origFrameSize.setWidth(0);
	m_origFrameSize.setHeight(0);

	setWindowSize(resetFrameSize);
}

void CaptureWin::customMenuRequested(QPoint pos)
{
	QMenu *menu = new QMenu(this);
	
	if (isFullScreen()) {
		menu->addAction(m_exitFullScreen);
		menu->setStyleSheet("background-color:none;");
	} else {
		menu->addAction(m_enterFullScreen);
	}
	
	menu->addAction(m_appWin->m_resetScalingAct);
	menu->addAction(m_appWin->m_useBlendingAct);
	menu->addAction(m_appWin->m_useLinearAct);
	menu->addAction(m_appWin->m_snapshotAct);
	menu->addAction(m_appWin->m_showFramesAct);
	menu->addMenu(m_appWin->m_overrideColorspaceMenu);
	menu->addMenu(m_appWin->m_overrideXferFuncMenu);
	menu->addMenu(m_appWin->m_overrideYCbCrEncMenu);
	menu->addMenu(m_appWin->m_overrideQuantizationMenu);
	
	menu->popup(mapToGlobal(pos));
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
