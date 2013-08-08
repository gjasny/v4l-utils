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

CaptureWin::CaptureWin()
{
	setWindowTitle("V4L2 Capture");
	m_hotkeyClose = new QShortcut(Qt::CTRL+Qt::Key_W, this);
	QObject::connect(m_hotkeyClose, SIGNAL(activated()), this, SLOT(close()));
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

QSize CaptureWin::getMargins()
{
 	int l, t, r, b;
 	layout()->getContentsMargins(&l, &t, &r, &b);
	return QSize(l + r, t + b + m_information.minimumSizeHint().height() + layout()->spacing());
}

void CaptureWin::setMinimumSize(int minw, int minh)
{
	QDesktopWidget *screen = QApplication::desktop();
	QRect resolution = screen->screenGeometry();
	QSize maxSize = maximumSize();

	QSize margins = getMargins();
	minw += margins.width();
	minh += margins.height();

	if (minw > resolution.width())
		minw = resolution.width();
	if (minw < 150)
		minw = 150;

	if (minh > resolution.height())
		minh = resolution.height();
	if (minh < 100)
		minh = 100;

	QWidget::setMinimumSize(minw, minh);
	QWidget::setMaximumSize(minw, minh);
	updateGeometry();
	QWidget::setMaximumSize(maxSize.width(), maxSize.height());
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
