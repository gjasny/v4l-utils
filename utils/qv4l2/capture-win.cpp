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

CaptureWin::CaptureWin() :
	m_curWidth(-1),
	m_curHeight(-1)
{
	setWindowTitle("V4L2 Capture");
	m_hotkeyClose = new QShortcut(Qt::CTRL+Qt::Key_W, this);
	connect(m_hotkeyClose, SIGNAL(activated()), this, SLOT(close()));
}

CaptureWin::~CaptureWin()
{
	if (layout() == NULL)
		return;

	layout()->removeWidget(this);
	delete layout();
	delete m_hotkeyClose;
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
	QResizeEvent *event = new QResizeEvent(QSize(width(), height()), QSize(width(), height()));
	QCoreApplication::sendEvent(this, event);
	delete event;
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
	width += margins.width();
	height += margins.height();

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
	int actualFrameWidth = frame.width();;
	int actualFrameHeight = frame.height();

	if (!m_enableScaling) {
		window.setWidth(frame.width());
		window.setHeight(frame.height());
	}

	double newW, newH;
	if (window.width() >= window.height()) {
		newW = (double)window.width() / actualFrameWidth;
		newH = (double)window.height() / actualFrameHeight;
	} else {
		newH = (double)window.width() / actualFrameWidth;
		newW = (double)window.height() / actualFrameHeight;
	}
	double resized = std::min(newW, newH);

	return QSize((int)(actualFrameWidth * resized), (int)(actualFrameHeight * resized));
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
