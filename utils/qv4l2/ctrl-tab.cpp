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
#include <QToolTip>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

static bool is_valid_type(__u32 type)
{
	switch (type) {
	case V4L2_CTRL_TYPE_INTEGER:
	case V4L2_CTRL_TYPE_BOOLEAN:
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
	case V4L2_CTRL_TYPE_BUTTON:
	case V4L2_CTRL_TYPE_INTEGER64:
	case V4L2_CTRL_TYPE_BITMASK:
	case V4L2_CTRL_TYPE_CTRL_CLASS:
	case V4L2_CTRL_TYPE_STRING:
		return true;
	default:
		return false;
	}
}

void ApplicationWindow::addWidget(QGridLayout *grid, QWidget *w, Qt::Alignment align)
{
	grid->addWidget(w, m_row, m_col, align | Qt::AlignVCenter);
	m_col++;
	if (m_col == m_cols) {
		m_col = 0;
		m_row++;
	}
}

void ApplicationWindow::addTabs()
{
	v4l2_queryctrl qctrl;
	unsigned ctrl_class;
	unsigned i;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (queryctrl(qctrl)) {
		if (is_valid_type(qctrl.type) &&
		    (qctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0) {
			m_ctrlMap[qctrl.id] = qctrl;
			if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS)
				m_classMap[V4L2_CTRL_ID2CLASS(qctrl.id)].push_back(qctrl.id);
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id == V4L2_CTRL_FLAG_NEXT_CTRL) {
		strcpy((char *)qctrl.name, "User Controls");
		qctrl.id = V4L2_CTRL_CLASS_USER | 1;
		qctrl.type = V4L2_CTRL_TYPE_CTRL_CLASS;
		m_ctrlMap[qctrl.id] = qctrl;
		for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
			qctrl.id = id;
			if (!queryctrl(qctrl))
				continue;
			if (!is_valid_type(qctrl.type))
				continue;
			if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			m_ctrlMap[qctrl.id] = qctrl;
			m_classMap[V4L2_CTRL_CLASS_USER].push_back(qctrl.id);
		}
		for (qctrl.id = V4L2_CID_PRIVATE_BASE;
				queryctrl(qctrl); qctrl.id++) {
			if (!is_valid_type(qctrl.type))
				continue;
			if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			m_ctrlMap[qctrl.id] = qctrl;
			m_classMap[V4L2_CTRL_CLASS_USER].push_back(qctrl.id);
		}
	}
	
	m_haveExtendedUserCtrls = false;
	for (unsigned i = 0; i < m_classMap[V4L2_CTRL_CLASS_USER].size(); i++) {
		unsigned id = m_classMap[V4L2_CTRL_CLASS_USER][i];

		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64 ||
		    m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING ||
		    V4L2_CTRL_DRIVER_PRIV(id)) {
			m_haveExtendedUserCtrls = true;
			break;
		}
	}

	for (ClassMap::iterator iter = m_classMap.begin(); iter != m_classMap.end(); ++iter) {
		if (iter->second.size() == 0)
			continue;
		ctrl_class = V4L2_CTRL_ID2CLASS(iter->second[0]);
		id = ctrl_class | 1;
		m_col = m_row = 0;
		m_cols = 4;

		const v4l2_queryctrl &qctrl = m_ctrlMap[id];
		QWidget *t = new QWidget(m_tabs);
		QVBoxLayout *vbox = new QVBoxLayout(t);
		QWidget *w = new QWidget(t);

		vbox->addWidget(w);

		QGridLayout *grid = new QGridLayout(w);

		grid->setSpacing(3);
		m_tabs->addTab(t, (char *)qctrl.name);
		for (i = 0; i < iter->second.size(); i++) {
			if (i & 1)
				id = iter->second[(1+iter->second.size()) / 2 + i / 2];
			else
				id = iter->second[i / 2];
			addCtrl(grid, m_ctrlMap[id]);
		}
		grid->addWidget(new QWidget(w), grid->rowCount(), 0, 1, m_cols);
		grid->setRowStretch(grid->rowCount() - 1, 1);
		w = new QWidget(t);
		vbox->addWidget(w);
		grid = new QGridLayout(w);
		finishGrid(grid, ctrl_class);
	}
}

