/*

    Minimum Profit - Programmer Text Editor

    GTK driver.

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

/** data **/

/* global data */

static GtkWidget *window = NULL;
static GtkWidget *file_tabs = NULL;
static GtkWidget *area = NULL;
static GtkWidget *scrollbar = NULL;
static GtkWidget *status = NULL;
static GtkWidget *menu_bar = NULL;
static GdkGC *gc = NULL;
static GtkIMContext *im = NULL;
static GdkPixmap *pixmap = NULL;

/* character read from the keyboard */
static wchar_t im_char[2];

/* font information */
static int font_width = 0;
static int font_height = 0;
static PangoFontDescription *font = NULL;

/* the attributes */
static GdkColor *inks = NULL;
static GdkColor *papers = NULL;
static int *underlines = NULL;

/* true if the selection is ours */
static int got_selection = 0;

/* hack for active waiting for the selection */
static int wait_for_selection = 0;

/* global modal status */
/* code for the 'normal' attribute */
static int normal_attr = 0;

/* mp.drv.form() controls */

static GtkWidget **form_widgets = NULL;
static mpdm_t form_args = NULL;
static mpdm_t form_values = NULL;

/* mouse down flag */
static int mouse_down = 0;

/* timer function */
static mpdm_t timer_func = NULL;

/* maximize wanted? */
static int maximize = 0;

/** code **/

/** support functions **/

#define LL(m) (m)

static char *wcs_to_utf8(const wchar_t * wptr)
/* converts a wcs to utf-8 */
{
    char *ptr;
    gsize i, o;

    i = wcslen(wptr);

    /* do the conversion */
    ptr = g_convert((const gchar *) wptr, (i + 1) * sizeof(wchar_t),
                    "UTF-8", "WCHAR_T", NULL, &o, NULL);

    return ptr;
}


static char *v_to_utf8(mpdm_t v)
{
    char *ptr = NULL;

    if (v != NULL) {
        mpdm_ref(v);
        ptr = wcs_to_utf8(mpdm_string(v));
        mpdm_unref(v);
    }

    return ptr;
}

static wchar_t *utf8_to_wcs(const char *ptr)
/* converts utf-8 to wcs */
{
    wchar_t *wptr;
    gsize i, o;

    i = strlen(ptr);

    /* do the conversion */
    wptr = (wchar_t *) g_convert((gchar *) ptr, i + 1,
                                 "WCHAR_T", "UTF-8", NULL, &o, NULL);

    return wptr;
}


static void update_window_size(void)
/* updates the viewport size in characters */
{
    PangoLayout *pa;
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
    if (pixmap != NULL)
        gdk_pixmap_unref(pixmap);

    pixmap = gdk_pixmap_new(area->window,
                            area->allocation.width, font_height, -1);
}


static void build_fonts(void)
/* builds the fonts */
{
    char tmp[128];
    int font_size = 12;
    const char *font_face = "Mono";
    mpdm_t c;
    mpdm_t w = NULL;

    if (font != NULL)
        pango_font_description_free(font);

    /* get current configuration */
    if ((c = mpdm_hget_s(mp, L"config")) != NULL) {
        mpdm_t v;

        if ((v = mpdm_hget_s(c, L"font_size")) != NULL)
            font_size = mpdm_ival(v);
        else
            mpdm_hset_s(c, L"font_size", MPDM_I(font_size));

        if ((v = mpdm_hget_s(c, L"font_face")) != NULL) {
            w = mpdm_ref(MPDM_2MBS(v->data));
            font_face = w->data;
        }
        else
            mpdm_hset_s(c, L"font_face", MPDM_MBS(font_face));
    }

    snprintf(tmp, sizeof(tmp) - 1, "%s %d", font_face, font_size);
    tmp[sizeof(tmp) - 1] = '\0';

    font = pango_font_description_from_string(tmp);
    update_window_size();

    mpdm_unref(w);
}


static void build_color(GdkColor * gdkcolor, int rgb)
/* builds a color */
{
    gdkcolor->pixel = 0;
    gdkcolor->blue = (rgb & 0x000000ff) << 8;
    gdkcolor->green = (rgb & 0x0000ff00);
    gdkcolor->red = (rgb & 0x00ff0000) >> 8;
    gdk_colormap_alloc_color(gdk_colormap_get_system(), gdkcolor, FALSE,
                             TRUE);
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
    inks = realloc(inks, sizeof(GdkColor) * s);
    papers = realloc(papers, sizeof(GdkColor) * s);
    underlines = realloc(underlines, sizeof(int) * s);

    /* loop the colors */
    for (n = 0; n < s && (c = mpdm_aget(l, n)) != NULL; n++) {
        mpdm_t d = mpdm_hget(colors, c);
        mpdm_t v = mpdm_hget_s(d, L"gui");

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(c), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_hset_s(d, L"attr", MPDM_I(n));

        build_color(&inks[n], mpdm_ival(mpdm_aget(v, 0)));
        build_color(&papers[n], mpdm_ival(mpdm_aget(v, 1)));

        /* flags */
        v = mpdm_hget_s(d, L"flags");
        underlines[n] = mpdm_seek_s(v, L"underline", 1) != -1 ? 1 : 0;

        if (mpdm_seek_s(v, L"reverse", 1) != -1) {
            GdkColor t;

            t = inks[n];
            inks[n] = papers[n];
            papers[n] = t;
        }
    }

    mpdm_unref(l);
}


/** menu functions **/

static void redraw(void);

static void menu_item_callback(mpdm_t action)
/* menu click callback */
{
    mp_process_action(action);
    redraw();

    if (mp_exit_requested)
        gtk_main_quit();
}


static void build_submenu(GtkWidget * menu, mpdm_t labels)
/* build a submenu */
{
    int n;
    GtkWidget *menu_item;

    mpdm_ref(labels);

    for (n = 0; n < mpdm_size(labels); n++) {
        /* get the action */
        mpdm_t v = mpdm_aget(labels, n);

        /* if the action is a separator... */
        if (*((wchar_t *) v->data) == L'-')
            menu_item = gtk_menu_item_new();
        else {
            char *ptr;

            ptr = v_to_utf8(mp_menu_label(v));
            menu_item = gtk_menu_item_new_with_label(ptr);
            g_free(ptr);
        }

        gtk_menu_append(GTK_MENU(menu), menu_item);
        g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
                                 G_CALLBACK(menu_item_callback), v);
        gtk_widget_show(menu_item);
    }

    mpdm_unref(labels);
}


