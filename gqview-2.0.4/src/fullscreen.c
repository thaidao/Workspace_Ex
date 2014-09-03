/*
 * GQview
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "gqview.h"
#include "fullscreen.h"

#include "image.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_misc.h"


enum {
	FULLSCREEN_CURSOR_HIDDEN = 1 << 0,
	FULLSCREEN_CURSOR_NORMAL = 1 << 1,
	FULLSCREEN_CURSOR_BUSY   = 1 << 2
};


/*
 *----------------------------------------------------------------------------
 * full screen functions
 *----------------------------------------------------------------------------
 */

static void clear_mouse_cursor(GtkWidget *widget, gint state)
{
	if (!widget->window) return;

	if (state & FULLSCREEN_CURSOR_BUSY)
		{
		GdkCursor *cursor;

		cursor = gdk_cursor_new(GDK_WATCH);
		gdk_window_set_cursor (widget->window, cursor);
		gdk_cursor_unref(cursor);
		}
	else if (state & FULLSCREEN_CURSOR_NORMAL)
		{
		gdk_window_set_cursor (widget->window, NULL);
		}
	else
		{
		GdkCursor *cursor;
		GdkPixmap *p;

		p = gdk_bitmap_create_from_data(widget->window, "\0\0\0", 1, 1);

		cursor = gdk_cursor_new_from_pixmap(p, p,
						    &widget->style->fg[GTK_STATE_ACTIVE],
						    &widget->style->bg[GTK_STATE_ACTIVE],
						    0, 0);

		gdk_window_set_cursor (widget->window, cursor);

		gdk_cursor_unref(cursor);
		g_object_unref(p);
		}
}

static gint fullscreen_hide_mouse_cb(gpointer data)
{
	FullScreenData *fs = data;

	if (fs->hide_mouse_id == -1) return FALSE;

	fs->cursor_state &= ~FULLSCREEN_CURSOR_NORMAL;
	if (!(fs->cursor_state & FULLSCREEN_CURSOR_BUSY)) clear_mouse_cursor(fs->window, fs->cursor_state);

	fs->hide_mouse_id = -1;
	return FALSE;
}

static void fullscreen_hide_mouse_disable(FullScreenData *fs)
{
	if (fs->hide_mouse_id != -1)
		{
		g_source_remove(fs->hide_mouse_id);
		fs->hide_mouse_id = -1;
		}
}

static void fullscreen_hide_mouse_reset(FullScreenData *fs)
{
	fullscreen_hide_mouse_disable(fs);
	fs->hide_mouse_id = g_timeout_add(FULL_SCREEN_HIDE_MOUSE_DELAY, fullscreen_hide_mouse_cb, fs);
}

static gint fullscreen_mouse_moved(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	FullScreenData *fs = data;

	if (!(fs->cursor_state & FULLSCREEN_CURSOR_NORMAL))
		{
		fs->cursor_state |= FULLSCREEN_CURSOR_NORMAL;
		if (!(fs->cursor_state & FULLSCREEN_CURSOR_BUSY)) clear_mouse_cursor(fs->window, fs->cursor_state);
		}
	fullscreen_hide_mouse_reset(fs);

	return FALSE;
}

static void fullscreen_busy_mouse_disable(FullScreenData *fs)
{
	if (fs->busy_mouse_id != -1)
		{
		g_source_remove(fs->busy_mouse_id);
		fs->busy_mouse_id = -1;
		}
}

static void fullscreen_mouse_set_busy(FullScreenData *fs, gint busy)
{
	fullscreen_busy_mouse_disable(fs);

	if ((fs->cursor_state & FULLSCREEN_CURSOR_BUSY) == (busy)) return;

	if (busy)
		{
		fs->cursor_state |= FULLSCREEN_CURSOR_BUSY;
		}
	else
		{
		fs->cursor_state &= ~FULLSCREEN_CURSOR_BUSY;
		}

	clear_mouse_cursor(fs->window, fs->cursor_state);
}

static gboolean fullscreen_mouse_set_busy_cb(gpointer data)
{
	FullScreenData *fs = data;

	fs->busy_mouse_id = -1;
	fullscreen_mouse_set_busy(fs, TRUE);
	return FALSE;
}

static void fullscreen_mouse_set_busy_idle(FullScreenData *fs)
{
	if (fs->busy_mouse_id == -1)
		{
		fs->busy_mouse_id = g_timeout_add(FULL_SCREEN_BUSY_MOUSE_DELAY,
						  fullscreen_mouse_set_busy_cb, fs);
		}
}

static void fullscreen_image_update_cb(ImageWindow *imd, gpointer data)
{
	FullScreenData *fs = data;

	if (fs->imd->il &&
	    fs->imd->pixbuf != fs->imd->il->pixbuf)
		{
		fullscreen_mouse_set_busy_idle(fs);
		}
}

