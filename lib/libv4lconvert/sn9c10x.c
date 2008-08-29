/*
# sonix decoder
#               Bertrik.Sikken. (C) 2005

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# Note this code was originally licensed under the GNU GPL instead of the
# GNU LGPL, its license has been changed with permission, see the permission
# mail at the end of this file.
*/

#include "libv4lconvert-priv.h"

#define CLAMP(x)	((x)<0?0:((x)>255)?255:(x))

typedef struct {
	int is_abs;
	int len;
	int val;
	int unk;
} code_table_t;


/* local storage */
/* FIXME not thread safe !! */
static code_table_t table[256];
static int init_done = 0;

/* global variable */
static int sonix_unknown = 0;

/*
	sonix_decompress_init
	=====================
		pre-calculates a locally stored table for efficient huffman-decoding.

	Each entry at index x in the table represents the codeword
	present at the MSB of byte x.

*/
static void sonix_decompress_init(void)
{
	int i;
	int is_abs, val, len, unk;

	for (i = 0; i < 256; i++) {
		is_abs = 0;
		val = 0;
		len = 0;
		unk = 0;
		if ((i & 0x80) == 0) {
			/* code 0 */
			val = 0;
			len = 1;
		}
		else if ((i & 0xE0) == 0x80) {
			/* code 100 */
			val = +4;
			len = 3;
		}
		else if ((i & 0xE0) == 0xA0) {
			/* code 101 */
			val = -4;
			len = 3;
		}
		else if ((i & 0xF0) == 0xD0) {
			/* code 1101 */
			val = +11;
			len = 4;
		}
		else if ((i & 0xF0) == 0xF0) {
			/* code 1111 */
			val = -11;
			len = 4;
		}
		else if ((i & 0xF8) == 0xC8) {
			/* code 11001 */
			val = +20;
			len = 5;
		}
		else if ((i & 0xFC) == 0xC0) {
			/* code 110000 */
			val = -20;
			len = 6;
		}
		else if ((i & 0xFC) == 0xC4) {
			/* code 110001xx: unknown */
			val = 0;
			len = 8;
			unk = 1;
		}
		else if ((i & 0xF0) == 0xE0) {
			/* code 1110xxxx */
			is_abs = 1;
			val = (i & 0x0F) << 4;
			len = 8;
		}
		table[i].is_abs = is_abs;
		table[i].val = val;
		table[i].len = len;
		table[i].unk = unk;
	}

	sonix_unknown = 0;
	init_done = 1;
}


/*
	sonix_decompress
	================
		decompresses an image encoded by a SN9C101 camera controller chip.

	IN	width
		height
		inp		pointer to compressed frame (with header already stripped)
	OUT	outp	pointer to decompressed frame

	Returns 0 if the operation was successful.
	Returns <0 if operation failed.

*/
void v4lconvert_decode_sn9c10x(const unsigned char *inp, unsigned char *outp,
	int width, int height)
{
	int row, col;
	int val;
	int bitpos;
	unsigned char code;
	const unsigned char *addr;

	if (!init_done)
		sonix_decompress_init();

	bitpos = 0;
	for (row = 0; row < height; row++) {

		col = 0;

		/* first two pixels in first two rows are stored as raw 8-bit */
		if (row < 2) {
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));
			bitpos += 8;
			*outp++ = code;

			col += 2;
		}

		while (col < width) {
			/* get bitcode from bitstream */
			addr = inp + (bitpos >> 3);
			code = (addr[0] << (bitpos & 7)) | (addr[1] >> (8 - (bitpos & 7)));

			/* update bit position */
			bitpos += table[code].len;

			/* update code statistics */
			sonix_unknown += table[code].unk;

			/* calculate pixel value */
			val = table[code].val;
			if (!table[code].is_abs) {
				/* value is relative to top and left pixel */
				if (col < 2) {
					/* left column: relative to top pixel */
					val += outp[-2*width];
				}
				else if (row < 2) {
					/* top row: relative to left pixel */
					val += outp[-2];
				}
				else {
					/* main area: average of left pixel and top pixel */
					val += (outp[-2] + outp[-2*width]) / 2;
				}
			}

			/* store pixel */
			*outp++ = CLAMP(val);
			col++;
		}
	}
}

