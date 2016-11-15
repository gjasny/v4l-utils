/*
 * The YUY2 shader code is based on face-responder. The code is under public domain:
 * https://bitbucket.org/nateharward/face-responder/src/0c3b4b957039d9f4bf1da09b9471371942de2601/yuv42201_laplace.frag?at=master
 *
 * All other OpenGL code:
 *
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

#include "capture-win-gl.h"

#include <stdio.h>

CaptureWinGL::CaptureWinGL(ApplicationWindow *aw) :
	CaptureWin(aw)
{
#ifdef HAVE_QTGL
	CaptureWin::buildWindow(&m_videoSurface);
#endif
	CaptureWin::setWindowTitle("V4L2 Capture (OpenGL)");
}

CaptureWinGL::~CaptureWinGL()
{
}

void CaptureWinGL::stop()
{
#ifdef HAVE_QTGL
	m_videoSurface.stop();
#endif
}

void CaptureWinGL::resizeEvent(QResizeEvent *event)
{
#ifdef HAVE_QTGL
	// Get size of frame viewport. Can't use size of m_videoSurface
	// since it is a subwidget of this widget.
	QSize margins = getMargins();
	m_windowSize = size() - margins;
	// Re-calculate sizes
	m_frame.updated = true;
	CaptureWin::updateSize();
	// Lock viewport size to follow calculated size
	m_videoSurface.lockSize(m_scaledSize);
#endif
	event->accept();
}

void CaptureWinGL::setRenderFrame()
{
#ifdef HAVE_QTGL
	m_videoSurface.setFrame(m_frame.size.width(), m_frame.size.height(),
				m_crop.delta.width(), m_crop.delta.height(),
				m_frame.format,
				m_frame.planeData[0],
				m_frame.planeData[1],
				m_frame.planeData[2]);
#endif
	m_frame.updated = false;
	m_crop.updated = false;
}

bool CaptureWinGL::hasNativeFormat(__u32 format)
{
#ifdef HAVE_QTGL
	return m_videoSurface.hasNativeFormat(format);
#else
	return false;
#endif
}

bool CaptureWinGL::isSupported()
{
#ifdef HAVE_QTGL
	return true;
#else
	return false;
#endif
}

void CaptureWinGL::setColorspace(unsigned colorspace, unsigned xfer_func,
		unsigned ycbcr_enc, unsigned quantization, bool is_sdtv)
{
#ifdef HAVE_QTGL
	m_videoSurface.setColorspace(colorspace, xfer_func,
			ycbcr_enc, quantization, is_sdtv);
#endif
}

void CaptureWinGL::setField(unsigned field)
{
#ifdef HAVE_QTGL
	m_videoSurface.setField(field);
#endif
}

void CaptureWinGL::setBlending(bool enable)
{
#ifdef HAVE_QTGL
	m_videoSurface.setBlending(enable);
#endif
}

void CaptureWinGL::setLinearFilter(bool enable)
{
#ifdef HAVE_QTGL
	m_videoSurface.setLinearFilter(enable);
#endif
}

#ifdef HAVE_QTGL
CaptureWinGLEngine::CaptureWinGLEngine() :
	m_frameWidth(0),
	m_frameHeight(0),
	m_WCrop(0),
	m_HCrop(0),
	m_colorspace(V4L2_COLORSPACE_REC709),
	m_xfer_func(V4L2_XFER_FUNC_DEFAULT),
	m_ycbcr_enc(V4L2_YCBCR_ENC_DEFAULT),
	m_quantization(V4L2_QUANTIZATION_DEFAULT),
	m_is_sdtv(false),
	m_is_rgb(false),
	m_field(V4L2_FIELD_NONE),
	m_screenTextureCount(0),
	m_formatChange(false),
	m_frameFormat(0),
	m_frameData(NULL),
	m_blending(false),
	m_mag_filter(GL_NEAREST),
	m_min_filter(GL_NEAREST)
{
	makeCurrent();
	m_glfunction.initializeGLFunctions(context());
}

CaptureWinGLEngine::~CaptureWinGLEngine()
{
	clearShader();
}

void CaptureWinGLEngine::setColorspace(unsigned colorspace, unsigned xfer_func,
		unsigned ycbcr_enc, unsigned quantization, bool is_sdtv)
{
	bool is_rgb = true;

	switch (m_frameFormat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
		is_rgb = false;
		break;
	}

	switch (colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_SMPTE240M:
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
	case V4L2_COLORSPACE_SRGB:
	case V4L2_COLORSPACE_ADOBERGB:
	case V4L2_COLORSPACE_BT2020:
	case V4L2_COLORSPACE_DCI_P3:
		break;
	default:
		// If the colorspace was not specified, then guess
		// based on the pixel format.
		if (is_rgb)
			colorspace = V4L2_COLORSPACE_SRGB;
		else if (is_sdtv)
			colorspace = V4L2_COLORSPACE_SMPTE170M;
		else
			colorspace = V4L2_COLORSPACE_REC709;
		break;
	}
	if (m_colorspace == colorspace && m_xfer_func == xfer_func &&
	    m_ycbcr_enc == ycbcr_enc && m_quantization == quantization &&
	    m_is_sdtv == is_sdtv && m_is_rgb == is_rgb)
		return;
	m_colorspace = colorspace;
	if (xfer_func == V4L2_XFER_FUNC_DEFAULT)
		xfer_func = V4L2_MAP_XFER_FUNC_DEFAULT(colorspace);
	if (ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT)
		ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(colorspace);
	if (quantization == V4L2_QUANTIZATION_DEFAULT)
		quantization = V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb, colorspace, ycbcr_enc);
	m_xfer_func = xfer_func;
	m_ycbcr_enc = ycbcr_enc;
	m_quantization = quantization;
	m_is_sdtv = is_sdtv;
	m_is_rgb = is_rgb;
	m_formatChange = true;
}

void CaptureWinGLEngine::setField(unsigned field)
{
	if (m_field == field)
		return;
	m_field = field;
	m_formatChange = true;
}

void CaptureWinGLEngine::setLinearFilter(bool enable)
{
	if (enable) {
		m_mag_filter = GL_LINEAR;
		m_min_filter = GL_LINEAR;
	}
	else {
		m_mag_filter = GL_NEAREST;
		m_min_filter = GL_NEAREST;
	}
	m_formatChange = true;
}

void CaptureWinGLEngine::clearShader()
{
	glDeleteTextures(m_screenTextureCount, m_screenTexture);
	if (m_shaderProgram.isLinked()) {
		m_shaderProgram.release();
		m_shaderProgram.removeAllShaders();
	}
}

void CaptureWinGLEngine::stop()
{
	// Setting the m_frameData to NULL stops OpenGL
	// from updating frames on repaint
	m_frameData = NULL;
	m_frameData2 = NULL;
}

void CaptureWinGLEngine::initializeGL()
{
	glShadeModel(GL_FLAT);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	// Check if the the GL_FRAMEBUFFER_SRGB feature is available.
	// If it is, then the GPU can perform the SRGB transfer function
	// for us.
	GLint res = 0;
	glGetIntegerv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &res);
	m_haveFramebufferSRGB = res;
	if (m_haveFramebufferSRGB)
		glEnable(GL_FRAMEBUFFER_SRGB);
	m_hasGLRed = glGetString(GL_VERSION)[0] >= '3';
	m_glRed = m_hasGLRed ? GL_RED : GL_LUMINANCE;
	m_glRedGreen = m_hasGLRed ? GL_RG : GL_LUMINANCE_ALPHA;

	glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
	glBlendFunc(GL_ONE, GL_ZERO);
	checkError("InitializeGL");
}

void CaptureWinGLEngine::lockSize(QSize size)
{
	if ((size.width() > 0) && (size.height() > 0)) {
		setFixedSize(size);
	}
}

void CaptureWinGLEngine::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);
}

void CaptureWinGLEngine::setFrame(int width, int height, int WCrop, int HCrop,
				  __u32 format, unsigned char *data, unsigned char *data2,
				  unsigned char *data3)
{
	if (format != m_frameFormat || width != m_frameWidth || height != m_frameHeight
	    || WCrop != m_WCrop || HCrop != m_HCrop) {
		m_formatChange = true;
		m_frameWidth = width;
		m_frameHeight = height;
		m_WCrop = WCrop;
		m_HCrop = HCrop;
		m_frameFormat = format;
	}

	m_frameData = data;
	m_frameData2 = data2 ? data2 : data;
	m_frameData3 = data3 ? data3 : data;
	updateGL();
}

void CaptureWinGLEngine::checkError(const char *msg)
{
	int err = glGetError();
	if (err) fprintf(stderr, "OpenGL Error 0x%x: %s.\n", err, msg);
}

bool CaptureWinGLEngine::hasNativeFormat(__u32 format)
{
	static const __u32 supported_fmts[] = {
		V4L2_PIX_FMT_RGB32,
		V4L2_PIX_FMT_XRGB32,
		V4L2_PIX_FMT_ARGB32,
		V4L2_PIX_FMT_BGR32,
		V4L2_PIX_FMT_XBGR32,
		V4L2_PIX_FMT_ABGR32,
		V4L2_PIX_FMT_RGB24,
		V4L2_PIX_FMT_BGR24,
		V4L2_PIX_FMT_RGB565,
		V4L2_PIX_FMT_RGB565X,
		V4L2_PIX_FMT_RGB444,
		V4L2_PIX_FMT_XRGB444,
		V4L2_PIX_FMT_ARGB444,
		V4L2_PIX_FMT_RGB555,
		V4L2_PIX_FMT_XRGB555,
		V4L2_PIX_FMT_ARGB555,
		V4L2_PIX_FMT_RGB555X,
		V4L2_PIX_FMT_XRGB555X,
		V4L2_PIX_FMT_ARGB555X,
		V4L2_PIX_FMT_RGB332,
		V4L2_PIX_FMT_BGR666,
		V4L2_PIX_FMT_SBGGR8,
		V4L2_PIX_FMT_SGBRG8,
		V4L2_PIX_FMT_SGRBG8,
		V4L2_PIX_FMT_SRGGB8,
		V4L2_PIX_FMT_SBGGR10,
		V4L2_PIX_FMT_SGBRG10,
		V4L2_PIX_FMT_SGRBG10,
		V4L2_PIX_FMT_SRGGB10,
		V4L2_PIX_FMT_SBGGR12,
		V4L2_PIX_FMT_SGBRG12,
		V4L2_PIX_FMT_SGRBG12,
		V4L2_PIX_FMT_SRGGB12,
		V4L2_PIX_FMT_YUYV,
		V4L2_PIX_FMT_YVYU,
		V4L2_PIX_FMT_UYVY,
		V4L2_PIX_FMT_VYUY,
		V4L2_PIX_FMT_YUV422M,
		V4L2_PIX_FMT_YVU422M,
		V4L2_PIX_FMT_YUV422P,
		V4L2_PIX_FMT_YVU420,
		V4L2_PIX_FMT_YUV420,
		V4L2_PIX_FMT_NV12,
		V4L2_PIX_FMT_NV21,
		V4L2_PIX_FMT_NV16,
		V4L2_PIX_FMT_NV61,
		V4L2_PIX_FMT_NV24,
		V4L2_PIX_FMT_NV42,
		V4L2_PIX_FMT_YUV444M,
		V4L2_PIX_FMT_YVU444M,
		V4L2_PIX_FMT_NV16M,
		V4L2_PIX_FMT_NV61M,
		V4L2_PIX_FMT_YVU420M,
		V4L2_PIX_FMT_YUV420M,
		V4L2_PIX_FMT_NV12M,
		V4L2_PIX_FMT_NV21M,
		V4L2_PIX_FMT_YUV444,
		V4L2_PIX_FMT_YUV555,
		V4L2_PIX_FMT_YUV565,
		V4L2_PIX_FMT_YUV32,
		V4L2_PIX_FMT_GREY,
		V4L2_PIX_FMT_Y16,
		V4L2_PIX_FMT_Y16_BE,
		V4L2_PIX_FMT_HSV24,
		V4L2_PIX_FMT_HSV32,
		0
	};

	if (!m_glfunction.hasOpenGLFeature(QGLFunctions::Shaders))
		return false;

	for (int i = 0; supported_fmts[i]; i++)
		if (supported_fmts[i] == format)
			return true;

	return false;
}

void CaptureWinGLEngine::changeShader()
{
	m_formatChange = false;
	clearShader();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, m_frameWidth, m_frameHeight, 0, 0, 1);
	resizeGL(QGLWidget::width(), QGLWidget::height());
	checkError("Render settings.\n");

	switch (m_frameFormat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		shader_YUY2(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		shader_NV16(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		shader_NV12(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		shader_NV24(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
		shader_YUV_packed(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		shader_YUV(m_frameFormat);
		break;

	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		shader_Bayer(m_frameFormat);
		break;

	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		shader_RGB(m_frameFormat);
		break;
	}
}

void CaptureWinGLEngine::paintFrame()
{
	float HCrop_f = (float)m_HCrop / m_frameHeight;
	float WCrop_f = (float)m_WCrop / m_frameWidth;

	glBegin(GL_QUADS);
	glTexCoord2f(WCrop_f, HCrop_f);               glVertex2f(0, 0);
	glTexCoord2f(1.0f - WCrop_f, HCrop_f);        glVertex2f(m_frameWidth, 0);
	glTexCoord2f(1.0f - WCrop_f, 1.0f - HCrop_f); glVertex2f(m_frameWidth, m_frameHeight);
	glTexCoord2f(WCrop_f, 1.0f - HCrop_f);        glVertex2f(0, m_frameHeight);
	glEnd();
}

void CaptureWinGLEngine::paintSquare()
{
	// Draw a black square on the white background to
	// test the alpha channel.
	unsigned w4 = m_frameWidth / 4;
	unsigned h4 = m_frameHeight / 4;

	glClear(GL_COLOR_BUFFER_BIT);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBegin(GL_QUADS);
	glColor4f(0.0f, 0.0f, 0.0f, 1.0f);
	glVertex2f(w4, h4);
	glVertex2f(w4, 3 * h4);
	glVertex2f(3 * w4, 3 * h4);
	glVertex2f(3 * w4, h4);
	glEnd();
}

void CaptureWinGLEngine::paintGL()
{
	if (m_frameWidth < 1 || m_frameHeight < 1) {
		return;
	}

	if (m_formatChange)
		changeShader();

	if (m_frameData == NULL) {
		paintFrame();
		return;
	}

	if (m_blending) {
		paintSquare();
		glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
	}

	switch (m_frameFormat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		render_YUY2(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		render_NV16(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		render_NV12(m_frameFormat);
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		render_NV24(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		render_YUV(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
		render_YUV_packed(m_frameFormat);
		break;

	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		render_Bayer(m_frameFormat);
		break;

	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		render_RGB(m_frameFormat);
		break;
	}
	paintFrame();

	if (m_blending)
		glBlendFunc(GL_ONE, GL_ZERO);
}

void CaptureWinGLEngine::configureTexture(size_t idx)
{
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[idx]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_min_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_mag_filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

// Normalize y to [0...1] and uv to [-0.5...0.5], taking into account the
// colorspace.
QString CaptureWinGLEngine::codeYUVNormalize()
{
	switch (m_quantization) {
	case V4L2_QUANTIZATION_FULL_RANGE:
		if (m_ycbcr_enc != V4L2_YCBCR_ENC_XV601 &&
		    m_ycbcr_enc != V4L2_YCBCR_ENC_XV709)
			return "";
		/*
		 * xv709 and xv601 have full range quantization, but they still
		 * need to be normalized as if they were limited range. But the
		 * result are values outside the normal 0-1 range, which is the
		 * point of these extended gamut encodings.
		 */

		/* fall-through */
	default:
		return QString("   y = (255.0 / 219.0) * (y - (16.0 / 255.0));"
			       "   u = (255.0 / 224.0) * u;"
			       "   v = (255.0 / 224.0) * v;"
			       );
	}
}

