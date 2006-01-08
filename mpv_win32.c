/*

    Minimum Profit - Programmer Text Editor

    Win32 driver.

    Copyright (C) 1991-2005 Angel Ortega <angel@triptico.com>

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

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/*******************
	Data
********************/

/* the driver */
mpdm_t win32_driver = NULL;
mpdm_t win32_window = NULL;

/* the instance */
HINSTANCE hinst;

/* the windows */
HWND hwnd = NULL;
HWND hwtabs = NULL;

/* font handlers and metrics */
HFONT font_normal = NULL;
int font_width = 0;
int font_height = 0;

int tab_height = 28;

int is_wm_keydown = 0;

/* colors */
static COLORREF inks[MP_ATTR_SIZE];
static COLORREF papers[MP_ATTR_SIZE];
HBRUSH bgbrush;

/*******************
	Code
********************/

static void update_window_size(void)
/* updates the viewport size in characters */
{
	RECT rect;
	int tx, ty;

	/* no font information? go */
	if(font_width == 0 || font_height == 0)
		return;

	GetClientRect(hwnd, &rect);

	/* calculate the size in chars */
	tx = ((rect.right - rect.left) / font_width) + 1;
	ty = ((rect.bottom - rect.top - tab_height) / font_height) + 1;

	/* store the 'window' size */
	mpdm_hset_s(win32_window, L"tx", MPDM_I(tx));
	mpdm_hset_s(win32_window, L"ty", MPDM_I(ty));
}


int font_size = 14;
char * font_face = "Lucida Console";

static void build_fonts(HDC hdc)
/* build the fonts */
{
	TEXTMETRIC tm;
	int n;

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
	mpdm_t attr_names;
	mpdm_t l;
	mpdm_t c;
	int n;

	/* gets the color definitions and attribute names */
	colors = mpdm_hget_s(mp, L"colors");
	attr_names = mpdm_hget_s(mp, L"attr_names");
	l = mpdm_keys(colors);

	/* loop the colors */
	for(n = 0;(c = mpdm_aget(l, n)) != NULL;n++)
	{
		int attr = mpdm_ival(mpdm_hget(attr_names, c));
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"gui");
		int m;

		m = mpdm_ival(mpdm_aget(v, 0));
		inks[attr] = ((m & 0x000000ff) << 16)|
			 ((m & 0x0000ff00)) |
			 ((m & 0x00ff0000) >> 16);
		m = mpdm_ival(mpdm_aget(v, 1));
		papers[attr] = ((m & 0x000000ff) << 16)|
			 ((m & 0x0000ff00)) |
			 ((m & 0x00ff0000) >> 16);

		/* flags */
		v = mpdm_hget_s(d, L"flags");
/*		underlines[attr] = mpdm_seek_s(v, L"underline", 1) != -1 ? 1 : 0;
*/
		if(mpdm_seek_s(v, L"reverse", 1) != -1)
		{
			COLORREF t;

			t = inks[attr];
			inks[attr] = papers[attr];
			papers[attr] = t;
		}
	}

	/* create the background brush */
	bgbrush = CreateSolidBrush(papers[MP_ATTR_NORMAL]);
}


static void draw_filetabs(void)
/* draws the filetabs */
{
	int n, a;
	static int last = -1;
	mpdm_t docs;

	/* gets the document list */
	if(hwtabs == NULL || (docs = mpdm_hget_s(mp, L"docs")) == NULL)
		return;

	/* gets the active document number */
	a = mpdm_ival(mpdm_hget_s(mp, L"active"));

	if(last != mpdm_size(docs))
	{
		/* if size is different that the last, rebuild the tabset */
		last = mpdm_size(docs);

		TabCtrl_DeleteAllItems(hwtabs);

		for(n = 0;n < mpdm_size(docs);n++)
		{
			TCITEM ti;
			char * ptr;
			wchar_t * wptr;
			mpdm_t v = mpdm_aget(docs, n);

			/* just get the name */
			v = mpdm_hget_s(v, L"name");

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
	}

	/* set the active one */
	TabCtrl_SetCurSel(hwtabs, a);
}


mpdm_t mpi_draw(mpdm_t v);

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

	/* no document? end */
	if((d = mpi_draw(doc)) == NULL)
	{
		EndPaint(hwnd, &ps);
		return;
	}

	/* no font? construct it */
	if(font_normal == NULL)
	{
		build_fonts(hdc);
		build_colors();
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
	}

	redraw();
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

	default:
		/* this is probably very bad */
		c[0] = (wchar_t) k;
		c[1] = L'\0';
		ptr = c;

		break;
	}

	if(ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	redraw();
}


