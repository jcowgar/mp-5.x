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

#include <stdlib.h>
#include <string.h>

/*******************
	Data
********************/

int _attrs[10];

int mpi_preread_lines = 60;

#define MP_ATTR_NORMAL		0
#define MP_ATTR_CURSOR		1
#define MP_ATTR_SELECTION	2
#define MP_ATTR_COMMENTS	3
#define MP_ATTR_QUOTES		4
#define MP_ATTR_MATCHING	5

/*******************
	Code
********************/

/* private data for drawing syntax-highlighted text */

static struct {
	int n_lines;	/* total number of lines */
	int p_lines;	/* number of prereaded lines */
	int * offsets;	/* offsets of lines */
	char * attrs;	/* attributes */
	int vx;		/* first visible column */
	int vy;		/* first visible line */
	int tx;		/* horizontal window size */
	int ty;		/* vertical window size */
	int visible;	/* offset to the first visible character */
	int cursor;	/* offset to cursor */
	int size;	/* size of data */
	mpdm_t txt;	/* the document */
	mpdm_t syntax;	/* syntax highlight information */
	mpdm_t v;	/* the data */
} drw = { 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL };


#define MP_REAL_TAB_SIZE(x) (8 - ((x) % 8))

static int mp_wcwidth(int x, wchar_t c)
{
	int r;

	switch(c) {
	case L'\n': r = 1; break;
	case L'\t': r = MP_REAL_TAB_SIZE(x); break;
	default: r = mpdm_wcwidth(c); break;
	}

	return(r);
}


static int drw_line_offset(int l)
/* returns the offset into v for line number l */
{
	return(drw.offsets[l - drw.vy + drw.p_lines]);
}


static int drw_adjust_y(int y, int * vy, int ty)
/* adjusts the visual y position */
{
	int t = *vy;

	/* is y above the first visible line? */
	if(y < *vy) *vy = y;

	/* is y below the last visible line? */
	if(y > *vy + (ty - 2)) *vy = y - (ty - 2);

	return(t != *vy);
}


static int drw_adjust_x(int x, int y, int * vx, int tx)
/* adjust the visual x position */
{
	int n, m;
	wchar_t * ptr = (wchar_t *) drw.v->data;
	int t = *vx;

	/* move to the first character of the line */
	ptr += drw_line_offset(y);

	/* calculate the column for the cursor position */
	for(n = m = 0;n < x;n++, ptr++)
		m += mp_wcwidth(n, *ptr);

	/* if new cursor column is nearer the leftmost column, set */
	if(m < *vx) *vx = m;

	/* if new cursor column is further the rightmost column, set */
	if(m > *vx + (tx - 2)) *vx = m - (tx - 2);

	return(t != *vx);
}


static mpdm_t drw_prepare(mpdm_t doc)
{
	mpdm_t txt = mpdm_hget_s(doc, L"txt");
	mpdm_t window = mpdm_hget_s(doc, L"window");
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));
	mpdm_t v;
	int n, o;

	drw.vx = mpdm_ival(mpdm_hget_s(txt, L"vx"));
	drw.vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	drw.tx = mpdm_ival(mpdm_hget_s(window, L"tx"));
	drw.ty = mpdm_ival(mpdm_hget_s(window, L"ty"));

	/* get the maximum prereadable lines */
	drw.p_lines = drw.vy > mpi_preread_lines ? mpi_preread_lines : drw.vy;

	/* maximum lines */
	drw.n_lines = drw.ty + drw.p_lines;

	/* alloc space for line offsets */
	drw.offsets = realloc(drw.offsets, drw.n_lines * sizeof(int));

	/* create an array for joining */
	v=MPDM_A(drw.n_lines);

	/* transfer all lines and offsets */
	for(n=o=0;n < drw.n_lines;n++)
	{
		mpdm_t t;

		t=mpdm_aget(lines, n + drw.vy - drw.p_lines);

		drw.offsets[n] = o;
		o += mpdm_size(t) + 1;

		mpdm_aset(v, t, n);
	}

	/* join all lines now */
	drw.v = mpdm_ajoin(MPDM_LS(L"\n"), v);
	drw.size = mpdm_size(drw.v);

	/* alloc and init space for the attributes */
	drw.attrs = realloc(drw.attrs, drw.size + 1);
	memset(drw.attrs, MP_ATTR_NORMAL, drw.size + 1);

	/* adjust the visual coordinates */
	if(drw_adjust_y(y, &drw.vy, drw.ty))
		mpdm_hset_s(txt, L"vy", MPDM_I(drw.vy));
	if(drw_adjust_x(x, y, &drw.vx, drw.tx))
		mpdm_hset_s(txt, L"vx", MPDM_I(drw.vx));

	/* store the syntax highlight structure */
	drw.syntax = mpdm_hget_s(doc, L"syntax");

	drw.txt = txt;
	drw.visible = drw_line_offset(drw.vy);
	drw.cursor = drw_line_offset(y) + x;

	return(v);
}


