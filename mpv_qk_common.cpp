/*

    Minimum Profit - Programmer Text Editor

    Code common to Qt4 and KDE4 drivers.

    Copyright (C) 2009/2010 Angel Ortega <angel@triptico.com>

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

MPArea *area;
QScrollBar *scrollbar;
static int font_width = -1;
static int font_height = -1;
static int mouse_down = 0;
static int key_down = 0;

mpdm_t timer_func = NULL;

/** code **/

static void draw_status(void);

static mpdm_t qstring_to_str(QString s)
/* converts a QString to an MPDM string */
{
    mpdm_t r = NULL;

    if (s != NULL) {
        int t = s.size();
        wchar_t *wptr = (wchar_t *) malloc((t + 1) * sizeof(wchar_t));

        r = MPDM_ENS(wptr, t);

        s.toWCharArray(wptr);
        wptr[t] = L'\0';
    }

    return r;
}


QString str_to_qstring(mpdm_t s)
/* converts an MPDM string to a QString */
{
    QString r;
    wchar_t *wptr;

    mpdm_ref(s);
    wptr = mpdm_string(s);
    r = QString::fromWCharArray(wptr);
    mpdm_unref(s);

    return r;
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
    l = mpdm_ref(mpdm_keys(colors));
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

        underlines[n] =
            mpdm_seek_s(v, L"underline", 1) != -1 ? true : false;

        inks[n] = QPen(QColor::fromRgbF((float) ((rgbi & 0x00ff0000) >> 16)
                                        / 256.0,
                                        (float) ((rgbi & 0x0000ff00) >> 8)
                                        / 256.0,
                                        (float) ((rgbi & 0x000000ff)) /
                                        256.0, 1));

        papers[n] = QBrush(QColor::fromRgbF((float)
                                            ((rgbp & 0x00ff0000) >> 16) /
                                            256.0,
                                            (float) ((rgbp & 0x0000ff00) >>
                                                     8) / 256.0,
                                            (float) ((rgbp & 0x000000ff)) /
                                            256.0, 1));
    }

    mpdm_unref(l);
}


