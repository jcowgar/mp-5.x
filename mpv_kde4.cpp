/*

    Minimum Profit - Programmer Text Editor

    KDE4 driver.

    Copyright (C) 2008 Angel Ortega <angel@triptico.com>

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

/* override auto-generated definition in config.h */
extern "C" int kde4_drv_detect(int * argc, char *** argv);

#include "config.h"

#include <stdio.h>
#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

extern "C" int kde4_drv_detect(int * argc, char *** argv)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"kde4"));
/*	mpdm_hset_s(drv, L"startup", MPDM_X(gtk_drv_startup));*/

	return 0;
}

