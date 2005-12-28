/*

    Minimum Profit - Programmer Text Editor

    Win32 driver.

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

#ifdef CONFOPT_WIN32

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/*******************
	Data
********************/

/*******************
	Code
********************/

int win32_drv_init(void)
{
/*
	gtk_driver = mpdm_ref(MPDM_H(0));

	mpdm_hset_s(gtk_driver, L"driver", MPDM_LS(L"gtk"));
	mpdm_hset_s(gtk_driver, L"startup", MPDM_X(gtk_drv_startup));
	mpdm_hset_s(gtk_driver, L"main_loop", MPDM_X(gtk_drv_main_loop));
	mpdm_hset_s(gtk_driver, L"shutdown", MPDM_X(gtk_drv_shutdown));

	mpdm_hset_s(mp, L"drv", gtk_driver);

	return(1);*/
	return(0);
}

#else /* CONFOPT_WIN32 */

int win32_drv_init(void)
{
	/* no Win32 */
	return(0);
}

#endif /* CONFOPT_WIN32 */
