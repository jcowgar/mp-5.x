/*

    Minimum Profit - Programmer Text Editor

    Win32 driver.

    Copyright (C) 1991-2010 Angel Ortega <angel@triptico.com>

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/** data **/

/* the instance */
HINSTANCE hinst;

/* the windows */
HWND hwnd = NULL;
HWND hwtabs = NULL;
HWND hwstatus = NULL;

/* font handlers and metrics */
HFONT font_normal = NULL;
HFONT font_underline = NULL;
int font_width = 0;
int font_height = 0;

/* height of the tab set */
int tab_height = 28;

/* height of the status bar */
int status_height = 16;

int is_wm_keydown = 0;

/* colors */
static COLORREF *inks = NULL;
static COLORREF *papers = NULL;
int *underlines = NULL;
HBRUSH bgbrush;

/* code for the 'normal' attribute */
static int normal_attr = 0;

/* the menu */
static HMENU menu = NULL;

/* mp.drv.form() controls */

static mpdm_t form_args = NULL;
static mpdm_t form_values = NULL;

/* mouse down flag */
static int mouse_down = 0;

/* timer function */
static mpdm_t timer_func = NULL;

/** code **/

static void update_window_size(void)
/* updates the viewport size in characters */
{
    RECT rect;
    int tx, ty;
    mpdm_t v;

    /* no font information? go */
    if (font_width == 0 || font_height == 0)
        return;

    GetClientRect(hwnd, &rect);

    /* calculate the size in chars */
    tx = ((rect.right - rect.left) / font_width) + 1;
    ty = (rect.bottom - rect.top - tab_height) / font_height;

    /* store the 'window' size */
    v = mpdm_hget_s(mp, L"window");
    mpdm_hset_s(v, L"tx", MPDM_I(tx));
    mpdm_hset_s(v, L"ty", MPDM_I(ty));
}


static void build_fonts(HDC hdc)
/* build the fonts */
{
    TEXTMETRIC tm;
    int n;
    int font_size = 10;
    char *font_face = "Lucida Console";
    mpdm_t v = NULL;
    mpdm_t c;

    if (font_normal != NULL) {
        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        DeleteObject(font_normal);
    }

    /* get current configuration */
    if ((c = mpdm_hget_s(mp, L"config")) != NULL) {
        if ((v = mpdm_hget_s(c, L"font_size")) != NULL)
            font_size = mpdm_ival(v);
        else
            mpdm_hset_s(c, L"font_size", MPDM_I(font_size));

        if ((v = mpdm_hget_s(c, L"font_face")) != NULL) {
            v = mpdm_ref(MPDM_2MBS(v->data));
            font_face = (char *) v->data;
        }
        else
            mpdm_hset_s(c, L"font_face", MPDM_MBS(font_face));
    }

    /* create fonts */
    n = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);

    font_normal = CreateFont(n, 0, 0, 0, 0, 0, 0,
                             0, 0, 0, 0, 0, 0, font_face);

    font_underline = CreateFont(n, 0, 0, 0, 0, 0, 1,
                                0, 0, 0, 0, 0, 0, font_face);

    SelectObject(hdc, font_normal);
    GetTextMetrics(hdc, &tm);

    /* store sizes */
    font_height = tm.tmHeight;
    font_width = tm.tmAveCharWidth;

    update_window_size();

    mpdm_unref(v);
}


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

    /* redim the structures */
    inks = realloc(inks, sizeof(COLORREF) * s);
    papers = realloc(papers, sizeof(COLORREF) * s);
    underlines = realloc(underlines, sizeof(int) * s);

    /* loop the colors */
    for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
        mpdm_t d = mpdm_hget(colors, c);
        mpdm_t v = mpdm_hget_s(d, L"gui");
        int m;

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(c), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_hset_s(d, L"attr", MPDM_I(n));

        m = mpdm_ival(mpdm_aget(v, 0));
        inks[n] = ((m & 0x000000ff) << 16) |
            ((m & 0x0000ff00)) | ((m & 0x00ff0000) >> 16);
        m = mpdm_ival(mpdm_aget(v, 1));
        papers[n] = ((m & 0x000000ff) << 16) |
            ((m & 0x0000ff00)) | ((m & 0x00ff0000) >> 16);

        /* flags */
        v = mpdm_hget_s(d, L"flags");

        underlines[n] = mpdm_seek_s(v, L"underline", 1) != -1 ? 1 : 0;

        if (mpdm_seek_s(v, L"reverse", 1) != -1) {
            COLORREF t;

            t = inks[n];
            inks[n] = papers[n];
            papers[n] = t;
        }
    }

    mpdm_unref(l);

    /* create the background brush */
    bgbrush = CreateSolidBrush(papers[normal_attr]);
}


