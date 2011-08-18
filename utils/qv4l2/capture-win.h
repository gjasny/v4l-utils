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
#include <sys/time.h>

class QImage;
class QLabel;

class CaptureWin : public QWidget
{
	Q_OBJECT

public:
	CaptureWin();
	virtual ~CaptureWin() {}

	void setImage(const QImage &image, const QString &status);

protected:
	virtual void closeEvent(QCloseEvent *event);

signals:
	void close();

private:
	QLabel *m_label;
	QLabel *m_msg;
};

#endif
