/*

    Minimum Profit - Programmer Text Editor

    Curses driver.

    Copyright (C) 1991-2010 Angel Ortega <angel@triptico.com>

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
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <ncursesw/ncurses.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/** data **/

/* the curses attributes */
int * nc_attrs = NULL;

/* code for the 'normal' attribute */
static int normal_attr = 0;

/* current window */
static WINDOW * cw = NULL;

/* stack of windows */
static int n_stack = 0;
static WINDOW ** w_stack = NULL;

/* last attr set */
static int last_attr = 0;

/* timer function */
static int timer_msecs = 0;
static mpdm_t timer_func = NULL;

/** code **/

static void set_attr(void)
/* set the current and fill attributes */
{
	wattrset(cw, nc_attrs[last_attr]);
	wbkgdset(cw, ' ' | nc_attrs[last_attr]);
}


static void nc_sigwinch(int s)
/* SIGWINCH signal handler */
{
	mpdm_t v;

#ifdef NCURSES_VERSION
	/* Make sure that window size changes... */
	struct winsize ws;

	int fd = open("/dev/tty", O_RDWR);

	if (fd == -1)
		return; /* This should never have to happen! */

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


#ifdef CONFOPT_WGET_WCH
int wget_wch(WINDOW * w, wint_t *ch);
#endif

static wchar_t * nc_getwch(void)
/* gets a key as a wchar_t */
{
	static wchar_t c[2];

#ifdef CONFOPT_WGET_WCH

	/* set timer period */
	if (timer_msecs > 0)
		timeout(timer_msecs);

	if (wget_wch(stdscr, (wint_t *)c) == -1)
		c[0] = (wchar_t) -1;

#else
	char tmp[MB_CUR_MAX + 1];
	int cc, n = 0;

	/* read one byte */
	cc = wgetch(cw);
	if (has_key(cc)) {
		c[0] = cc;
		return c;
	}

	/* set to non-blocking */
	nodelay(cw, 1);

	/* read all possible following characters */
	tmp[n++] = cc;
	while (n < sizeof(tmp) - 1 && (cc = getch()) != ERR)
		tmp[n++] = cc;

	/* sets input as blocking */
	nodelay(cw, 0);

	tmp[n] = '\0';
	mbstowcs(c, tmp, n);
#endif

	c[1] = '\0';
	return c;
}


#define ctrl(k) ((k) & 31)

static mpdm_t nc_getkey(mpdm_t args)
/* reads a key and converts to an action */
{
	static int shift = 0;
	wchar_t * f = NULL;
	mpdm_t k = NULL;

	/* any pending key? return it */
	if ((k = mp_pending_key()) != NULL)
		return k;

	f = nc_getwch();

	if (f[0] == -1) {
		mpdm_exec(timer_func, NULL);
		return NULL;
	}

	if (shift) {
		switch (f[0]) {
		case L'0':			f = L"f10"; break;
		case L'1':			f = L"f1"; break;
		case L'2':			f = L"f2"; break;
		case L'3':			f = L"f3"; break;
		case L'4':			f = L"f4"; break;
		case L'5':			f = L"f5"; break;
		case L'6':			f = L"f6"; break;
		case L'7':			f = L"f7"; break;
		case L'8':			f = L"f8"; break;
		case L'9':			f = L"f9"; break;
		case KEY_LEFT:		f = L"alt-cursor-left"; break;
		case KEY_RIGHT: 	f = L"alt-cursor-right"; break;
		case KEY_DOWN:		f = L"alt-cursor-down"; break;
		case KEY_UP:		f = L"alt-cursor-up"; break;
		case KEY_END:		f = L"alt-end"; break;
		case KEY_HOME:		f = L"alt-home"; break;
		case L'\r':			f = L"alt-enter"; break;
		case L'\e':			f = L"escape"; break;
		case KEY_ENTER:		f = L"alt-enter"; break;
		case L' ':			f = L"alt-space"; break;
		case L'a':			f = L"alt-a"; break;
		case L'b':			f = L"alt-b"; break;
		case L'c':			f = L"alt-c"; break;
		case L'd':			f = L"alt-d"; break;
		case L'e':			f = L"alt-e"; break;
		case L'f':			f = L"alt-f"; break;
		case L'g':			f = L"alt-g"; break;
		case L'h':			f = L"alt-h"; break;
		case L'i':			f = L"alt-i"; break;
		case L'j':			f = L"alt-j"; break;
		case L'k':			f = L"alt-k"; break;
		case L'l':			f = L"alt-l"; break;
		case L'm':			f = L"alt-m"; break;
		case L'n':			f = L"alt-n"; break;
		case L'o':			f = L"alt-o"; break;
		case L'p':			f = L"alt-p"; break;
		case L'q':			f = L"alt-q"; break;
		case L'r':			f = L"alt-r"; break;
		case L's':			f = L"alt-s"; break;
		case L't':			f = L"alt-t"; break;
		case L'u':			f = L"alt-u"; break;
		case L'v':			f = L"alt-v"; break;
		case L'w':			f = L"alt-w"; break;
		case L'x':			f = L"alt-x"; break;
		case L'y':			f = L"alt-y"; break;
		case L'z':			f = L"alt-z"; break;
		case L'\'':			f = L"alt-'"; break;
		case L',':			f = L"alt-,"; break;
		case L'-':			f = L"alt--"; break;
		case L'.':			f = L"alt-."; break;
		case L'/':			f = L"alt-/"; break;
		case L'=':			f = L"alt-="; break;
		}

		shift = 0;
	}
	else {
		switch (f[0]) {
		case KEY_LEFT:		f = L"cursor-left"; break;
		case KEY_RIGHT:		f = L"cursor-right"; break;
		case KEY_UP:		f = L"cursor-up"; break;
		case KEY_DOWN:		f = L"cursor-down"; break;
		case KEY_PPAGE:		f = L"page-up"; break;
		case KEY_NPAGE:		f = L"page-down"; break;
		case KEY_HOME:		f = L"home"; break;
		case KEY_END:		f = L"end"; break;
		case KEY_LL:		f = L"end"; break;
		case KEY_IC:		f = L"insert"; break;
		case KEY_DC:		f = L"delete"; break;
		case 0x7f:
		case KEY_BACKSPACE:
		case L'\b':			f = L"backspace"; break;
		case L'\r':
		case KEY_ENTER:		f = L"enter"; break;
		case L'\t':			f = L"tab"; break;
		case KEY_BTAB:		f = L"shift-tab"; break;
		case L' ':			f = L"space"; break;
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
		case ctrl('k'):		f = L"ctrl-k"; break;
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
		case L'\e':			shift = 1; f = NULL; break;
		}
	}

	/* no known key? do nothing */
	if(f == NULL)
		return NULL;

	return mp_process_keyseq(MPDM_S(f));
}


static mpdm_t nc_addwstr(mpdm_t str)
/* draws a string */
{
	wchar_t * wptr = mpdm_string(str);

#ifndef CONFOPT_ADDWSTR
	char * cptr;

	cptr = mpdm_wcstombs(wptr, NULL);
	waddstr(cw, cptr);
	free(cptr);

#else
	waddwstr(cw, wptr);
#endif /* CONFOPT_ADDWSTR */

	return NULL;
}


static mpdm_t nc_doc_draw(mpdm_t args)
/* draws the document part */
{
	mpdm_t d;
	int n, m;

	werase(cw);

	d = mpdm_aget(args, 0);
	d = mp_draw(d, 0);

	for (n = 0; n < mpdm_size(d); n++) {
		mpdm_t l = mpdm_aget(d, n);

		wmove(cw, n, 0);

		for (m = 0; m < mpdm_size(l); m++) {
			int attr;
			mpdm_t s;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			wattrset(cw, nc_attrs[attr]);
			nc_addwstr(s);
		}
	}

	return NULL;
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
	for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"text");
		int cp, c0, c1;

		/* store the 'normal' attribute */
		if (wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		/* get color indexes */
		if ((c0 = mpdm_seek(color_names, mpdm_aget(v, 0), 1)) == -1 ||
		   (c1 = mpdm_seek(color_names, mpdm_aget(v, 1), 1)) == -1)
			continue;

		init_pair(n + 1, c0 - 1, c1 - 1);
		cp = COLOR_PAIR(n + 1);

		/* flags */
		v = mpdm_hget_s(d, L"flags");
		if (mpdm_seek_s(v, L"reverse", 1) != -1)
			cp |= A_REVERSE;
		if (mpdm_seek_s(v, L"bright", 1) != -1)
			cp |= A_BOLD;
		if (mpdm_seek_s(v, L"underline", 1) != -1)
			cp |= A_UNDERLINE;

		nc_attrs[n] = cp;
	}

	/* set the background filler */
	wbkgdset(cw, ' ' | nc_attrs[normal_attr]);
}


