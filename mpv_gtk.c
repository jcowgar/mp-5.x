/*

    Minimum Profit - Programmer Text Editor

    GTK driver.

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

#ifdef CONFOPT_GTK

#include <stdio.h>
#include <wchar.h>
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

#include "mp.xpm"

/*******************
	Data
********************/

/* global data */
static GtkWidget * window = NULL;
static GtkWidget * file_tabs = NULL;
static GtkWidget * area = NULL;
static GtkWidget * scrollbar = NULL;
static GtkWidget * status = NULL;
static GtkWidget * entry = NULL;
static GdkGC * gc = NULL;
static GtkIMContext * im = NULL;
static GdkPixmap * pixmap = NULL;

/* character read from the keyboard */
static wchar_t im_char[2];

/* font information */
static int font_width = 0;
static int font_height = 0;
static PangoFontDescription * font = NULL;

/* the attributes */
static GdkColor * inks = NULL;
static GdkColor * papers = NULL;
static int * underlines = NULL;

/* true if the selection is ours */
static int got_selection = 0;

/* hack for active waiting for the selection */
static int wait_for_selection = 0;

/* global modal status */
static int modal_status = -1;

/* global text from entry widget */
static mpdm_t readline_text = NULL;

/* code for the 'normal' attribute */
static int normal_attr = 0;

/*******************
	Code
********************/

static char * wcs_to_utf8(wchar_t * wptr, int i, gsize * o)
/* converts a wcs to utf-8 */
{
	static char * prev = NULL;

	/* free the previously allocated string */
	if(prev != NULL) g_free(prev);

	/* do the conversion */
	prev = g_convert((gchar *) wptr, i * sizeof(wchar_t),
		"UTF-8", "WCHAR_T", NULL, o, NULL);

	return(prev);
}


static void update_window_size(void)
/* updates the viewport size in characters */
{
	PangoLayout * pa;
	int tx, ty;
	mpdm_t v;

	/* get font metrics */
	pa = gtk_widget_create_pango_layout(area, "m");
	pango_layout_set_font_description(pa, font);
	pango_layout_get_pixel_size(pa, &font_width, &font_height);
	g_object_unref(pa);

	/* calculate the size in chars */
	tx = (area->allocation.width / font_width);
	ty = (area->allocation.height / font_height) + 1;

	/* store the 'window' size */
	v = mpdm_hget_s(mp, L"window");
	mpdm_hset_s(v, L"tx", MPDM_I(tx));
	mpdm_hset_s(v, L"ty", MPDM_I(ty));

	/* rebuild the pixmap for the double buffer */
	if(pixmap != NULL) gdk_pixmap_unref(pixmap);

	pixmap = gdk_pixmap_new(area->window,
		area->allocation.width, font_height, -1);
}


static void build_fonts(void)
/* builds the fonts */
{
	char tmp[128];
	int font_size = 12;
	char * font_face = "Mono";
	mpdm_t c;

	if(font != NULL)
		pango_font_description_free(font);

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

	snprintf(tmp, sizeof(tmp) - 1, "%s %d", font_face, font_size);
	tmp[sizeof(tmp) - 1] = '\0';

	font = pango_font_description_from_string(tmp);
	update_window_size();
}


static void build_color(GdkColor * gdkcolor, int rgb)
/* builds a color */
{
	gdkcolor->pixel = 0;
	gdkcolor->blue  = (rgb & 0x000000ff) << 8;
	gdkcolor->green = (rgb & 0x0000ff00);
	gdkcolor->red   = (rgb & 0x00ff0000) >> 8;
	gdk_color_alloc(gdk_colormap_get_system(), gdkcolor);
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
	inks = realloc(inks, sizeof(GdkColor) * s);
	papers = realloc(papers, sizeof(GdkColor) * s);
	underlines = realloc(underlines, sizeof(int) * s);

	/* loop the colors */
	for(n = 0;n < s && (c = mpdm_aget(l, n)) != NULL;n++)
	{
		mpdm_t d = mpdm_hget(colors, c);
		mpdm_t v = mpdm_hget_s(d, L"gui");

		/* store the 'normal' attribute */
		if(wcscmp(mpdm_string(c), L"normal") == 0)
			normal_attr = n;

		/* store the attr */
		mpdm_hset_s(d, L"attr", MPDM_I(n));

		build_color(&inks[n], mpdm_ival(mpdm_aget(v, 0)));
		build_color(&papers[n], mpdm_ival(mpdm_aget(v, 1)));

		/* flags */
		v = mpdm_hget_s(d, L"flags");
		underlines[n] = mpdm_seek_s(v, L"underline", 1) != -1 ? 1 : 0;

		if(mpdm_seek_s(v, L"reverse", 1) != -1)
		{
			GdkColor t;

			t = inks[n];
			inks[n] = papers[n];
			papers[n] = t;
		}
	}
}