static void fullscreen_image_complete_cb(ImageWindow *imd, gint preload, gpointer data)
{
	FullScreenData *fs = data;

	if (!preload) fullscreen_mouse_set_busy(fs, FALSE);
}

#define XSCREENSAVER_BINARY	"xscreensaver-command"
#define XSCREENSAVER_COMMAND	"xscreensaver-command -deactivate >&- 2>&- &"

static void fullscreen_saver_deactivate(void)
{
	static gint checked = FALSE;
	static gint found = FALSE;

	if (!checked)
		{
		checked = TRUE;
		found = file_in_path(XSCREENSAVER_BINARY);
		}

	if (found)
		{
		system (XSCREENSAVER_COMMAND);
		}
}

static gboolean fullscreen_saver_block_cb(gpointer data)
{
	if (fullscreen_disable_saver)
		{
		fullscreen_saver_deactivate();
		}

	return TRUE;
}

static gint fullscreen_delete_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	FullScreenData *fs = data;

	fullscreen_stop(fs);
	return TRUE;
}

FullScreenData *fullscreen_start(GtkWidget *window, ImageWindow *imd,
				 void (*stop_func)(FullScreenData *, gpointer), gpointer stop_data)
{
	FullScreenData *fs;
	GdkScreen *screen;
	gint same;
	gint x, y;
	gint w, h;
	GdkGeometry geometry;

	if (!window || !imd) return NULL;

	fs = g_new0(FullScreenData, 1);

	fs->hide_mouse_id = -1;
	fs->busy_mouse_id = -1;
	fs->cursor_state = FULLSCREEN_CURSOR_HIDDEN;

	fs->normal_window = window;
	fs->normal_imd = imd;

	fs->stop_func = stop_func;
	fs->stop_data = stop_data;

	if (debug) printf("full screen requests screen %d\n", fullscreen_screen);
	fullscreen_prefs_get_geometry(fullscreen_screen, window, &x, &y, &w, &h,
				      &screen, &same);

	fs->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(fs->window), "fullscreen", "GQview");

	/* this requests no decorations, if you still have them complain to the window manager author(s) */
	gtk_window_set_decorated(GTK_WINDOW(fs->window), FALSE);

	if (fullscreen_screen < 0)
		{
		/* If we want control of the window size and position this is not what we want.
		 * GQview needs control of which monitor(s) to use for full screen.
		 */
		gtk_window_fullscreen(GTK_WINDOW(fs->window));
		}
	else if (fullscreen_above)
		{
		/* request to be above other windows */
		gtk_window_set_keep_above(GTK_WINDOW(fs->window), TRUE);
		}

	gtk_window_set_resizable(GTK_WINDOW(fs->window), FALSE);

	gtk_window_set_screen(GTK_WINDOW(fs->window), screen);
	gtk_container_set_border_width(GTK_CONTAINER(fs->window), 0);
	g_signal_connect(G_OBJECT(fs->window), "delete_event",
			 G_CALLBACK(fullscreen_delete_cb), fs);

	gtk_window_set_title(GTK_WINDOW(fs->window), _("GQview full screen"));

	geometry.min_width = w;
	geometry.min_height = h;
	geometry.max_width = w;
	geometry.max_height = h;
	geometry.base_width = w;
	geometry.base_height = h;
	geometry.win_gravity = GDK_GRAVITY_STATIC;
	/* By setting USER_POS and USER_SIZE, most window managers will
	 * not request positioning of the full screen window (for example twm).
	 *
	 * In addition, setting gravity to STATIC will result in the
	 * decorations of twm to not effect the requested window position,
	 * the decorations will simply be off screen, except in multi monitor setups :-/
	 */
	gtk_window_set_geometry_hints(GTK_WINDOW(fs->window), fs->window, &geometry,
				      GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE | GDK_HINT_BASE_SIZE |
				      GDK_HINT_WIN_GRAVITY |
				      GDK_HINT_USER_POS);

	gtk_window_set_default_size(GTK_WINDOW(fs->window), w, h);
	gtk_window_move(GTK_WINDOW(fs->window), x, y);

	fs->imd = image_new(FALSE);

	gtk_container_add(GTK_CONTAINER(fs->window), fs->imd->widget);

	/* set background to black */
	if (BLACK_BACKGROUND)
		{
		image_background_set_black(fs->imd, TRUE);
		}

	image_set_delay_flip(fs->imd, fullscreen_clean_flip);
	image_auto_refresh(fs->imd, fs->normal_imd->auto_refresh_interval);

	if (fullscreen_clean_flip)
		{
		image_set_update_func(fs->imd, fullscreen_image_update_cb, fs);
		image_set_complete_func(fs->imd, fullscreen_image_complete_cb, fs);
		}

	gtk_widget_show(fs->imd->widget);

	image_change_from_image(fs->imd, fs->normal_imd);

	gtk_widget_show(fs->window);

	/* for hiding the mouse */
	g_signal_connect(G_OBJECT(fs->imd->image), "motion_notify_event",
			   G_CALLBACK(fullscreen_mouse_moved), fs);
	clear_mouse_cursor(fs->window, fs->cursor_state);

	/* set timer to block screen saver */
	fs->saver_block_id = g_timeout_add(60 * 1000, fullscreen_saver_block_cb, fs);

	/* hide normal window
	 * FIXME: properly restore this window on show
	 */
