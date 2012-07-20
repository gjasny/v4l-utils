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


#include <stdint.h>
#include "vbi-tab.h"
#include <QTableWidget>

#include <stdio.h>
#include <errno.h>

VbiTab::VbiTab(QWidget *parent) :
	QGridLayout(parent)
{
	QStringList q;

	m_tableF1 = new QTableWidget(1, 1, parent);
	m_tableF2 = new QTableWidget(1, 1, parent);
	q.append("Field 1");
	m_tableF1->setHorizontalHeaderLabels(q);
	q.clear();
	q.append("Field 2");
	m_tableF2->setHorizontalHeaderLabels(q);
	q.clear();
	q.append("Line");
	m_tableF1->setVerticalHeaderLabels(q);
	m_tableF2->setVerticalHeaderLabels(q);
	addWidget(m_tableF1, 0, 0);
	addWidget(m_tableF2, 0, 1);
}

void VbiTab::tableFormat()
{
	QTableWidgetItem *item;
	QStringList q;
	unsigned i;

	m_tableF1->setRowCount(m_countF1);
	m_tableF2->setRowCount(m_countF2);
	for (i = 0; i < m_countF1; i++) {
		item = new QTableWidgetItem();
		item->setFlags(Qt::ItemIsEnabled);
		m_tableF1->setItem(i, 0, item);
		q.append("Line " + QString::number(i + m_startF1));
	}
	m_tableF1->setVerticalHeaderLabels(q);
	q.clear();
	for (i = 0; i < m_countF2; i++) {
		item = new QTableWidgetItem();
		item->setFlags(Qt::ItemIsEnabled);
		m_tableF2->setItem(i, 0, item);
		q.append("Line " + QString::number(i + m_startF2));
	}
	m_tableF2->setVerticalHeaderLabels(q);
}

void VbiTab::rawFormat(const v4l2_vbi_format &fmt)
{

	m_countF1 = fmt.count[0];
	m_countF2 = fmt.count[1];
	m_startF1 = fmt.start[0];
	m_startF2 = fmt.start[1];
	m_offsetF2 = m_startF2 >= 313 ? 313 : 263;
	tableFormat();
}

void VbiTab::slicedFormat(const v4l2_sliced_vbi_format &fmt)
{
	bool is_625 = fmt.service_set & V4L2_SLICED_VBI_625;

	m_startF1 = is_625 ? 6 : 10;
	m_startF2 = is_625 ? 319 : 273;
	m_offsetF2 = is_625 ? 313 : 263;
	m_countF1 = m_countF2 = is_625 ? 18 : 12;
	tableFormat();
}

static const char *formats[] = {
	"Full format 4:3, 576 lines",
	"Letterbox 14:9 centre, 504 lines",
	"Letterbox 14:9 top, 504 lines",
	"Letterbox 16:9 centre, 430 lines",
	"Letterbox 16:9 top, 430 lines",
	"Letterbox > 16:9 centre",
	"Full format 14:9 centre, 576 lines",
	"Anamorphic 16:9, 576 lines"
};

static const char *subtitles[] = {
	"none",
	"in active image area",
	"out of active image area",
	"?"
};

static void decode_wss(QTableWidgetItem *item, const struct v4l2_sliced_vbi_data *s)
{
	unsigned char parity;
	int wss;

	wss = s->data[0] | (s->data[1] << 8);

	parity = wss & 15;
	parity ^= parity >> 2;
	parity ^= parity >> 1;

	if (!(parity & 1)) {
		item->setStatusTip("");
		return;
	}

	item->setToolTip(QString(formats[wss & 7]) + "\n" + 
			((wss & 0x10) ? "Film" : "Camera") + " mode\n" +
			((wss & 0x20) ? "Motion Adaptive ColorPlus" : "Standard") + " color coding\n" +
			"Helper signals " + ((wss & 0x40) ? "" : "not ") + "present\n" +
			((wss & 0x0100) ? "Teletext subtitles\n" : "") +
			"Open subtitles: " + subtitles[(wss >> 9) & 3] + "\n" +
			((wss & 0x0800) ? "Surround sound\n" : "") +
			"Copyright " + ((wss & 0x1000) ? "asserted\n" : "unknown\n") +
			"Copying " + ((wss & 0x2000) ? "restricted" : "not restricted"));
}

