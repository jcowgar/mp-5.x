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
#include <stdlib.h>
#include <string.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/*******************
	Data
********************/

int mpi_preread_lines = 60;

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
	wchar_t * ptr;	/* pointer to joined data */
	int size;	/* size of joined data */
	mpdm_t txt;	/* the document */
	mpdm_t syntax;	/* syntax highlight information */
	mpdm_t v;	/* the data */
} drw = { 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, NULL, 0, NULL, NULL, NULL };


#define MP_REAL_TAB_SIZE(x) (8 - ((x) % 8))

static int mp_wcwidth(int x, wchar_t c)
{
	int r;

	switch(c) {
	case L'\n': r = 1; break;
	case L'\t': r = MP_REAL_TAB_SIZE(x); break;
	default: r = mpdm_wcwidth(c); break;
	}

	return(r < 0 ? 1 : r);
}


int drw_vx2x(mpdm_t str, int vx)
/* returns the character in str that is on column vx */
{
	wchar_t * ptr = str->data;
	int n, x;

	for(n = x = 0;n < vx && ptr[x] != L'\0';x++)
		n += mp_wcwidth(n, ptr[x]);

	return(x);
}


int drw_x2vx(mpdm_t str, int x)
/* returns the column where the character at offset x seems to be */
{
	wchar_t * ptr = str->data;
	int n, vx;

	for(n = vx = 0;n < x && ptr[n] != L'\0';n++)
		vx += mp_wcwidth(vx, ptr[n]);

	return(vx);
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
	wchar_t * ptr;
	int t = *vx;

	/* move to the first character of the line */
	ptr = drw.ptr + drw_line_offset(y);

	/* calculate the column for the cursor position */
	for(n = m = 0;n < x;n++, ptr++)
		m += mp_wcwidth(n, *ptr);

	/* if new cursor column is nearer the leftmost column, set */
	if(m < *vx) *vx = m;

	/* if new cursor column is further the rightmost column, set */
	if(m > *vx + (tx - 2)) *vx = m - (tx - 2);

	return(t != *vx);
}


