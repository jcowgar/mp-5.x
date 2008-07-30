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

#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>

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
		MPWindow(QWidget *parent = 0);
		bool queryExit(void);
		bool event(QEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void keyReleaseEvent(QKeyEvent *event);
/*  private:
    KTextEdit* textArea;*/
};

class MPArea : public QWidget
{
	public:
		MPArea(QWidget *parent = 0);

	protected:
		void paintEvent(QPaintEvent *event);
};

/* global data */
KApplication *app;
MPWindow *window;
MPArea *area;
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
		int t = s.size();
		wchar_t *wptr = (wchar_t *)malloc((t + 1) * sizeof(wchar_t));

		r = MPDM_ENS(wptr, t);

		s.toWCharArray(wptr);
		wptr[t] = L'\0';
	}

	return r;
}


QString str_to_qstring(mpdm_t s)
/* converts an MPDM string to a QString */
{
	char *ptr = mpdm_wcstombs(mpdm_string(s), NULL);
	QString qs(ptr);
	free(ptr);

	return qs;
}


QBrush *papers = NULL;
int normal_attr = 0;

static void build_colors(void)
/* builds the colors */
{
	mpdm_t colors;
	mpdm_t l;
	mpdm_t c;
	int n, s;

	/* gets the color definitions and attribute names */
	colors = mpdm_hget_s(mp, L"colors");
	l = mpdm_keys(colors);
	s = mpdm_size(l);

	/* redim the structures */
//	inks = realloc(inks, sizeof(GdkColor) * s);
	papers = (QBrush *)realloc(papers, sizeof(QBrush) * s);

	/* loop the colors */
	for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
		int rgb;
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"gui");

		/* store the 'normal' attribute */
		if (wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		rgb = mpdm_ival(mpdm_aget(v, 1));

		papers[n] = QBrush(QColor::fromRgbF(
			(float) ((rgb & 0x00ff0000) >> 16)	/ 256.0,
			(float) ((rgb & 0x0000ff00) >> 8)	/ 256.0,
			(float) ((rgb & 0x000000ff))		/ 256.0,
			1));
	}
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
		mpdm_t mi;
		mpdm_t v;
		int i;

		/* get the label and the items */
		mi = mpdm_aget(m, n);
		v = mpdm_aget(mi, 0);

		menubar->addMenu(str_to_qstring(mpdm_gettext(v)));
	}

	menubar->show();
}


static void draw_status(void)
{
	statusbar->changeItem(str_to_qstring(mp_build_status_line()), 0);
}


/* MPArea class methods */

MPArea::MPArea(QWidget *parent) : QWidget(parent)
{
}


void MPArea::paintEvent(QPaintEvent *) 
{ 
	int h = (fontMetrics().height() * 5) / 4;
	mpdm_t w, v;
	int n, m, y;

	QPainter painter(area);

/*	if (papers == NULL)
		build_colors();*/

	painter.setFont(QFont(QString("Mono"), 14));

	/* calculate window size */
	w = mpdm_hget_s(mp, L"window");
	mpdm_hset_s(w, L"tx", MPDM_I(this->width() / fontMetrics().width("M")));
	mpdm_hset_s(w, L"ty", MPDM_I(this->height() / h));

	w = mp_draw(mp_active(), 0);
	y = 16;

	painter.setBackgroundMode(Qt::OpaqueMode);
	painter.setBackground(QBrush(QColor::fromRgbF(1,1,1,1)));

	painter.setBrush(QBrush(QColor::fromRgbF(1,1,1,1)));
	painter.drawRect(0, 0, this->width(), this->height());

	for (n = 0; n < mpdm_size(w); n++) {
		mpdm_t l = mpdm_aget(w, n);
		int x = 0;

		if (l == NULL)
			continue;

		for (m = 0; m < mpdm_size(l); m++) {
			int attr;
			mpdm_t s;
			char *ptr;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			ptr = mpdm_wcstombs(mpdm_string(s), NULL);

			painter.drawText(x, y, QString(ptr));

			x += fontMetrics().width(ptr);

			free(ptr);
		}

		y += h;
	}

	draw_status();
}