const uint8_t vbi_bit_reverse[256] = {
	0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0,
	0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
	0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8,
	0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
	0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4,
	0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
	0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec,
	0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
	0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2,
	0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
	0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea,
	0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
	0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6,
	0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
	0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee,
	0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
	0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1,
	0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
	0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9,
	0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
	0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5,
	0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
	0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed,
	0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
	0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3,
	0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
	0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb,
	0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
	0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7,
	0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
	0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef,
	0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff
};

#define printable(c) ((((c) & 0x7F) < 0x20 || ((c) & 0x7F) > 0x7E) ? '.' : ((c) & 0x7F))

#define PIL(day, mon, hour, min) \
	(((day) << 15) + ((mon) << 11) + ((hour) << 6) + ((min) << 0))

static QString dump_pil(int pil)
{
	int day, mon, hour, min;
	char buf[50];

	day = pil >> 15;
	mon = (pil >> 11) & 0xF;
	hour = (pil >> 6) & 0x1F;
	min = pil & 0x3F;

	if (pil == PIL(0, 15, 31, 63))
		return QString("PDC: Timer-control (no PDC)");
	if (pil == PIL(0, 15, 30, 63))
		return QString("PDC: Recording inhibit/terminate");
	if (pil == PIL(0, 15, 29, 63))
		return QString("PDC: Interruption");
	if (pil == PIL(0, 15, 28, 63))
		return QString("PDC: Continue");
	if (pil == PIL(31, 15, 31, 63))
		return QString("PDC: No time");
	sprintf(buf, "PDC: 20XX-%02d-%02d %02d:%02d",
		       mon, day, hour, min);
	return buf;
}

static const char *pcs_text[] = {
	"unknown",
	"mono",
	"stereo",
	"dual sound",
};

static const char *pty_text[] = {
	/* 0x00 - 0x0f */
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	
	/* 0x10 - 0x1f */
	"movie (general)",
	"detective/thriller",
	"adventure/western/war",
	"science fiction/fantasy/horror",
	"comedy",
	"soap/melodrama/folklore",
	"romance",
	"serious/classical/religious/historical drama",
	"adult movie",
	NULL, NULL, NULL, NULL, NULL, NULL,
	"user defined",

	/* 0x20 - 0x2f */
	"news/current affairs (general)",
	"news/weather report",
	"news magazine",
	"documentary",
	"discussion/interview/debate",
	"social/political issues/economics (general)",
	"magazines/reports/documentary",
	"economics/social advisory",
	"remarkable people",
	NULL, NULL, NULL, NULL, NULL, NULL,
	"user defined",

	/* 0x30 - 0x3f */
	"show/game show (general)",
	"game show/quiz/contest",
	"variety show",
	"talk show",
	"leisure hobbies (general)",
	"tourism/travel",
	"handicraft",
	"motoring",
	"fitness and health",
	"cooking",
	"advertisement/shopping",
	NULL, NULL, NULL, NULL,
	"alarm/emergency identification",

	/* 0x40 - 0x4f */
	"sports (general)",
	"special events (Olympic Games, World Cup etc.)",
	"sports magazines",
	"football/soccer",
	"tennis/squash",
	"team sports (excluding football)",
	"athletics",
	"motor sport",
	"water sport",
	"winter sports",
	"equestrian",
	"martial sports",
	"local sports",
	NULL, NULL,
	"user defined",

	/* 0x50 - 0x5f */
	"children's/youth programmes (general)",
	"pre-school children's programmes",
	"entertainment programmes for 6 to 14",
	"entertainment programmes for 10 to 16",
	"informational/educational/school programmes",
	"cartoons/puppets",
	"education/science/factual topics (general)",
	"nature/animals/environment",
	"technology/natural sciences",
	"medicine/physiology/psychology",
	"foreign countries/expeditions",
	"social/spiritual sciences",
	"further education",
	"languages",
	NULL,
	"user defined",

	/* 0x60 - 0x6f */
	"music/ballet/dance (general)",
	"rock/pop",
	"serious music/classical music",
	"folk/traditional music",
	"jazz",
	"musical/opera",
	"ballet",
	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"user defined",

	/* 0x70 - 0x7f */
	"arts/culture (without music, general)",
	"performing arts",
	"fine arts",
	"religion",
	"popular culture/traditional arts",
	"literature",
	"film/cinema",
	"experimental film/video",
	"broadcasting/press",
	"new media",
	"arts/culture magazines",
	"fashion",
	NULL, NULL, NULL,
	"user defined",
};