static int drw_fill_attr(int attr, int offset, int size)
/* fill an attribute */
{
	if(attr != -1)
		memset(drw.attrs + offset, attr, size);

	return(offset + size);
}


static int drw_fill_attr_regex(int attr)
/* fills with an attribute the last regex match */
{
	return(drw_fill_attr(attr, mpdm_regex_offset, mpdm_regex_size));
}


static void drw_words(void)
/* fills the attributes for separate words */
{
	mpdm_t r, t;
	int o = drw.visible;
	mpdm_t hl_words;

	/* if there is no syntax highlight info for words, exit */
	if((hl_words = mpdm_hget_s(drw.syntax, L"hl_words")) == NULL)
		return;

	/* @#@ */
	r=MPDM_LS(L"/\\w+/");

	while((t = mpdm_regex(r, drw.v, o)) != NULL)
	{
		mpdm_t c;
		int attr = -1;

		if((c = mpdm_hget(hl_words, t)) != NULL)
			attr = mpdm_ival(c);

		/* @#@ tags and spell will be here */

		o=drw_fill_attr_regex(attr);
	}
}


static void drw_multiline_regex(mpdm_t a, int attr)
/* sets the attribute to all matching (possibly multiline) regexes */
{
	int n;

	for(n=0;n < mpdm_size(a);n++)
	{
		mpdm_t r = mpdm_aget(a, n);
		int o = 0;

		/* while the regex matches, fill attributes */
		while(mpdm_regex(r, drw.v, o))
			o = drw_fill_attr_regex(attr);
	}
}


static void drw_blocks(void)
/* fill attributes for multiline blocks */
{
	/* fill attributes for quotes (strings) */
	drw_multiline_regex(mpdm_hget_s(drw.syntax, L"quotes"), MP_ATTR_QUOTES);

	/* fill attributes for comments */
	drw_multiline_regex(mpdm_hget_s(drw.syntax, L"comments"), MP_ATTR_COMMENTS);
}


static void drw_selection(void)
/* draws the selected block, if any */
{
	int bx, by, ex, ey;
	mpdm_t mark;
	int so, eo;

	/* no mark? return */
	if((mark = mpdm_hget_s(drw.txt, L"mark")) == NULL)
		return;

	bx=mpdm_ival(mpdm_hget_s(mark, L"bx"));
	by=mpdm_ival(mpdm_hget_s(mark, L"by"));
	ex=mpdm_ival(mpdm_hget_s(mark, L"ex"));
	ey=mpdm_ival(mpdm_hget_s(mark, L"ey"));

	/* if block is not visible, return */
	if(ey < drw.vy || by > drw.vy + drw.ty)
		return;

	so=by < drw.vy ? drw.visible : drw_line_offset(by) + bx;
	eo=ey > drw.vy + drw.ty ? drw.size : drw_line_offset(ey) + ex;

	drw_fill_attr(MP_ATTR_SELECTION, so, eo - so);
}


