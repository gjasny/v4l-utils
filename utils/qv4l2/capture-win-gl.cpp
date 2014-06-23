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

CaptureWinGL::CaptureWinGL()
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
	QSize margins = getMargins();
	m_videoSurface.setSize(width() - margins.width(), height() - margins.height());
#endif
	event->accept();
}

void CaptureWinGL::setFrame(int width, int height, __u32 format,
		unsigned char *data, unsigned char *data2, const QString &info)
{
#ifdef HAVE_QTGL
	m_videoSurface.setFrame(width, height, format, data, data2);
#endif
	m_information.setText(info);
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

#ifdef HAVE_QTGL
CaptureWinGLEngine::CaptureWinGLEngine() :
	m_frameHeight(0),
	m_frameWidth(0),
	m_screenTextureCount(0),
	m_formatChange(false),
	m_frameFormat(0),
	m_frameData(NULL)
{
	m_glfunction.initializeGLFunctions(context());
}

CaptureWinGLEngine::~CaptureWinGLEngine()
{
	clearShader();
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

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	checkError("InitializeGL");
}

void CaptureWinGLEngine::setSize(int width, int height)
{
	QSize sizedFrame = CaptureWin::scaleFrameSize(QSize(width, height), QSize(m_frameWidth, m_frameHeight));

	width = sizedFrame.width();
	height = sizedFrame.height();

	if (width > 0 && height > 0) {
		setMaximumSize(width, height);
		resizeGL(width, height);
	}
}

void CaptureWinGLEngine::resizeGL(int width, int height)
{
	glViewport(0, 0, width, height);
}

void CaptureWinGLEngine::setFrame(int width, int height, __u32 format, unsigned char *data, unsigned char *data2)
{
	if (format != m_frameFormat || width != m_frameWidth || height != m_frameHeight) {
		m_formatChange = true;
		m_frameWidth = width;
		m_frameHeight = height;
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

struct supported_fmt {
	__u32 fmt;
	bool uses_shader;
};

bool CaptureWinGLEngine::hasNativeFormat(__u32 format)
{
	static const struct supported_fmt supported_fmts[] = {
		{ V4L2_PIX_FMT_RGB32, false },
		{ V4L2_PIX_FMT_BGR32, false },
		{ V4L2_PIX_FMT_RGB24, false },
		{ V4L2_PIX_FMT_BGR24, true },
		{ V4L2_PIX_FMT_RGB565, false },
		{ V4L2_PIX_FMT_RGB555, false },
		{ V4L2_PIX_FMT_YUYV, true },
		{ V4L2_PIX_FMT_YVYU, true },
		{ V4L2_PIX_FMT_UYVY, true },
		{ V4L2_PIX_FMT_VYUY, true },
		{ V4L2_PIX_FMT_YVU420, true },
		{ V4L2_PIX_FMT_YUV420, true },
		{ V4L2_PIX_FMT_NV16M, true },
		{ V4L2_PIX_FMT_NV61M, true },
		{ 0, false }
	};
	bool haveShaders = m_glfunction.hasOpenGLFeature(QGLFunctions::Shaders);

	for (int i = 0; supported_fmts[i].fmt; i++)
		if (supported_fmts[i].fmt == format &&
		    (!supported_fmts[i].uses_shader || haveShaders))
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

	case V4L2_PIX_FMT_RGB32:
		m_screenTextureCount = 1;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(m_screenTextureCount, m_screenTexture);
		configureTexture(0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		checkError("RGB32 shader");
		break;

	case V4L2_PIX_FMT_BGR32:
		m_screenTextureCount = 1;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(m_screenTextureCount, m_screenTexture);
		configureTexture(0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_frameWidth, m_frameHeight, 0,
			     GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		checkError("BGR32 shader");
		break;

	case V4L2_PIX_FMT_RGB555:
		m_screenTextureCount = 1;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(m_screenTextureCount, m_screenTexture);
		configureTexture(0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, m_frameWidth, m_frameHeight, 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		checkError("RGB555 shader");
		break;

	case V4L2_PIX_FMT_RGB565:
		m_screenTextureCount = 1;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(m_screenTextureCount, m_screenTexture);
		configureTexture(0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_frameWidth, m_frameHeight, 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		checkError("RGB565 shader");
		break;
	case V4L2_PIX_FMT_BGR24:
		shader_BGR();
		break;
	case V4L2_PIX_FMT_RGB24:
	default:
		m_screenTextureCount = 1;
		glActiveTexture(GL_TEXTURE0);
		glGenTextures(m_screenTextureCount, m_screenTexture);
		configureTexture(0);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_frameWidth, m_frameHeight, 0,
			     GL_RGB, GL_UNSIGNED_BYTE, NULL);
		checkError("Default shader");
		break;
	}

	glClear(GL_COLOR_BUFFER_BIT);
}

void CaptureWinGLEngine::paintFrame()
{
	float cropH = (float)CaptureWin::cropHeight(m_frameWidth, m_frameHeight) / m_frameHeight;
	float cropW = (float)CaptureWin::cropWidth(m_frameWidth, m_frameHeight) / m_frameWidth;

	glBegin(GL_QUADS);
	glTexCoord2f(cropW, cropH);               glVertex2f(0, 0);
	glTexCoord2f(1.0f - cropW, cropH);        glVertex2f(m_frameWidth, 0);
	glTexCoord2f(1.0f - cropW, 1.0f - cropH); glVertex2f(m_frameWidth, m_frameHeight);
	glTexCoord2f(cropW, 1.0f - cropH);        glVertex2f(0, m_frameHeight);
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

	case V4L2_PIX_FMT_RGB32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8, m_frameData);
		checkError("RGB32 paint");
		break;

	case V4L2_PIX_FMT_BGR32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_frameData);
		checkError("BGR32 paint");
		break;

	case V4L2_PIX_FMT_RGB555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_frameData);
		checkError("RGB555 paint");
		break;

	case V4L2_PIX_FMT_RGB565:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_frameData);
		checkError("RGB565 paint");
		break;

	case V4L2_PIX_FMT_BGR24:
		render_BGR();
		break;
	case V4L2_PIX_FMT_RGB24:
	default:
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
				GL_RGB, GL_UNSIGNED_BYTE, m_frameData);
		checkError("Default paint");
		break;
	}
	paintFrame();
}

