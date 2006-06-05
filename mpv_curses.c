/*

    Minimum Profit - Programmer Text Editor

    Curses driver.

    Copyright (C) 1991-2006 Angel Ortega <angel@triptico.com>

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
int * nc_attrs = NULL;

/* code for the 'normal' attribute */
static int normal_attr = 0;

/*******************
	Code
********************/

static void nc_sigwinch(int s)
/* SIGWINCH signal handler */
{
	mpdm_t v;

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
	v = mpdm_hget_s(mp, L"window");
	mpdm_hset_s(v, L"tx", MPDM_I(COLS));
	mpdm_hset_s(v, L"ty", MPDM_I(LINES));

	/* reattach */
	signal(SIGWINCH, nc_sigwinch);
}


static wchar_t * nc_getwch(void)
/* gets a key as a wchar_t */
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
/* reads a key and converts to an action */
{
	static int shift = 0;
	wchar_t * f = NULL;

	f = nc_getwch();

	if(shift)
	{
		switch(f[0]) {
		case L'0':		f = L"f10"; break;
		case L'1':		f = L"f1"; break;
		case L'2':		f = L"f2"; break;
		case L'3':		f = L"f3"; break;
		case L'4':		f = L"f4"; break;
		case L'5':		f = L"f5"; break;
		case L'6':		f = L"f6"; break;
		case L'7':		f = L"f7"; break;
		case L'8':		f = L"f8"; break;
		case L'9':		f = L"f9"; break;
		case KEY_LEFT:		f = L"ctrl-cursor-left"; break;
		case KEY_RIGHT: 	f = L"ctrl-cursor-right"; break;
		case KEY_DOWN:		f = L"ctrl-cursor-down"; break;
		case KEY_UP:		f = L"ctrl-cursor-up"; break;
		case KEY_END:		f = L"ctrl-end"; break;
		case KEY_HOME:		f = L"ctrl-home"; break;
		case L'\r':		f = L"ctrl-enter"; break;
		case L'\e':		f = L"escape"; break;
		case KEY_ENTER:		f = L"ctrl-enter"; break;
		case L' ':		f = L"ctrl-space"; break;
		case L'a':		f = L"ctrl-a"; break;
		case L'b':		f = L"ctrl-b"; break;
		case L'c':		f = L"ctrl-c"; break;
		case L'd':		f = L"ctrl-d"; break;
		case L'e':		f = L"ctrl-e"; break;
		case L'f':		f = L"ctrl-f"; break;
		case L'g':		f = L"ctrl-g"; break;
		case L'h':		f = L"ctrl-h"; break;
		case L'i':		f = L"ctrl-i"; break;
		case L'j':		f = L"ctrl-j"; break;
		case L'k':		f = L"ctrl-k"; break;
		case L'l':		f = L"ctrl-l"; break;
		case L'm':		f = L"ctrl-m"; break;
		case L'n':		f = L"ctrl-n"; break;
		case L'o':		f = L"ctrl-o"; break;
		case L'p':		f = L"ctrl-p"; break;
		case L'q':		f = L"ctrl-q"; break;
		case L'r':		f = L"ctrl-r"; break;
		case L's':		f = L"ctrl-s"; break;
		case L't':		f = L"ctrl-t"; break;
		case L'u':		f = L"ctrl-u"; break;
		case L'v':		f = L"ctrl-v"; break;
		case L'w':		f = L"ctrl-w"; break;
		case L'x':		f = L"ctrl-x"; break;
		case L'y':		f = L"ctrl-y"; break;
		case L'z':		f = L"ctrl-z"; break;
		}

		shift = 0;
	}
	else
	{
		switch(f[0]) {
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
		case 0x7f:
		case KEY_BACKSPACE:
		case L'\b':		f = L"backspace"; break;
		case L'\r':
		case KEY_ENTER:		f = L"enter"; break;
		case L'\t':		f = L"tab"; break;
		case L' ':		f = L"space"; break;
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
		case L'\e':		shift = 1; f = NULL; break;
		}
	}

	return(f != NULL ? MPDM_S(f) : NULL);
}


static void nc_addwstr(wchar_t * str)
/* draws a string */
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


static void draw_status(void)
/* draws the status bar */
{
	mpdm_t t;
	int n;

	t = mp_build_status_line();

	/* move to the last line and draw there */
	move(LINES - 1, 0);
	attrset(nc_attrs[normal_attr]);
	nc_addwstr(t->data);

	/* fill the line to the end */
	for(n = mpdm_size(t);n < COLS;n++)
		addch(' ');
}


static void nc_draw(mpdm_t doc)
/* driver drawing function for cursesw */
{
	mpdm_t d;
	int n, m;

	erase();

	d = mp_draw(doc, 0);

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

	draw_status();

	refresh();
}