// Normalize r, g and b to [0...1]
QString CaptureWinGLEngine::codeRGBNormalize()
{
	switch (m_quantization) {
	case V4L2_QUANTIZATION_FULL_RANGE:
		return "";
	default:
		return QString("   r = (255.0 / 219.0) * (r - (16.0 / 255.0));"
			       "   g = (255.0 / 219.0) * (g - (16.0 / 255.0));"
			       "   b = (255.0 / 219.0) * (b - (16.0 / 255.0));"
			       );
	}
}

// Convert Y'CbCr (aka YUV) to R'G'B', taking into account the
// colorspace.
QString CaptureWinGLEngine::codeYUV2RGB()
{
	switch (m_ycbcr_enc) {
	case V4L2_YCBCR_ENC_SMPTE240M:
		// Old obsolete HDTV standard. Replaced by REC 709.
		// SMPTE 240M has its own luma coefficients
		return QString("   float r = y + 1.5756 * v;"
			       "   float g = y - 0.2253 * u - 0.4768 * v;"
			       "   float b = y + 1.8270 * u;"
			       );
	case V4L2_YCBCR_ENC_BT2020:
		// BT.2020 luma coefficients
		return QString("   float r = y + 1.4719 * v;"
			       "   float g = y - 0.1646 * u - 0.5703 * v;"
			       "   float b = y + 1.8814 * u;"
			       );
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		// BT.2020_CONST_LUM luma coefficients
		return QString("   float b = u <= 0.0 ? y + 1.9404 * u : y + 1.5816 * u;"
			       "   float r = v <= 0.0 ? y + 1.7184 * v : y + 0.9936 * v;"
			       "   float lin_r = (r < 0.081) ? r / 4.5 : pow((r + 0.099) / 1.099, 1.0 / 0.45);"
			       "   float lin_b = (b < 0.081) ? b / 4.5 : pow((b + 0.099) / 1.099, 1.0 / 0.45);"
			       "   float lin_y = (y < 0.081) ? y / 4.5 : pow((y + 0.099) / 1.099, 1.0 / 0.45);"
			       "   float lin_g = lin_y / 0.6780 - lin_r * 0.2627 / 0.6780 - lin_b * 0.0593 / 0.6780;"
			       "   float g = (lin_g < 0.018) ? lin_g * 4.5 : 1.099 * pow(lin_g, 0.45) - 0.099;"
			       );
	case V4L2_YCBCR_ENC_601:
	case V4L2_YCBCR_ENC_XV601:
		// These colorspaces all use the BT.601 luma coefficients
		return QString("   float r = y + 1.403 * v;"
		       "   float g = y - 0.344 * u - 0.714 * v;"
		       "   float b = y + 1.773 * u;"
		       );
	default:
		// The HDTV colorspaces all use REC 709 luma coefficients
		return QString("   float r = y + 1.5701 * v;"
			       "   float g = y - 0.1870 * u - 0.4664 * v;"
			       "   float b = y + 1.8556 * u;"
			       );
	}
}

