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

#include "mp.xpm"

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
MPArea *area;
KMenuBar *menubar;
KStatusBar *statusbar;
QScrollBar *scrollbar;
KTabBar *file_tabs;

static int font_width = -1;
static int font_height = -1;
static int mouse_down = 0;
static int key_down = 0;

/** code **/

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
	wchar_t *wptr = mpdm_string(s);
	return QString::fromWCharArray(wptr);
}


#define MAX_COLORS 1000
QPen inks[MAX_COLORS];
QBrush papers[MAX_COLORS];
bool underlines[MAX_COLORS];
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

	/* loop the colors */
	for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
		int rgbi, rgbp;
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"gui");

		/* store the 'normal' attribute */
		if (wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		rgbi = mpdm_ival(mpdm_aget(v, 0));
		rgbp = mpdm_ival(mpdm_aget(v, 1));

		/* flags */
		v = mpdm_hget_s(d, L"flags");

		if (mpdm_seek_s(v, L"reverse", 1) != -1) {
			int t = rgbi;
			rgbi = rgbp;
			rgbp = t;
		}

		underlines[n] = mpdm_seek_s(v, L"underline", 1) != -1 ? true : false;

		inks[n] = QPen(QColor::fromRgbF(
			(float) ((rgbi & 0x00ff0000) >> 16)	/ 256.0,
			(float) ((rgbi & 0x0000ff00) >> 8)	/ 256.0,
			(float) ((rgbi & 0x000000ff))		/ 256.0,
			1));

		papers[n] = QBrush(QColor::fromRgbF(
			(float) ((rgbp & 0x00ff0000) >> 16)	/ 256.0,
			(float) ((rgbp & 0x0000ff00) >> 8)	/ 256.0,
			(float) ((rgbp & 0x000000ff))		/ 256.0,
			1));
	}
}


static QFont build_font(int rebuild)
/* (re)builds the font */
{
	static QFont font;

	if (rebuild) {
		mpdm_t c;
		char * font_face = (char *)"Mono";
		int font_size = 12;

		if ((c = mpdm_hget_s(mp, L"config")) != NULL) {
			mpdm_t v;

			if ((v = mpdm_hget_s(c, L"font_size")) != NULL)
				font_size = mpdm_ival(v);
			else
				mpdm_hset_s(c, L"font_size", MPDM_I(font_size));

			if ((v = mpdm_hget_s(c, L"font_face")) != NULL) {
				v = MPDM_2MBS((wchar_t *)v->data);
				font_face = (char *)v->data;
			}
			else
				mpdm_hset_s(c, L"font_face", MPDM_MBS(font_face));
		}

		font = QFont(QString(font_face), font_size);
	}

	return font;
}


static void build_menu(void)
/* builds the menu */
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

		/* get the label */
		mi = mpdm_aget(m, n);
		v = mpdm_aget(mi, 0);

		KMenu *menu = new KMenu(str_to_qstring(mpdm_gettext(v)));

		/* get the items */
		v = mpdm_aget(mi, 1);

		for (i = 0; i < mpdm_size(v); i++) {
			wchar_t *wptr;
			mpdm_t w = mpdm_aget(v, i);

			wptr = mpdm_string(w);

			if (*wptr == L'-')
				menu->addSeparator();
			else
				menu->addAction(str_to_qstring(
					mp_menu_label(w)));
		}

		menubar->addMenu(menu);
	}

	menubar->show();
}


static int ignore_scrollbar_signal = 0;

static void draw_scrollbar(void)
{
	static int ol = -1;
	static int ovy = -1;
	static int oty = -1;
	mpdm_t txt = mpdm_hget_s(mp_active(), L"txt");
	mpdm_t window = mpdm_hget_s(mp, L"window");
	int vy = mpdm_ival(mpdm_hget_s(txt, L"vy"));
	int ty = mpdm_ival(mpdm_hget_s(window, L"ty"));
	int l = mpdm_size(mpdm_hget_s(txt, L"lines")) - ty;

	if (ol != l || ovy != vy || oty != ty) {

		ignore_scrollbar_signal = 1;

		scrollbar->setMinimum(0);
		scrollbar->setMaximum(ol = l);
		scrollbar->setValue(ovy = vy);
		scrollbar->setPageStep(oty = ty);

		ignore_scrollbar_signal = 0;
	}
}


static void draw_status(void)
{
	statusbar->changeItem(str_to_qstring(mp_build_status_line()), 0);
}