static void build_menu(void)
/* builds the menu */
{
    static mpdm_t prev_menu = NULL;
    int n;
    mpdm_t m;

    /* gets the current menu */
    if ((m = mpdm_hget_s(mp, L"menu")) == NULL)
        return;

    /* if it's the same, do nothing */
    if (mpdm_cmp(m, prev_menu) == 0)
        return;

    /* create a new menu */
    menu_bar = gtk_menu_bar_new();

    for (n = 0; n < mpdm_size(m); n++) {
        char *ptr;
        mpdm_t mi;
        mpdm_t v;
        GtkWidget *menu;
        GtkWidget *menu_item;
        int i;

        /* get the label and the items */
        mi = mpdm_aget(m, n);
        v = mpdm_aget(mi, 0);

        if ((ptr = v_to_utf8(mpdm_gettext(v))) == NULL)
            continue;

        /* change the & by _ for the mnemonic */
        for (i = 0; ptr[i]; i++)
            if (ptr[i] == '&')
                ptr[i] = '_';

        /* add the menu and the label */
        menu = gtk_menu_new();
        menu_item = gtk_menu_item_new_with_mnemonic(ptr);
        g_free(ptr);

        gtk_widget_show(menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);
        gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), menu_item);

        /* now loop the items */
        build_submenu(menu, mpdm_aget(mi, 1));
    }
}


/** main area drawing functions **/

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
    static mpdm_t last = NULL;
    mpdm_t names;
    int n;

    names = mpdm_ref(mp_get_doc_names());

    /* disconnect redraw signal to avoid infinite loops */
    g_signal_handlers_disconnect_by_func(G_OBJECT(file_tabs),
                                         G_CALLBACK(switch_page), NULL);

    /* is the list different from the previous one? */
    if (mpdm_cmp(names, last) != 0) {

        /* delete the current tabs */
        for (n = 0; n < mpdm_size(last); n++)
            gtk_notebook_remove_page(GTK_NOTEBOOK(file_tabs), 0);

        /* create the new ones */
        for (n = 0; n < mpdm_size(names); n++) {
            GtkWidget *p;
            GtkWidget *f;
            char *ptr;
            mpdm_t v = mpdm_aget(names, n);

            if ((ptr = v_to_utf8(v)) != NULL) {
                p = gtk_label_new(ptr);
                gtk_widget_show(p);

                f = gtk_frame_new(NULL);
                gtk_widget_show(f);

                gtk_notebook_append_page(GTK_NOTEBOOK(file_tabs), f, p);

                g_free(ptr);
            }
        }

        /* store for the next time */
        mpdm_unref(last);
        last = mpdm_ref(names);
    }

    mpdm_unref(names);

    /* set the active one */
    gtk_notebook_set_page(GTK_NOTEBOOK(file_tabs),
                          mpdm_ival(mpdm_hget_s(mp, L"active_i")));

    /* reconnect signal */
    g_signal_connect(G_OBJECT(file_tabs), "switch_page",
                     G_CALLBACK(switch_page), NULL);

    gtk_widget_grab_focus(area);
}


static void draw_status(void)
/* draws the status line */
{
    char *ptr;

    if ((ptr = v_to_utf8(mp_build_status_line())) != NULL) {
        gtk_label_set_text(GTK_LABEL(status), ptr);
        g_free(ptr);
    }
}


