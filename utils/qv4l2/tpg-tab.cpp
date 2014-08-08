/*
 * qv4l2 - Test Pattern Generator Tab
 *
 * Copyright 2014 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
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

#include "qv4l2.h"

#include <QFrame>
#include <QVBoxLayout>
#include <QStatusBar>
#include <QLineEdit>
#include <QValidator>
#include <QLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QToolTip>

#include <math.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

void ApplicationWindow::addTpgTab(int m_winWidth)
{
	QWidget *t = new QWidget(m_tabs);
	QVBoxLayout *vbox = new QVBoxLayout(t);
	QWidget *w = new QWidget(t);
	QCheckBox *check;
	QComboBox *combo;
	QSpinBox *spin;

	m_col = m_row = 0;
	m_cols = 4;
	for (int j = 0; j < m_cols; j++) {
		m_maxw[j] = 0;
	}

	vbox->addWidget(w);

	QGridLayout *grid = new QGridLayout(w);
	QLabel *title_tab = new QLabel("Test Pattern Generator", parentWidget());
	QFont f = title_tab->font();
	f.setBold(true);
	title_tab->setFont(f);
	grid->addWidget(title_tab, m_row, m_col, 1, m_cols, Qt::AlignLeft);
	grid->setRowMinimumHeight(m_row, 25);
	m_row++;

	QFrame *m_line = new QFrame(grid->parentWidget());
	m_line->setFrameShape(QFrame::HLine);
	m_line->setFrameShadow(QFrame::Sunken);
	grid->addWidget(m_line, m_row, m_col, 1, m_cols, Qt::AlignVCenter);
	m_row++;

	m_tabs->addTab(t, "Test Pattern Generator");
	grid->addWidget(new QWidget(w), grid->rowCount(), 0, 1, m_cols);

	addLabel(grid, "Test Pattern");
	combo = new QComboBox(w);
	for (int i = 0; tpg_pattern_strings[i]; i++)
		combo->addItem(tpg_pattern_strings[i]);
	addWidget(grid, combo);
	connect(combo, SIGNAL(activated(int)), SLOT(testPatternChanged(int)));

	m_row++;
	m_col = 0;

	addLabel(grid, "Horizontal Movement");
	combo = new QComboBox(w);
	combo->addItem("Move Left Fast");
	combo->addItem("Move Left");
	combo->addItem("Move Left Slow");
	combo->addItem("No Movement");
	combo->addItem("Move Right Slow");
	combo->addItem("Move Right");
	combo->addItem("Move Right Fast");
	combo->setCurrentIndex(3);
	addWidget(grid, combo);
	connect(combo, SIGNAL(activated(int)), SLOT(horMovementChanged(int)));

	addLabel(grid, "Video Aspect Ratio");
	combo = new QComboBox(w);
	combo->addItem("Source Width x Height");
	combo->addItem("4x3");
	combo->addItem("14x9");
	combo->addItem("16x9");
	combo->addItem("16x9 Anamorphic");
	addWidget(grid, combo);
	connect(combo, SIGNAL(activated(int)), SLOT(videoAspectRatioChanged(int)));

	addLabel(grid, "Vertical Movement");
	combo = new QComboBox(w);
	combo->addItem("Move Up Fast");
	combo->addItem("Move Up");
	combo->addItem("Move Up Slow");
	combo->addItem("No Movement");
	combo->addItem("Move Down Slow");
	combo->addItem("Move Down");
	combo->addItem("Move Down Fast");
	combo->setCurrentIndex(3);
	addWidget(grid, combo);
	connect(combo, SIGNAL(activated(int)), SLOT(vertMovementChanged(int)));

	m_tpgColorspace = 0;
	addLabel(grid, "Colorspace");
	combo = new QComboBox(w);
	combo->addItem("Autodetect");
	combo->addItem("SMPTE 170M");
	combo->addItem("SMPTE 240M");
	combo->addItem("REC 709");
	combo->addItem("470 System M");
	combo->addItem("470 System BG");
	combo->addItem("sRGB");
	addWidget(grid, combo);
	connect(combo, SIGNAL(activated(int)), SLOT(colorspaceChanged(int)));

	addLabel(grid, "Show Border");
	check = new QCheckBox(w);
	addWidget(grid, check);
	connect(check, SIGNAL(stateChanged(int)), SLOT(showBorderChanged(int)));

	addLabel(grid, "Insert SAV Code in Image");
	check = new QCheckBox(w);
	addWidget(grid, check);
	connect(check, SIGNAL(stateChanged(int)), SLOT(insSAVChanged(int)));

	addLabel(grid, "Show Square");
	check = new QCheckBox(w);
	addWidget(grid, check);
	connect(check, SIGNAL(stateChanged(int)), SLOT(showSquareChanged(int)));

	addLabel(grid, "Insert EAV Code in Image");
	check = new QCheckBox(w);
	addWidget(grid, check);
	connect(check, SIGNAL(stateChanged(int)), SLOT(insEAVChanged(int)));

	addLabel(grid, "Fill Percentage of Frame");
	spin = new QSpinBox(w);
	spin->setRange(0, 100);
	spin->setValue(100);
	addWidget(grid, spin);
	connect(spin, SIGNAL(valueChanged(int)), SLOT(fillPercentageChanged(int)));

	addLabel(grid, "Limited RGB Range (16-235)");
	m_tpgLimRGBRange = new QCheckBox(w);
	addWidget(grid, m_tpgLimRGBRange);
	connect(m_tpgLimRGBRange, SIGNAL(stateChanged(int)), SLOT(limRGBRangeChanged(int)));

	addLabel(grid, "Alpha Component");
	spin = new QSpinBox(w);
	spin->setRange(0, 255);
	spin->setValue(0);
	addWidget(grid, spin);
	connect(spin, SIGNAL(valueChanged(int)), SLOT(alphaComponentChanged(int)));

	addLabel(grid, "Apply Alpha To Red Only");
	check = new QCheckBox(w);
	addWidget(grid, check);
	connect(check, SIGNAL(stateChanged(int)), SLOT(applyToRedChanged(int)));

	m_row++;
	m_col = 0;
	addWidget(grid, new QWidget(w));
	grid->setRowStretch(grid->rowCount() - 1, 1);
	w = new QWidget(t);
	vbox->addWidget(w);
	fixWidth(grid);

	int totalw = 0;
	int diff = 0;
	for (int i = 0; i < m_cols; i++) {
		totalw += m_maxw[i] + m_pxw;
	}
	if (totalw > m_winWidth)
		m_winWidth = totalw;
	else {
		diff = m_winWidth - totalw;
		grid->setHorizontalSpacing(diff/5);
	}
}

void ApplicationWindow::testPatternChanged(int val)
{
	tpg_s_pattern(&m_tpg, (tpg_pattern)val);
}

void ApplicationWindow::horMovementChanged(int val)
{
	tpg_s_mv_hor_mode(&m_tpg, (tpg_move_mode)val);
}

void ApplicationWindow::vertMovementChanged(int val)
{
	tpg_s_mv_vert_mode(&m_tpg, (tpg_move_mode)val);
}

void ApplicationWindow::showBorderChanged(int val)
{
	tpg_s_show_border(&m_tpg, val);
}

void ApplicationWindow::showSquareChanged(int val)
{
	tpg_s_show_square(&m_tpg, val);
}

void ApplicationWindow::insSAVChanged(int val)
{
	tpg_s_insert_sav(&m_tpg, val);
}

void ApplicationWindow::insEAVChanged(int val)
{
	tpg_s_insert_eav(&m_tpg, val);
}

void ApplicationWindow::videoAspectRatioChanged(int val)
{
	tpg_s_video_aspect(&m_tpg, (tpg_video_aspect)val);
}

void ApplicationWindow::updateLimRGBRange()
{
	if (m_tpgLimRGBRange == NULL)
		return;

	v4l2_output out;

	g_output(out.index);
	enum_output(out, true, out.index);

	if (out.capabilities & V4L2_OUT_CAP_STD) {
		m_tpgLimRGBRange->setChecked(false);
	} else if (out.capabilities & V4L2_OUT_CAP_DV_TIMINGS) {
		v4l2_dv_timings timings;

		g_dv_timings(timings);
		if (timings.bt.standards & V4L2_DV_BT_STD_CEA861)
			m_tpgLimRGBRange->setChecked(true);
		else
			m_tpgLimRGBRange->setChecked(false);
	} else {
		m_tpgLimRGBRange->setChecked(false);
	}
}

__u32 ApplicationWindow::defaultColorspace(bool capture)
{
	v4l2_dv_timings timings = { 0 };
	v4l2_output out;
	v4l2_input in;
	__u32 io_caps;
	bool dvi_d = false;

	if (capture) {
		g_input(in.index);
		enum_input(in, true, in.index);
		io_caps = in.capabilities;
	} else {
		g_output(out.index);
		enum_output(out, true, out.index);
		io_caps = out.capabilities;
		v4l2_control ctrl = { V4L2_CID_DV_TX_MODE };

		if (!g_ctrl(ctrl))
			dvi_d = ctrl.value == V4L2_DV_TX_MODE_DVI_D;
	}

	if (io_caps & V4L2_OUT_CAP_STD)
		return V4L2_COLORSPACE_SMPTE170M;
	if (!(io_caps & V4L2_OUT_CAP_DV_TIMINGS))
		return V4L2_COLORSPACE_SRGB;

	g_dv_timings(timings);

	if (!(timings.bt.standards & V4L2_DV_BT_STD_CEA861) || dvi_d)
		return V4L2_COLORSPACE_SRGB;
	if (timings.bt.width == 720 && timings.bt.height <= 576)
		return V4L2_COLORSPACE_SMPTE170M;
	return V4L2_COLORSPACE_REC709;
}

void ApplicationWindow::colorspaceChanged(int val)
{
	switch (val) {
	case 0:
		m_tpgColorspace = 0;
		break;
	case 1:
		m_tpgColorspace = V4L2_COLORSPACE_SMPTE170M;
		break;
	case 2:
		m_tpgColorspace = V4L2_COLORSPACE_SMPTE240M;
		break;
	case 3:
		m_tpgColorspace = V4L2_COLORSPACE_REC709;
		break;
	case 4:
		m_tpgColorspace = V4L2_COLORSPACE_470_SYSTEM_M;
		break;
	case 5:
		m_tpgColorspace = V4L2_COLORSPACE_470_SYSTEM_BG;
		break;
	case 6:
	default:
		m_tpgColorspace = V4L2_COLORSPACE_SRGB;
		break;
	}

	cv4l_fmt fmt;
	v4l2_output out;

	g_output(out.index);
	enum_output(out, true, out.index);

	g_fmt(fmt);
	if (m_tpgColorspace == 0)
		fmt.s_colorspace(defaultColorspace(false));
	else
		fmt.s_colorspace(m_tpgColorspace);
	s_fmt(fmt);
	tpg_s_colorspace(&m_tpg, m_tpgColorspace ? m_tpgColorspace : fmt.g_colorspace());
}

void ApplicationWindow::limRGBRangeChanged(int val)
{
	tpg_s_real_rgb_range(&m_tpg, val ? V4L2_DV_RGB_RANGE_LIMITED : V4L2_DV_RGB_RANGE_FULL);
}

void ApplicationWindow::fillPercentageChanged(int val)
{
	tpg_s_perc_fill(&m_tpg, val);
}

void ApplicationWindow::alphaComponentChanged(int val)
{
	tpg_s_alpha_component(&m_tpg, val);
}

void ApplicationWindow::applyToRedChanged(int val)
{
	tpg_s_alpha_mode(&m_tpg, val);
}
