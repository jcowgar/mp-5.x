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

extern mpdm_v _mp;

void mp_move_up(mpdm_v cdata);
void mp_move_down(mpdm_v cdata);
void mp_move_bol(mpdm_v cdata);
void mp_move_eol(mpdm_v cdata);
void mp_move_bof(mpdm_v cdata);
void mp_move_eof(mpdm_v cdata);
void mp_move_left(mpdm_v cdata);
void mp_move_right(mpdm_v cdata);
void mp_move_xy(mpdm_v cdata, int x, int y);

int mp_insert_line(mpdm_v cdata);
int mp_insert(mpdm_v cdata, mpdm_v str);
int mp_delete(mpdm_v cdata);

int mp_insert_char(mpdm_v txt, int * x, int * y, int c);
int mp_delete_char(mpdm_v txt, int * x, int * y);

mpdm_v mp_load_file(char * file);

int mp_startup(void);
int mp_shutdown(void);
