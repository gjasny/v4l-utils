
#include "qv4l2.h"
#include "libv4l2util.h"

#include <qstatusbar.h>
#include <qlineedit.h>
#include <qvalidator.h>
#include <qlayout.h>
#include <qvbox.h>
#include <qhbox.h>
#include <qlabel.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qcombobox.h>
#include <qcheckbox.h>
#include <qpushbutton.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>

void ApplicationWindow::addTabs()
{
	struct v4l2_queryctrl qctrl;
	unsigned ctrl_class;
	unsigned i;
	int id;

	memset(&qctrl, 0, sizeof(qctrl));
	qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
	while (::ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
		if ((qctrl.flags & V4L2_CTRL_FLAG_DISABLED) == 0) {
			ctrlMap[qctrl.id] = qctrl;
			if (qctrl.type != V4L2_CTRL_TYPE_CTRL_CLASS)
				classMap[V4L2_CTRL_ID2CLASS(qctrl.id)].push_back(qctrl.id);
		}
		qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
	}
	if (qctrl.id == V4L2_CTRL_FLAG_NEXT_CTRL) {
		strcpy((char *)qctrl.name, "User Controls");
		qctrl.id = V4L2_CTRL_CLASS_USER | 1;
		qctrl.type = V4L2_CTRL_TYPE_CTRL_CLASS;
		ctrlMap[qctrl.id] = qctrl;
		for (id = V4L2_CID_USER_BASE; id < V4L2_CID_LASTP1; id++) {
			qctrl.id = id;
			if (::ioctl(fd, VIDIOC_QUERYCTRL, &qctrl))
				continue;
			if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			ctrlMap[qctrl.id] = qctrl;
			classMap[V4L2_CTRL_CLASS_USER].push_back(qctrl.id);
		}
		for (qctrl.id = V4L2_CID_PRIVATE_BASE;
				::ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0; qctrl.id++) {
			if (qctrl.flags & V4L2_CTRL_FLAG_DISABLED)
				continue;
			ctrlMap[qctrl.id] = qctrl;
			classMap[V4L2_CTRL_CLASS_USER].push_back(qctrl.id);
		}
	}

	for (ClassMap::iterator iter = classMap.begin(); iter != classMap.end(); ++iter) {
		ctrl_class = V4L2_CTRL_ID2CLASS(iter->second[0]);
		id = ctrl_class | 1;
		const struct v4l2_queryctrl &qctrl = ctrlMap[id];
		QVBox *vbox = new QVBox(tabs);
		QGrid *grid = new QGrid(4, vbox);
		grid->setSpacing(3);
		tabs->addTab(vbox, (char *)qctrl.name);
		for (i = 0; i < iter->second.size(); i++) {
			if (i & 1)
				id = iter->second[(1+iter->second.size()) / 2 + i / 2];
			else
				id = iter->second[i / 2];
			addCtrl(grid, ctrlMap[id]);
		}
		finishGrid(vbox, grid, ctrl_class, i & 1);
	}
}

void ApplicationWindow::finishGrid(QWidget *vbox, QGrid *grid, unsigned ctrl_class, bool odd)
{
	if (odd) {
		new QWidget(grid);
		new QWidget(grid);
	}
	QWidget *stretch = new QWidget(grid);
	stretch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);

	QFrame *frame = new QFrame(vbox);
	frame->setFrameShape(QFrame::HLine);
	frame->setFrameShadow(QFrame::Sunken);
	frame->setMargin(3);

	QHBox *hbox = new QHBox(vbox);
	hbox->setSpacing(3);

	QCheckBox *cbox = new QCheckBox("Update on change", hbox);
	widgetMap[ctrl_class | CTRL_UPDATE_ON_CHANGE] = cbox;
	connect(cbox, SIGNAL(clicked()), sigMapper, SLOT(map()));
	sigMapper->setMapping(cbox, ctrl_class | CTRL_UPDATE_ON_CHANGE);

	stretch = new QWidget(hbox);
	stretch->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);

	QPushButton *defBut = new QPushButton("Set Defaults", hbox);
	widgetMap[ctrl_class | CTRL_DEFAULTS] = defBut;
	connect(defBut, SIGNAL(clicked()), sigMapper, SLOT(map()));
	sigMapper->setMapping(defBut, ctrl_class | CTRL_DEFAULTS);

	QPushButton *refreshBut = new QPushButton("Refresh", hbox);
	widgetMap[ctrl_class | CTRL_REFRESH] = refreshBut;
	connect(refreshBut, SIGNAL(clicked()), sigMapper, SLOT(map()));
	sigMapper->setMapping(refreshBut, ctrl_class | CTRL_REFRESH);

	QPushButton *button = new QPushButton("Update", hbox);
	widgetMap[ctrl_class | CTRL_UPDATE] = button;
	connect(button, SIGNAL(clicked()), sigMapper, SLOT(map()));
	sigMapper->setMapping(button, ctrl_class | CTRL_UPDATE);
	connect(cbox, SIGNAL(toggled(bool)), button, SLOT(setDisabled(bool)));

	cbox->setChecked(ctrl_class == V4L2_CTRL_CLASS_USER);

	refresh(ctrl_class);
}

