/*

    Minimum Profit - Programmer Text Editor

    Win32 driver.

    Copyright (C) 1991-2006 Angel Ortega <angel@triptico.com>

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

#include "mp_res.h"

/*******************
	Data
********************/

/* the instance */
HINSTANCE hinst;

/* the windows */
HWND hwnd = NULL;
HWND hwtabs = NULL;
HWND hwstatus = NULL;

/* font handlers and metrics */
HFONT font_normal = NULL;
int font_width = 0;
int font_height = 0;

/* height of the tab set */
int tab_height = 28;

/* height of the status bar */
int status_height = 16;

int is_wm_keydown = 0;

/* colors */
static COLORREF * inks = NULL;
static COLORREF * papers = NULL;
HBRUSH bgbrush;

/* prompt for dialogs */
static char * dialog_prompt = NULL;

/* code for the 'normal' attribute */
static int normal_attr = 0;

/* readline text */
static mpdm_t readline_text = NULL;

/* history for readline */
static mpdm_t readline_history = NULL;

/* default value for readline */
static mpdm_t readline_default = NULL;

/* list data */
static mpdm_t list_data = NULL;

/* list position */
int list_pos = -1;

/* the menu */
static HMENU menu = NULL;

/* show the entry as password */
static int entry_is_password = 0;

/*******************
	Code
********************/

static void update_window_size(void)
/* updates the viewport size in characters */
{
	RECT rect;
	int tx, ty;
	mpdm_t v;

	/* no font information? go */
	if(font_width == 0 || font_height == 0)
		return;

	GetClientRect(hwnd, &rect);

	/* calculate the size in chars */
	tx = ((rect.right - rect.left) / font_width) + 1;
	ty = ((rect.bottom - rect.top - tab_height) / font_height) + 1;

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
	int font_size = 14;
	char * font_face = "Lucida Console";
	mpdm_t c;

	if(font_normal != NULL)
	{
		SelectObject(hdc, GetStockObject(SYSTEM_FONT));
		DeleteObject(font_normal);
	}

	/* get current configuration */
	if((c = mpdm_hget_s(mp, L"config")) != NULL)
	{
		mpdm_t v;

		if((v = mpdm_hget_s(c, L"font_size")) != NULL)
			font_size = mpdm_ival(v);
		else
			mpdm_hset_s(c, L"font_size", MPDM_I(font_size));

		if((v = mpdm_hget_s(c, L"font_face")) != NULL)
		{
			v = MPDM_2MBS(v->data);
			font_face = v->data;
		}
		else
			mpdm_hset_s(c, L"font_face", MPDM_MBS(font_face));
	}

	/* create fonts */
	n = -MulDiv(font_size, GetDeviceCaps(hdc, LOGPIXELSY), 72);

	font_normal = CreateFont(n, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, font_face);

	SelectObject(hdc, font_normal);
	GetTextMetrics(hdc, &tm);

	/* store sizes */
	font_height = tm.tmHeight;
	font_width = tm.tmAveCharWidth;

	update_window_size();
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
	l = mpdm_keys(colors);
	s = mpdm_size(l);

	/* redim the structures */
	inks = realloc(inks, sizeof(COLORREF) * s);
	papers = realloc(papers, sizeof(COLORREF) * s);

	/* loop the colors */
	for(n = 0;n < s && (c = mpdm_aget(l, n)) != NULL;n++)
	{
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"gui");
		int m;

		/* store the 'normal' attribute */
		if(wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		m = mpdm_ival(mpdm_aget(v, 0));
		inks[n] = ((m & 0x000000ff) << 16)|
			 ((m & 0x0000ff00)) |
			 ((m & 0x00ff0000) >> 16);
		m = mpdm_ival(mpdm_aget(v, 1));
		papers[n] = ((m & 0x000000ff) << 16)|
			 ((m & 0x0000ff00)) |
			 ((m & 0x00ff0000) >> 16);

		/* flags */
		v = mpdm_hget_s(d, L"flags");
/*		underlines[attr] = mpdm_seek_s(v, L"underline", 1) != -1 ? 1 : 0;
*/
		if(mpdm_seek_s(v, L"reverse", 1) != -1)
		{
			COLORREF t;

			t = inks[n];
			inks[n] = papers[n];
			papers[n] = t;
		}
	}

	/* create the background brush */
	bgbrush = CreateSolidBrush(papers[normal_attr]);
}


static void build_menu(void)
/* builds the menu */
{
	int n;
	mpdm_t desc, m;
	int w32_menu_id = 1000;

	/* gets the current menu */
	if((m = mpdm_hget_s(mp, L"menu")) == NULL)
		return;

	/* get the action descriptions */
	desc = mpdm_hget_s(mp, L"actdesc");

	if(menu != NULL)
		DestroyMenu(menu);

	menu = CreateMenu();

	for(n = 0;n < mpdm_size(m);n += 2)
	{
		char * ptr;
		mpdm_t v, l;
		int i;
		HMENU submenu = CreatePopupMenu();

		/* get the label and the items */
		v = mpdm_aget(m, n);
		l = mpdm_aget(m, n + 1);

		/* create the submenus */
		for(i = 0;i < mpdm_size(l);i++)
		{
			/* get the action */
			mpdm_t v = mpdm_aget(l, i);

			/* if the action is a separator... */
			if(*((wchar_t *)v->data) == L'-')
				AppendMenu(submenu, MF_SEPARATOR, 0, NULL);
			else
			{
				MENUITEMINFO mi;
				mpdm_t d = mp_menu_label(v);

				/* set the string */
				ptr = mpdm_wcstombs(mpdm_string(d), NULL);
				AppendMenu(submenu, MF_STRING, w32_menu_id, ptr);
				free(ptr);

				/* store the action inside the menu */
				memset(&mi, '\0', sizeof(mi));
				mi.cbSize = sizeof(mi);
				mi.fMask = MIIM_DATA;
				mi.dwItemData = (unsigned long)v;

				SetMenuItemInfo(submenu, w32_menu_id, FALSE, &mi);

				w32_menu_id++;
			}
		}

		/* now store the popup inside the menu */
		ptr = mpdm_wcstombs(mpdm_string(v), NULL);
		AppendMenu(menu, MF_STRING|MF_POPUP, (UINT)submenu, ptr);
		free(ptr);
	}

	SetMenu(hwnd, menu);
}


