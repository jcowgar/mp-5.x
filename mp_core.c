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

/* exit requested? */
int mp_exit_requested = 0;

/* main namespace */
mpdm_t mp = NULL;

/*******************
	Code
********************/

/* private data for drawing syntax-highlighted text */

struct drw_1_info {
	mpdm_t txt;		/* the document */
	mpdm_t syntax;		/* syntax highlight information */
	mpdm_t colors;		/* color definitions (for attributes) */
	mpdm_t word_color_func;	/* word color function (just for detection) */
	int normal_attr;	/* normal attr */
	int cursor_attr;	/* cursor attr */
	int n_lines;		/* total number of lines */
	int p_lines;		/* number of prereaded lines */
	int vx;			/* first visible column */
	int vy;			/* first visible line */
	int tx;			/* horizontal window size */
	int ty;			/* vertical window size */
	int tab_size;		/* tabulator size */
	int mod;		/* modify count */
};

struct drw_1_info drw_1;
struct drw_1_info drw_1_o;

static struct {
	int x;			/* cursor x */
	int y;			/* cursor y */
	int * offsets;		/* offsets of lines */
	char * attrs;		/* attributes */
	int visible;		/* offset to the first visible character */
	int cursor;		/* offset to cursor */
	wchar_t * ptr;		/* pointer to joined data */
	int size;		/* size of joined data */
	int matchparen_offset;	/* offset to matched paren */
	int matchparen_o_attr;	/* original attribute there */
	int cursor_o_attr;	/* original attribute under cursor */
	mpdm_t v;		/* the data */
	mpdm_t old;		/* the previously generated array */
	int mark_offset;	/* offset to the marked block */
	int mark_size;		/* size of mark_o_attr */
	char * mark_o_attr;	/* saved attributes for the mark */
} drw_2;


#define MP_REAL_TAB_SIZE(x) (drw_1.tab_size - ((x) % drw_1.tab_size))

static int drw_wcwidth(int x, wchar_t c)
/* returns the wcwidth of c, or the tab spaces for
   the x column if it's a tab */
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
		n += drw_wcwidth(n, ptr[x]);

	return(x);
}


int drw_x2vx(mpdm_t str, int x)
/* returns the column where the character at offset x seems to be */
{
	wchar_t * ptr = str->data;
	int n, vx;

	for(n = vx = 0;n < x && ptr[n] != L'\0';n++)
		vx += drw_wcwidth(vx, ptr[n]);

	return(vx);
}


