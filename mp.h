/*

    Minimum Profit - Programmer Text Editor

    Copyright (C) 1991-2007 Angel Ortega <angel@triptico.com>

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

extern int mp_exit_requested;
extern mpdm_t mp;

mpdm_t mp_draw(mpdm_t doc, int optimize);

mpdm_t mp_active(void);
mpdm_t mp_process_action(mpdm_t action);
mpdm_t mp_process_event(mpdm_t keycode);
mpdm_t mp_set_y(mpdm_t doc, int y);
mpdm_t mp_build_status_line(void);
mpdm_t mp_get_history(mpdm_t key);
mpdm_t mp_menu_label(mpdm_t action);