void ApplicationWindow::finishGrid(QGridLayout *grid, unsigned ctrl_class)
{
	QWidget *w = grid->parentWidget();

	QFrame *frame = new QFrame(w);
	frame->setFrameShape(QFrame::HLine);
	frame->setFrameShadow(QFrame::Sunken);
	grid->addWidget(frame, 0, 0, 1, m_cols);
	m_col = 0;
	m_row = grid->rowCount();

	QCheckBox *cbox = new QCheckBox("Update on change", w);
	m_widgetMap[ctrl_class | CTRL_UPDATE_ON_CHANGE] = cbox;
	addWidget(grid, cbox);
	connect(cbox, SIGNAL(clicked()), m_sigMapper, SLOT(map()));
	m_sigMapper->setMapping(cbox, ctrl_class | CTRL_UPDATE_ON_CHANGE);

	grid->setColumnStretch(0, 1);

	QPushButton *defBut = new QPushButton("Set Defaults", w);
	m_widgetMap[ctrl_class | CTRL_DEFAULTS] = defBut;
	addWidget(grid, defBut);
	connect(defBut, SIGNAL(clicked()), m_sigMapper, SLOT(map()));
	m_sigMapper->setMapping(defBut, ctrl_class | CTRL_DEFAULTS);

	QPushButton *refreshBut = new QPushButton("Refresh", w);
	m_widgetMap[ctrl_class | CTRL_REFRESH] = refreshBut;
	addWidget(grid, refreshBut);
	connect(refreshBut, SIGNAL(clicked()), m_sigMapper, SLOT(map()));
	m_sigMapper->setMapping(refreshBut, ctrl_class | CTRL_REFRESH);

	QPushButton *button = new QPushButton("Update", w);
	m_widgetMap[ctrl_class | CTRL_UPDATE] = button;
	addWidget(grid, button);
	connect(button, SIGNAL(clicked()), m_sigMapper, SLOT(map()));
	m_sigMapper->setMapping(button, ctrl_class | CTRL_UPDATE);
	connect(cbox, SIGNAL(toggled(bool)), button, SLOT(setDisabled(bool)));

	cbox->setChecked(true);

	refresh(ctrl_class);
}

