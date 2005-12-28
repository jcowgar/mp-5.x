/*

    Minimum Profit - Programmer Text Editor

    Copyright (C) 1991-2005 Angel Ortega <angel@triptico.com>

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

#define MP_ATTR_NORMAL		0
#define MP_ATTR_CURSOR		1
#define MP_ATTR_SELECTION	2
#define MP_ATTR_COMMENTS	3
#define MP_ATTR_QUOTES		4
#define MP_ATTR_MATCHING	5
#define MP_ATTR_WORD_1		6
#define MP_ATTR_WORD_2		7
#define MP_ATTR_TAG		8

extern int mp_exit_requested;
extern int mp_main_argc;
extern char ** mp_main_argv;
extern mpdm_t mp;

mpdm_t mp_get_active(void);
mpdm_t mp_process_event(mpdm_t keycode);
