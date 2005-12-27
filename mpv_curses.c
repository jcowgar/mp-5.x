/*

    Minimum Profit - Programmer Text Editor

    Curses driver.

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

#ifdef CONFOPT_CURSES

#include <stdio.h>
#include <wchar.h>
#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/*******************
	Data
********************/

/* the curses attributes */
int nc_attrs[10];

/* the driver */
mpdm_t nc_driver = NULL;
mpdm_t nc_window = NULL;

/*******************
	Code
********************/

static void nc_sigwinch(int s)
{
#ifdef NCURSES_VERSION
	/* Make sure that window size changes... */
	struct winsize ws;

	int fd = open("/dev/tty", O_RDWR);

	if (fd == -1) return; /* This should never have to happen! */

	if (ioctl(fd, TIOCGWINSZ, &ws) == 0)
		resizeterm(ws.ws_row, ws.ws_col);

	close(fd);
#else
	/* restart curses */
	/* ... */
#endif

	/* invalidate main window */
	clearok(stdscr, 1);
	refresh();

	/* re-set dimensions */
	mpdm_hset_s(nc_window, L"tx", MPDM_I(COLS));
	mpdm_hset_s(nc_window, L"ty", MPDM_I(LINES));

	/* reattach */
	signal(SIGWINCH, nc_sigwinch);
}


static wchar_t * nc_getwch(void)
{
	static wchar_t c[2];

#ifdef CONFOPT_GET_WCH

	get_wch(c);

#else
	char tmp[MB_CUR_MAX + 1];
	int cc, n = 0;

	/* read one byte */
	cc = getch();
	if(has_key(cc))
	{
		c[0] = cc;
		return(c);
	}

	/* set to non-blocking */
	nodelay(stdscr, 1);

	/* read all possible following characters */
	tmp[n++] = cc;
	while(n < sizeof(tmp) - 1 && (cc = getch()) != ERR)
		tmp[n++] = cc;

	/* sets input as blocking */
	nodelay(stdscr, 0);

	tmp[n] = '\0';
	mbstowcs(c, tmp, n);
#endif

	c[1] = '\0';
	return(c);
}


#define ctrl(k) ((k) & 31)

static mpdm_t nc_getkey(void)
{
	wchar_t * f = NULL;

	f = nc_getwch();

	switch(f[0]) {
	case L'\e':		f = L"escape"; break;
	case KEY_LEFT:		f = L"cursor-left"; break;
	case KEY_RIGHT:		f = L"cursor-right"; break;
	case KEY_UP:		f = L"cursor-up"; break;
	case KEY_DOWN:		f = L"cursor-down"; break;
	case KEY_PPAGE:		f = L"page-up"; break;
	case KEY_NPAGE:		f = L"page-down"; break;
	case KEY_HOME:		f = L"home"; break;
	case KEY_END:		f = L"end"; break;
	case KEY_IC:		f = L"insert"; break;
	case KEY_DC:		f = L"delete"; break;
	case KEY_BACKSPACE:
	case '\b':		f = L"backspace"; break;
	case '\r':
	case KEY_ENTER:		f = L"enter"; break;
	case '\t':		f = L"tab"; break;
	case ' ':		f = L"space"; break;
	case KEY_F(1):		f = L"f1"; break;
	case KEY_F(2):		f = L"f2"; break;
	case KEY_F(3):		f = L"f3"; break;
	case KEY_F(4):		f = L"f4"; break;
	case KEY_F(5):		f = L"f5"; break;
	case KEY_F(6):		f = L"f6"; break;
	case KEY_F(7):		f = L"f7"; break;
	case KEY_F(8):		f = L"f8"; break;
	case KEY_F(9):		f = L"f9"; break;
	case KEY_F(10): 	f = L"f10"; break;
	case ctrl(' '): 	f = L"ctrl-space"; break;
	case ctrl('a'):		f = L"ctrl-a"; break;
	case ctrl('b'):		f = L"ctrl-b"; break;
	case ctrl('c'):		f = L"ctrl-c"; break;
	case ctrl('d'):		f = L"ctrl-d"; break;
	case ctrl('e'):		f = L"ctrl-e"; break;
	case ctrl('f'):		f = L"ctrl-f"; break;
	case ctrl('g'):		f = L"ctrl-g"; break;
	case ctrl('j'):		f = L"ctrl-j"; break;
	case ctrl('l'):		f = L"ctrl-l"; break;
	case ctrl('n'):		f = L"ctrl-n"; break;
	case ctrl('o'):		f = L"ctrl-o"; break;
	case ctrl('p'):		f = L"ctrl-p"; break;
	case ctrl('q'):		f = L"ctrl-q"; break;
	case ctrl('r'):		f = L"ctrl-r"; break;
	case ctrl('s'):		f = L"ctrl-s"; break;
	case ctrl('t'):		f = L"ctrl-t"; break;
	case ctrl('u'):		f = L"ctrl-u"; break;
	case ctrl('v'):		f = L"ctrl-v"; break;
	case ctrl('w'):		f = L"ctrl-w"; break;
	case ctrl('x'):		f = L"ctrl-x"; break;
	case ctrl('y'):		f = L"ctrl-y"; break;
	case ctrl('z'):		f = L"ctrl-z"; break;
	}

	return(MPDM_LS(f));
}