static void redraw(void);

static void switch_page(GtkNotebook * notebook, GtkNotebookPage * page,
	gint pg_num, gpointer data)
/* 'switch_page' handler (filetabs) */
{
	/* sets the active one */
	mpdm_hset_s(mp, L"active_i", MPDM_I(pg_num));

	gtk_widget_grab_focus(area);
	redraw();
}


static void draw_filetabs(void)
/* draws the filetabs */
{
	int active, last;
	mpdm_t docs;

	/* disconnect redraw signal to avoid infinite loops */
	gtk_signal_disconnect_by_func(GTK_OBJECT(file_tabs),
		(GtkSignalFunc) switch_page, NULL);

	/* gets the document list */
	if((docs = mp_get_filetabs(&active, &last)) != NULL)
	{
		int n;

		/* delete the current tabs */
		for(n = 0;n < last;n++)
			gtk_notebook_remove_page(
				GTK_NOTEBOOK(file_tabs), 0);

		/* create the new ones */
		for(n = 0;n < mpdm_size(docs);n++)
		{
			GtkWidget * l;
			GtkWidget * f;
			char * ptr;
			wchar_t * wptr;
			mpdm_t v = mpdm_aget(docs, n);

			/* just get the name */
			v = mpdm_hget_s(v, L"name");

			/* move to the filename if path included */
			if((wptr = wcsrchr(v->data, L'/')) == NULL)
				wptr = v->data;
			else
				wptr++;

			ptr = wcs_to_utf8(wptr, wcslen(wptr), NULL);
			l = gtk_label_new(ptr);
			gtk_widget_show(l);

			f = gtk_frame_new(NULL);
			gtk_widget_show(f);

			gtk_notebook_append_page(
				GTK_NOTEBOOK(file_tabs), f, l);
		}
	}

	/* set the active one */
	gtk_notebook_set_page(GTK_NOTEBOOK(file_tabs), active);

	/* reconnect signal */
	gtk_signal_connect(GTK_OBJECT(file_tabs), "switch_page",
		(GtkSignalFunc) switch_page, NULL);

	gtk_widget_grab_focus(area);
}


static void draw_status(void)
/* draws the status line */
{
	mpdm_t t;
	char * ptr;

	/* call mp.status_line() */
	t = mp_build_status_line();

	if(t != NULL && (ptr = wcs_to_utf8(t->data, mpdm_size(t), NULL)) != NULL)
		gtk_label_set_text(GTK_LABEL(status), ptr);
}