static gint scroll_event(GtkWidget * widget, GdkEventScroll * event)
/* 'scroll_event' handler (mouse wheel) */
{
    wchar_t *ptr = NULL;

    switch (event->direction) {
    case GDK_SCROLL_UP:
        ptr = L"mouse-wheel-up";
        break;
    case GDK_SCROLL_DOWN:
        ptr = L"mouse-wheel-down";
        break;
    case GDK_SCROLL_LEFT:
        ptr = L"mouse-wheel-left";
        break;
    case GDK_SCROLL_RIGHT:
        ptr = L"mouse-wheel-right";
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    redraw();

    return 0;
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
    if (y != i) {
        mp_set_y(doc, i);
        mpdm_hset_s(txt, L"vy", MPDM_I(i));
        redraw();
    }
}


static void draw_scrollbar(void)
/* updates the scrollbar */
{
    GtkAdjustment *adjustment;
    mpdm_t d;
    mpdm_t v;
    int pos, size, max;

    /* gets the active document */
    if ((d = mp_active()) == NULL)
        return;

    /* get the coordinates */
    v = mpdm_hget_s(d, L"txt");
    pos = mpdm_ival(mpdm_hget_s(v, L"vy"));
    max = mpdm_size(mpdm_hget_s(v, L"lines"));

    v = mpdm_hget_s(mp, L"window");
    size = mpdm_ival(mpdm_hget_s(v, L"ty"));

    adjustment = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    /* disconnect to avoid infinite loops */
    g_signal_handlers_disconnect_by_func(G_OBJECT(adjustment),
                                         G_CALLBACK(value_changed), NULL);

    adjustment->step_increment = (gfloat) 1;
    adjustment->upper = (gfloat) max;
    adjustment->page_size = (gfloat) size;
    adjustment->page_increment = (gfloat) size;
    adjustment->value = (gfloat) pos;

    gtk_range_set_adjustment(GTK_RANGE(scrollbar), adjustment);

    gtk_adjustment_changed(adjustment);
    gtk_adjustment_value_changed(adjustment);

    /* reattach again */
    g_signal_connect(G_OBJECT
                     (gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
                     "value_changed", G_CALLBACK(value_changed), NULL);
}


static void gtk_drv_paint(mpdm_t doc, int optimize)
/* GTK document draw function */
{
    GdkRectangle gr;
    mpdm_t d = NULL;
    int n, m;

    if (maximize)
        gtk_window_maximize(GTK_WINDOW(window));

    /* no gc? create it */
    if (gc == NULL)
        gc = gdk_gc_new(area->window);

    if (font == NULL)
        build_fonts();

    if ((d = mp_draw(doc, optimize)) == NULL)
        return;

    mpdm_ref(d);

    gr.x = 0;
    gr.y = 0;
    gr.width = area->allocation.width;
    gr.height = font_height;

    for (n = 0; n < mpdm_size(d); n++) {
        PangoLayout *pl;
        PangoAttrList *pal;
        mpdm_t l = mpdm_aget(d, n);
        char *str = NULL;
        int u, p;

        if (l == NULL)
            continue;

        /* create the pango stuff */
        pl = gtk_widget_create_pango_layout(area, NULL);
        pango_layout_set_font_description(pl, font);
        pal = pango_attr_list_new();

        for (m = u = p = 0; m < mpdm_size(l); m++, u = p) {
            PangoAttribute *pa;
            int attr;
            mpdm_t s;
            char *ptr;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_aget(l, m++));
            s = mpdm_aget(l, m);

            /* convert the string to utf8 */
            ptr = v_to_utf8(s);

            /* add to the full line */
            str = mpdm_poke(str, &p, ptr, strlen(ptr), 1);

            g_free(ptr);

            /* create the background if it's
               different from the default */
            if (papers[attr].red != papers[normal_attr].red ||
                papers[attr].green != papers[normal_attr].green ||
                papers[attr].blue != papers[normal_attr].blue) {
                pa = pango_attr_background_new(papers[attr].red,
                                               papers[attr].green,
                                               papers[attr].blue);

                pa->start_index = u;
                pa->end_index = p;

                pango_attr_list_insert(pal, pa);
            }

            /* underline? */
            if (underlines[attr]) {
                pa = pango_attr_underline_new(TRUE);

                pa->start_index = u;
                pa->end_index = p;

                pango_attr_list_insert(pal, pa);
            }

            /* foreground color */
            pa = pango_attr_foreground_new(inks[attr].red,
                                           inks[attr].green,
                                           inks[attr].blue);

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
        gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, gr.width, gr.height);

        /* draw the text */
        gtk_paint_layout(area->style, pixmap,
                         GTK_STATE_NORMAL, TRUE, &gr, area, "", 2, 0, pl);

        /* dump the pixmap */
        gdk_draw_pixmap(area->window, gc, pixmap,
                        0, 0, 0, n * font_height, gr.width, gr.height);

        g_object_unref(pl);
    }

    mpdm_unref(d);

    draw_filetabs();
    draw_scrollbar();
    draw_status();
}


static void redraw(void)
{
    if (mpdm_size(mpdm_hget_s(mp, L"docs")))
        gtk_drv_paint(mp_active(), 0);
}


static gint delete_event(GtkWidget * w, GdkEvent * e, gpointer data)
/* 'delete_event' handler */
{
    mp_process_event(MPDM_LS(L"close-window"));

    return mp_exit_requested ? FALSE : TRUE;
}


static void destroy(GtkWidget * w, gpointer data)
/* 'destroy' handler */
{
    gtk_main_quit();
}


static gint key_release_event(GtkWidget * widget, GdkEventKey * event,
                              gpointer data)
/* 'key_release_event' handler */
{
    if (mp_keypress_throttle(0))
        gtk_drv_paint(mp_active(), 0);

    return 0;
}


static gint key_press_event(GtkWidget * widget, GdkEventKey * event,
                            gpointer data)
/* 'key_press_event' handler */
{
    wchar_t *ptr = NULL;

    gtk_im_context_filter_keypress(im, event);

    /* set mp.shift_pressed */
    if (event->state & (GDK_SHIFT_MASK))
        mpdm_hset_s(mp, L"shift_pressed", MPDM_I(1));

    /* reserve alt for menu mnemonics */
/*	if (GDK_MOD1_MASK & event->state)
		return(0);*/

    if (event->state & (GDK_CONTROL_MASK)) {
        switch (event->keyval) {
        case GDK_Up:
            ptr = L"ctrl-cursor-up";
            break;
        case GDK_Down:
            ptr = L"ctrl-cursor-down";
            break;
        case GDK_Left:
            ptr = L"ctrl-cursor-left";
            break;
        case GDK_Right:
            ptr = L"ctrl-cursor-right";
            break;
        case GDK_Prior:
            ptr = L"ctrl-page-up";
            break;
        case GDK_Next:
            ptr = L"ctrl-page-down";
            break;
        case GDK_Home:
            ptr = L"ctrl-home";
            break;
        case GDK_End:
            ptr = L"ctrl-end";
            break;
        case GDK_space:
            ptr = L"ctrl-space";
            break;
        case GDK_KP_Add:
            ptr = L"ctrl-kp-plus";
            break;
        case GDK_KP_Subtract:
            ptr = L"ctrl-kp-minus";
            break;
        case GDK_KP_Multiply:
            ptr = L"ctrl-kp-multiply";
            break;
        case GDK_KP_Divide:
            ptr = L"ctrl-kp-divide";
            break;
        case GDK_F1:
            ptr = L"ctrl-f1";
            break;
        case GDK_F2:
            ptr = L"ctrl-f2";
            break;
        case GDK_F3:
            ptr = L"ctrl-f3";
            break;
        case GDK_F4:
            ptr = L"ctrl-f4";
            break;
        case GDK_F5:
            ptr = L"ctrl-f5";
            break;
        case GDK_F6:
            ptr = L"ctrl-f6";
            break;
        case GDK_F7:
            ptr = L"ctrl-f7";
            break;
        case GDK_F8:
            ptr = L"ctrl-f8";
            break;
        case GDK_F9:
            ptr = L"ctrl-f9";
            break;
        case GDK_F10:
            ptr = L"ctrl-f10";
            break;
        case GDK_F11:
            ptr = L"ctrl-f11";
            break;
        case GDK_F12:
            ptr = L"ctrl-f12";
            break;
        case GDK_KP_Enter:
        case GDK_Return:
            ptr = L"ctrl-enter";
            break;
        case GDK_Cyrillic_ve:
            ptr = L"ctrl-d";
            break;
        case GDK_Cyrillic_a:
            ptr = L"ctrl-f";
            break;
        case GDK_Cyrillic_tse:
            ptr = L"ctrl-w";
            break;
        case GDK_Cyrillic_de:
            ptr = L"ctrl-l";
            break;
        case GDK_Cyrillic_ie:
            ptr = L"ctrl-t";
            break;
        case GDK_Cyrillic_ef:
            ptr = L"ctrl-a";
            break;
        case GDK_Cyrillic_ghe:
            ptr = L"ctrl-u";
            break;
        case GDK_Cyrillic_i:
            ptr = L"ctrl-b";
            break;
        case GDK_Cyrillic_shorti:
            ptr = L"ctrl-q";
            break;
        case GDK_Cyrillic_ka:
            ptr = L"ctrl-r";
            break;
        case GDK_Cyrillic_el:
            ptr = L"ctrl-k";
            break;
        case GDK_Cyrillic_em:
            ptr = L"ctrl-v";
            break;
        case GDK_Cyrillic_en:
            ptr = L"ctrl-y";
            break;
        case GDK_Cyrillic_o:
            ptr = L"ctrl-j";
            break;
        case GDK_Cyrillic_pe:
            ptr = L"ctrl-g";
            break;
        case GDK_Cyrillic_ya:
            ptr = L"ctrl-z";
            break;
        case GDK_Cyrillic_er:
            ptr = L"ctrl-h";
            break;
        case GDK_Cyrillic_es:
            ptr = L"ctrl-c";
            break;
        case GDK_Cyrillic_te:
            ptr = L"ctrl-n";
            break;
        case GDK_Cyrillic_softsign:
            ptr = L"ctrl-m";
            break;
        case GDK_Cyrillic_yeru:
            ptr = L"ctrl-s";
            break;
        case GDK_Cyrillic_ze:
            ptr = L"ctrl-p";
            break;
        case GDK_Cyrillic_sha:
            ptr = L"ctrl-i";
            break;
        case GDK_Cyrillic_e:
            ptr = L"ctrl-t";
            break;
        case GDK_Cyrillic_shcha:
            ptr = L"ctrl-o";
            break;
        case GDK_Cyrillic_che:
            ptr = L"ctrl-x";
            break;
        }

        if (ptr == NULL) {
            char c = event->keyval & 0xdf;

            switch (c) {
            case 'A':
                ptr = L"ctrl-a";
                break;
            case 'B':
                ptr = L"ctrl-b";
                break;
            case 'C':
                ptr = L"ctrl-c";
                break;
            case 'D':
                ptr = L"ctrl-d";
                break;
            case 'E':
                ptr = L"ctrl-e";
                break;
            case 'F':
                ptr = L"ctrl-f";
                break;
            case 'G':
                ptr = L"ctrl-g";
                break;
            case 'H':
                ptr = L"ctrl-h";
                break;
            case 'I':
                ptr = L"ctrl-i";
                break;
            case 'J':
                ptr = L"ctrl-j";
                break;
            case 'K':
                ptr = L"ctrl-k";
                break;
            case 'L':
                ptr = L"ctrl-l";
                break;
            case 'M':
                ptr = L"ctrl-m";
                break;
            case 'N':
                ptr = L"ctrl-n";
                break;
            case 'O':
                ptr = L"ctrl-o";
                break;
            case 'P':
                ptr = L"ctrl-p";
                break;
            case 'Q':
                ptr = L"ctrl-q";
                break;
            case 'R':
                ptr = L"ctrl-r";
                break;
            case 'S':
                ptr = L"ctrl-s";
                break;
            case 'T':
                ptr = L"ctrl-t";
                break;
            case 'U':
                ptr = L"ctrl-u";
                break;
            case 'V':
                ptr = L"ctrl-v";
                break;
            case 'W':
                ptr = L"ctrl-w";
                break;
            case 'X':
                ptr = L"ctrl-x";
                break;
            case 'Y':
                ptr = L"ctrl-y";
                break;
            case 'Z':
                ptr = L"ctrl-z";
                break;
            }
        }
    }
    else
    if (event->state & (GDK_MOD1_MASK)) {
        switch (event->keyval) {
        case GDK_Up:
            ptr = L"alt-cursor-up";
            break;
        case GDK_Down:
            ptr = L"alt-cursor-down";
            break;
        case GDK_Left:
            ptr = L"alt-cursor-left";
            break;
        case GDK_Right:
            ptr = L"alt-cursor-right";
            break;
        case GDK_Prior:
            ptr = L"alt-page-up";
            break;
        case GDK_Next:
            ptr = L"alt-page-down";
            break;
        case GDK_Home:
            ptr = L"alt-home";
            break;
        case GDK_End:
            ptr = L"alt-end";
            break;
        case GDK_space:
            ptr = L"alt-space";
            break;
        case GDK_KP_Add:
            ptr = L"alt-kp-plus";
            break;
        case GDK_KP_Subtract:
            ptr = L"alt-kp-minus";
            break;
        case GDK_KP_Multiply:
            ptr = L"alt-kp-multiply";
            break;
        case GDK_KP_Divide:
            ptr = L"alt-kp-divide";
            break;
        case GDK_F1:
            ptr = L"alt-f1";
            break;
        case GDK_F2:
            ptr = L"alt-f2";
            break;
        case GDK_F3:
            ptr = L"alt-f3";
            break;
        case GDK_F4:
            ptr = L"alt-f4";
            break;
        case GDK_F5:
            ptr = L"alt-f5";
            break;
        case GDK_F6:
            ptr = L"alt-f6";
            break;
        case GDK_F7:
            ptr = L"alt-f7";
            break;
        case GDK_F8:
            ptr = L"alt-f8";
            break;
        case GDK_F9:
            ptr = L"alt-f9";
            break;
        case GDK_F10:
            ptr = L"alt-f10";
            break;
        case GDK_F11:
            ptr = L"alt-f11";
            break;
        case GDK_F12:
            ptr = L"alt-f12";
            break;
        case GDK_KP_Enter:
        case GDK_Return:
            ptr = L"alt-enter";
            break;
        case GDK_Cyrillic_ve:
            ptr = L"alt-d";
            break;
        case GDK_Cyrillic_a:
            ptr = L"alt-f";
            break;
        case GDK_Cyrillic_tse:
            ptr = L"alt-w";
            break;
        case GDK_Cyrillic_de:
            ptr = L"alt-l";
            break;
        case GDK_Cyrillic_ie:
            ptr = L"alt-t";
            break;
        case GDK_Cyrillic_ef:
            ptr = L"alt-a";
            break;
        case GDK_Cyrillic_ghe:
            ptr = L"alt-u";
            break;
        case GDK_Cyrillic_i:
            ptr = L"alt-b";
            break;
        case GDK_Cyrillic_shorti:
            ptr = L"alt-q";
            break;
        case GDK_Cyrillic_ka:
            ptr = L"alt-r";
            break;
        case GDK_Cyrillic_el:
            ptr = L"alt-k";
            break;
        case GDK_Cyrillic_em:
            ptr = L"alt-v";
            break;
        case GDK_Cyrillic_en:
            ptr = L"alt-y";
            break;
        case GDK_Cyrillic_o:
            ptr = L"alt-j";
            break;
        case GDK_Cyrillic_pe:
            ptr = L"alt-g";
            break;
        case GDK_Cyrillic_ya:
            ptr = L"alt-z";
            break;
        case GDK_Cyrillic_er:
            ptr = L"alt-h";
            break;
        case GDK_Cyrillic_es:
            ptr = L"alt-c";
            break;
        case GDK_Cyrillic_te:
            ptr = L"alt-n";
            break;
        case GDK_Cyrillic_softsign:
            ptr = L"alt-m";
            break;
        case GDK_Cyrillic_yeru:
            ptr = L"alt-s";
            break;
        case GDK_Cyrillic_ze:
            ptr = L"alt-p";
            break;
        case GDK_Cyrillic_sha:
            ptr = L"alt-i";
            break;
        case GDK_Cyrillic_e:
            ptr = L"alt-t";
            break;
        case GDK_Cyrillic_shcha:
            ptr = L"alt-o";
            break;
        case GDK_Cyrillic_che:
            ptr = L"alt-x";
            break;
        }

        if (ptr == NULL) {
            char c = event->keyval & 0xdf;

            switch (c) {
            case 'A':
                ptr = L"alt-a";
                break;
            case 'B':
                ptr = L"alt-b";
                break;
            case 'C':
                ptr = L"alt-c";
                break;
            case 'D':
                ptr = L"alt-d";
                break;
            case 'E':
                ptr = L"alt-e";
                break;
            case 'F':
                ptr = L"alt-f";
                break;
            case 'G':
                ptr = L"alt-g";
                break;
            case 'H':
                ptr = L"alt-h";
                break;
            case 'I':
                ptr = L"alt-i";
                break;
            case 'J':
                ptr = L"alt-j";
                break;
            case 'K':
                ptr = L"alt-k";
                break;
            case 'L':
                ptr = L"alt-l";
                break;
            case 'M':
                ptr = L"alt-m";
                break;
            case 'N':
                ptr = L"alt-n";
                break;
            case 'O':
                ptr = L"alt-o";
                break;
            case 'P':
                ptr = L"alt-p";
                break;
            case 'Q':
                ptr = L"alt-q";
                break;
            case 'R':
                ptr = L"alt-r";
                break;
            case 'S':
                ptr = L"alt-s";
                break;
            case 'T':
                ptr = L"alt-t";
                break;
            case 'U':
                ptr = L"alt-u";
                break;
            case 'V':
                ptr = L"alt-v";
                break;
            case 'W':
                ptr = L"alt-w";
                break;
            case 'X':
                ptr = L"alt-x";
                break;
            case 'Y':
                ptr = L"alt-y";
                break;
            case 'Z':
                ptr = L"alt-z";
                break;
            }
        }
    }
    else {
        switch (event->keyval) {
        case GDK_Up:
            ptr = L"cursor-up";
            break;
        case GDK_Down:
            ptr = L"cursor-down";
            break;
        case GDK_Left:
            ptr = L"cursor-left";
            break;
        case GDK_Right:
            ptr = L"cursor-right";
            break;
        case GDK_Prior:
            ptr = L"page-up";
            break;
        case GDK_Next:
            ptr = L"page-down";
            break;
        case GDK_Home:
            ptr = L"home";
            break;
        case GDK_End:
            ptr = L"end";
            break;
        case GDK_space:
            ptr = L"space";
            break;
        case GDK_KP_Add:
            ptr = L"kp-plus";
            break;
        case GDK_KP_Subtract:
            ptr = L"kp-minus";
            break;
        case GDK_KP_Multiply:
            ptr = L"kp-multiply";
            break;
        case GDK_KP_Divide:
            ptr = L"kp-divide";
            break;
        case GDK_F1:
            ptr = L"f1";
            break;
        case GDK_F2:
            ptr = L"f2";
            break;
        case GDK_F3:
            ptr = L"f3";
            break;
        case GDK_F4:
            ptr = L"f4";
            break;
        case GDK_F5:
            ptr = L"f5";
            break;
        case GDK_F6:
            ptr = L"f6";
            break;
        case GDK_F7:
            ptr = L"f7";
            break;
        case GDK_F8:
            ptr = L"f8";
            break;
        case GDK_F9:
            ptr = L"f9";
            break;
        case GDK_F10:
            ptr = L"f10";
            break;
        case GDK_F11:
            ptr = L"f11";
            break;
        case GDK_F12:
            ptr = L"f12";
            break;
        case GDK_Insert:
            ptr = L"insert";
            break;
        case GDK_BackSpace:
            ptr = L"backspace";
            break;
        case GDK_Delete:
            ptr = L"delete";
            break;
        case GDK_KP_Enter:
        case GDK_Return:
            ptr = L"enter";
            break;
        case GDK_Tab:
            ptr = L"tab";
            break;
        case GDK_ISO_Left_Tab:
            ptr = L"shift-tab";
            break;
        case GDK_Escape:
            ptr = L"escape";
            break;
        }
    }

    /* if there is a pending char in im_char, use it */
    if (ptr == NULL && im_char[0] != L'\0')
        ptr = im_char;

    /* finally process */
    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    /* delete the pending char */
    im_char[0] = L'\0';

    if (mp_exit_requested)
        gtk_main_quit();

    if (mp_keypress_throttle(1))
        gtk_drv_paint(mp_active(), 1);

    return 0;
}


static gint button_press_event(GtkWidget * widget, GdkEventButton * event,
                               gpointer data)
/* 'button_press_event' handler (mouse buttons) */
{
    int x, y;
    wchar_t *ptr = NULL;

    mouse_down = 1;

    /* mouse instant positioning */
    x = ((int) event->x) / font_width;
    y = ((int) event->y) / font_height;

    mpdm_hset_s(mp, L"mouse_x", MPDM_I(x));
    mpdm_hset_s(mp, L"mouse_y", MPDM_I(y));

    switch (event->button) {
    case 1:
        ptr = L"mouse-left-button";
        break;
    case 2:
        ptr = L"mouse-middle-button";
        break;
    case 3:
        ptr = L"mouse-right-button";
        break;
    case 4:
        ptr = L"mouse-wheel-up";
        break;
    case 5:
        ptr = L"mouse-wheel-down";
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    redraw();

    return 0;
}


static gint button_release_event(GtkWidget * widget,
                                 GdkEventButton * event, gpointer data)
/* 'button_release_event' handle (mouse buttons) */
{
    mouse_down = 0;

    return TRUE;
}


static gint motion_notify_event(GtkWidget * widget, GdkEventMotion * event,
                                gpointer data)
/* 'motion_notify_event' handler (mouse movement) */
{
    static int ox = 0;
    static int oy = 0;

    if (mouse_down) {
        int x, y;

        /* mouse dragging */
        x = ((int) event->x) / font_width;
        y = ((int) event->y) / font_height;

        if (ox != x && oy != y) {
            mpdm_hset_s(mp, L"mouse_to_x", MPDM_I(x));
            mpdm_hset_s(mp, L"mouse_to_y", MPDM_I(y));

            mp_process_event(MPDM_LS(L"mouse-drag"));
            gtk_drv_paint(mp_active(), 1);
        }
    }

    return TRUE;
}


static void drag_data_received(GtkWidget * widget, GdkDragContext * dc,
                               gint x, gint y, GtkSelectionData * data,
                               guint info, guint time)
/* 'drag_data_received' handler */
{
    printf("drag_data_received (unsupported)\n");
}


/** clipboard functions **/

static void commit(GtkIMContext * i, char *str, gpointer u)
/* 'commit' handler */
{
    wchar_t *wstr;

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

    return FALSE;
}


static gint configure_event(GtkWidget * widget, GdkEventConfigure * event)
/* 'configure_event' handler */
{
    static GdkEventConfigure o;

    if (memcmp(&o, event, sizeof(o)) == 0)
        return TRUE;

    memcpy(&o, event, sizeof(o));

    update_window_size();
    redraw();

    return TRUE;
}


static gint selection_clear_event(GtkWidget * widget,
                                  GdkEventSelection * event, gpointer data)
/* 'selection_clear_event' handler */
{
    got_selection = 0;

    return TRUE;
}


static void selection_get(GtkWidget * widget,
                          GtkSelectionData * sel, guint info, guint tm)
/* 'selection_get' handler */
{
    mpdm_t d;
    unsigned char *ptr;
    int s;

    if (!got_selection)
        return;

    /* gets the clipboard and joins */
    d = mpdm_hget_s(mp, L"clipboard");

    if (mpdm_size(d) == 0)
        return;

    d = mpdm_ref(mpdm_join_s(d, L"\n"));

    /* convert to current locale */
    ptr = (unsigned char *) mpdm_wcstombs(d->data, &s);

    /* pastes into primary selection */
    gtk_selection_data_set(sel, GDK_SELECTION_TYPE_STRING, 8, ptr,
                           (gsize) s);

    free(ptr);

    mpdm_unref(d);
}


static void selection_received(GtkWidget * widget,
                               GtkSelectionData * sel, gpointer data)
/* 'selection_received' handler */
{
    mpdm_t d;

    if (sel->data != NULL) {
        /* get selection */
        wchar_t *wptr = utf8_to_wcs((char *) sel->data);
        d = MPDM_S(wptr);
        g_free(wptr);

        /* split and set as the clipboard */
        mpdm_hset_s(mp, L"clipboard", mpdm_split_s(d, L"\n"));
        mpdm_hset_s(mp, L"clipboard_vertical", MPDM_I(0));

        /* wait no more for the selection */
        wait_for_selection = 0;
    }
    else
        wait_for_selection = -1;
}


static mpdm_t gtk_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
/* driver-dependent mp to system clipboard */
{
    got_selection = gtk_selection_owner_set(area,
                                            GDK_SELECTION_PRIMARY,
                                            GDK_CURRENT_TIME);

    return NULL;
}


static mpdm_t gtk_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
/* driver-dependent system to mp clipboard */
{
    if (!got_selection) {
        int n;
        char *formats[] = { "UTF8_STRING", "STRING", NULL };

        for (n = 0; formats[n] != NULL; n++) {

            /* triggers a selection capture */
            if (gtk_selection_convert(area, GDK_SELECTION_PRIMARY,
                                      gdk_atom_intern(formats[n], FALSE),
                                      GDK_CURRENT_TIME)) {

                /* processes the pending events
                   (i.e., the 'selection_received' handler) */
                wait_for_selection = 1;

                while (wait_for_selection == 1)
                    gtk_main_iteration();

                if (!wait_for_selection)
                    break;
            }
        }
    }

    return NULL;
}


/** interface functions **/

static void clicked_ok(GtkWidget * widget, gpointer data)
/* 'clicked_on' signal handler (for gtk_drv_form) */
{
    int n;

    for (n = 0; n < mpdm_size(form_args); n++) {
        GtkWidget *widget = form_widgets[n];
        mpdm_t w = mpdm_aget(form_args, n);
        wchar_t *wptr = mpdm_string(mpdm_hget_s(w, L"type"));
        mpdm_t v = NULL;

        /* if there is already a value there, if was
           previously set from a callback */
        if (mpdm_aget(form_values, n) != NULL)
            continue;

        if (wcscmp(wptr, L"text") == 0 || wcscmp(wptr, L"password") == 0) {
            char *ptr;
            GtkWidget *gw = widget;
            mpdm_t h;

            if (wcscmp(wptr, L"text") == 0)
                gw = GTK_COMBO(widget)->entry;

            if ((ptr =
                 gtk_editable_get_chars(GTK_EDITABLE(gw), 0, -1)) != NULL
                && (wptr = utf8_to_wcs(ptr)) != NULL) {
                v = MPDM_S(wptr);
                g_free(wptr);
                g_free(ptr);
            }

            mpdm_ref(v);

            /* if it has history, fill it */
            if ((h = mpdm_hget_s(w, L"history")) != NULL &&
                v != NULL && mpdm_cmp_s(v, L"") != 0) {
                h = mp_get_history(h);

                if (mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
                    mpdm_push(h, v);
            }

            mpdm_unrefnd(v);
        }
        else
        if (wcscmp(wptr, L"checkbox") == 0) {
            v = MPDM_I(gtk_toggle_button_get_active
                       (GTK_TOGGLE_BUTTON(widget)));
        }
        else
        if (wcscmp(wptr, L"list") == 0) {
            GtkWidget *list = gtk_bin_get_child(GTK_BIN(widget));
            GtkTreeSelection *selection =
                gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
            GList *selected =
                gtk_tree_selection_get_selected_rows(selection, NULL);
            GtkTreePath *path = selected->data;

            v = MPDM_I(gtk_tree_path_get_indices(path)[0]);
            gtk_tree_path_free(path);
            g_list_free(selected);

        }

        mpdm_aset(form_values, v, n);
    }
}


static gint timer_callback(gpointer data)
{
    mpdm_void(mpdm_exec(timer_func, NULL, NULL));
    redraw();

    return TRUE;
}


static void build_form_data(mpdm_t widget_list)
/* builds the necessary information for a list of widgets */
{
    mpdm_unref(form_args);
    form_args = mpdm_ref(widget_list);

    mpdm_unref(form_values);
    form_values = widget_list == NULL ? NULL :
        mpdm_ref(MPDM_A(mpdm_size(form_args)));

    /* resize the widget array */
    form_widgets = (GtkWidget **) realloc(form_widgets,
                                          mpdm_size(form_args) *
                                          sizeof(GtkWidget *));
}


/** dialog functions **/

#define DIALOG_BUTTON(l,f) do { GtkWidget * btn; \
	ptr = localize(l); btn = gtk_button_new_with_label(ptr); \
	g_signal_connect_swapped(G_OBJECT(btn), "clicked", \
		G_CALLBACK(f), G_OBJECT(dlg)); \
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->action_area), \
		btn, TRUE, TRUE, 0); \
	g_free(ptr); \
	} while (0);

