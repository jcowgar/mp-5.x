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

fdm_v txt;
int x;
int y;

/*******************
	Code
********************/

void dump(void)
{
/*	fdm_v v;
	int n, m;
	char * ptr;

	for(n=0;n < txt->size;n++)
	{
		v=fdm_aget(txt, n);
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
		fdm_v v;

		v=fdm_aget(txt, y);
		printf("[%c]\n", ((char *)v->data)[x]);
	}

	fdm_sweep(0);
	fdm_dump(txt, 0);

	getchar();
}


int main(void)
{
	int n;

	mp_startup();

	fdm_dump(fdm_root(), 0);

	txt=FDM_A(0);
	fdm_ref(txt);

	fdm_ains(txt, FDM_S("/* esto es la leche que te cagas"), 0);
	fdm_ains(txt, FDM_S("una prueba */"), 1);
	fdm_ains(txt, FDM_S("int main(void) { return 0;}"), 2);

	txt=mp_load_file("config.h");
	fdm_ref(txt);
	fdm_dump(txt, 0);

	mp_move_eol(txt, &x, &y);

	for(n=0;n < 8;n++)
	{
		dump();
		mp_move_left(txt, &x, &y);
	}

	dump();
	fdm_copy(txt);
	mp_insert_char(txt, &x, &y, '-');
	dump();

	fdm_copy(txt);
	mp_delete_char(txt, &x, &y);
	dump();

	fdm_copy(txt);
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

	mp_shutdown();

	return(0);
}
