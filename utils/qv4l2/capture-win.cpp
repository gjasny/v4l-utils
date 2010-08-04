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

	m_frame = 0;
	m_label = new QLabel();
	m_msg = new QLabel("No frame");

	vbox->addWidget(m_label);
	vbox->addWidget(m_msg);
}

void CaptureWin::setImage(const QImage &image, bool init)
{
	m_label->setPixmap(QPixmap::fromImage(image));
	if (init) {
		m_frame = m_lastFrame = m_fps = 0;
		m_msg->setText("No frame");
	} else {
		struct timeval tv, res;

		if (m_frame == 0)
			gettimeofday(&m_tv, NULL);
		gettimeofday(&tv, NULL);
		timersub(&tv, &m_tv, &res);
		if (res.tv_sec) {
			m_fps = (100 * (m_frame - m_lastFrame)) /
				(res.tv_sec * 100 + res.tv_usec / 10000);
			m_lastFrame = m_frame;
			m_tv = tv;
		}
		m_msg->setText(QString("Frame: %1 Fps: %2")
				.arg(++m_frame).arg(m_fps));
	}
}

void CaptureWin::closeEvent(QCloseEvent *event)
{
	QWidget::closeEvent(event);
	emit close();
}
