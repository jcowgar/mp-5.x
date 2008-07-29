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
#include <wchar.h>
#include <unistd.h>
#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include <KMainWindow>
#include <KMenuBar>
#include <KStatusBar>

#include <KMessageBox>
#include <KFileDialog>
#include <KUrl>

/*******************
	Data
********************/

class MPWindow : public KMainWindow
{
	public:
		MPWindow(QWidget *parent=0);
               
/*  private:
    KTextEdit* textArea;*/
};

/* global data */
KApplication *app;
MPWindow *window;
KMenuBar *menubar;
KStatusBar *statusbar;

/*******************
	Code
********************/

static mpdm_t qstring_to_str(QString s)
/* converts a QString to an MPDM string */
{
	mpdm_t r = NULL;

	if (s != NULL) {
		wchar_t *wptr;
		int t = s.size();

		r = MPDM_NS(NULL, t + 1);
		wptr = (wchar_t *)r->data;

		s.toWCharArray(wptr);
		wptr[t] = L'\0';
	}

	return r;
}


static void build_menu(void)
{
	int n;
	mpdm_t m;

	/* gets the current menu */
	if ((m = mpdm_hget_s(mp, L"menu")) == NULL)
		return;

	menubar->clear();

	for (n = 0; n < mpdm_size(m); n++) {
		char *ptr;
		mpdm_t mi;
		mpdm_t v;
		int i;

		/* get the label and the items */
		mi = mpdm_aget(m, n);
		v = mpdm_aget(mi, 0);

		if ((ptr = mpdm_wcstombs(mpdm_string(mpdm_gettext(v)), NULL)) == NULL)
			continue;

		menubar->addMenu(ptr);

		free(ptr);
	}

	menubar->show();
}


MPWindow::MPWindow(QWidget *parent) : KMainWindow(parent)
{
	menubar = this->menuBar();
	statusbar = this->statusBar();

	build_menu();
}


static mpdm_t kde4_drv_update_ui(mpdm_t a)
{
	build_menu();

	return NULL;
}


static mpdm_t kde4_drv_alert(mpdm_t a)
/* alert driver function */
{
	char *cptr;

	/* 1# arg: prompt */
	cptr = mpdm_wcstombs(mpdm_string(mpdm_aget(a, 0)), NULL);

	KMessageBox::information(0, i18n(cptr), i18n("mp" VERSION));

	free(cptr);

	return NULL;
}

static mpdm_t kde4_drv_confirm(mpdm_t a)
/* confirm driver function */
{
	char *cptr;
	int r;

	/* 1# arg: prompt */
	cptr = mpdm_wcstombs(mpdm_string(mpdm_aget(a, 0)), NULL);

	r = KMessageBox::questionYesNoCancel(0, i18n(cptr), i18n("mp" VERSION));

	switch (r) {
	case KMessageBox::Yes:
		r = 1;
		break;

	case KMessageBox::No:
		r = 2;
		break;

	case KMessageBox::Cancel:
		r = 0;
		break;
	}

	free(cptr);

	return MPDM_I(r);
}


static mpdm_t kde4_drv_openfile(mpdm_t a)
{
	char *cptr;
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	cptr = mpdm_wcstombs(mpdm_string(mpdm_aget(a, 0)), NULL);

	r = KFileDialog::getOpenFileName(KUrl::fromPath(tmp), "*", 0, i18n(cptr));

	free(cptr);

	return qstring_to_str(r);
}


static mpdm_t kde4_drv_savefile(mpdm_t a)
{
	char *cptr;
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	cptr = mpdm_wcstombs(mpdm_string(mpdm_aget(a, 0)), NULL);

	r = KFileDialog::getSaveFileName(KUrl::fromPath(tmp), "*", 0, i18n(cptr));

	free(cptr);

	return qstring_to_str(r);
}


static mpdm_t kde4_drv_main_loop(mpdm_t a)
{
	app->exec();

	return NULL;
}


static mpdm_t kde4_drv_shutdown(mpdm_t a)
{
	mpdm_t v;

	if ((v = mpdm_hget_s(mp, L"exit_message")) != NULL) {
		mpdm_write_wcs(stdout, mpdm_string(v));
		printf("\n");
	}

	return NULL;
}


static void register_functions(void)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"main_loop", MPDM_X(kde4_drv_main_loop));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(kde4_drv_shutdown));

/*	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(kde4_drv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(kde4_drv_sys_to_clip));*/
	mpdm_hset_s(drv, L"update_ui", MPDM_X(kde4_drv_update_ui));
/*	mpdm_hset_s(drv, L"timer", MPDM_X(kde4_drv_timer));
	mpdm_hset_s(drv, L"busy", MPDM_X(kde4_drv_busy));*/

	mpdm_hset_s(drv, L"alert", MPDM_X(kde4_drv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(kde4_drv_confirm));
	mpdm_hset_s(drv, L"openfile", MPDM_X(kde4_drv_openfile));
	mpdm_hset_s(drv, L"savefile", MPDM_X(kde4_drv_savefile));
/*	mpdm_hset_s(drv, L"form", MPDM_X(kde4_drv_form));*/
}


static mpdm_t kde4_drv_startup(mpdm_t a)
/* driver initialization */
{
	register_functions();

	window = new MPWindow();
	window->show();

	return NULL;
}


extern "C" int kde4_drv_detect(int * argc, char *** argv)
{
	mpdm_t drv;
	KCmdLineOptions opts;

	KAboutData aboutData(
		"mp", 0,
		ki18n("Minimum Profit"), VERSION,
		ki18n("A programmer's text editor"),
		KAboutData::License_GPL,
		ki18n("Copyright (c) 1991-2008 Angel Ortega"),
		ki18n(""),
		"http://triptico.com",
		"angel@triptico.com"
	);

	KCmdLineArgs::init(*argc, *argv, &aboutData);

	/* command line options should be inserted here (I don't like this) */
	opts.add("t {tag}", ki18n("Edits the file where tag is defined"));
	opts.add("e {mpsl_code}", ki18n("Executes MPSL code"));
	opts.add("f {mpsl_script}", ki18n("Executes MPSL script file"));
	opts.add(" +NNN", ki18n("Moves to line number NNN of last file"));
	opts.add("+[file(s)]", ki18n("Documents to open"));
	KCmdLineArgs::addCmdLineOptions(opts);

	/* this is where it crashes if no X server */
	app = new KApplication();

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"kde4"));
	mpdm_hset_s(drv, L"startup", MPDM_X(kde4_drv_startup));

	return 1;
}