static QFont build_font(int rebuild)
/* (re)builds the font */
{
    static QFont font;

    if (rebuild) {
        mpdm_t c;
        char *font_face = (char *) "Mono";
        int font_size = 12;
        mpdm_t w = NULL;

        if ((c = mpdm_hget_s(mp, L"config")) != NULL) {
            mpdm_t v;

            if ((v = mpdm_hget_s(c, L"font_size")) != NULL)
                font_size = mpdm_ival(v);
            else
                mpdm_hset_s(c, L"font_size", MPDM_I(font_size));

            if ((v = mpdm_hget_s(c, L"font_face")) != NULL) {
                w = mpdm_ref(MPDM_2MBS((wchar_t *) v->data));
                font_face = (char *) w->data;
            }
            else
                mpdm_hset_s(c, L"font_face", MPDM_MBS(font_face));
        }

        font = QFont(QString(font_face), font_size);
        mpdm_unref(w);
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

        MENU_CLASS *menu = new MENU_CLASS(str_to_qstring(mpdm_gettext(v)));

        /* get the items */
        v = mpdm_aget(mi, 1);

        for (i = 0; i < mpdm_size(v); i++) {
            wchar_t *wptr;
            mpdm_t w = mpdm_aget(v, i);

            wptr = mpdm_string(w);

            if (*wptr == L'-')
                menu->addSeparator();
            else
                menu->addAction(str_to_qstring(mp_menu_label(w)));
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


static void draw_filetabs(void)
{
    static mpdm_t last = NULL;
    mpdm_t names;
    int n, i;

    names = mpdm_ref(mp_get_doc_names());

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

    mpdm_unref(names);

    /* set the active one */
    file_tabs->setCurrentIndex(i);
}


/** MPArea methods **/

MPArea::MPArea(QWidget * parent):QWidget(parent)
{
    setAttribute(Qt::WA_InputMethodEnabled, true);

    setAcceptDrops(true);

    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(from_timer()));
}


bool MPArea::event(QEvent * event)
{
    /* special tab processing */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = (QKeyEvent *) event;

        if (ke->key() == Qt::Key_Tab) {
            mp_process_event(MPDM_LS(L"tab"));
            area->update();
            return true;
        }
        else
        if (ke->key() == Qt::Key_Backtab) {
            mp_process_event(MPDM_LS(L"shift-tab"));
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

    mpdm_ref(w);

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

    mpdm_unref(w);

    draw_filetabs();
    draw_scrollbar();
    draw_status();

    area->setFocus(Qt::OtherFocusReason);
}


void MPArea::inputMethodEvent(QInputMethodEvent * event)
{
    QString s = event->commitString();

    mp_process_event(qstring_to_str(s));
    area->update();
}


void MPArea::keyReleaseEvent(QKeyEvent * event)
{
    if (!event->isAutoRepeat()) {
        key_down = 0;

        if (mp_keypress_throttle(0))
            area->update();
    }
}


void MPArea::keyPressEvent(QKeyEvent * event)
{
    mpdm_t k = NULL;
    wchar_t *ptr = NULL;

    key_down = 1;

    /* set mp.shift_pressed */
    if (event->modifiers() & Qt::ShiftModifier)
        mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

    if (event->modifiers() & Qt::ControlModifier) {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"ctrl-cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"ctrl-cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"ctrl-cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"ctrl-cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"ctrl-page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"ctrl-page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"ctrl-home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"ctrl-end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"ctrl-space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"ctrl-f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"ctrl-f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"ctrl-f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"ctrl-f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"ctrl-f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"ctrl-f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"ctrl-f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"ctrl-f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"ctrl-f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"ctrl-f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"ctrl-f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"ctrl-f12";
            break;
        case 'A':
            ptr = (wchar_t *) L"ctrl-a";
            break;
        case 'B':
            ptr = (wchar_t *) L"ctrl-b";
            break;
        case 'C':
            ptr = (wchar_t *) L"ctrl-c";
            break;
        case 'D':
            ptr = (wchar_t *) L"ctrl-d";
            break;
        case 'E':
            ptr = (wchar_t *) L"ctrl-e";
            break;
        case 'F':
            ptr = (wchar_t *) L"ctrl-f";
            break;
        case 'G':
            ptr = (wchar_t *) L"ctrl-g";
            break;
        case 'H':
            ptr = (wchar_t *) L"ctrl-h";
            break;
        case 'I':
            ptr = (wchar_t *) L"ctrl-i";
            break;
        case 'J':
            ptr = (wchar_t *) L"ctrl-j";
            break;
        case 'K':
            ptr = (wchar_t *) L"ctrl-k";
            break;
        case 'L':
            ptr = (wchar_t *) L"ctrl-l";
            break;
        case 'M':
            ptr = (wchar_t *) L"ctrl-m";
            break;
        case 'N':
            ptr = (wchar_t *) L"ctrl-n";
            break;
        case 'O':
            ptr = (wchar_t *) L"ctrl-o";
            break;
        case 'P':
            ptr = (wchar_t *) L"ctrl-p";
            break;
        case 'Q':
            ptr = (wchar_t *) L"ctrl-q";
            break;
        case 'R':
            ptr = (wchar_t *) L"ctrl-r";
            break;
        case 'S':
            ptr = (wchar_t *) L"ctrl-s";
            break;
        case 'T':
            ptr = (wchar_t *) L"ctrl-t";
            break;
        case 'U':
            ptr = (wchar_t *) L"ctrl-u";
            break;
        case 'V':
            ptr = (wchar_t *) L"ctrl-v";
            break;
        case 'W':
            ptr = (wchar_t *) L"ctrl-w";
            break;
        case 'X':
            ptr = (wchar_t *) L"ctrl-x";
            break;
        case 'Y':
            ptr = (wchar_t *) L"ctrl-y";
            break;
        case 'Z':
            ptr = (wchar_t *) L"ctrl-z";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"ctrl-enter";
            break;

        default:
            break;
        }
    }
    else
    if (event->modifiers() & Qt::AltModifier) {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"alt-cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"alt-cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"alt-cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"alt-cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"alt-page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"alt-page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"alt-home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"alt-end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"alt-space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"alt-f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"alt-f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"alt-f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"alt-f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"alt-f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"alt-f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"alt-f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"alt-f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"alt-f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"alt-f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"alt-f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"alt-f12";
            break;
        case 'A':
            ptr = (wchar_t *) L"alt-a";
            break;
        case 'B':
            ptr = (wchar_t *) L"alt-b";
            break;
        case 'C':
            ptr = (wchar_t *) L"alt-c";
            break;
        case 'D':
            ptr = (wchar_t *) L"alt-d";
            break;
        case 'E':
            ptr = (wchar_t *) L"alt-e";
            break;
        case 'F':
            ptr = (wchar_t *) L"alt-f";
            break;
        case 'G':
            ptr = (wchar_t *) L"alt-g";
            break;
        case 'H':
            ptr = (wchar_t *) L"alt-h";
            break;
        case 'I':
            ptr = (wchar_t *) L"alt-i";
            break;
        case 'J':
            ptr = (wchar_t *) L"alt-j";
            break;
        case 'K':
            ptr = (wchar_t *) L"alt-k";
            break;
        case 'L':
            ptr = (wchar_t *) L"alt-l";
            break;
        case 'M':
            ptr = (wchar_t *) L"alt-m";
            break;
        case 'N':
            ptr = (wchar_t *) L"alt-n";
            break;
        case 'O':
            ptr = (wchar_t *) L"alt-o";
            break;
        case 'P':
            ptr = (wchar_t *) L"alt-p";
            break;
        case 'Q':
            ptr = (wchar_t *) L"alt-q";
            break;
        case 'R':
            ptr = (wchar_t *) L"alt-r";
            break;
        case 'S':
            ptr = (wchar_t *) L"alt-s";
            break;
        case 'T':
            ptr = (wchar_t *) L"alt-t";
            break;
        case 'U':
            ptr = (wchar_t *) L"alt-u";
            break;
        case 'V':
            ptr = (wchar_t *) L"alt-v";
            break;
        case 'W':
            ptr = (wchar_t *) L"alt-w";
            break;
        case 'X':
            ptr = (wchar_t *) L"alt-x";
            break;
        case 'Y':
            ptr = (wchar_t *) L"alt-y";
            break;
        case 'Z':
            ptr = (wchar_t *) L"alt-z";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"alt-enter";
            break;

        default:
            break;
        }
    }
    else {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"f12";
            break;
        case Qt::Key_Insert:
            ptr = (wchar_t *) L"insert";
            break;
        case Qt::Key_Backspace:
            ptr = (wchar_t *) L"backspace";
            break;
        case Qt::Key_Delete:
            ptr = (wchar_t *) L"delete";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"enter";
            break;
        case Qt::Key_Escape:
            ptr = (wchar_t *) L"escape";
            break;

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


void MPArea::mousePressEvent(QMouseEvent * event)
{
    wchar_t *ptr = NULL;

    mouse_down = 1;

    QPoint pos = event->pos();

    mpdm_hset_s(mp, L"mouse_x", MPDM_I(pos.x() / font_width));
    mpdm_hset_s(mp, L"mouse_y", MPDM_I(pos.y() / font_height));

    switch (event->button()) {
    case Qt::LeftButton:
        ptr = (wchar_t *) L"mouse-left-button";
        break;
    case Qt::MidButton:
        ptr = (wchar_t *) L"mouse-middle-button";
        break;
    case Qt::RightButton:
        ptr = (wchar_t *) L"mouse-right-button";
        break;
    default:
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    area->update();
}


void MPArea::mouseReleaseEvent(QMouseEvent * event)
{
    mouse_down = 0;
}


void MPArea::mouseMoveEvent(QMouseEvent * event)
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


void MPArea::wheelEvent(QWheelEvent * event)
{
    if (event->delta() > 0)
        mp_process_event(MPDM_S(L"mouse-wheel-up"));
    else
        mp_process_event(MPDM_S(L"mouse-wheel-down"));

    area->update();
}


void MPArea::dragEnterEvent(QDragEnterEvent * event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}


void MPArea::dropEvent(QDropEvent * event)
{
    int n;
    mpdm_t v = qstring_to_str(event->mimeData()->text());
    mpdm_t l = MPDM_A(0);

    mpdm_ref(l);

    /* split the list of files */
    v = mpdm_split_s(v, L"\n");

    for (n = 0; n < mpdm_size(v); n++) {
        wchar_t *ptr;
        mpdm_t w = mpdm_aget(v, n);

        /* strip file:///, if found */
        ptr = mpdm_string(w);

        if (wcsncmp(ptr, L"file://", 7) == 0)
            ptr += 7;

        if (*ptr != L'\0')
            mpdm_push(l, MPDM_S(ptr));
    }

    mpdm_hset_s(mp, L"dropped_files", l);

    event->acceptProposedAction();
    mp_process_event(MPDM_LS(L"dropped-files"));

    mpdm_unref(l);

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


void MPArea::from_menu(QAction * action)
{
    mpdm_t label = qstring_to_str(action->text());
    label = mpdm_sregex(label, MPDM_LS(L"/&/"), NULL, 0);

    mpdm_t a = mpdm_hget_s(mp, L"actions_by_menu_label");

    mp_process_action(mpdm_hget(a, label));
    area->update();
}


void MPArea::from_timer(void)
{
    mpdm_void(mpdm_exec(timer_func, NULL, NULL));
    area->update();
}


/** driver functions **/

static mpdm_t qt4_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    build_font(1);
    build_colors();
    build_menu();

    return NULL;
}


static mpdm_t qt4_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    int onoff = mpdm_ival(mpdm_aget(a, 0));

    window->setCursor(onoff ? Qt::WaitCursor : Qt::ArrowCursor);

    return NULL;
}


static mpdm_t qt4_drv_main_loop(mpdm_t a, mpdm_t ctxt)
{
    app->exec();

    return NULL;
}


static mpdm_t qt4_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    if ((v = mpdm_hget_s(mp, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static mpdm_t qt4_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    QClipboard *qc = QApplication::clipboard();

    /* gets the clipboard and joins */
    v = mpdm_hget_s(mp, L"clipboard");

    if (mpdm_size(v) != 0) {
        v = mpdm_join_s(v, L"\n");
        qc->setText(str_to_qstring(v), QClipboard::Selection);
    }

    return NULL;
}


static mpdm_t qt4_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
{
    QClipboard *qc = QApplication::clipboard();
    QString qs = qc->text(QClipboard::Selection);

    /* split and set as the clipboard */
    mpdm_hset_s(mp, L"clipboard", mpdm_split_s(qstring_to_str(qs), L"\n"));
    mpdm_hset_s(mp, L"clipboard_vertical", MPDM_I(0));

    return NULL;
}


static mpdm_t qt4_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    int msecs = mpdm_ival(mpdm_aget(a, 0));
    mpdm_t func = mpdm_aget(a, 1);

    mpdm_ref(func);
    mpdm_unref(timer_func);
    timer_func = func;

    if (timer_func == NULL)
        area->timer->stop();
    else
        area->timer->start(msecs);

    return NULL;
}