// Convert non-linear R'G'B' to linear RGB, taking into account the
// colorspace.
QString CaptureWinGLEngine::codeTransformToLinear()
{
	switch (m_xfer_func) {
	case V4L2_XFER_FUNC_SMPTE240M:
		// Old obsolete HDTV standard. Replaced by REC 709.
		// This is the transfer function for SMPTE 240M
		return QString("   r = (r < 0.0913) ? r / 4.0 : pow((r + 0.1115) / 1.1115, 1.0 / 0.45);"
			       "   g = (g < 0.0913) ? g / 4.0 : pow((g + 0.1115) / 1.1115, 1.0 / 0.45);"
			       "   b = (b < 0.0913) ? b / 4.0 : pow((b + 0.1115) / 1.1115, 1.0 / 0.45);"
			       );
	case V4L2_XFER_FUNC_SRGB:
		// This is used for sRGB as specified by the IEC FDIS 61966-2-1 standard
		return QString("   r = (r < -0.04045) ? -pow((-r + 0.055) / 1.055, 2.4) : "
			       "        ((r <= 0.04045) ? r / 12.92 : pow((r + 0.055) / 1.055, 2.4));"
			       "   g = (g < -0.04045) ? -pow((-g + 0.055) / 1.055, 2.4) : "
			       "        ((g <= 0.04045) ? g / 12.92 : pow((g + 0.055) / 1.055, 2.4));"
			       "   b = (b < -0.04045) ? -pow((-b + 0.055) / 1.055, 2.4) : "
			       "        ((b <= 0.04045) ? b / 12.92 : pow((b + 0.055) / 1.055, 2.4));"
			       );
	case V4L2_XFER_FUNC_ADOBERGB:
		return QString("   r = pow(max(r, 0.0), 2.19921875);"
			       "   g = pow(max(g, 0.0), 2.19921875);"
			       "   b = pow(max(b, 0.0), 2.19921875);");
	case V4L2_XFER_FUNC_DCI_P3:
		return QString("   r = pow(max(r, 0.0), 2.6);"
			       "   g = pow(max(g, 0.0), 2.6);"
			       "   b = pow(max(b, 0.0), 2.6);");
	case V4L2_XFER_FUNC_SMPTE2084:
		return QString("   float m1 = 1.0 / ((2610.0 / 4096.0) / 4.0);"
			       "   float m2 = 1.0 / (128.0 * 2523.0 / 4096.0);"
			       "   float c1 = 3424.0 / 4096.0;"
			       "   float c2 = 32.0 * 2413.0 / 4096.0;"
			       "   float c3 = 32.0 * 2392.0 / 4096.0;"
			       "   r = pow(max(r, 0.0), m2);"
			       "   g = pow(max(g, 0.0), m2);"
			       "   b = pow(max(b, 0.0), m2);"
			       "   r = pow(max(r - c1, 0.0) / (c2 - c3 * r), m1);"
			       "   g = pow(max(g - c1, 0.0) / (c2 - c3 * g), m1);"
			       "   b = pow(max(b - c1, 0.0) / (c2 - c3 * b), m1);");
	case V4L2_XFER_FUNC_NONE:
		return "";
	case V4L2_XFER_FUNC_709:
	default:
		// All others use the transfer function specified by REC 709
		return QString("   r = (r <= -0.081) ? -pow((r - 0.099) / -1.099, 1.0 / 0.45) : "
			       "        ((r < 0.081) ? r / 4.5 : pow((r + 0.099) / 1.099, 1.0 / 0.45));"
			       "   g = (g <= -0.081) ? -pow((g - 0.099) / -1.099, 1.0 / 0.45) : "
			       "        ((g < 0.081) ? g / 4.5 : pow((g + 0.099) / 1.099, 1.0 / 0.45));"
			       "   b = (b <= -0.081) ? -pow((b - 0.099) / -1.099, 1.0 / 0.45) : "
			       "        ((b < 0.081) ? b / 4.5 : pow((b + 0.099) / 1.099, 1.0 / 0.45));"
			       );
	}
}

