/*

    Minimum Profit - Programmer Text Editor

    Copyright (C) 1991-2004 Angel Ortega <angel@triptico.com>

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
#include <stdlib.h>
#include <string.h>

#include "mpdm.h"
#include "mp_core.h"

/*******************
	Data
********************/

/* configuration hash */
mpdm_t mp_config=NULL;

/*******************
	Code
********************/

#define MP_CONF_GET_I(k) mpdm_ival(mpdm_sget(mp_config, MPDM_LS(k)))
#define MP_CONF_SET_I(k,v) mpdm_sset(mp_config, MPDM_LS(k), MPDM_I(v))

static mpdm_t _tie_txt_d(mpdm_t v)
{
	struct mp_txt * txt=v->data;

	if(txt != NULL)
	{
		/* unrefs the values */
		mpdm_unref(txt->lines);

		/* frees the struct itself */
		free(txt);
		v->data=NULL;
	}

	return(NULL);
}


static mpdm_t _tie_mp_txt(void);

static mpdm_t _tie_txt_clo(mpdm_t v)
{
	struct mp_txt * txt=v->data;

	/* creates a new value, copying the contents of the original */
	v=mpdm_new(0, txt, v->size, _tie_mp_txt());

	/* creates and references a clone of the content */
	txt->lines=mpdm_ref(mpdm_clone(txt->lines));

	return(v);
}


static mpdm_t _tie_mp_txt(void)
{
	static mpdm_t _tie=NULL;

	if(_tie == NULL)
	{
		_tie=mpdm_ref(mpdm_clone(_mpdm_tie_cpy()));

		mpdm_aset(_tie, MPDM_X(_tie_txt_d), MPDM_TIE_DESTROY);
		mpdm_aset(_tie, MPDM_X(_tie_txt_clo), MPDM_TIE_CLONE);
	}

	return(_tie);
}


mpdm_t mp_new(void)
{
	struct mp_txt txt;

	memset(&txt, '\0', sizeof(txt));

	/* creates internal data */
	txt.lines=mpdm_ref(MPDM_A(0));

	return mpdm_new(0, &txt, sizeof(struct mp_txt), _tie_mp_txt());
}


int _mp_set_x(mpdm_t t, int x)
{
	struct mp_txt * txt;
	int ret=0;

	txt=(struct mp_txt *) t->data;

	if(x < 0)
	{
		/* cursor moved left of the bol;
		   effective cursor up + eol */
		if(txt->y > 0)
		{
			txt->y--;
			txt->x=mpdm_size(mpdm_aget(txt->lines, txt->y));
		}
	}
	else
	{
		/* test if moved beyond end of line */
		if(x > mpdm_size(mpdm_aget(txt->lines, txt->y)))
		{
			if(txt->y < mpdm_size(txt->lines) - 1)
			{
				/* cursor moved right of eol;
				   effective cursor down + bol */
				txt->x=0;
				txt->y++;
			}
		}
		else
			txt->x=x;
	}

	return(ret);
}


int _mp_set_y(mpdm_t t, int y)
{
	mpdm_t c;
	struct mp_txt * txt;
	int ret=0;

	txt=(struct mp_txt *) t->data;

	if(y >= 0 && y < mpdm_size(txt->lines))
	{
		txt->y=y;

		/* gets new line */
		c=mpdm_aget(txt->lines, y);

		/* test if y movement made x be
		   far beyond current line eol */
		if(txt->x > mpdm_size(c))
			txt->x=mpdm_size(c);
	}

	return(ret);
}


void mp_move_up(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_y(t, txt->y - 1);
}


void mp_move_down(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_y(t, txt->y + 1);
}


void mp_move_bol(mpdm_t t)
{
	_mp_set_x(t, 0);
}


void mp_move_eol(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_x(t, mpdm_size(mpdm_aget(txt->lines, txt->y)));
}


void mp_move_bof(mpdm_t t)
{
	_mp_set_y(t, 0);
	_mp_set_x(t, 0);
}


void mp_move_eof(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_y(t, mpdm_size(txt->lines) - 1);
}


void mp_move_left(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_x(t, txt->x - 1);
}


void mp_move_right(mpdm_t t)
{
	struct mp_txt * txt=t->data;

	_mp_set_x(t, txt->x + 1);
}


void mp_move_xy(mpdm_t t, int x, int y)
{
	struct mp_txt * txt=t->data;

	/* set x inconditionally; _mp_set_y will correct
	   invalid or out-of-bound values of x */
	txt->x=x;
	_mp_set_y(t, y);
}


/* modifying */

void mp_save_undo(mpdm_t t, mpdm_t u)
{
	/* enqueue */
	mpdm_aqueue(u, mpdm_clone(t), MP_CONF_GET_I(L"undo_levels"));
}