void CaptureWinGLEngine::configureTexture(size_t idx)
{
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[idx]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

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

	bool src_c = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment,
				"uniform sampler2D ytex;"
				"uniform sampler2D utex;"
				"uniform sampler2D vtex;"
				"void main()"
				"{"
				"   vec2 xy = vec2(gl_TexCoord[0].xy);"
				"   float y = 1.1640625 * (texture2D(ytex, xy).r - 0.0625);"
				"   float u = texture2D(utex, xy).r - 0.5;"
				"   float v = texture2D(vtex, xy).r - 0.5;"
				"   float r = y + 1.59765625 * v;"
				"   float g = y - 0.390625 * u - 0.8125 *v;"
				"   float b = y + 2.015625 * u;"
				"   gl_FragColor = vec4(r, g, b, 1.0);"
				"}"
				);

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
			       "   u = texture2D(uvtex, vec2(pixelx, pixely)).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(pixelx + texl_w, pixely)).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, vec2(pixelx - texl_w, pixely)).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(pixelx, pixely)).r - 0.5;"
			       "}"
			       );

	case V4L2_PIX_FMT_NV61M:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   u = texture2D(uvtex, vec2(pixelx + texl_w, pixely)).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(pixelx, pixely)).r - 0.5;"
			       "} else {"
			       "   u = texture2D(uvtex, vec2(pixelx, pixely)).r - 0.5;"
			       "   v = texture2D(uvtex, vec2(pixelx - texl_w, pixely)).r - 0.5;"
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
				   "void main()"
				   "{"
				   "   vec2 xy = vec2(gl_TexCoord[0].xy);"
				   "   float y = 1.1640625 * (texture2D(ytex, xy).r - 0.0625);"
				   "   float u, v;"
				   "   float pixelx = gl_TexCoord[0].x;"
				   "   float pixely = gl_TexCoord[0].y;"
				   "   float xcoord = floor(pixelx * tex_w);"
				   );

	QString codeBody = shader_NV16M_invariant(format);

	QString codeTail = QString("   float r = y + 1.5958 * v;"
				   "   float g = y - 0.39173 * u - 0.81290 * v;"
				   "   float b = y + 2.017 * u;"
				   "   gl_FragColor = vec4(r, g, b, 1.0);"
				   "}"
				   );

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
			       "   luma_chroma = texture2D(tex, vec2(pixelx, pixely));"
			       "   y = (luma_chroma.r - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx - texl_w, pixely));"
			       "   y = (luma_chroma.b - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.g - 0.5;"
			       "v = luma_chroma.a - 0.5;"
			       );

	case V4L2_PIX_FMT_YVYU:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx, pixely));"
			       "   y = (luma_chroma.r - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx - texl_w, pixely));"
			       "   y = (luma_chroma.b - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.a - 0.5;"
			       "v = luma_chroma.g - 0.5;"
			       );

	case V4L2_PIX_FMT_UYVY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx, pixely));"
			       "   y = (luma_chroma.g - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx - texl_w, pixely));"
			       "   y = (luma_chroma.a - 0.0625) * 1.1643;"
			       "}"
			       "u = luma_chroma.r - 0.5;"
			       "v = luma_chroma.b - 0.5;"
			       );

	case V4L2_PIX_FMT_VYUY:
		return QString("if (mod(xcoord, 2.0) == 0.0) {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx, pixely));"
			       "   y = (luma_chroma.g - 0.0625) * 1.1643;"
			       "} else {"
			       "   luma_chroma = texture2D(tex, vec2(pixelx - texl_w, pixely));"
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
				   "void main()"
				   "{"
				   "   float y, u, v;"
				   "   vec4 luma_chroma;"
				   "   float pixelx = gl_TexCoord[0].x;"
				   "   float pixely = gl_TexCoord[0].y;"
				   "   float xcoord = floor(pixelx * tex_w);"
				   );

	QString codeBody = shader_YUY2_invariant(format);

	QString codeTail = QString("   float r = y + 1.5958 * v;"
				   "   float g = y - 0.39173 * u - 0.81290 * v;"
				   "   float b = y + 2.017 * u;"
				   "   gl_FragColor = vec4(r, g, b, 1.0);"
				   "}"
				   );

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

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth / 2, m_frameHeight,
			GL_RGBA, GL_UNSIGNED_BYTE, m_frameData);
	checkError("YUY2 paint");
}

void CaptureWinGLEngine::shader_BGR()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_frameWidth, m_frameHeight, 0,
		     GL_RGB, GL_UNSIGNED_BYTE, NULL);
	checkError("BGR shader");

	bool src_c = m_shaderProgram.addShaderFromSourceCode(
				QGLShader::Fragment,
				"uniform sampler2D tex;"
				"void main()"
				"{"
				"   vec4 color = texture2D(tex, gl_TexCoord[0].xy);"
				"   gl_FragColor = vec4(color.b, color.g, color.r, 1.0);"
				"}"
				);
	if (!src_c)
		fprintf(stderr, "OpenGL Error: BGR shader compilation failed.\n");

	m_shaderProgram.bind();
}

void CaptureWinGLEngine::render_BGR()
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	GLint Y = m_glfunction.glGetUniformLocation(m_shaderProgram.programId(), "tex");
	glUniform1i(Y, 0);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_frameWidth, m_frameHeight,
			GL_RGB, GL_UNSIGNED_BYTE, m_frameData);
	checkError("BGR paint");
}
#endif