void ApplicationWindow::addCtrl(QGridLayout *grid, const v4l2_queryctrl &qctrl)
{
	QWidget *p = grid->parentWidget();
	QIntValidator *val;
	QLineEdit *edit;
	QString name((char *)qctrl.name);
	QComboBox *combo;
	QSpinBox *spin;
	QSlider *slider;
	struct v4l2_querymenu qmenu;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		addLabel(grid, name);
		if (qctrl.flags & V4L2_CTRL_FLAG_SLIDER) {
			m_widgetMap[qctrl.id] = slider = new QSlider(Qt::Horizontal, p);
			slider->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
			slider->setMinimum(qctrl.minimum);
			slider->setMaximum(qctrl.maximum);
			slider->setSingleStep(qctrl.step);
			slider->setSliderPosition(qctrl.default_value);
			addWidget(grid, m_widgetMap[qctrl.id]);
			connect(m_widgetMap[qctrl.id], SIGNAL(valueChanged(int)),
				m_sigMapper, SLOT(map()));
			break;
		}

		if (qctrl.maximum - qctrl.minimum <= 255) {
			m_widgetMap[qctrl.id] = spin = new QSpinBox(p);
			spin->setMinimum(qctrl.minimum);
			spin->setMaximum(qctrl.maximum);
			spin->setSingleStep(qctrl.step);
			addWidget(grid, m_widgetMap[qctrl.id]);
			connect(m_widgetMap[qctrl.id], SIGNAL(valueChanged(int)),
				m_sigMapper, SLOT(map()));
			break;
		}

		val = new QIntValidator(qctrl.minimum, qctrl.maximum, p);
		edit = new QLineEdit(p);
		edit->setValidator(val);
		addWidget(grid, edit);
		m_widgetMap[qctrl.id] = edit;
		connect(m_widgetMap[qctrl.id], SIGNAL(lostFocus()),
				m_sigMapper, SLOT(map()));
		connect(m_widgetMap[qctrl.id], SIGNAL(returnPressed()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_INTEGER64:
		addLabel(grid, name);
		edit = new QLineEdit(p);
		m_widgetMap[qctrl.id] = edit;
		addWidget(grid, edit);
		connect(m_widgetMap[qctrl.id], SIGNAL(lostFocus()),
				m_sigMapper, SLOT(map()));
		connect(m_widgetMap[qctrl.id], SIGNAL(returnPressed()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_BITMASK:
		addLabel(grid, name);
		edit = new QLineEdit(p);
		edit->setInputMask("HHHHHHHH");
		addWidget(grid, edit);
		m_widgetMap[qctrl.id] = edit;
		connect(m_widgetMap[qctrl.id], SIGNAL(lostFocus()),
				m_sigMapper, SLOT(map()));
		connect(m_widgetMap[qctrl.id], SIGNAL(returnPressed()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_STRING:
		addLabel(grid, name);
		edit = new QLineEdit(p);
		m_widgetMap[qctrl.id] = edit;
		edit->setMaxLength(qctrl.maximum);
		addWidget(grid, edit);
		connect(m_widgetMap[qctrl.id], SIGNAL(lostFocus()),
				m_sigMapper, SLOT(map()));
		connect(m_widgetMap[qctrl.id], SIGNAL(returnPressed()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		addLabel(grid, name);
		m_widgetMap[qctrl.id] = new QCheckBox(p);
		addWidget(grid, m_widgetMap[qctrl.id]);
		connect(m_widgetMap[qctrl.id], SIGNAL(clicked()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_BUTTON:
		addLabel(grid, "");
		m_widgetMap[qctrl.id] = new QPushButton((char *)qctrl.name, p);
		addWidget(grid, m_widgetMap[qctrl.id]);
		connect(m_widgetMap[qctrl.id], SIGNAL(clicked()),
				m_sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		addLabel(grid, name);
		combo = new QComboBox(p);
		m_widgetMap[qctrl.id] = combo;
		for (int i = qctrl.minimum; i <= qctrl.maximum; i++) {
			qmenu.id = qctrl.id;
			qmenu.index = i;
			if (!querymenu(qmenu))
				continue;
			if (qctrl.type == V4L2_CTRL_TYPE_MENU)
				combo->addItem((char *)qmenu.name);
			else
				combo->addItem(QString("%1").arg(qmenu.value));
		}
		addWidget(grid, m_widgetMap[qctrl.id]);
		connect(m_widgetMap[qctrl.id], SIGNAL(activated(int)),
				m_sigMapper, SLOT(map()));
		break;

	default:
		return;
	}
	struct v4l2_event_subscription sub;
	memset(&sub, 0, sizeof(sub));
	sub.type = V4L2_EVENT_CTRL;
	sub.id = qctrl.id;
	subscribe_event(sub);

	m_sigMapper->setMapping(m_widgetMap[qctrl.id], qctrl.id);
	if (qctrl.flags & CTRL_FLAG_DISABLED)
		m_widgetMap[qctrl.id]->setDisabled(true);
}

void ApplicationWindow::ctrlAction(int id)
{
	unsigned ctrl_class = V4L2_CTRL_ID2CLASS(id);
	if (ctrl_class == V4L2_CID_PRIVATE_BASE)
		ctrl_class = V4L2_CTRL_CLASS_USER;
	unsigned ctrl = id & 0xffff;
	QCheckBox *cbox = static_cast<QCheckBox *>(m_widgetMap[ctrl_class | CTRL_UPDATE_ON_CHANGE]);
	bool update = cbox->isChecked();
	bool all = (ctrl == CTRL_UPDATE || (update && ctrl == CTRL_UPDATE_ON_CHANGE));

	if (ctrl == CTRL_DEFAULTS) {
		setDefaults(ctrl_class);
		return;
	}
	if (ctrl == CTRL_REFRESH) {
		refresh(ctrl_class);
		return;
	}
	if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER &&
	    (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_SLIDER)) {
		QWidget *w = m_widgetMap[id];
		int v = static_cast<QSlider *>(w)->value();
		info(QString("Value: %1").arg(v));
	}
	if (!update && !all && m_ctrlMap[id].type != V4L2_CTRL_TYPE_BUTTON)
		return;
	if (!m_haveExtendedUserCtrls && ctrl_class == V4L2_CTRL_CLASS_USER) {
		if (!all) {
			updateCtrl(id);
			return;
		}
		for (unsigned i = 0; i < m_classMap[ctrl_class].size(); i++) {
			updateCtrl(m_classMap[ctrl_class][i]);
		}
		return;
	}
	if (!all) {
		updateCtrl(id);
		return;
	}
	unsigned count = m_classMap[ctrl_class].size();
	struct v4l2_ext_control *c = new v4l2_ext_control[count];
	struct v4l2_ext_controls ctrls;
	int idx = 0;

	for (unsigned i = 0; i < count; i++) {
		unsigned id = m_classMap[ctrl_class][i];

		if (m_ctrlMap[id].flags & CTRL_FLAG_DISABLED)
			continue;
		c[idx].id = id;
		c[idx].size = 0;
		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
			c[idx].value64 = getVal64(id);
		else if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING) {
			c[idx].size = m_ctrlMap[id].maximum + 1;
			c[idx].string = (char *)malloc(c[idx].size);
			strcpy(c[idx].string, getString(id).toLatin1());
		}
		else
			c[idx].value = getVal(id);
		idx++;
	}
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = idx;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = c;
	if (ioctl(VIDIOC_S_EXT_CTRLS, &ctrls)) {
		if (ctrls.error_idx >= ctrls.count) {
			error(errno);
		}
		else {
			errorCtrl(c[ctrls.error_idx].id, errno);
		}
	}
	for (unsigned i = 0; i < ctrls.count; i++) {
		if (c[i].size)
			free(c[i].string);
	}
	delete [] c;
	refresh(ctrl_class);
}

QString ApplicationWindow::getString(unsigned id)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QWidget *w = m_widgetMap[qctrl.id];
	QString v;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_STRING:
		v = static_cast<QLineEdit *>(w)->text();
		break;
	default:
		break;
	}
	setWhat(w, id, v);
	return v;
}

long long ApplicationWindow::getVal64(unsigned id)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QWidget *w = m_widgetMap[qctrl.id];
	long long v = 0;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		v = static_cast<QLineEdit *>(w)->text().toLongLong();
		break;
	default:
		break;
	}
	setWhat(w, id, v);
	return v;
}

int ApplicationWindow::getVal(unsigned id)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QWidget *w = m_widgetMap[qctrl.id];
	v4l2_querymenu qmenu;
	int i, idx;
	int v = 0;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		if (qctrl.flags & V4L2_CTRL_FLAG_SLIDER) {
			v = static_cast<QSlider *>(w)->value();
			break;
		}

		if (qctrl.maximum - qctrl.minimum <= 255) {
			v = static_cast<QSpinBox *>(w)->value();
			break;
		}
		v = static_cast<QLineEdit *>(w)->text().toInt();
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		v = static_cast<QCheckBox *>(w)->isChecked();
		break;

	case V4L2_CTRL_TYPE_BITMASK:
		v = (int)static_cast<QLineEdit *>(w)->text().toUInt(0, 16);
		break;
	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		idx = static_cast<QComboBox *>(w)->currentIndex();
		for (i = qctrl.minimum; i <= qctrl.maximum; i++) {
			qmenu.id = qctrl.id;
			qmenu.index = i;
			if (!querymenu(qmenu))
				continue;
			if (idx-- == 0)
				break;
		}
		v = i;
		break;

	default:
		break;
	}
	setWhat(w, id, v);
	return v;
}

void ApplicationWindow::updateCtrl(unsigned id)
{
	unsigned ctrl_class = V4L2_CTRL_ID2CLASS(id);
	if (ctrl_class == V4L2_CID_PRIVATE_BASE)
		ctrl_class = V4L2_CTRL_CLASS_USER;

	if (m_ctrlMap[id].flags & CTRL_FLAG_DISABLED)
		return;

	if (!m_haveExtendedUserCtrls && ctrl_class == V4L2_CTRL_CLASS_USER) {
		struct v4l2_control c;

		c.id = id;
		c.value = getVal(id);
		if (ioctl(VIDIOC_S_CTRL, &c)) {
			errorCtrl(id, errno, c.value);
		}
		else if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_UPDATE)
			refresh(ctrl_class);
		return;
	}
	struct v4l2_ext_control c;
	struct v4l2_ext_controls ctrls;

	memset(&c, 0, sizeof(c));
	memset(&ctrls, 0, sizeof(ctrls));
	c.id = id;
	if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
		c.value64 = getVal64(id);
	else if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING) {
		c.size = m_ctrlMap[id].maximum + 1;
		c.string = (char *)malloc(c.size);
		strcpy(c.string, getString(id).toLatin1());
	}
	else
		c.value = getVal(id);
	ctrls.count = 1;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = &c;
	if (ioctl(VIDIOC_S_EXT_CTRLS, &ctrls)) {
		errorCtrl(id, errno, c.value);
	}
	else if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_UPDATE)
		refresh(ctrl_class);
	else {
		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
			setVal64(id, c.value64);
		else if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING) {
			setString(id, c.string);
			free(c.string);
		}
		else
			setVal(id, c.value);
	}
}