static void drw_prepare(mpdm_t doc)
{
	mpdm_t txt = mpdm_hget_s(doc, L"txt");
	mpdm_t window = mpdm_hget_s(doc, L"window");
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));
	int n;

	drw.vx = mpdm_ival(mpdm_hget_s(txt, L"vx"));
	drw.vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	drw.tx = mpdm_ival(mpdm_hget_s(window, L"tx"));
	drw.ty = mpdm_ival(mpdm_hget_s(window, L"ty"));

	/* adjust the visual y coordinate */
	if(drw_adjust_y(y, &drw.vy, drw.ty))
		mpdm_hset_s(txt, L"vy", MPDM_I(drw.vy));

	/* get the maximum prereadable lines */
	drw.p_lines = drw.vy > mpi_preread_lines ? mpi_preread_lines : drw.vy;

	/* maximum lines */
	drw.n_lines = drw.ty + drw.p_lines;

	/* alloc space for line offsets */
	drw.offsets = realloc(drw.offsets, drw.n_lines * sizeof(int));

	drw.ptr = NULL;
	drw.size = 0;

	/* add first line */
	drw.ptr = mpdm_pokev(drw.ptr, &drw.size,
		mpdm_aget(lines, drw.vy - drw.p_lines));

	/* first line start at 0 */
	drw.offsets[0] = 0;

	/* add the following lines and store their offsets */
	for(n = 1;n < drw.n_lines;n++)
	{
		/* add the separator */
		drw.ptr = mpdm_poke(drw.ptr, &drw.size,
			L"\n", 1, sizeof(wchar_t));

		/* this line starts here */
		drw.offsets[n] = drw.size;

		/* now add it */
		drw.ptr = mpdm_pokev(drw.ptr, &drw.size,
			mpdm_aget(lines, n + drw.vy - drw.p_lines));
	}

	/* now create a value */
	drw.v = mpdm_new(MPDM_STRING|MPDM_FREE, drw.ptr, drw.size);

	/* alloc and init space for the attributes */
	drw.attrs = realloc(drw.attrs, drw.size + 1);
	memset(drw.attrs, MP_ATTR_NORMAL, drw.size + 1);

	/* store the syntax highlight structure */
	drw.syntax = mpdm_hget_s(doc, L"syntax");

	/* adjust the visual x coordinate */
	if(drw_adjust_x(x, y, &drw.vx, drw.tx))
		mpdm_hset_s(txt, L"vx", MPDM_I(drw.vx));

	drw.txt = txt;
	drw.visible = drw_line_offset(drw.vy);
	drw.cursor = drw_line_offset(y) + x;
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

		/* if the regex is an array, it's a pair of
		   'match from this' / 'match until this' */
		if(r->flags & MPDM_MULTIPLE)
		{
			mpdm_t rs = mpdm_aget(r, 0);
			mpdm_t re = mpdm_aget(r, 1);

			while(mpdm_regex(rs, drw.v, o))
			{
				int s;

				/* fill the matched part */
				o = drw_fill_attr_regex(attr);

				/* try to match the end */
				if(mpdm_regex(re, drw.v, o))
				{
					/* found; fill the attribute
					   to the end of the match */
					s = mpdm_regex_size +
						(mpdm_regex_offset - o);
				}
				else
				{
					/* not found; fill to the end
					   of the document */
					s = drw.size - o;
				}

				/* fill to there */
				o = drw_fill_attr(attr, o, s);
			}
		}
		else
		{
			/* it's a scalar: */
			/* while the regex matches, fill attributes */
			while(mpdm_regex(r, drw.v, o))
				o = drw_fill_attr_regex(attr);
		}
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

	bx = mpdm_ival(mpdm_hget_s(mark, L"bx"));
	by = mpdm_ival(mpdm_hget_s(mark, L"by"));
	ex = mpdm_ival(mpdm_hget_s(mark, L"ex"));
	ey = mpdm_ival(mpdm_hget_s(mark, L"ey"));

	/* if block is not visible, return */
	if(ey < drw.vy || by > drw.vy + drw.ty)
		return;

	so = by < drw.vy ? drw.visible : drw_line_offset(by) + bx;
	eo = ey >= drw.vy + drw.ty ? drw.size : drw_line_offset(ey) + ex;

	drw_fill_attr(MP_ATTR_SELECTION, so, eo - so);
}