/** driver functions **/

static mpdm_t ncursesw_drv_timer(mpdm_t a)
{
	mpdm_t func = mpdm_aget(a, 1);
	mpdm_t r;

	timer_msecs = mpdm_ival(mpdm_aget(a, 0));

	r = mpdm_unref(timer_func);
	timer_func = mpdm_ref(func);

	return r;
}


static mpdm_t ncursesw_drv_shutdown(mpdm_t a)
{
	mpdm_t v;

	endwin();

	if ((v = mpdm_hget_s(mp, L"exit_message")) != NULL) {
		mpdm_write_wcs(stdout, mpdm_string(v));
		printf("\n");
	}

	return NULL;
}


/** TUI **/

static mpdm_t tui_addstr(mpdm_t a)
/* TUI: add a string */
{
	return nc_addwstr(mpdm_aget(a, 0));
}


static mpdm_t tui_move(mpdm_t a)
/* TUI: move to a screen position */
{
	/* curses' move() use y, x */
	wmove(cw, mpdm_ival(mpdm_aget(a, 1)), mpdm_ival(mpdm_aget(a, 0)));

	/* if third argument is not NULL, clear line */
	if (mpdm_aget(a, 2) != NULL)
		wclrtoeol(cw);

	return NULL;
}


static mpdm_t tui_attr(mpdm_t a)
/* TUI: set attribute for next string */
{
	last_attr = mpdm_ival(mpdm_aget(a, 0));

	set_attr();

	return NULL;
}