int mp_insert_line(mpdm_t t)
{
	struct mp_txt * txt=t->data;
	mpdm_t w;
	mpdm_t c;

/*	mp_save_undo(cdata); */

	c=mpdm_aget(txt->lines, txt->y);

	/* split current line in two */
	w=mpdm_splice(c, NULL, txt->x, mpdm_size(c) - txt->x);

	/* store first part inside current line */
	mpdm_aset(txt->lines, mpdm_aget(w, 0), txt->y);

	/* move to next line */
	txt->y++;

	/* insert a new line here */
	mpdm_aexpand(txt->lines, txt->y, 1);

	/* get second part of the line */
	w=mpdm_aget(w, 1);

	/* if auto-indenting... */
	if(MP_CONF_GET_I(L"auto_indent"))
	{
		int n;
		wchar_t * ptr=c->data;

		/* count spaces and tabs */
		for(n=0;ptr[n] == L' ' || ptr[n] == L'\t';n++);

		/* if any, concat before second part of line */
		if(n)
			w=mpdm_strcat(MPDM_NS(ptr, n), w);
	}

	/* store second part of line as a new one */
	mpdm_aset(txt->lines, w, txt->y);

	/* move to that line, to the beginning */
	_mp_set_x(t, 0);

	return(1);
}


int mp_insert(mpdm_t t, mpdm_t s)
{
	struct mp_txt * txt=t->data;
	mpdm_t c;
	mpdm_t w;
	int l, n;

	/* is s a multiple value? */
	if(s->flags & MPDM_MULTIPLE)
	{
		l=mpdm_size(s);

		if(l)
		{
			/* insert first element */
			mp_insert(t, mpdm_aget(s, 0));
		}

		if(l > 1)
		{
			/* break line at current x position */
			mp_insert_line(t);

			if(l > 2)
			{
				/* expand l - 2 elements */
				mpdm_aexpand(txt->lines, txt->y, l - 2);

				/* put all lines except first
				   and last in the middle */
				for(n=1;n < l - 1;n++, txt->y++)
					mpdm_aset(txt->lines,
					mpdm_aget(s, n), txt->y);
			}

			/* insert last element */
			mp_insert(t, mpdm_aget(s, l - 1));
		}
	}
	else
	{
		/* s is scalar */

		/* gets current line */
		c=mpdm_aget(txt->lines, txt->y);

		/* insert */
		w=mpdm_splice(c, s, txt->x, 0);

		/* store as new line */
		mpdm_aset(txt->lines, mpdm_aget(w, 0), txt->y);

		/* move cursor right */
		_mp_set_x(t, txt->x + mpdm_size(s));
	}

	return(1);
}


int mp_delete(mpdm_t t)
{
	struct mp_txt * txt=t->data;
	mpdm_t c;
	mpdm_t w;

/*	mp_save_undo(cdata); */

	c=mpdm_aget(txt->lines, txt->y);

	if(txt->x == mpdm_size(c) && txt->y < mpdm_size(txt->lines) - 1)
	{
		/* deleting at end of line:
		   effectively join both lines */
		w=mpdm_splice(c, mpdm_aget(txt->lines, txt->y + 1), txt->x, 0);

		/* store as new */
		mpdm_aset(txt->lines, mpdm_aget(w, 0), txt->y);

		/* collapse array (one line less) */
		mpdm_acollapse(txt->lines, txt->y + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=mpdm_splice(c, NULL, txt->x, 1);

		/* set as new */
		mpdm_aset(txt->lines, mpdm_aget(w, 0), txt->y);
	}

	return(1);
}


void mp_load_file(mpdm_t t, char * file)
{
	struct mp_txt * txt=t->data;
	mpdm_t v;
	mpdm_t fv;
/*
	if((f=fopen(file, "r")) == NULL)
		return(NULL);

	fv=mpdm_new(MPDM_FILE, f, 0);

	w=MPDM_A(0);

	while((v=mp_fgets(fv)) != NULL)
		mpdm_apush(w, v);

	fclose(f);
*/

	if((fv=mpdm_open(MPDM_MBS(file), MPDM_LS(L"r"))) == NULL)
		return;

	while((v=mpdm_read(fv)) != NULL)
		mpdm_apush(txt->lines, v);

	mpdm_close(fv);
}


int mp_startup(void)
{
	mpdm_startup();

	mpdm_sset(NULL, MPDM_LS(L"mp"), MPDM_H(0));
	mp_config=MPDM_H(0);
	mpdm_sset(NULL, MPDM_LS(L"mp.config"), mp_config);

	/* default configuration values */
	MP_CONF_SET_I(L"tab_size", 8);
	MP_CONF_SET_I(L"word_wrap", 1);
	MP_CONF_SET_I(L"auto_indent", 0);
	MP_CONF_SET_I(L"undo_levels", 8);
	MP_CONF_SET_I(L"cr_lf", 0);
	MP_CONF_SET_I(L"case_cmp", 1);
	MP_CONF_SET_I(L"use_regex", 1);

	return(1);
}


int mp_shutdown(void)
{
	mpdm_shutdown();

	return(1);
}
