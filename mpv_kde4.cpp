/*

    Minimum Profit - Programmer Text Editor

    KDE4 driver.

    Copyright (C) 2008-2009 Angel Ortega <angel@triptico.com>

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
#include "mp.xpm"

#include <QtGui/QKeyEvent>
#include <QtGui/QPainter>
#include <QtGui/QMenu>

#include <QtGui/QLabel>
#include <QtGui/QComboBox>
#include <QtGui/QLineEdit>
#include <QtGui/QCheckBox>
#include <QtGui/QListWidget>
#include <QtGui/QScrollBar>
#include <QtGui/QClipboard>

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include <KMainWindow>
#include <KMenuBar>
#include <KStatusBar>
#include <KMenu>
#include <KTabBar>

#include <KVBox>
#include <KHBox>

#include <KDialog>
#include <KMessageBox>
#include <KFileDialog>
#include <KUrl>

/** data **/

class MPWindow : public KMainWindow
{
	public:
		MPWindow(QWidget *parent = 0);
		bool queryExit(void);
		bool event(QEvent *event);
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
KApplication *app;
MPWindow *window;
KMenuBar *menubar;
KStatusBar *statusbar;
KTabBar *file_tabs;

#define MENU_CLASS KMenu

#include "mpv_qk_common.cpp"

static void draw_status(void)
{
	statusbar->changeItem(str_to_qstring(mp_build_status_line()), 0);
}

/** MPWindow methods **/

MPWindow::MPWindow(QWidget *parent) : KMainWindow(parent)
{
	menubar = this->menuBar();
	build_menu();

	statusbar = this->statusBar();
	statusbar->insertItem("mp " VERSION, 0);

	/* the full container */
	KVBox *vb = new KVBox(this);

	file_tabs = new KTabBar(vb);
	file_tabs->setFocusPolicy(Qt::NoFocus);

	KHBox *hb = new KHBox(vb);

	/* main area */
	area = new MPArea(hb);
	scrollbar = new QScrollBar(hb);
	scrollbar->setFocusPolicy(Qt::NoFocus);

	setCentralWidget(vb);

	connect(scrollbar, SIGNAL(valueChanged(int)),
		area, SLOT(from_scrollbar(int)));

	connect(file_tabs, SIGNAL(currentChanged(int)),
		area, SLOT(from_filetabs(int)));

	connect(menubar, SIGNAL(triggered(QAction *)),
		area, SLOT(from_menu(QAction *)));

	this->setWindowIcon(QIcon(QPixmap(mp_xpm)));

	this->setAutoSaveSettings(QLatin1String("MinimumProfit"), true);
}


bool MPWindow::queryExit(void)
{
	mp_process_event(MPDM_LS(L"close-window"));

	this->saveAutoSaveSettings();

	return mp_exit_requested ? true : false;
}


bool MPWindow::event(QEvent *event)
{
	/* do the processing */
	bool r = QWidget::event(event);

	if (mp_exit_requested) {
		this->saveAutoSaveSettings();
		kde4_drv_shutdown(NULL);
		exit(0);
	}

	return r;
}


/** driver functions **/

static mpdm_t kde4_drv_update_ui(mpdm_t a)
{
	return qt4_drv_update_ui(a);
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


static mpdm_t kde4_drv_form(mpdm_t a)
{
	int n;
	mpdm_t widget_list;
	QWidget *qlist[100];
	mpdm_t r;

	KDialog *dialog = new KDialog(window);

	dialog->setModal(true);
	dialog->setButtons(KDialog::Ok | KDialog::Cancel);

	widget_list = mpdm_aget(a, 0);

	KVBox *vb = new KVBox(dialog);
	dialog->setMainWidget(vb);

	for (n = 0; n < mpdm_size(widget_list); n++) {
		mpdm_t w = mpdm_aget(widget_list, n);
		wchar_t *type;
		mpdm_t t;
		KHBox *hb = new KHBox(vb);

		type = mpdm_string(mpdm_hget_s(w, L"type"));

		if ((t = mpdm_hget_s(w, L"label")) != NULL) {
			QLabel *ql = new QLabel(hb);
			ql->setText(str_to_qstring(mpdm_gettext(t)));
		}

		t = mpdm_hget_s(w, L"value");

		if (wcscmp(type, L"text") == 0) {
			mpdm_t h;
			QComboBox *ql = new QComboBox(hb);

			ql->setEditable(true);
			ql->setMinimumContentsLength(30);
			ql->setMaxVisibleItems(8);

			if (t != NULL)
				ql->setEditText(str_to_qstring(t));

			qlist[n] = ql;

			if ((h = mpdm_hget_s(w, L"history")) != NULL) {
				int i;

				/* has history; fill it */
				h = mp_get_history(h);

				for (i = mpdm_size(h) - 1; i >= 0; i--)
					ql->addItem(str_to_qstring(mpdm_aget(h, i)));
			}
		}
		else
		if (wcscmp(type, L"password") == 0) {
			QLineEdit *ql = new QLineEdit(hb);

			ql->setEchoMode(QLineEdit::Password);

			qlist[n] = ql;
		}
		else
		if (wcscmp(type, L"checkbox") == 0) {
			QCheckBox *qc = new QCheckBox(hb);

			if (mpdm_ival(t))
				qc->setCheckState(Qt::Checked);

			qlist[n] = qc;
		}
		else
		if (wcscmp(type, L"list") == 0) {
			int i;
			QListWidget *ql = new QListWidget(hb);
			ql->setMinimumWidth(480);

			/* use a monospaced font */
			ql->setFont(QFont(QString("Mono")));

			mpdm_t l = mpdm_hget_s(w, L"list");

			for (i = 0; i < mpdm_size(l); i++)
				ql->addItem(str_to_qstring(mpdm_aget(l, i)));

			ql->setCurrentRow(mpdm_ival(t));

			qlist[n] = ql;
		}

		if (n == 0)
			qlist[n]->setFocus(Qt::OtherFocusReason);
	}

	n = dialog->exec();

	if (!n)
		return NULL;

	r = MPDM_A(mpdm_size(widget_list));

	/* fill the return values */
	for (n = 0; n < mpdm_size(widget_list); n++) {
		mpdm_t w = mpdm_aget(widget_list, n);
		mpdm_t v = NULL;
		wchar_t *type;

		type = mpdm_string(mpdm_hget_s(w, L"type"));

		if (wcscmp(type, L"text") == 0) {
			mpdm_t h;
			QComboBox *ql = (QComboBox *)qlist[n];

			v = qstring_to_str(ql->currentText());

			/* if it has history, add to it */
			if ((h = mpdm_hget_s(w, L"history")) != NULL &&
				v != NULL && mpdm_cmp(v, MPDM_LS(L"")) != 0) {
				h = mp_get_history(h);

				if (mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
					mpdm_push(h, v);
			}
		}
		else
		if (wcscmp(type, L"password") == 0) {
			QLineEdit *ql = (QLineEdit *)qlist[n];

			v = qstring_to_str(ql->text());
		}
		else
		if (wcscmp(type, L"checkbox") == 0) {
			QCheckBox *qb = (QCheckBox *)qlist[n];

			v = MPDM_I(qb->checkState() == Qt::Checked);
		}
		else
		if (wcscmp(type, L"list") == 0) {
			QListWidget *ql = (QListWidget *)qlist[n];

			v = MPDM_I(ql->currentRow());
		}

		mpdm_aset(r, v, n);
	}

	return r;
}


static mpdm_t kde4_drv_busy(mpdm_t a)
{
	return qt4_drv_busy(a);
}


static mpdm_t kde4_drv_main_loop(mpdm_t a)
{
	return qt4_drv_main_loop(a);
}


static mpdm_t kde4_drv_shutdown(mpdm_t a)
{
	return qt4_drv_shutdown(a);
}


static mpdm_t kde4_drv_clip_to_sys(mpdm_t a)
{
	return qt4_drv_clip_to_sys(a);
}


static mpdm_t kde4_drv_sys_to_clip(mpdm_t a)
{
	return qt4_drv_sys_to_clip(a);
}


static void register_functions(void)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"main_loop", MPDM_X(kde4_drv_main_loop));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(kde4_drv_shutdown));

	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(kde4_drv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(kde4_drv_sys_to_clip));
	mpdm_hset_s(drv, L"update_ui", MPDM_X(kde4_drv_update_ui));
/*	mpdm_hset_s(drv, L"timer", MPDM_X(kde4_drv_timer));*/
	mpdm_hset_s(drv, L"busy", MPDM_X(kde4_drv_busy));