static int drw_line_offset(int l)
/* returns the offset into v for line number l */
{
	return(drw_2.offsets[l - drw_1.vy + drw_1.p_lines]);
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


static int drw_adjust_x(int x, int y, int * vx, int tx, wchar_t * ptr)
/* adjust the visual x position */
{
	int n, m;
	int t = *vx;

	/* calculate the column for the cursor position */
	for(n = m = 0;n < x;n++, ptr++)
		m += drw_wcwidth(m, *ptr);

	/* if new cursor column is nearer the leftmost column, set */
	if(m < *vx) *vx = m;

	/* if new cursor column is further the rightmost column, set */
	if(m > *vx + (tx - 1)) *vx = m - (tx - 1);

	return(t != *vx);
}


static int drw_get_attr(wchar_t * color_name)
/* returns the attribute number for a color */
{
	mpdm_t v;
	int attr = 0;

	if((v = mpdm_hget_s(drw_1.colors, color_name)) != NULL)
		attr = mpdm_ival(mpdm_hget_s(v, L"attr"));

	return(attr);
}


static int drw_prepare(mpdm_t doc)
/* prepares the document for screen drawing */
{
	mpdm_t window = mpdm_hget_s(mp, L"window");
	mpdm_t config = mpdm_hget_s(mp, L"config");
	mpdm_t txt = mpdm_hget_s(doc, L"txt");
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));
	int n;

	drw_1.vx = mpdm_ival(mpdm_hget_s(txt, L"vx"));
	drw_1.vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	drw_1.tx = mpdm_ival(mpdm_hget_s(window, L"tx"));
	drw_1.ty = mpdm_ival(mpdm_hget_s(window, L"ty"));
	drw_1.tab_size = mpdm_ival(mpdm_hget_s(config, L"tab_size"));
	drw_1.mod = mpdm_ival(mpdm_hget_s(txt, L"mod"));

	/* adjust the visual y coordinate */
	if(drw_adjust_y(y, &drw_1.vy, drw_1.ty))
		mpdm_hset_s(txt, L"vy", MPDM_I(drw_1.vy));

	/* adjust the visual x coordinate */
	if(drw_adjust_x(x, y, &drw_1.vx, drw_1.tx, mpdm_string(mpdm_aget(lines, y))))
		mpdm_hset_s(txt, L"vx", MPDM_I(drw_1.vx));

	/* get the maximum prereadable lines */
	drw_1.p_lines = drw_1.vy > mpi_preread_lines ? mpi_preread_lines : drw_1.vy;

	/* maximum lines */
	drw_1.n_lines = drw_1.ty + drw_1.p_lines;

	/* get the mp.colors structure and the most used attributes */
	drw_1.colors = mpdm_hget_s(mp, L"colors");
	drw_1.normal_attr = drw_get_attr(L"normal");
	drw_1.cursor_attr = drw_get_attr(L"cursor");

	/* store the syntax highlight structure */
	drw_1.syntax = mpdm_hget_s(doc, L"syntax");

	drw_1.word_color_func = mpdm_hget_s(mp, L"word_color_func");

	mpdm_unref(drw_1.txt);
	drw_1.txt = mpdm_ref(txt);

	drw_2.x = x;
	drw_2.y = y;

	/* compare drw_1 with drw_1_o; if they are the same,
	   no more expensive calculations on drw_2 are needed */
	if(memcmp(&drw_1, &drw_1_o, sizeof(drw_1)) == 0)
		return(0);

	/* different; store now */
	memcpy(&drw_1_o, &drw_1, sizeof(drw_1_o));

	/* alloc space for line offsets */
	drw_2.offsets = realloc(drw_2.offsets, drw_1.n_lines * sizeof(int));

	drw_2.ptr = NULL;
	drw_2.size = 0;

	/* add first line */
	drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
		mpdm_aget(lines, drw_1.vy - drw_1.p_lines));

	/* first line start at 0 */
	drw_2.offsets[0] = 0;

	/* add the following lines and store their offsets */
	for(n = 1;n < drw_1.n_lines;n++)
	{
		/* add the separator */
		drw_2.ptr = mpdm_poke(drw_2.ptr, &drw_2.size,
			L"\n", 1, sizeof(wchar_t));

		/* this line starts here */
		drw_2.offsets[n] = drw_2.size;

		/* now add it */
		drw_2.ptr = mpdm_pokev(drw_2.ptr, &drw_2.size,
			mpdm_aget(lines, n + drw_1.vy - drw_1.p_lines));
	}

	drw_2.ptr = mpdm_poke(drw_2.ptr, &drw_2.size, L"", 1, sizeof(wchar_t));
	drw_2.size--;

	/* now create a value */
	mpdm_unref(drw_2.v);
	drw_2.v = mpdm_ref(MPDM_ENS(drw_2.ptr, drw_2.size));

	/* alloc and init space for the attributes */
	drw_2.attrs = realloc(drw_2.attrs, drw_2.size + 1);
	memset(drw_2.attrs, drw_1.normal_attr, drw_2.size + 1);

	drw_2.visible = drw_line_offset(drw_1.vy);

	return(1);
}


static int drw_fill_attr(int attr, int offset, int size)
/* fill an attribute */
{
	if(attr != -1)
		memset(drw_2.attrs + offset, attr, size);

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
	int o = drw_2.visible;
	mpdm_t word_color = NULL;
	mpdm_t word_color_func = NULL;

	/* take the hash of word colors, if any */
	if((word_color = mpdm_hget_s(mp, L"word_color")) == NULL)
		return;

	/* get the regex for words */
	if((r = mpdm_hget_s(mp, L"word_regex")) == NULL)
		return;

	/* get the word color function */
	word_color_func = mpdm_hget_s(mp, L"word_color_func");

	while((t = mpdm_regex(r, drw_2.v, o)) != NULL)
	{
		int attr = -1;
		mpdm_t v;

		if((v = mpdm_hget(word_color, t)) != NULL)
			attr = mpdm_ival(v);
		else
		if(word_color_func != NULL)
			attr = mpdm_ival(mpdm_exec_1(word_color_func, t));

		o = drw_fill_attr_regex(attr);
	}
}


