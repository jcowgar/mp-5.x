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

#include "mpdm.h"
#include "mp_core.h"

/*******************
	Data
********************/

mpdm_v cdata;

/*******************
	Code
********************/

void dump(void)
{
/*	mpdm_v v;
	int n, m;
	char * ptr;

	for(n=0;n < txt->size;n++)
	{
		v=mpdm_aget(txt, n);
		ptr=(char *)v->data;

		printf("[%d]", v->ref);

		for(m=0;m < v->size;m++)
		{
			if(m == x && n == y)
				printf("_");

			if(ptr[m] == '\0')
			{
				printf(">");
				break;
			}

			printf("%c", ptr[m]);
		}

			if(m == x && n == y)
				printf("_");

		printf("\n");
	}
*/

	{
		int x,y;
		mpdm_v lines;
		mpdm_v line;

		x=mpdm_ival(MPDM_SGET(cdata, "txt.x"));
		y=mpdm_ival(MPDM_SGET(cdata, "txt.y"));
		lines=MPDM_SGET(cdata, "txt.lines");
		line=mpdm_aget(lines, y);

		printf("[%c]\n", ((char *)line->data)[x]);
	}

	mpdm_sweep(0);
	mpdm_dump(cdata);

	getchar();
}


int main(void)
{
	int n;

	mp_startup();

/*	mpdm_dump(mpdm_root(), 0); */

	cdata=MPDM_H(7);
	mpdm_ref(cdata);
	MPDM_SSET(cdata, "txt", MPDM_H(7));
	MPDM_SSET(cdata, "txt.lines", mp_load_file("config.h"));
	MPDM_SSET(cdata, "txt.x", MPDM_I(0));
	MPDM_SSET(cdata, "txt.y", MPDM_I(0));
	MPDM_SSET(cdata, "undo", MPDM_A(0));

#ifdef QQ
	mpdm_ains(txt, MPDM_S("/* esto es la leche que te cagas"), 0);
	mpdm_ains(txt, MPDM_S("una prueba */"), 1);
	mpdm_ains(txt, MPDM_S("int main(void) { return 0;}"), 2);

	txt=mp_load_file("config.h");
	mpdm_ref(txt);
	mpdm_dump(txt);
#endif

	dump();

	mp_move_right(cdata);
	dump();
	mp_move_right(cdata);
	dump();

	mp_move_eol(cdata);
	dump();

	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();

	mp_move_down(cdata);
	dump();
	mp_move_down(cdata);
	dump();

	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();

	mp_move_bol(cdata);
	dump();

	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();
	mp_move_left(cdata);
	dump();

	mp_move_right(cdata);
	dump();
	mp_move_right(cdata);
	dump();
	mp_move_right(cdata);
	dump();
	mp_move_right(cdata);
	dump();

	mp_move_eol(cdata);
	dump();
	mp_move_right(cdata);
	dump();
	mp_move_right(cdata);
	dump();

	mp_move_up(cdata);
	dump();

	mp_insert_line(cdata);
	dump();

	mp_insert(cdata, MPDM_LS("uah!"));
	dump();
	mp_insert(cdata, MPDM_LS("UAH?"));
	dump();
	mp_move_eol(cdata);
	mp_insert(cdata, MPDM_LS("MUAHAHAHA!!!"));
	dump();

	mp_move_bol(cdata);
	mp_move_right(cdata);
	mp_move_right(cdata);
	mp_move_right(cdata);
	mp_delete(cdata);
	dump();

	mp_move_eol(cdata);
	mp_delete(cdata);
	dump();

	mp_move_eol(cdata);
	mp_delete(cdata);
	dump();

	mpdm_sweep(-1);
	dump();

#ifdef QQ
	mp_move_eol(txt, &x, &y);

	for(n=0;n < 8;n++)
	{
		dump();
		mp_move_left(txt, &x, &y);
	}

	dump();
	mpdm_copy(txt);
	mp_insert_char(txt, &x, &y, '-');
	dump();

	mpdm_copy(txt);
	mp_delete_char(txt, &x, &y);
	dump();

	mpdm_copy(txt);
	mp_insert_char(txt, &x, &y, '\n');
	dump();

	mp_move_eol(txt, &x, &y);
	dump();
	mp_insert_char(txt, &x, &y, '2');
	dump();

	mp_delete_char(txt, &x, &y);

	{
		int n;

		for(n=0;n < 25;n++)
			dump();
	}
#endif

	mp_shutdown();

	return(0);
}
