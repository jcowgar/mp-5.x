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
#include <string.h>

#include "mpdm.h"
#include "mp_core.h"

/*******************
	Data
********************/

/* root hash */
mpdm_v _mp;

/* array of documents */
mpdm_v _mp_docs;

/* document hash template */
mpdm_v _mp_template;

/*
mpdm_v _mp_tags;
mpdm_v _mp_keys;
*/

/*******************
	Code
********************/

void _mp_get(mpdm_v cdata, int * x, int * y, int * mx, int * my)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;

	txt=mpdm_hget(cdata, MPDM_LS("txt"));

	if(x != NULL)
		*x=mpdm_ival(mpdm_hget(txt, MPDM_LS("x")));

	if(y != NULL)
		*y=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));

	if(my != NULL || mx != NULL)
	{
		lines=mpdm_hget(txt, MPDM_LS("lines"));

		if(my != NULL)
			*my=mpdm_size(lines) - 1;

		if(mx != NULL)
		{
			int i=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));
			line=mpdm_aget(lines, i);

			*mx=mpdm_size(line);
		}
	}
}


int _mp_set_x(mpdm_v cdata, int x)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;
	int nx, ny, i;
	int ret=0;

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	lines=mpdm_hget(txt, MPDM_LS("lines"));

	nx=ny=-1;

	i=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));

	if(x < 0)
	{
		/* cursor moved left of the bol;
		   effective cursor up + eol */
		if(i > 0)
		{
			ny=i - 1;
			line=mpdm_aget(lines, ny);
			nx=mpdm_size(line);
		}
	}
	else
	{
		/* test if moved beyond end of line */
		line=mpdm_aget(lines, i);

		if(x > mpdm_size(line))
		{
			if(i < mpdm_size(lines) - 1)
			{
				/* cursor moved right of eol;
				   effective cursor down + bol */
				nx=0;
				ny=i + 1;
			}
		}
		else
			nx=x;
	}

	/* store new coords */
	if(nx >= 0)
	{
		mpdm_hset(txt, MPDM_LS("x"), MPDM_I(nx));
		ret++;
	}

	if(ny >= 0)
	{
		mpdm_hset(txt, MPDM_LS("y"), MPDM_I(ny));
		ret++;
	}

	return(ret);
}


int _mp_set_y(mpdm_v cdata, int y)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;
	int nx, ny;
	int ret=0;

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	lines=mpdm_hget(txt, MPDM_LS("lines"));

	nx=ny=-1;

	/* never move beyond last line */
	ny=y > mpdm_size(lines) - 1 ? mpdm_size(lines) - 1 : y;

	/* gets new line */
	line=mpdm_aget(lines, ny);

	/* test if y movement made x be far beyond current line eol */
	if(mpdm_ival(mpdm_hget(txt, MPDM_LS("x"))) > mpdm_size(line))
		nx=mpdm_size(line);

	/* store new coords */
	if(nx >= 0)
	{
		mpdm_hset(txt, MPDM_LS("x"), MPDM_I(nx));
		ret++;
	}

	if(ny >= 0)
	{
		mpdm_hset(txt, MPDM_LS("y"), MPDM_I(ny));
		ret++;
	}

	return(ret);
}


void mp_move_up(mpdm_v cdata)
{
	int y;

	_mp_get(cdata, NULL, &y, NULL, NULL);
	_mp_set_y(cdata, y - 1);
}


void mp_move_down(mpdm_v cdata)
{
	int y;

	_mp_get(cdata, NULL, &y, NULL, NULL);
	_mp_set_y(cdata, y + 1);
}


void mp_move_bol(mpdm_v cdata)
{
	_mp_set_x(cdata, 0);
}


void mp_move_eol(mpdm_v cdata)
{
	int mx;

	_mp_get(cdata, NULL, NULL, &mx, NULL);
	_mp_set_x(cdata, mx);
}


void mp_move_bof(mpdm_v cdata)
{
	_mp_set_y(cdata, 0);
	_mp_set_x(cdata, 0);
}


void mp_move_eof(mpdm_v cdata)
{
	int my;

	_mp_get(cdata, NULL, NULL, NULL, &my);
	_mp_set_y(cdata, my);
}


void mp_move_left(mpdm_v cdata)
{
	int x;

	_mp_get(cdata, &x, NULL, NULL, NULL);
	_mp_set_x(cdata, x - 1);
}


void mp_move_right(mpdm_v cdata)
{
	int x;

	_mp_get(cdata, &x, NULL, NULL, NULL);
	_mp_set_x(cdata, x + 1);
}


void mp_move_xy(mpdm_v cdata, int x, int y)
{
	_mp_set_y(cdata, y);
	_mp_set_x(cdata, x);
}


/* modifying */

void mp_save_undo(mpdm_v cdata)
{
	mpdm_v txt;
	mpdm_v undo;
	int undo_levels;

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	if((undo=mpdm_hget(cdata, MPDM_LS("undo"))) == NULL)
	{
		undo=MPDM_A(0);
		mpdm_hset(cdata, MPDM_LS("undo"), undo);
	}

	/* gets from config */
	if((undo_levels=mpdm_ival(MPDM_SGET(NULL, "mp.config.undo_levels"))) == 0)
		undo_levels=8;

	/* enqueue */
	mpdm_aqueue(undo, mpdm_clone(txt), undo_levels);
}