static void drw_multiline_regex(mpdm_t a, int attr)
/* sets the attribute to all matching (possibly multiline) regexes */
{
	int n;

	for(n = 0;n < mpdm_size(a);n++)
	{
		mpdm_t r = mpdm_aget(a, n);
		int o = 0;

		/* if the regex is an array, it's a pair of
		   'match from this' / 'match until this' */
		if(r->flags & MPDM_MULTIPLE)
		{
			mpdm_t rs = mpdm_aget(r, 0);
			mpdm_t re = mpdm_aget(r, 1);

			while(mpdm_regex(rs, drw_2.v, o))
			{
				int s;

				/* fill the matched part */
				o = drw_fill_attr_regex(attr);

				/* try to match the end */
				if(mpdm_regex(re, drw_2.v, o))
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
					s = drw_2.size - o;
				}

				/* fill to there */
				o = drw_fill_attr(attr, o, s);
			}
		}
		else
		{
			/* it's a scalar: */
			/* while the regex matches, fill attributes */
			while(mpdm_regex(r, drw_2.v, o))
				o = drw_fill_attr_regex(attr);
		}
	}
}


static void drw_blocks(void)
/* fill attributes for multiline blocks */
{
	mpdm_t defs;
	int n;

	/* no definitions? return */
	if(drw_1.syntax == NULL || (defs = mpdm_hget_s(drw_1.syntax, L"defs")) == NULL)
		return;

	for(n = 0;n < mpdm_size(defs);n += 2)
	{
		mpdm_t attr;
		mpdm_t list;

		/* get the attribute */
		attr = mpdm_aget(defs, n);
		attr = mpdm_hget(drw_1.colors, attr);
		attr = mpdm_hget_s(attr, L"attr");

		/* get the list for this word color */
		list = mpdm_aget(defs, n + 1);

		drw_multiline_regex(list, mpdm_ival(attr));
	}
}


static void drw_selection(void)
/* draws the selected block, if any */
{
	mpdm_t mark;
	int bx, by, ex, ey;
	int so, eo;

	/* no mark? return */
	if((mark = mpdm_hget_s(drw_1.txt, L"mark")) == NULL)
		return;

	bx = mpdm_ival(mpdm_hget_s(mark, L"bx"));
	by = mpdm_ival(mpdm_hget_s(mark, L"by"));
	ex = mpdm_ival(mpdm_hget_s(mark, L"ex"));
	ey = mpdm_ival(mpdm_hget_s(mark, L"ey"));

	/* if block is not visible, return */
	if(ey < drw_1.vy || by > drw_1.vy + drw_1.ty)
		return;

	so = by < drw_1.vy ? drw_2.visible : drw_line_offset(by) + bx;
	eo = ey >= drw_1.vy + drw_1.ty ? drw_2.size : drw_line_offset(ey) + ex;

	/* alloc space and save the attributes being destroyed */
	drw_2.mark_offset = so;
	drw_2.mark_size = eo - so;
	drw_2.mark_o_attr = malloc(eo - so);
	memcpy(drw_2.mark_o_attr, &drw_2.attrs[so], eo - so);

	drw_fill_attr(drw_get_attr(L"selection"), so, eo - so);
}


static void drw_cursor(void)
/* fill the attribute for the cursor */
{
	/* calculate the cursor offset */
	drw_2.cursor = drw_line_offset(drw_2.y) + drw_2.x;

	drw_2.cursor_o_attr = drw_2.attrs[drw_2.cursor];
	drw_fill_attr(drw_1.cursor_attr, drw_2.cursor, 1);
}


