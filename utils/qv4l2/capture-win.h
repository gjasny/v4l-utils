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
#include <QPushButton>
#include <QMenu>

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

struct frame {
	__u32 format;
	QSize size;        // int   frameHeight; int   frameWidth;
	unsigned char *planeData[3];
	bool updated;
};

struct crop {              // cropInfo
	QSize delta;       // int cropH; int cropW;
	QSize size;        // int height; int width;
	bool updated;
};

class ApplicationWindow;

class CaptureWin : public QWidget
{
	Q_OBJECT

public:
	CaptureWin(ApplicationWindow *aw);
	~CaptureWin();

	void setWindowSize(QSize size);
	void enableScaling(bool enable);
	void setPixelAspectRatio(double ratio);
	float getHorScaleFactor();
	float getVertScaleFactor();
	virtual void setColorspace(unsigned colorspace, unsigned xfer_func,
			unsigned ycbcr_enc, unsigned quantization, bool is_sdtv) = 0;
	virtual void setField(unsigned field) = 0;
	virtual void setBlending(bool enable) = 0;
	virtual void setLinearFilter(bool enable) = 0;
	void setCropMethod(CropMethod crop);
	void makeFullScreen(bool);
	QAction *m_exitFullScreen;
	QAction *m_enterFullScreen;

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
	void setFrame(int width, int height, __u32 format,
		      unsigned char *data, unsigned char *data2, unsigned char *data3);

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
	 * @brief Crop size
	 *
	 * Reduces size width or height according to m_cropMethod
	 *
	 * @param size Input size
	 * @return Cropped size
	 *
	 */
	static QSize cropSize(QSize size);

	/**
	 * @brief Get the frame size when aspect ratio is applied and increases size.
	 *
	 * @param size The original frame size.
	 * @return The new size with aspect ratio correction (scaling must be enabled).
	 */
	static QSize pixelAspectFrameSize(QSize size);

public slots:
	void resetSize();
	void customMenuRequested(QPoint pos);

private slots:
	void escape();
	void fullScreen();

protected:
	void closeEvent(QCloseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *e);

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
	 * @brief Calculate source size after pixel aspect scaling and cropping
	 *
	 */
	void updateSize();

	/**
	 * @brief Frame information.
	 *
	 * @note Set and accessed from derived render dependent classes.
	 */
	struct frame m_frame;
	struct crop  m_crop;
	QSize m_origFrameSize;  // int m_sourceWinWidth; int m_sourceWinHeight;
	QSize m_windowSize;     // int m_curWinWidth; int m_curWinHeight;
	QSize m_scaledSize;

	/**
	 * @brief Update frame information to renderer.
	 *
	 * @note Must be implemented by derived render dependent classes.
	 */
	virtual void setRenderFrame() = 0;

	/**
	 * @brief Determines if scaling is to be applied to video frame.
	 */
	static bool m_enableScaling;

signals:
	void close();

private:
	ApplicationWindow *m_appWin;
	static double m_pixelAspectRatio;
	static CropMethod m_cropMethod;
	QShortcut *m_hotkeyClose;
	QShortcut *m_hotkeyScaleReset;
	QShortcut *m_hotkeyExitFullscreen;
	QShortcut *m_hotkeyToggleFullscreen;
	QVBoxLayout *m_vboxLayout;
	unsigned m_vboxSpacing;
};
#endif