static mpdm_t gtk_drv_alert(mpdm_t a, mpdm_t ctxt)
/* alert driver function */
{
    gchar *ptr;
    GtkWidget *dlg;

    build_form_data(NULL);

    /* 1# arg: prompt */
    if ((ptr = v_to_utf8(mpdm_aget(a, 0))) == NULL)
        return NULL;

    dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, ptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
    g_free(ptr);

    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return NULL;
}


static mpdm_t gtk_drv_confirm(mpdm_t a, mpdm_t ctxt)
/* confirm driver function */
{
    char *ptr;
    GtkWidget *dlg;
    gint response;

    build_form_data(NULL);

    /* 1# arg: prompt */
    if ((ptr = v_to_utf8(mpdm_aget(a, 0))) == NULL)
        return NULL;

    dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                 ptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
    g_free(ptr);

    gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_YES, 1);
    gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_NO, 2);
    gtk_dialog_add_button(GTK_DIALOG(dlg), GTK_STOCK_CANCEL, 0);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = 0;

    return MPDM_I(response);
}


static mpdm_t gtk_drv_form(mpdm_t a, mpdm_t ctxt)
/* 'form' driver function */
{
    GtkWidget *dlg;
    GtkWidget *table;
    GtkWidget *content_area;
    int n;
    mpdm_t ret = NULL;

    /* first argument: list of widgets */
    build_form_data(mpdm_aget(a, 0));

    dlg = gtk_dialog_new_with_buttons("mp " VERSION, GTK_WINDOW(window),
                                      GTK_DIALOG_MODAL |
                                      GTK_DIALOG_NO_SEPARATOR,
                                      GTK_STOCK_CANCEL,
                                      GTK_RESPONSE_CANCEL, GTK_STOCK_OK,
                                      GTK_RESPONSE_OK, NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 5);

    content_area = GTK_DIALOG(dlg)->vbox;
    gtk_box_set_spacing(GTK_BOX(content_area), 2);

    table = gtk_table_new(mpdm_size(a), 2, FALSE);
    gtk_container_set_border_width(GTK_CONTAINER(table), 5);
    gtk_table_set_col_spacings(GTK_TABLE(table), 12);
    gtk_table_set_row_spacings(GTK_TABLE(table), 6);

    for (n = 0; n < mpdm_size(form_args); n++) {
        mpdm_t w = mpdm_aget(form_args, n);
        GtkWidget *widget = NULL;
        wchar_t *type;
        char *ptr;
        mpdm_t t;
        int col = 0;

        type = mpdm_string(mpdm_hget_s(w, L"type"));

        if ((t = mpdm_hget_s(w, L"label")) != NULL) {
            GtkWidget *label;

            if ((ptr = v_to_utf8(mpdm_gettext(t))) != NULL) {
                label = gtk_label_new(ptr);
                gtk_misc_set_alignment(GTK_MISC(label), 0, .5);

                gtk_table_attach_defaults(GTK_TABLE(table),
                                          label, 0, wcscmp(type,
                                                           L"label") ==
                                          0 ? 2 : 1, n, n + 1);

                g_free(ptr);

                col++;
            }
        }

        t = mpdm_hget_s(w, L"value");

        if (wcscmp(type, L"text") == 0) {
            GList *combo_items = NULL;
            mpdm_t h;

            widget = gtk_combo_new();
            gtk_widget_set_size_request(widget, 300, -1);
            gtk_combo_set_use_arrows_always(GTK_COMBO(widget), TRUE);
            gtk_combo_set_case_sensitive(GTK_COMBO(widget), TRUE);
            gtk_entry_set_activates_default(GTK_ENTRY
                                            (GTK_COMBO(widget)->entry),
                                            TRUE);

            if ((h = mpdm_hget_s(w, L"history")) != NULL) {
                int i;

                /* has history; fill it */
                h = mp_get_history(h);

                for (i = 0; i < mpdm_size(h); i++) {
                    ptr = v_to_utf8(mpdm_aget(h, i));

                    combo_items = g_list_prepend(combo_items, ptr);
                }
            }

            if (t != NULL) {
                ptr = v_to_utf8(t);

                combo_items = g_list_prepend(combo_items, ptr);
            }

            gtk_combo_set_popdown_strings(GTK_COMBO(widget), combo_items);
            g_list_free(combo_items);
        }
        else
    if (wcscmp(type, L"password") == 0) {
            widget = gtk_entry_new();
            gtk_widget_set_size_request(widget, 300, -1);
            gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            widget = gtk_check_button_new();

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
                                         mpdm_ival(t) ? TRUE : FALSE);
        }
        else
        if (wcscmp(type, L"list") == 0) {
            GtkWidget *list;
            GtkListStore *list_store;
            GtkCellRenderer *renderer;
            GtkTreeViewColumn *column;
            GtkTreePath *path;
            mpdm_t l;
            gint i;

            if ((i = 450 / mpdm_size(form_args)) < 100)
                i = 100;

            widget = gtk_scrolled_window_new(NULL, NULL);
            gtk_widget_set_size_request(widget, 500, i);
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
                                           GTK_POLICY_NEVER,
                                           GTK_POLICY_AUTOMATIC);
            gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
                                                (widget), GTK_SHADOW_IN);

            list_store = gtk_list_store_new(1, G_TYPE_STRING);
            list =
                gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
            gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);
            renderer = gtk_cell_renderer_text_new();
            column = gtk_tree_view_column_new_with_attributes("", renderer,
                                                              "text", 0,
                                                              NULL);
            gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);
            gtk_container_add(GTK_CONTAINER(widget), list);

            l = mpdm_hget_s(w, L"list");

            for (i = 0; i < mpdm_size(l); i++) {
                if ((ptr = v_to_utf8(mpdm_aget(l, i))) != NULL) {
                    GtkTreeIter iter;
                    gtk_list_store_append(list_store, &iter);
                    gtk_list_store_set(list_store, &iter, 0, ptr, -1);
                    g_free(ptr);
                }
            }

            /* initial position */
            i = mpdm_ival(t);

            path = gtk_tree_path_new_from_indices(i, -1);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL,
                                     FALSE);
            gtk_tree_path_free(path);

            g_signal_connect_swapped(G_OBJECT(list), "row-activated",
                                     G_CALLBACK
                                     (gtk_window_activate_default), dlg);
        }

        if (widget != NULL) {
            form_widgets[n] = widget;
            gtk_table_attach_defaults(GTK_TABLE(table),
                                      widget, col, 2, n, n + 1);
        }
    }

    gtk_widget_show_all(table);

    gtk_box_pack_start(GTK_BOX(content_area), table, TRUE, TRUE, 0);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK) {
        clicked_ok(NULL, NULL);
        ret = form_values;
    }
    gtk_widget_destroy(dlg);

    return ret;
}