void ApplicationWindow::refresh(unsigned ctrl_class)
{
	if (!m_haveExtendedUserCtrls && ctrl_class == V4L2_CTRL_CLASS_USER) {
		for (unsigned i = 0; i < m_classMap[ctrl_class].size(); i++) {
			unsigned id = m_classMap[ctrl_class][i];
			v4l2_control c;

			queryctrl(m_ctrlMap[id]);
			if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_BUTTON)
				continue;
			if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_WRITE_ONLY)
				continue;
			c.id = id;
			if (ioctl(VIDIOC_G_CTRL, &c)) {
				errorCtrl(id, errno);
			}
			setVal(id, c.value);
			m_widgetMap[id]->setDisabled(m_ctrlMap[id].flags & CTRL_FLAG_DISABLED);
		}
		return;
	}
	unsigned count = m_classMap[ctrl_class].size();
	unsigned cnt = 0;
	struct v4l2_ext_control *c = new v4l2_ext_control[count];
	struct v4l2_ext_controls ctrls;

	for (unsigned i = 0; i < count; i++) {
		unsigned id = c[cnt].id = m_classMap[ctrl_class][i];
		
		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_BUTTON)
			continue;
		if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_WRITE_ONLY)
			continue;
		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING) {
			c[cnt].size = m_ctrlMap[id].maximum + 1;
			c[cnt].string = (char *)malloc(c[cnt].size);
		}
		cnt++;
	}
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = cnt;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = c;
	if (ioctl(VIDIOC_G_EXT_CTRLS, &ctrls)) {
		if (ctrls.error_idx >= ctrls.count) {
			error(errno);
		}
		else {
			errorCtrl(c[ctrls.error_idx].id, errno);
		}
	}
	else {
		for (unsigned i = 0; i < ctrls.count; i++) {
			unsigned id = c[i].id;
			
			queryctrl(m_ctrlMap[id]);
			if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
				setVal64(id, c[i].value64);
			else if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING) {
				setString(id, c[i].string);
				free(c[i].string);
			}
			else
				setVal(id, c[i].value);
			m_widgetMap[id]->setDisabled(m_ctrlMap[id].flags & CTRL_FLAG_DISABLED);
		}
	}
	delete [] c;
}

