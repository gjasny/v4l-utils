/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef CAPTURE_WIN_GL_H
#define CAPTURE_WIN_GL_H

#define GL_GLEXT_PROTOTYPES 1
#define QT_NO_OPENGL_ES_2

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QFile>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QScrollArea>
#include <QtGui/QOpenGLShaderProgram>

#include "qvidcap.h"

extern "C" {
#include "v4l2-tpg.h"
#include "v4l-stream.h"
}

extern const __u32 formats[];
extern const __u32 fields[];
extern const __u32 colorspaces[];
extern const __u32 xfer_funcs[];
extern const __u32 ycbcr_encs[];
extern const __u32 hsv_encs[];
extern const __u32 quantizations[];

class QOpenGLPaintDevice;

enum AppMode {
	AppModeV4L2,
	AppModeFile,
	AppModeSocket,
	AppModeTPG,
	AppModeTest
};

#define FMT_MASK		(1 << 0)
#define FIELD_MASK		(1 << 1)
#define COLORSPACE_MASK		(1 << 2)
#define XFER_FUNC_MASK		(1 << 3)
#define YCBCR_HSV_ENC_MASK	(1 << 4)
#define QUANT_MASK		(1 << 5)

struct TestState {
	unsigned fmt_idx;
	unsigned field_idx;
	unsigned colorspace_idx;
	unsigned xfer_func_idx;
	unsigned ycbcr_enc_idx;
	unsigned hsv_enc_idx;
	unsigned quant_idx;
	unsigned mask;
};

// This must be equal to the max number of textures that any shader uses
#define MAX_TEXTURES_NEEDED 3

class CaptureGLWin : public QOpenGLWidget, protected QOpenGLFunctions
{
	Q_OBJECT
public:
	explicit CaptureGLWin(QScrollArea *sa, QWidget *parent = 0);
	~CaptureGLWin();

	void setModeV4L2(cv4l_fd *fd);
	void setModeSocket(int sock, int port);
	void setModeFile(const QString &filename);
	void setModeTPG();
	void setModeTest(unsigned cnt);
	void setQueue(cv4l_queue *q);
	bool setV4LFormat(cv4l_fmt &fmt);
	void setPixelAspect(v4l2_fract &pixelaspect);
	bool updateV4LFormat(const cv4l_fmt &fmt);
	void setOverrideWidth(__u32 w);
	void setOverrideHeight(__u32 h);
	void setCount(unsigned cnt) { m_cnt = cnt; }
	void setReportTimings(bool report) { m_reportTimings = report; }
	void setVerbose(bool verbose) { m_verbose = verbose; }
	void setOverridePixelFormat(__u32 fmt) { m_overridePixelFormat = fmt; }
	void setOverrideField(__u32 field) { m_overrideField = field; }
	void setOverrideColorspace(__u32 colsp) { m_overrideColorspace = colsp; }
	void setOverrideYCbCrEnc(__u32 ycbcr) { m_overrideYCbCrEnc = ycbcr; }
	void setOverrideHSVEnc(__u32 hsv) { m_overrideHSVEnc = hsv; }
	void setOverrideXferFunc(__u32 xfer_func) { m_overrideXferFunc = xfer_func; }
	void setOverrideQuantization(__u32 quant) { m_overrideQuantization = quant; }
	void setFps(double fps) { m_fps = fps; }
	void setSingleStepStart(unsigned start) { m_singleStep = true; m_singleStepStart = start; }
	void setTestState(const TestState &state) { m_testState = state; }
	QSize correctAspect(const QSize &s) const;
	void startTimer();
	struct tpg_data *getTPG() { return &m_tpg; }

private slots:
	void v4l2ReadEvent();
	void v4l2ExceptionEvent();
	void sockReadEvent();
	void tpgUpdateFrame();