static void decode_vps(QTableWidgetItem *item, const struct v4l2_sliced_vbi_data *s)
{
	QString q;
	int cni, pcs, pty, pil;
	const char *pty_txt;
	const unsigned char *buf = s->data;

	pcs = buf[2] >> 6;

	cni = +((buf[10] & 3) << 10)
	    + ((buf[11] & 0xC0) << 2)
	    + ((buf[8] & 0xC0) << 0)
	    + (buf[11] & 0x3F);

	pil = ((buf[8] & 0x3F) << 14) + (buf[9] << 6) + (buf[10] >> 2);

	pty = buf[12];
	if (pty > 0x7f)
		pty_txt = "service specific";
	else if (pty < 0x10)
		pty_txt = "undefined content";
	else
		pty_txt = pty_text[pty];
	if (pty_txt == NULL)
		pty_txt = "reserved for future use";

	q = "CNI: 0x";
	q += QString::number(cni, 16) + " PCS: " + pcs_text[pcs] + " PTY: " + pty_txt + "\n";
	item->setToolTip(q + dump_pil(pil));
}

static void setItem(QTableWidgetItem *item, const v4l2_sliced_vbi_data *data)
{
	switch (data->id) {
	case V4L2_SLICED_TELETEXT_B:
		item->setText("TXT");
		break;
	case V4L2_SLICED_VPS:
		item->setText("VPS");
		decode_vps(item, data);
		break;
	case V4L2_SLICED_CAPTION_525:
		item->setText("CC");
		break;
	case V4L2_SLICED_WSS_625:
		item->setText("WSS");
		decode_wss(item, data);
		break;
	default:
		item->setText("");
		break;
	}
}

void VbiTab::slicedData(const v4l2_sliced_vbi_data *data, unsigned elems)
{
	char found[m_countF1 + m_countF2];

	memset(found, 0, m_countF1 + m_countF2);
	for (unsigned i = 0; i < elems; i++) {
		QTableWidgetItem *item;
		
		if (data[i].id == 0)
			continue;
		if (data[i].field == 0) {
			if (data[i].line < m_startF1 ||
			    data[i].line >= m_startF1 + m_countF1)
				continue;
			item = m_tableF1->item(data[i].line - m_startF1, 0);
			found[data[i].line - m_startF1] = 1;
		} else {
			if (data[i].line + m_offsetF2 < m_startF2 ||
			    data[i].line + m_offsetF2 >= m_startF2 + m_countF2)
				continue;
			item = m_tableF2->item(data[i].line + m_offsetF2 - m_startF2, 0);
			found[data[i].line + m_offsetF2 - m_startF2 + m_countF1] = 1;
		}
		setItem(item, data + i);
	}
	for (unsigned i = 0; i < m_countF1 + m_countF2; i++) {
		if (found[i])
			continue;
		QTableWidgetItem *item = (i < m_countF1) ?
			m_tableF1->item(i, 0) : m_tableF2->item(i - m_countF1, 0);
		item->setText("");
	}
}
