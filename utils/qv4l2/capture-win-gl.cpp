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
				m_frame.planeData[1]);
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

void CaptureWinGL::setColorspace(unsigned colorspace)
{
#ifdef HAVE_QTGL
	m_videoSurface.setColorspace(colorspace);
#endif
}

void CaptureWinGL::setDisplayColorspace(unsigned colorspace)
{
#ifdef HAVE_QTGL
	m_videoSurface.setDisplayColorspace(colorspace);
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
	m_field(V4L2_FIELD_NONE),
	m_displayColorspace(V4L2_COLORSPACE_SRGB),
	m_screenTextureCount(0),
	m_formatChange(false),
	m_frameFormat(0),
	m_frameData(NULL),
	m_blending(false),
	m_mag_filter(GL_NEAREST),
	m_min_filter(GL_NEAREST)
{
	m_glfunction.initializeGLFunctions(context());
}

CaptureWinGLEngine::~CaptureWinGLEngine()
{
	clearShader();
}

void CaptureWinGLEngine::setColorspace(unsigned colorspace)
{
	switch (colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_SMPTE240M:
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
	case V4L2_COLORSPACE_SRGB:
		break;
	default:
		// If the colorspace was not specified, then guess
		// based on the pixel format.
		switch (m_frameFormat) {
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_VYUY:
		case V4L2_PIX_FMT_YVU420:
		case V4L2_PIX_FMT_YUV420:
		case V4L2_PIX_FMT_NV16M:
		case V4L2_PIX_FMT_NV61M:
			// SDTV or HDTV?
			if (m_frameWidth <= 720 && m_frameHeight <= 575)
				colorspace = V4L2_COLORSPACE_SMPTE170M;
			else
				colorspace = V4L2_COLORSPACE_REC709;
			break;
		default:
			colorspace = V4L2_COLORSPACE_SRGB;
			break;
		}
		break;
	}
	if (m_colorspace == colorspace)
		return;
	m_colorspace = colorspace;
	m_formatChange = true;
}

void CaptureWinGLEngine::setDisplayColorspace(unsigned colorspace)
{
	if (colorspace == m_displayColorspace)
		return;
	m_displayColorspace = colorspace;
	if (m_haveFramebufferSRGB) {
		if (m_displayColorspace == V4L2_COLORSPACE_SRGB)
			glEnable(GL_FRAMEBUFFER_SRGB);
		else
			glDisable(GL_FRAMEBUFFER_SRGB);
	}
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
	m_shaderProgram.release();
	m_shaderProgram.removeAllShaders();
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
				  __u32 format, unsigned char *data, unsigned char *data2)
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
		V4L2_PIX_FMT_RGB555,
		V4L2_PIX_FMT_XRGB555,
		V4L2_PIX_FMT_ARGB555,
		V4L2_PIX_FMT_RGB555X,
		V4L2_PIX_FMT_YUYV,
		V4L2_PIX_FMT_YVYU,
		V4L2_PIX_FMT_UYVY,
		V4L2_PIX_FMT_VYUY,
		V4L2_PIX_FMT_YVU420,
		V4L2_PIX_FMT_YUV420,
		V4L2_PIX_FMT_NV16M,
		V4L2_PIX_FMT_NV61M,
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

	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		shader_NV16M(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		shader_YUV();
		break;

	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
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
	default:
		shader_RGB();
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
		render_YUY2();
		break;

	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		render_NV16M(m_frameFormat);
		break;

	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		render_YUV(m_frameFormat);
		break;

	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
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
	default:
		render_RGB();
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

// Convert Y'CbCr (aka YUV) to R'G'B', taking into account the
// colorspace.
QString CaptureWinGLEngine::codeYUV2RGB()
{
	switch (m_colorspace) {
	case V4L2_COLORSPACE_SMPTE240M:
		// Old obsolete HDTV standard. Replaced by REC 709.
		// SMPTE 240M has its own luma coefficients
		return QString("   float r = y + 1.8007 * v;"
			       "   float g = y - 0.2575 * u - 0.57143 * v;"
			       "   float b = y + 2.088 * u;"
			       );
	case V4L2_COLORSPACE_SMPTE170M:
	case V4L2_COLORSPACE_470_SYSTEM_M:
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		// These SDTV colorspaces all use the BT.601 luma coefficients
		return QString("   float r = y + 1.5958 * v;"
			       "   float g = y - 0.39173 * u - 0.81290 * v;"
			       "   float b = y + 2.017 * u;"
			       );
	case V4L2_COLORSPACE_REC709:
	case V4L2_COLORSPACE_SRGB:
	default:
		// The HDTV/graphics colorspaces all use REC 709 luma coefficients
		return QString("   float r = y + 1.79274 * v;"
			       "   float g = y - 0.21325 * u - 0.53291 * v;"
			       "   float b = y + 2.1124 * u;"
			       );
	}
}

// Convert non-linear R'G'B' to linear RGB, taking into account the
// colorspace.
QString CaptureWinGLEngine::codeTransformToLinear()
{
	switch (m_colorspace) {
	case V4L2_COLORSPACE_SMPTE240M:
		// Old obsolete HDTV standard. Replaced by REC 709.
		// This is the transfer function for SMPTE 240M
		return QString("   r = (r < 0.0913) ? r / 4.0 : pow((r + 0.1115) / 1.1115, 1.0 / 0.45);"
			       "   g = (g < 0.0913) ? g / 4.0 : pow((g + 0.1115) / 1.1115, 1.0 / 0.45);"
			       "   b = (b < 0.0913) ? b / 4.0 : pow((b + 0.1115) / 1.1115, 1.0 / 0.45);"
			       );
	case V4L2_COLORSPACE_SRGB:
		// This is used for sRGB as specified by the IEC FDIS 61966-2-1 standard
		return QString("   r = (r <= 0.03928) ? r / 12.92 : pow((r + 0.055) / 1.055, 2.4);"
			       "   g = (g <= 0.03928) ? g / 12.92 : pow((g + 0.055) / 1.055, 2.4);"
			       "   b = (b <= 0.03928) ? b / 12.92 : pow((b + 0.055) / 1.055, 2.4);"
			       );
	case V4L2_COLORSPACE_REC709:
	default:
		// All others use the transfer function specified by REC 709
		return QString("   r = (r < 0.081) ? r / 4.5 : pow((r + 0.099) / 1.099, 1.0 / 0.45);"
			       "   g = (g < 0.081) ? g / 4.5 : pow((g + 0.099) / 1.099, 1.0 / 0.45);"
			       "   b = (b < 0.081) ? b / 4.5 : pow((b + 0.099) / 1.099, 1.0 / 0.45);"
			       );
	}
}

// Convert the given colorspace to the REC 709/sRGB colorspace. All colors are
// specified as linear RGB.
QString CaptureWinGLEngine::codeColorspaceConversion()
{
	switch (m_colorspace) {
	case V4L2_COLORSPACE_SMPTE170M:
		// Current SDTV standard, although slowly being replaced by REC 709.
		// Use the SMPTE 170M aka SMPTE-C aka SMPTE RP 145 conversion matrix.
		return QString("   float rr =  0.939536 * r + 0.050215 * g + 0.001789 * b;"
			       "   float gg =  0.017743 * r + 0.965758 * g + 0.016243 * b;"
			       "   float bb = -0.001591 * r - 0.004356 * g + 1.005951 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_SMPTE240M:
		// Old obsolete HDTV standard. Replaced by REC 709.
		// Use the SMPTE 240M conversion matrix.
		return QString("   float rr =  1.4086 * r - 0.4086 * g;"
			       "   float gg = -0.0257 * r + 1.0457 * g;"
			       "   float bb = -0.0254 * r - 0.0440 * g + 1.0695 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_470_SYSTEM_M:
		// Old obsolete NTSC standard. Replaced by REC 709.
		// Use the NTSC 1953 conversion matrix.
		return QString("   float rr =  1.5073 * r - 0.3725 * g - 0.0832 * b;"
			       "   float gg = -0.0275 * r + 0.9350 * g + 0.0670 * b;"
			       "   float bb = -0.0272 * r - 0.0401 * g + 1.1677 * b;"
			       "   r = rr; g = gg; b = bb;"
			       );
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		// Old obsolete PAL/SECAM standard. Replaced by REC 709.
		// Use the EBU Tech. 3213 conversion matrix.
		return QString("   float rr = 1.0440 * r - 0.0440 * g;"
			       "   float bb = -0.0119 * g + 1.0119 * b;"
			       "   r = rr; b = bb;"
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
	switch (m_displayColorspace) {
	case 0:	// Keep as linear RGB
		return "";

	case V4L2_COLORSPACE_SMPTE240M:
		// Use the SMPTE 240M transfer function
		return QString("   r = (r < 0.0228) ? r * 4.0 : 1.1115 * pow(r, 0.45) - 0.1115;"
			       "   g = (g < 0.0228) ? g * 4.0 : 1.1115 * pow(g, 0.45) - 0.1115;"
			       "   b = (b < 0.0228) ? b * 4.0 : 1.1115 * pow(b, 0.45) - 0.1115;"
			       );
	case V4L2_COLORSPACE_SRGB:
		// Use the sRGB transfer function. Do nothing if the GL_FRAMEBUFFER_SRGB
		// is available.
		if (m_haveFramebufferSRGB)
			return "";
		return QString("   r = (r <= 0.0031308) ? r * 12.92 : 1.055 * pow(r, 1.0 / 2.4) - 0.055;"
			       "   g = (g <= 0.0031308) ? g * 12.92 : 1.055 * pow(g, 1.0 / 2.4) - 0.055;"
			       "   b = (b <= 0.0031308) ? b * 12.92 : 1.055 * pow(b, 1.0 / 2.4) - 0.055;"
			       );
	case V4L2_COLORSPACE_REC709:
	default:
		// Use the REC 709 transfer function
		return QString("   r = (r < 0.018) ? r * 4.5 : 1.099 * pow(r, 0.45) - 0.099;"
			       "   g = (g < 0.018) ? g * 4.5 : 1.099 * pow(g, 0.45) - 0.099;"
			       "   b = (b < 0.018) ? b * 4.5 : 1.099 * pow(b, 0.45) - 0.099;"
			       );
	}
}

static const QString codeSuffix("   gl_FragColor = vec4(r, g, b, 0.0);"
			  "}");

static const QString codeSuffixWithAlpha("   gl_FragColor = vec4(r, g, b, a);"
			  "}");

void CaptureWinGLEngine::shader_YUV()
{
	m_screenTextureCount = 3;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth, m_frameHeight, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth / 2, m_frameHeight / 2, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 1");

	glActiveTexture(GL_TEXTURE2);
	configureTexture(2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth / 2, m_frameHeight / 2, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
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

	codeHead += 		   "   float y = 1.1640625 * (texture2D(ytex, xy).r - 0.0625);"
				   "   float u = texture2D(utex, xy).r - 0.5;"
				   "   float v = texture2D(vtex, xy).r - 0.5;";

	QString codeTail = codeYUV2RGB() +
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
	int idxU;
	int idxV;

	if (format == V4L2_PIX_FMT_YUV420) {
		idxU = m_frameWidth * m_frameHeight;
		idxV = idxU + (idxU / 4);
	} else {
		idxV = m_frameWidth * m_frameHeight;
		idxU = idxV + (idxV / 4);
	}

	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "ytex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData);
	checkError("YUV paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint U = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "utex");
	glUniform1i(U, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / 2, m_frameHeight / 2,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData == NULL ? NULL : &m_frameData[idxU]);
	checkError("YUV paint utex");

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[2]);
	GLint V = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "vtex");
	glUniform1i(V, 2);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / 2, m_frameHeight / 2,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData == NULL ? NULL : &m_frameData[idxV]);
	checkError("YUV paint vtex");
}

QString CaptureWinGLEngine::shader_NV16M_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_NV16M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, xy).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(xy.x + texl_w, xy.y)).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, vec2(xy.x - texl_w, xy.y)).r - 0.5;"
			       "   v = texture2D(uvtex, xy).r - 0.5;"
			       "}"
			       );

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

void CaptureWinGLEngine::shader_NV16M(__u32 format)
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth, m_frameHeight, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16M shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, m_frameWidth, m_frameHeight, 0,
		     GL_LUMINANCE, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16M shader texture 1");

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

	codeHead +=		   "   float u, v;"
				   "   float xcoord = floor(xy.x * tex_w);"
				   "   float y = 1.1640625 * (texture2D(ytex, xy).r - 0.0625);";



	QString codeBody = shader_NV16M_invariant(format);

	QString codeTail = codeYUV2RGB() +
			   codeTransformToLinear() +
			   codeColorspaceConversion() +
			   codeTransformToNonLinear() +
			   codeSuffix;

	bool src_ok = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment, QString("%1%2%3").arg(codeHead, codeBody, codeTail)
				);

	if (!src_ok)
		fprintf(stderr, "OpenGL Error: NV16M shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_NV16M(__u32 format)
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
			GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData);
	checkError("NV16M paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	GLint UV = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "uvtex");
	glUniform1i(UV, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			GL_LUMINANCE, GL_UNSIGNED_BYTE, m_frameData2);
	checkError("NV16M paint");
}

