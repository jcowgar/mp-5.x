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

static void _mp_recover_x(fdm_v txt, int * x, int * y)
{
	fdm_v c;

	c=fdm_aget(txt, *y);

	/* try to move to previous column */
	if(*x > c->size)
		*x=c->size;
}


int mp_move_up(fdm_v txt, int * x, int * y)
{
	/* can't go up beyond the first line */
	if(*y > 0)
	{
		(*y)--;
		_mp_recover_x(txt, x, y);
		return(1);
	}

	return(0);
}


int mp_move_down(fdm_v txt, int * x, int * y)
{
	/* can't go down beyond the last line */
	if(*y < txt->size - 1)
	{
		(*y)++;
		_mp_recover_x(txt, x, y);
		return(1);
	}

	return(0);
}


void mp_move_bol(fdm_v txt, int * x, int * y)
{
	*x=0;
}


void mp_move_eol(fdm_v txt, int * x, int * y)
{
	fdm_v v;

	v=fdm_aget(txt, *y);
	*x=v->size;
}


void mp_move_bof(fdm_v txt, int * x, int * y)
{
	*x=*y=0;
}


void mp_move_eof(fdm_v txt, int * x, int * y)
{
	*y=txt->size - 1;
}


void mp_move_left(fdm_v txt, int * x, int * y)
{
	if(*x > 0)
		(*x)--;
	else
	{
		if(*y > 0)
		{
			(*y)--;
			mp_move_eol(txt, x, y);
		}
	}
}


void mp_move_right(fdm_v txt, int * x, int * y)
{
	fdm_v v;

	v=fdm_aget(txt, *y);

	if(*x < v->size)
		(*x)++;
	else
	{
		if(*y < txt->size - 1)
		{
			(*y)++;
			mp_move_bol(txt, x, y);
		}
	}
}


void mp_move_xy(fdm_v txt, int * x, int * y)
{
	if(*y > txt->size - 1)
		*y=txt->size - 1;

	_mp_recover_x(txt, x, y);
}


int mp_insert_char(fdm_v txt, int * x, int * y, int c)
{
	char tmp[2];
	fdm_v v;
	fdm_v w;

	v=fdm_aget(txt, *y);

	if(c == '\n')
	{
		/* split line in two */
		w=fdm_splice(v, NULL, *x, v->size - *x);

		/* store first part inside current line */
		fdm_aset(txt, fdm_aget(w, 0), *y);

		/* move to next line */
		(*y)++;

		/* insert a new line here */
		fdm_aexpand(txt, *y, 1);

		/* store second part of line as a new one */
		fdm_aset(txt, fdm_aget(w, 1), *y);

		/* move to bol */
		mp_move_bol(txt, x, y);
	}
	else
	{
		/* insert the char */
		tmp[0]=c; tmp[1]='\0';
		w=fdm_splice(v, FDM_LS(tmp), *x, 0);

		/* store as new line */
		fdm_aset(txt, fdm_aget(w, 0), *y);

		/* move one char right */
		mp_move_right(txt, x, y);
	}

	return(1);
}


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


fdm_v mp_fgets(FILE * f)
{
	char line[128];
	fdm_v v;
	int i;

	v=FDM_A(0);

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
		fdm_apush(v, FDM_S(line));

		/* exit if the line is completely read */
		if(i == 0) break;
	}

	/* if some lines were read, join and return */
	if(v->size > 0)
		return(fdm_ajoin(NULL, v));

	/* EOF */
	return(NULL);
}


fdm_v mp_load_file(char * file)
{
	FILE * f;
	fdm_v w;
	fdm_v v;

	if((f=fopen(file, "r")) == NULL)
		return(NULL);

	w=FDM_A(0);

	while((v=mp_fgets(f)) != NULL)
		fdm_apush(w, v);

	fclose(f);

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
