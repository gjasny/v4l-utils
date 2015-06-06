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

#ifndef CAPTURE_WIN_QT_H
#define CAPTURE_WIN_QT_H

#include "qv4l2.h"
#include "capture-win.h"

#include <QLabel>
#include <QImage>
#include <QResizeEvent>

class CaptureWinQt : public CaptureWin
{
public:
	CaptureWinQt(ApplicationWindow *aw);
	~CaptureWinQt();

	void stop();
	bool hasNativeFormat(__u32 format);
	static bool isSupported() { return true; }
	void setColorspace(unsigned colorspace, unsigned xfer_func,
			unsigned ycbcr_enc, unsigned quantization, bool is_sdtv) {}
	void setField(unsigned field) {}
	void setBlending(bool enable) {}
	void setLinearFilter(bool enable) {}

protected:
	void resizeEvent(QResizeEvent *event);
	void setRenderFrame();

private:
	bool findNativeFormat(__u32 format, QImage::Format &dstFmt);
	void paintFrame();

	QImage *m_image;
	unsigned char *m_data;
	QLabel *m_videoSurface;
	bool m_supportedFormat;
	bool m_filled;
	int m_cropBytes;
	int m_cropOffset;
};
#endif
