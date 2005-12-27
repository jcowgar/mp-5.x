/*

    Minimum Profit - Programmer Text Editor

    GTK driver.

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

#ifdef CONFOPT_GTK

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

/*******************
	Data
********************/

/* the driver */
mpdm_t gtk_driver = NULL;

/* global data */
GtkWidget * window = NULL;
GtkWidget * area = NULL;
GtkWidget * scrollbar = NULL;
GdkGC * gc = NULL;
GtkIMContext * im = NULL;
/*GtkWidget * menu = NULL;
GtkWidget * menu_bar = NULL;
GtkWidget * file_tabs = NULL;
GtkWidget * entry = NULL;
GtkWidget * list = NULL;
GtkWidget * status = NULL;
GdkPixmap * pixmap = NULL;*/

/*******************
	Code
********************/

static gint delete_event(GtkWidget * w, GdkEvent * e, gpointer data)
/* 'delete_event' handler */
{
	mp_exit_requested = 1;
	return(FALSE);
}


static void destroy(GtkWidget * w, gpointer data)
/* 'destroy' handler */
{
	gtk_main_quit();
}


static gint key_press_event(GtkWidget * widget, GdkEventKey * event, gpointer data)
/* 'key_press_event' handler */
{
	wchar_t * ptr = NULL;
	int c = '\0';

	gtk_im_context_filter_keypress(im, event);

	if(event->keyval < 10000)
		c = event->keyval;

/*	mpi_move_selecting=event->state & GDK_SHIFT_MASK;
*/
	/* reserve alt for menu mnemonics */
	if (GDK_MOD1_MASK & event->state)
		return(0);

	if(event->state & (GDK_CONTROL_MASK))
	{
		switch(event->keyval)
		{
		case GDK_Up:	ptr = L"ctrl-cursor-up"; break;
		case GDK_Down:	ptr = L"ctrl-cursor-down"; break;
		case GDK_Left:	ptr = L"ctrl-cursor-left"; break;
		case GDK_Right: ptr = L"ctrl-cursor-right"; break;
		case GDK_Prior: ptr = L"ctrl-page-up"; break;
		case GDK_Next:	ptr = L"ctrl-page-down"; break;
		case GDK_Home:	ptr = L"ctrl-home"; break;
		case GDK_End:	ptr = L"ctrl-end"; break;
		case GDK_space:	ptr = L"ctrl-space"; break;

		case GDK_KP_Add: ptr = L"ctrl-kp-plus"; break;
		case GDK_KP_Subtract: ptr = L"ctrl-kp-minus"; break;
		case GDK_KP_Multiply: ptr = L"ctrl-kp-multiply"; break;
		case GDK_KP_Divide: ptr = L"ctrl-kp-divide"; break;

		case GDK_F1:	ptr = L"ctrl-f1"; break;
		case GDK_F2:	ptr = L"ctrl-f2"; break;
		case GDK_F3:	ptr = L"ctrl-f3"; break;
		case GDK_F4:	ptr = L"ctrl-f4"; break;
		case GDK_F5:	ptr = L"ctrl-f5"; break;
		case GDK_F6:	ptr = L"ctrl-f6"; break;
		case GDK_F7:	ptr = L"ctrl-f7"; break;
		case GDK_F8:	ptr = L"ctrl-f8"; break;
		case GDK_F9:	ptr = L"ctrl-f9"; break;
		case GDK_F10:	ptr = L"ctrl-f10"; break;
		case GDK_F11:	ptr = L"ctrl-f11"; break;
		case GDK_F12:	ptr = L"ctrl-f12"; break;
		case GDK_KP_Enter:
		case GDK_Return: ptr = L"ctrl-enter"; break;

		case 'a':	ptr = L"ctrl-a"; break;
		case 'b':	ptr = L"ctrl-b"; break;
		case 'c':	ptr = L"ctrl-c"; break;
		case 'd':	ptr = L"ctrl-d"; break;
		case 'e':	ptr = L"ctrl-e"; break;
		case 'f':	ptr = L"ctrl-f"; break;
		case 'g':	ptr = L"ctrl-g"; break;
		case 'h':	ptr = L"ctrl-h"; break;
		case 'i':	ptr = L"ctrl-i"; break;
		case 'j':	ptr = L"ctrl-j"; break;
		case 'k':	ptr = L"ctrl-k"; break;
		case 'l':	ptr = L"ctrl-l"; break;
		case 'm':	ptr = L"ctrl-m"; break;
		case 'n':	ptr = L"ctrl-n"; break;
		case 'o':	ptr = L"ctrl-o"; break;
		case 'p':	ptr = L"ctrl-p"; break;
		case 'q':	ptr = L"ctrl-q"; break;
		case 'r':	ptr = L"ctrl-r"; break;
		case 's':	ptr = L"ctrl-s"; break;
		case 't':	ptr = L"ctrl-t"; break;
		case 'u':	ptr = L"ctrl-u"; break;
		case 'v':	ptr = L"ctrl-v"; break;
		case 'w':	ptr = L"ctrl-w"; break;
		case 'x':	ptr = L"ctrl-x"; break;
		case 'y':	ptr = L"ctrl-y"; break;
		case 'z':	ptr = L"ctrl-z"; break;
		}
	}
	else
	if(event->keyval > 256)
	{
		switch(event->keyval)
		{
		case GDK_Up:	ptr = L"cursor-up"; break;
		case GDK_Down:	ptr = L"cursor-down"; break;
		case GDK_Left:	ptr = L"cursor-left"; break;
		case GDK_Right: ptr = L"cursor-right"; break;
		case GDK_Prior: ptr = L"page-up"; break;
		case GDK_Next:	ptr = L"page-down"; break;
		case GDK_Home:	ptr = L"home"; break;
		case GDK_End:	ptr = L"end"; break;

		case GDK_KP_Add: ptr = L"kp-plus"; break;
		case GDK_KP_Subtract: ptr = L"kp-minus"; break;
		case GDK_KP_Multiply: ptr = L"kp-multiply"; break;
		case GDK_KP_Divide: ptr = L"kp-divide"; break;

		case GDK_F1:	ptr = L"f1"; break;
		case GDK_F2:	ptr = L"f2"; break;
		case GDK_F3:	ptr = L"f3"; break;
		case GDK_F4:	ptr = L"f4"; break;
		case GDK_F5:	ptr = L"f5"; break;
		case GDK_F6:	ptr = L"f6"; break;
		case GDK_F7:	ptr = L"f7"; break;
		case GDK_F8:	ptr = L"f8"; break;
		case GDK_F9:	ptr = L"f9"; break;
		case GDK_F10:	ptr = L"f10"; break;
		case GDK_F11:	ptr = L"f11"; break;
		case GDK_F12:	ptr = L"f12"; break;

		case GDK_Insert: ptr = L"insert"; break;
		case GDK_BackSpace: ptr = L"backspace"; break;
		case GDK_Delete: ptr = L"delete"; break;
		case GDK_KP_Enter:
		case GDK_Return: ptr = L"enter"; break;
		case GDK_Tab:	 ptr = L"tab"; break;
		case GDK_Escape: ptr = L"escape"; break;
		}
	}
	else
	{
		if(c < 32 || c > 255 || c==127)
			c = '\0';
	}
/*
	if(_mpv_im_char != '\0')
	{
		c=_mpv_im_char;
		_mpv_im_char='\0';
	}
*/
/*	if(c!='\0' || ptr!=NULL)
	{*/
		/* tell next expose we've call it */
/*		_mpv_expose_by_key=1;

		mpi_process(c, ptr, NULL);

		_mpv_expose_by_key=0;
	}
*/
	if(ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	if(mp_exit_requested)
		gtk_main_quit();

	return(0);
}


static mpdm_t gtk_drv_startup(mpdm_t a)
{
	GtkWidget * vbox;
	GtkWidget * hbox;
	GdkPixmap * pixmap;
	GdkPixmap * mask;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		GTK_SIGNAL_FUNC(delete_event), NULL);

	gtk_signal_connect(GTK_OBJECT(window), "destroy",
		GTK_SIGNAL_FUNC(destroy), NULL);

	/* file tabs */
/*	file_tabs=gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(file_tabs), GTK_POS_TOP);
	GTK_WIDGET_UNSET_FLAGS(file_tabs,GTK_CAN_FOCUS);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(file_tabs),1);
*/
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(window), vbox);
/*
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), file_tabs, FALSE, FALSE, 0);
*/
	/* horizontal box holding the text and the scrollbar */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	/* the Minimum Profit area */
	area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(hbox), area, TRUE, TRUE, 0);
	gtk_widget_set_usize(GTK_WIDGET(area), 600, 400);
	gtk_widget_set_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK);

	gtk_widget_set_double_buffered(area, FALSE);