	void restoreAll(bool checked);
	void restoreSize(bool checked = false);
	void fmtChanged(QAction *a);
	void fieldChanged(QAction *a);
	void colorspaceChanged(QAction *a);
	void xferFuncChanged(QAction *a);
	void ycbcrEncChanged(QAction *a);
	void hsvEncChanged(QAction *a);
	void quantChanged(QAction *a);
	void windowScalingChanged(QAction *a);
	void resolutionOverrideChanged(bool);
	void toggleFullScreen(bool b = false);

private:
	void resizeEvent(QResizeEvent *event);
	void paintGL();
	void initializeGL();
	void resizeGL(int w, int h);
	void contextMenuEvent(QContextMenuEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * e);
	void listenForNewConnection();
	int read_u32(__u32 &v);
	void showCurrentOverrides();
	void cycleMenu(__u32 &overrideVal, __u32 &origVal,
		       const __u32 values[], bool hasShift, bool hasCtrl);

	bool supportedFmt(__u32 fnt);
	void checkError(const char *msg);
	void configureTexture(size_t idx);
	void initImageFormat();
	void updateOrigValues();
	void updateShader();
	void changeShader();

	// Colorspace conversion shaders
	void shader_YUV();
	void shader_NV12();
	void shader_NV16();
	void shader_NV24();
	void shader_RGB();
	void shader_Bayer();
	void shader_YUV_packed();
	void shader_YUY2();

	// Colorspace conversion render
	void render_RGB(__u32 format);
	void render_Bayer(__u32 format);
	void render_YUY2(__u32 format);
	void render_YUV(__u32 format);
	void render_YUV_packed(__u32 format);
	void render_NV12(__u32 format);
	void render_NV16(__u32 format);
	void render_NV24(__u32 format);

	enum AppMode m_mode;
	cv4l_fd *m_fd;
	int m_sock;
	int m_port;
	QFile m_file;
	bool m_v4l2;
	cv4l_fmt m_v4l_fmt;
	v4l2_fract m_pixelaspect;
	cv4l_queue *m_v4l_queue;
	unsigned m_cnt;
	unsigned m_frame;
	unsigned m_test;
	TestState m_testState;
	unsigned m_imageSize;
	bool m_verbose;
	bool m_reportTimings;
	bool m_is_sdtv;
	v4l2_std_id m_std;
	bool m_is_rgb;
	bool m_is_hsv;
	bool m_is_bayer;
	bool m_uses_gl_red;
	bool m_accepts_srgb;
	bool m_haveSwapBytes;
	bool m_updateShader;
	QSize m_viewSize;
	bool m_canOverrideResolution;
	codec_ctx *m_ctx;

	__u32 m_overridePixelFormat;
	__u32 m_overrideWidth;
	__u32 m_overrideHeight;
	__u32 m_overrideField;
	__u32 m_overrideColorspace;
	__u32 m_overrideYCbCrEnc;
	__u32 m_overrideHSVEnc;
	__u32 m_overrideXferFunc;
	__u32 m_overrideQuantization;
	__u32 m_origPixelFormat;
	__u32 m_origWidth;
	__u32 m_origHeight;
	__u32 m_origField;
	__u32 m_origColorspace;
	__u32 m_origYCbCrEnc;
	__u32 m_origHSVEnc;
	__u32 m_origXferFunc;
	__u32 m_origQuantization;
	double m_fps;
	bool m_singleStep;
	unsigned m_singleStepStart;
	bool m_singleStepNext;

	QTimer *m_timer;
	int m_screenTextureCount;
	GLuint m_screenTexture[MAX_TEXTURES_NEEDED];
	QOpenGLShaderProgram *m_program;
	__u8 *m_curData[MAX_TEXTURES_NEEDED];
	unsigned m_curSize[MAX_TEXTURES_NEEDED];
	__u8 *m_nextData[MAX_TEXTURES_NEEDED];
	unsigned m_nextSize[MAX_TEXTURES_NEEDED];
	int m_curIndex;
	int m_nextIndex;
	struct tpg_data m_tpg;

	QScrollArea *m_scrollArea;
	QAction *m_resolutionOverride;
	QAction *m_exitFullScreen;
	QAction *m_enterFullScreen;
	QMenu *m_fmtMenu;
	QMenu *m_fieldMenu;
	QMenu *m_colorspaceMenu;
	QMenu *m_xferFuncMenu;
	QMenu *m_ycbcrEncMenu;
	QMenu *m_hsvEncMenu;
	QMenu *m_quantMenu;
	QMenu *m_displayMenu;
};

#endif