static void draw_filetabs(void)
/* draws the filetabs */
{
	static mpdm_t last = NULL;
	mpdm_t docs, names;
	int n;

	if(hwtabs == NULL)
		return;

	docs = mpdm_hget_s(mp, L"docs");
	names = MPDM_A(mpdm_size(docs));

	/* gets a list with the names of the documents */
	for(n = 0;n < mpdm_size(docs);n++)
		mpdm_aset(names, mpdm_hget_s(mpdm_aget(docs, n), L"name"), n);

	/* is the list different from the previous one? */
	if(mpdm_cmp(names, last) != 0)
	{
		TabCtrl_DeleteAllItems(hwtabs);

		for(n = 0;n < mpdm_size(names);n++)
		{
			TCITEM ti;
			char * ptr;
			wchar_t * wptr;
			mpdm_t v = mpdm_aget(names, n);

			/* move to the filename if path included */
			if((wptr = wcsrchr(v->data, L'\\')) == NULL)
				wptr = v->data;
			else
				wptr++;

			/* convert to mbs */
			ptr = mpdm_wcstombs(wptr, NULL);

			ti.mask = TCIF_TEXT;
			ti.pszText = ptr;

			/* create it */
			TabCtrl_InsertItem(hwtabs, n, &ti);

			free(ptr);
		}

		/* store for the next time */
		mpdm_unref(last); last = mpdm_ref(names);
	}

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
	if((d = mp_active()) == NULL)
		return;

	/* get the coordinates */
	v = mpdm_hget_s(d, L"txt");
	pos = mpdm_ival(mpdm_hget_s(v, L"y"));
	max = mpdm_size(mpdm_hget_s(v, L"lines"));

	v = mpdm_hget_s(mp, L"window");
	size = mpdm_ival(mpdm_hget_s(v, L"ty"));

	si.cbSize=sizeof(si);
	si.fMask=SIF_ALL;
	si.nMin=1;
	si.nMax=max;
	si.nPage=size;
	si.nPos=pos;

	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}