static void build_menu(void)
/* builds the menu */
{
    int n;
    mpdm_t m;
    int win32_menu_id = 1000;

    /* gets the current menu */
    if ((m = mpdm_hget_s(mp, L"menu")) == NULL)
        return;

    if (menu != NULL)
        DestroyMenu(menu);

    menu = CreateMenu();

    for (n = 0; n < mpdm_size(m); n++) {
        char *ptr;
        mpdm_t mi, v, l;
        int i;
        HMENU submenu = CreatePopupMenu();

        /* get the label and the items */
        mi = mpdm_aget(m, n);
        v = mpdm_gettext(mpdm_aget(mi, 0));
        l = mpdm_aget(mi, 1);

        /* create the submenus */
        for (i = 0; i < mpdm_size(l); i++) {
            /* get the action */
            mpdm_t v = mpdm_aget(l, i);

            /* if the action is a separator... */
            if (*((wchar_t *) v->data) == L'-')
                AppendMenu(submenu, MF_SEPARATOR, 0, NULL);
            else {
                MENUITEMINFO mi;
                mpdm_t d = mpdm_ref(mp_menu_label(v));

                /* set the string */
                ptr = mpdm_wcstombs(mpdm_string(d), NULL);
                AppendMenu(submenu, MF_STRING, win32_menu_id, ptr);
                free(ptr);

                mpdm_unref(d);

                /* store the action inside the menu */
                memset(&mi, '\0', sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_DATA;
                mi.dwItemData = (unsigned long) v;

                SetMenuItemInfo(submenu, win32_menu_id, FALSE, &mi);

                win32_menu_id++;
            }
        }

        /* now store the popup inside the menu */
        ptr = mpdm_wcstombs(mpdm_string(v), NULL);
        AppendMenu(menu, MF_STRING | MF_POPUP, (UINT) submenu, ptr);
        free(ptr);
    }

    SetMenu(hwnd, menu);
}


static void draw_filetabs(void)
/* draws the filetabs */
{
    static mpdm_t last = NULL;
    mpdm_t names;
    int n;

    if (hwtabs == NULL)
        return;

    names = mpdm_ref(mp_get_doc_names());

    /* is the list different from the previous one? */
    if (mpdm_cmp(names, last) != 0) {
        TabCtrl_DeleteAllItems(hwtabs);

        for (n = 0; n < mpdm_size(names); n++) {
            TCITEM ti;
            char *ptr;
            mpdm_t v = mpdm_aget(names, n);

            /* convert to mbs */
            ptr = mpdm_wcstombs(v->data, NULL);

            ti.mask = TCIF_TEXT;
            ti.pszText = ptr;

            /* create it */
            TabCtrl_InsertItem(hwtabs, n, &ti);

            free(ptr);
        }

        /* store for the next time */
        mpdm_unref(last);
        last = mpdm_ref(names);
    }

    mpdm_unref(names);

    /* set the active one */
    TabCtrl_SetCurSel(hwtabs, mpdm_ival(mpdm_hget_s(mp, L"active_i")));
}


static void draw_scrollbar(void)
/* updates the scrollbar */
{
    mpdm_t d;
    mpdm_t v;
    int pos, size, max;
    SCROLLINFO si;

    /* gets the active document */
    if ((d = mp_active()) == NULL)
        return;

    /* get the coordinates */
    v = mpdm_hget_s(d, L"txt");
    pos = mpdm_ival(mpdm_hget_s(v, L"vy"));
    max = mpdm_size(mpdm_hget_s(v, L"lines"));

    v = mpdm_hget_s(mp, L"window");
    size = mpdm_ival(mpdm_hget_s(v, L"ty"));

    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = max;
    si.nPage = size;
    si.nPos = pos;

    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}


void draw_status(void)
/* draws the status line */
{
    mpdm_t t;

    if (hwstatus != NULL && (t = mpdm_ref(mp_build_status_line())) != NULL) {
        mpdm_t v = mpdm_ref(MPDM_2MBS(t->data));

        if (v->data != NULL)
            SetWindowText(hwstatus, v->data);

        mpdm_unref(v);
        mpdm_unref(t);
    }
}


static void win32_draw(HWND hwnd, mpdm_t doc)
/* win32 document draw function */
{
    HDC hdc;
    PAINTSTRUCT ps;
    RECT rect;
    RECT r2;
    mpdm_t d = NULL;
    int n, m;

    /* start painting */
    hdc = BeginPaint(hwnd, &ps);

    /* no font? construct it */
    if (font_normal == NULL) {
        build_fonts(hdc);
        build_colors();
    }

    /* no document? end */
    if ((d = mp_draw(doc, 0)) == NULL) {
        EndPaint(hwnd, &ps);
        return;
    }

    mpdm_ref(d);

    /* select defaults to start painting */
    SelectObject(hdc, font_normal);

    GetClientRect(hwnd, &rect);
    r2 = rect;

    r2.top += tab_height;
    r2.bottom = r2.top + font_height;

    for (n = 0; n < mpdm_size(d); n++) {
        mpdm_t l = mpdm_aget(d, n);

        r2.left = rect.left;

        for (m = 0; m < mpdm_size(l); m++) {
            int attr;
            mpdm_t s;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_aget(l, m++));
            s = mpdm_aget(l, m);

            SetTextColor(hdc, inks[attr]);
            SetBkColor(hdc, papers[attr]);

            SelectObject(hdc, underlines[attr] ?
                         font_underline : font_normal);

            TextOutW(hdc, r2.left, r2.top, s->data, mpdm_size(s));
            r2.left += mpdm_size(s) * font_width;
        }

        /* fills the rest of the line */
        FillRect(hdc, &r2, bgbrush);

        r2.top += font_height;
        r2.bottom += font_height;
    }

    EndPaint(hwnd, &ps);

    mpdm_unref(d);

    draw_filetabs();
    draw_scrollbar();
    draw_status();
}


static void redraw(void)
{
    InvalidateRect(hwnd, NULL, TRUE);
}


