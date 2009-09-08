/*

    Minimum Profit - Programmer Text Editor

    Qt4 driver.

    Copyright (C) 2009 Angel Ortega <angel@triptico.com>

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
extern "C" int qt4_drv_detect(int * argc, char *** argv);

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"
#include "mp.xpm"

#include <QtGui>

/** data **/

class MPWindow : public QMainWindow
{
	public:
		MPWindow(QWidget *parent = 0);
		bool queryExit(void);
		bool event(QEvent *event);

		QSettings *settings;
};

class MPArea : public QWidget
{
	Q_OBJECT

	public:
		MPArea(QWidget *parent = 0);
		void inputMethodEvent(QInputMethodEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void keyReleaseEvent(QKeyEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void wheelEvent(QWheelEvent *event);
		void dragEnterEvent(QDragEnterEvent *event);
		void dropEvent(QDropEvent *event);
		bool event(QEvent *event);

	protected:
		void paintEvent(QPaintEvent *event);

	public slots:
		void from_scrollbar(int);
		void from_filetabs(int);
		void from_menu(QAction *);
};

/* global data */
QApplication *app;
MPWindow *window;
QMenuBar *menubar;
QStatusBar *statusbar;
QTabBar *file_tabs;

#define MENU_CLASS QMenu

#include "mpv_qk_common.cpp"

static void draw_status(void)
{
	statusbar->showMessage(str_to_qstring(mp_build_status_line()));
}


/** MPWindow methods **/

MPWindow::MPWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("MP " VERSION);

	menubar = this->menuBar();
	build_menu();

	statusbar = this->statusBar();

	/* the full container */
	QVBoxLayout *vb = new QVBoxLayout(this);

	file_tabs = new QTabBar();
	file_tabs->setFocusPolicy(Qt::NoFocus);

	QHBoxLayout *hb = new QHBoxLayout();

	/* main area */
	area = new MPArea();
	scrollbar = new QScrollBar();
	scrollbar->setFocusPolicy(Qt::NoFocus);

	hb->addWidget(area);
	hb->addWidget(scrollbar);
	QWidget *cc = new QWidget();
	cc->setLayout(hb);

	vb->addWidget(file_tabs);
	vb->addWidget(cc);
	QWidget *mc = new QWidget();
	mc->setLayout(vb);

	setCentralWidget(mc);

	connect(scrollbar, SIGNAL(valueChanged(int)),
		area, SLOT(from_scrollbar(int)));

	connect(file_tabs, SIGNAL(currentChanged(int)),
		area, SLOT(from_filetabs(int)));

	connect(menubar, SIGNAL(triggered(QAction *)),
		area, SLOT(from_menu(QAction *)));

	this->setWindowIcon(QIcon(QPixmap(mp_xpm)));

	settings = new QSettings("triptico.com", "MinimumProfit");

	QPoint pos = settings->value("pos", QPoint(20, 20)).toPoint();
	QSize size = settings->value("size", QSize(600, 400)).toSize();

	move(pos);
	resize(size);
}


static void save_settings(MPWindow *w)
{
	w->settings->setValue("pos", w->pos());
	w->settings->setValue("size", w->size());
	w->settings->sync();
}


bool MPWindow::queryExit(void)
{
	mp_process_event(MPDM_LS(L"close-window"));

	save_settings(this);

	return mp_exit_requested ? true : false;
}


bool MPWindow::event(QEvent *event)
{
	/* do the processing */
	bool r = QWidget::event(event);

	if (mp_exit_requested) {
		save_settings(this);
		exit(0);
	}

	return r;
}


/** driver functions **/

static mpdm_t qt4_drv_alert(mpdm_t a)
{
	return NULL;
}


static mpdm_t qt4_drv_confirm(mpdm_t a)
{
	return NULL;
}


static mpdm_t qt4_drv_openfile(mpdm_t a)
{
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	r = QFileDialog::getOpenFileName(window,
		str_to_qstring(mpdm_aget(a, 0)), tmp);

	return qstring_to_str(r);
}


static mpdm_t qt4_drv_savefile(mpdm_t a)
{
	QString r;
	char tmp[128];

	getcwd(tmp, sizeof(tmp));

	/* 1# arg: prompt */
	r = QFileDialog::getSaveFileName(window,
		str_to_qstring(mpdm_aget(a, 0)), tmp);

	return qstring_to_str(r);
}


static mpdm_t qt4_drv_form(mpdm_t a)
{
	return NULL;
}


static void register_functions(void)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"main_loop", MPDM_X(qt4_drv_main_loop));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(qt4_drv_shutdown));

	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(qt4_drv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(qt4_drv_sys_to_clip));
	mpdm_hset_s(drv, L"update_ui", MPDM_X(qt4_drv_update_ui));
/*	mpdm_hset_s(drv, L"timer", MPDM_X(qt4_drv_timer));*/
	mpdm_hset_s(drv, L"busy", MPDM_X(qt4_drv_busy));

	mpdm_hset_s(drv, L"alert", MPDM_X(qt4_drv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(qt4_drv_confirm));
	mpdm_hset_s(drv, L"openfile", MPDM_X(qt4_drv_openfile));
	mpdm_hset_s(drv, L"savefile", MPDM_X(qt4_drv_savefile));
	mpdm_hset_s(drv, L"form", MPDM_X(qt4_drv_form));
}


static mpdm_t qt4_drv_startup(mpdm_t a)
/* driver initialization */
{
	register_functions();

	build_font(1);
	build_colors();

	window = new MPWindow();
	window->show();

	return NULL;
}

extern "C" Display *XOpenDisplay(char *);

extern "C" int qt4_drv_detect(int * argc, char *** argv)
{
	mpdm_t drv;
	Display *x11_display;

	/* try connecting directly to the Xserver */
	if ((x11_display = XOpenDisplay((char *)NULL)) == NULL)
		return 0;

	/* this is where it crashes if no X server */
	app = new QApplication(x11_display);

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"qt4"));
	mpdm_hset_s(drv, L"startup", MPDM_X(qt4_drv_startup));

	return 1;
}

#include "mpv_qt4.moc"