static void drw_matching_paren(void)
/* highlights the matching paren */
{
	int o = drw_2.cursor;
	int i = 0;
	wchar_t c;

	/* by default, no offset has been found */
	drw_2.matchparen_offset = -1;

	/* find the opposite and the increment (direction) */
	switch(drw_2.ptr[o]) {
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
		wchar_t s = drw_2.ptr[o];
		int m = 0;
		int l = i == -1 ? drw_2.visible - 1 : drw_2.size;

		while(o != l)
		{
			if (drw_2.ptr[o] == s)
			{
				/* found the same */
				m++;
			}
			else
			if (drw_2.ptr[o] == c)
			{
				/* found the opposite */
				if(--m == 0)
				{
					/* found! fill and exit */
					drw_2.matchparen_offset = o;
					drw_2.matchparen_o_attr = drw_2.attrs[o];
					drw_fill_attr(drw_get_attr(L"matching"), o, 1);
					break;
				}
			}

			o += i;
		}
	}
}


static mpdm_t drw_push_pair(mpdm_t l, int i, int a, wchar_t * tmp)
/* pushes a pair of attribute / string into l */
{
	/* create the array, if doesn't exist yet */
	if(l == NULL) l = MPDM_A(0);

	/* finish the string */
	tmp[i] = L'\0';

	/* special magic: if the attribute is the
	   one of the cursor and the string is more than
	   one character, create two strings; the
	   cursor is over a tab */
	if(a == drw_1.cursor_attr && i > 1)
	{
		mpdm_push(l, MPDM_I(a));
		mpdm_push(l, MPDM_NS(tmp, 1));

		/* the rest of the string has the original attr */
		a = drw_2.cursor_o_attr;

		/* one char less */
		tmp[i - 1] = L'\0';
	}

	/* store the attribute and the string */
	mpdm_push(l, MPDM_I(a));
	mpdm_push(l, MPDM_S(tmp));

	return(l);
}


#define BUF_LINE 128

static mpdm_t drw_line(int line)
/* creates a list of attribute / string pairs for the current line */
{
	mpdm_t l = NULL;
	int m, i, t, n;
	int o = drw_2.offsets[line + drw_1.p_lines];
	int a = drw_2.attrs[o];
	wchar_t tmp[BUF_LINE];
	wchar_t c;

	/* loop while not beyond the right margin */
	for(m = i = 0;m < drw_1.vx + drw_1.tx;m += t, o++)
	{
		/* take char and size */
		c = drw_2.ptr[o];
		t = drw_wcwidth(m, c);

		/* further the left margin? */
		if(m >= drw_1.vx)
		{
			/* if the attribute is different or we're out of
			   temporary space, push and go on */
			if(drw_2.attrs[o] != a || i >= BUF_LINE - t - 1)
			{
				l = drw_push_pair(l, i, a, tmp);
				i = 0;
			}

			/* size is 1, unless it's a tab */
			n = c == L'\t' ? t : 1;
			
			/* fill EOLs and tabs with spaces */
			if(c == L'\0' || c == L'\n' || c == L'\t')
				c = L' ';

			/* if next char will not fit, use a space */
			if(m + t > drw_1.vx + drw_1.tx)
				c = L' ';
		}
		else
		{
			/* left filler */
			n = m + t - drw_1.vx;
			c = L' ';
		}

		/* fill the string */
		for(;n > 0;n--)
			tmp[i++] = c;

		a = drw_2.attrs[o];

		/* end of line? */
		if(drw_2.ptr[o] == L'\0' || drw_2.ptr[o] == L'\n')
			break;
	}

	return(drw_push_pair(l, i, a, tmp));
}


static mpdm_t drw_as_array(void)
/* returns an mpdm array of ty elements, which are also arrays of
   attribute - string pairs */
{
	mpdm_t a;
	int n;

	/* the array of lines */
	a = MPDM_A(drw_1.ty);

	/* push each line */
	for(n = 0;n < drw_1.ty;n++)
		mpdm_aset(a, drw_line(n), n);

	return(a);
}