void ApplicationWindow::refresh()
{
	for (ClassMap::iterator iter = m_classMap.begin(); iter != m_classMap.end(); ++iter)
		refresh(iter->first);
}

void ApplicationWindow::setWhat(QWidget *w, unsigned id, const QString &v)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QString what;
	QString flags = getCtrlFlags(qctrl.flags);

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_STRING:
		w->setWhatsThis(QString("Type: String\n"
					"Minimum: %1\n"
					"Maximum: %2\n"
					"Step: %3\n"
					"Current: %4")
			.arg(qctrl.minimum).arg(qctrl.maximum).arg(qctrl.step).arg(v) + flags);
		w->setStatusTip(w->whatsThis());
		break;
	default:
		break;
	}
}

void ApplicationWindow::setWhat(QWidget *w, unsigned id, long long v)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QString what;
	QString flags = getCtrlFlags(qctrl.flags);

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		w->setWhatsThis(QString("Type: Integer\n"
					"Minimum: %1\n"
					"Maximum: %2\n"
					"Step: %3\n"
					"Current: %4\n"
					"Default: %5")
			.arg(qctrl.minimum).arg(qctrl.maximum).arg(qctrl.step).arg(v).arg(qctrl.default_value) + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_INTEGER64:
		w->setWhatsThis(QString("Type: Integer64\n"
					"Current: %1").arg(v) + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_BITMASK:
		w->setWhatsThis(QString("Type: Bitmask\n"
					"Maximum: %1\n"
					"Current: %2\n"
					"Default: %3")
			.arg((unsigned)qctrl.maximum, 0, 16).arg((unsigned)v, 0, 16)
			.arg((unsigned)qctrl.default_value, 0, 16) + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_BUTTON:
		w->setWhatsThis(QString("Type: Button") + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		w->setWhatsThis(QString("Type: Boolean\n"
					"Current: %1\n"
					"Default: %2")
			.arg(v).arg(qctrl.default_value) + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_MENU:
		w->setWhatsThis(QString("Type: Menu\n"
					"Minimum: %1\n"
					"Maximum: %2\n"
					"Current: %3\n"
					"Default: %4")
			.arg(qctrl.minimum).arg(qctrl.maximum).arg(v).arg(qctrl.default_value) + flags);
		w->setStatusTip(w->whatsThis());
		break;

	case V4L2_CTRL_TYPE_INTEGER_MENU:
		w->setWhatsThis(QString("Type: Integer Menu\n"
					"Minimum: %1\n"
					"Maximum: %2\n"
					"Current: %3\n"
					"Default: %4")
			.arg(qctrl.minimum).arg(qctrl.maximum).arg(v).arg(qctrl.default_value) + flags);
		w->setStatusTip(w->whatsThis());
		break;
	default:
		break;
	}
}

void ApplicationWindow::setVal(unsigned id, int v)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	v4l2_querymenu qmenu;
	QWidget *w = m_widgetMap[qctrl.id];
	int i, idx;

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		if (qctrl.flags & V4L2_CTRL_FLAG_SLIDER)
			static_cast<QSlider *>(w)->setValue(v);
		else if (qctrl.maximum - qctrl.minimum <= 255)
			static_cast<QSpinBox *>(w)->setValue(v);
		else
			static_cast<QLineEdit *>(w)->setText(QString::number(v));
		break;

	case V4L2_CTRL_TYPE_BITMASK:
		static_cast<QLineEdit *>(w)->setText(QString("%1").arg((unsigned)v, 8, 16, QChar('0')));
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		static_cast<QCheckBox *>(w)->setChecked(v);
		break;

	case V4L2_CTRL_TYPE_MENU:
	case V4L2_CTRL_TYPE_INTEGER_MENU:
		idx = 0;
		for (i = qctrl.minimum; i <= v; i++) {
			qmenu.id = id;
			qmenu.index = i;
			if (!querymenu(qmenu))
				continue;
			idx++;
		}
		static_cast<QComboBox *>(w)->setCurrentIndex(idx - 1);
		break;
	default:
		break;
	}
	setWhat(w, id, v);
}

void ApplicationWindow::setVal64(unsigned id, long long v)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QWidget *w = m_widgetMap[qctrl.id];

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		static_cast<QLineEdit *>(w)->setText(QString::number(v));
		break;
	default:
		break;
	}
	setWhat(w, id, v);
}