static mpdm_t run_filechooser(mpdm_t a, gboolean save)
/* openfile driver function */
{
    GtkWidget *dlg;
    char *ptr;
    mpdm_t ret = NULL;
    gint response;

    /* 1# arg: prompt */
    if ((ptr = v_to_utf8(mpdm_aget(a, 0))) == NULL)
        return (NULL);

    if (!save) {
        dlg = gtk_file_chooser_dialog_new(ptr, GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);
    }
    else {
        dlg = gtk_file_chooser_dialog_new(ptr, GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL,
                                          GTK_STOCK_CANCEL, GTK_STOCK_OK,
                                          GTK_RESPONSE_OK, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER
                                                       (dlg), TRUE);
    }
    g_free(ptr);

    build_form_data(NULL);

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dlg), TRUE);
    response = gtk_dialog_run(GTK_DIALOG(dlg));

    if (response == GTK_RESPONSE_OK) {
        gchar *filename;
        gchar *utf8name;
        wchar_t *wfilename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        utf8name = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
        g_free(filename);
        wfilename = utf8_to_wcs(utf8name);
        g_free(utf8name);
        ret = MPDM_S(wfilename);
        g_free(wfilename);
    }
    gtk_widget_destroy(dlg);

    return ret;
}


static mpdm_t gtk_drv_openfile(mpdm_t a, mpdm_t ctxt)
/* openfile driver function */
{
    return run_filechooser(a, FALSE);
}