static void win32_vkey(int c)
/* win32 virtual key processing */
{
    wchar_t *ptr = NULL;
    static int maxed = 0;

    /* set mp.shift_pressed */
    if (GetKeyState(VK_SHIFT) & 0x8000)
        mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

    if (GetKeyState(VK_CONTROL) & 0x8000) {
        switch (c) {
        case VK_UP:
            ptr = L"ctrl-cursor-up";
            break;
        case VK_DOWN:
            ptr = L"ctrl-cursor-down";
            break;
        case VK_LEFT:
            ptr = L"ctrl-cursor-left";
            break;
        case VK_RIGHT:
            ptr = L"ctrl-cursor-right";
            break;
        case VK_PRIOR:
            ptr = L"ctrl-page-up";
            break;
        case VK_NEXT:
            ptr = L"ctrl-page-down";
            break;
        case VK_HOME:
            ptr = L"ctrl-home";
            break;
        case VK_END:
            ptr = L"ctrl-end";
            break;
        case VK_SPACE:
            ptr = L"ctrl-space";
            break;
        case VK_DIVIDE:
            ptr = L"ctrl-kp-divide";
            break;
        case VK_MULTIPLY:
            ptr = L"ctrl-kp-multiply";
            break;
        case VK_SUBTRACT:
            ptr = L"ctrl-kp-minus";
            break;
        case VK_ADD:
            ptr = L"ctrl-kp-plus";
            break;
        case VK_RETURN:
            ptr = L"ctrl-enter";
            break;
        case VK_BACK:
            ptr = L"ctrl-backspace";
            break;
       	case VK_DELETE:
       	    ptr = L"ctrl-delete";
            break;
        case VK_F1:
            ptr = L"ctrl-f1";
            break;
        case VK_F2:
            ptr = L"ctrl-f2";
            break;
        case VK_F3:
            ptr = L"ctrl-f3";
            break;
        case VK_F4:
            ptr = L"ctrl-f4";
            break;
        case VK_F5:
            ptr = L"ctrl-f5";
            break;
        case VK_F6:
            ptr = L"ctrl-f6";
            break;
        case VK_F7:
            ptr = L"ctrl-f7";
            break;
        case VK_F8:
            ptr = L"ctrl-f8";
            break;
        case VK_F9:
            ptr = L"ctrl-f9";
            break;
        case VK_F10:
            ptr = L"ctrl-f10";
            break;
        case VK_F11:
            ptr = L"ctrl-f11";
            break;
        case VK_F12:
            SendMessage(hwnd, WM_SYSCOMMAND,
                        maxed ? SC_RESTORE : SC_MAXIMIZE, 0);

            maxed ^= 1;

            break;
        }
    }
    else
    if (GetKeyState(VK_LMENU) & 0x8000) {
        switch (c) {
        case VK_UP:
            ptr = L"alt-cursor-up";
            break;
        case VK_DOWN:
            ptr = L"alt-cursor-down";
            break;
        case VK_LEFT:
            ptr = L"alt-cursor-left";
            break;
        case VK_RIGHT:
            ptr = L"alt-cursor-right";
            break;
        case VK_PRIOR:
            ptr = L"alt-page-up";
            break;
        case VK_NEXT:
            ptr = L"alt-page-down";
            break;
        case VK_HOME:
            ptr = L"alt-home";
            break;
        case VK_END:
            ptr = L"alt-end";
            break;
        case VK_SPACE:
            ptr = L"alt-space";
            break;
        case VK_DIVIDE:
            ptr = L"alt-kp-divide";
            break;
        case VK_MULTIPLY:
            ptr = L"alt-kp-multiply";
            break;
        case VK_SUBTRACT:
            ptr = L"alt-kp-minus";
            break;
        case VK_ADD:
            ptr = L"alt-kp-plus";
            break;
        case VK_RETURN:
            ptr = L"alt-enter";
            break;
        case VK_BACK:
            ptr = L"alt-backspace";
            break;
        case VK_DELETE:
            ptr = L"alt-delete";
            break;
        case VK_F1:
            ptr = L"alt-f1";
            break;
        case VK_F2:
            ptr = L"alt-f2";
            break;
        case VK_F3:
            ptr = L"alt-f3";
            break;
        case VK_F4:
            ptr = L"alt-f4";
            break;
        case VK_F5:
            ptr = L"alt-f5";
            break;
        case VK_F6:
            ptr = L"alt-f6";
            break;
        case VK_F7:
            ptr = L"alt-f7";
            break;
        case VK_F8:
            ptr = L"alt-f8";
            break;
        case VK_F9:
            ptr = L"alt-f9";
            break;
        case VK_F10:
            ptr = L"alt-f10";
            break;
        case VK_F11:
            ptr = L"alt-f11";
            break;
        case VK_F12:
            ptr = L"alt-f12";
            break;
        }
    }
    else {
        switch (c) {
        case VK_UP:
            ptr = L"cursor-up";
            break;
        case VK_DOWN:
            ptr = L"cursor-down";
            break;
        case VK_LEFT:
            ptr = L"cursor-left";
            break;
        case VK_RIGHT:
            ptr = L"cursor-right";
            break;
        case VK_PRIOR:
            ptr = L"page-up";
            break;
        case VK_NEXT:
            ptr = L"page-down";
            break;
        case VK_HOME:
            ptr = L"home";
            break;
        case VK_END:
            ptr = L"end";
            break;
        case VK_RETURN:
            ptr = L"enter";
            break;
        case VK_BACK:
            ptr = L"backspace";
            break;
        case VK_DELETE:
            ptr = L"delete";
            break;
        case VK_INSERT:
            ptr = L"insert";
            break;
        case VK_DIVIDE:
            ptr = L"kp-divide";
            break;
        case VK_MULTIPLY:
            ptr = L"kp-multiply";
            break;
        case VK_SUBTRACT:
            ptr = L"kp-minus";
            break;
        case VK_ADD:
            ptr = L"kp-plus";
            break;
        case VK_F1:
            ptr = L"f1";
            break;
        case VK_F2:
            ptr = L"f2";
            break;
        case VK_F3:
            ptr = L"f3";
            break;
        case VK_F4:
            ptr = L"f4";
            break;
        case VK_F5:
            ptr = L"f5";
            break;
        case VK_F6:
            ptr = L"f6";
            break;
        case VK_F7:
            ptr = L"f7";
            break;
        case VK_F8:
            ptr = L"f8";
            break;
        case VK_F9:
            ptr = L"f9";
            break;
        case VK_F10:
            ptr = L"f10";
            break;
        case VK_F11:
            ptr = L"f11";
            break;
        case VK_F12:
            ptr = L"f12";
            break;
        }
    }

    if (ptr != NULL) {
        mp_process_event(MPDM_S(ptr));

        is_wm_keydown = 1;
        mp_active();

        if (mp_keypress_throttle(1))
            redraw();
    }
}