void ApplicationWindow::setString(unsigned id, const QString &v)
{
	const v4l2_queryctrl &qctrl = m_ctrlMap[id];
	QWidget *w = m_widgetMap[qctrl.id];

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_STRING:
		static_cast<QLineEdit *>(w)->setText(v);
		break;
	default:
		break;
	}
	setWhat(w, id, v);
}

void ApplicationWindow::setDefaults(unsigned ctrl_class)
{
	for (unsigned i = 0; i < m_classMap[ctrl_class].size(); i++) {
		unsigned id = m_classMap[ctrl_class][i];

		if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_READ_ONLY)
			continue;
		if (m_ctrlMap[id].flags & V4L2_CTRL_FLAG_GRABBED)
			continue;
		if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
			setVal64(id, 0);
		else if (m_ctrlMap[id].type == V4L2_CTRL_TYPE_STRING)
			setString(id, QString(m_ctrlMap[id].minimum, ' '));
		else if (m_ctrlMap[id].type != V4L2_CTRL_TYPE_BUTTON)
			setVal(id, m_ctrlMap[id].default_value);
	}
	ctrlAction(ctrl_class | CTRL_UPDATE);
}

QString ApplicationWindow::getCtrlFlags(unsigned flags)
{
	QString s;

	if (flags & V4L2_CTRL_FLAG_GRABBED)
		s += "grabbed ";
	if (flags & V4L2_CTRL_FLAG_READ_ONLY)
		s += "readonly ";
	if (flags & V4L2_CTRL_FLAG_UPDATE)
		s += "update ";
	if (flags & V4L2_CTRL_FLAG_INACTIVE)
		s += "inactive ";
	if (flags & V4L2_CTRL_FLAG_VOLATILE)
		s += "volatile ";
	if (flags & V4L2_CTRL_FLAG_SLIDER)
		s += "slider ";
	if (s.length()) s = QString("\nFlags: ") + s;
	return s;
}