/* MPWindow class methods */

MPWindow::MPWindow(QWidget *parent) : KMainWindow(parent)
{
	this->setAutoSaveSettings(QLatin1String("Minimum Profit"), true);

	menubar = this->menuBar();
	build_menu();

	statusbar = this->statusBar();
	statusbar->insertItem("mp " VERSION, 0);

	/* main area */
	area = new MPArea(this);
	setCentralWidget(area);
}


bool MPWindow::queryExit(void)
{
	mp_process_event(MPDM_LS(L"close-window"));

	return mp_exit_requested ? true : false;
}


bool MPWindow::event(QEvent *event)
{
	/* special events */

/*	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *ke = (QKeyEvent *)event;
	}*/

	return QWidget::event(event);
}


void MPWindow::keyReleaseEvent(QKeyEvent *event)
{
	if (!event->isAutoRepeat() && mp_keypress_throttle(0))
		area->update();
}


void MPWindow::keyPressEvent(QKeyEvent *event)
{
	mpdm_t k = NULL;
	wchar_t *ptr = NULL;

	/* set mp.shift_pressed */
	if (event->modifiers() & Qt::ShiftModifier)
		mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

	if (event->modifiers() & Qt::ControlModifier) {
		switch (event->key()) {
		case Qt::Key_Up:		ptr = L"ctrl-cursor-up"; break;
		case Qt::Key_Down:		ptr = L"ctrl-cursor-down"; break;
		case Qt::Key_Left:		ptr = L"ctrl-cursor-left"; break;
		case Qt::Key_Right:		ptr = L"ctrl-cursor-right"; break;
		case Qt::Key_PageUp:		ptr = L"ctrl-page-up"; break;
		case Qt::Key_PageDown:		ptr = L"ctrl-page-down"; break;
		case Qt::Key_Home:		ptr = L"ctrl-home"; break;
		case Qt::Key_End:		ptr = L"ctrl-end"; break;
		case Qt::Key_Space:		ptr = L"ctrl-space"; break;
		case Qt::Key_F1:		ptr = L"ctrl-f1"; break;
		case Qt::Key_F2:		ptr = L"ctrl-f2"; break;
		case Qt::Key_F3:		ptr = L"ctrl-f3"; break;
		case Qt::Key_F4:		ptr = L"ctrl-f4"; break;
		case Qt::Key_F5:		ptr = L"ctrl-f5"; break;
		case Qt::Key_F6:		ptr = L"ctrl-f6"; break;
		case Qt::Key_F7:		ptr = L"ctrl-f7"; break;
		case Qt::Key_F8:		ptr = L"ctrl-f8"; break;
		case Qt::Key_F9:		ptr = L"ctrl-f9"; break;
		case Qt::Key_F10:		ptr = L"ctrl-f10"; break;
		case Qt::Key_F11:		ptr = L"ctrl-f11"; break;
		case Qt::Key_F12:		ptr = L"ctrl-f12"; break;
		case 'A':			ptr = L"ctrl-a"; break;
		case 'B':			ptr = L"ctrl-b"; break;
		case 'C':			ptr = L"ctrl-c"; break;
		case 'D':			ptr = L"ctrl-d"; break;
		case 'E':			ptr = L"ctrl-e"; break;
		case 'F':			ptr = L"ctrl-f"; break;
		case 'G':			ptr = L"ctrl-g"; break;
		case 'H':			ptr = L"ctrl-h"; break;
		case 'I':			ptr = L"ctrl-i"; break;
		case 'J':			ptr = L"ctrl-j"; break;
		case 'K':			ptr = L"ctrl-k"; break;
		case 'L':			ptr = L"ctrl-l"; break;
		case 'M':			ptr = L"ctrl-m"; break;
		case 'N':			ptr = L"ctrl-n"; break;
		case 'O':			ptr = L"ctrl-o"; break;
		case 'P':			ptr = L"ctrl-p"; break;
		case 'Q':			ptr = L"ctrl-q"; break;
		case 'R':			ptr = L"ctrl-r"; break;
		case 'S':			ptr = L"ctrl-s"; break;
		case 'T':			ptr = L"ctrl-t"; break;
		case 'U':			ptr = L"ctrl-u"; break;
		case 'V':			ptr = L"ctrl-v"; break;
		case 'W':			ptr = L"ctrl-w"; break;
		case 'X':			ptr = L"ctrl-x"; break;
		case 'Y':			ptr = L"ctrl-y"; break;
		case 'Z':			ptr = L"ctrl-z"; break;
		case Qt::Key_Return:
		case Qt::Key_Enter:		ptr = L"ctrl-enter"; break;

		default:
			break;
		}
	}
	else {
		switch (event->key()) {
		case Qt::Key_Up:		ptr = L"cursor-up"; break;
		case Qt::Key_Down:		ptr = L"cursor-down"; break;
		case Qt::Key_Left:		ptr = L"cursor-left"; break;
		case Qt::Key_Right:		ptr = L"cursor-right"; break;
		case Qt::Key_PageUp:		ptr = L"page-up"; break;
		case Qt::Key_PageDown:		ptr = L"page-down"; break;
		case Qt::Key_Home:		ptr = L"home"; break;
		case Qt::Key_End:		ptr = L"end"; break;
		case Qt::Key_Space:		ptr = L"space"; break;
		case Qt::Key_F1:		ptr = L"f1"; break;
		case Qt::Key_F2:		ptr = L"f2"; break;
		case Qt::Key_F3:		ptr = L"f3"; break;
		case Qt::Key_F4:		ptr = L"f4"; break;
		case Qt::Key_F5:		ptr = L"f5"; break;
		case Qt::Key_F6:		ptr = L"f6"; break;
		case Qt::Key_F7:		ptr = L"f7"; break;
		case Qt::Key_F8:		ptr = L"f8"; break;
		case Qt::Key_F9:		ptr = L"f9"; break;
		case Qt::Key_F10:		ptr = L"f10"; break;
		case Qt::Key_F11:		ptr = L"f11"; break;
		case Qt::Key_F12:		ptr = L"f12"; break;
		case Qt::Key_Insert:		ptr = L"insert"; break;
		case Qt::Key_Backspace:		ptr = L"backspace"; break;
		case Qt::Key_Delete:		ptr = L"delete"; break;
		case Qt::Key_Return:
		case Qt::Key_Enter:		ptr = L"enter"; break;
		case Qt::Key_Escape:		ptr = L"escape"; break;

		default:
			break;
		}
	}

	if (ptr == NULL)
		k = qstring_to_str(event->text());
	else
		k = MPDM_S(ptr);

	if (k != NULL)
		mp_process_event(k);

	if (mp_keypress_throttle(1))
		area->update();
}


/* driver functions */

static mpdm_t kde4_drv_update_ui(mpdm_t a)
{
	build_menu();

	return NULL;
}


static mpdm_t kde4_drv_alert(mpdm_t a)
/* alert driver function */
{
	/* 1# arg: prompt */
	KMessageBox::information(0, str_to_qstring(mpdm_aget(a, 0)),
		i18n("mp " VERSION));

	return NULL;
}

static mpdm_t kde4_drv_confirm(mpdm_t a)
/* confirm driver function */
{
	int r;

	/* 1# arg: prompt */
	r = KMessageBox::questionYesNoCancel(0,
		str_to_qstring(mpdm_aget(a, 0)), i18n("mp" VERSION));

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

	return MPDM_I(r);
}


static mpdm_t kde4_drv_openfile(mpdm_t a)
{
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	r = KFileDialog::getOpenFileName(KUrl::fromPath(tmp), "*", 0,
		str_to_qstring(mpdm_aget(a, 0)));

	return qstring_to_str(r);
}


static mpdm_t kde4_drv_savefile(mpdm_t a)
{
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	r = KFileDialog::getSaveFileName(KUrl::fromPath(tmp), "*", 0,
		str_to_qstring(mpdm_aget(a, 0)));

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