void draw_status(void)
/* draws the status line */
{
	mpdm_t t;

	if(hwstatus != NULL && (t = mp_build_status_line()) != NULL)
	{
		t = MPDM_2MBS(t->data);

		if(t->data != NULL)
			SetWindowText(hwstatus, t->data);
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
	if(font_normal == NULL)
	{
		build_fonts(hdc);
		build_colors();
	}

	/* no document? end */
	if((d = mp_draw(doc, 0)) == NULL)
	{
		EndPaint(hwnd, &ps);
		return;
	}

	/* select defaults to start painting */
	SelectObject(hdc, font_normal);

	GetClientRect(hwnd, &rect);
	r2 = rect;

	r2.top += tab_height;
	r2.bottom = r2.top + font_height;

	for(n = 0;n < mpdm_size(d);n++)
	{
		mpdm_t l = mpdm_aget(d, n);

		r2.left = rect.left;

		for(m = 0;m < mpdm_size(l);m++)
		{
			int attr;
			mpdm_t s;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			SetTextColor(hdc, inks[attr]);
			SetBkColor(hdc, papers[attr]);
/*
			SelectObject(hdc, color==MP_COLOR_COMMENT ?
				_font_italic :
				color==MP_COLOR_LOCAL ? _font_underline :
				_font_normal);
*/
/*			DrawTextW(hdc, (wchar_t *)s->data,
				-1, &r2, DT_SINGLELINE|DT_NOPREFIX);*/
			TextOutW(hdc, r2.left, r2.top, s->data, mpdm_size(s));
			r2.left += mpdm_size(s) * font_width;
		}

		/* fills the rest of the line */
		FillRect(hdc, &r2, bgbrush);

		r2.top += font_height;
		r2.bottom += font_height;
	}

	EndPaint(hwnd, &ps);

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
	wchar_t * ptr = NULL;
	static int maxed = 0;

/*	mpi_move_selecting=(GetKeyState(VK_SHIFT) & 0x8000);
*/
	if(GetKeyState(VK_CONTROL) & 0x8000 ||
	   GetKeyState(VK_MENU) & 0x8000)
	{
		switch(c)
		{
		case VK_UP:		ptr = L"ctrl-cursor-up"; break;
		case VK_DOWN:		ptr = L"ctrl-cursor-down"; break;
		case VK_LEFT:		ptr = L"ctrl-cursor-left"; break;
		case VK_RIGHT:		ptr = L"ctrl-cursor-right"; break;
		case VK_PRIOR:		ptr = L"ctrl-page-up"; break;
		case VK_NEXT:		ptr = L"ctrl-page-down"; break;
		case VK_HOME:		ptr = L"ctrl-home"; break;
		case VK_END:		ptr = L"ctrl-end"; break;
		case VK_SPACE:		ptr = L"ctrl-space"; break;
		case VK_DIVIDE:		ptr = L"ctrl-kp-divide"; break;
		case VK_MULTIPLY:	ptr = L"ctrl-kp-multiply"; break;
		case VK_SUBTRACT:	ptr = L"ctrl-kp-minus"; break;
		case VK_ADD:		ptr = L"ctrl-kp-plus"; break;
		case VK_RETURN:		ptr = L"ctrl-enter"; break;
		case VK_F1:		ptr = L"ctrl-f1"; break;
		case VK_F2:		ptr = L"ctrl-f2"; break;
		case VK_F3:		ptr = L"ctrl-f3"; break;
		case VK_F4:		ptr = L"ctrl-f4"; break;
		case VK_F5:		ptr = L"ctrl-f5"; break;
		case VK_F6:		ptr = L"ctrl-f6"; break;
		case VK_F7:		ptr = L"ctrl-f7"; break;
		case VK_F8:		ptr = L"ctrl-f8"; break;
		case VK_F9:		ptr = L"ctrl-f9"; break;
		case VK_F10:		ptr = L"ctrl-f10"; break;
		case VK_F11:		ptr = L"ctrl-f11"; break;
		case VK_F12:
			SendMessage(hwnd, WM_SYSCOMMAND,
			maxed ? SC_RESTORE : SC_MAXIMIZE, 0);

			maxed ^= 1;

			break;
		}
	}
	else
	{
		switch(c)
		{
		case VK_UP:		ptr = L"cursor-up"; break;
		case VK_DOWN:		ptr = L"cursor-down"; break;
		case VK_LEFT:		ptr = L"cursor-left"; break;
		case VK_RIGHT:		ptr = L"cursor-right"; break;
		case VK_PRIOR:		ptr = L"page-up"; break;
		case VK_NEXT:		ptr = L"page-down"; break;
		case VK_HOME:		ptr = L"home"; break;
		case VK_END:		ptr = L"end"; break;
		case VK_TAB:		ptr = L"tab"; break;
		case VK_RETURN: 	ptr = L"enter"; break;
		case VK_BACK:		ptr = L"backspace"; break;
		case VK_DELETE: 	ptr = L"delete"; break;
		case VK_INSERT: 	ptr = L"insert"; break;
		case VK_DIVIDE:   	ptr = L"kp-divide"; break;
		case VK_MULTIPLY: 	ptr = L"kp-multiply"; break;
		case VK_SUBTRACT: 	ptr = L"kp-minus"; break;
		case VK_ADD:	  	ptr = L"kp-plus"; break;
		case VK_F1:		ptr = L"f1"; break;
		case VK_F2:		ptr = L"f2"; break;
		case VK_F3:		ptr = L"f3"; break;
		case VK_F4:		ptr = L"f4"; break;
		case VK_F5:		ptr = L"f5"; break;
		case VK_F6:		ptr = L"f6"; break;
		case VK_F7:		ptr = L"f7"; break;
		case VK_F8:		ptr = L"f8"; break;
		case VK_F9:		ptr = L"f9"; break;
		case VK_F10:		ptr = L"f10"; break;
		case VK_F11:		ptr = L"f11"; break;
		case VK_F12:		ptr = L"f12"; break;
		}
	}

	if(ptr != NULL)
	{
		mp_process_event(MPDM_S(ptr));
		is_wm_keydown = 1;
		redraw();
	}
}


#define ctrl(c) ((c) & 31)

static void win32_akey(int k)
/* win32 alphanumeric key processing */
{
	wchar_t c[2];
	wchar_t * ptr = NULL;

	if (is_wm_keydown)
		return;

	switch(k)
	{
	case ctrl(' '):		ptr = L"ctrl-space"; break;
	case ctrl('a'):		ptr = L"ctrl-a"; break;
	case ctrl('b'):		ptr = L"ctrl-b"; break;
	case ctrl('c'):		ptr = L"ctrl-c"; break;
	case ctrl('d'):		ptr = L"ctrl-d"; break;
	case ctrl('e'):		ptr = L"ctrl-e"; break;
	case ctrl('f'):		ptr = L"ctrl-f"; break;
	case ctrl('g'):		ptr = L"ctrl-g"; break;
	case ctrl('h'):		ptr = L"ctrl-h"; break;
	case ctrl('i'):		ptr = L"ctrl-i"; break;
	case ctrl('j'):		ptr = L"ctrl-j"; break;
	case ctrl('k'):		ptr = L"ctrl-k"; break;
	case ctrl('l'):		ptr = L"ctrl-l"; break;
	case ctrl('m'):		ptr = L"ctrl-m"; break;
	case ctrl('n'):		ptr = L"ctrl-n"; break;
	case ctrl('o'):		ptr = L"ctrl-o"; break;
	case ctrl('p'):		ptr = L"ctrl-p"; break;
	case ctrl('q'):		ptr = L"ctrl-q"; break;
	case ctrl('r'):		ptr = L"ctrl-r"; break;
	case ctrl('s'):		ptr = L"ctrl-s"; break;
	case ctrl('t'):		ptr = L"ctrl-t"; break;
	case ctrl('u'):		ptr = L"ctrl-u"; break;
	case ctrl('v'):		ptr = L"ctrl-v"; break;
	case ctrl('w'):		ptr = L"ctrl-w"; break;
	case ctrl('x'):		ptr = L"ctrl-x"; break;
	case ctrl('y'):		ptr = L"ctrl-y"; break;
	case ctrl('z'):		ptr = L"ctrl-z"; break;
	case ' ':		ptr = L"space"; break;
	case 27:		ptr = L"escape"; break;

	default:
		/* this is probably very bad */
		c[0] = (wchar_t) k;
		c[1] = L'\0';
		ptr = c;

		break;
	}

	if(ptr != NULL)
	{
		mp_process_event(MPDM_S(ptr));
		redraw();
	}
}


static void win32_vscroll(UINT wparam)
/* scrollbar messages handler */
{
	wchar_t * ptr = NULL;

	switch(LOWORD(wparam))
	{
	case SB_PAGEUP:		ptr = L"page-up"; break;
	case SB_PAGEDOWN:	ptr = L"page-down"; break;
	case SB_LINEUP:		ptr = L"cursor-up"; break;
	case SB_LINEDOWN:	ptr = L"cursor-down"; break;
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		mp_set_y(mp_active(), HIWORD(wparam));
		redraw();
		break;
	}

	if(ptr != NULL)
	{
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

	if(GetMenuItemInfo(menu, item, FALSE, &mi))
	{
		if(mi.dwItemData != 0)
			mp_process_action((mpdm_t)mi.dwItemData);
        }
}


#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL			0x020A
#endif

long STDCALL WndProc(HWND hwnd, UINT msg, UINT wparam, LONG lparam)
/* main window Proc */
{
	int x, y;
	LPNMHDR p;
	wchar_t * ptr = NULL;

	switch(msg)
	{
	case WM_CREATE:

		is_wm_keydown = 0;
		DragAcceptFiles(hwnd, TRUE);
		return(0);

/*	case WM_DROPFILES:

		(void) load_dropped_files ((HANDLE) wparam, hwnd);
		return(0);
*/
	case WM_KEYUP:

		is_wm_keydown = 0;
		return(0);

	case WM_KEYDOWN:

		win32_vkey(wparam);
		return(0);

	case WM_CHAR:

		win32_akey(wparam);
		return(0);

	case WM_VSCROLL:

		win32_vscroll(wparam);
		return(0);

	case WM_PAINT:
		win32_draw(hwnd, mp_active());
		return(0);

	case WM_SIZE:

		if(!IsIconic(hwnd))
		{
			update_window_size();

			MoveWindow(hwtabs, 0, 0, LOWORD(lparam), tab_height, FALSE);

			MoveWindow(hwstatus, 0, HIWORD(lparam) - status_height,
				LOWORD(lparam), status_height, FALSE);

			redraw();
		}

		return(0);

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:

		x = (LOWORD(lparam)) / font_width;
		y = (HIWORD(lparam) - tab_height) / font_height;

		mpdm_hset_s(mp, L"mouse_x", MPDM_I(x));
		mpdm_hset_s(mp, L"mouse_y", MPDM_I(y));

		switch(msg)
		{
		case WM_LBUTTONDOWN: ptr = L"mouse-left-button"; break;
		case WM_RBUTTONDOWN: ptr = L"mouse-right-button"; break;
		case WM_MBUTTONDOWN: ptr = L"mouse-middle-button"; break;
		}

		if(ptr != NULL)
		{
			mp_process_event(MPDM_S(ptr));
			redraw();
		}

		return(0);

	case WM_MOUSEWHEEL:

		if((int) wparam > 0)
			ptr = L"mouse-wheel-up";
		else
			ptr = L"mouse-wheel-down";

		if(ptr != NULL)
		{
			mp_process_event(MPDM_S(ptr));
			redraw();
		}

		return(0);

	case WM_COMMAND:

		action_by_menu(LOWORD(wparam));
		redraw();

		return(0);

	case WM_CLOSE:

		if(!mp_exit_requested)
			mp_process_event(MPDM_LS(L"close-window"));

		if(mp_exit_requested)
			DestroyWindow(hwnd);

		return(0);

	case WM_DESTROY:
		PostQuitMessage(0);
		return(0);

	case WM_NOTIFY:
		p = (LPNMHDR)lparam;

		if(p->code == TCN_SELCHANGE)
		{
			/* tab selected by clicking on it */
			int n = TabCtrl_GetCurSel(hwtabs);

			/* set mp.active_i to this */
			mpdm_hset_s(mp, L"active_i", MPDM_I(n));

			redraw();
		}

		return(0);
	}

	if(mp_exit_requested)
	{
		PostMessage(hwnd, WM_CLOSE, 0, 0);
		mp_exit_requested = 0;
	}

	return(DefWindowProc(hwnd, msg, wparam, lparam));
}


static mpdm_t w32drv_clip_to_sys(mpdm_t a)
/* driver-dependent mp to system clipboard */
{
	HGLOBAL hclp;
	mpdm_t d;
	char * ptr;
	char * clpptr;
	int s;

	/* convert the clipboard to DOS text */
	d = mpdm_hget_s(mp, L"clipboard");
	d = mpdm_join(MPDM_LS(L"\r\n"), d);
	ptr = mpdm_wcstombs(d->data, &s);

	/* allocates a handle and copies */
	hclp = GlobalAlloc(GHND, s + 1);
	clpptr = (char *)GlobalLock(hclp);
	memcpy(clpptr, ptr, s);
	clpptr[s] = '\0';
	GlobalUnlock(hclp);

	free(ptr);

	OpenClipboard(NULL);
	EmptyClipboard();
	SetClipboardData(CF_TEXT, hclp);
	CloseClipboard();

	return(NULL);
}


static mpdm_t w32drv_sys_to_clip(mpdm_t a)
/* driver-dependent system to mp clipboard */
{
	HGLOBAL hclp;
	char * ptr;

	OpenClipboard(NULL);
	hclp = GetClipboardData(CF_TEXT);
	CloseClipboard();

	if(hclp && (ptr = GlobalLock(hclp)) != NULL)
	{
		mpdm_t d;

		/* create a value and split */
		d = MPDM_MBS(ptr);
		d = mpdm_split(MPDM_LS(L"\r\n"), d);

		/* and set as the clipboard */
		mpdm_hset_s(mp, L"clipboard", d);

		GlobalUnlock(hclp);
	}

	return(NULL);
}


static mpdm_t w32drv_main_loop(mpdm_t a)
{
	MSG msg;

	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return(NULL);
}


static mpdm_t w32drv_shutdown(mpdm_t a)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
	return(NULL);
}


static mpdm_t w32drv_alert(mpdm_t a)
/* alert driver function */
{
	wchar_t * wptr;
	char * ptr;

	/* 1# arg: prompt */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((ptr = mpdm_wcstombs(wptr, NULL)) != NULL)
	{
		MessageBox(hwnd, ptr, "mp " VERSION, MB_ICONWARNING|MB_OK);
		free(ptr);
	}

	return(NULL);
}


static mpdm_t w32drv_confirm(mpdm_t a)
/* confirm driver function */
{
	wchar_t * wptr;
	char * ptr;
	int ret = 0;

	/* 1# arg: prompt */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((ptr = mpdm_wcstombs(wptr, NULL)) != NULL)
	{
		ret = MessageBox(hwnd, ptr, "mp " VERSION, MB_ICONQUESTION|MB_YESNOCANCEL);
		free(ptr);

		if(ret == IDYES) ret = 1;
		else
		if(ret == IDNO) ret = 2;
		else
		ret = 0;
	}

	return(MPDM_I(ret));
}


BOOL CALLBACK readlineDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
/* readline dialog proc */
{
	int ret, n;

	switch(msg)
	{
	case WM_INITDIALOG:

		SetWindowText(hwnd, "mp " VERSION);

		if(dialog_prompt != NULL)
		{
			SetDlgItemText(hwnd, WMP_1STR_LABEL, dialog_prompt);
			free(dialog_prompt);
			dialog_prompt = NULL;
		}

		/* store the history into combo_items */
		SendDlgItemMessage(hwnd, WMP_1STR_EDIT, CB_RESETCONTENT, 0, 0);
		
		for(n = 0;n < mpdm_size(readline_history);n++)
		{
			char * ptr;

			mpdm_t v = mpdm_aget(readline_history, n);

			if((ptr = mpdm_wcstombs(v->data, NULL)) != NULL)
			{
			 	SendDlgItemMessage(hwnd, WMP_1STR_EDIT,
		 			CB_INSERTSTRING, 0, (LPARAM)ptr);
				free(ptr);
			}
		}

		/* set the default value */
		if(readline_default != NULL)
		{
			char * ptr;

			if((ptr = mpdm_wcstombs(readline_default->data, NULL)) != NULL)
			{
				SetDlgItemText(hwnd, WMP_1STR_EDIT, ptr);
				SendDlgItemMessage(hwnd, WMP_1STR_EDIT,
					EM_SETSEL, 0, 1000);

				free(ptr);
			}
		}

		if(entry_is_password)
		{
			SendDlgItemMessage(hwnd, WMP_1STR_EDIT,
				EM_SETPASSWORDCHAR, (WPARAM)'*', (LPARAM)0);

			entry_is_password = 0;
		}

		return(TRUE);

	case WM_COMMAND:

		switch(LOWORD(wparam))
		{
		case WMP_OK:
		case WMP_CANCEL:

			if(LOWORD(wparam) == WMP_OK)
			{
				char tmp[1024];

				mpdm_unref(readline_text);

				GetDlgItemText(hwnd, WMP_1STR_EDIT,
					tmp, sizeof(tmp));

				readline_text = mpdm_ref(MPDM_MBS(tmp));

				ret = 1;
			}
			else
				ret = 0;

			EndDialog(hwnd, ret);

			return(TRUE);
		}
	}

	return(FALSE);
}


static LPWORD lpwAlign(LPWORD lpIn)
{
	ULONG ul;

	ul = (ULONG)lpIn;
	ul ++;
	ul >>= 1;
	ul <<= 1;
	return (LPWORD)ul;
}


/* mp.drv.form() controls */

static mpdm_t form_args = NULL;
static mpdm_t form_values = NULL;

BOOL CALLBACK formDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
/* mp.drv.form() dialog proc */
{
	int n;
	HFONT hf;

	switch(msg)
	{
	case WM_INITDIALOG:

		SetWindowText(hwnd, "mp " VERSION);

		hf = GetStockObject(DEFAULT_GUI_FONT);

		for(n = 0;n < mpdm_size(form_args);n++)
		{
			mpdm_t w = mpdm_aget(form_args, n);
			wchar_t * type;
			mpdm_t t;
			int ctrl = 101 + n * 2;

			if((t = mpdm_hget_s(w, L"label")) != NULL)
				SetDlgItemTextW(hwnd, 100 + n * 2, mpdm_string(t));

			SendDlgItemMessage(hwnd, 100 + n * 2, WM_SETFONT,
				(WPARAM) hf, MAKELPARAM(FALSE, 0));
			SendDlgItemMessage(hwnd, ctrl, WM_SETFONT,
				(WPARAM) hf, MAKELPARAM(FALSE, 0));

			type = mpdm_string(mpdm_hget_s(w, L"type"));

			if(wcscmp(type, L"text") == 0)
			{
				if((t = mpdm_hget_s(w, L"value")) != NULL)
					SetDlgItemTextW(hwnd, ctrl,
						mpdm_string(t));

				/* store the history into combo_items */
				if((t = mpdm_hget_s(w, L"history")) != NULL)
				{
					t = mp_get_history(t);
					int i;

					for(i = 0;i < mpdm_size(t);i++)
					{
						mpdm_t v = mpdm_aget(t, i);
						char * ptr;

						if((ptr = mpdm_wcstombs(v->data,
							NULL)) != NULL)
						{
			 				SendDlgItemMessage(hwnd,
								ctrl,
								CB_INSERTSTRING, 0,
								(LPARAM)ptr);
							free(ptr);
						}
					}
				}
			}
			else
			if(wcscmp(type, L"password") == 0)
			{
				SendDlgItemMessage(hwnd, ctrl,
				EM_SETPASSWORDCHAR, (WPARAM)'*', (LPARAM)0);
			}
			else
			if(wcscmp(type, L"checkbox") == 0)
			{
				if((t = mpdm_hget_s(w, L"value")) != NULL)
					SendDlgItemMessage(hwnd, ctrl,
					BM_SETCHECK, mpdm_ival(t) ?
						BST_CHECKED : BST_UNCHECKED,
					0);
			}
			else
			if(wcscmp(type, L"list") == 0)
			{
				int i;

				t = mpdm_hget_s(w, L"list");

				/* fill the list */
				for(i = 0;i < mpdm_size(t);i++)
				{
					wchar_t * wptr;
					char * ptr;

					wptr = mpdm_string(mpdm_aget(t, i));
					if((ptr = mpdm_wcstombs(wptr, NULL)) != NULL)
					{
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


		SetDlgItemTextW(hwnd, IDOK, L"OK");
		SendDlgItemMessage(hwnd, IDOK, WM_SETFONT,
			(WPARAM) hf, MAKELPARAM(FALSE, 0));

		SetDlgItemTextW(hwnd, IDCANCEL, L"Cancel");
		SendDlgItemMessage(hwnd, IDCANCEL, WM_SETFONT,
			(WPARAM) hf, MAKELPARAM(FALSE, 0));

		return(TRUE);

	case WM_COMMAND:

		if(LOWORD(wparam) == IDCANCEL)
		{
			EndDialog(hwnd, 0);
			return(TRUE);
		}

		if(LOWORD(wparam) != IDOK)
			break;

		/* fill all values in form_values */
		for(n = 0;n < mpdm_size(form_args);n++)
		{
			mpdm_t w = mpdm_aget(form_args, n);
			wchar_t * type = mpdm_string(mpdm_hget_s(w, L"type"));
			int ctrl = 101 + n * 2;

			if(wcscmp(type, L"text") == 0)
			{
				wchar_t tmp[2048];
				mpdm_t v;
				mpdm_t h;

				GetDlgItemTextW(hwnd, ctrl, tmp, sizeof(tmp) - 1);
				v = MPDM_S(tmp);

				mpdm_aset(form_values, v, n);

				/* if it has history, fill it */
				if((h = mpdm_hget_s(w, L"history")) != NULL &&
					v != NULL && mpdm_cmp(v, MPDM_LS(L"")) != 0)
				{
					h = mp_get_history(h);

					if(mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
						mpdm_push(h, v);
				}
			}
			if(wcscmp(type, L"password") == 0)
			{
				wchar_t tmp[2048];

				GetDlgItemTextW(hwnd, ctrl, tmp, sizeof(tmp) - 1);
				mpdm_aset(form_values, MPDM_S(tmp), n);
			}
			else
			if(wcscmp(type, L"checkbox") == 0)
			{
				mpdm_aset(form_values,
					MPDM_I(SendDlgItemMessage(hwnd, ctrl,
						BM_GETCHECK, 0, 0)), n);
			}
			else
			if(wcscmp(type, L"list") == 0)
			{
				mpdm_aset(form_values,
					MPDM_I(SendDlgItemMessage(hwnd, ctrl,
						LB_GETCURSEL, 0, 0)), n);
			}
		}

		EndDialog(hwnd, 1);
		return(TRUE);
	}

	return(FALSE);
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
	int cx, int cy,	int id, int class, int style)
{
	LPDLGITEMTEMPLATE lpdit;

	lpw = lpwAlign(lpw);
	lpdit = (LPDLGITEMTEMPLATE)lpw;
	lpdit->x  = x; lpdit->y  = y;
	lpdit->cx = cx; lpdit->cy = cy;
	lpdit->id = id;
	lpdit->style = style;

	lpw = (LPWORD)(lpdit + 1);
	*lpw++ = 0xFFFF;
	*lpw++ = class;

	/* no text (will be set on dialog setup) */
	*lpw++ = 0;
	*lpw++ = 0;

	/* Align creation data on DWORD boundary */
	lpw = lpwAlign(lpw);
	/* No creation data */
	*lpw++ = 0;

	return(lpw);
}


static mpdm_t w32drv_form(mpdm_t a)
/* mp.drv.form() function */
{
	HGLOBAL hgbl;
	LPDLGTEMPLATE lpdt;
	LPWORD lpw;
	int n, p;
	int il = 12;

	/* first argument: list of widgets */
	build_form_data(mpdm_aget(a, 0));

	/* On-the-fly dialog template creation */
	/* Note: all this crap is taken from MSDN, no less */

	/* magic size; looking for problems */
	hgbl = GlobalAlloc(GMEM_ZEROINIT, 4096);
	lpdt = (LPDLGTEMPLATE)GlobalLock(hgbl);

	lpdt->style = WS_POPUP | WS_BORDER | WS_SYSMENU | DS_MODALFRAME | WS_CAPTION;
	lpdt->cdit = (2 * mpdm_size(form_args)) + 2;
	lpdt->x  = 20;  lpdt->y  = 20;
	lpdt->cx = 260;

	lpw = (LPWORD)(lpdt + 1);
	*lpw++ = 0;	/* No menu */
	*lpw++ = 0;	/* Predefined dialog box class (by default) */
	*lpw++ = 0;	/* No title */

	for(n = p = 0;n < mpdm_size(form_args);n++)
	{
		mpdm_t w = mpdm_aget(form_args, n);
		wchar_t * type;
		int class;
		int style;
		int inc = 1;

		/* label */
		lpw = build_control(lpw, 10, 5 + p * il,
			50, 20, 100 + (n * 2), 0x0082,
			WS_CHILD | WS_VISIBLE | SS_RIGHT);

		type = mpdm_string(mpdm_hget_s(w, L"type"));

		if(wcscmp(type, L"text") == 0)
		{
			class = 0x0085;
			style = WS_CHILD | WS_VISIBLE | WS_TABSTOP |
				CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL;
		}
		else
		if(wcscmp(type, L"password") == 0)
		{
			class = 0x0081;
			style = WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP;
		}
		else
		if(wcscmp(type, L"checkbox") == 0)
		{
			class = 0x0080;
			style = WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP;
		}
		else
		if(wcscmp(type, L"list") == 0)
		{
			class = 0x0083;
			style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
				LBS_NOINTEGRALHEIGHT | WS_VSCROLL |
				LBS_NOTIFY | LBS_USETABSTOPS;

			/* height */
			inc = 5;
		}

		lpw = build_control(lpw, 65, 5 + p * il,
			190, (inc * il) - 5, 101 + (n * 2), class, style);

		/* next position */
		p += inc;
	}

	/* set total height */
	lpdt->cy = 30 + p * il;

	/* OK */
	lpw = build_control(lpw, 170, 10 + p * il, 40, 15, IDOK,
			0x0080, WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP);

	/* Cancel */
	lpw = build_control(lpw, 215, 10 + p * il, 40, 15, IDCANCEL,
			0x0080, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP);

	GlobalUnlock(hgbl);
	n = DialogBoxIndirect(hinst, (LPDLGTEMPLATE)hgbl,
		hwnd, (DLGPROC)formDlgProc);

	GlobalFree(hgbl);

	return(n ? form_values : NULL);
}


static mpdm_t w32drv_readline(mpdm_t a)
/* readline driver function */
{
	wchar_t * wptr;

	/* 1# arg: prompt */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((dialog_prompt = mpdm_wcstombs(wptr, NULL)) != NULL)
	{
		/* 2# arg: history key */
		readline_history = mp_get_history(mpdm_aget(a, 1));

		/* 3# arg: default value */
		readline_default = mpdm_aget(a, 2);

		if(DialogBox(hinst,
			entry_is_password ? "READLINE_PASSWORD" : "READLINE",
			hwnd, readlineDlgProc))
		{
			if(readline_history != NULL &&
				mpdm_size(readline_text) > 0 &&
				mpdm_cmp(readline_text,
					mpdm_aget(readline_history, -1)) != 0)
				mpdm_push(readline_history, readline_text);

			return(readline_text);
		}
	}

	return(NULL);
}


static mpdm_t w32drv_readline_search(mpdm_t a)
/* readline_search driver function */
{
	return(w32drv_readline(a));
}


static mpdm_t w32drv_readline_password(mpdm_t a)
/* readline_password driver function */
{
	entry_is_password = 1;

	return(w32drv_readline(a));
}


static mpdm_t w32drv_readline_replace(mpdm_t a)
/* readline_replace driver function */
{
	mpdm_t r = NULL;
	mpdm_t r1, r2;

	r1 = MPDM_A(2);
	r2 = MPDM_A(3);

	/* r1 = [ this_p, this_d ]; */
	mpdm_aset(r1, mpdm_aget(a, 0), 0);
	mpdm_aset(r1, mpdm_aget(a, 1), 1);

	/* r2 = [ that_p, 'replace', that_d ]; */
	mpdm_aset(r2, mpdm_aget(a, 2), 0);
	mpdm_aset(r2, MPDM_LS(L"replace"), 1);
	mpdm_aset(r2, mpdm_aget(a, 3), 2);

	if((r1 = w32drv_readline_search(r1)) != NULL &&
	   (r2 = w32drv_readline(r2)) != NULL)
	{
		r = MPDM_A(2);
		mpdm_aset(r, r1, 0);
		mpdm_aset(r, r2, 1);
	}

	return(r);
}


static mpdm_t open_or_save(int o, mpdm_t a)
/* manages an open or save file dialog */
{
	OPENFILENAME ofn;
	wchar_t * wptr;
	char * ptr;
	char buf[1024] = "";
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
/*	ofn.lpstrDefExt=(def==NULL ? "" : def);*/

	if(o)
	{
		ofn.Flags = OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|
			OFN_NOCHANGEDIR|OFN_FILEMUSTEXIST;

		r = GetOpenFileName(&ofn);
	}
	else
	{
		ofn.Flags = OFN_HIDEREADONLY;

		r = GetSaveFileName(&ofn);
	}

	free(ptr);

	if(r)
		return(MPDM_MBS(buf));

	return(NULL);
}


static mpdm_t w32drv_openfile(mpdm_t a)
/* openfile driver function */
{
	return(open_or_save(1, a));
}


static mpdm_t w32drv_savefile(mpdm_t a)
/* savefile driver function */
{
	return(open_or_save(0, a));
}


BOOL CALLBACK listDlgProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
/* list dialog proc */
{
	int ret;
	HWND lst;
	int n;

	switch(msg)
	{
	case WM_INITDIALOG:

		if(dialog_prompt != NULL)
		{
			SetWindowText(hwnd, dialog_prompt);
			free(dialog_prompt);
			dialog_prompt = NULL;
		}
		else
			SetWindowText(hwnd, "mp " VERSION);

		lst = GetDlgItem(hwnd, WMP_LIST);
		SendMessage(lst, WM_SETFONT, 
			(WPARAM) GetStockObject(ANSI_FIXED_FONT), 0);

		{
			int n = 20 * 4;

			/* sets tab stops on each 20 quarters of char */
			SendMessage(lst, LB_SETTABSTOPS,
				(WPARAM) 1, (LPARAM) &n);
		}

		/* traverses the list, filling the listbox */
		for(n = 0;n < mpdm_size(list_data);n++)
		{
			wchar_t * wptr;
			char * ptr;

			wptr = mpdm_string(mpdm_aget(list_data, n));
			if((ptr = mpdm_wcstombs(wptr, NULL)) != NULL)
			{
				SendMessage(lst, LB_ADDSTRING, 0, (LPARAM) ptr);
				free(ptr);
			}
		}

		/* sets the desired element as default */
		SendMessage(lst, LB_SETCURSEL, list_pos, 0);

		return(TRUE);

	case WM_COMMAND:

		switch(LOWORD(wparam))
		{
		case WMP_OK:
		case WMP_CANCEL:

			if(LOWORD(wparam) == WMP_OK)
				ret = SendDlgItemMessage(hwnd, WMP_LIST,
					LB_GETCURSEL, 0, 0);
			else
				ret = -1;

			EndDialog(hwnd, ret);

			return(TRUE);
		}
	}

	return(FALSE);
}


static mpdm_t w32drv_list(mpdm_t a)
/* readline list function */
{
	wchar_t * wptr;
	int ret = -1;

	/* 1# arg: prompt */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((dialog_prompt = mpdm_wcstombs(wptr, NULL)) != NULL)
	{
		/* 2# arg: list */
		list_data = mpdm_aget(a, 1);

		/* 3# arg: position */
		list_pos = mpdm_ival(mpdm_aget(a, 2));

		ret = DialogBox(hinst, "LIST", hwnd, listDlgProc);
	}

	return(ret == -1 ? NULL : MPDM_I(ret));
}


static mpdm_t w32drv_update_ui(mpdm_t a)
{
	build_fonts(GetDC(hwnd));
	build_colors();
	build_menu();

	return(NULL);
}


static void register_functions(void)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"main_loop", MPDM_X(w32drv_main_loop));
	mpdm_hset_s(drv, L"shutdown", MPDM_X(w32drv_shutdown));

	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(w32drv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(w32drv_sys_to_clip));
	mpdm_hset_s(drv, L"update_ui", MPDM_X(w32drv_update_ui));

	mpdm_hset_s(drv, L"alert", MPDM_X(w32drv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(w32drv_confirm));
	mpdm_hset_s(drv, L"readline", MPDM_X(w32drv_readline));
	mpdm_hset_s(drv, L"readline_search", MPDM_X(w32drv_readline_search));
	mpdm_hset_s(drv, L"readline_replace", MPDM_X(w32drv_readline_replace));
	mpdm_hset_s(drv, L"readline_password", MPDM_X(w32drv_readline_password));
	mpdm_hset_s(drv, L"openfile", MPDM_X(w32drv_openfile));
	mpdm_hset_s(drv, L"savefile", MPDM_X(w32drv_savefile));
	mpdm_hset_s(drv, L"list", MPDM_X(w32drv_list));
	mpdm_hset_s(drv, L"form", MPDM_X(w32drv_form));
}


static mpdm_t w32drv_startup(mpdm_t a)
{
	WNDCLASS wc;
	RECT r;

	register_functions();

	InitCommonControls();

	/* register the window */
	wc.style = CS_HREDRAW|CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hinst;
	wc.hIcon = LoadIcon(hinst,"MP_ICON");
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "minimumprofit5.x";

	RegisterClass(&wc);

	/* create the window */
	hwnd = CreateWindow("minimumprofit5.x", "mp " VERSION,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hinst, NULL);

/*	mpv_add_menu("");

	DrawMenuBar(hwnd);
*/
	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	GetClientRect(hwnd, &r);

	hwtabs = CreateWindow(WC_TABCONTROL, "tab",
		WS_CHILD | TCS_TABS | TCS_SINGLELINE | TCS_FOCUSNEVER,
		0, 0, r.right - r.left, tab_height, hwnd, NULL, hinst, NULL);

/*	SendMessage(hwtabs, WM_SETFONT, 
		(WPARAM) GetStockObject(ANSI_VAR_FONT), 0);
*/
	ShowWindow(hwtabs, SW_SHOW);
	UpdateWindow(hwtabs);

	hwstatus = CreateWindow(WC_STATIC, "status",
		WS_CHILD,
		0, r.bottom - r.top - status_height,
		r.right - r.left, status_height, hwnd, NULL, hinst, NULL);

	build_menu();

	ShowWindow(hwstatus, SW_SHOW);
	UpdateWindow(hwstatus);

	redraw();

	return(NULL);
}


int w32drv_init(void)
{
	mpdm_t drv;

	drv = mpdm_hget_s(mp, L"drv");
	mpdm_hset_s(drv, L"id", MPDM_LS(L"win32"));
	mpdm_hset_s(drv, L"startup", MPDM_X(w32drv_startup));

	return(1);
}

#else /* CONFOPT_WIN32 */

int w32drv_init(void)
{
	/* no Win32 */
	return(0);
}

#endif /* CONFOPT_WIN32 */
