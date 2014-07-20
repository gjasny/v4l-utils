/* qv4l2: a control panel controlling v4l2 devices.
 *
 * Copyright (C) 2012 Hans Verkuil <hverkuil@xs4all.nl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Vbi Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Vbi Public License for more details.
 *
 * You should have received a copy of the GNU Vbi Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */


#ifndef VBI_TAB_H
#define VBI_TAB_H

#include "qv4l2.h"

class QTableWidget;

class VbiTab: public QGridLayout
{
	Q_OBJECT

public:
	VbiTab(QWidget *parent = 0);
	virtual ~VbiTab() {}

	void rawFormat(const v4l2_vbi_format &fmt);
	void slicedFormat(const v4l2_sliced_vbi_format &fmt);
	void slicedData(const v4l2_sliced_vbi_data *data, unsigned elems);

private:
	void info(const QString &info)
	{
		g_mw->info(info);
	}
	virtual void error(const QString &error)
	{
		g_mw->error(error);
	}
	void tableFormat();

	QTableWidget *m_tableF1;
	QTableWidget *m_tableF2;
	unsigned m_startF1, m_startF2;
	unsigned m_countF1, m_countF2;
	unsigned m_offsetF2;
};

#endif