/*
Return-Path: <bertrik@sikken.nl>
Received: from koko.hhs.nl ([145.52.2.16] verified)
  by hhs.nl (CommuniGate Pro SMTP 4.3.6)
  with ESMTP id 89132066 for j.w.r.degoede@hhs.nl; Thu, 03 Jul 2008 15:19:55 +0200
Received: from exim (helo=koko)
	by koko.hhs.nl with local-smtp (Exim 4.62)
	(envelope-from <bertrik@sikken.nl>)
	id 1KEOj5-0000nq-KR
	for j.w.r.degoede@hhs.nl; Thu, 03 Jul 2008 15:19:55 +0200
Received: from [192.87.102.69] (port=33783 helo=filter1-ams.mf.surf.net)
	by koko.hhs.nl with esmtp (Exim 4.62)
	(envelope-from <bertrik@sikken.nl>)
	id 1KEOj5-0000nj-7r
	for j.w.r.degoede@hhs.nl; Thu, 03 Jul 2008 15:19:55 +0200
Received: from cardassian.kabelfoon.nl (cardassian3.kabelfoon.nl [62.45.45.105])
	by filter1-ams.mf.surf.net (8.13.8/8.13.8/Debian-3) with ESMTP id m63DJsKW032598
	for <j.w.r.degoede@hhs.nl>; Thu, 3 Jul 2008 15:19:54 +0200
Received: from [192.168.1.1] (044-013-045-062.dynamic.caiway.nl [62.45.13.44])
	by cardassian.kabelfoon.nl (Postfix) with ESMTP id 77761341D9A
	for <j.w.r.degoede@hhs.nl>; Thu,  3 Jul 2008 15:19:54 +0200 (CEST)
Message-ID: <486CD1F9.8000307@sikken.nl>
Date: Thu, 03 Jul 2008 15:19:53 +0200
From: Bertrik Sikken <bertrik@sikken.nl>
User-Agent: Thunderbird 2.0.0.14 (Windows/20080421)
MIME-Version: 1.0
To: Hans de Goede <j.w.r.degoede@hhs.nl>
Subject: Re: pac207 bayer decompression algorithm license question
References: <48633F02.3040108@hhs.nl> <4863F611.80104@sikken.nl> <486CC6AF.7050509@hhs.nl>
In-Reply-To: <486CC6AF.7050509@hhs.nl>
X-Enigmail-Version: 0.95.6
Content-Type: text/plain; charset=ISO-8859-1; format=flowed
Content-Transfer-Encoding: 7bit
X-Canit-CHI2: 0.00
X-Bayes-Prob: 0.0001 (Score 0, tokens from: @@RPTN)
X-Spam-Score: 0.00 () [Tag at 8.00]
X-CanItPRO-Stream: hhs:j.w.r.degoede@hhs.nl (inherits from hhs:default,base:default)
X-Canit-Stats-ID: 90943081 - 6a9ff19e8165
X-Scanned-By: CanIt (www . roaringpenguin . com) on 192.87.102.69
X-Anti-Virus: Kaspersky Anti-Virus for MailServers 5.5.2/RELEASE, bases: 03072008 #811719, status: clean

-----BEGIN PGP SIGNED MESSAGE-----
Hash: SHA1

Hans de Goede wrote:
| Bertrik Sikken wrote:
|> Hallo Hans,
|>
|> Hans de Goede wrote:
|>> I would like to also add support for decompressing the pac207's
|>> compressed
|>> bayer to this lib (and remove it from the kernel driver) and I've
|>> heard from Thomas Kaiser that you are a co-author of the
|>> decompression code. In order to add support for decompressing pac207
|>> compressed bayer to libv4l I need
|>> permission to relicense the decompression code under the LGPL
|>> (version 2 or later).
|>>
|>> Can you give me permission for this?
|>
|> Ja, vind ik goed.
|>
|
| Thanks!
|
| I'm currently working on adding support for the sn9c10x bayer
| compression to libv4l too, and I noticed this was written by you too.
|
| May I have your permission to relicense the sn9c10x bayer decompression
| code under the LGPL (version 2 or later)?

I hereby grant you permission to relicense the sn9c10x bayer
decompression code under the LGPL (version 2 or later).

Kind regards,
Bertrik
-----BEGIN PGP SIGNATURE-----
Version: GnuPG v1.4.7 (MingW32)
Comment: Using GnuPG with Mozilla - http://enigmail.mozdev.org

iD8DBQFIbNH5ETD6mlrWxPURAipvAJ9sv1ZpHyb81NMFejr6x0wqHX3i7QCfRDoB
jZi2e5lUjEh5KvS0dqXbi9I=
=KQfR
-----END PGP SIGNATURE-----
*/
