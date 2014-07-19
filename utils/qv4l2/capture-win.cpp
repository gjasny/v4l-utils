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

#include <QCloseEvent>
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <QApplication>
#include <QDesktopWidget>

#define MIN_WIN_SIZE_WIDTH 160
#define MIN_WIN_SIZE_HEIGHT 120

bool CaptureWin::m_enableScaling = true;
double CaptureWin::m_pixelAspectRatio = 1.0;
CropMethod CaptureWin::m_cropMethod = QV4L2_CROP_NONE;

CaptureWin::CaptureWin(ApplicationWindow *aw) :
	m_appWin(aw)
{
	setWindowTitle("V4L2 Capture");
	m_hotkeyClose = new QShortcut(Qt::CTRL+Qt::Key_W, this);
	connect(m_hotkeyClose, SIGNAL(activated()), this, SLOT(close()));
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
	m_information = new QLabel(this);
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
			  unsigned char *data, unsigned char *data2, const QString &info)
{
	m_frame.planeData[0] = data;
	m_frame.planeData[1] = data2;
	m_frame.info         = info;

	m_frame.updated = false;
	if (width != m_frame.size.width() || height != m_frame.size.height()
	    || format != m_frame.format) {
		m_frame.size.setHeight(height);
		m_frame.size.setWidth(width);
		m_frame.format       = format;
		m_frame.updated      = true;
		updateSize();
	}
	m_information->setText(m_frame.info);

	setRenderFrame();
}

void CaptureWin::buildWindow(QWidget *videoSurface)
{
	int l, t, r, b;
	m_vboxLayout = new QVBoxLayout(this);
	m_information->setText("No Frame");
	m_vboxLayout->getContentsMargins(&l, &t, &r, &b);
	m_vboxSpacing = t + b;
	m_vboxLayout->setMargin(0);
	m_vboxLayout->addWidget(videoSurface, 1000, Qt::AlignCenter);
	m_bottom = new QWidget(parentWidget());
	m_bottom->installEventFilter(this);
	
	QHBoxLayout *hbox = new QHBoxLayout(m_bottom);
	hbox->addWidget(m_information, 1, Qt::AlignLeft);
	
	m_fullscreenButton = new QPushButton("Show Fullscreen", m_bottom);
	m_fullscreenButton->setMaximumWidth(200);
	m_fullscreenButton->setMinimumWidth(100);
	hbox->addWidget(m_fullscreenButton, 0, Qt::AlignRight);
	connect(m_fullscreenButton, SIGNAL(clicked()), m_appWin->m_makeFullScreenAct, SLOT(toggle()));
	m_vboxLayout->addWidget(m_bottom, 0, Qt::AlignBottom);
	m_vboxLayout->setSpacing(m_vboxSpacing);
	
	setContextMenuPolicy(Qt::CustomContextMenu);
	connect(this, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customMenuRequested(QPoint)));
}

void CaptureWin::resetSize()
{
	if (m_appWin->m_makeFullScreenAct->isChecked())
		escape();

        // Force resize even if no size change
	QSize resetFrameSize = m_origFrameSize;
	m_origFrameSize.setWidth(0);
	m_origFrameSize.setHeight(0);

	setWindowSize(resetFrameSize);
}

QSize CaptureWin::cropFrameSize(QSize size)
{
	// Crop width
	int validWidth;

	if (m_cropMethod == QV4L2_CROP_P43)
		validWidth = (size.height() * 4.0 / 3.0) / m_pixelAspectRatio;
	else
		validWidth = size.width(); // no width crop

	if (validWidth < MIN_WIN_SIZE_WIDTH || validWidth >= size.width())
		validWidth = size.width();  // no width crop

	int deltaWidth = (size.width() - validWidth) / 2;

	// Crop height
	double realWidth = size.width() * m_pixelAspectRatio;
	int validHeight;

	switch (m_cropMethod) {
	case QV4L2_CROP_W149:
		validHeight = realWidth / (14.0 / 9.0);
		break;
	case QV4L2_CROP_W169:
		validHeight = realWidth / (16.0 / 9.0);
		break;
	case QV4L2_CROP_C185:
		validHeight = realWidth / 1.85;
		break;
	case QV4L2_CROP_C239:
		validHeight = realWidth / 2.39;
		break;
	case QV4L2_CROP_TB:
		validHeight = size.height() - 2;
		break;
	default:
		validHeight = size.height();  // No height crop
	}

	if (validHeight < MIN_WIN_SIZE_HEIGHT || validHeight >= size.height())
		validHeight = size.height();  // No height crop

	int deltaHeight = (size.height() - validHeight) / 2;

	return QSize(deltaWidth, deltaHeight);
}

void CaptureWin::updateSize()
{
	m_crop.updated = false;
	if (m_frame.updated) {
		m_scaledSize = scaleFrameSize(m_windowSize, m_frame.size);
		m_crop.delta = cropFrameSize(m_frame.size);
		m_crop.size = m_frame.size - 2 * m_crop.delta;
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
	if (m_information->isVisible())
		return QSize(l + r, t + b + m_information->minimumSizeHint().height() + layout()->spacing());
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

	QSize windowSize =  margins + pixelAspectFrameSize(frameSize)
		- 2 * cropFrameSize(frameSize);

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
	QSize croppedSize = frame - 2 * cropFrameSize(frame);
	QSize actualSize = pixelAspectFrameSize(croppedSize);

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

void CaptureWin::mouseDoubleClickEvent(QMouseEvent *e)
{
	m_appWin->m_makeFullScreenAct->toggle();
}

bool CaptureWin::eventFilter(QObject *target, QEvent *event)
{
	if (target == m_bottom && isFullScreen()) {
		if (event->type() == QEvent::Enter) {
			m_bottom->setStyleSheet("background-color:#bebebe;");
			m_bottom->setFixedHeight(75);
			m_fullscreenButton->show();
			m_information->show();
			return true;
		}
		if (event->type() == QEvent::Leave && m_bottom->geometry().bottom() >= QCursor::pos().y()) {
			m_bottom->setMinimumHeight(0);
			m_bottom->setStyleSheet("background-color:#000000;");
			m_fullscreenButton->hide();
			m_information->hide();
			return true;
		}
	}
	return false;
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
		m_fullscreenButton->setText("Exit Fullscreen");
		setStyleSheet("background-color:#000000;");
		m_vboxLayout->setSpacing(0);
		m_fullscreenButton->hide();
		m_information->hide();
	} else {
		showNormal();
		m_vboxLayout->setSpacing(m_vboxSpacing);
		m_bottom->setMinimumHeight(0);
		m_bottom->setMaximumHeight(height());
		m_fullscreenButton->setText("Show Fullscreen");
		setStyleSheet("background-color:none;");
		m_bottom->setStyleSheet("background-color:none;");
		m_fullscreenButton->show();
		m_information->show();
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
	menu->addAction(m_appWin->m_snapshotAct);
	menu->addAction(m_appWin->m_showFramesAct);
	
	menu->popup(mapToGlobal(pos));
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