static mpdm_t drw_optimize_array(mpdm_t a, int optimize)
/* optimizes the array, NULLifying all lines that are the same as the last time */
{
	mpdm_t o = drw_2.old;
	mpdm_t r = a;

	if(optimize && o != NULL)
	{
		int n = 0;

		/* creates a copy */
		r = mpdm_clone(a);

		/* compare each array */
		while(n < mpdm_size(o) && n < mpdm_size(r))
		{
			/* if both lines are equal, optimize out */
			if(mpdm_cmp(mpdm_aget(o, n), mpdm_aget(r, n)) == 0)
				mpdm_aset(r, NULL, n);

			n++;
		}
	}

	mpdm_unref(drw_2.old); drw_2.old = mpdm_ref(a);

	return(r);
}


static void drw_restore_attrs(void)
/* restored the patched attrs */
{
	/* matching paren, if any */
	if(drw_2.matchparen_offset != -1)
		drw_fill_attr(drw_2.matchparen_o_attr, drw_2.matchparen_offset, 1);

	/* cursor */
	drw_fill_attr(drw_2.cursor_o_attr, drw_2.cursor, 1);

	/* marked block, if any */
	if(drw_2.mark_o_attr != NULL)
	{
		memcpy(&drw_2.attrs[drw_2.mark_offset],
			drw_2.mark_o_attr, drw_2.mark_size);

		free(drw_2.mark_o_attr);
		drw_2.mark_o_attr = NULL;
	}
}


static mpdm_t drw_draw(mpdm_t doc, int optimize)
/* main document drawing function: takes a document and returns an array of
   arrays of attribute / string pairs */
{
	mpdm_t r = NULL;

	if(drw_prepare(doc))
	{
		/* colorize separate words */
		drw_words();

		/* colorize multiline blocks */
		drw_blocks();
	}

	/* now set the marked block (if any) */
	drw_selection();

	/* the cursor */
	drw_cursor();

	/* highlight the matching paren */
	drw_matching_paren();

	/* convert to an array of string / atribute pairs */
	r = drw_as_array();

	/* optimize */
	r = drw_optimize_array(r, optimize);

	/* restore the patched attrs */
	drw_restore_attrs();

	return(r);
}


mpdm_t mp_draw(mpdm_t doc, int optimize)
/* main generic drawing function: if the document has a 'paint' code,
   calls it; otherwise, call drw_draw() */
{
	mpdm_t r = NULL;
	static int ppp = 0;	/* previous private paint */
	mpdm_t f;

	/* if previous paint was private, disable optimizations */
	if(ppp) optimize = ppp = 0;

	if(doc != NULL)
	{
		if((f = mpdm_hget_s(doc, L"paint")) != NULL)
		{
			ppp = 1;
			r = mpdm_exec_2(f, doc, MPDM_I(optimize));
		}
		else
			r = drw_draw(doc, optimize);
	}

	/* if there is a global post_paint function, execute it */
	if((f = mpdm_hget_s(mp, L"post_paint")) != NULL)
		r = mpdm_exec_1(f, r);

	/* if doc has a post_paint function, execute it */
	if((f = mpdm_hget_s(doc, L"post_paint")) != NULL)
		r = mpdm_exec_1(f, r);

	return(r);
}


mpdm_t mp_active(void)
/* interface to mp.active() */
{
	return(mpdm_exec(mpdm_hget_s(mp, L"active"), NULL));
}


mpdm_t mp_process_action(mpdm_t action)
/* interface to mp.process_action() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"process_action"), action));
}


mpdm_t mp_process_event(mpdm_t keycode)
/* interface to mp.process_event() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"process_event"), keycode));
}


mpdm_t mp_set_y(mpdm_t doc, int y)
/* interface to mp.set_y() */
{
	return(mpdm_exec_2(mpdm_hget_s(mp, L"set_y"), doc, MPDM_I(y)));
}


mpdm_t mp_build_status_line(void)
/* interface to mp.build_status_line() */
{
	return(mpdm_exec(mpdm_hget_s(mp, L"build_status_line"), NULL));
}