#define ctrl(c) ((c) & 31)

static void win32_akey(int k)
/* win32 alphanumeric key processing */
{
    wchar_t c[2];
    wchar_t *ptr = NULL;

    /* set mp.shift_pressed */
    if (GetKeyState(VK_SHIFT) & 0x8000)
        mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

    switch (k) {
    case ctrl(' '):
        ptr = L"ctrl-space";
        break;
    case ctrl('a'):
        ptr = L"ctrl-a";
        break;
    case ctrl('b'):
        ptr = L"ctrl-b";
        break;
    case ctrl('c'):
        ptr = L"ctrl-c";
        break;
    case ctrl('d'):
        ptr = L"ctrl-d";
        break;
    case ctrl('e'):
        ptr = L"ctrl-e";
        break;
    case ctrl('f'):
        ptr = L"ctrl-f";
        break;
    case ctrl('g'):
        ptr = L"ctrl-g";
        break;
    case ctrl('h'):         /* same as backspace */
        break;
    case ctrl('i'):         /* same as tab */
        ptr = (GetKeyState(VK_SHIFT) & 0x8000) ? L"shift-tab" : L"tab";
        break;
    case ctrl('j'):
        ptr = L"ctrl-j";
        break;
    case ctrl('k'):
        ptr = L"ctrl-k";
        break;
    case ctrl('l'):
        ptr = L"ctrl-l";
        break;
    case ctrl('m'):            /* same as ENTER */
        break;
    case ctrl('n'):
        ptr = L"ctrl-n";
        break;
    case ctrl('o'):
        ptr = L"ctrl-o";
        break;
    case ctrl('p'):
        ptr = L"ctrl-p";
        break;
    case ctrl('q'):
        ptr = L"ctrl-q";
        break;
    case ctrl('r'):
        ptr = L"ctrl-r";
        break;
    case ctrl('s'):
        ptr = L"ctrl-s";
        break;
    case ctrl('t'):
        ptr = L"ctrl-t";
        break;
    case ctrl('u'):
        ptr = L"ctrl-u";
        break;
    case ctrl('v'):
        ptr = L"ctrl-v";
        break;
    case ctrl('w'):
        ptr = L"ctrl-w";
        break;
    case ctrl('x'):
        ptr = L"ctrl-x";
        break;
    case ctrl('y'):
        ptr = L"ctrl-y";
        break;
    case ctrl('z'):
        ptr = L"ctrl-z";
        break;
    case ' ':
        ptr = L"space";
        break;
    case 27:
        ptr = L"escape";
        break;

    default:
        /* this is probably very bad */
        c[0] = (wchar_t) k;
        c[1] = L'\0';
        ptr = c;

        break;
    }

    if (ptr != NULL) {
        mp_process_event(MPDM_S(ptr));

        mp_active();
        redraw();
    }
}


static void win32_vscroll(UINT wparam)
/* scrollbar messages handler */
{
    wchar_t *ptr = NULL;
    mpdm_t txt;

    switch (LOWORD(wparam)) {
    case SB_PAGEUP:
        ptr = L"page-up";
        break;
    case SB_PAGEDOWN:
        ptr = L"page-down";
        break;
    case SB_LINEUP:
        ptr = L"cursor-up";
        break;
    case SB_LINEDOWN:
        ptr = L"cursor-down";
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        /* set both y and vy */
        txt = mpdm_hget_s(mp_active(), L"txt");
        mp_set_y(mp_active(), HIWORD(wparam));
        mpdm_hset_s(txt, L"vy", MPDM_I(HIWORD(wparam)));
        redraw();
        break;
    }

    if (ptr != NULL) {
        mp_process_event(MPDM_S(ptr));

        redraw();
    }
}