static void drw_matching_paren(void)
/* highlights the matching paren */
{
	int o = drw.cursor;
	int i = 0;
	wchar_t c;

	/* find the opposite and the increment (direction) */
	switch(drw.ptr[o]) {
	case L'(': c = L')'; i = 1; break;
	case L'{': c = L'}'; i = 1; break;
	case L'[': c = L']'; i = 1; break;
	case L')': c = L'('; i = -1; break;
	case L'}': c = L'{'; i = -1; break;
	case L']': c = L'['; i = -1; break;
	}

	/* if a direction is set, do the searching */
	if(i)
	{
		wchar_t s = drw.ptr[o];
		int m = 0;
		int l = i == -1 ? drw.visible - 1 : drw.size;

		while(o != l)
		{
			if (drw.ptr[o] == s)
			{
				/* found the same */
				m++;
			}
			else
			if (drw.ptr[o] == c)
			{
				/* found the opposite */
				if(--m == 0)
				{
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

static mpdm_t drw_line(int line, wchar_t * tmp)
{
	mpdm_t l = NULL;
	int m, i, x, t;
	int o = drw.offsets[line + drw.p_lines];
	int a = drw.attrs[o];
	wchar_t c = L' ';

	m = i = x = 0;

	/* skip first the lost-to-the-left characters */
	while(!EOS(drw.ptr[o]) && m < drw.vx)
	{
		a = drw.attrs[o];
		m += mp_wcwidth(x++, drw.ptr[o++]);
	}

	/* if current position is further the first column,
	   fill with spaces */
	for(t = m - drw.vx;t > 0;t--)
		tmp[i++] = L' ';

	/* now loop storing into l the pairs of
	   attributes and strings */
	while(m < drw.vx + drw.tx)
	{
		while(drw.attrs[o] == a && m < drw.vx + drw.tx)
		{
			c = drw.ptr[o];
			t = mp_wcwidth(m, c);
			m += t;

			switch(c) {
			case L'\t': while(t--) tmp[i++] = ' '; break;
			case L'\0':
			case L'\n': tmp[i++] = ' '; break;
			default: tmp[i++] = c; break;
			}

			if(EOS(c)) break;

			/* next char */
			x++; o++;
		}

		/* finish the string */
		tmp[i] = L'\0';

		/* create the array, if doesn't exist yet */
		if(l == NULL) l = MPDM_A(0);

		/* special magic: if the attribute is the
		   one of the cursor and the string is more than
		   one character, create two strings; the
		   cursor is over a tab */
		if(a == MP_ATTR_CURSOR && i > 1)
		{
			mpdm_apush(l, MPDM_I(a));
			mpdm_apush(l, MPDM_NS(tmp, 1));

			/* cursor color is normal */
			a = MP_ATTR_NORMAL;

			/* one char less */
			tmp[i - 1] = L'\0';
		}

		/* store the attribute and the string */
		mpdm_apush(l, MPDM_I(a));
		mpdm_apush(l, MPDM_S(tmp));

		a = drw.attrs[o];
		i = 0;

		if(EOS(c)) break;
	}

	return(l);
}


static mpdm_t drw_as_array(void)
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
		mpdm_t l = drw_line(n, tmp);

		mpdm_aset(r, l, n);
	}

	free(tmp);

	return(r);
}


mpdm_t mpi_draw(mpdm_t doc)
/* main drawing function: takes a document and returns an array of
   arrays of attribute / string pairs */
{
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

	return(drw_as_array());
}


mpdm_t mp_vx2x(mpdm_t args)
/* interfaz to drw_vx2x() */
{
	return(MPDM_I(drw_vx2x(mpdm_aget(args, 0),
		mpdm_ival(mpdm_aget(args, 1)))));
}


mpdm_t mp_x2vx(mpdm_t args)
/* interfaz to drw_x2vx() */
{
	return(MPDM_I(drw_x2vx(mpdm_aget(args, 0),
		mpdm_ival(mpdm_aget(args, 1)))));
}


int curses_init(mpdm_t mp);
int gtk_init(mpdm_t mp);
int win32_init(mpdm_t mp);

void mp_startup(int argc, char * argv[])
{
	mpdm_t mp;

	mpdm_startup();

	mpsl_argv(argc, argv);

	/* create main namespace */
	mp = MPDM_H(0);
	mpdm_hset_s(mpdm_root(), L"mp", mp);

	/* basic functions */
	mpdm_hset_s(mp, L"x2vx", MPDM_X(mp_x2vx));
	mpdm_hset_s(mp, L"vx2x", MPDM_X(mp_vx2x));

/*	if(!win32_init(mp))
	if(!gtk_init(mp))*/
	if(!curses_init(mp))
	{
		printf("No usable driver found; exiting.\n");
		exit(1);
	}
}


void mp_mpsl(void)
{
	mpdm_t v;

	/* HACK: Create an INC array with the current directory.
	   This won't finally be here; only CONFOPT_PREFIX will */
	mpdm_exec(mpsl_compile(MPDM_LS(L"INC = [ '.' ];")), NULL);

	if((v = mpsl_compile_file(MPDM_LS(L"mp_core.mpsl"))) == NULL)
	{
		/* compilation failed; print and exit */
		mpdm_t e = mpdm_hget_s(mpdm_root(), L"ERROR");

		if(e != NULL)
		{
			mpdm_write_wcs(stdout, mpdm_string(e));
			printf("\n");
		}
	}
	else
		mpdm_exec(v, NULL);
}


void mp_shutdown(void)
{
	mpdm_shutdown();
}


int main(int argc, char * argv[])
{
	mp_startup(argc, argv);

	mp_mpsl();

	mp_shutdown();

	return(0);
}