static void build_colors(void)
/* builds the colors */
{
	mpdm_t colors;
	mpdm_t color_names;
	mpdm_t l;
	mpdm_t c;
	int n, s;

#ifdef CONFOPT_TRANSPARENCY
	use_default_colors();

#define DEFAULT_INK -1
#define DEFAULT_PAPER -1

#else /* CONFOPT_TRANSPARENCY */

#define DEFAULT_INK COLOR_BLACK
#define DEFAULT_PAPER COLOR_WHITE

#endif

	/* gets the color definitions and attribute names */
	colors = mpdm_hget_s(mp, L"colors");
	color_names = mpdm_hget_s(mp, L"color_names");
	l = mpdm_keys(colors);
	s = mpdm_size(l);

	/* redim the structures */
	nc_attrs = realloc(nc_attrs, sizeof(int) * s);

	/* loop the colors */
	for(n = 0;n < s && (c = mpdm_aget(l, n)) != NULL;n++)
	{
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"text");
		int cp, c0, c1;

		/* store the 'normal' attribute */
		if(wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		/* get color indexes */
		if((c0 = mpdm_seek(color_names, mpdm_aget(v, 0), 1)) == -1 ||
		   (c1 = mpdm_seek(color_names, mpdm_aget(v, 1), 1)) == -1)
			continue;

		init_pair(n + 1, c0 - 1, c1 - 1);
		cp = COLOR_PAIR(n + 1);

		/* flags */
		v = mpdm_hget_s(d, L"flags");
		if(mpdm_seek_s(v, L"reverse", 1) != -1) cp |= A_REVERSE;
		if(mpdm_seek_s(v, L"bright", 1) != -1) cp |= A_BOLD;
		if(mpdm_seek_s(v, L"underline", 1) != -1) cp |= A_UNDERLINE;

		nc_attrs[n] = cp;
	}

	/* set the background filler */
	bkgdset(' ' | nc_attrs[normal_attr]);
}


static mpdm_t ncdrv_startup(mpdm_t a)
{
	mpdm_t v;

	initscr();
	start_color();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

	build_colors();

	v = mpdm_hget_s(mp, L"window");
	mpdm_hset_s(v, L"tx", MPDM_I(COLS));
	mpdm_hset_s(v, L"ty", MPDM_I(LINES));

	signal(SIGWINCH, nc_sigwinch);

	return(NULL);
}


static mpdm_t ncdrv_main_loop(mpdm_t a)
/* curses driver main loop */
{
	while(! mp_exit_requested)
	{
		/* get current document and draw it */
		nc_draw(mp_active());

		/* get key and process it */
		mp_process_event(nc_getkey());
	}

	return(NULL);
}


static mpdm_t ncdrv_shutdown(mpdm_t a)
{
	endwin();
	return(NULL);
}


static mpdm_t ncdrv_clip_to_sys(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


static mpdm_t ncdrv_sys_to_clip(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


static mpdm_t ncdrv_alert(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


static mpdm_t ncdrv_confirm(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


static mpdm_t ncdrv_readline(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


static mpdm_t ncdrv_openfile(mpdm_t a)
/* openfile driver function */
{
	return(ncdrv_readline(a));
}


static mpdm_t ncdrv_savefile(mpdm_t a)
/* savefile driver function */
{
	return(ncdrv_readline(a));
}


static mpdm_t ncdrv_update_ui(mpdm_t a)
{
	/* dummy */
	return(NULL);
}


int ncdrv_init(void)
{
	mpdm_t drv;

	drv = mpdm_ref(MPDM_H(0));
	mpdm_hset_s(mp, L"drv", drv);

	mpdm_hset_s(drv, L"id", MPDM_LS(L"curses"));

	mpdm_hset_s(drv, L"startup", MPDM_X(ncdrv_startup));
	mpdm_hset_s(drv, L"main_loop", MPDM_X(ncdrv_main_loop));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(ncdrv_shutdown));

	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(ncdrv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(ncdrv_sys_to_clip));
	mpdm_hset_s(drv, L"update_ui", MPDM_X(ncdrv_update_ui));

	mpdm_hset_s(drv, L"alert", MPDM_X(ncdrv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(ncdrv_confirm));
	mpdm_hset_s(drv, L"readline", MPDM_X(ncdrv_readline));
	mpdm_hset_s(drv, L"openfile", MPDM_X(ncdrv_openfile));
	mpdm_hset_s(drv, L"savefile", MPDM_X(ncdrv_savefile));

	return(1);
}

#else /* CONFOPT_CURSES */

int ncdrv_init(void)
{
	/* no curses */
	return(0);
}

#endif /* CONFOPT_CURSES */
