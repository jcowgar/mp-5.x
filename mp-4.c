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

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <curses.h>

#include "mpdm.h"
#include "mpsl.h"

/*******************
	Data
********************/

/*******************
	Code
********************/

mpdm_t nc_startup(mpdm_t v)
{
	initscr();
	start_color();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

	return(NULL);
}


mpdm_t nc_shutdown(mpdm_t v)
{
	endwin();
	return(NULL);
}


mpdm_t nc_getkey(mpdm_t v)
{
	wchar_t k[2];

	get_wch(k);
	k[1] = L'\0';

	return(MPDM_S(k));
}


mpdm_t nc_draw(mpdm_t a)
{
	int n, m;
	mpdm_t txt = mpdm_aget(a, 0);
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));

	for(n = 0;n < LINES;n++)
	{
		mpdm_t l;

		if((l = mpdm_aget(lines, y + n)) == NULL)
			break;

		move(n, 0);
		addwstr((wchar_t *) l->data);
	}

	refresh();

	return(NULL);
}


void mp_4_startup(int argc, char * argv[])
{
	int n;
	mpdm_t ARGV;

	mpdm_startup();

	/* create the ARGV array */
	ARGV=MPDM_A(0);

	for(n = 0;n < argc;n++)
		mpdm_apush(ARGV, MPDM_MBS(argv[n]));

	mpdm_hset_s(mpdm_root(), L"ARGV", ARGV);
	mpdm_hset_s(mpdm_root(), L"nc_startup", MPDM_X(nc_startup));
	mpdm_hset_s(mpdm_root(), L"nc_shutdown", MPDM_X(nc_shutdown));
	mpdm_hset_s(mpdm_root(), L"nc_getkey", MPDM_X(nc_getkey));
	mpdm_hset_s(mpdm_root(), L"nc_draw", MPDM_X(nc_draw));
}


void mp_4_mpsl(void)
{
	mpdm_t v;

	v=mpsl_compile_file(MPDM_LS(L"mp-4.mpsl"));
	mpdm_exec(v, NULL);
}


void mp_4_shutdown(void)
{
	mpdm_shutdown();
}


int main(int argc, char * argv[])
{
	mp_4_startup(argc, argv);

	mp_4_mpsl();

	mp_4_shutdown();

	return(0);
}