static void draw_filetabs(void)
{
	static mpdm_t last = NULL;
	mpdm_t names;
	int n, i;

	names = mp_get_doc_names();

	/* get mp.active_i now, because it can be changed
	   from the signal handler */
	i = mpdm_ival(mpdm_hget_s(mp, L"active_i"));

	/* is the list different from the previous one? */
	if (mpdm_cmp(names, last) != 0) {

		while (file_tabs->count())
			file_tabs->removeTab(0);

		/* create the new ones */
		for (n = 0; n < mpdm_size(names); n++)
			file_tabs->addTab(str_to_qstring(mpdm_aget(names, n)));

		/* store for the next time */
		mpdm_unref(last);
		last = mpdm_ref(names);
	}

	/* set the active one */
	file_tabs->setCurrentIndex(i);
}


/** MPArea methods **/

MPArea::MPArea(QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_InputMethodEnabled, true);
}


bool MPArea::event(QEvent *event)
{
	/* special tab processing */
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *ke = (QKeyEvent *)event;

		if (ke->key() == Qt::Key_Tab) {
			mp_process_event(MPDM_LS(L"tab"));
			area->update();
			return true;
		}
	}

	/* keep normal processing */
	return QWidget::event(event);
}

void MPArea::paintEvent(QPaintEvent *) 
{ 
	mpdm_t w;
	int n, m, y;
	QFont font;
	bool underline = false;

	QPainter painter(this);

	font = build_font(0);
	font.setUnderline(false);
	painter.setFont(font);

	font_width = painter.fontMetrics().width("M");
	font_height = painter.fontMetrics().height();

	/* calculate window size */
	w = mpdm_hget_s(mp, L"window");
	mpdm_hset_s(w, L"tx", MPDM_I(this->width() / font_width));
	mpdm_hset_s(w, L"ty", MPDM_I(this->height() / font_height));

	w = mp_draw(mp_active(), 0);
	y = painter.fontMetrics().ascent() + 1;

	painter.setBackgroundMode(Qt::OpaqueMode);

	painter.setBrush(papers[normal_attr]);
	painter.drawRect(0, 0, this->width(), this->height());

	for (n = 0; n < mpdm_size(w); n++) {
		mpdm_t l = mpdm_aget(w, n);
		int x = 0;

		if (l == NULL)
			continue;

		for (m = 0; m < mpdm_size(l); m++) {
			int attr;
			mpdm_t s;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			painter.setPen(inks[attr]);
			painter.setBackground(papers[attr]);

			QString qs = str_to_qstring(s);

			if (underline != underlines[attr]) {
				underline = underlines[attr];
				font.setUnderline(underline);
				painter.setFont(font);
			}

			painter.drawText(x, y, qs);

			x += painter.fontMetrics().width(qs);
		}

		y += font_height;
	}

	draw_filetabs();
	draw_scrollbar();
	draw_status();

	area->setFocus(Qt::OtherFocusReason);
}


void MPArea::inputMethodEvent(QInputMethodEvent *event)
{
	QString s = event->commitString();

	mp_process_event(qstring_to_str(s));
	area->update();
}


void MPArea::keyReleaseEvent(QKeyEvent *event)
{
	if (!event->isAutoRepeat()) {
		key_down = 0;

		if (mp_keypress_throttle(0))
			area->update();
	}
}