static gint scroll_event(GtkWidget * widget, GdkEventScroll * event)
/* 'scroll_event' handler (mouse wheel) */
{
	wchar_t * ptr = NULL;

	switch(event->direction)
	{
	case GDK_SCROLL_UP: ptr = L"mouse-wheel-up"; break;
	case GDK_SCROLL_DOWN: ptr = L"mouse-wheel-down"; break;
	case GDK_SCROLL_LEFT: ptr = L"mouse-wheel-left"; break;
	case GDK_SCROLL_RIGHT: ptr = L"mouse-wheel-right"; break;
	}

	if(ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	redraw();

	return(0);
}


static void value_changed(GtkAdjustment * adj, gpointer * data)
/* 'value_changed' handler (scrollbar) */
{
	int i = (int) adj->value;
	mpdm_t doc;
	mpdm_t txt;
	int y;

	/* get current y position */
	doc = mp_active();
	txt = mpdm_hget_s(doc, L"txt");
	y = mpdm_ival(mpdm_hget_s(txt, L"y"));

	/* if it's different, set and redraw */
	if(y != i)
	{
		mp_set_y(doc, i);
		redraw();
	}
}


static void draw_scrollbar(void)
/* updates the scrollbar */
{
	GtkAdjustment * adjustment;
	mpdm_t d;
	mpdm_t v;
	int pos, size, max;

	/* gets the active document */
	if((d = mp_active()) == NULL)
		return;

	/* get the coordinates */
	v = mpdm_hget_s(d, L"txt");
	pos = mpdm_ival(mpdm_hget_s(v, L"y"));
	max = mpdm_size(mpdm_hget_s(v, L"lines"));

	v = mpdm_hget_s(mp, L"window");
	size = mpdm_ival(mpdm_hget_s(v, L"ty"));

	adjustment = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

/*	if((int)adjustment->upper==max &&
	   (int)adjustment->page_size==size &&
	   (int)adjustment->page_increment==size &&
	   (int)adjustment->value==pos)
		return;
*/
	/* disconnect to avoid infinite loops */
	gtk_signal_disconnect_by_func(GTK_OBJECT(adjustment),
		(GtkSignalFunc) value_changed, NULL);

	adjustment->step_increment = (gfloat)1;
	adjustment->upper = (gfloat)(max + size);
	adjustment->page_size = (gfloat)size;
	adjustment->page_increment = (gfloat)size;
	adjustment->value = (gfloat)pos;

	gtk_range_set_adjustment(GTK_RANGE(scrollbar), adjustment);

	gtk_adjustment_changed(adjustment);
	gtk_adjustment_value_changed(adjustment);

	/* reattach again */
	gtk_signal_connect(
		GTK_OBJECT(gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
		"value_changed", (GtkSignalFunc) value_changed, NULL);
}


static void gtkdrv_paint(mpdm_t doc, int optimize)
/* GTK document draw function */
{
	GdkRectangle gr;
	mpdm_t d = NULL;
	int n, m;

	/* no gc? create it */
	if(gc == NULL)
		gc = gdk_gc_new(area->window);

	if((d = mp_draw(doc, optimize)) == NULL)
		return;

	if(font == NULL)
	{
		build_fonts();
		build_colors();
	}

	gr.x = 0;
	gr.y = 0;
	gr.width = area->allocation.width;
	gr.height = font_height;

	for(n = 0;n < mpdm_size(d);n++)
	{
		PangoLayout * pl;
		PangoAttrList * pal;
		mpdm_t l = mpdm_aget(d, n);
		char * str = NULL;
		int u, p;

		if(l == NULL) continue;

		/* create the pango stuff */
		pl = gtk_widget_create_pango_layout(area, NULL);
		pango_layout_set_font_description(pl, font);
		pal = pango_attr_list_new();

		for(m = u = p = 0;m < mpdm_size(l);m++, u = p)
		{
			PangoAttribute * pa;
			int attr;
			mpdm_t s;
			char * ptr;

			/* get the attribute and the string */
			attr = mpdm_ival(mpdm_aget(l, m++));
			s = mpdm_aget(l, m);

			/* convert the string to utf8 */
			ptr = wcs_to_utf8(s->data, mpdm_size(s), NULL);

			/* add to the full line */
			str = mpdm_poke(str, &p, ptr, strlen(ptr), 1);

			/* create the background if it's
			   different from the default */
			if(papers[attr].red != papers[normal_attr].red ||
			   papers[attr].green != papers[normal_attr].green ||
			   papers[attr].blue != papers[normal_attr].blue)
			{
				pa = pango_attr_background_new(
					papers[attr].red, papers[attr].green,
					papers[attr].blue);

				pa->start_index = u;
				pa->end_index = p;

				pango_attr_list_insert(pal, pa);
			}

			/* underline? */
			if(underlines[attr])
			{
				pa=pango_attr_underline_new(TRUE);

				pa->start_index = u;
				pa->end_index = p;

				pango_attr_list_insert(pal, pa);
			}

			/* foreground color */
			pa=pango_attr_foreground_new(inks[attr].red,
				inks[attr].green, inks[attr].blue);

			pa->start_index = u;
			pa->end_index = p;

			pango_attr_list_insert(pal, pa);
		}

		/* store the attributes */
		pango_layout_set_attributes(pl, pal);
		pango_attr_list_unref(pal);

		/* store and free the text */
		pango_layout_set_text(pl, str, p);
		free(str);

		/* draw the background */
		gdk_gc_set_foreground(gc, &papers[normal_attr]);
		gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0,
			gr.width, gr.height);

		/* draw the text */
		gtk_paint_layout(area->style, pixmap,
			GTK_STATE_NORMAL, TRUE,
			&gr, area, "", 2, 0, pl);

		/* dump the pixmap */
		gdk_draw_pixmap(area->window, gc, pixmap,
			0, 0, 0, n * font_height, gr.width, gr.height);

		g_object_unref(pl);
	}

	draw_filetabs();
	draw_scrollbar();
	draw_status();
}


static void redraw(void)
{
	gtkdrv_paint(mp_active(), 0);
}


static gint delete_event(GtkWidget * w, GdkEvent * e, gpointer data)
/* 'delete_event' handler */
{
	mp_process_event(MPDM_LS(L"close-window"));

	return(mp_exit_requested ? FALSE : TRUE);
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

	gtk_im_context_filter_keypress(im, event);

/*	mpi_move_selecting=event->state & GDK_SHIFT_MASK;
*/
	/* reserve alt for menu mnemonics */
	if (GDK_MOD1_MASK & event->state)
		return(0);

	if(event->state & (GDK_CONTROL_MASK))
	{
		switch(event->keyval)
		{
		case GDK_Up:		ptr = L"ctrl-cursor-up"; break;
		case GDK_Down:		ptr = L"ctrl-cursor-down"; break;
		case GDK_Left:		ptr = L"ctrl-cursor-left"; break;
		case GDK_Right: 	ptr = L"ctrl-cursor-right"; break;
		case GDK_Prior: 	ptr = L"ctrl-page-up"; break;
		case GDK_Next:		ptr = L"ctrl-page-down"; break;
		case GDK_Home:		ptr = L"ctrl-home"; break;
		case GDK_End:		ptr = L"ctrl-end"; break;
		case GDK_space:		ptr = L"ctrl-space"; break;
		case GDK_KP_Add: 	ptr = L"ctrl-kp-plus"; break;
		case GDK_KP_Subtract:	ptr = L"ctrl-kp-minus"; break;
		case GDK_KP_Multiply:	ptr = L"ctrl-kp-multiply"; break;
		case GDK_KP_Divide:	ptr = L"ctrl-kp-divide"; break;
		case GDK_F1:		ptr = L"ctrl-f1"; break;
		case GDK_F2:		ptr = L"ctrl-f2"; break;
		case GDK_F3:		ptr = L"ctrl-f3"; break;
		case GDK_F4:		ptr = L"ctrl-f4"; break;
		case GDK_F5:		ptr = L"ctrl-f5"; break;
		case GDK_F6:		ptr = L"ctrl-f6"; break;
		case GDK_F7:		ptr = L"ctrl-f7"; break;
		case GDK_F8:		ptr = L"ctrl-f8"; break;
		case GDK_F9:		ptr = L"ctrl-f9"; break;
		case GDK_F10:		ptr = L"ctrl-f10"; break;
		case GDK_F11:		ptr = L"ctrl-f11"; break;
		case GDK_F12:		ptr = L"ctrl-f12"; break;
		case GDK_KP_Enter:
		case GDK_Return:	ptr = L"ctrl-enter"; break;

		case 'a':		ptr = L"ctrl-a"; break;
		case 'b':		ptr = L"ctrl-b"; break;
		case 'c':		ptr = L"ctrl-c"; break;
		case 'd':		ptr = L"ctrl-d"; break;
		case 'e':		ptr = L"ctrl-e"; break;
		case 'f':		ptr = L"ctrl-f"; break;
		case 'g':		ptr = L"ctrl-g"; break;
		case 'h':		ptr = L"ctrl-h"; break;
		case 'i':		ptr = L"ctrl-i"; break;
		case 'j':		ptr = L"ctrl-j"; break;
		case 'k':		ptr = L"ctrl-k"; break;
		case 'l':		ptr = L"ctrl-l"; break;
		case 'm':		ptr = L"ctrl-m"; break;
		case 'n':		ptr = L"ctrl-n"; break;
		case 'o':		ptr = L"ctrl-o"; break;
		case 'p':		ptr = L"ctrl-p"; break;
		case 'q':		ptr = L"ctrl-q"; break;
		case 'r':		ptr = L"ctrl-r"; break;
		case 's':		ptr = L"ctrl-s"; break;
		case 't':		ptr = L"ctrl-t"; break;
		case 'u':		ptr = L"ctrl-u"; break;
		case 'v':		ptr = L"ctrl-v"; break;
		case 'w':		ptr = L"ctrl-w"; break;
		case 'x':		ptr = L"ctrl-x"; break;
		case 'y':		ptr = L"ctrl-y"; break;
		case 'z':		ptr = L"ctrl-z"; break;
		}
	}
	else
	{
		switch(event->keyval)
		{
		case GDK_Up:		ptr = L"cursor-up"; break;
		case GDK_Down:		ptr = L"cursor-down"; break;
		case GDK_Left:		ptr = L"cursor-left"; break;
		case GDK_Right:		ptr = L"cursor-right"; break;
		case GDK_Prior:		ptr = L"page-up"; break;
		case GDK_Next:		ptr = L"page-down"; break;
		case GDK_Home:		ptr = L"home"; break;
		case GDK_End:		ptr = L"end"; break;
		case GDK_space:		ptr = L"space"; break;
		case GDK_KP_Add:	ptr = L"kp-plus"; break;
		case GDK_KP_Subtract:	ptr = L"kp-minus"; break;
		case GDK_KP_Multiply:	ptr = L"kp-multiply"; break;
		case GDK_KP_Divide:	ptr = L"kp-divide"; break;
		case GDK_F1:		ptr = L"f1"; break;
		case GDK_F2:		ptr = L"f2"; break;
		case GDK_F3:		ptr = L"f3"; break;
		case GDK_F4:		ptr = L"f4"; break;
		case GDK_F5:		ptr = L"f5"; break;
		case GDK_F6:		ptr = L"f6"; break;
		case GDK_F7:		ptr = L"f7"; break;
		case GDK_F8:		ptr = L"f8"; break;
		case GDK_F9:		ptr = L"f9"; break;
		case GDK_F10:		ptr = L"f10"; break;
		case GDK_F11:		ptr = L"f11"; break;
		case GDK_F12:		ptr = L"f12"; break;
		case GDK_Insert:	ptr = L"insert"; break;
		case GDK_BackSpace:	ptr = L"backspace"; break;
		case GDK_Delete:	ptr = L"delete"; break;
		case GDK_KP_Enter:
		case GDK_Return:	ptr = L"enter"; break;
		case GDK_Tab:		ptr = L"tab"; break;
		case GDK_Escape:	ptr = L"escape"; break;
		}
	}

	/* if there is a pending char in im_char, use it */
	if(ptr == NULL && im_char[0] != L'\0')
		ptr = im_char;

	/* finally process */
	if(ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	/* delete the pending char */
	im_char[0] = L'\0';

	if(mp_exit_requested)
		gtk_main_quit();

	gtkdrv_paint(mp_active(), 1);

	return(0);
}


static gint button_press_event(GtkWidget * widget, GdkEventButton * event, gpointer data)
/* 'button_press_event' handler (mouse buttons) */
{
	int x, y;
	wchar_t * ptr = NULL;

	/* mouse instant positioning */
	x = ((int)event->x) / font_width;
	y = ((int)event->y) / font_height;

	mpdm_hset_s(mp, L"mouse_x", MPDM_I(x));
	mpdm_hset_s(mp, L"mouse_y", MPDM_I(y));

	switch(event->button)
	{
	case 1: ptr = L"mouse-left-button"; break;
	case 2: ptr = L"mouse-middle-button"; break;
	case 3: ptr = L"mouse-right-button"; break;
	case 4: ptr = L"mouse-wheel-up"; break;
	case 5: ptr = L"mouse-wheel-down"; break;
	}

	if(ptr != NULL)
		mp_process_event(MPDM_S(ptr));

	redraw();

	return(0);
}


static void commit(GtkIMContext * i, char * str, gpointer u)
/* 'commit' handler */
{
	wchar_t * wstr;

	wstr = (wchar_t *) g_convert(str, -1,
		"WCHAR_T", "UTF-8", NULL, NULL, NULL);

	im_char[0] = *wstr;
	im_char[1] = L'\0';

	g_free(wstr);
}


static void realize(GtkWidget * widget)
/* 'realize' handler */
{
	im = gtk_im_multicontext_new();
	g_signal_connect(im, "commit", G_CALLBACK(commit), NULL);
	gtk_im_context_set_client_window(im, widget->window);
}


static gint expose_event(GtkWidget * widget, GdkEventExpose * event)
/* 'expose_event' handler */
{
	redraw();

	return(FALSE);
}


static gint configure_event(GtkWidget * widget, GdkEventConfigure * event)
/* 'configure_event' handler */
{
	static GdkEventConfigure o;

	if(memcmp(&o, event, sizeof(o)) == 0)
		return(TRUE);

	memcpy(&o, event, sizeof(o));

	update_window_size();
	redraw();

	return(TRUE);
}


static gint selection_clear_event(GtkWidget * widget,
	GdkEventSelection * event, gpointer data)
/* 'selection_clear_event' handler */
{
	got_selection = 0;

	return(TRUE);
}


static void selection_get(GtkWidget * widget,
	GtkSelectionData * sel, guint info, guint tm, gpointer data)
/* 'selection_get' handler */
{
	mpdm_t d;
	unsigned char * ptr;
	int s;

	if(!got_selection) return;

	/* gets the clipboard and joins */
	d = mpdm_hget_s(mp, L"clipboard");
	d = mpdm_join(MPDM_LS(L"\n"), d);

	/* convert to current locale */
	ptr = (unsigned char *) mpdm_wcstombs(d->data, &s);

        /* pastes into primary selection */
        gtk_selection_data_set(sel, GDK_SELECTION_TYPE_STRING, 8, ptr, (gsize) s);

	free(ptr);
}


static void selection_received(GtkWidget * widget,
	GtkSelectionData * sel, guint tm, gpointer data)
/* 'selection_received' handler */
{
	mpdm_t d;

	/* get selection */
	d = MPDM_NMBS((char *)sel->data, sel->length);

	/* split and set as the clipboard */
	mpdm_hset_s(mp, L"clipboard", mpdm_split(MPDM_LS(L"\n"), d));

	/* wait no more for the selection */
	wait_for_selection = 0;
}


static mpdm_t gtkdrv_clip_to_sys(mpdm_t a)
/* driver-dependent mp to system clipboard */
{
	got_selection = gtk_selection_owner_set(area,
		GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME);

	return(NULL);
}


static mpdm_t gtkdrv_sys_to_clip(mpdm_t a)
/* driver-dependent system to mp clipboard */
{
	if(!got_selection)
	{
		/* triggers a selection capture */
		if(gtk_selection_convert(area, GDK_SELECTION_PRIMARY,
			gdk_atom_intern("STRING", FALSE),
			GDK_CURRENT_TIME))
		{

			/* processes the pending events
			   (i.e., the 'selection_received' handler) */
			wait_for_selection = 1;

			while(wait_for_selection)
				gtk_main_iteration();
		}
	}

	return(NULL);
}


static void gtkdrv_startup(void)
/* driver initialization */
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
	file_tabs = gtk_notebook_new();
	gtk_notebook_set_tab_pos(GTK_NOTEBOOK(file_tabs), GTK_POS_TOP);
	GTK_WIDGET_UNSET_FLAGS(file_tabs,GTK_CAN_FOCUS);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(file_tabs), 1);

	vbox = gtk_vbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(window), vbox);
