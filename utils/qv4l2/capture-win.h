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

#include "qv4l2.h"

#include <QWidget>
#include <QShortcut>
#include <QLabel>

enum CropMethod {
	// Crop Height
	QV4L2_CROP_NONE,
	QV4L2_CROP_W149,
	QV4L2_CROP_W169,
	QV4L2_CROP_C185,
	QV4L2_CROP_C239,
	QV4L2_CROP_TB,

	// Crop Width
	QV4L2_CROP_P43,
};

class CaptureWin : public QWidget
{
	Q_OBJECT

public:
	CaptureWin();
	~CaptureWin();

	void resize(int minw, int minh);
	void enableScaling(bool enable);
	void setPixelAspectRatio(double ratio);
	void setCropMethod(CropMethod crop);

	/**
	 * @brief Set a frame into the capture window.
	 *
	 * When called the capture stream is
	 * either already running or starting for the first time.
	 *
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 * @param format The frame's format, given as a V4L2_PIX_FMT.
	 * @param data The frame data.
	 * @param info A string containing capture information.
	 */
	virtual void setFrame(int width, int height, __u32 format,
			      unsigned char *data, unsigned char *data2, const QString &info) = 0;

	/**
	 * @brief Called when the capture stream is stopped.
	 */
	virtual void stop() = 0;

	/**
	 * @brief Queries the current capture window for its supported formats.
	 *
	 * Unsupported formats are converted by v4lconvert_convert().
	 *
	 * @param format The frame format question, given as a V4L2_PIX_FMT.
	 * @return true if the format is supported, false if not.
	 */
	virtual bool hasNativeFormat(__u32 format) = 0;

	/**
	 * @brief Defines wether a capture window is supported.
	 *
	 * By default nothing is supported, but derived classes can override this.
	 *
	 * @return true if the capture window is supported on the system, false if not.
	 */
	static bool isSupported() { return false; }

	/**
	 * @brief Return the scaled size.
	 *
	 * Scales a frame to fit inside a given window. Preserves aspect ratio.
	 *
	 * @param window The window the frame shall scale into
	 * @param frame The frame to scale
	 * @return The scaledsizes to use for the frame
	 */
	static QSize scaleFrameSize(QSize window, QSize frame);

	/**
	 * @brief Get the number of pixels to crop.
	 *
	 * When cropping is applied this gives the number of pixels to
	 * remove from top and bottom. To get total multiply the return
	 * value by 2.
	 *
	 * @param width Frame width
	 * @param height Frame height
	 * @return The pixels to remove when cropping height
	 *
	 * @note The width and height must be original frame size
	 *       to ensure that the cropping is done correctly.
	 */
	static int cropHeight(int width, int height);
	static int cropWidth(int width, int height);

	/**
	 * @brief Get the frame width when aspect ratio is applied if ratio > 1.
	 *
	 * @param width The original frame width.
	 * @return The width with aspect ratio correction (scaling must be enabled).
	 */
	static int actualFrameWidth(int width);

	/**
	 * @brief Get the frame height when aspect ratio is applied if ratio < 1.
	 *
	 * @param width The original frame width.
	 * @return The width with aspect ratio correction (scaling must be enabled).
	 */
	static int actualFrameHeight(int height);

public slots:
	void resetSize();

protected:
	void closeEvent(QCloseEvent *event);

	/**
	 * @brief Get the amount of space outside the video frame.
	 *
	 * The margins are that of the window that encloses the displaying
	 * video frame. The sizes are total in both width and height.
	 *
	 * @return The margins around the video frame.
	 */
	QSize getMargins();

	/**
	 * @brief Creates the content of the window.
	 *
	 * Construct the new window layout and adds the video display surface.
	 * @param videoSurface The widget that contains the renderer.
	 */
	void buildWindow(QWidget *videoSurface);

	/**
	 * @brief A label that can is used to display capture information.
	 *
	 * @note This must be set in the derived class' setFrame() function.
	 */
	QLabel m_information;

	/**
	 * @brief Determines if scaling is to be applied to video frame.
	 */
	static bool m_enableScaling;

signals:
	void close();

private:
	static double m_pixelAspectRatio;
	static CropMethod m_cropMethod;
	QShortcut *m_hotkeyClose;
	QShortcut *m_hotkeyScaleReset;
	int m_curWidth;
	int m_curHeight;
};
#endif