QString CaptureWinGLEngine::shader_YUY2_invariant(__u32 format)
{
	switch (format) {
	case V4L2_PIX_FMT_YUYV:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = (luma_chroma.r - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = (luma_chroma.b - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.g - 0.5;"
			       "v = luma_chroma.a - 0.5;"
			       );

	case V4L2_PIX_FMT_YVYU:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = (luma_chroma.r - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = (luma_chroma.b - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.a - 0.5;"
			       "v = luma_chroma.g - 0.5;"
			       );

	case V4L2_PIX_FMT_UYVY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = (luma_chroma.g - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = (luma_chroma.a - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.r - 0.5;"
			       "v = luma_chroma.b - 0.5;"
			       );

	case V4L2_PIX_FMT_VYUY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, xy);"
			       "   y = (luma_chroma.g - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(xy.x - texl_w, xy.y));"
			       "   y = (luma_chroma.a - 0.0625) * 1.1643;"
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

	QString codeTail = codeYUV2RGB() +
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

void CaptureWinGLEngine::render_YUY2()
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

void CaptureWinGLEngine::shader_RGB()
{
	bool hasAlpha = false;

	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	GLint internalFmt = m_colorspace == V4L2_COLORSPACE_SRGB ?
						GL_SRGB8_ALPHA8 : GL_RGBA8;

	switch (m_frameFormat) {
	case V4L2_PIX_FMT_ARGB555:
		hasAlpha = true;
		/* fall-through */
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_RGB555X:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		hasAlpha = true;
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
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
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

	if (m_frameFormat == V4L2_PIX_FMT_BGR24)
		codeHead += "   float r = color.b;"
			    "   float g = color.g;"
			    "   float b = color.r;";
	else
		codeHead += "   float r = color.r;"
			    "   float g = color.g;"
			    "   float b = color.b;";

	QString codeTail;
	
	if (m_colorspace != V4L2_COLORSPACE_SRGB)
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

void CaptureWinGLEngine::render_RGB()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	int idx = glGetUniformLocation(m_shaderProgram.programId(), "tex_h"); // Texture height
	glUniform1f(idx, m_frameHeight);

	switch (m_frameFormat) {
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_frameData);
		break;

	case V4L2_PIX_FMT_RGB555X:
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
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, m_frameData);
		break;
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_frameData);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	default:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_BYTE, m_frameData);
		break;
	}
	checkError("RGB paint");
}

#endif