long STDCALL WndProc(HWND hwnd, UINT msg, UINT wparam, LONG lparam)
/* main window Proc */
{
	int x, y;
	LPNMHDR p;
	char * ptr = NULL;

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

/*	case WM_VSCROLL:

		switch(LOWORD(wparam))
		{
		case SB_PAGEUP: ptr="move-page-up"; break;
		case SB_PAGEDOWN: ptr="move-page-down"; break;
		case SB_LINEUP: ptr="move-up"; break;
		case SB_LINEDOWN: ptr="move-down"; break;
 		case SB_THUMBPOSITION:
 		case SB_THUMBTRACK:
 		  sb_thumbscroll(LOWORD(wparam),HIWORD(wparam));
 		  return(0);
		}

		if(ptr != NULL)
			mpi_process('\0', NULL, ptr);

		return(0);
*/
	case WM_PAINT:
		win32_draw(hwnd, mp_get_active());
		return(0);

	case WM_SIZE:

		if(!IsIconic(hwnd))
		{
			update_window_size();
			redraw();

			MoveWindow(hwtabs, 0, 0, LOWORD(lparam), tab_height, TRUE);
		}

		return(0);

/*	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:

		x=LOWORD(lparam);
		y=HIWORD(lparam) - _tab_height;

		x/=_mpv_font_width;
		y/=_mpv_font_height;

		mp_move_xy(_mp_active,x,y+_mp_active->vy);
		mp_move_bol(_mp_active);
		mp_move_to_visual_column(_mp_active,x);

		mpi_draw_all(_mp_active);

		switch(msg)
		{
		case WM_LBUTTONDOWN: ptr="mouse-left-button"; break;
		case WM_RBUTTONDOWN: ptr="mouse-right-button"; break;
		case WM_MBUTTONDOWN: ptr="mouse-middle-button"; break;
		}

		if(ptr != NULL)
			mpi_process('\0', ptr, NULL);

		return(0);
*/
/*	case WM_MOUSEWHEEL:

		if((int)wparam > 0)
			ptr="mouse-wheel-up";
		else
			ptr="mouse-wheel-down";

		if(ptr != NULL)
			mpi_process('\0', ptr, NULL);

		return(0);
*/
/*	case WM_COMMAND:

		_mpv_amenu(LOWORD(wparam));

		return(0);
*/
	case WM_CLOSE:

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

			/* set mp.active to this */
			mpdm_hset_s(mp, L"active", MPDM_I(n));

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


static mpdm_t win32_drv_clip_to_sys(mpdm_t a)
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


static mpdm_t win32_drv_sys_to_clip(mpdm_t a)
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


static void win32_drv_startup(void)
{
	WNDCLASS wc;
	RECT r;

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

	if(!RegisterClass(&wc))
		return(NULL);

	/* create the window */
	hwnd = CreateWindow("minimumprofit5.x", "mp " VERSION,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
/*		NULL, _mmenu, hinst, NULL);*/
		NULL, NULL, hinst, NULL);

	if(hwnd == NULL)
		return(NULL);

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

	redraw();
}


static void win32_drv_main_loop(void)
{
	MSG msg;

	while(GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}


static void win32_drv_shutdown(void)
{
	SendMessage(hwnd, WM_CLOSE, 0, 0);
}


static mpdm_t win32_drv_ui(mpdm_t a)
{
	win32_drv_startup();
	win32_drv_main_loop();
	win32_drv_shutdown();

	return(NULL);
}


int win32_drv_init(void)
{
	win32_driver = mpdm_ref(MPDM_H(0));

	mpdm_hset_s(win32_driver, L"driver", MPDM_LS(L"win32"));
	mpdm_hset_s(win32_driver, L"ui", MPDM_X(win32_drv_ui));
	mpdm_hset_s(gtk_driver, L"clip_to_sys", MPDM_X(win32_drv_clip_to_sys));
	mpdm_hset_s(gtk_driver, L"sys_to_clip", MPDM_X(win32_drv_sys_to_clip));

	win32_window = MPDM_H(0);
	mpdm_hset_s(win32_driver, L"window", win32_window);

	mpdm_hset_s(mp, L"drv", win32_driver);

	return(1);
}

#else /* CONFOPT_WIN32 */

int win32_drv_init(void)
{
	/* no Win32 */
	return(0);
}

#endif /* CONFOPT_WIN32 */