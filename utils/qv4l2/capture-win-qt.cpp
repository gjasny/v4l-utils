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
	m_frame(new QImage(0, 0, QImage::Format_Invalid)),
	m_data(NULL),
	m_supportedFormat(false),
	m_filled(false)
{

	CaptureWin::buildWindow(&m_videoSurface);
	m_scaledSize.setWidth(0);
	m_scaledSize.setHeight(0);
	m_crop.cropH  = 0;
	m_crop.cropW  = 0;
	m_crop.height = 0;
	m_crop.width  = 0;
	m_crop.offset = 0;
	m_crop.bytes  = 0;
}

CaptureWinQt::~CaptureWinQt()
{
	delete m_frame;
}

void CaptureWinQt::resizeScaleCrop()
{
	m_scaledSize = scaleFrameSize(QSize(m_videoSurface.width(), m_videoSurface.height()),
				      QSize(m_frame->width(), m_frame->height()));

	if (!m_crop.bytes || m_crop.cropH != cropHeight(m_frame->width(), m_frame->height())
	    || m_crop.cropW != cropWidth(m_frame->width(), m_frame->height())) {

		m_crop.cropH  = cropHeight(m_frame->width(), m_frame->height());
		m_crop.cropW  = cropWidth(m_frame->width(), m_frame->height());
		m_crop.height = m_frame->height() - (m_crop.cropH * 2);
		m_crop.width  = m_frame->width() - (m_crop.cropW * 2);
		m_crop.offset = m_crop.cropH * (m_frame->depth() / 8) * m_frame->width()
			+ m_crop.cropW * (m_frame->depth() / 8);

		// Even though the values above can be valid, it might be that there is no
		// data at all. This makes sure that it is.
		m_crop.bytes = m_crop.height * m_crop.width * (m_frame->depth() / 8);
	}
}

void CaptureWinQt::resizeEvent(QResizeEvent *event)
{
	resizeScaleCrop();
	paintFrame();
}

void CaptureWinQt::setFrame(int width, int height, __u32 format,
		unsigned char *data, unsigned char *data2, const QString &info)
{
	m_data = data;

	QImage::Format dstFmt;
	m_supportedFormat = findNativeFormat(format, dstFmt);
	if (!m_supportedFormat)
		dstFmt = QImage::Format_RGB888;

	if (m_frame->width() != width
	    || m_frame->height() != height
	    || m_frame->format() != dstFmt) {
		delete m_frame;
		m_frame = new QImage(width, height, dstFmt);
		// Force a recalculation by setting this to 0.
		m_crop.bytes = 0;

		resizeScaleCrop();
	}

	m_information.setText(info);
	paintFrame();
}

void CaptureWinQt::paintFrame()
{
	if (!m_supportedFormat || !m_crop.bytes) {
		if (!m_filled) {
			m_filled = true;
			m_frame->fill(0);
			QPixmap img = QPixmap::fromImage(*m_frame);
			m_videoSurface.setPixmap(img);
		}
		return;
	}
	m_filled = false;

	unsigned char *data = (m_data == NULL) ? m_frame->bits() : m_data;

	QImage displayFrame(&data[m_crop.offset], m_crop.width, m_crop.height,
			    m_frame->width() * (m_frame->depth() / 8), m_frame->format());

	QPixmap img = QPixmap::fromImage(displayFrame);

	// No scaling is performed by scaled() if the scaled size is equal to original size
	img = img.scaled(m_scaledSize.width(), m_scaledSize.height(), Qt::IgnoreAspectRatio);

	m_videoSurface.setPixmap(img);
}

void CaptureWinQt::stop()
{
	if (m_data != NULL)
		memcpy(m_frame->bits(), m_data, m_frame->numBytes());
	m_data = NULL;
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