static void drw_matching_paren(void)
/* highlights the matching paren */
{
	int o = drw.cursor;
	int i = 0;
	wchar_t * ptr = (wchar_t *)drw.v->data;
	wchar_t c;

	/* find the opposite and the increment (direction) */
	switch(ptr[o]) {
	case L'(': c = L')'; i = 1; break;
	case L'{': c = L'}'; i = 1; break;
	case L'[': c = L']'; i = 1; break;
	case L')': c = L'('; i = -1; break;
	case L'}': c = L'{'; i = -1; break;
	case L']': c = L'['; i = -1; break;
	}

	/* if a direction is set, do the searching */
	if(i) {
		wchar_t s = ptr[o];
		int m = 0;
		int l = i == -1 ? drw.visible - 1 : drw.size;

		while(o != l) {
			if (ptr[o] == s) {
				/* found the same */
				m++;
			}
			else
			if (ptr[o] == c) {
				/* found the opposite */
				if(--m == 0) {
					/* found! fill and exit */
					drw_fill_attr(MP_ATTR_MATCHING, o, 1);
					break;
				}
			}

			o += i;
		}
	}
}


#define EOS(c) ((c) == L'\n' || (c) == L'\0')

mpdm_t mpi_draw_line(int line, wchar_t * tmp)
{
	mpdm_t l = NULL;
	int m, i, x, t;
	int o = drw.offsets[line + drw.p_lines];
	int a = drw.attrs[o];
	wchar_t * ptr = (wchar_t *)drw.v->data;

	m = i = x = 0;

	/* skip first the lost-to-the-left characters */
	while(!EOS(ptr[o]) && m < drw.vx)
	{
		a = drw.attrs[o];
		m += mp_wcwidth(x++, ptr[o++]);
	}

	/* if current position is further the first column,
	   fill with spaces */
	for(t = m - drw.vx;t > 0;t--)
		tmp[i++] = L' ';

	/* now loop storing into l the pairs of
	   attributes and strings */
	while(! EOS(ptr[o]) && m < drw.vx + drw.tx)
	{
		while(drw.attrs[o] == a && m < drw.vx + drw.tx)
		{
			wchar_t c = ptr[o];

			t = mp_wcwidth(x, c);
			m += t;

			switch(c) {
			case L'\t': while(t--) tmp[i++] = ' '; break;
			case L'\n': tmp[i++] = ' '; break;
			default: tmp[i++] = c; break;
			}

			if(EOS(c)) break;

			/* next char */
			x++; o++;
		}

		/* finish the string */
		tmp[i] = L'\0';

		/* store the attribute and the string */
		if(l == NULL) l = MPDM_A(0);
		mpdm_apush(l, MPDM_I(a));
		mpdm_apush(l, MPDM_S(tmp));

		a = drw.attrs[o];
		i = 0;
	}

	return(l);
}


mpdm_t mpi_draw_2(void)
/* returns an mpdm array of ty elements, which are also arrays of
   attribute - string pairs */
{
	mpdm_t r;
	int n;
	wchar_t * tmp;

	/* alloc temporary string space */
	tmp = malloc((drw.tx + 1) * sizeof(wchar_t));

	/* the array of lines */
	r = MPDM_A(drw.ty);

	for(n = 0;n < drw.ty;n++)
	{
		mpdm_t l = mpi_draw_line(n, tmp);

		mpdm_aset(r, l, n);
	}

	free(tmp);

	return(r);
}


mpdm_t mpi_draw_1(mpdm_t a)
/* first stage of draw */
{
	mpdm_t doc = mpdm_aget(a, 0);

	drw_prepare(doc);

	/* colorize separate words */
	drw_words();

	/* colorize multiline blocks */
	drw_blocks();

	/* now set the marked block (if any) */
	drw_selection();

	/* highlight the matching paren */
	drw_matching_paren();

	/* and finally the cursor */
	drw_fill_attr(MP_ATTR_CURSOR, drw.cursor, 1);

	return(mpi_draw_2());
}