#ifdef HIDE_WINDOW_IN_FULLSCREEN
	gtk_widget_hide(fs->normal_window);
#endif
	image_change_path(fs->normal_imd, NULL, image_zoom_get(fs->normal_imd));

	return fs;
}

void fullscreen_stop(FullScreenData *fs)
{
	if (!fs) return;

	g_source_remove(fs->saver_block_id);

	fullscreen_hide_mouse_disable(fs);
	fullscreen_busy_mouse_disable(fs);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);

	image_change_from_image(fs->normal_imd, fs->imd);
#ifdef HIDE_WINDOW_IN_FULLSCREEN
	gtk_widget_show(fs->normal_window);
#endif
	if (fs->stop_func) fs->stop_func(fs, fs->stop_data);

	gtk_widget_destroy(fs->window);

	g_free(fs);
}


/*
 *----------------------------------------------------------------------------
 * full screen preferences and utils
 *----------------------------------------------------------------------------
 */

GList *fullscreen_prefs_list(void)
{
	GList *list = NULL;
	GdkDisplay *display;
	gint number;
	gint i;

	display = gdk_display_get_default();
	number = gdk_display_get_n_screens(display);

	for (i = 0; i < number ; i++)
		{
		GdkScreen *screen;
		gint monitors;
		gint j;

		screen = gdk_display_get_screen(display, i);
		monitors = gdk_screen_get_n_monitors(screen);

		for (j = -1; j < monitors; j++)
			{
			ScreenData *sd;
			GdkRectangle rect;
			gchar *name;
			gchar *subname;

			name = gdk_screen_make_display_name(screen);

			if (j < 0)
				{
				rect.x = 0;
				rect.y = 0;
				rect.width = gdk_screen_get_width(screen);
				rect.height = gdk_screen_get_height(screen);
				subname = g_strdup(_("Full size"));
				}
			else
				{
				gdk_screen_get_monitor_geometry(screen, j, &rect);
				subname = g_strdup_printf("%s %d", _("Monitor"), j + 1);
				}

			sd = g_new0(ScreenData, 1);
			sd->number = (i+1) * 100 + j + 1;
			sd->description = g_strdup_printf("%s %s, %s", _("Screen"), name, subname);
			sd->x = rect.x;
			sd->y = rect.y;
			sd->width = rect.width;
			sd->height = rect.height;

			if (debug) printf("Screen %d %30s %4d,%4d (%4dx%4d)\n",
					  sd->number, sd->description, sd->x, sd->y, sd->width, sd->height);

			list = g_list_append(list, sd);

			g_free(name);
			g_free(subname);
			}
		}

	return list;
}

void fullscreen_prefs_list_free(GList *list)
{
	GList *work;

	work = list;
	while (work)
		{
		ScreenData *sd = work->data;
		work = work->next;

		g_free(sd->description);	
		g_free(sd);
		}

	g_list_free(list);
}

ScreenData *fullscreen_prefs_list_find(GList *list, gint screen)
{
	GList *work;

	work = list;
	while (work)
		{
		ScreenData *sd = work->data;
		work = work->next;

		if (sd->number == screen) return sd;
		}

	return NULL;
}

/* screen is interpreted as such:
 *  -1  window manager determines size and position, fallback is (1) active monitor
 *   0  full size of screen containing widget
 *   1  size of monitor containing widget
 * 100  full size of screen 1 (screen, monitor counts start at 1)
 * 101  size of monitor 1 on screen 1
 * 203  size of monitor 3 on screen 2
 * returns:
 * dest_screen: screen to place widget [use gtk_window_set_screen()]
 * same_region: the returned region will overlap the current location of widget.
 */