static mpdm_t gtk_drv_savefile(mpdm_t a, mpdm_t ctxt)
/* savefile driver function */
{
    return run_filechooser(a, TRUE);
}


static mpdm_t gtk_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    build_fonts();
    build_colors();
    build_menu();

    redraw();

    return NULL;
}


static mpdm_t gtk_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    static guint prev = 0;
    int msecs = mpdm_ival(mpdm_aget(a, 0));
    mpdm_t func = mpdm_aget(a, 1);

    /* previously defined one? remove */
    if (timer_func != NULL)
        gtk_timeout_remove(prev);

    /* if msecs and func are set, program timer */
    if (msecs > 0 && func != NULL)
        prev = gtk_timeout_add(msecs, timer_callback, NULL);

    mpdm_ref(func);
    mpdm_unref(timer_func);
    timer_func = func;

    return NULL;
}


static mpdm_t gtk_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    int onoff = mpdm_ival(mpdm_aget(a, 0));

    gdk_window_set_cursor(window->window,
                          gdk_cursor_new(onoff ? GDK_WATCH :
                                         GDK_LEFT_PTR));

    while (gtk_events_pending())
        gtk_main_iteration();

    return NULL;
}


static mpdm_t gtk_drv_main_loop(mpdm_t a, mpdm_t ctxt)
/* main loop */
{
    if (!mp_exit_requested) {
        gtk_drv_paint(mp_active(), 0);

        gtk_main();
    }

    return NULL;
}


