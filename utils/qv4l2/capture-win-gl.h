/*
 * Copyright 2013 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef CAPTURE_WIN_GL_H
#define CAPTURE_WIN_GL_H

#include <config.h>

#ifdef HAVE_QTGL
#define GL_GLEXT_PROTOTYPES
#define QT_NO_OPENGL_ES_2

#include <QGLWidget>
#include <QGLShader>
#include <QGLShaderProgram>
#include <QGLFunctions>
#endif

#include "qv4l2.h"
#include "capture-win.h"

#include <QResizeEvent>

#ifdef HAVE_QTGL

// This must be equal to the max number of textures that any shader uses
#define MAX_TEXTURES_NEEDED 3

class CaptureWinGLEngine : public QGLWidget
{
public:
	CaptureWinGLEngine();
	~CaptureWinGLEngine();

	void stop();
	void setFrame(int width, int height, int cropWidth, int cropHeight,
		      __u32 format, unsigned char *data, unsigned char *data2,
		      unsigned char *data3);
	bool hasNativeFormat(__u32 format);
	void lockSize(QSize size);
	void setColorspace(unsigned colorspace, unsigned xfer_func,
			unsigned ycbcr_enc, unsigned quantization, bool is_sdtv);
	void setField(unsigned field);
	void setBlending(bool enable) { m_blending = enable; }
	void setLinearFilter(bool enable);

protected:
	void paintGL();
	void initializeGL();
	void resizeGL(int width, int height);

private:
	// Colorspace conversion shaders
	void shader_YUV(__u32 format);
	QString shader_NV12_invariant(__u32 format);
	void shader_NV12(__u32 format);
	void shader_NV16(__u32 format);
	QString shader_NV16_invariant(__u32 format);
	void shader_NV24(__u32 format);
	QString shader_NV24_invariant(__u32 format);
	void shader_RGB(__u32 format);
	void shader_Bayer(__u32 format);
	void shader_YUV_packed(__u32 format);
	void shader_YUY2(__u32 format);
	QString shader_YUY2_invariant(__u32 format);
	QString codeYUVNormalize();
	QString codeRGBNormalize();
	QString codeYUV2RGB();
	QString codeTransformToLinear();
	QString codeColorspaceConversion();
	QString codeTransformToNonLinear();

	// Colorspace conversion render
	void render_RGB(__u32 format);
	void render_Bayer(__u32 format);
	void render_YUY2(__u32 format);
	void render_YUV(__u32 format);
	void render_YUV_packed(__u32 format);
	void render_NV12(__u32 format);
	void render_NV16(__u32 format);
	void render_NV24(__u32 format);

	void clearShader();
	void changeShader();
	void paintFrame();
	void paintSquare();
	void configureTexture(size_t idx);
	void checkError(const char *msg);

	int m_frameWidth;
	int m_frameHeight;
	int m_WCrop;
	int m_HCrop;
	unsigned m_colorspace;
	unsigned m_xfer_func;
	unsigned m_ycbcr_enc;
	unsigned m_quantization;
	bool m_is_sdtv;
	bool m_is_rgb;
	unsigned m_field;
	int m_screenTextureCount;
	bool m_formatChange;
	__u32 m_frameFormat;
	GLuint m_screenTexture[MAX_TEXTURES_NEEDED];
	QGLFunctions m_glfunction;
	unsigned char *m_frameData;
	unsigned char *m_frameData2;
	unsigned char *m_frameData3;
	QGLShaderProgram m_shaderProgram;
	bool m_haveFramebufferSRGB;
	bool m_hasGLRed;
	unsigned m_glRed;
	unsigned m_glRedGreen;
	bool m_blending;
	GLint m_mag_filter;
	GLint m_min_filter;
};

#endif

class CaptureWinGL : public CaptureWin
{
public:
	CaptureWinGL(ApplicationWindow *aw);
	~CaptureWinGL();

	void stop();
	bool hasNativeFormat(__u32 format);
	static bool isSupported();
	void setColorspace(unsigned colorspace, unsigned xfer_func,
			unsigned ycbcr_enc, unsigned quantization, bool is_sdtv);
	void setField(unsigned field);
	void setBlending(bool enable);
	void setLinearFilter(bool enable);

protected:
	void resizeEvent(QResizeEvent *event);
	void setRenderFrame();

private:
#ifdef HAVE_QTGL
	CaptureWinGLEngine m_videoSurface;
#endif
};

#endif