static void action_by_menu(int item)
/* execute an action triggered by the menu */
{
    MENUITEMINFO mi;

    memset(&mi, '\0', sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_DATA;

    if (GetMenuItemInfo(menu, item, FALSE, &mi)) {
        if (mi.dwItemData != 0) {
            mp_process_action((mpdm_t) mi.dwItemData);
            mp_active();
        }
    }
}


static void dropped_files(HDROP hDrop)
/* fill the mp.dropped_files array with the dropped files */
{
    mpdm_t a = MPDM_A(0);
    char tmp[1024];
    int n;

    mpdm_ref(a);

    n = DragQueryFile(hDrop, 0xffffffff, NULL, sizeof(tmp) - 1);

    while (--n >= 0) {
        DragQueryFile(hDrop, n, tmp, sizeof(tmp) - 1);
        mpdm_push(a, MPDM_MBS(tmp));
    }

    DragFinish(hDrop);

    mpdm_hset_s(mp, L"dropped_files", a);

    mpdm_unref(a);

    mp_process_event(MPDM_LS(L"dropped-files"));

    redraw();
}


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL			0x020A
#endif

long STDCALL WndProc(HWND hwnd, UINT msg, UINT wparam, LONG lparam)
/* main window Proc */
{
    int x, y;
    LPNMHDR p;
    wchar_t *ptr = NULL;

    switch (msg) {
    case WM_CREATE:

        is_wm_keydown = 0;
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_DROPFILES:

        dropped_files((HDROP) wparam);
        return 0;

    case WM_SYSKEYUP:
    case WM_KEYUP:

        is_wm_keydown = 0;

        if (mp_keypress_throttle(0))
            redraw();

        return 0;

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:

        win32_vkey(wparam);
        return 0;

    case WM_CHAR:

        win32_akey(wparam);
        return 0;

    case WM_VSCROLL:

        win32_vscroll(wparam);
        return 0;

    case WM_PAINT:

        if (mpdm_size(mpdm_hget_s(mp, L"docs")))
            win32_draw(hwnd, mp_active());

        return 0;

    case WM_SIZE:

        if (!IsIconic(hwnd)) {
            update_window_size();

            MoveWindow(hwtabs, 0, 0, LOWORD(lparam), tab_height, FALSE);

            MoveWindow(hwstatus, 0, HIWORD(lparam) - status_height,
                       LOWORD(lparam), status_height, FALSE);

            redraw();
        }

        return 0;

    case WM_LBUTTONDOWN:

        mouse_down = 1;
        /* fallthrough */

    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:

        x = (LOWORD(lparam)) / font_width;
        y = (HIWORD(lparam) - tab_height) / font_height;

        mpdm_hset_s(mp, L"mouse_x", MPDM_I(x));
        mpdm_hset_s(mp, L"mouse_y", MPDM_I(y));

        switch (msg) {
        case WM_LBUTTONDOWN:
            ptr = L"mouse-left-button";
            break;
        case WM_RBUTTONDOWN:
            ptr = L"mouse-right-button";
            break;
        case WM_MBUTTONDOWN:
            ptr = L"mouse-middle-button";
            break;
        }

        if (ptr != NULL) {
            mp_process_event(MPDM_S(ptr));

            redraw();
        }

        return 0;

    case WM_LBUTTONUP:

        mouse_down = 0;
        return 0;

    case WM_MOUSEMOVE:

        if (mouse_down) {
            x = (LOWORD(lparam)) / font_width;
            y = (HIWORD(lparam) - tab_height) / font_height;

            mpdm_hset_s(mp, L"mouse_to_x", MPDM_I(x));
            mpdm_hset_s(mp, L"mouse_to_y", MPDM_I(y));

            mp_process_event(MPDM_LS(L"mouse-drag"));

            redraw();
        }

        return 0;

    case WM_MOUSEWHEEL:

        if ((int) wparam > 0)
            ptr = L"mouse-wheel-up";
        else
            ptr = L"mouse-wheel-down";

        if (ptr != NULL) {
            mp_process_event(MPDM_S(ptr));

            redraw();
        }

        return 0;

    case WM_COMMAND:

        action_by_menu(LOWORD(wparam));
        redraw();

        return 0;

    case WM_CLOSE:

        if (!mp_exit_requested)
            mp_process_event(MPDM_LS(L"close-window"));

        if (mp_exit_requested)
            DestroyWindow(hwnd);

        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NOTIFY:
        p = (LPNMHDR) lparam;

        if (p->code == TCN_SELCHANGE) {
            /* tab selected by clicking on it */
            int n = TabCtrl_GetCurSel(hwtabs);

            /* set mp.active_i to this */
            mpdm_hset_s(mp, L"active_i", MPDM_I(n));

            redraw();
        }

        return 0;

    case WM_TIMER:
        mpdm_void(mpdm_exec(timer_func, NULL, NULL));
        redraw();

        return 0;
    }

    if (mp_exit_requested)
        PostMessage(hwnd, WM_CLOSE, 0, 0);

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}


static mpdm_t win32_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
/* driver-dependent mp to system clipboard */
{
    HGLOBAL hclp;
    mpdm_t d, v;
    char *ptr;
    char *clpptr;
    int s;

    /* convert the clipboard to DOS text */
    d = mpdm_hget_s(mp, L"clipboard");

    if (mpdm_size(d) == 0)
        return NULL;

    v = mpdm_ref(mpdm_join_s(d, L"\r\n"));
    ptr = mpdm_wcstombs(v->data, &s);

    /* allocates a handle and copies */
    hclp = GlobalAlloc(GHND, s + 1);
    clpptr = (char *) GlobalLock(hclp);
    memcpy(clpptr, ptr, s);
    clpptr[s] = '\0';
    GlobalUnlock(hclp);

    free(ptr);

    OpenClipboard(NULL);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, hclp);
    CloseClipboard();

    mpdm_unref(v);

    return NULL;
}


static mpdm_t win32_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
/* driver-dependent system to mp clipboard */
{
    HGLOBAL hclp;
    char *ptr;

    OpenClipboard(NULL);
    hclp = GetClipboardData(CF_TEXT);
    CloseClipboard();

    if (hclp && (ptr = GlobalLock(hclp)) != NULL) {
        mpdm_t d, v;

        /* create a value and split */
        v = mpdm_ref(MPDM_MBS(ptr));
        d = mpdm_ref(mpdm_split_s(v, L"\r\n"));

        /* and set as the clipboard */
        mpdm_hset_s(mp, L"clipboard", d);
        mpdm_hset_s(mp, L"clipboard_vertical", MPDM_I(0));

        GlobalUnlock(hclp);

        mpdm_unref(d);
        mpdm_unref(v);
    }

    return NULL;
}


static mpdm_t win32_drv_main_loop(mpdm_t a, mpdm_t ctxt)
{
    MSG msg;

    if (!mp_exit_requested) {
        mp_active();

        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return NULL;
}


static mpdm_t win32_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    SendMessage(hwnd, WM_CLOSE, 0, 0);

    if ((v = mpdm_hget_s(mp, L"exit_message")) != NULL) {
        char *ptr = mpdm_wcstombs(mpdm_string(v), NULL);
        MessageBox(NULL, ptr, "mp " VERSION, MB_ICONWARNING | MB_OK);
        free(ptr);
    }

    return NULL;
}


static mpdm_t win32_drv_alert(mpdm_t a, mpdm_t ctxt)
/* alert driver function */
{
    wchar_t *wptr;
    char *ptr;

    /* 1# arg: prompt */
    wptr = mpdm_string(mpdm_aget(a, 0));

    if ((ptr = mpdm_wcstombs(wptr, NULL)) != NULL) {
        MessageBox(hwnd, ptr, "mp " VERSION, MB_ICONWARNING | MB_OK);
        free(ptr);
    }

    return NULL;
}