// Convert the given colorspace to the REC 709/sRGB colorspace. All colors are
// specified as linear RGB.
QString CaptureWinGLEngine::codeColorspaceConversion()
{
	switch (m_colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_SMPTE240M:
		// Current SDTV standard, although slowly being replaced by REC 709.
		// Uses the SMPTE 170M aka SMPTE-C aka SMPTE RP 145 conversion matrix.
		return QString("   float rr =  0.939536 * r + 0.050215 * g + 0.001789 * b;"
			       "   float gg =  0.017743 * r + 0.965758 * g + 0.016243 * b;"
			       "   float bb = -0.001591 * r - 0.004356 * g + 1.005951 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_470_SYSTEM_M:
		// Old obsolete NTSC standard. Replaced by REC 709.
		// Uses the NTSC 1953 conversion matrix and the Bradford method to
		// compensate for the different whitepoints.
		return QString("   float rr =  1.4858417 * r - 0.4033361 * g - 0.0825056 * b;"
			       "   float gg = -0.0251179 * r + 0.9541568 * g + 0.0709611 * b;"
			       "   float bb = -0.0272254 * r - 0.0440815 * g + 1.0713068 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		// Old obsolete PAL/SECAM standard. Replaced by REC 709.
		// Uses the EBU Tech. 3213 conversion matrix.
		return QString("   float rr = 1.0440 * r - 0.0440 * g;"
			       "   float bb = -0.0119 * g + 1.0119 * b;"
			       "   r = rr; b = bb;"
			       );
	case V4L2_COLORSPACE_ADOBERGB:
		return QString("   float rr =  1.3982832 * r - 0.3982831 * g;"
			       "   float bb = -0.0429383 * g + 1.0429383 * b;"
			       "   r = rr; b = bb;"
			       );
	case V4L2_COLORSPACE_DCI_P3:
		// Uses the Bradford method to compensate for the different whitepoints.
		return QString("   float rr =  1.1574000 * r - 0.1548597 * g - 0.0025403 * b;"
			       "   float gg = -0.0415052 * r + 1.0455684 * g - 0.0040633 * b;"
			       "   float bb = -0.0180562 * r - 0.0785993 * g + 1.0966555 * b;"
			       );
	case V4L2_COLORSPACE_BT2020:
		return QString("   float rr =  1.6603627 * r - 0.5875400 * g - 0.0728227 * b;"
			       "   float gg = -0.1245635 * r + 1.1329114 * g - 0.0083478 * b;"
			       "   float bb = -0.0181566 * r - 0.1006017 * g + 1.1187583 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_SRGB:
	default:
		return "";
	}
}

// Convert linear RGB to non-linear R'G'B', taking into account the
// given display colorspace.
QString CaptureWinGLEngine::codeTransformToNonLinear()
{
	// Use the sRGB transfer function. Do nothing if the GL_FRAMEBUFFER_SRGB
	// is available.
	if (m_haveFramebufferSRGB)
		return "";
	return QString("   r = (r < -0.0031308) ? -1.055 * pow(-r, 1.0 / 2.4) + 0.055 : "
		       "        ((r <= 0.0031308) ? r * 12.92 : 1.055 * pow(r, 1.0 / 2.4) - 0.055);"
		       "   g = (g < -0.0031308) ? -1.055 * pow(-g, 1.0 / 2.4) + 0.055 : "
		       "        ((g <= 0.0031308) ? g * 12.92 : 1.055 * pow(g, 1.0 / 2.4) - 0.055);"
		       "   b = (b < -0.0031308) ? -1.055 * pow(-b, 1.0 / 2.4) + 0.055 : "
		       "        ((b <= 0.0031308) ? b * 12.92 : 1.055 * pow(b, 1.0 / 2.4) - 0.055);"
		       );
}

static const QString codeSuffix("   gl_FragColor = vec4(r, g, b, 0.0);"
			  "}");

static const QString codeSuffixWithAlpha("   gl_FragColor = vec4(r, g, b, a);"
			  "}");

void CaptureWinGLEngine::shader_YUV(__u32 format)
{
	unsigned vdiv = 2, hdiv = 2;

	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		vdiv = hdiv = 1;
		break;
	}

	m_screenTextureCount = 3;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth / hdiv, m_frameHeight / vdiv, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 1");

	glActiveTexture(GL_TEXTURE2);
	configureTexture(2);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth / hdiv, m_frameHeight / vdiv, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 2");

	QString codeHead = QString("uniform sampler2D ytex;"
				   "uniform sampler2D utex;"
				   "uniform sampler2D vtex;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead += "   float y = texture2D(ytex, xy).r;"
		    "   float u = texture2D(utex, xy).r - 0.5;"
		    "   float v = texture2D(vtex, xy).r - 0.5;";

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_c = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, codeHead + codeTail);

	if (!src_c)
		fprintf(stderr, "OpenGL Error: YUV shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_YUV(__u32 format)
{
	unsigned vdiv = 2, hdiv = 2;
	int idxU = 0;
	int idxV = 0;

	switch (format) {
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
		vdiv = hdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV422P:
		idxU = m_frameWidth * m_frameHeight;
		idxV = idxU + (idxU / 2);
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV420:
		idxU = m_frameWidth * m_frameHeight;
		idxV = idxU + (idxU / 4);
		break;
	case V4L2_PIX_FMT_YVU420:
		idxV = m_frameWidth * m_frameHeight;
		idxU = idxV + (idxV / 4);
		break;
	}

	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "ytex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData);
	checkError("YUV paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint U = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "utex");
	glUniform1i(U, 1);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData == NULL ? NULL : &m_frameData[idxU]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData2);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData3);
		break;
	}
	checkError("YUV paint utex");

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[2]);
	GLint V = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "vtex");
	glUniform1i(V, 2);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData == NULL ? NULL : &m_frameData[idxV]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData3);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / hdiv, m_frameHeight / vdiv,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData2);
		break;
	}
	checkError("YUV paint vtex");
}