static mpdm_t gtk_drv_shutdown(mpdm_t a, mpdm_t ctxt)
/* shutdown */
{
    mpdm_t v;

    if ((v = mpdm_hget_s(mp, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static void register_functions(void)
{
    mpdm_t drv;

    drv = mpdm_hget_s(mp, L"drv");
    mpdm_hset_s(drv, L"main_loop",   MPDM_X(gtk_drv_main_loop));
    mpdm_hset_s(drv, L"shutdown",    MPDM_X(gtk_drv_shutdown));
    mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(gtk_drv_clip_to_sys));
    mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(gtk_drv_sys_to_clip));
    mpdm_hset_s(drv, L"update_ui",   MPDM_X(gtk_drv_update_ui));
    mpdm_hset_s(drv, L"timer",       MPDM_X(gtk_drv_timer));
    mpdm_hset_s(drv, L"busy",        MPDM_X(gtk_drv_busy));
    mpdm_hset_s(drv, L"alert",       MPDM_X(gtk_drv_alert));
    mpdm_hset_s(drv, L"confirm",     MPDM_X(gtk_drv_confirm));
    mpdm_hset_s(drv, L"openfile",    MPDM_X(gtk_drv_openfile));
    mpdm_hset_s(drv, L"savefile",    MPDM_X(gtk_drv_savefile));
    mpdm_hset_s(drv, L"form",        MPDM_X(gtk_drv_form));
}


static mpdm_t gtk_drv_startup(mpdm_t a, mpdm_t ctxt)
/* driver initialization */
{
    GtkWidget *vbox;
    GtkWidget *hbox;
    GdkPixmap *pixmap;
    GdkPixmap *mask;
    GdkScreen *screen;
    mpdm_t v;
    int w, h;
    GtkTargetEntry targets[] = {
        {"text/plain", 0, 0},
        {"text/uri-list", 0, 1}
    };

    register_functions();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), "mp " VERSION);

    /* get real screen and pick a usable size for the main area */
    screen = gtk_window_get_screen(GTK_WINDOW(window));
    w = (gdk_screen_get_width(screen) * 3) / 4;
    h = (gdk_screen_get_height(screen) * 2) / 3;

    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(delete_event), NULL);

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(destroy), NULL);

    /* file tabs */
    file_tabs = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(file_tabs), GTK_POS_TOP);
    GTK_WIDGET_UNSET_FLAGS(file_tabs, GTK_CAN_FOCUS);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(file_tabs), 1);

    vbox = gtk_vbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    build_menu();

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), file_tabs, TRUE, TRUE, 0);

    gtk_notebook_popup_enable(GTK_NOTEBOOK(file_tabs));

    /* horizontal box holding the text and the scrollbar */
    hbox = gtk_hbox_new(FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /* the Minimum Profit area */
    area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(hbox), area, TRUE, TRUE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(area), w, h);
    gtk_widget_set_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK
                          | GDK_LEAVE_NOTIFY_MASK);

    gtk_widget_set_double_buffered(area, FALSE);

    g_signal_connect(G_OBJECT(area), "configure_event",
                     G_CALLBACK(configure_event), NULL);

    g_signal_connect(G_OBJECT(area), "expose_event",
                     G_CALLBACK(expose_event), NULL);

    g_signal_connect(G_OBJECT(area), "realize", G_CALLBACK(realize), NULL);

    g_signal_connect(G_OBJECT(window), "key_press_event",
                     G_CALLBACK(key_press_event), NULL);

    g_signal_connect(G_OBJECT(window), "key_release_event",
                     G_CALLBACK(key_release_event), NULL);

    g_signal_connect(G_OBJECT(area), "button_press_event",
                     G_CALLBACK(button_press_event), NULL);

    g_signal_connect(G_OBJECT(area), "button_release_event",
                     G_CALLBACK(button_release_event), NULL);

    g_signal_connect(G_OBJECT(area), "motion_notify_event",
                     G_CALLBACK(motion_notify_event), NULL);

    g_signal_connect(G_OBJECT(area), "selection_clear_event",
                     G_CALLBACK(selection_clear_event), NULL);

    g_signal_connect(G_OBJECT(area), "selection_get",
                     G_CALLBACK(selection_get), NULL);

    g_signal_connect(G_OBJECT(area), "selection_received",
                     G_CALLBACK(selection_received), NULL);

    g_signal_connect(G_OBJECT(area), "scroll_event",
                     G_CALLBACK(scroll_event), NULL);

    gtk_drag_dest_set(area, GTK_DEST_DEFAULT_ALL, targets,
                      sizeof(targets) / sizeof(GtkTargetEntry),
                      GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(area), "drag_data_received",
                     G_CALLBACK(drag_data_received), NULL);

    gtk_selection_add_target(area, GDK_SELECTION_PRIMARY,
                             GDK_SELECTION_TYPE_STRING, 1);

    g_signal_connect(G_OBJECT(file_tabs), "switch_page",
                     G_CALLBACK(switch_page), NULL);

    /* the scrollbar */
    scrollbar = gtk_vscrollbar_new(NULL);
    gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT
                     (gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
                     "value_changed", G_CALLBACK(value_changed), NULL);

    /* the status bar */
    status = gtk_label_new("mp " VERSION);
    gtk_box_pack_start(GTK_BOX(vbox), status, FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(status), 0, 0.5);
    gtk_label_set_justify(GTK_LABEL(status), GTK_JUSTIFY_LEFT);

    gtk_widget_show_all(window);

    /* set application icon */
    pixmap = gdk_pixmap_create_from_xpm_d(window->window,
                                          &mask, NULL, mp_xpm);
    gdk_window_set_icon(window->window, NULL, pixmap, mask);

    build_colors();

    if ((v = mpdm_hget_s(mp, L"config")) != NULL &&
        mpdm_ival(mpdm_hget_s(v, L"maximize")) > 0)
        maximize = 1;

    return NULL;
}


int gtk_drv_detect(int *argc, char ***argv)
{
    mpdm_t drv;
    int n;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-txt") == 0 ||
            strcmp(argv[0][n], "-h") == 0)
            return 0;
    }

    if (!gtk_init_check(argc, argv))
        return 0;

    drv = mpdm_hget_s(mp, L"drv");
    mpdm_hset_s(drv, L"id", MPDM_LS(L"gtk"));
    mpdm_hset_s(drv, L"startup", MPDM_X(gtk_drv_startup));

    return 1;
}

#endif                          /* CONFOPT_GTK */