/*
	gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 0);
*/	gtk_box_pack_start(GTK_BOX(vbox), file_tabs, FALSE, FALSE, 0);

	/* horizontal box holding the text and the scrollbar */
	hbox = gtk_hbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	/* the Minimum Profit area */
	area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(hbox), area, TRUE, TRUE, 0);
	gtk_widget_set_usize(GTK_WIDGET(area), 600, 400);
	gtk_widget_set_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK);

	gtk_widget_set_double_buffered(area, FALSE);

	gtk_signal_connect(GTK_OBJECT(area),"configure_event",
		(GtkSignalFunc) configure_event, NULL);

	gtk_signal_connect(GTK_OBJECT(area),"expose_event",
		(GtkSignalFunc) expose_event, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "realize",
		(GtkSignalFunc) realize, NULL);

	gtk_signal_connect(GTK_OBJECT(window),"key_press_event",
		(GtkSignalFunc) key_press_event, NULL);

	gtk_signal_connect(GTK_OBJECT(area),"button_press_event",
		(GtkSignalFunc) button_press_event, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_clear_event",
		(GtkSignalFunc) selection_clear_event, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_get",
		(GtkSignalFunc) selection_get, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "selection_received",
		(GtkSignalFunc) selection_received, NULL);

	gtk_signal_connect(GTK_OBJECT(area), "scroll_event",
		(GtkSignalFunc) scroll_event, NULL);

	gtk_selection_add_target(area, GDK_SELECTION_PRIMARY,
		GDK_SELECTION_TYPE_STRING, 1);

	gtk_signal_connect(GTK_OBJECT(file_tabs),"switch_page",
		(GtkSignalFunc) switch_page, NULL);

	/* the scrollbar */
	scrollbar = gtk_vscrollbar_new(NULL);
	gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

	gtk_signal_connect(
		GTK_OBJECT(gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
		"value_changed", (GtkSignalFunc) value_changed, NULL);

	/* the status bar */
	status = gtk_label_new("mp " VERSION);
	gtk_box_pack_start(GTK_BOX(vbox), status, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(status), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(status), GTK_JUSTIFY_LEFT);

	gtk_widget_show_all(window);

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
	pixmap = gdk_pixmap_create_from_xpm_d(window->window,
		&mask, NULL, mp_xpm);
	gdk_window_set_icon(window->window, NULL, pixmap, mask);
}