static mpdm_t win32_drv_confirm(mpdm_t a, mpdm_t ctxt)
/* confirm driver function */
{
    wchar_t *wptr;
    char *ptr;
    int ret = 0;

    /* 1# arg: prompt */
    wptr = mpdm_string(mpdm_aget(a, 0));

    if ((ptr = mpdm_wcstombs(wptr, NULL)) != NULL) {
        ret =
            MessageBox(hwnd, ptr, "mp " VERSION,
                       MB_ICONQUESTION | MB_YESNOCANCEL);
        free(ptr);

        if (ret == IDYES)
            ret = 1;
        else
        if (ret == IDNO)
            ret = 2;
        else
            ret = 0;
    }

    return MPDM_I(ret);
}


static LPWORD lpwAlign(LPWORD lpIn)
/* aligns a pointer to DWORD boundary (for dialog templates) */
{
    ULONG ul;

    ul = (ULONG) lpIn;
    ul++;
    ul >>= 1;
    ul <<= 1;
    return (LPWORD) ul;
}


#define LABEL_ID	1000
#define CTRL_ID		2000

BOOL CALLBACK formDlgProc(HWND hwnd, UINT msg, WPARAM wparam,
                          LPARAM lparam)
/* mp.drv.form() dialog proc */
{
    int n;
    HFONT hf;

    switch (msg) {
    case WM_INITDIALOG:

        SetWindowText(hwnd, "mp " VERSION);

        hf = GetStockObject(DEFAULT_GUI_FONT);

        /* fill controls with its initial data */
        for (n = 0; n < mpdm_size(form_args); n++) {
            mpdm_t w = mpdm_aget(form_args, n);
            wchar_t *type;
            mpdm_t t;
            int ctrl = CTRL_ID + n;
            wchar_t *wptr;
            char *ptr;

            if ((t = mpdm_hget_s(w, L"label")) != NULL) {
                if ((ptr = mpdm_wcstombs(mpdm_string(t), NULL)) != NULL) {
                    SetDlgItemText(hwnd, LABEL_ID + n, ptr);
                    free(ptr);

                    SendDlgItemMessage(hwnd, LABEL_ID + n, WM_SETFONT,
                                       (WPARAM) hf, MAKELPARAM(FALSE, 0));
                }
            }

            SendDlgItemMessage(hwnd, ctrl, WM_SETFONT,
                               (WPARAM) hf, MAKELPARAM(FALSE, 0));

            type = mpdm_string(mpdm_hget_s(w, L"type"));

            if (wcscmp(type, L"text") == 0) {
                if ((t = mpdm_hget_s(w, L"value")) != NULL &&
                    (ptr = mpdm_wcstombs(mpdm_string(t), NULL)) != NULL) {
                    SetDlgItemText(hwnd, ctrl, ptr);
                    free(ptr);
                }

                /* store the history into combo_items */
                if ((t = mpdm_hget_s(w, L"history")) != NULL) {
                    t = mp_get_history(t);
                    int i;

                    for (i = 0; i < mpdm_size(t); i++) {
                        mpdm_t v = mpdm_aget(t, i);

                        if ((ptr = mpdm_wcstombs(v->data, NULL)) != NULL) {
                            SendDlgItemMessage(hwnd,
                                               ctrl,
                                               CB_INSERTSTRING, 0,
                                               (LPARAM) ptr);
                            free(ptr);
                        }
                    }
                }
            }
            else
            if (wcscmp(type, L"password") == 0) {
                SendDlgItemMessage(hwnd, ctrl,
                                   EM_SETPASSWORDCHAR, (WPARAM) '*',
                                   (LPARAM) 0);
            }
            else
            if (wcscmp(type, L"checkbox") == 0) {
                if ((t = mpdm_hget_s(w, L"value")) != NULL)
                    SendDlgItemMessage(hwnd, ctrl,
                                       BM_SETCHECK, mpdm_ival(t) ?
                                       BST_CHECKED : BST_UNCHECKED, 0);
            }
            else
            if (wcscmp(type, L"list") == 0) {
                int i;

                t = mpdm_hget_s(w, L"list");

                /* fill the list */
                for (i = 0; i < mpdm_size(t); i++) {
                    wptr = mpdm_string(mpdm_aget(t, i));
                    if ((ptr = mpdm_wcstombs(wptr, NULL)) != NULL) {
                        SendDlgItemMessage(hwnd, ctrl,
                                           LB_ADDSTRING, 0, (LPARAM) ptr);
                        free(ptr);
                    }
                }

                /* set position */
                SendDlgItemMessage(hwnd, ctrl, LB_SETCURSEL,
                                   mpdm_ival(mpdm_hget_s(w, L"value")), 0);
            }
        }

        /* FIXME: untranslated strings */

        SetDlgItemText(hwnd, IDOK, "OK");
        SendDlgItemMessage(hwnd, IDOK, WM_SETFONT,
                           (WPARAM) hf, MAKELPARAM(FALSE, 0));

        SetDlgItemText(hwnd, IDCANCEL, "Cancel");
        SendDlgItemMessage(hwnd, IDCANCEL, WM_SETFONT,
                           (WPARAM) hf, MAKELPARAM(FALSE, 0));

        return TRUE;

    case WM_COMMAND:

        if (LOWORD(wparam) == IDCANCEL) {
            EndDialog(hwnd, 0);
            return TRUE;
        }

        if (LOWORD(wparam) != IDOK)
            break;

        /* fill all return values */
        for (n = 0; n < mpdm_size(form_args); n++) {
            mpdm_t w = mpdm_aget(form_args, n);
            wchar_t *type = mpdm_string(mpdm_hget_s(w, L"type"));
            int ctrl = CTRL_ID + n;

            if (wcscmp(type, L"text") == 0) {
                char tmp[2048];
                mpdm_t v;
                mpdm_t h;

                GetDlgItemText(hwnd, ctrl, tmp, sizeof(tmp) - 1);
                v = MPDM_MBS(tmp);

                mpdm_aset(form_values, v, n);

                /* if it has history, fill it */
                if ((h = mpdm_hget_s(w, L"history")) != NULL &&
                    v != NULL && mpdm_cmp_s(v, L"") != 0) {
                    h = mp_get_history(h);

                    if (mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
                        mpdm_push(h, v);
                }
            }
            if (wcscmp(type, L"password") == 0) {
                char tmp[2048];

                GetDlgItemText(hwnd, ctrl, tmp, sizeof(tmp) - 1);
                mpdm_aset(form_values, MPDM_MBS(tmp), n);
            }
            else
            if (wcscmp(type, L"checkbox") == 0) {
                mpdm_aset(form_values,
                          MPDM_I(SendDlgItemMessage(hwnd, ctrl,
                                                    BM_GETCHECK, 0, 0)),
                          n);
            }
            else
            if (wcscmp(type, L"list") == 0) {
                mpdm_aset(form_values,
                          MPDM_I(SendDlgItemMessage(hwnd, ctrl,
                                                    LB_GETCURSEL, 0, 0)),
                          n);
            }
        }

        EndDialog(hwnd, 1);
        return TRUE;
    }

    return FALSE;
}


static void build_form_data(mpdm_t widget_list)
/* builds the necessary information for a list of widgets */
{
    mpdm_unref(form_args);
    form_args = mpdm_ref(widget_list);

    mpdm_unref(form_values);
    form_values = widget_list == NULL ? NULL :
        mpdm_ref(MPDM_A(mpdm_size(form_args)));
}


LPWORD static build_control(LPWORD lpw, int x, int y,
                            int cx, int cy, int id, int class, int style)
/* fills a control structure in a hand-made dialog template */
{
    LPDLGITEMTEMPLATE lpdit;

    lpw = lpwAlign(lpw);
    lpdit = (LPDLGITEMTEMPLATE) lpw;
    lpdit->x = x;
    lpdit->y = y;
    lpdit->cx = cx;
    lpdit->cy = cy;
    lpdit->id = id;
    lpdit->style = style;

    lpw = (LPWORD) (lpdit + 1);
    *lpw++ = 0xFFFF;
    *lpw++ = class;

    /* no text (will be set on dialog setup) */
    *lpw++ = 0;
    *lpw++ = 0;

    /* Align creation data on DWORD boundary */
    lpw = lpwAlign(lpw);
    /* No creation data */
    *lpw++ = 0;

    return lpw;
}


static mpdm_t win32_drv_form(mpdm_t a, mpdm_t ctxt)
/* mp.drv.form() function */
{
    HGLOBAL hgbl;
    LPDLGTEMPLATE lpdt;
    LPWORD lpw;
    int n, p;
    int il = 10;
    int lbl = 0;

    /* first argument: list of widgets */
    build_form_data(mpdm_aget(a, 0));

    /* On-the-fly dialog template creation */
    /* Note: all this crap is taken from MSDN, no less */

    /* magic size; looking for problems */
    hgbl = GlobalAlloc(GMEM_ZEROINIT, 4096);
    lpdt = (LPDLGTEMPLATE) GlobalLock(hgbl);

    lpdt->style =
        WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
    lpdt->cdit = (2 * mpdm_size(form_args)) + 2;
    lpdt->x = 20;
    lpdt->y = 20;
    lpdt->cx = 260;

    lpw = (LPWORD) (lpdt + 1);
    *lpw++ = 0;                 /* No menu */
    *lpw++ = 0;                 /* Predefined dialog box class (by default) */
    *lpw++ = 0;                 /* No title */

    /* first pass: calculate maximum size of labels */
    for (n = 0; n < mpdm_size(form_args); n++) {
        mpdm_t w = mpdm_aget(form_args, n);
        int l = mpdm_size(mpdm_hget_s(w, L"label"));

        if (lbl < l)
            lbl = l;
    }

    /* second pass: create the dialog controls */
    for (n = p = 0; n < mpdm_size(form_args); n++) {
        mpdm_t w = mpdm_aget(form_args, n);
        wchar_t *type;
        int class;
        int style;
        int inc = 1;
        int sz = 1;

        /* label control */
        lpw = build_control(lpw, 0, 5 + p * il,
                            lbl * 3, 20, LABEL_ID + n, 0x0082,
                            WS_CHILD | WS_VISIBLE | SS_RIGHT);

        type = mpdm_string(mpdm_hget_s(w, L"type"));

        if (wcscmp(type, L"text") == 0) {
            class = 0x0085;
            style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL;

            /* size */
            sz = 5;
        }
        else
        if (wcscmp(type, L"password") == 0) {
            class = 0x0081;
            style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP;
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            class = 0x0080;
            style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
        }
        else
        if (wcscmp(type, L"list") == 0) {
            class = 0x0083;
            style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                LBS_NOINTEGRALHEIGHT | WS_VSCROLL |
                LBS_NOTIFY | LBS_USETABSTOPS;

            /* height */
            inc = 5;
        }

        /* the control */
        lpw = build_control(lpw, 10 + lbl * 3, 5 + p * il,
                            245 - lbl * 3, inc * il * sz, CTRL_ID + n,
                            class, style);

        /* next position */
        p += inc;
    }

    /* set total height */
    lpdt->cy = 30 + p * il;

    /* OK */
    lpw = build_control(lpw, 170, 10 + p * il, 40, 15, IDOK,
                        0x0080,
                        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON |
                        WS_TABSTOP);

    /* Cancel */
    lpw = build_control(lpw, 215, 10 + p * il, 40, 15, IDCANCEL,
                        0x0080,
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON |
                        WS_TABSTOP);

    GlobalUnlock(hgbl);
    n = DialogBoxIndirect(hinst, (LPDLGTEMPLATE) hgbl,
                          hwnd, (DLGPROC) formDlgProc);

    GlobalFree(hgbl);

    return n ? form_values : NULL;
}


static mpdm_t open_or_save(int o, mpdm_t a)
/* manages an open or save file dialog */
{
    OPENFILENAME ofn;
    wchar_t *wptr;
    char *ptr;
    char buf[1024] = "";
    char buf2[1024];
    int r;

    /* 1# arg: prompt */
    wptr = mpdm_string(mpdm_aget(a, 0));
    ptr = mpdm_wcstombs(wptr, NULL);

    memset(&ofn, '\0', sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "*.*\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = sizeof(buf);
    ofn.lpstrTitle = ptr;
    ofn.lpstrDefExt = "";

    GetCurrentDirectory(sizeof(buf2), buf2);
    ofn.lpstrInitialDir = buf2;

/*	ofn.lpstrDefExt=(def==NULL ? "" : def);*/

    if (o) {
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
            OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST;

        r = GetOpenFileName(&ofn);
    }
    else {
        ofn.Flags = OFN_HIDEREADONLY;

        r = GetSaveFileName(&ofn);
    }

    free(ptr);

    if (r)
        return MPDM_MBS(buf);

    return NULL;
}


static mpdm_t win32_drv_openfile(mpdm_t a, mpdm_t ctxt)
/* openfile driver function */
{
    return open_or_save(1, a);
}


static mpdm_t win32_drv_savefile(mpdm_t a, mpdm_t ctxt)
/* savefile driver function */
{
    return open_or_save(0, a);
}


static mpdm_t win32_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    build_fonts(GetDC(hwnd));
    build_colors();
    build_menu();

    return NULL;
}


static mpdm_t win32_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    int msecs = mpdm_ival(mpdm_aget(a, 0));
    mpdm_t func = mpdm_aget(a, 1);

    /* previously defined one? remove */
    if (timer_func != NULL)
        KillTimer(hwnd, 1);

    /* if msecs and func are set, program timer */
    if (msecs > 0 && func != NULL)
        SetTimer(hwnd, 1, msecs, NULL);

    mpdm_ref(func);
    mpdm_unref(timer_func);
    timer_func = func;

    return NULL;
}


