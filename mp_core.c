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

#include "fdm.h"
#include "mp_core.h"

/*******************
	Data
********************/

/* root hash */
fdm_v _mp;

/* array of documents */
fdm_v _mp_docs;

/* document hash template */
fdm_v _mp_template;

/*
fdm_v _mp_tags;
fdm_v _mp_keys;
*/

/*******************
	Code
********************/

void _mp_get(fdm_v cdata, int * x, int * y, int * mx, int * my)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;

	txt=fdm_hget(cdata, FDM_LS("txt"));

	if(x != NULL)
		*x=fdm_ival(fdm_hget(txt, FDM_LS("x")));

	if(y != NULL)
		*y=fdm_ival(fdm_hget(txt, FDM_LS("y")));

	if(my != NULL || mx != NULL)
	{
		lines=fdm_hget(txt, FDM_LS("lines"));

		if(my != NULL)
			*my=lines->size - 1;

		if(mx != NULL)
		{
			int i=fdm_ival(fdm_hget(txt, FDM_LS("y")));
			line=fdm_aget(lines, i);

			*mx=line->size;
		}
	}
}


int _mp_set_x(fdm_v cdata, int x)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;
	int nx, ny, i;
	int ret=0;

	txt=fdm_hget(cdata, FDM_LS("txt"));
	lines=fdm_hget(txt, FDM_LS("lines"));

	nx=ny=-1;

	i=fdm_ival(fdm_hget(txt, FDM_LS("y")));

	if(x < 0)
	{
		/* cursor moved left of the bol;
		   effective cursor up + eol */
		if(i > 0)
		{
			ny=i - 1;
			line=fdm_aget(lines, ny);
			nx=line->size;
		}
	}
	else
	{
		/* test if moved beyond end of line */
		line=fdm_aget(lines, i);

		if(x > line->size)
		{
			if(i < lines->size - 1)
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
		fdm_hset(txt, FDM_LS("x"), FDM_I(nx));
		ret++;
	}

	if(ny >= 0)
	{
		fdm_hset(txt, FDM_LS("y"), FDM_I(ny));
		ret++;
	}

	return(ret);
}


int _mp_set_y(fdm_v cdata, int y)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;
	int nx, ny;
	int ret=0;

	txt=fdm_hget(cdata, FDM_LS("txt"));
	lines=fdm_hget(txt, FDM_LS("lines"));

	nx=ny=-1;

	/* never move beyond last line */
	ny=y > lines->size - 1 ? lines->size - 1 : y;

	/* gets new line */
	line=fdm_aget(lines, ny);

	/* test if y movement made x be far beyond current line eol */
	if(fdm_ival(fdm_hget(txt, FDM_LS("x"))) > line->size)
		nx=line->size;

	/* store new coords */
	if(nx >= 0)
	{
		fdm_hset(txt, FDM_LS("x"), FDM_I(nx));
		ret++;
	}

	if(ny >= 0)
	{
		fdm_hset(txt, FDM_LS("y"), FDM_I(ny));
		ret++;
	}

	return(ret);
}


void mp_move_up(fdm_v cdata)
{
	int y;

	_mp_get(cdata, NULL, &y, NULL, NULL);
	_mp_set_y(cdata, y - 1);
}


void mp_move_down(fdm_v cdata)
{
	int y;

	_mp_get(cdata, NULL, &y, NULL, NULL);
	_mp_set_y(cdata, y + 1);
}


void mp_move_bol(fdm_v cdata)
{
	_mp_set_x(cdata, 0);
}


void mp_move_eol(fdm_v cdata)
{
	int mx;

	_mp_get(cdata, NULL, NULL, &mx, NULL);
	_mp_set_x(cdata, mx);
}


void mp_move_bof(fdm_v cdata)
{
	_mp_set_y(cdata, 0);
	_mp_set_x(cdata, 0);
}


void mp_move_eof(fdm_v cdata)
{
	int my;

	_mp_get(cdata, NULL, NULL, NULL, &my);
	_mp_set_y(cdata, my);
}


void mp_move_left(fdm_v cdata)
{
	int x;

	_mp_get(cdata, &x, NULL, NULL, NULL);
	_mp_set_x(cdata, x - 1);
}


void mp_move_right(fdm_v cdata)
{
	int x;

	_mp_get(cdata, &x, NULL, NULL, NULL);
	_mp_set_x(cdata, x + 1);
}


void mp_move_xy(fdm_v cdata, int x, int y)
{
	_mp_set_y(cdata, y);
	_mp_set_x(cdata, x);
}


/* modifying */

void mp_save_undo(fdm_v cdata)
{
	fdm_v txt;
	fdm_v undo;
	int undo_levels;

	txt=fdm_hget(cdata, FDM_LS("txt"));
	if((undo=fdm_hget(cdata, FDM_LS("undo"))) == NULL)
	{
		undo=FDM_A(0);
		fdm_hset(cdata, FDM_LS("undo"), undo);
	}

	/* gets from config */
	if((undo_levels=fdm_ival(FDM_SGET(NULL, "mp.config.undo_levels"))) == 0)
		undo_levels=8;

	/* enqueue */
	fdm_aqueue(undo, fdm_clone(txt), undo_levels);
}