QString CaptureWinGLEngine::shader_NV12_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV12M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(xy.x + texl_w, xy.y)).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, vec2(xy.x - texl_w, xy.y)).r - 0.5;"
			       "   v = texture2D(uvtex, xy).r - 0.5;"
			       "}"
			       );

	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV21M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, vec2(xy.x + texl_w, xy.y)).r - 0.5;"
			       "   v = texture2D(uvtex, xy).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(xy.x - texl_w, xy.y)).r - 0.5;"
			       "}"
			       );

	default:
		return QString();
	}
}


void CaptureWinGLEngine::shader_NV12(__u32 format)
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight / 2, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 1");

	QString codeHead = QString("uniform sampler2D ytex;"
				   "uniform sampler2D uvtex;"
				   "uniform float texl_w;"
				   "uniform float tex_w;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead += "   float u, v;"
		    "   float xcoord = floor(xy.x * tex_w);"
		    "   float y = texture2D(ytex, xy).r;";

	QString codeBody = shader_NV12_invariant(format);

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_c = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment,
				QString("%1%2%3").arg(codeHead, codeBody, codeTail));

	if (!src_c)
		fprintf(stderr, "OpenGL Error: YUV shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_NV12(__u32 format)
{
	int idx;

	idx = glGetUniformLocation(m_shaderProgram.programId(), "texl_w"); // Texel width
	glUniform1f(idx, 1.0 / m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_w"); // Texture width
	glUniform1f(idx, m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "ytex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData);
	checkError("NV12 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint U = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "uvtex");
	glUniform1i(U, 1);
	switch (format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight / 2,
				m_glRed, GL_UNSIGNED_BYTE,
				m_frameData ? m_frameData + m_frameWidth * m_frameHeight : NULL);
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight / 2,
				m_glRed, GL_UNSIGNED_BYTE, m_frameData2);
		break;
	}
	checkError("NV12 paint uvtex");
}

QString CaptureWinGLEngine::shader_NV24_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_NV24:
		if (m_hasGLRed)
			return QString("   u = texture2D(uvtex, xy).r - 0.5;"
				       "   v = texture2D(uvtex, xy).g - 0.5;"
				       );
		return QString("   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, xy).a - 0.5;"
			       );

	case V4L2_PIX_FMT_NV42:
		if (m_hasGLRed)
			return QString("   v = texture2D(uvtex, xy).r - 0.5;"
				       "   u = texture2D(uvtex, xy).g - 0.5;"
				       );
		return QString("   v = texture2D(uvtex, xy).r - 0.5;"
			       "   u = texture2D(uvtex, xy).a - 0.5;"
			       );

	default:
		return QString();
	}
}