void fullscreen_prefs_get_geometry(gint screen, GtkWidget *widget, gint *x, gint *y, gint *width, gint *height,
				   GdkScreen **dest_screen, gint *same_region)
{
	GList *list;
	ScreenData *sd;

	list = fullscreen_prefs_list();
	if (screen >= 100)
		{
		sd = fullscreen_prefs_list_find(list, screen);
		}
	else
		{
		sd = NULL;
		if (screen < 0) screen = 1;
		}

	if (sd)
		{
		GdkDisplay *display;
		GdkScreen *screen;
		gint n;

		display = gdk_display_get_default();
		n = sd->number / 100 - 1;
		if (n >= 0 && n < gdk_display_get_n_screens(display))
			{
			screen = gdk_display_get_screen(display, n);
			}
		else
			{
			screen = gdk_display_get_default_screen(display);
			}

		if (x) *x = sd->x;
		if (y) *y = sd->y;
		if (width) *width = sd->width;
		if (height) *height = sd->height;

		if (dest_screen) *dest_screen = screen;
		if (same_region) *same_region = (!widget || !widget->window ||
					(screen == gtk_widget_get_screen(widget) &&
					(sd->number%100 == 0 ||
					 sd->number%100 == gdk_screen_get_monitor_at_window(screen, widget->window)+1)));

		}
	else if (screen != 1 || !widget || !widget->window)
		{
		GdkScreen *screen;

		if (widget)
			{
			screen = gtk_widget_get_screen(widget);
			}
		else
			{
			screen = gdk_screen_get_default();
			}

		if (x) *x = 0;
		if (y) *y = 0;
		if (width) *width = gdk_screen_get_width(screen);
		if (height) *height = gdk_screen_get_height(screen);

		if (dest_screen) *dest_screen = screen;
		if (same_region) *same_region = TRUE;
		}
	else
		{
		GdkScreen *screen;
		gint monitor;
		GdkRectangle rect;

		screen = gtk_widget_get_screen(widget);
		monitor = gdk_screen_get_monitor_at_window(screen, widget->window);

		gdk_screen_get_monitor_geometry(screen, monitor, &rect);

		if (x) *x = rect.x;
		if (y) *y = rect.y;
		if (width) *width = rect.width;
		if (height) *height = rect.height;

		if (dest_screen) *dest_screen = screen;
		if (same_region) *same_region = TRUE;
		}

	fullscreen_prefs_list_free(list);
}

gint fullscreen_prefs_find_screen_for_widget(GtkWidget *widget)
{
	GdkScreen *screen;
	gint monitor;
	gint n;

	if (!widget || !widget->window) return 0;

	screen = gtk_widget_get_screen(widget);
	monitor = gdk_screen_get_monitor_at_window(screen, widget->window);

	n = (gdk_screen_get_number(screen)+1) * 100 + monitor + 1;

	if (debug || TRUE) printf("Screen appears to be %d\n", n);

	return n;
}

enum {
	FS_MENU_COLUMN_NAME = 0,
	FS_MENU_COLUMN_VALUE
};

#define BUTTON_ABOVE_KEY  "button_above"

static void fullscreen_prefs_selection_cb(GtkWidget *combo, gpointer data)
{
	gint *value = data;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GtkWidget *button;

	if (!value) return;

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter)) return;
	gtk_tree_model_get(store, &iter, FS_MENU_COLUMN_VALUE, value, -1);

	button = g_object_get_data(G_OBJECT(combo), BUTTON_ABOVE_KEY);
	if (button)
		{
		gtk_widget_set_sensitive(button, *value != -1);
		}
}

static void fullscreen_prefs_selection_add(GtkListStore *store, const gchar *text, gint value)
{
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, FS_MENU_COLUMN_NAME, text,
					 FS_MENU_COLUMN_VALUE, value, -1);
}

GtkWidget *fullscreen_prefs_selection_new(const gchar *text, gint *screen_value, gint *above_value)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *combo;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkWidget *button = NULL;
	GList *list;
	GList *work;
	gint current = 0;
	gint n;

	if (!screen_value) return NULL;

	vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);
	hbox = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);
	if (text) pref_label_new(hbox, text);

	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer,
				       "text", FS_MENU_COLUMN_NAME, NULL);

	if (above_value)
		{
		button = pref_checkbox_new_int(vbox, _("Stay above other windows"),
					       *above_value, above_value);
		gtk_widget_set_sensitive(button, *screen_value != -1);

		g_object_set_data(G_OBJECT(combo), BUTTON_ABOVE_KEY, button);
		}

	fullscreen_prefs_selection_add(store, _("Determined by Window Manager"), -1);
	fullscreen_prefs_selection_add(store, _("Active screen"), 0);
	if (*screen_value == 0) current = 1;
	fullscreen_prefs_selection_add(store, _("Active monitor"), 1);
	if (*screen_value == 1) current = 2;

	n = 3;
	list = fullscreen_prefs_list();
	work = list;
	while (work)
		{
		ScreenData *sd = work->data;

		fullscreen_prefs_selection_add(store, sd->description, sd->number);
		if (*screen_value == sd->number) current = n;

		work = work->next;
		n++;
		}
	fullscreen_prefs_list_free(list);

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current);

	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);
	gtk_widget_show(combo);

	g_signal_connect(G_OBJECT(combo), "changed",
			 G_CALLBACK(fullscreen_prefs_selection_cb), screen_value);

	return vbox;
}