/*	gtk_signal_connect(GTK_OBJECT(area),"configure_event",
		(GtkSignalFunc) _mpv_configure_event_callback, NULL);

	gtk_signal_connect(GTK_OBJECT(area),"expose_event",
		(GtkSignalFunc) _mpv_expose_event_callback, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "realize",
		(GtkSignalFunc) _mpv_realize_callback, NULL);
*/
	gtk_signal_connect(GTK_OBJECT(window),"key_press_event",
		(GtkSignalFunc) key_press_event, NULL);
/*
	gtk_signal_connect(GTK_OBJECT(area),"button_press_event",
		(GtkSignalFunc) _mpv_mouse_callback, NULL);

	gtk_signal_connect(GTK_OBJECT(file_tabs),"switch_page",
		(GtkSignalFunc) _mpv_filetabs_callback, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_clear_event",
		GTK_SIGNAL_FUNC(_mpv_selection_clear_callback), NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_get",
		GTK_SIGNAL_FUNC(_mpv_selection_get_callback), NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_received",
		GTK_SIGNAL_FUNC(_mpv_selection_received_callback), NULL);

#if GTK_MAJOR_VERSION >= 2

	gtk_signal_connect(GTK_OBJECT(area), "scroll_event",
		GTK_SIGNAL_FUNC(_mpv_scroll_callback), NULL);

#endif

	gtk_selection_add_target(area, GDK_SELECTION_PRIMARY,
		GDK_SELECTION_TYPE_STRING, 1);
*/
	/* the scrollbar */
	scrollbar = gtk_vscrollbar_new(NULL);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