void CaptureWinGLEngine::shader_NV24(__u32 format)
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRedGreen, m_frameWidth, m_frameHeight, 0,
		     m_glRedGreen, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 1");

	QString codeHead = QString("uniform sampler2D ytex;"
				   "uniform sampler2D uvtex;"
				   "uniform float tex_w;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead += "   float u, v;"
		    "   float y = texture2D(ytex, xy).r;";

	QString codeBody = shader_NV24_invariant(format);

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_c = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment,
				QString("%1%2%3").arg(codeHead, codeBody, codeTail));

	if (!src_c)
		fprintf(stderr, "OpenGL Error: YUV shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_NV24(__u32 format)
{
	int idx;

	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_w"); // Texture width
	glUniform1f(idx, m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "ytex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData);
	checkError("NV24 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint U = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "uvtex");
	glUniform1i(U, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			m_glRedGreen, GL_UNSIGNED_BYTE,
			m_frameData ? m_frameData + m_frameWidth * m_frameHeight : NULL);
	checkError("NV24 paint uvtex");
}

QString CaptureWinGLEngine::shader_NV16_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV16M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(xy.x + texl_w, xy.y)).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, vec2(xy.x - texl_w, xy.y)).r - 0.5;"
			       "   v = texture2D(uvtex, xy).r - 0.5;"
			       "}"
			       );

	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV61M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, vec2(xy.x + texl_w, xy.y)).r - 0.5;"
			       "   v = texture2D(uvtex, xy).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(xy.x - texl_w, xy.y)).r - 0.5;"
			       "}"
			       );

	default:
		return QString();
	}
}

void CaptureWinGLEngine::shader_NV16(__u32 format)
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
		     m_glRed, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 1");

	QString codeHead = QString("uniform sampler2D ytex;"
				   "uniform sampler2D uvtex;"
				   "uniform float texl_w;"
				   "uniform float tex_w;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead += "   float u, v;"
		    "   float xcoord = floor(xy.x * tex_w);"
		    "   float y = texture2D(ytex, xy).r;";

	QString codeBody = shader_NV16_invariant(format);

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2%3").arg(codeHead, codeBody, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: NV16 shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_NV16(__u32 format)
{
	int idx;
	idx = glGetUniformLocation(m_shaderProgram.programId(), "texl_w"); // Texel width
	glUniform1f(idx, 1.0 / m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_w"); // Texture width
	glUniform1f(idx, m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "ytex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			m_glRed, GL_UNSIGNED_BYTE, m_frameData);
	checkError("NV16 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint UV = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "uvtex");
	glUniform1i(UV, 1);
	switch (format) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_BYTE,
				m_frameData ? m_frameData + m_frameWidth * m_frameHeight : NULL);
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_BYTE, m_frameData2);
		break;
	}
	checkError("NV16 paint");
}

QString CaptureWinGLEngine::shader_YUY2_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_YUYV:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = luma_chroma.r;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = luma_chroma.b;"
			       "}"
			       "u = luma_chroma.g - 0.5;"
			       "v = luma_chroma.a - 0.5;"
			       );

	case V4L2_PIX_FMT_YVYU:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = luma_chroma.r;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = luma_chroma.b;"
			       "}"
			       "u = luma_chroma.a - 0.5;"
			       "v = luma_chroma.g - 0.5;"
			       );

	case V4L2_PIX_FMT_UYVY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = luma_chroma.g;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = luma_chroma.a;"
			       "}"
			       "u = luma_chroma.r - 0.5;"
			       "v = luma_chroma.b - 0.5;"
			       );

	case V4L2_PIX_FMT_VYUY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = luma_chroma.g;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = luma_chroma.a;"
			       "}"
			       "u = luma_chroma.b - 0.5;"
			       "v = luma_chroma.r - 0.5;"
			       );

	default:
		return QString();
	}
}