static void gtkdrv_main_loop(void)
/* main loop */
{
	gtk_main();
}


static void gtkdrv_shutdown(void)
/* shutdown */
{
}


static void wait_for_modal_status_change(void)
/* wait until modal status changes */
{
	modal_status = -1;

	while(modal_status == -1)
		gtk_main_iteration();
}


static void clicked_ok(GtkWidget * widget, gpointer data)
{
	if(entry != NULL)
	{
		/* if there is an entry widget, get its text */
		char * ptr;

		mpdm_unref(readline_text);
		ptr = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

		readline_text = mpdm_ref(MPDM_MBS(ptr));
		g_free(ptr);

		entry = NULL;
	}

	modal_status = 1;
	gtk_widget_destroy(GTK_WIDGET(widget));
}


static void clicked_cancel(GtkWidget * widget, gpointer data)
{
	modal_status = 0;
	gtk_widget_destroy(GTK_WIDGET(widget));
}


static void clicked_no(GtkWidget * widget, gpointer data)
{
	modal_status = 2;
	gtk_widget_destroy(GTK_WIDGET(widget));
}


static int confirm_key_press_event(GtkWidget * widget, GdkEventKey * event)
{
	if(event->string[0] == '\r')
	{
		clicked_ok(widget, NULL);
		return(1);
	}
	else
	if(event->string[0] == '\e')
	{
		clicked_cancel(widget, NULL);
		return(1);
	}

	return(0);
}


