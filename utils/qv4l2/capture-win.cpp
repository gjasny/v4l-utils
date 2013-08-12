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

CaptureWin::CaptureWin() :
	m_curWidth(-1),
	m_curHeight(-1)
{
	setWindowTitle("V4L2 Capture");
	m_hotkeyClose = new QShortcut(Qt::CTRL+Qt::Key_W, this);
	connect(m_hotkeyClose, SIGNAL(activated()), this, SLOT(close()));
	m_hotkeyScaleReset = new QShortcut(Qt::CTRL+Qt::Key_F, this);
	connect(m_hotkeyScaleReset, SIGNAL(activated()), this, SLOT(resetSize()));
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

void CaptureWin::buildWindow(QWidget *videoSurface)
{
	int l, t, r, b;
	QVBoxLayout *vbox = new QVBoxLayout(this);
	m_information.setText("No Frame");
	vbox->addWidget(videoSurface, 2000);
	vbox->addWidget(&m_information, 1, Qt::AlignBottom);
	vbox->getContentsMargins(&l, &t, &r, &b);
	vbox->setSpacing(b);
}

void CaptureWin::resetSize()
{
	if (isMaximized())
		showNormal();

	int w = m_curWidth;
	int h = m_curHeight;
	m_curWidth = -1;
	m_curHeight = -1;
	resize(w, h);
}

int CaptureWin::cropHeight(int width, int height)
{
	int validHeight;

	switch (m_cropMethod) {
	case QV4L2_CROP_W149:
		validHeight = (int)(width / 1.57);
		break;
	case QV4L2_CROP_W169:
		validHeight = (int)(width / 1.78);
		break;
	case QV4L2_CROP_C185:
		validHeight = (int)(width / 1.85);
		break;
	case QV4L2_CROP_C239:
		validHeight = (int)(width / 2.39);
		break;
	case QV4L2_CROP_TB:
		validHeight = height - 2;
		break;
	default:
		return 0;
	}

	if (validHeight < MIN_WIN_SIZE_HEIGHT || validHeight >= height)
		return 0;

	return (height - validHeight) / 2;
}


void CaptureWin::setCropMethod(CropMethod crop)
{
	m_cropMethod = crop;
	QResizeEvent event (QSize(width(), height()), QSize(width(), height()));
	QCoreApplication::sendEvent(this, &event);
}

int CaptureWin::actualFrameWidth(int width)
{
	if (m_enableScaling)
		return width * m_pixelAspectRatio;

	return width;
}

QSize CaptureWin::getMargins()
{
	int l, t, r, b;
	layout()->getContentsMargins(&l, &t, &r, &b);
	return QSize(l + r, t + b + m_information.minimumSizeHint().height() + layout()->spacing());
}

void CaptureWin::enableScaling(bool enable)
{
	if (!enable) {
		QSize margins = getMargins();
		QWidget::setMinimumSize(m_curWidth + margins.width(), m_curHeight + margins.height());
	} else {
		QWidget::setMinimumSize(MIN_WIN_SIZE_WIDTH, MIN_WIN_SIZE_HEIGHT);
	}
	m_enableScaling = enable;
	QResizeEvent event (QSize(width(), height()), QSize(width(), height()));
	QCoreApplication::sendEvent(this, &event);
}

void CaptureWin::resize(int width, int height)
{
	// Dont resize window if the frame size is the same in
	// the event the window has been paused when beeing resized.
	if (width == m_curWidth && height == m_curHeight)
		return;

	m_curWidth = width;
	m_curHeight = height;

	QSize margins = getMargins();
	height = height + margins.height() - cropHeight(width, height) * 2;
	width = margins.width() + actualFrameWidth(width);

	QDesktopWidget *screen = QApplication::desktop();
	QRect resolution = screen->screenGeometry();

	if (width > resolution.width())
		width = resolution.width();
	if (width < MIN_WIN_SIZE_WIDTH)
		width = MIN_WIN_SIZE_WIDTH;

	if (height > resolution.height())
		height = resolution.height();
	if (height < MIN_WIN_SIZE_HEIGHT)
		height = MIN_WIN_SIZE_HEIGHT;

	QWidget::setMinimumSize(MIN_WIN_SIZE_WIDTH, MIN_WIN_SIZE_HEIGHT);
	QWidget::resize(width, height);
}

QSize CaptureWin::scaleFrameSize(QSize window, QSize frame)
{
	int actualWidth;
	int actualHeight = frame.height() - cropHeight(frame.width(), frame.height()) * 2;

	if (!m_enableScaling) {
		window.setWidth(frame.width());
		window.setHeight(frame.height());
		actualWidth = frame.width();
	} else {
		actualWidth = CaptureWin::actualFrameWidth(frame.width());
	}

	double newW, newH;
	if (window.width() >= window.height()) {
		newW = (double)window.width() / actualWidth;
		newH = (double)window.height() / actualHeight;
	} else {
		newH = (double)window.width() / actualWidth;
		newW = (double)window.height() / actualHeight;
	}
	double resized = std::min(newW, newH);

	return QSize((int)(actualWidth * resized), (int)(actualHeight * resized));
}

void CaptureWin::setPixelAspectRatio(double ratio)
{
	m_pixelAspectRatio = ratio;
	QResizeEvent event(QSize(width(), height()), QSize(width(), height()));
	QCoreApplication::sendEvent(this, &event);
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
