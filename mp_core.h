/*

    Minimum Profit - Programmer Text Editor

    Copyright (C) 1991-2004 Angel Ortega <angel@triptico.com>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

    http://www.triptico.com

*/

struct mp_txt
{
	mpdm_t lines;	/* document content */
	int x;		/* x cursor position */
	int y;		/* y cursor position */
};

extern mpdm_t _mp;

mpdm_t mp_new(void);
void mp_move_up(mpdm_t t);
void mp_move_down(mpdm_t t);
void mp_move_bol(mpdm_t t);
void mp_move_eol(mpdm_t t);
void mp_move_bof(mpdm_t t);
void mp_move_eof(mpdm_t t);
void mp_move_left(mpdm_t t);
void mp_move_right(mpdm_t t);
void mp_move_xy(mpdm_t t, int x, int y);

int mp_insert_line(mpdm_t t);
int mp_insert(mpdm_t t, mpdm_t str);
int mp_delete(mpdm_t t);

void mp_load_file(mpdm_t t, char * file);

int mp_startup(void);
int mp_shutdown(void);