mpdm_t mp_get_history(mpdm_t key)
/* interface to mp.get_history() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"get_history"), key));
}


mpdm_t mp_menu_label(mpdm_t action)
/* interface to mp.menu_label() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"menu_label"), action));
}


mpdm_t mp_pending_key(void)
/* interface to mp.pending_key() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"pending_key"), NULL));
}


mpdm_t mp_process_keyseq(mpdm_t key)
/* interface to mp.process_keyseq() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"process_keyseq"), key));
}


mpdm_t mp_exit(mpdm_t args)
/* exit the editor (set mp_exit_requested) */
{
	mp_exit_requested = 1;

	return(NULL);
}


mpdm_t mp_vx2x(mpdm_t args)
/* interface to drw_vx2x() */
{
	return(MPDM_I(drw_vx2x(mpdm_aget(args, 0),
		mpdm_ival(mpdm_aget(args, 1)))));
}


mpdm_t mp_x2vx(mpdm_t args)
/* interface to drw_x2vx() */
{
	return(MPDM_I(drw_x2vx(mpdm_aget(args, 0),
		mpdm_ival(mpdm_aget(args, 1)))));
}

mpdm_t mp_plain_load(mpdm_t args)
/* loads a plain file into an array (highly optimized one) */
{
	mpdm_t f = mpdm_aget(args, 0);
	mpdm_t a = MPDM_A(0);
	mpdm_t v;
	int chomped = 1;

	while((v = mpdm_read(f)) != NULL)
	{
		wchar_t * ptr = v->data;
		int size = v->size;

		/* chomp */
		if(size && ptr[size - 1] == L'\n')
		{
			if(--size && ptr[size - 1] == L'\r')
				--size;
		}
		else
			chomped = 0;

		mpdm_push(a, MPDM_NS(ptr, size));
		mpdm_destroy(v);
	}

	/* if last line was chomped, add a last, empty one */
	if(chomped) mpdm_push(a, MPDM_LS(L""));

	return(a);
}


int ncdrv_detect(int * argc, char *** argv);
int gtkdrv_detect(int * argc, char *** argv);
int w32drv_detect(int * argc, char *** argv);

void mp_startup(int argc, char * argv[])
{
	mpdm_t INC;

	mpsl_startup();

	/* reset the structures */
	memset(&drw_1, '\0', sizeof(drw_1));
	memset(&drw_1_o, '\0', sizeof(drw_1_o));

	/* create main namespace */
	mp = MPDM_H(0);
	mpdm_hset_s(mpdm_root(), L"mp", mp);

	/* basic functions and data */
	mpdm_hset_s(mp, L"x2vx", MPDM_X(mp_x2vx));
	mpdm_hset_s(mp, L"vx2x", MPDM_X(mp_vx2x));
	mpdm_hset_s(mp, L"exit", MPDM_X(mp_exit));
	mpdm_hset_s(mp, L"plain_load", MPDM_X(mp_plain_load));
	mpdm_hset_s(mp, L"window", MPDM_H(0));
	mpdm_hset_s(mp, L"drv", MPDM_H(0));

	/* version */
	mpdm_hset_s(mp, L"VERSION", MPDM_S(L"mp " VERSION));

	/* creates the INC (executable path) array */
	INC = MPDM_A(0);

	/* add installed library path */
	mpdm_push(INC, mpdm_strcat(
		mpdm_hget_s(mpdm_root(), L"APPDIR"),
		MPDM_MBS(CONFOPT_APPNAME))
	);

	/* set INC */
	mpdm_hset_s(mpdm_root(), L"INC", INC);

	if(!w32drv_detect(&argc, &argv))
	if(!gtkdrv_detect(&argc, &argv))
	if(!ncdrv_detect(&argc, &argv))
	{
		printf("No usable driver found; exiting.\n");
		exit(1);
	}

	mpsl_argv(argc, argv);
}


void mp_mpsl(void)
{
	mpdm_t e;

	mpsl_eval(MPDM_LS(L"load('mp_core.mpsl');"), NULL);

	if((e = mpdm_hget_s(mpdm_root(), L"ERROR")) != NULL)
	{
		mpdm_write_wcs(stdout, mpdm_string(e));
		printf("\n");
	}
}


void mp_shutdown(void)
{
	mpsl_shutdown();
}


int main(int argc, char * argv[])
{
	mp_startup(argc, argv);

	mp_mpsl();

	mp_shutdown();

	return(0);
}
