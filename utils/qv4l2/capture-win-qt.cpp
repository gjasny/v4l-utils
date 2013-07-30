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

#include "capture-win-qt.h"

CaptureWinQt::CaptureWinQt() :
	m_frame(new QImage(0, 0, QImage::Format_Invalid))
{

	CaptureWin::buildWindow(&m_videoSurface);
}

CaptureWinQt::~CaptureWinQt()
{
	delete m_frame;
}

void CaptureWinQt::setFrame(int width, int height, __u32 format, unsigned char *data, const QString &info)
{
	QImage::Format dstFmt;
	bool supported = findNativeFormat(format, dstFmt);
	if (!supported)
		dstFmt = QImage::Format_RGB888;

	if (m_frame->width() != width || m_frame->height() != height || m_frame->format() != dstFmt) {
		delete m_frame;
		m_frame = new QImage(width, height, dstFmt);
	}

	if (data == NULL || !supported)
		m_frame->fill(0);
	else
		memcpy(m_frame->bits(), data, m_frame->numBytes());

	m_information.setText(info);
	m_videoSurface.setPixmap(QPixmap::fromImage(*m_frame));
}

bool CaptureWinQt::hasNativeFormat(__u32 format)
{
	QImage::Format fmt;
	return findNativeFormat(format, fmt);
}

bool CaptureWinQt::findNativeFormat(__u32 format, QImage::Format &dstFmt)
{
	static const struct {
		__u32 v4l2_pixfmt;
		QImage::Format qt_pixfmt;
	} supported_fmts[] = {
#if Q_BYTE_ORDER == Q_BIG_ENDIAN
		{ V4L2_PIX_FMT_RGB32, QImage::Format_RGB32 },
		{ V4L2_PIX_FMT_RGB24, QImage::Format_RGB888 },
		{ V4L2_PIX_FMT_RGB565X, QImage::Format_RGB16 },
		{ V4L2_PIX_FMT_RGB555X, QImage::Format_RGB555 },
#else
		{ V4L2_PIX_FMT_BGR32, QImage::Format_RGB32 },
		{ V4L2_PIX_FMT_RGB24, QImage::Format_RGB888 },
		{ V4L2_PIX_FMT_RGB565, QImage::Format_RGB16 },
		{ V4L2_PIX_FMT_RGB555, QImage::Format_RGB555 },
		{ V4L2_PIX_FMT_RGB444, QImage::Format_RGB444 },
#endif
		{ 0, QImage::Format_Invalid }
	};

	for (int i = 0; supported_fmts[i].v4l2_pixfmt; i++) {
		if (supported_fmts[i].v4l2_pixfmt == format) {
			dstFmt = supported_fmts[i].qt_pixfmt;
			return true;
		}
	}
	return false;
}
