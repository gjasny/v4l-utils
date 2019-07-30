// SPDX-License-Identifier: GPL-2.0-only
/*
 * The YUY2 shader code is based on face-responder. The code is under public domain:
 * https://bitbucket.org/nateharward/face-responder/src/0c3b4b957039d9f4bf1da09b9471371942de2601/yuv42201_laplace.frag?at=master
 *
 * All other OpenGL code:
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#include "capture.h"

#include <QtCore/QTextStream>
#include <QtCore/QCoreApplication>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLPaintDevice>
#include <QtGui/QContextMenuEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtCore/QSocketNotifier>
#include <QtMath>
#include <QTimer>
#include <QApplication>

#include "v4l2-info.h"

void CaptureWin::initializeGL()
{
	initializeOpenGLFunctions();
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);

	// Check if the the GL_FRAMEBUFFER_SRGB feature is available.
	// If it is, then the GPU can perform the SRGB transfer function
	// for us.
	GLint res = 0;
	if (context()->hasExtension("GL_EXT_framebuffer_sRGB"))
		glGetIntegerv(GL_FRAMEBUFFER_SRGB_CAPABLE_EXT, &res);
	checkError("InitializeGL Part 1");

	glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
	m_haveSwapBytes = glGetError() == GL_NO_ERROR;

	if (m_verbose) {
		printf("OpenGL %sdoes%s support GL_UNPACK_SWAP_BYTES\n",
		       context()->isOpenGLES() ? "ES " : "",
		       m_haveSwapBytes ? "" : " not");
	}
	if (m_uses_gl_red && glGetString(GL_VERSION)[0] < '3') {
		fprintf(stderr, "The openGL implementation does not support GL_RED/GL_RG\n");
		exit(1);
	}

	QColor bg = QWidget::palette().color(QWidget::backgroundRole());
	glClearColor(bg.red() / 255.0f,
		     bg.green() / 255.0f,
		     bg.blue() / 255.0f,
		     0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	checkError("InitializeGL Part 2");
	m_program = new QOpenGLShaderProgram(this);
	m_updateShader = true;
}


void CaptureWin::paintGL()
{
	if (m_v4l_fmt.g_width() < 16 || m_v4l_fmt.g_frame_height() < 16)
		return;

	if (m_mode == AppModeV4L2) {
		if (m_v4l_queue == NULL || m_nextIndex == -1)
			return;

		cv4l_buffer buf(*m_v4l_queue, m_curIndex);

		m_fd->qbuf(buf);
		for (unsigned i = 0; i < m_v4l_queue->g_num_planes(); i++) {
			m_curData[i] = m_nextData[i];
			m_curSize[i] = m_nextSize[i];
			m_curIndex = m_nextIndex;
			m_nextIndex = -1;
			m_nextData[i] = 0;
			m_nextSize[i] = 0;
		}
	}


	if (m_curData[0] == NULL) {
		// No data, just clear display
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		checkError("paintGL - no data");
		return;
	}

	if (m_updateShader) {
		m_updateShader = false;
		if (!supportedFmt(m_v4l_fmt.g_pixelformat())) {
			fprintf(stderr, "OpenGL ES unsupported format 0x%08x ('%s').\n",
				m_v4l_fmt.g_pixelformat(), fcc2s(m_v4l_fmt.g_pixelformat()).c_str());

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			checkError("paintGL - no data");
			return;
		}

		changeShader();
	}

	if (!supportedFmt(m_v4l_fmt.g_pixelformat()))
		return;

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		render_YUY2(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		render_NV16(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		render_NV12(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		render_NV24(m_v4l_fmt.g_pixelformat());
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
		render_YUV(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
		render_YUV_packed(m_v4l_fmt.g_pixelformat());
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
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		render_Bayer(m_v4l_fmt.g_pixelformat());
		break;

	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGBX555:
	case V4L2_PIX_FMT_RGBA555:
	case V4L2_PIX_FMT_XBGR555:
	case V4L2_PIX_FMT_ABGR555:
	case V4L2_PIX_FMT_BGRX555:
	case V4L2_PIX_FMT_BGRA555:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_XBGR444:
	case V4L2_PIX_FMT_ABGR444:
	case V4L2_PIX_FMT_RGBX444:
	case V4L2_PIX_FMT_RGBA444:
	case V4L2_PIX_FMT_BGRX444:
	case V4L2_PIX_FMT_BGRA444:
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
	case V4L2_PIX_FMT_RGBX32:
	case V4L2_PIX_FMT_BGRX32:
	case V4L2_PIX_FMT_RGBA32:
	case V4L2_PIX_FMT_BGRA32:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		render_RGB(m_v4l_fmt.g_pixelformat());
		break;
	}

	static unsigned long long tot_t;
	static unsigned cnt;
	GLuint query;
	QSize s = correctAspect(m_viewSize);
	bool scale = m_scrollArea->widgetResizable();

	glViewport(scale ? (size().width() - s.width()) / 2 : 0,
		   scale ? (size().height() - s.height()) / 2 : 0,
		   s.width(), s.height());

	if (m_reportTimings) {
		glGenQueries(1, &query);
		glBeginQuery(GL_TIME_ELAPSED, query);
	}

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Define Quad surface to draw to, vertices sequenced for GL_TRIANGLE_FAN
	// Adjusting these will make the screen to draw to smaller or larger
	const GLfloat quadVertex[] = {
		-1.0f, -1.0f, // Left Bottom
		+1.0f, -1.0f, // Right Bottom
		+1.0f, +1.0f, // Rigth Top
		-1.0f, +1.0f  // Left Top
	};

	// Normalized texture coords to be aligned to draw quad corners, same sequence but 0,0 is Left Top
	const GLuint texCoords[] = {
		0, 1,
		1, 1,
		1, 0,
		0, 0
	};

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertex), quadVertex, GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);

	// Attach Quad positions to vertex shader "position" attribute (location = 0)
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

	// Attach texture corner positions to vertex shader "texCoord" attribute (location = 1)
	glEnableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_INT, GL_FALSE, 0, (void*)0);

	// Draw quad with texture
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	// Disable attrib arrays
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDeleteBuffers(1, &vertexbuffer);

	checkError("paintGL");

	if (m_reportTimings) {
		glEndQuery(GL_TIME_ELAPSED);
		GLuint t;
		glGetQueryObjectuiv(query, GL_QUERY_RESULT, &t);
		glDeleteQueries(1, &query);

		cnt++;
		tot_t += t;
		unsigned ave = tot_t / cnt;
		printf("Average render time: %09u ns, frame %d render time: %09u ns\n",
		       ave, cnt, t);
	}
}

static const char *prog =
#include "v4l2-convert.h"
;

struct define {
	const char *name;
	unsigned id;
};

#define DEF(c) { .name = #c, .id = c }

static const struct define defines[] = {
	DEF(V4L2_PIX_FMT_YUYV),
	DEF(V4L2_PIX_FMT_YVYU),
	DEF(V4L2_PIX_FMT_UYVY),
	DEF(V4L2_PIX_FMT_VYUY),
	DEF(V4L2_PIX_FMT_YUV422P),
	DEF(V4L2_PIX_FMT_YVU420),
	DEF(V4L2_PIX_FMT_YUV420),
	DEF(V4L2_PIX_FMT_NV12),
	DEF(V4L2_PIX_FMT_NV21),
	DEF(V4L2_PIX_FMT_NV16),
	DEF(V4L2_PIX_FMT_NV61),
	DEF(V4L2_PIX_FMT_NV24),
	DEF(V4L2_PIX_FMT_NV42),
	DEF(V4L2_PIX_FMT_NV16M),
	DEF(V4L2_PIX_FMT_NV61M),
	DEF(V4L2_PIX_FMT_YVU420M),
	DEF(V4L2_PIX_FMT_YUV420M),
	DEF(V4L2_PIX_FMT_YVU422M),
	DEF(V4L2_PIX_FMT_YUV422M),
	DEF(V4L2_PIX_FMT_YVU444M),
	DEF(V4L2_PIX_FMT_YUV444M),
	DEF(V4L2_PIX_FMT_NV12M),
	DEF(V4L2_PIX_FMT_NV21M),
	DEF(V4L2_PIX_FMT_YUV444),
	DEF(V4L2_PIX_FMT_YUV555),
	DEF(V4L2_PIX_FMT_YUV565),
	DEF(V4L2_PIX_FMT_YUV32),
	DEF(V4L2_PIX_FMT_AYUV32),
	DEF(V4L2_PIX_FMT_XYUV32),
	DEF(V4L2_PIX_FMT_VUYA32),
	DEF(V4L2_PIX_FMT_VUYX32),
	DEF(V4L2_PIX_FMT_RGB32),
	DEF(V4L2_PIX_FMT_XRGB32),
	DEF(V4L2_PIX_FMT_ARGB32),
	DEF(V4L2_PIX_FMT_RGBX32),
	DEF(V4L2_PIX_FMT_RGBA32),
	DEF(V4L2_PIX_FMT_BGR32),
	DEF(V4L2_PIX_FMT_XBGR32),
	DEF(V4L2_PIX_FMT_ABGR32),
	DEF(V4L2_PIX_FMT_BGRX32),
	DEF(V4L2_PIX_FMT_BGRA32),
	DEF(V4L2_PIX_FMT_RGB24),
	DEF(V4L2_PIX_FMT_BGR24),
	DEF(V4L2_PIX_FMT_RGB565),
	DEF(V4L2_PIX_FMT_RGB565X),
	DEF(V4L2_PIX_FMT_RGB444),
	DEF(V4L2_PIX_FMT_XRGB444),
	DEF(V4L2_PIX_FMT_ARGB444),
	DEF(V4L2_PIX_FMT_XBGR444),
	DEF(V4L2_PIX_FMT_ABGR444),
	DEF(V4L2_PIX_FMT_RGBX444),
	DEF(V4L2_PIX_FMT_RGBA444),
	DEF(V4L2_PIX_FMT_BGRX444),
	DEF(V4L2_PIX_FMT_BGRA444),
	DEF(V4L2_PIX_FMT_RGB555),
	DEF(V4L2_PIX_FMT_XRGB555),
	DEF(V4L2_PIX_FMT_ARGB555),
	DEF(V4L2_PIX_FMT_RGB555X),
	DEF(V4L2_PIX_FMT_XRGB555X),
	DEF(V4L2_PIX_FMT_ARGB555X),
	DEF(V4L2_PIX_FMT_RGBX555),
	DEF(V4L2_PIX_FMT_RGBA555),
	DEF(V4L2_PIX_FMT_XBGR555),
	DEF(V4L2_PIX_FMT_ABGR555),
	DEF(V4L2_PIX_FMT_BGRX555),
	DEF(V4L2_PIX_FMT_BGRA555),
	DEF(V4L2_PIX_FMT_RGB332),
	DEF(V4L2_PIX_FMT_BGR666),
	DEF(V4L2_PIX_FMT_SBGGR8),
	DEF(V4L2_PIX_FMT_SGBRG8),
	DEF(V4L2_PIX_FMT_SGRBG8),
	DEF(V4L2_PIX_FMT_SRGGB8),
	DEF(V4L2_PIX_FMT_SBGGR10),
	DEF(V4L2_PIX_FMT_SGBRG10),
	DEF(V4L2_PIX_FMT_SGRBG10),
	DEF(V4L2_PIX_FMT_SRGGB10),
	DEF(V4L2_PIX_FMT_SBGGR12),
	DEF(V4L2_PIX_FMT_SGBRG12),
	DEF(V4L2_PIX_FMT_SGRBG12),
	DEF(V4L2_PIX_FMT_SRGGB12),
	DEF(V4L2_PIX_FMT_SBGGR16),
	DEF(V4L2_PIX_FMT_SGBRG16),
	DEF(V4L2_PIX_FMT_SGRBG16),
	DEF(V4L2_PIX_FMT_SRGGB16),
	DEF(V4L2_PIX_FMT_HSV24),
	DEF(V4L2_PIX_FMT_HSV32),
	DEF(V4L2_PIX_FMT_GREY),
	DEF(V4L2_PIX_FMT_Y10),
	DEF(V4L2_PIX_FMT_Y12),
	DEF(V4L2_PIX_FMT_Y16),
	DEF(V4L2_PIX_FMT_Y16_BE),
	DEF(V4L2_PIX_FMT_Z16),

	DEF(V4L2_FIELD_ANY),
	DEF(V4L2_FIELD_NONE),
	DEF(V4L2_FIELD_TOP),
	DEF(V4L2_FIELD_BOTTOM),
	DEF(V4L2_FIELD_INTERLACED),
	DEF(V4L2_FIELD_SEQ_TB),
	DEF(V4L2_FIELD_SEQ_BT),
	DEF(V4L2_FIELD_ALTERNATE),
	DEF(V4L2_FIELD_INTERLACED_TB),
	DEF(V4L2_FIELD_INTERLACED_BT),

	DEF(V4L2_COLORSPACE_DEFAULT),
	DEF(V4L2_COLORSPACE_SMPTE170M),
	DEF(V4L2_COLORSPACE_SMPTE240M),
	DEF(V4L2_COLORSPACE_REC709),
	DEF(V4L2_COLORSPACE_470_SYSTEM_M),
	DEF(V4L2_COLORSPACE_470_SYSTEM_BG),
	DEF(V4L2_COLORSPACE_SRGB),
	DEF(V4L2_COLORSPACE_OPRGB),
	DEF(V4L2_COLORSPACE_BT2020),
	DEF(V4L2_COLORSPACE_RAW),
	DEF(V4L2_COLORSPACE_DCI_P3),

	DEF(V4L2_XFER_FUNC_DEFAULT),
	DEF(V4L2_XFER_FUNC_709),
	DEF(V4L2_XFER_FUNC_SRGB),
	DEF(V4L2_XFER_FUNC_OPRGB),
	DEF(V4L2_XFER_FUNC_SMPTE240M),
	DEF(V4L2_XFER_FUNC_NONE),
	DEF(V4L2_XFER_FUNC_DCI_P3),
	DEF(V4L2_XFER_FUNC_SMPTE2084),

	DEF(V4L2_YCBCR_ENC_DEFAULT),
	DEF(V4L2_YCBCR_ENC_601),
	DEF(V4L2_YCBCR_ENC_709),
	DEF(V4L2_YCBCR_ENC_XV601),
	DEF(V4L2_YCBCR_ENC_XV709),
	DEF(V4L2_YCBCR_ENC_BT2020),
	DEF(V4L2_YCBCR_ENC_BT2020_CONST_LUM),
	DEF(V4L2_YCBCR_ENC_SMPTE240M),
	DEF(V4L2_HSV_ENC_180),
	DEF(V4L2_HSV_ENC_256),

	DEF(V4L2_QUANTIZATION_DEFAULT),
	DEF(V4L2_QUANTIZATION_FULL_RANGE),
	DEF(V4L2_QUANTIZATION_LIM_RANGE),
	{ NULL, 0 }
};

void CaptureWin::changeShader()
{
	if (m_screenTextureCount)
		glDeleteTextures(m_screenTextureCount, m_screenTexture);
	m_program->removeAllShaders();
	checkError("Render settings.\n");

	QString code;

	if (context()->isOpenGLES())
		code = "#version 300 es\n"
			"precision mediump float;\n";
	else
		code = "#version 330\n";
	code += QString(
		"const float tex_w = %1.0;\n"
		"const float tex_h = %2.0;\n"
		"#define FIELD %3\n"
		"#define IS_RGB %4\n"
		"#define PIXFMT %5u\n"
		"#define COLSP %6\n"
		"#define XFERFUNC %7\n"
		"#define YCBCRENC %8\n"
		"#define QUANT %9\n\n"
		"#define IS_HSV %10\n"
		"#define HSVENC %11\n")
		.arg(m_v4l_fmt.g_width())
		.arg(m_v4l_fmt.g_height())
		.arg(m_v4l_fmt.g_field())
		.arg(m_is_rgb)
		.arg(m_v4l_fmt.g_pixelformat())
		.arg(m_v4l_fmt.g_colorspace())
		.arg(m_accepts_srgb ? V4L2_XFER_FUNC_NONE : m_v4l_fmt.g_xfer_func())
		.arg(m_v4l_fmt.g_ycbcr_enc())
		.arg(m_v4l_fmt.g_quantization())
		.arg(m_is_hsv)
		.arg(m_v4l_fmt.g_hsv_enc());

	for (unsigned i = 0; defines[i].name; i++)
		code += QString("#define ") + defines[i].name + " " + QString("%1").arg(defines[i].id) + "u\n";
	code += "#line 1\n";

	code += prog;

	bool src_ok = m_program->addShaderFromSourceCode(
		QOpenGLShader::Fragment, code);

	if (!src_ok) {
		fprintf(stderr, "OpenGL Error: fragment shader compilation failed.\n");
		exit(1);
	}

	// Mandatory vertex shader replaces fixed pipeline in GLES 2.0. In this case just a feedthrough shader.
	QString vertexShaderSrc;

	if (context()->isOpenGLES())
		vertexShaderSrc = "#version 300 es\n"
			"precision mediump float;\n";
	else
		vertexShaderSrc = "#version 330\n";

	vertexShaderSrc +=
		"layout(location = 0) in vec2 position;\n"
		"layout(location = 1) in vec2 texCoord;\n"
		"out vec2 vs_TexCoord;\n"
		"void main() {\n"
		"       gl_Position = vec4(position, 0.0, 1.0);\n"
		"       vs_TexCoord = texCoord;\n"
		"}\n";

	src_ok = m_program->addShaderFromSourceCode(
		QOpenGLShader::Vertex, vertexShaderSrc);

	if (!src_ok) {
		fprintf(stderr, "OpenGL Error: vertex shader compilation failed.\n");
		exit(1);
	}

	if (!m_program->bind()) {
		fprintf(stderr, "OpenGL Error: shader bind failed.\n");
		exit(1);
	}

	GLint loc = m_program->uniformLocation("uvtex");

	if (loc >= 0)
		m_program->setUniformValue(loc, 1);
	loc = m_program->uniformLocation("utex");
	if (loc >= 0)
		m_program->setUniformValue(loc, 1);
	loc = m_program->uniformLocation("vtex");
	if (loc >= 0)
		m_program->setUniformValue(loc, 2);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
		shader_YUY2();
		break;

	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		shader_NV16();
		break;

	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		shader_NV12();
		break;

	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
		shader_NV24();
		break;

	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
		shader_YUV_packed();
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
		shader_YUV();
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
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		shader_Bayer();
		break;

	case V4L2_PIX_FMT_RGB332:
	case V4L2_PIX_FMT_BGR666:
	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_XBGR444:
	case V4L2_PIX_FMT_ABGR444:
	case V4L2_PIX_FMT_RGBX444:
	case V4L2_PIX_FMT_RGBA444:
	case V4L2_PIX_FMT_BGRX444:
	case V4L2_PIX_FMT_BGRA444:
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_RGBX555:
	case V4L2_PIX_FMT_RGBA555:
	case V4L2_PIX_FMT_XBGR555:
	case V4L2_PIX_FMT_ABGR555:
	case V4L2_PIX_FMT_BGRX555:
	case V4L2_PIX_FMT_BGRA555:
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
	case V4L2_PIX_FMT_RGBX32:
	case V4L2_PIX_FMT_BGRX32:
	case V4L2_PIX_FMT_RGBA32:
	case V4L2_PIX_FMT_BGRA32:
	case V4L2_PIX_FMT_GREY:
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
	case V4L2_PIX_FMT_HSV24:
	case V4L2_PIX_FMT_HSV32:
	default:
		shader_RGB();
		break;
	}
}

void CaptureWin::shader_YUV()
{
	unsigned vdiv = 2, hdiv = 2;

	switch (m_v4l_fmt.g_pixelformat()) {
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
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 1");

	glActiveTexture(GL_TEXTURE2);
	configureTexture(2);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("YUV shader texture 2");
}

void CaptureWin::shader_NV12()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2, 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV12 shader texture 1");
}

void CaptureWin::shader_NV24()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RG, GL_UNSIGNED_BYTE, NULL);
	checkError("NV24 shader texture 1");
}

void CaptureWin::shader_NV16()
{
	m_screenTextureCount = 2;
	glGenTextures(m_screenTextureCount, m_screenTexture);

	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 0");

	glActiveTexture(GL_TEXTURE1);
	configureTexture(1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
		     GL_RED, GL_UNSIGNED_BYTE, NULL);
	checkError("NV16 shader texture 1");
}

void CaptureWin::shader_YUY2()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	configureTexture(0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width() / 2, m_v4l_fmt.g_height(), 0,
		     GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	checkError("YUY2 shader");
}

void CaptureWin::shader_RGB()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	GLint internalFmt = m_accepts_srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ABGR444:
	case V4L2_PIX_FMT_XBGR444:
	case V4L2_PIX_FMT_RGBA444:
	case V4L2_PIX_FMT_RGBX444:
	case V4L2_PIX_FMT_BGRA444:
	case V4L2_PIX_FMT_BGRX444:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
		break;

	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
	case V4L2_PIX_FMT_XBGR555:
	case V4L2_PIX_FMT_ABGR555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_RGBX555:
	case V4L2_PIX_FMT_RGBA555:
	case V4L2_PIX_FMT_BGRX555:
	case V4L2_PIX_FMT_BGRA555:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, NULL);
		break;

	case V4L2_PIX_FMT_BGR666:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, NULL);
		break;

	case V4L2_PIX_FMT_RGB332:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_BYTE_3_3_2, NULL);
		break;

	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_RGBA32:
	case V4L2_PIX_FMT_RGBX32:
	case V4L2_PIX_FMT_HSV32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_BGRA32:
	case V4L2_PIX_FMT_BGRX32:
		glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_GREY:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Y16_BE:
	case V4L2_PIX_FMT_Z16:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glTexImage2D(GL_TEXTURE_2D, 0, m_accepts_srgb ? GL_SRGB8 : GL_RGB8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
				GL_RGB, GL_UNSIGNED_BYTE, NULL);
		break;
	}

	checkError("RGB shader");
}

void CaptureWin::shader_Bayer()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RED_INTEGER, GL_UNSIGNED_SHORT, NULL);
		break;
	}

	checkError("Bayer shader");
}

void CaptureWin::shader_YUV_packed()
{
	m_screenTextureCount = 1;
	glGenTextures(m_screenTextureCount, m_screenTexture);
	glActiveTexture(GL_TEXTURE0);
	configureTexture(0);

	switch (m_v4l_fmt.g_pixelformat()) {
	case V4L2_PIX_FMT_YUV555:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
		break;

	case V4L2_PIX_FMT_YUV444:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, NULL);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
		break;
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(), 0,
			     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		break;
	}

	checkError("Packed YUV shader");
}

void CaptureWin::render_YUV(__u32 format)
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
		idxU = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxV = idxU + (idxU / 2);
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
		vdiv = 1;
		break;
	case V4L2_PIX_FMT_YUV420:
		idxU = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxV = idxU + (idxU / 4);
		break;
	case V4L2_PIX_FMT_YVU420:
		idxV = m_v4l_fmt.g_width() * m_v4l_fmt.g_height();
		idxU = idxV + (idxV / 4);
		break;
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("YUV paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0] == NULL ? NULL : &m_curData[0][idxU]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[2]);
		break;
	}
	checkError("YUV paint utex");

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[2]);
	switch (format) {
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0] == NULL ? NULL : &m_curData[0][idxV]);
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YUV444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[2]);
		break;
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YVU444M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / hdiv, m_v4l_fmt.g_height() / vdiv,
			GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	checkError("YUV paint vtex");
}

void CaptureWin::render_NV12(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV12 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2,
				GL_RED, GL_UNSIGNED_BYTE,
				m_curData[0] ? m_curData[0] + m_v4l_fmt.g_width() * m_v4l_fmt.g_height() : NULL);
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height() / 2,
				GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	checkError("NV12 paint uvtex");
}

void CaptureWin::render_NV24(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV24 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RG, GL_UNSIGNED_BYTE,
			m_curData[0] ? m_curData[0] + m_v4l_fmt.g_sizeimage() / 3 : NULL);
	checkError("NV24 paint uvtex");
}

void CaptureWin::render_NV16(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
			GL_RED, GL_UNSIGNED_BYTE, m_curData[0]);
	checkError("NV16 paint ytex");

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[1]);
	switch (format) {
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED, GL_UNSIGNED_BYTE,
				m_curData[0] ? m_curData[0] + m_v4l_fmt.g_sizeimage() / 2 : NULL);
		break;
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED, GL_UNSIGNED_BYTE, m_curData[1]);
		break;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("NV16 paint uvtex");
}

void CaptureWin::render_YUY2(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width() / 2, m_v4l_fmt.g_height(),
			GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("YUY2 paint");
}

void CaptureWin::render_RGB(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_RGB332:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_BYTE_3_3_2, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB444:
	case V4L2_PIX_FMT_XRGB444:
	case V4L2_PIX_FMT_ARGB444:
	case V4L2_PIX_FMT_XBGR444:
	case V4L2_PIX_FMT_ABGR444:
	case V4L2_PIX_FMT_RGBX444:
	case V4L2_PIX_FMT_RGBA444:
	case V4L2_PIX_FMT_BGRX444:
	case V4L2_PIX_FMT_BGRA444:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, m_curData[0]);
		break;

	case V4L2_PIX_FMT_GREY:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline());
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, m_curData[0]);
		break;

	case V4L2_PIX_FMT_Y10:
	case V4L2_PIX_FMT_Y12:
	case V4L2_PIX_FMT_Y16:
	case V4L2_PIX_FMT_Z16:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;
	case V4L2_PIX_FMT_Y16_BE:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB555:
	case V4L2_PIX_FMT_XRGB555:
	case V4L2_PIX_FMT_ARGB555:
	case V4L2_PIX_FMT_XBGR555:
	case V4L2_PIX_FMT_ABGR555:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB555X:
	case V4L2_PIX_FMT_XRGB555X:
	case V4L2_PIX_FMT_ARGB555X:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB555 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGBX555:
	case V4L2_PIX_FMT_RGBA555:
	case V4L2_PIX_FMT_BGRX555:
	case V4L2_PIX_FMT_BGRA555:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_5_5_5_1, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB565:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB565X:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 2);
		// Note: most likely for big-endian systems SWAP_BYTES should be true
		// for the RGB565 format, and false for this format. This would have
		// to be tested first, though.
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_FALSE);
		break;

	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_XRGB32:
	case V4L2_PIX_FMT_ARGB32:
	case V4L2_PIX_FMT_RGBX32:
	case V4L2_PIX_FMT_RGBA32:
	case V4L2_PIX_FMT_HSV32:
	case V4L2_PIX_FMT_BGR32:
	case V4L2_PIX_FMT_XBGR32:
	case V4L2_PIX_FMT_ABGR32:
	case V4L2_PIX_FMT_BGRX32:
	case V4L2_PIX_FMT_BGRA32:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	case V4L2_PIX_FMT_BGR666:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 4);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
	case V4L2_PIX_FMT_HSV24:
	default:
		glPixelStorei(GL_UNPACK_ROW_LENGTH, m_v4l_fmt.g_bytesperline() / 3);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	}
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	checkError("RGB paint");
}

void CaptureWin::render_Bayer(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_SBGGR8:
	case V4L2_PIX_FMT_SGBRG8:
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SRGGB8:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	case V4L2_PIX_FMT_SBGGR10:
	case V4L2_PIX_FMT_SGBRG10:
	case V4L2_PIX_FMT_SGRBG10:
	case V4L2_PIX_FMT_SRGGB10:
	case V4L2_PIX_FMT_SBGGR12:
	case V4L2_PIX_FMT_SGBRG12:
	case V4L2_PIX_FMT_SGRBG12:
	case V4L2_PIX_FMT_SRGGB12:
	case V4L2_PIX_FMT_SBGGR16:
	case V4L2_PIX_FMT_SGBRG16:
	case V4L2_PIX_FMT_SGRBG16:
	case V4L2_PIX_FMT_SRGGB16:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RED_INTEGER, GL_UNSIGNED_SHORT, m_curData[0]);
		break;
	}
	checkError("Bayer paint");
}

void CaptureWin::render_YUV_packed(__u32 format)
{
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, m_screenTexture[0]);

	switch (format) {
	case V4L2_PIX_FMT_YUV555:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV444:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV565:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGB, GL_UNSIGNED_SHORT_5_6_5, m_curData[0]);
		break;

	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_v4l_fmt.g_width(), m_v4l_fmt.g_height(),
				GL_RGBA, GL_UNSIGNED_BYTE, m_curData[0]);
		break;
	}
	checkError("Packed YUV paint");
}