static mpdm_t tui_refresh(mpdm_t a)
/* TUI: refresh the screen */
{
	wrefresh(cw);
	return NULL;
}


static mpdm_t tui_getxy(mpdm_t a)
/* TUI: returns the x and y cursor position */
{
	mpdm_t v;
	int x, y;

	getyx(cw, y, x);

	v = MPDM_A(2);
	mpdm_aset(v, MPDM_I(x), 0);
	mpdm_aset(v, MPDM_I(y), 1);

	return v;
}


static mpdm_t tui_openpanel(mpdm_t a)
/* opens a panel (creates new window) */
{
	n_stack++;
	w_stack = realloc(w_stack, n_stack * sizeof(WINDOW *));
	cw = w_stack[n_stack - 1] = newwin(mpdm_ival(mpdm_aget(a, 3)),
			mpdm_ival(mpdm_aget(a, 2)),
			mpdm_ival(mpdm_aget(a, 1)),
			mpdm_ival(mpdm_aget(a, 0)));

	set_attr();
	wclrtobot(cw);
	box(cw, 0, 0);

	return NULL;
}


static mpdm_t tui_closepanel(mpdm_t a)
/* closes a panel (deletes last window) */
{
	n_stack--;
	delwin(w_stack[n_stack]);

	w_stack = realloc(w_stack, n_stack * sizeof(WINDOW *));
	cw = n_stack == 0 ? stdscr : w_stack[n_stack - 1];

	touchwin(cw);
	wrefresh(cw);

	return NULL;
}


static void register_functions(void)
{
	mpdm_t drv;
	mpdm_t tui;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"timer", MPDM_X(ncursesw_drv_timer));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(ncursesw_drv_shutdown));

	tui = mpsl_eval(MPDM_LS(L"load('mp_tui.mpsl');"), NULL);

	/* FIXME: if tui failed, a fatal error must be shown */

/*	if((e = mpdm_hget_s(mpdm_root(), L"ERROR")) != NULL)
	{
		mpdm_write_wcs(stdout, mpdm_string(e));
		printf("\n");

		return(0);
	}*/

	/* execute tui */
	mpdm_hset_s(tui, L"getkey", MPDM_X(nc_getkey));
	mpdm_hset_s(tui, L"addstr", MPDM_X(tui_addstr));
	mpdm_hset_s(tui, L"move", MPDM_X(tui_move));
	mpdm_hset_s(tui, L"attr", MPDM_X(tui_attr));
	mpdm_hset_s(tui, L"refresh", MPDM_X(tui_refresh));
	mpdm_hset_s(tui, L"getxy", MPDM_X(tui_getxy));
	mpdm_hset_s(tui, L"openpanel", MPDM_X(tui_openpanel));
	mpdm_hset_s(tui, L"closepanel", MPDM_X(tui_closepanel));
	mpdm_hset_s(tui, L"doc_draw", MPDM_X(nc_doc_draw));
}


static mpdm_t ncursesw_drv_startup(mpdm_t a)
{
	mpdm_t v;

	register_functions();

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

	cw = stdscr;

	return NULL;
}


int ncursesw_drv_detect(int * argc, char *** argv)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"curses"));
	mpdm_hset_s(drv, L"startup", MPDM_X(ncursesw_drv_startup));

	return 1;
}

#endif /* CONFOPT_CURSES */
