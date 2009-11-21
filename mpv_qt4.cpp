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
//QStatusBar *statusbar;
QLabel *statusbar;
QTabBar *file_tabs;

#define MENU_CLASS QMenu

#include "mpv_qk_common.cpp"

static void draw_status(void)
{
	statusbar->setText(str_to_qstring(mp_build_status_line()));
}


/** MPWindow methods **/

MPWindow::MPWindow(QWidget *parent) : QMainWindow(parent)
{
	setWindowTitle("mp-5");

	menubar = this->menuBar();
	build_menu();

	statusbar = new QLabel();
	this->statusBar()->addWidget(statusbar);

	/* the full container */
	QVBoxLayout *vb = new QVBoxLayout();

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
		qt4_drv_shutdown(NULL);
		exit(0);
	}

	return r;
}


/** driver functions **/

static mpdm_t qt4_drv_alert(mpdm_t a)
{
	/* 1# arg: prompt */
	QMessageBox::information(0, "mp " VERSION, str_to_qstring(mpdm_aget(a, 0)));

	return NULL;
}


static mpdm_t qt4_drv_confirm(mpdm_t a)
{
	int r;

	/* 1# arg: prompt */
	r = QMessageBox::question(0, "mp" VERSION,
		str_to_qstring(mpdm_aget(a, 0)),
		QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
	);

	switch (r) {
	case QMessageBox::Yes:
		r = 1;
		break;

	case QMessageBox::No:
		r = 2;
		break;

	case QMessageBox::Cancel:
		r = 0;
		break;
	}

	return MPDM_I(r);
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


class MPForm : public QDialog
{
public:
	QDialogButtonBox *button_box;

	MPForm(QWidget *parent = 0) : QDialog(parent)
	{
		button_box = new QDialogButtonBox(QDialogButtonBox::Ok |
							QDialogButtonBox::Cancel);

		connect(button_box, SIGNAL(accepted()), this, SLOT(accept()));
		connect(button_box, SIGNAL(rejected()), this, SLOT(reject()));
	}
};


static mpdm_t qt4_drv_form(mpdm_t a)
{
	int n;
	mpdm_t widget_list;
	QWidget *qlist[100];
	mpdm_t r;

	MPForm *dialog = new MPForm(window);
	dialog->setWindowTitle("mp " VERSION);

	dialog->setModal(true);

	widget_list = mpdm_aget(a, 0);

	QWidget *form = new QWidget();
	QFormLayout *fl = new QFormLayout();

	for (n = 0; n < mpdm_size(widget_list); n++) {
		mpdm_t w = mpdm_aget(widget_list, n);
		wchar_t *type;
		mpdm_t t;
		QLabel *ql = new QLabel("");

		type = mpdm_string(mpdm_hget_s(w, L"type"));

		if ((t = mpdm_hget_s(w, L"label")) != NULL) {
			ql->setText(str_to_qstring(mpdm_gettext(t)));
		}

		t = mpdm_hget_s(w, L"value");

		if (wcscmp(type, L"text") == 0) {
			mpdm_t h;
			QComboBox *qc = new QComboBox();

			qc->setEditable(true);
			qc->setMinimumContentsLength(30);
			qc->setMaxVisibleItems(8);

			if (t != NULL)
				qc->setEditText(str_to_qstring(t));

			qlist[n] = qc;

			if ((h = mpdm_hget_s(w, L"history")) != NULL) {
				int i;

				/* has history; fill it */
				h = mp_get_history(h);

				qc->addItem("");

				for (i = mpdm_size(h) - 1; i >= 0; i--)
					qc->addItem(str_to_qstring(mpdm_aget(h, i)));
			}

			fl->addRow(ql, qc);
		}
		else
		if (wcscmp(type, L"password") == 0) {
			QLineEdit *qe = new QLineEdit();

			qe->setEchoMode(QLineEdit::Password);

			qlist[n] = qe;

			fl->addRow(ql, qe);
		}
		else
		if (wcscmp(type, L"checkbox") == 0) {
			QCheckBox *qc = new QCheckBox();

			if (mpdm_ival(t))
				qc->setCheckState(Qt::Checked);

			qlist[n] = qc;

			fl->addRow(ql, qc);
		}
		else
		if (wcscmp(type, L"list") == 0) {
			int i;
			QListWidget *qlw = new QListWidget();
			qlw->setMinimumWidth(480);

			/* use a monospaced font */
			qlw->setFont(QFont(QString("Mono")));

			mpdm_t l = mpdm_hget_s(w, L"list");

			for (i = 0; i < mpdm_size(l); i++)
				qlw->addItem(str_to_qstring(mpdm_aget(l, i)));

			qlw->setCurrentRow(mpdm_ival(t));

			qlist[n] = qlw;

			fl->addRow(ql, qlw);
		}

		if (n == 0)
			qlist[n]->setFocus(Qt::OtherFocusReason);
	}

	form->setLayout(fl);

	QVBoxLayout *ml = new QVBoxLayout();
	ml->addWidget(form);
	ml->addWidget(dialog->button_box);

	dialog->setLayout(ml);

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
