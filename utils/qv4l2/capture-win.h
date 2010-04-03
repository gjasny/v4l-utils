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

#ifndef CAPTURE_WIN_H
#define CAPTURE_WIN_H

#include <QWidget>

class QImage;
class QLabel;
class QTimer;

class CaptureWin : public QWidget
{
	Q_OBJECT

public:
	CaptureWin();
	virtual ~CaptureWin() {}

	void setImage(const QImage &image, bool init = false);
	void stop();
	unsigned frame() const { return m_frame; }

protected:
	virtual void closeEvent(QCloseEvent *event);

private slots:
	void update();

signals:
	void close();

private:
	QLabel *m_label;
	QLabel *m_msg;
	unsigned m_frame;
	unsigned m_lastFrame;
	unsigned m_fps;
	QTimer *m_timer;
};

#endif