/*	gtk_signal_connect(
		GTK_OBJECT(gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
		"value_changed",
		(GtkSignalFunc) _mpv_value_changed_callback, NULL);
*/
	/* the status bar */
/*	status = gtk_label_new("mp " VERSION);
	gtk_box_pack_start(GTK_BOX(vbox), status, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(status), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(status), GTK_JUSTIFY_LEFT);
*/
	gtk_widget_show_all(window);

	gc = gdk_gc_new(area->window);

/*	_mpv_font_size--;
	mpv_zoom(1);

	if (mpv_gtk_maximize)
		gtk_window_maximize(GTK_WINDOW(window));
*/

	/* set size */
/*	if(!mpv_gtk_maximize)
	{
		if(_mpv_gtk_xpos >= 0 && _mpv_gtk_ypos >= 0)
			gdk_window_move(GTK_WIDGET(window)->window,
				_mpv_gtk_xpos, _mpv_gtk_ypos);

		if(_mpv_gtk_width > 0 && _mpv_gtk_height > 0)
		{
			if(_mpv_gtk_width < 150) _mpv_gtk_width=150;
			if(_mpv_gtk_height < 150) _mpv_gtk_height=150;

			gtk_window_set_default_size(GTK_WINDOW(window),
				_mpv_gtk_width, _mpv_gtk_height);

			gdk_window_resize(GTK_WIDGET(window)->window,
				_mpv_gtk_width, _mpv_gtk_height);
		}
	}
*/
	/* colors */
/*	_mpv_create_colors();

	fclose(stderr);

	mp_log("X11 geometry set to %dx%d+%d+%d\n", _mpv_gtk_width, _mpv_gtk_height,
		_mpv_gtk_xpos, _mpv_gtk_ypos);
*/
	/* set application icon */
/*	pixmap=gdk_pixmap_create_from_xpm_d(window->window,
		&mask, NULL, mp_xpm);
	gdk_window_set_icon(window->window, NULL, pixmap, mask);*/

	return(NULL);
}


static mpdm_t gtk_drv_shutdown(mpdm_t a)
{
	return(NULL);
}


static mpdm_t gtk_drv_main_loop(mpdm_t a)
{
	gtk_main();

	return(NULL);
}


int gtk_drv_init(mpdm_t mp)
{
/*	if(!gtk_init_check(&mp_main_argc, &mp_main_argv))*/
		return(0);

	gtk_driver = mpdm_ref(MPDM_H(0));

	mpdm_hset_s(gtk_driver, L"driver", MPDM_LS(L"gtk"));
	mpdm_hset_s(gtk_driver, L"startup", MPDM_X(gtk_drv_startup));
	mpdm_hset_s(gtk_driver, L"main_loop", MPDM_X(gtk_drv_main_loop));
	mpdm_hset_s(gtk_driver, L"shutdown", MPDM_X(gtk_drv_shutdown));

	mpdm_hset_s(mp, L"drv", gtk_driver);

	return(1);
}

#else /* CONFOPT_GTK */

int gtk_drv_init(void)
{
	/* no GTK */
	return(0);
}

#endif /* CONFOPT_GTK */