void ApplicationWindow::addCtrl(QGrid *grid, const struct v4l2_queryctrl &qctrl)
{
	QIntValidator *val;
	QLineEdit *edit;
	QString name((char *)qctrl.name);
	QComboBox *combo;
	struct v4l2_querymenu qmenu;

	QLabel *label = new QLabel(name, grid);
	label->setAlignment(Qt::AlignRight);

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		if (qctrl.flags & V4L2_CTRL_FLAG_SLIDER) {
			widgetMap[qctrl.id] =
				new QSlider(qctrl.minimum, qctrl.maximum,
					qctrl.step, qctrl.default_value,
					Horizontal, grid);
			connect(widgetMap[qctrl.id], SIGNAL(valueChanged(int)),
				sigMapper, SLOT(map()));
			break;
		}

		if (qctrl.maximum - qctrl.minimum <= 255) {
			widgetMap[qctrl.id] =
				new QSpinBox(qctrl.minimum, qctrl.maximum, 1, grid);
			connect(widgetMap[qctrl.id], SIGNAL(valueChanged(int)),
				sigMapper, SLOT(map()));
			break;
		}

		val = new QIntValidator(qctrl.minimum, qctrl.maximum, grid);
		edit = new QLineEdit(grid);
		edit->setValidator(val);
		widgetMap[qctrl.id] = edit;
		connect(widgetMap[qctrl.id], SIGNAL(lostFocus()),
				sigMapper, SLOT(map()));
		connect(widgetMap[qctrl.id], SIGNAL(returnPressed()),
				sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_INTEGER64:
		widgetMap[qctrl.id] = new QLineEdit(grid);
		connect(widgetMap[qctrl.id], SIGNAL(lostFocus()),
				sigMapper, SLOT(map()));
		connect(widgetMap[qctrl.id], SIGNAL(returnPressed()),
				sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		label->setText("");
		widgetMap[qctrl.id] = new QCheckBox(name, grid);
		connect(widgetMap[qctrl.id], SIGNAL(clicked()),
				sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_BUTTON:
		label->setText("");
		widgetMap[qctrl.id] = new QPushButton((char *)qctrl.name, grid);
		connect(widgetMap[qctrl.id], SIGNAL(clicked()),
				sigMapper, SLOT(map()));
		break;

	case V4L2_CTRL_TYPE_MENU:
		combo = new QComboBox(grid);
		widgetMap[qctrl.id] = combo;
		for (int i = qctrl.minimum; i <= qctrl.maximum; i++) {
			qmenu.id = qctrl.id;
			qmenu.index = i;
			if (::ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			combo->insertItem((char *)qmenu.name);
		}
		connect(widgetMap[qctrl.id], SIGNAL(activated(int)),
				sigMapper, SLOT(map()));
		break;

	default:
		return;
	}
	sigMapper->setMapping(widgetMap[qctrl.id], qctrl.id);
	if (qctrl.flags & (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_INACTIVE))
		widgetMap[qctrl.id]->setDisabled(true);
}

void ApplicationWindow::ctrlAction(int id)
{
	unsigned ctrl_class = V4L2_CTRL_ID2CLASS(id);
	if (ctrl_class == V4L2_CID_PRIVATE_BASE)
		ctrl_class = V4L2_CTRL_CLASS_USER;
	unsigned ctrl = id & 0xffff;
	QCheckBox *cbox = static_cast<QCheckBox *>(widgetMap[ctrl_class | CTRL_UPDATE_ON_CHANGE]);
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
	if (!update && !all && ctrlMap[id].type != V4L2_CTRL_TYPE_BUTTON)
		return;
	if (ctrl_class == V4L2_CTRL_CLASS_USER) {
		if (!all) {
			updateCtrl(id);
			return;
		}
		for (unsigned i = 0; i < classMap[ctrl_class].size(); i++) {
			updateCtrl(classMap[ctrl_class][i]);
		}
		return;
	}
	if (!all) {
		updateCtrl(id);
		return;
	}
	unsigned count = classMap[ctrl_class].size();
	struct v4l2_ext_control *c = new v4l2_ext_control[count];
	struct v4l2_ext_controls ctrls;
	int idx = 0;

	for (unsigned i = 0; i < count; i++) {
		unsigned id = classMap[ctrl_class][i];

		if (ctrlMap[id].flags & (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_INACTIVE))
			continue;
		c[idx].id = id;
		if (ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
			c[idx].value64 = getVal64(id);
		else
			c[idx].value = getVal(id);
		idx++;
	}
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = idx;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = c;
	if (::ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
		int err = errno;

		if (ctrls.error_idx >= ctrls.count) {
			printf("error: %s\n", strerror(err));
		}
		else {
			id = c[ctrls.error_idx].id;
			printf("error %08x (%s): %s\n", id,
					ctrlMap[id].name, strerror(err));
		}
	}
	delete [] c;
	refresh(ctrl_class);
}

long long ApplicationWindow::getVal64(unsigned id)
{
	const v4l2_queryctrl &qctrl = ctrlMap[id];
	QWidget *w = widgetMap[qctrl.id];
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
	const v4l2_queryctrl &qctrl = ctrlMap[id];
	QWidget *w = widgetMap[qctrl.id];
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

	case V4L2_CTRL_TYPE_MENU:
		idx = static_cast<QComboBox *>(w)->currentItem();
		for (i = qctrl.minimum; i <= qctrl.maximum; i++) {
			qmenu.id = qctrl.id;
			qmenu.index = i;
			if (::ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
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

	if (ctrlMap[id].flags & (V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_INACTIVE))
		return;

	if (ctrl_class == V4L2_CTRL_CLASS_USER) {
		struct v4l2_control c;

		c.id = id;
		c.value = getVal(id);
		if (::ioctl(fd, VIDIOC_S_CTRL, &c)) {
			int err = errno;
			char buf[200];

			sprintf(buf, "Error %08x (%s): %s", id,
				ctrlMap[id].name, strerror(err));
			statusBar()->message(buf, 10000);
		}
		return;
	}
	struct v4l2_ext_control c;
	struct v4l2_ext_controls ctrls;

	memset(&c, 0, sizeof(c));
	memset(&ctrls, 0, sizeof(ctrls));
	c.id = id;
	if (ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
		c.value64 = getVal64(id);
	else
		c.value = getVal(id);
	ctrls.count = 1;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = &c;
	if (::ioctl(fd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
		int err = errno;
		char buf[200];

		sprintf(buf, "Error %08x (%s): %s", id,
				ctrlMap[id].name, strerror(err));
		statusBar()->message(buf, 10000);
	}
	else if (ctrlMap[id].flags & V4L2_CTRL_FLAG_UPDATE)
		refresh(ctrl_class);
	else {
		if (ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
			setVal64(id, c.value64);
		else
			setVal(id, c.value);
	}
}

void ApplicationWindow::refresh(unsigned ctrl_class)
{
	if (ctrl_class == V4L2_CTRL_CLASS_USER) {
		for (unsigned i = 0; i < classMap[ctrl_class].size(); i++) {
			unsigned id = classMap[ctrl_class][i];

			v4l2_control c;

			c.id = id;
			if (::ioctl(fd, VIDIOC_G_CTRL, &c)) {
				int err = errno;
				char buf[200];

				sprintf(buf, "Error %08x (%s): %s", id,
						ctrlMap[id].name, strerror(err));
				statusBar()->message(buf, 10000);
			}
			setVal(id, c.value);
		}
		return;
	}
	unsigned count = classMap[ctrl_class].size();
	struct v4l2_ext_control *c = new v4l2_ext_control[count];
	struct v4l2_ext_controls ctrls;

	for (unsigned i = 0; i < count; i++) {
		c[i].id = classMap[ctrl_class][i];
	}
	memset(&ctrls, 0, sizeof(ctrls));
	ctrls.count = count;
	ctrls.ctrl_class = ctrl_class;
	ctrls.controls = c;
	if (::ioctl(fd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
		int err = errno;

		if (ctrls.error_idx >= ctrls.count) {
			statusBar()->message(strerror(err), 10000);
		}
		else {
			unsigned id = c[ctrls.error_idx].id;
			char buf[200];

			sprintf(buf, "Error %08x (%s): %s", id,
					ctrlMap[id].name, strerror(err));
			statusBar()->message(buf, 10000);
		}
	}
	else {
		for (unsigned i = 0; i < ctrls.count; i++) {
			unsigned id = c[i].id;
			if (ctrlMap[id].type == V4L2_CTRL_TYPE_INTEGER64)
				setVal64(id, c[i].value64);
			else
				setVal(id, c[i].value);
			::ioctl(fd, VIDIOC_QUERYCTRL, &ctrlMap[id]);
			widgetMap[id]->setDisabled(ctrlMap[id].flags &
				(V4L2_CTRL_FLAG_READ_ONLY | V4L2_CTRL_FLAG_INACTIVE));
		}
	}
	delete [] c;
}

void ApplicationWindow::setWhat(QWidget *w, unsigned id, long long v)
{
	const v4l2_queryctrl &qctrl = ctrlMap[id];
	QString what;
	QString flags = getCtrlFlags(qctrl.flags);

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER:
		QWhatsThis::add(w, what.sprintf("Integer type control\n"
					"Minimum: %d\n"
					"Maximum: %d\n"
					"Current: %d\n"
					"Default: %d\n",
			qctrl.minimum, qctrl.maximum, (int)v, qctrl.default_value) + flags);
		break;

	case V4L2_CTRL_TYPE_INTEGER64:
		QWhatsThis::add(w, what.sprintf("64-bit Integer type control\n"
					"Current: %lld\n", v) + flags);
		break;

	case V4L2_CTRL_TYPE_BUTTON:
		QWhatsThis::add(w, what.sprintf("Button type control\n") + flags);
		break;

	case V4L2_CTRL_TYPE_BOOLEAN:
		QWhatsThis::add(w, what.sprintf("Boolean type control\n"
					"Current: %d\n"
					"Default: %d\n",
			(int)v, qctrl.default_value) + flags);
		break;

	case V4L2_CTRL_TYPE_MENU:
		QWhatsThis::add(w, what.sprintf("Menu type control\n"
					"Minimum: %d\n"
					"Maximum: %d\n"
					"Current: %d\n"
					"Default: %d\n",
			qctrl.minimum, qctrl.maximum, (int)v, qctrl.default_value) + flags);
		break;
	default:
		break;
	}
}

void ApplicationWindow::setVal(unsigned id, int v)
{
	const v4l2_queryctrl &qctrl = ctrlMap[id];
	v4l2_querymenu qmenu;
	QWidget *w = widgetMap[qctrl.id];
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

	case V4L2_CTRL_TYPE_BOOLEAN:
		static_cast<QCheckBox *>(w)->setChecked(v);
		break;

	case V4L2_CTRL_TYPE_MENU:
		idx = 0;
		for (i = qctrl.minimum; i <= v; i++) {
			qmenu.id = id;
			qmenu.index = i;
			if (::ioctl(fd, VIDIOC_QUERYMENU, &qmenu))
				continue;
			idx++;
		}
		static_cast<QComboBox *>(w)->setCurrentItem(idx - 1);
		break;
	default:
		break;
	}
	setWhat(w, id, v);
}

void ApplicationWindow::setVal64(unsigned id, long long v)
{
	const v4l2_queryctrl &qctrl = ctrlMap[id];
	QWidget *w = widgetMap[qctrl.id];

	switch (qctrl.type) {
	case V4L2_CTRL_TYPE_INTEGER64:
		static_cast<QLineEdit *>(w)->setText(QString::number(v));
		break;
	default:
		break;
	}
	setWhat(w, id, v);
}

void ApplicationWindow::setDefaults(unsigned ctrl_class)
{
	for (unsigned i = 0; i < classMap[ctrl_class].size(); i++) {
		unsigned id = classMap[ctrl_class][i];

		if (ctrlMap[id].type != V4L2_CTRL_TYPE_INTEGER64 &&
		    ctrlMap[id].type != V4L2_CTRL_TYPE_BUTTON)
			setVal(id, ctrlMap[id].default_value);
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
	if (flags & V4L2_CTRL_FLAG_SLIDER)
		s += "slider ";
	if (s.length()) s = QString("Flags: ") + s;
	return s;
}