static mpdm_t gtkdrv_alert(mpdm_t a)
/* alert driver function */
{
	wchar_t * wptr;
	char * ptr;
	GtkWidget * dlg;
	GtkWidget * label;
	GtkWidget * button;

	/* gets a printable representation of the first argument */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((ptr = wcs_to_utf8(wptr, wcslen(wptr), NULL)) == NULL)
		return(NULL);

	dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), 5);

	label = gtk_label_new(ptr);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	button = gtk_button_new_with_label("OK");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(clicked_ok), GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), button, TRUE, TRUE, 0);
	gtk_widget_show(button);

	gtk_signal_connect(GTK_OBJECT(dlg),"key_press_event",
		(GtkSignalFunc) confirm_key_press_event, NULL);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(dlg),TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dlg),GTK_WINDOW(window));

	gtk_widget_show(dlg);

	wait_for_modal_status_change();

	return(NULL);
}


static mpdm_t gtkdrv_confirm(mpdm_t a)
/* confirm driver function */
{
	wchar_t * wptr;
	char * ptr;
	GtkWidget * dlg;
	GtkWidget * label;
	GtkWidget * ybutton;
	GtkWidget * nbutton;
	GtkWidget * cbutton;

	/* gets a printable representation of the first argument */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((ptr = wcs_to_utf8(wptr, wcslen(wptr), NULL)) == NULL)
		return(NULL);

	dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), 5);

	label = gtk_label_new(ptr);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	ybutton = gtk_button_new_with_label("Yes");
	gtk_signal_connect_object(GTK_OBJECT(ybutton), "clicked",
		GTK_SIGNAL_FUNC(clicked_ok),GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), ybutton, TRUE, TRUE, 0);
	gtk_widget_show(ybutton);

	nbutton = gtk_button_new_with_label("No");
	gtk_signal_connect_object(GTK_OBJECT(nbutton), "clicked",
		GTK_SIGNAL_FUNC(clicked_no), GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), nbutton, TRUE, TRUE, 0);
	gtk_widget_show(nbutton);

	cbutton = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(cbutton), "clicked",
		GTK_SIGNAL_FUNC(clicked_cancel), GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), cbutton, TRUE, TRUE, 0);
	gtk_widget_show(cbutton);

	gtk_signal_connect(GTK_OBJECT(dlg),"key_press_event",
		(GtkSignalFunc) confirm_key_press_event, NULL);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(window));

	gtk_widget_show(dlg);

	wait_for_modal_status_change();

	return(MPDM_I(modal_status));
}


