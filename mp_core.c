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

#include "fdm.h"
#include "mp_core.h"

/*******************
	Data
********************/

/* root hash */
fdm_v _mp=NULL;

/* array of documents */
fdm_v _mp_docs=NULL;


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
		if(mp_move_up(txt, x, y))
			mp_move_eol(txt, x, y);
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
		if(mp_move_down(txt, x, y))
			mp_move_bol(txt, x, y);
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
		w=fdm_splice(v, FDM_LS(""), *x, v->size - *x);

		/* store first part inside current line */
		fdm_aset(txt, fdm_aget(w, 0), *y);

		/* move to next line */
/*		mp_move_down(txt, x, y); */
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
	if(*x == v->size)
	{
		/* creates a two element array */
		w=FDM_A(2);

		/* insert current line as elem 0 */
		fdm_aset(w, v, 0);

		/* insert next line as elem 1 */
		fdm_aset(w, fdm_aget(txt, (*y) + 1), 1);

		/* join using "" as joiner */
		v=fdm_ajoin(FDM_LS(""), w);

		/* set the joined element as the new line */
		fdm_aset(txt, v, *y);

		/* set lower line as NULL (unreferencing it) */
		fdm_aset(txt, NULL, (*y) + 1);

		/* collapse array (one line less) */
		fdm_acollapse(txt, (*y) + 1, 1);
	}
	else
	{
		/* creates a new string without current char */
		w=fdm_splice(v, FDM_LS(""), *x, 1);

		/* set as new */
		fdm_aset(txt, fdm_aget(w, 0), *y);
	}

	return(1);
}


int mp_startup(void)
{
	/* mp's root */
	_mp=FDM_H(7);

	/* store in fdm's root */
	fdm_hset(fdm_root(), FDM_LS("mp"), _mp);

	fdm_hset(_mp, FDM_LS("docs"), FDM_A(0));
	fdm_hset(_mp, FDM_LS("keys"), FDM_H(0));

	return(1);
}


int mp_shutdown(void)
{
	return(1);
}