static void nc_addwstr(wchar_t * str)
{
#ifndef CONFOPT_ADDWSTR
	char * cptr;

	cptr = mpdm_wcstombs(str, NULL);
	addstr(cptr);
	free(cptr);

#else
	addwstr(str);
#endif /* CONFOPT_ADDWSTR */
}


mpdm_t mpi_draw(mpdm_t v);

static void nc_draw(mpdm_t doc)
/* driver drawing function for cursesw */
{
	mpdm_t d;
	int n, m;

	erase();

	d = mpi_draw(doc);

	for(n = 0;n < mpdm_size(d);n++)
	{
		mpdm_t l = mpdm_aget(d, n);

		move(n, 0);

		for(m = 0;m < mpdm_size(l);m++)
		{
			int attr;
			mpdm_t s;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			attrset(nc_attrs[attr]);
			nc_addwstr((wchar_t *) s->data);
		}
	}

	refresh();
}


static mpdm_t nc_drv_startup(mpdm_t v)
{
	initscr();
	start_color();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

#ifdef CONFOPT_TRANSPARENCY
	use_default_colors();

#define DEFAULT_INK -1
#define DEFAULT_PAPER -1

#else /* CONFOPT_TRANSPARENCY */

#define DEFAULT_INK COLOR_BLACK
#define DEFAULT_PAPER COLOR_WHITE

#endif

	init_pair(1, DEFAULT_INK, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_NORMAL] = COLOR_PAIR(1);

	init_pair(2, DEFAULT_INK, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_CURSOR] = COLOR_PAIR(2) | A_REVERSE;

	init_pair(3, COLOR_RED, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_SELECTION] = COLOR_PAIR(3) | A_REVERSE;

	init_pair(4, COLOR_GREEN, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_COMMENTS] = COLOR_PAIR(4);

	init_pair(5, COLOR_BLUE, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_QUOTES] = COLOR_PAIR(5) | A_BOLD;

	init_pair(6, DEFAULT_INK, COLOR_CYAN);
	nc_attrs[MP_ATTR_MATCHING] = COLOR_PAIR(6);

	init_pair(7, COLOR_GREEN, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_WORD_1] = COLOR_PAIR(7) | A_BOLD;

	init_pair(8, COLOR_RED, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_WORD_2] = COLOR_PAIR(8) | A_BOLD;

	init_pair(9, COLOR_CYAN, DEFAULT_PAPER);
	nc_attrs[MP_ATTR_TAG] = COLOR_PAIR(9) | A_UNDERLINE;

	bkgdset(' ' | nc_attrs[MP_ATTR_NORMAL]);

	nc_window = MPDM_H(0);
	mpdm_hset_s(nc_window, L"tx", MPDM_I(COLS));
	mpdm_hset_s(nc_window, L"ty", MPDM_I(LINES));
	mpdm_hset_s(nc_driver, L"window", nc_window);

	signal(SIGWINCH, nc_sigwinch);

	return(NULL);
}


mpdm_t nc_drv_main_loop(mpdm_t a)
/* curses driver main loop */
{
	while(! mp_exit_requested)
	{
		/* get current document and draw it */
		nc_draw(mp_get_active());

		/* get key and process it */
		mp_process_event(nc_getkey());
	}
}


static mpdm_t nc_drv_shutdown(mpdm_t v)
{
	endwin();
	return(NULL);
}


int curses_drv_init(mpdm_t mp)
{
	nc_driver = mpdm_ref(MPDM_H(0));

	mpdm_hset_s(nc_driver, L"driver", MPDM_LS(L"curses"));
	mpdm_hset_s(nc_driver, L"startup", MPDM_X(nc_drv_startup));
	mpdm_hset_s(nc_driver, L"main_loop", MPDM_X(nc_drv_main_loop));
	mpdm_hset_s(nc_driver, L"shutdown", MPDM_X(nc_drv_shutdown));

	mpdm_hset_s(mp, L"drv", nc_driver);

	return(1);
}

#else /* CONFOPT_CURSES */

int curses_drv_init(void)
{
	/* no curses */
	return(0);
}

#endif /* CONFOPT_CURSES */