static mpdm_t gtkdrv_readline(mpdm_t a)
/* readline driver function */
{
	wchar_t * wptr;
	char * ptr;
	GtkWidget * dlg;
	GtkWidget * label;
	GtkWidget * ybutton;
	GtkWidget * nbutton;
	GtkWidget * combo;
	mpdm_t h;

	/* gets a printable representation of the first argument */
	wptr = mpdm_string(mpdm_aget(a, 0));

	if((ptr = wcs_to_utf8(wptr, wcslen(wptr), NULL)) == NULL)
		return(NULL);

	/* get the history */
	h = mp_get_history(mpdm_aget(a, 1));

	dlg = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
	gtk_container_border_width(GTK_CONTAINER(GTK_DIALOG(dlg)->vbox), 5);

	label = gtk_label_new(ptr);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), label, TRUE, TRUE, 0);
	gtk_widget_show(label);

	combo = gtk_combo_new();
	entry = GTK_COMBO(combo)->entry;
	gtk_widget_set_usize(combo, 300, -1);
	gtk_combo_set_use_arrows_always(GTK_COMBO(combo), TRUE);
	gtk_combo_set_case_sensitive(GTK_COMBO(combo), TRUE);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	ybutton = gtk_button_new_with_label("OK");
	gtk_signal_connect_object(GTK_OBJECT(ybutton),"clicked",
		GTK_SIGNAL_FUNC(clicked_ok), GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), ybutton, TRUE, TRUE, 0);
	gtk_widget_show(ybutton);

	nbutton = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(nbutton), "clicked",
			GTK_SIGNAL_FUNC(clicked_cancel), GTK_OBJECT(dlg));
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), nbutton, TRUE, TRUE, 0);
	gtk_widget_show(nbutton);

	gtk_signal_connect(GTK_OBJECT(dlg),"key_press_event",
		(GtkSignalFunc) confirm_key_press_event, NULL);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(dlg), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dlg), GTK_WINDOW(window));

	gtk_widget_show(dlg);
	gtk_widget_grab_focus(entry);

	wait_for_modal_status_change();

	if(modal_status == 1)
	{
		if(h != NULL)
			mpdm_push(h, readline_text);

		return(readline_text);
	}

	return(NULL);
}