int mp_insert_line(fdm_v cdata)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;
	fdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=fdm_hget(cdata, FDM_LS("txt"));
	lines=fdm_hget(txt, FDM_LS("lines"));
	x=fdm_ival(fdm_hget(txt, FDM_LS("x")));
	y=fdm_ival(fdm_hget(txt, FDM_LS("y")));
	line=fdm_aget(lines, y);
	
	/* split current line in two */
	w=fdm_splice(line, NULL, x, line->size - x);

	/* store first part inside current line */
	fdm_aset(lines, fdm_aget(w, 0), y);

	/* move to next line */
	y++;

	/* insert a new line here */
	fdm_aexpand(lines, y, 1);

	/* store second part of line as a new one */
	fdm_aset(lines, fdm_aget(w, 1), y);

	/* move to that line, to the beginning */
	_mp_set_y(cdata, y);
	_mp_set_x(cdata, 0);

	return(1);
}


int mp_insert(fdm_v cdata, fdm_v str)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;
	fdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=fdm_hget(cdata, FDM_LS("txt"));
	lines=fdm_hget(txt, FDM_LS("lines"));
	x=fdm_ival(fdm_hget(txt, FDM_LS("x")));
	y=fdm_ival(fdm_hget(txt, FDM_LS("y")));
	line=fdm_aget(lines, y);
	
	/* insert */
	w=fdm_splice(line, str, x, 0);

	/* store as new line */
	fdm_aset(lines, fdm_aget(w, 0), y);

	/* move cursor right */
	_mp_set_x(cdata, x + str->size);

	return(1);
}


int mp_delete(fdm_v cdata)
{
	fdm_v txt;
	fdm_v lines;
	fdm_v line;
	fdm_v w;
	int x,y;

	mp_save_undo(cdata);

	txt=fdm_hget(cdata, FDM_LS("txt"));
	lines=fdm_hget(txt, FDM_LS("lines"));
	x=fdm_ival(fdm_hget(txt, FDM_LS("x")));
	y=fdm_ival(fdm_hget(txt, FDM_LS("y")));
	line=fdm_aget(lines, y);
	
	if(x == line->size && y < lines->size - 1)
	{
		/* deleting at end of line:
		   effectively join both lines */
		w=fdm_splice(line, fdm_aget(lines, y + 1), x, 0);

		/* store as new */
		fdm_aset(lines, fdm_aget(w, 0), y);

		/* collapse array (one line less) */
		fdm_acollapse(lines, y + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=fdm_splice(line, NULL, x, 1);

		/* set as new */
		fdm_aset(lines, fdm_aget(w, 0), y);
	}

	return(1);
}


#ifdef QQ
int mp_delete_char(fdm_v txt, int * x, int * y)
{
	fdm_v v;
	fdm_v w;

	v=fdm_aget(txt, *y);

	/* deleting a newline? */
	if(*x == v->size && *y < txt->size - 1)
	{
		/* joins both lines */
		w=fdm_splice(v, fdm_aget(txt, (*y) + 1), *x, 0);

		/* store */
		fdm_aset(txt, fdm_aget(w, 0), *y);

		/* collapse array (one line less) */
		fdm_acollapse(txt, (*y) + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=fdm_splice(v, NULL, *x, 1);

		/* set as new */
		fdm_aset(txt, fdm_aget(w, 0), *y);
	}

	return(1);
}
#endif

fdm_v mp_fgets(fdm_v fv)
{
	char line[128];
	fdm_v v=NULL;
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
		v=fdm_strcat(v, FDM_S(line));

		/* exit if the line is completely read */
		if(i == 0) break;
	}

	return(v);
}


fdm_v mp_load_file(char * file)
{
	FILE * f;
	fdm_v w;
	fdm_v v;
	fdm_v fv;
/*
	if((f=fopen(file, "r")) == NULL)
		return(NULL);

	fv=fdm_new(FDM_FILE, f, 0);

	w=FDM_A(0);

	while((v=mp_fgets(fv)) != NULL)
		fdm_apush(w, v);

	fclose(f);
*/

	if((fv=fdm_open(FDM_LS(file), FDM_LS("r"))) == NULL)
		return(NULL);

	w=FDM_A(0);

	while((v=fdm_read(fv)) != NULL)
		fdm_apush(w, v);

	fdm_close(fv);

	return(w);
}


int mp_startup(void)
{
	/* mp's root */
	_mp=FDM_H(7);

	/* store in fdm's root */
	fdm_hset(fdm_root(), FDM_LS("mp"), _mp);

	_mp_docs=FDM_A(0);

	fdm_hset(_mp, FDM_LS("docs"), _mp_docs);

	/* builds the document template */
	_mp_template=FDM_H(7);
	fdm_hset(_mp_template, FDM_LS("txt"), FDM_A(0));
	fdm_hset(_mp_template, FDM_LS("x"), FDM_I(0));
	fdm_hset(_mp_template, FDM_LS("y"), FDM_I(0));

	/* store in mp's root */
	fdm_hset(_mp, FDM_LS("template"), _mp_template);

	return(1);
}


int mp_shutdown(void)
{
	return(1);
}