void MPArea::keyPressEvent(QKeyEvent *event)
{
	mpdm_t k = NULL;
	wchar_t *ptr = NULL;

	key_down = 1;

	/* set mp.shift_pressed */
	if (event->modifiers() & Qt::ShiftModifier)
		mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

	if (event->modifiers() & Qt::ControlModifier) {
		switch (event->key()) {
		case Qt::Key_Up:		ptr = (wchar_t *) L"ctrl-cursor-up"; break;
		case Qt::Key_Down:		ptr = (wchar_t *) L"ctrl-cursor-down"; break;
		case Qt::Key_Left:		ptr = (wchar_t *) L"ctrl-cursor-left"; break;
		case Qt::Key_Right:		ptr = (wchar_t *) L"ctrl-cursor-right"; break;
		case Qt::Key_PageUp:		ptr = (wchar_t *) L"ctrl-page-up"; break;
		case Qt::Key_PageDown:		ptr = (wchar_t *) L"ctrl-page-down"; break;
		case Qt::Key_Home:		ptr = (wchar_t *) L"ctrl-home"; break;
		case Qt::Key_End:		ptr = (wchar_t *) L"ctrl-end"; break;
		case Qt::Key_Space:		ptr = (wchar_t *) L"ctrl-space"; break;
		case Qt::Key_F1:		ptr = (wchar_t *) L"ctrl-f1"; break;
		case Qt::Key_F2:		ptr = (wchar_t *) L"ctrl-f2"; break;
		case Qt::Key_F3:		ptr = (wchar_t *) L"ctrl-f3"; break;
		case Qt::Key_F4:		ptr = (wchar_t *) L"ctrl-f4"; break;
		case Qt::Key_F5:		ptr = (wchar_t *) L"ctrl-f5"; break;
		case Qt::Key_F6:		ptr = (wchar_t *) L"ctrl-f6"; break;
		case Qt::Key_F7:		ptr = (wchar_t *) L"ctrl-f7"; break;
		case Qt::Key_F8:		ptr = (wchar_t *) L"ctrl-f8"; break;
		case Qt::Key_F9:		ptr = (wchar_t *) L"ctrl-f9"; break;
		case Qt::Key_F10:		ptr = (wchar_t *) L"ctrl-f10"; break;
		case Qt::Key_F11:		ptr = (wchar_t *) L"ctrl-f11"; break;
		case Qt::Key_F12:		ptr = (wchar_t *) L"ctrl-f12"; break;
		case 'A':			ptr = (wchar_t *) L"ctrl-a"; break;
		case 'B':			ptr = (wchar_t *) L"ctrl-b"; break;
		case 'C':			ptr = (wchar_t *) L"ctrl-c"; break;
		case 'D':			ptr = (wchar_t *) L"ctrl-d"; break;
		case 'E':			ptr = (wchar_t *) L"ctrl-e"; break;
		case 'F':			ptr = (wchar_t *) L"ctrl-f"; break;
		case 'G':			ptr = (wchar_t *) L"ctrl-g"; break;
		case 'H':			ptr = (wchar_t *) L"ctrl-h"; break;
		case 'I':			ptr = (wchar_t *) L"ctrl-i"; break;
		case 'J':			ptr = (wchar_t *) L"ctrl-j"; break;
		case 'K':			ptr = (wchar_t *) L"ctrl-k"; break;
		case 'L':			ptr = (wchar_t *) L"ctrl-l"; break;
		case 'M':			ptr = (wchar_t *) L"ctrl-m"; break;
		case 'N':			ptr = (wchar_t *) L"ctrl-n"; break;
		case 'O':			ptr = (wchar_t *) L"ctrl-o"; break;
		case 'P':			ptr = (wchar_t *) L"ctrl-p"; break;
		case 'Q':			ptr = (wchar_t *) L"ctrl-q"; break;
		case 'R':			ptr = (wchar_t *) L"ctrl-r"; break;
		case 'S':			ptr = (wchar_t *) L"ctrl-s"; break;
		case 'T':			ptr = (wchar_t *) L"ctrl-t"; break;
		case 'U':			ptr = (wchar_t *) L"ctrl-u"; break;
		case 'V':			ptr = (wchar_t *) L"ctrl-v"; break;
		case 'W':			ptr = (wchar_t *) L"ctrl-w"; break;
		case 'X':			ptr = (wchar_t *) L"ctrl-x"; break;
		case 'Y':			ptr = (wchar_t *) L"ctrl-y"; break;
		case 'Z':			ptr = (wchar_t *) L"ctrl-z"; break;
		case Qt::Key_Return:
		case Qt::Key_Enter:		ptr = (wchar_t *) L"ctrl-enter"; break;

		default:
			break;
		}
	}
	else {
		switch (event->key()) {
		case Qt::Key_Up:		ptr = (wchar_t *) L"cursor-up"; break;
		case Qt::Key_Down:		ptr = (wchar_t *) L"cursor-down"; break;
		case Qt::Key_Left:		ptr = (wchar_t *) L"cursor-left"; break;
		case Qt::Key_Right:		ptr = (wchar_t *) L"cursor-right"; break;
		case Qt::Key_PageUp:		ptr = (wchar_t *) L"page-up"; break;
		case Qt::Key_PageDown:		ptr = (wchar_t *) L"page-down"; break;
		case Qt::Key_Home:		ptr = (wchar_t *) L"home"; break;
		case Qt::Key_End:		ptr = (wchar_t *) L"end"; break;
		case Qt::Key_Space:		ptr = (wchar_t *) L"space"; break;
		case Qt::Key_F1:		ptr = (wchar_t *) L"f1"; break;
		case Qt::Key_F2:		ptr = (wchar_t *) L"f2"; break;
		case Qt::Key_F3:		ptr = (wchar_t *) L"f3"; break;
		case Qt::Key_F4:		ptr = (wchar_t *) L"f4"; break;
		case Qt::Key_F5:		ptr = (wchar_t *) L"f5"; break;
		case Qt::Key_F6:		ptr = (wchar_t *) L"f6"; break;
		case Qt::Key_F7:		ptr = (wchar_t *) L"f7"; break;
		case Qt::Key_F8:		ptr = (wchar_t *) L"f8"; break;
		case Qt::Key_F9:		ptr = (wchar_t *) L"f9"; break;
		case Qt::Key_F10:		ptr = (wchar_t *) L"f10"; break;
		case Qt::Key_F11:		ptr = (wchar_t *) L"f11"; break;
		case Qt::Key_F12:		ptr = (wchar_t *) L"f12"; break;
		case Qt::Key_Insert:		ptr = (wchar_t *) L"insert"; break;
		case Qt::Key_Backspace:		ptr = (wchar_t *) L"backspace"; break;
		case Qt::Key_Delete:		ptr = (wchar_t *) L"delete"; break;
		case Qt::Key_Return:
		case Qt::Key_Enter:		ptr = (wchar_t *) L"enter"; break;
		case Qt::Key_Escape:		ptr = (wchar_t *) L"escape"; break;

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


void MPArea::mousePressEvent(QMouseEvent *event)
{
	wchar_t *ptr = NULL;

	mouse_down = 1;

	QPoint pos = event->pos();

	mpdm_hset_s(mp, L"mouse_x", MPDM_I(pos.x() / font_width));
	mpdm_hset_s(mp, L"mouse_y", MPDM_I(pos.y() / font_height));

	switch (event->button()) {
	case Qt::LeftButton: ptr = (wchar_t *)L"mouse-left-button"; break;
	case Qt::MidButton: ptr = (wchar_t *)L"mouse-middle-button"; break;
	case Qt::RightButton: ptr = (wchar_t *)L"mouse-right-button"; break;
	default:
		break;
	}

	if (ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	area->update();
}


void MPArea::mouseReleaseEvent(QMouseEvent *event)
{
	mouse_down = 0;
}


void MPArea::mouseMoveEvent(QMouseEvent *event)
{
	static int ox = 0;
	static int oy = 0;

	if (mouse_down) {
		int x, y;

		QPoint pos = event->pos();

		/* mouse dragging */
		x = pos.x() / font_width;
		y = pos.y() / font_height;

		if (ox != x && oy != y) {
			mpdm_hset_s(mp, L"mouse_to_x", MPDM_I(x));
			mpdm_hset_s(mp, L"mouse_to_y", MPDM_I(y));

			mp_process_event(MPDM_LS(L"mouse-drag"));

			area->update();
		}
	}
}


void MPArea::wheelEvent(QWheelEvent *event)
{
	if (event->delta() > 0)
		mp_process_event(MPDM_S(L"mouse-wheel-up"));
	else
		mp_process_event(MPDM_S(L"mouse-wheel-down"));

	area->update();
}


/** MPArea slots **/

void MPArea::from_scrollbar(int value)
{
	if (!ignore_scrollbar_signal) {
		mpdm_t v = mp_active();

		mp_set_y(v, value);

		/* set the vy to the same value */
		v = mpdm_hget_s(v, L"txt");
		mpdm_hset_s(v, L"vy", MPDM_I(value));

		area->update();
	}
}


void MPArea::from_filetabs(int value)
{
	if (value >= 0) {
		/* sets the active one */
		mpdm_hset_s(mp, L"active_i", MPDM_I(value));
		area->update();
	}
}


void MPArea::from_menu(QAction *action)
{
	mpdm_t label = qstring_to_str(action->text());
	label = mpdm_sregex(MPDM_LS(L"/&/"), label, NULL, 0);

	mpdm_t a = mpdm_hget_s(mp, L"actions_by_menu_label");

	mp_process_action(mpdm_hget(a, label));
	area->update();
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
		exit(0);
	}

	return r;
}


/** driver functions **/

static mpdm_t kde4_drv_update_ui(mpdm_t a)
{
	build_font(1);
	build_colors();
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
	int onoff = mpdm_ival(mpdm_aget(a, 0));

	window->setCursor(onoff ? Qt::WaitCursor : Qt::ArrowCursor);

	return NULL;
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


static mpdm_t kde4_drv_clip_to_sys(mpdm_t a)
{
	mpdm_t v;

	QClipboard *qc = QApplication::clipboard();

	/* gets the clipboard and joins */
	v = mpdm_hget_s(mp, L"clipboard");

	if (mpdm_size(v) != 0) {
		v = mpdm_join(MPDM_LS(L"\n"), v);
		qc->setText(str_to_qstring(v), QClipboard::Selection);
	}

	return NULL;
}


static mpdm_t kde4_drv_sys_to_clip(mpdm_t a)
{
	QClipboard *qc = QApplication::clipboard();
	QString qs = qc->text(QClipboard::Selection);

	/* split and set as the clipboard */
	mpdm_hset_s(mp, L"clipboard", mpdm_split(MPDM_LS(L"\n"), 
		qstring_to_str(qs)));
	mpdm_hset_s(mp, L"clipboard_vertical", MPDM_I(0));

	return NULL;
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