static mpdm_t win32_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    int onoff = mpdm_ival(mpdm_aget(a, 0));

    SetCursor(LoadCursor(NULL, onoff ? IDC_WAIT : IDC_ARROW));

    return NULL;
}


static void register_functions(void)
{
    mpdm_t drv;

    drv = mpdm_hget_s(mp, L"drv");
    mpdm_hset_s(drv, L"main_loop",   MPDM_X(win32_drv_main_loop));
    mpdm_hset_s(drv, L"shutdown",    MPDM_X(win32_drv_shutdown));
    mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(win32_drv_clip_to_sys));
    mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(win32_drv_sys_to_clip));
    mpdm_hset_s(drv, L"update_ui",   MPDM_X(win32_drv_update_ui));
    mpdm_hset_s(drv, L"timer",       MPDM_X(win32_drv_timer));
    mpdm_hset_s(drv, L"busy",        MPDM_X(win32_drv_busy));
    mpdm_hset_s(drv, L"alert",       MPDM_X(win32_drv_alert));
    mpdm_hset_s(drv, L"confirm",     MPDM_X(win32_drv_confirm));
    mpdm_hset_s(drv, L"openfile",    MPDM_X(win32_drv_openfile));
    mpdm_hset_s(drv, L"savefile",    MPDM_X(win32_drv_savefile));
    mpdm_hset_s(drv, L"form",        MPDM_X(win32_drv_form));
}