static mpdm_t gtkdrv_ui(mpdm_t a)
{
	gtkdrv_startup();
	gtkdrv_main_loop();
	gtkdrv_shutdown();

	return(NULL);
}


static mpdm_t gtkdrv_update_ui(mpdm_t a)
{
	build_fonts();
	build_colors();

	redraw();

	return(NULL);
}


int gtkdrv_init(void)
{
	mpdm_t drv;

	if(!gtk_init_check(&mp_main_argc, &mp_main_argv))
		return(0);

	drv = MPDM_H(0);
	mpdm_hset_s(mp, L"drv", drv);

	mpdm_hset_s(drv, L"id", MPDM_LS(L"gtk"));

	mpdm_hset_s(drv, L"ui", MPDM_X(gtkdrv_ui));
	mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(gtkdrv_clip_to_sys));
	mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(gtkdrv_sys_to_clip));
	mpdm_hset_s(drv, L"update_ui", MPDM_X(gtkdrv_update_ui));

	mpdm_hset_s(drv, L"alert", MPDM_X(gtkdrv_alert));
	mpdm_hset_s(drv, L"confirm", MPDM_X(gtkdrv_confirm));
	mpdm_hset_s(drv, L"readline", MPDM_X(gtkdrv_readline));

	return(1);
}

#else /* CONFOPT_GTK */

int gtkdrv_init(void)
{
	/* no GTK */
	return(0);
}

#endif /* CONFOPT_GTK */