void CaptureWinGLEngine::shader_YUY2(__u32 format)
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_frameWidth / 2, m_frameHeight, 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	checkError("YUY2 shader");

	QString codeHead = QString("uniform sampler2D tex;"
				   "uniform float texl_w;"
				   "uniform float tex_w;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   float y, u, v;"
				   "   vec4 luma_chroma;"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float xcoord = floor(xy.x * tex_w);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	QString codeBody = shader_YUY2_invariant(format);

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2%3").arg(codeHead, codeBody, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: YUY2 shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_YUY2(__u32 format)
{
	int idx;
	idx = glGetUniformLocation(m_shaderProgram.programId(), "texl_w"); // Texel width
	glUniform1f(idx, 1.0 / m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_w"); // Texture width
	glUniform1f(idx, m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / 2, m_frameHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, m_frameData);
	checkError("YUY2 paint");
}

void CaptureWinGLEngine::shader_RGB(__u32 format)
{
	bool manualTransform;
	bool hasAlpha = false;

	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	manualTransform = m_quantization == V4L2_QUANTIZATION_LIM_RANGE ||
                          m_xfer_func != V4L2_XFER_FUNC_SRGB ||
			  format == V4L2_PIX_FMT_BGR666 ||
			  format == V4L2_PIX_FMT_GREY ||
			  format == V4L2_PIX_FMT_Y16 ||
			  format == V4L2_PIX_FMT_Y16_BE ||
			  format == V4L2_PIX_FMT_HSV24 ||
			  format == V4L2_PIX_FMT_HSV32;
	GLint internalFmt = manualTransform ? GL_RGBA8 : GL_SRGB8_ALPHA8;

	switch (format) {
	case V4L2_PIX_FMT_ARGB555:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_ARGB444:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, NULL);
		break;

	case V4L2_PIX_FMT_ARGB555X:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_BGR666:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;

	case V4L2_PIX_FMT_RGB332:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_RGB, GL_UNSIGNED_BYTE_3_3_2, NULL);
		break;

	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_ARGB32:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_HSV32:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;
	case V4L2_PIX_FMT_ABGR32:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;
	case V4L2_PIX_FMT_GREY:
		glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
			     m_glRed, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
		glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
			     m_glRed, GL_UNSIGNED_SHORT, NULL);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
				GL_RGB, GL_UNSIGNED_BYTE, NULL);
		break;
	}

	checkError("RGB shader");

	QString codeHead = QString("uniform sampler2D tex;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead +=		   "   vec4 color = texture2D(tex, xy);"
				   "   float a = color.a;";

	switch (format) {
	case V4L2_PIX_FMT_BGR666:
		codeHead += "   float ub = floor(color.b * 255.0);"
			    "   float ug = floor(color.g * 255.0);"
			    "   float ur = floor(color.r * 255.0);"
			    "   ur = floor(ur / 64.0) + mod(ug, 16.0) * 4.0;"
			    "   ug = floor(ug / 16.0) + mod(ub, 4.0) * 16.0;"
			    "   ub = floor(ub / 4.0);"
			    "   float b = ub / 63.0;"
			    "   float g = ug / 63.0;"
			    "   float r = ur / 63.0;";
		break;
	case V4L2_PIX_FMT_BGR24:
		codeHead += "   float r = color.b;"
			    "   float g = color.g;"
			    "   float b = color.r;";
		break;
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
		codeHead += "   float r = color.r;"
			    "   float g = r;"
			    "   float b = r;";
		break;
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
		/* From http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl */
		if (m_ycbcr_enc == V4L2_HSV_ENC_256)
			codeHead += "   float hue = color.r;";
		else
			codeHead += "   float hue = (color.r * 256.0) / 180.0;";

		codeHead += "   vec3 c = vec3(hue, color.g, color.b);"
			    "   vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);"
			    "   vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);"
			    "   vec3 ret = c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);"
			    "   float r = ret.x;"
			    "   float g = ret.y;"
			    "   float b = ret.z;";
		break;
	default:
		codeHead += "   float r = color.r;"
			    "   float g = color.g;"
			    "   float b = color.b;";
		break;
	}

	QString codeTail;
	
	if (m_quantization == V4L2_QUANTIZATION_LIM_RANGE)
		codeTail += codeRGBNormalize();
	if (manualTransform)
		codeTail += codeTransformToLinear();

	codeTail += codeColorspaceConversion() + 
		    codeTransformToNonLinear() +
		    (hasAlpha ? codeSuffixWithAlpha : codeSuffix);

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2").arg(codeHead, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: RGB shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_RGB(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	switch (format) {
	case V4L2_PIX_FMT_RGB332:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_BYTE_3_3_2, m_frameData);
		break;
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_frameData);
		break;

	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, m_frameData);
		break;

	case V4L2_PIX_FMT_GREY:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_BYTE, m_frameData);
		break;

	case V4L2_PIX_FMT_Y16:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_SHORT, m_frameData);
		break;
	case V4L2_PIX_FMT_Y16_BE:
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_SHORT, m_frameData);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB555 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_frameData);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB565:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_frameData);
		break;

	case V4L2_PIX_FMT_RGB565X:
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB565 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_frameData);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_HSV32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, m_frameData);
		break;
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_frameData);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_BYTE, m_frameData);
		break;
	}
	checkError("RGB paint");
}

