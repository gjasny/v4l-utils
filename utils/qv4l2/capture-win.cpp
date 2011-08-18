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
#include <stdio.h>
#include <QLabel>
#include <QImage>
#include <QVBoxLayout>
#include <QCloseEvent>

#include "qv4l2.h"
#include "capture-win.h"

CaptureWin::CaptureWin()
{
	QVBoxLayout *vbox = new QVBoxLayout(this);

	setWindowTitle("V4L2 Capture");
	m_label = new QLabel();
	m_msg = new QLabel("No frame");

	vbox->addWidget(m_label);
	vbox->addWidget(m_msg);
}

void CaptureWin::setImage(const QImage &image, const QString &status)
{
	m_label->setPixmap(QPixmap::fromImage(image));
	m_msg->setText(status);
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