static mpdm_t win32_drv_startup(mpdm_t a, mpdm_t ctxt)
{
    WNDCLASSW wc;
    RECT r;
    mpdm_t v;

    register_functions();

    InitCommonControls();

    /* register the window */
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hinst;
    wc.hIcon = LoadIcon(hinst, "MP_ICON");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"minimumprofit5.x";

    RegisterClassW(&wc);

    /* create the window */
    hwnd = CreateWindowW(L"minimumprofit5.x", L"mp " VERSION,
                         WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN |
                         WS_VSCROLL, CW_USEDEFAULT, CW_USEDEFAULT,
                         CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hinst,
                         NULL);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    GetClientRect(hwnd, &r);

    hwtabs = CreateWindow(WC_TABCONTROL, "tab",
                          WS_CHILD | TCS_TABS | TCS_SINGLELINE |
                          TCS_FOCUSNEVER, 0, 0, r.right - r.left,
                          tab_height, hwnd, NULL, hinst, NULL);

    SendMessage(hwtabs, WM_SETFONT,
                (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

    ShowWindow(hwtabs, SW_SHOW);
    UpdateWindow(hwtabs);

    hwstatus = CreateWindow(WC_STATIC, "status",
                            WS_CHILD,
                            0, r.bottom - r.top - status_height,
                            r.right - r.left, status_height, hwnd, NULL,
                            hinst, NULL);

    win32_drv_update_ui(NULL, NULL);

    SendMessage(hwstatus, WM_SETFONT,
                (WPARAM) GetStockObject(DEFAULT_GUI_FONT), 0);

    ShowWindow(hwstatus, SW_SHOW);
    UpdateWindow(hwstatus);

    if ((v = mpdm_hget_s(mp, L"config")) != NULL &&
        mpdm_ival(mpdm_hget_s(v, L"maximize")) > 0)
        SendMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);

    return NULL;
}


int win32_drv_detect(int *argc, char ***argv)
{
    mpdm_t drv;
    int n;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-txt") == 0 ||
            strcmp(argv[0][n], "-h") == 0)
            return 0;
    }

    drv = mpdm_hget_s(mp, L"drv");
    mpdm_hset_s(drv, L"id", MPDM_LS(L"win32"));
    mpdm_hset_s(drv, L"startup", MPDM_X(win32_drv_startup));

    return 1;
}

#endif                          /* CONFOPT_WIN32 */