mpdm_t nc_startup(mpdm_t v)
{
	initscr();
	start_color();
	keypad(stdscr, TRUE);
	nonl();
	raw();
	noecho();

	mpdm_hset_s(mpdm_root(), L"COLS", MPDM_I(COLS));
	mpdm_hset_s(mpdm_root(), L"LINES", MPDM_I(LINES));

	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_BLACK, COLOR_WHITE);

	_attrs[0] = COLOR_PAIR(1);
	_attrs[1] = COLOR_PAIR(2) | A_REVERSE;

	bkgdset(' ' | _attrs[0]);

	return(NULL);
}


mpdm_t nc_shutdown(mpdm_t v)
{
	endwin();
	return(NULL);
}


mpdm_t nc_getkey(mpdm_t v)
{
	wchar_t c[2];
	wchar_t * f = c;

	get_wch(c);
	c[1] = L'\0';

	switch(c[0]) {
	case L'\e': f = L"escape"; break;
	case KEY_LEFT: f = L"cursor-left"; break;
	case KEY_RIGHT: f = L"cursor-right"; break;
	case KEY_UP: f = L"cursor-up"; break;
	case KEY_DOWN: f = L"cursor-down"; break;
	case KEY_HOME: f = L"home"; break;
	case KEY_END: f = L"end"; break;
	}

	return(MPDM_LS(f));
}


mpdm_t nc_draw(mpdm_t a)
{
	int n;
	wchar_t buf[1024];
	mpdm_t txt = mpdm_aget(a, 0);
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));
	int vx = mpdm_ival(mpdm_hget_s(txt, L"vx"));
	int vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	wchar_t * ptr;

	move(0, 0);
	erase();

	for(n = 0;n < LINES;n++)
	{
		int m, w, o, k;
		mpdm_t l;

		/* gets the line */
		if((l = mpdm_aget(lines, vy + n)) == NULL)
			break;

		ptr=(wchar_t *) l->data;

		for(m = w = o = k = 0;m < vx + COLS;)
		{
			wchar_t c = ptr[k];
			int i = mp_wcwidth(m, c);

			if(c == L'\0') break;

			if(m >= vx)
			{
				if(c == L'\t')
				{
					int t;

					for(t = 0;t < i;t++)
						buf[o++] = L'#';
				}
				else
				{
					if(n + vy == y && k == x)
						buf[o++] = L'_';
					else
						buf[o++] = c;
				}

				w += i;
			}
			else
			{
				/* will this char cross the left margin? */
				if(m + i > vx)
				{
					int t;

					for(t = m + i - vx;t;t--, w++)
						buf[o++] = L'~';
				}
			}

			m += i;
			k++;
		}

		/* fill with spaces to the end of the line */
/*		while(w < COLS)
		{
			buf[o++] = L'-';
			w++;
		}
*/
		/* null terminate */
		buf[o] = L'\0';

		/* and draw */
		move(n, 0);
		addwstr(buf);
	}

	refresh();

	return(NULL);
}


void mp_4_startup(int argc, char * argv[])
{
	mpdm_startup();

	mpsl_argv(argc, argv);

	mpdm_hset_s(mpdm_root(), L"nc_startup", MPDM_X(nc_startup));
	mpdm_hset_s(mpdm_root(), L"nc_shutdown", MPDM_X(nc_shutdown));
	mpdm_hset_s(mpdm_root(), L"nc_getkey", MPDM_X(nc_getkey));
	mpdm_hset_s(mpdm_root(), L"nc_draw", MPDM_X(nc_draw));

	mpdm_hset_s(mpdm_root(), L"mpi_draw_1", MPDM_X(mpi_draw_1));
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