void CaptureWinGLEngine::shader_Bayer(__u32 format)
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
			     m_glRed, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		glTexImage2D(GL_TEXTURE_2D, 0, m_glRed, m_frameWidth, m_frameHeight, 0,
			     m_glRed, GL_UNSIGNED_SHORT, NULL);
		break;
	}

	checkError("Bayer shader");

	QString codeHead = QString("uniform sampler2D tex;"
				   "uniform float tex_h;"
				   "uniform float tex_w;"
				   "uniform float texl_h;"
				   "uniform float texl_w;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float xcoord = floor(xy.x * tex_w);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead +=		   "   ycoord = floor(xy.y * tex_h);"
				   "   float r, g, b;"
				   "   vec2 cell;"
				   "   cell.x = (mod(xcoord, 2.0) == 0.0) ? xy.x : xy.x - texl_w;"
				   "   cell.y = (mod(ycoord, 2.0) == 0.0) ? xy.y : xy.y - texl_h;";

	/* Poor quality Bayer to RGB conversion, but good enough for now */
	switch (format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SBGGR12:
		codeHead +=	   "   r = texture2D(tex, vec2(cell.x + texl_w, cell.y + texl_h)).r;"
				   "   g = texture2D(tex, vec2((cell.y == xy.y) ? cell.x + texl_w : cell.x, xy.y)).r;"
				   "   b = texture2D(tex, cell).r;";
		break;
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGBRG12:
		codeHead +=	   "   r = texture2D(tex, vec2(cell.x, cell.y + texl_h)).r;"
				   "   g = texture2D(tex, vec2((cell.y == xy.y) ? cell.x : cell.x + texl_w, xy.y)).r;"
				   "   b = texture2D(tex, vec2(cell.x + texl_w, cell.y)).r;";
		break;
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SGRBG12:
		codeHead +=	   "   r = texture2D(tex, vec2(cell.x + texl_w, cell.y)).r;"
				   "   g = texture2D(tex, vec2((cell.y == xy.y) ? cell.x : cell.x + texl_w, xy.y)).r;"
				   "   b = texture2D(tex, vec2(cell.x, cell.y + texl_h)).r;";
		break;
	case V4L2_PIX_FMT_SRGGB8:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SRGGB12:
		codeHead +=	   "   b = texture2D(tex, vec2(cell.x + texl_w, cell.y + texl_h)).r;"
				   "   g = texture2D(tex, vec2((cell.y == xy.y) ? cell.x + texl_w : cell.x, xy.y)).r;"
				   "   r = texture2D(tex, cell).r;";
		break;
	}

	switch (format) {
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
		codeHead +=	   "   b = b * (65535.0 / 1023.0);"
				   "   g = g * (65535.0 / 1023.0);"
				   "   r = r * (65535.0 / 1023.0);";
		break;
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		codeHead +=	   "   b = b * (65535.0 / 4095.0);"
				   "   g = g * (65535.0 / 4095.0);"
				   "   r = r * (65535.0 / 4095.0);";
		break;
	}

	QString codeTail;

	if (m_quantization == V4L2_QUANTIZATION_LIM_RANGE)
		codeTail += codeRGBNormalize();

	codeTail += codeTransformToLinear() +
		    codeColorspaceConversion() +
		    codeTransformToNonLinear() +
		    codeSuffix;

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2").arg(codeHead, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: Bayer shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_Bayer(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_w"); // Texture width
	glUniform1f(idx, m_frameWidth);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "texl_h"); // Texture width
	glUniform1f(idx, 1.0 / m_frameHeight);
	idx = glGetUniformLocation(m_shaderProgram.programId(), "texl_w"); // Texture width
	glUniform1f(idx, 1.0 / m_frameWidth);

	switch (format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_BYTE, m_frameData);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				m_glRed, GL_UNSIGNED_SHORT, m_frameData);
		break;
	}
	checkError("Bayer paint");
}

void CaptureWinGLEngine::shader_YUV_packed(__u32 format)
{
	bool hasAlpha = false;

	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (format) {
	case V4L2_PIX_FMT_YUV555:
		hasAlpha = true;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_YUV444:
		hasAlpha = true;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, NULL);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_frameWidth, m_frameHeight, 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_YUV32:
		hasAlpha = true;
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_frameWidth, m_frameHeight, 0,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;
	}

	checkError("Packed YUV shader");

	QString codeHead = QString("uniform sampler2D tex;"
				   "uniform float tex_h;"
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float ycoord = floor(xy.y * tex_h);");

	if (m_field == V4L2_FIELD_SEQ_TB)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 : xy.y / 2.0 + 0.5;";
	else if (m_field == V4L2_FIELD_SEQ_BT)
		codeHead += "   xy.y = (mod(ycoord, 2.0) == 0.0) ? xy.y / 2.0 + 0.5 : xy.y / 2.0;";

	codeHead +=		   "   vec4 color = texture2D(tex, xy);"
				   "   float a = color.a;";

	codeHead += "   float y = color.r;"
		    "   float u = color.g - 0.5;"
		    "   float v = color.b - 0.5;";

	QString codeTail = codeYUVNormalize() +
			   codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   (hasAlpha ? codeSuffixWithAlpha : codeSuffix);

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2").arg(codeHead, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: Packed YUV shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_YUV_packed(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	switch (format) {
	case V4L2_PIX_FMT_YUV555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_frameData);
		break;

	case V4L2_PIX_FMT_YUV444:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV, m_frameData);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_frameData);
		break;

	case V4L2_PIX_FMT_YUV32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, m_frameData);
		break;
	}
	checkError("Packed YUV paint");
}

#endif