	mpdm_hset_s(drv, L"alert", MPDM_X(kde4_drv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(kde4_drv_confirm));
	mpdm_hset_s(drv, L"openfile", MPDM_X(kde4_drv_openfile));
	mpdm_hset_s(drv, L"savefile", MPDM_X(kde4_drv_savefile));
	mpdm_hset_s(drv, L"form", MPDM_X(kde4_drv_form));
}


static mpdm_t kde4_drv_startup(mpdm_t a)
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

extern "C" int kde4_drv_detect(int * argc, char *** argv)
{
	mpdm_t drv;
	KCmdLineOptions opts;
	Display *x11_display;

	/* try connecting directly to the Xserver */
	if ((x11_display = XOpenDisplay((char *)NULL)) == NULL)
		return 0;

	KAboutData aboutData(
		"mp", 0,
		ki18n("Minimum Profit"), VERSION,
		ki18n("A programmer's text editor"),
		KAboutData::License_GPL,
		ki18n("Copyright (c) 1991-2009 Angel Ortega"),
		ki18n(""),
		"http://triptico.com",
		"angel@triptico.com"
	);

	KCmdLineArgs::init(*argc, *argv, &aboutData);

	/* command line options should be inserted here (I don't like this) */
	opts.add("t {tag}", ki18n("Edits the file where tag is defined"));
	opts.add("e {mpsl_code}", ki18n("Executes MPSL code"));
	opts.add("f {mpsl_script}", ki18n("Executes MPSL script file"));
	opts.add("d {directory}", ki18n("Sets working directory"));
	opts.add("x {file}", ki18n("Open file in the hexadecimal viewer"));
	opts.add(" +NNN", ki18n("Moves to line number NNN of last file"));
	opts.add("+[file(s)]", ki18n("Documents to open"));
	KCmdLineArgs::addCmdLineOptions(opts);

	/* this is where it crashes if no X server */
	app = new KApplication(x11_display);

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"kde4"));
	mpdm_hset_s(drv, L"startup", MPDM_X(kde4_drv_startup));

	return 1;
}

#include "mpv_kde4.moc"