int mp_insert_line(mpdm_v cdata)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;
	mpdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	lines=mpdm_hget(txt, MPDM_LS("lines"));
	x=mpdm_ival(mpdm_hget(txt, MPDM_LS("x")));
	y=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));
	line=mpdm_aget(lines, y);
	
	/* split current line in two */
	w=mpdm_splice(line, NULL, x, mpdm_size(line) - x);

	/* store first part inside current line */
	mpdm_aset(lines, mpdm_aget(w, 0), y);

	/* move to next line */
	y++;

	/* insert a new line here */
	mpdm_aexpand(lines, y, 1);

	/* store second part of line as a new one */
	mpdm_aset(lines, mpdm_aget(w, 1), y);

	/* move to that line, to the beginning */
	_mp_set_y(cdata, y);
	_mp_set_x(cdata, 0);

	return(1);
}


int mp_insert(mpdm_v cdata, mpdm_v str)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;
	mpdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	lines=mpdm_hget(txt, MPDM_LS("lines"));
	x=mpdm_ival(mpdm_hget(txt, MPDM_LS("x")));
	y=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));
	line=mpdm_aget(lines, y);
	
	/* insert */
	w=mpdm_splice(line, str, x, 0);

	/* store as new line */
	mpdm_aset(lines, mpdm_aget(w, 0), y);

	/* move cursor right */
	_mp_set_x(cdata, x + mpdm_size(str));

	return(1);
}


int mp_delete(mpdm_v cdata)
{
	mpdm_v txt;
	mpdm_v lines;
	mpdm_v line;
	mpdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=mpdm_hget(cdata, MPDM_LS("txt"));
	lines=mpdm_hget(txt, MPDM_LS("lines"));
	x=mpdm_ival(mpdm_hget(txt, MPDM_LS("x")));
	y=mpdm_ival(mpdm_hget(txt, MPDM_LS("y")));
	line=mpdm_aget(lines, y);
	
	if(x == mpdm_size(line) && y < mpdm_size(lines) - 1)
	{
		/* deleting at end of line:
		   effectively join both lines */
		w=mpdm_splice(line, mpdm_aget(lines, y + 1), x, 0);

		/* store as new */
		mpdm_aset(lines, mpdm_aget(w, 0), y);

		/* collapse array (one line less) */
		mpdm_acollapse(lines, y + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=mpdm_splice(line, NULL, x, 1);

		/* set as new */
		mpdm_aset(lines, mpdm_aget(w, 0), y);
	}

	return(1);
}


#ifdef QQ
int mp_delete_char(mpdm_v txt, int * x, int * y)
{
	mpdm_v v;
	mpdm_v w;

	v=mpdm_aget(txt, *y);

	/* deleting a newline? */
	if(*x == mpdm_size(v) && *y < mpdm_size(txt) - 1)
	{
		/* joins both lines */
		w=mpdm_splice(v, mpdm_aget(txt, (*y) + 1), *x, 0);

		/* store */
		mpdm_aset(txt, mpdm_aget(w, 0), *y);

		/* collapse array (one line less) */
		mpdm_acollapse(txt, (*y) + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=mpdm_splice(v, NULL, *x, 1);

		/* set as new */
		mpdm_aset(txt, mpdm_aget(w, 0), *y);
	}

	return(1);
}
#endif

mpdm_v mp_fgets(mpdm_v fv)
{
	char line[128];
	mpdm_v v=NULL;
	int i;
	FILE * f;

	f=(FILE *)fv->data;

	while(fgets(line, sizeof(line) - 1, f) != NULL)
	{
		if((i=strlen(line)) == 0)
			continue;

		/* if line includes \n, it's complete */
		if(line[i - 1] == '\n')
		{
			line[i - 1]='\0';
			i=0;
		}

		/* store */
		v=mpdm_strcat(v, MPDM_S(line));

		/* exit if the line is completely read */
		if(i == 0) break;
	}

	return(v);
}


mpdm_v mp_load_file(char * file)
{
	FILE * f;
	mpdm_v w;
	mpdm_v v;
	mpdm_v fv;
/*
	if((f=fopen(file, "r")) == NULL)
		return(NULL);

	fv=mpdm_new(MPDM_FILE, f, 0);

	w=MPDM_A(0);

	while((v=mp_fgets(fv)) != NULL)
		mpdm_apush(w, v);

	fclose(f);
*/

	if((fv=mpdm_open(MPDM_LS(file), MPDM_LS("r"))) == NULL)
		return(NULL);

	w=MPDM_A(0);

	while((v=mpdm_read(fv)) != NULL)
		mpdm_apush(w, v);

	mpdm_close(fv);

	return(w);
}


int mp_startup(void)
{
	/* mp's root */
	_mp=MPDM_H(7);

	/* store in mpdm's root */
	mpdm_hset(mpdm_root(), MPDM_LS("mp"), _mp);

	_mp_docs=MPDM_A(0);

	mpdm_hset(_mp, MPDM_LS("docs"), _mp_docs);

	/* builds the document template */
	_mp_template=MPDM_H(7);
	mpdm_hset(_mp_template, MPDM_LS("txt"), MPDM_A(0));
	mpdm_hset(_mp_template, MPDM_LS("x"), MPDM_I(0));
	mpdm_hset(_mp_template, MPDM_LS("y"), MPDM_I(0));

	/* store in mp's root */
	mpdm_hset(_mp, MPDM_LS("template"), _mp_template);

	return(1);
}


int mp_shutdown(void)
{
	return(1);
}
