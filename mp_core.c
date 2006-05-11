/*

    Minimum Profit - Programmer Text Editor

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

/* arguments from main() */
int mp_main_argc = 0;
char ** mp_main_argv = NULL;

/* main namespace */
mpdm_t mp = NULL;

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
	int tab_size;	/* tabulator size */
	wchar_t * ptr;	/* pointer to joined data */
	int size;	/* size of joined data */
	mpdm_t txt;	/* the document */
	mpdm_t syntax;	/* syntax highlight information */
	mpdm_t v;	/* the data */
} drw = { 0, 0, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, NULL, 0, NULL, NULL, NULL };


#define MP_REAL_TAB_SIZE(x) (drw.tab_size - ((x) % drw.tab_size))

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
		m += drw_wcwidth(n, *ptr);

	/* if new cursor column is nearer the leftmost column, set */
	if(m < *vx) *vx = m;

	/* if new cursor column is further the rightmost column, set */
	if(m > *vx + (tx - 2)) *vx = m - (tx - 2);

	return(t != *vx);
}


static void drw_prepare(mpdm_t doc)
/* prepares the document for screen drawing */
{
	mpdm_t window = mpdm_hget_s(mp, L"window");
	mpdm_t config = mpdm_hget_s(mp, L"config");
	mpdm_t txt = mpdm_hget_s(doc, L"txt");
	mpdm_t lines = mpdm_hget_s(txt, L"lines");
	int x = mpdm_ival(mpdm_hget_s(txt, L"x"));
	int y = mpdm_ival(mpdm_hget_s(txt, L"y"));
	int n;

	drw.vx = mpdm_ival(mpdm_hget_s(txt, L"vx"));
	drw.vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	drw.tx = mpdm_ival(mpdm_hget_s(window, L"tx"));
	drw.ty = mpdm_ival(mpdm_hget_s(window, L"ty"));
	drw.tab_size = mpdm_ival(mpdm_hget_s(config, L"tab_size"));

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
	mpdm_unref(drw.v);
	drw.v = mpdm_ref(MPDM_ENS(drw.ptr, drw.size));

	/* alloc and init space for the attributes */
	drw.attrs = realloc(drw.attrs, drw.size + 1);
	memset(drw.attrs, MP_ATTR_NORMAL, drw.size + 1);

	/* store the syntax highlight structure */
	drw.syntax = mpdm_hget_s(doc, L"syntax");

	/* adjust the visual x coordinate */
	if(drw_adjust_x(x, y, &drw.vx, drw.tx))
		mpdm_hset_s(txt, L"vx", MPDM_I(drw.vx));

	mpdm_unref(drw.txt);
	drw.txt = mpdm_ref(txt);
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
	mpdm_t tags = NULL;

	/* take the tags, if any */
	if((tags = mpdm_hget_s(mp, L"tags")) == NULL)
		return;

	/* @#@ */
	r = MPDM_LS(L"/[A-Za-z_]+/");

	while((t = mpdm_regex(r, drw.v, o)) != NULL)
	{
		int attr = -1;

		if(mpdm_hget(tags, t) != NULL)
			attr = MP_ATTR_TAG;

		/* @#@ spell will be here */

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
	/* fill attributes for tokens */
	drw_multiline_regex(mpdm_hget_s(drw.syntax, L"tokens"), MP_ATTR_WORD_1);

	/* fill attributes for variables */
	drw_multiline_regex(mpdm_hget_s(drw.syntax, L"variables"), MP_ATTR_WORD_2);

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


static mpdm_t drw_push_pair(mpdm_t l, int i, int a, wchar_t * tmp)
/* pushes a pair of attribute / string into l */
{
	/* nothing to add? just return */
	if(i == 0) return(l);

	/* create the array, if doesn't exist yet */
	if(l == NULL) l = MPDM_A(0);

	/* finish the string */
	tmp[i] = L'\0';

	/* special magic: if the attribute is the
	   one of the cursor and the string is more than
	   one character, create two strings; the
	   cursor is over a tab */
	if(a == MP_ATTR_CURSOR && i > 1)
	{
		mpdm_push(l, MPDM_I(a));
		mpdm_push(l, MPDM_NS(tmp, 1));

		/* cursor color is normal */
		a = MP_ATTR_NORMAL;

		/* one char less */
		tmp[i - 1] = L'\0';
	}

	/* store the attribute and the string */
	mpdm_push(l, MPDM_I(a));
	mpdm_push(l, MPDM_S(tmp));

	return(l);
}


static mpdm_t drw_line(int line)
/* creates a list of attribute / string pairs for the current line */
{
	mpdm_t l = NULL;
	int m, i, t, n;
	int o = drw.offsets[line + drw.p_lines];
	int a = drw.attrs[o];
	wchar_t tmp[128];
	wchar_t c;

	/* loop while not beyond the right margin */
	for(m = i = 0;m < drw.vx + drw.tx;m += t, o++)
	{
		/* take char and size */
		c = drw.ptr[o];
		t = drw_wcwidth(m, c);

		/* further the left margin? */
		if(m >= drw.vx)
		{
			/* if the attribute is different or we're out of
			   temporary space, push and go on */
			if(drw.attrs[o] != a || i >= sizeof(tmp) - t - 1)
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
			if(m + t > drw.vx + drw.tx)
				c = L' ';
		}
		else
		{
			/* left filler */
			n = m + t - drw.vx;
			c = L' ';
		}

		/* fill the string */
		for(;n > 0;n--)
			tmp[i++] = c;

		a = drw.attrs[o];

		/* end of line? */
		if(drw.ptr[o] == L'\0' || drw.ptr[o] == L'\n')
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
	a = MPDM_A(drw.ty);

	/* push each line */
	for(n = 0;n < drw.ty;n++)
		mpdm_aset(a, drw_line(n), n);

	return(a);
}


mpdm_t mp_draw(mpdm_t doc)
/* main drawing function: takes a document and returns an array of
   arrays of attribute / string pairs */
{
	if(doc == NULL)
		return(NULL);

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


mpdm_t mp_get_filetabs(int * active, int * last)
/* returns an array containing the names of the documents
   to fill the filetabs, or NULL if no rebuild is needed */
{
	mpdm_t docs;
	static int last_size = -1;
	static mpdm_t last_seen = NULL;
	mpdm_t r = NULL;

	/* gets the document list */
	if((docs = mpdm_hget_s(mp, L"docs")) == NULL)
		return(r);

	/* gets the active document number and the last size */
	*active = mpdm_ival(mpdm_hget_s(mp, L"active_i"));
	*last = last_size;

	if(last_size != mpdm_size(docs))
	{
		last_size = mpdm_size(docs);
		last_seen = NULL;
		r = docs;
	}
	else
	{
		if(last_size == 1)
		{
			/* same number of documents; test if the active
			   one's name is the same as the last seen one */
			mpdm_t t = mpdm_aget(docs, *active);
			t = mpdm_hget_s(t, L"name");

			/* not the same? */
			if(t != last_seen)
			{
				last_seen = t;
				r = docs;
			}
		}
	}

	return(r);
}


mpdm_t mp_active(void)
/* interfaz to mp.active() */
{
	return(mpdm_exec(mpdm_hget_s(mp, L"active"), NULL));
}


mpdm_t mp_process_event(mpdm_t keycode)
/* interfaz to mp.process_event() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"process_event"), keycode));
}


mpdm_t mp_set_y(mpdm_t doc, int y)
/* interfaz to mp.set_y() */
{
	return(mpdm_exec_2(mpdm_hget_s(mp, L"set_y"), doc, MPDM_I(y)));
}


mpdm_t mp_build_status_line(void)
/* interfaz to mp.build_status_line() */
{
	return(mpdm_exec(mpdm_hget_s(mp, L"build_status_line"), NULL));
}


mpdm_t mp_get_history(mpdm_t key)
/* interfaz to mp.get_history() */
{
	return(mpdm_exec_1(mpdm_hget_s(mp, L"get_history"), key));
}


mpdm_t mp_exit(mpdm_t args)
/* exit the editor (set mp_exit_requested) */
{
	mp_exit_requested = 1;

	return(NULL);
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


int ncdrv_init(void);
int gtkdrv_init(void);
int w32drv_init(void);

void mp_startup(void)
{
	mpdm_t t;
	int n;
	char * ptr;

	mpsl_startup();

	mpsl_argv(mp_main_argc, mp_main_argv);

	/* create main namespace */
	mp = MPDM_H(0);
	mpdm_hset_s(mpdm_root(), L"mp", mp);

	/* basic functions and data */
	mpdm_hset_s(mp, L"x2vx", MPDM_X(mp_x2vx));
	mpdm_hset_s(mp, L"vx2x", MPDM_X(mp_vx2x));
	mpdm_hset_s(mp, L"exit", MPDM_X(mp_exit));
	mpdm_hset_s(mp, L"window", MPDM_H(0));

	/* version */
	mpdm_hset_s(mp, L"VERSION", MPDM_S(L"mp " VERSION));

	/* home directory */
	if((ptr = getenv("HOME")) == NULL)
		ptr = ".";

	mpdm_hset_s(mp, L"HOME", MPDM_MBS(ptr));

	/* lib directory */
	mpdm_hset_s(mp, L"LIB",
		MPDM_MBS(CONFOPT_PREFIX "/share/" CONFOPT_APPNAME "/"));

	if(!w32drv_init())
	if(!gtkdrv_init())
	if(!ncdrv_init())
	{
		printf("No usable driver found; exiting.\n");
		exit(1);
	}
}


void mp_mpsl(void)
{
	mpdm_t v;
	mpdm_t INC;

	/* creates the INC (executable path) array */
	INC = MPDM_A(0);

	/* HACK: use current directory */
	mpdm_push(INC, MPDM_LS(L"."));

	/* add installed library path */
	mpdm_push(INC, mpdm_hget_s(mp, L"LIB"));

	/* set INC */
	mpdm_hset_s(mpdm_root(), L"INC", INC);

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
	mpsl_shutdown();
}


int main(int argc, char * argv[])
{
	/* store for later usage */
	mp_main_argc = argc;
	mp_main_argv = argv;

	mp_startup();

	mp_mpsl();

	mp_shutdown();

	return(0);
}
