/*
 * (SLIK) SimpLIstic sKin functions
 * (C) 2006 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>

#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "ui_tabcomp.h"

#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_spinner.h"
#include "ui_utildlg.h"

#include <gdk/gdkkeysyms.h> /* for key values */


/* define this to enable a pop-up menu that shows possible matches
 * #define TAB_COMPLETION_ENABLE_POPUP_MENU
 */
#define TAB_COMPLETION_ENABLE_POPUP_MENU 1
#define TAB_COMP_POPUP_MAX 500

#ifdef TAB_COMPLETION_ENABLE_POPUP_MENU
#include "ui_menu.h"
#endif


/* ----------------------------------------------------------------
   Tab completion routines, can be connected to any gtkentry widget
   using the tab_completion_add_to_entry() function.
   Use remove_trailing_slash() to strip the trailing '/'.
   ----------------------------------------------------------------*/

typedef struct _TabCompData TabCompData;
struct _TabCompData
{
	GtkWidget *entry;
	gchar *dir_path;
	GList *file_list;
	void (*enter_func)(const gchar *, gpointer);
	void (*tab_func)(const gchar *, gpointer);
	gpointer enter_data;
	gpointer tab_data;

	GtkWidget *combo;
	gint has_history;
	gchar *history_key;
	gint history_levels;

	FileDialog *fd;
	gchar *fd_title;
	gint fd_folders_only;
	GtkWidget *fd_button;
};


static void tab_completion_select_show(TabCompData *td);

static void tab_completion_free_list(TabCompData *td)
{
	GList *list;

	g_free(td->dir_path);
	td->dir_path = NULL;

	list = td->file_list;

	while(list)
		{
		g_free(list->data);
		list = list->next;
		}

	g_list_free(td->file_list);
	td->file_list = NULL;
}

static void tab_completion_read_dir(TabCompData *td, const gchar *path)
{
        DIR *dp;
        struct dirent *dir;
        GList *list = NULL;
	gchar *pathl;

	tab_completion_free_list(td);

	pathl = path_from_utf8(path);
        dp = opendir(pathl);
	g_free(pathl);
	if (!dp)
                {
                /* dir not found */
                return;
                }
        while ((dir = readdir(dp)) != NULL)
                {
		gchar *name = dir->d_name;
		if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
			{
			list = g_list_prepend(list, path_to_utf8(name));
			}
		}
        closedir(dp);

	td->dir_path = g_strdup(path);
	td->file_list = list;
}

static void tab_completion_destroy(GtkWidget *widget, gpointer data)
{
	TabCompData *td = data;

	tab_completion_free_list(td);
	g_free(td->history_key);

	if (td->fd) file_dialog_close(td->fd);
	g_free(td->fd_title);

	g_free(td);
}

static gint tab_completion_emit_enter_signal(TabCompData *td)
{
	gchar *text;
	if (!td->enter_func) return FALSE;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(td->entry)));

	if (text[0] == '~')
		{
		gchar *t = text;
		text = g_strconcat(homedir(), t + 1, NULL);
		g_free(t);
		}

	td->enter_func(text, td->enter_data);
	g_free(text);

	return TRUE;
}

static void tab_completion_emit_tab_signal(TabCompData *td)
{
	gchar *text;
	if (!td->tab_func) return;

	text = g_strdup(gtk_entry_get_text(GTK_ENTRY(td->entry)));

	if (text[0] == '~')
		{
		gchar *t = text;
		text = g_strconcat(homedir(), t + 1, NULL);
		g_free(t);
		}

	td->tab_func(text, td->tab_data);
	g_free(text);
}

#ifdef TAB_COMPLETION_ENABLE_POPUP_MENU

static gint tab_completion_popup_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	TabCompData *td = data;

	if (event->keyval == GDK_Tab ||
	    event->keyval == GDK_BackSpace ||
	    (event->keyval >= 0x20 && event->keyval <= 0xFF) )
		{
		if (event->keyval >= 0x20 && event->keyval <= 0xFF)
			{
			gchar buf[2];
			gint p = -1;

			buf[0] = event->keyval;
			buf[1] = '\0';
			gtk_editable_insert_text(GTK_EDITABLE(td->entry), buf, 1, &p);
			gtk_editable_set_position(GTK_EDITABLE(td->entry), -1);
			}

		/*close the menu */
		gtk_menu_popdown(GTK_MENU(widget));
		/* doing this does not emit the "selection done" signal, unref it ourselves */
		gtk_widget_unref(widget);
			
		return TRUE;
		}

	return FALSE;
}

static void tab_completion_popup_cb(GtkWidget *widget, gpointer data)
{
	gchar *name = data;
	TabCompData *td;
	gchar *buf;
	gchar *ptr;

	td = g_object_get_data(G_OBJECT(widget), "tab_completion_data");
	if (!td) return;

	ptr = td->dir_path + strlen(td->dir_path) - 1;
	buf = g_strconcat(td->dir_path, (ptr[0] == '/') ? "" : "/", name, NULL);
	gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
	gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(buf));
	g_free(buf);

	tab_completion_emit_tab_signal(td);
}

static void tab_completion_popup_pos_cb(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	TabCompData *td = data;
	gint height;
	PangoLayout *layout;
	PangoRectangle strong_pos, weak_pos;
	gint length;
	gint xoffset, yoffset;
							

	gdk_window_get_origin(td->entry->window, x, y);

	height = MIN(td->entry->requisition.height, td->entry->allocation.height);
	*y += height;

	length = strlen(gtk_entry_get_text(GTK_ENTRY(td->entry)));
	gtk_entry_get_layout_offsets(GTK_ENTRY(td->entry), &xoffset, &yoffset);

	layout = gtk_entry_get_layout(GTK_ENTRY(td->entry));
	pango_layout_get_cursor_pos(layout, length, &strong_pos, &weak_pos);
	*x += strong_pos.x / PANGO_SCALE + xoffset;
}

static void tab_completion_popup_list(TabCompData *td, GList *list)
{
	GtkWidget *menu;
	GList *work;
	GdkEvent *event;
	guint32 etime;
	gint ebutton;
	gint count = 0;

	if (!list) return;

#if 0
	/*
	 * well, the menu would be too long anyway...
	 * (listing /dev causes gtk+ window allocation errors, -> too big a window)
	 * this is why menu popups are disabled, this really should be a popup scrollable listview.
	 */
	if (g_list_length(list) > 200) return;
#endif

	menu = popup_menu_short_lived();

	work = list;
	while (work && count < TAB_COMP_POPUP_MAX)
		{
		gchar *name = work->data;
		GtkWidget *item;

		item = menu_item_add_simple(menu, name, G_CALLBACK(tab_completion_popup_cb), name);
		g_object_set_data(G_OBJECT(item), "tab_completion_data", td);

		work = work->next;
		count++;
		}

	g_signal_connect(G_OBJECT(menu), "key_press_event",
			 G_CALLBACK(tab_completion_popup_key_press), td);

	/* peek at the current event to get the time, etc. */
	event = gtk_get_current_event();

	if (event && event->type == GDK_BUTTON_RELEASE)
		{
		ebutton = event->button.button;
		}
	else
		{
		ebutton = 0;
		}

	if (event)
		{
		etime = gdk_event_get_time(event);
		gdk_event_free(event);
		}
	else
		{
		etime = 0;
		}

	gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
		       tab_completion_popup_pos_cb, td, ebutton, etime);
}

#ifndef CASE_SORT
#define CASE_SORT strcmp
#endif

static gint simple_sort(gconstpointer a, gconstpointer b)
{
        return CASE_SORT((gchar *)a, (gchar *)b);
}

#endif

static gint tab_completion_do(TabCompData *td)
{
	const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(td->entry));
	const gchar *entry_file;
	gchar *entry_dir;
	gchar *ptr;
	gint home_exp = FALSE;

	/* home dir expansion */
	if (entry_text[0] == '~')
		{
		entry_dir = g_strconcat(homedir(), entry_text + 1, NULL);
		home_exp = TRUE;
		}
	else
		{
		entry_dir = g_strdup(entry_text);
		}

	entry_file = filename_from_path(entry_text);

	if (isfile(entry_dir))
		{
		if (home_exp)
			{
			gtk_entry_set_text(GTK_ENTRY(td->entry), entry_dir);
			gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(entry_dir));
			}
		g_free(entry_dir);
		return home_exp;
		}
	if (isdir(entry_dir) && strcmp(entry_file, ".") != 0 && strcmp(entry_file, "..") != 0)
		{
		ptr = entry_dir + strlen(entry_dir) - 1;
		if (ptr[0] == '/')
			{
			if (home_exp)
				{
				gtk_entry_set_text(GTK_ENTRY(td->entry), entry_dir);
				gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(entry_dir));
				}

			tab_completion_read_dir(td, entry_dir);
			td->file_list = g_list_sort(td->file_list, simple_sort);
			if (td->file_list && !td->file_list->next)
				{
				gchar *buf;
				const gchar *file;

				file = td->file_list->data;
				buf = g_strconcat(entry_dir, file, NULL);
				if (isdir(buf))
					{
					g_free(buf);
					buf = g_strconcat(entry_dir, file, "/", NULL);
					}
				gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
				gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(buf));
				g_free(buf);
				}

#ifdef TAB_COMPLETION_ENABLE_POPUP_MENU

			else
				{
				tab_completion_popup_list(td, td->file_list);
				}
#endif

			g_free(entry_dir);
			return home_exp;
			}
		else
			{
			gchar *buf = g_strconcat(entry_dir, "/", NULL);
			gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
			gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(buf));
			g_free(buf);
			g_free(entry_dir);
			return TRUE;
			}
		}

	ptr = (gchar *)filename_from_path(entry_dir);
	if (ptr > entry_dir) ptr--;
	ptr[0] = '\0';

	if (strlen(entry_dir) == 0)
		{
		g_free(entry_dir);
		entry_dir = g_strdup("/");
		}

	if (isdir(entry_dir))
		{
		GList *list;
		GList *poss = NULL;
		gint l = strlen(entry_file);

		if (!td->dir_path || !td->file_list || strcmp(td->dir_path, entry_dir) != 0)
			{
			tab_completion_read_dir(td, entry_dir);
			}

		if (strcmp(entry_dir, "/") == 0) entry_dir[0] = '\0';

		list = td->file_list;
		while(list)
			{
			gchar *file = list->data;
			if (strncmp(entry_file, file, l) == 0)
				{
				poss = g_list_prepend(poss, file);
				}
			list = list->next;
			}

		if (poss)
			{
			if (!poss->next)
				{
				gchar *file = poss->data;
				gchar *buf;

				buf = g_strconcat(entry_dir, "/", file, NULL);

				if (isdir(buf))
					{
					g_free(buf);
					buf = g_strconcat(entry_dir, "/", file, "/", NULL);
					}
				gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
				gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(buf));
				g_free(buf);
				g_list_free(poss);
				g_free(entry_dir);
				return TRUE;
				}
			else
				{
				gint c = strlen(entry_file);
				gint done = FALSE;
				gchar *test_file = poss->data;

				while (!done)
					{
					list = poss;
					if (!list) done = TRUE;
					while(list && !done)
						{
						gchar *file = list->data;
						if (strlen(file) < c || strncmp(test_file, file, c) != 0)
							{
							done = TRUE;
							}
						list = list->next;
						}
					c++;
					}
				c -= 2;
				if (c > 0)
					{
					gchar *file;
					gchar *buf;
					file = g_strdup(test_file);
					file[c] = '\0';
					buf = g_strconcat(entry_dir, "/", file, NULL);
					gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
					gtk_editable_set_position(GTK_EDITABLE(td->entry), strlen(buf));

#ifdef TAB_COMPLETION_ENABLE_POPUP_MENU

					poss = g_list_sort(poss, simple_sort);
					tab_completion_popup_list(td, poss);

#endif

					g_free(file);
					g_free(buf);
					g_list_free(poss);
					g_free(entry_dir);
					return TRUE;
					}
				}
			g_list_free(poss);
			}
		}

	g_free(entry_dir);

	return FALSE;
}

static gint tab_completion_key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	TabCompData *td = data;
	gint stop_signal = FALSE;

	switch (event->keyval)
		{
                case GDK_Tab:
			if (!(event->state & GDK_CONTROL_MASK))
				{
				if (tab_completion_do(td))
					{
					tab_completion_emit_tab_signal(td);
					}
				stop_signal = TRUE;
				}
			break;
		case GDK_Return: case GDK_KP_Enter:
			if (td->fd_button &&
			    (event->state & GDK_CONTROL_MASK))
				{
				tab_completion_select_show(td);
				stop_signal = TRUE;
				}
			else if (tab_completion_emit_enter_signal(td))
				{
				stop_signal = TRUE;
				}
			break;
		default:
			break;
		}

	if (stop_signal) g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

	return (stop_signal);
}

static void tab_completion_button_pressed(GtkWidget *widget, gpointer data)
{
	TabCompData *td;
	GtkWidget *entry = data;

	td = g_object_get_data(G_OBJECT(entry), "tab_completion_data");

	if (!td) return;

	if (!GTK_WIDGET_HAS_FOCUS(entry))
		{
		gtk_widget_grab_focus(entry);
		}

	if (tab_completion_do(td))
		{
		tab_completion_emit_tab_signal(td);
		}
}

static void tab_completion_button_size_allocate(GtkWidget *button, GtkAllocation *allocation, gpointer data)
{
	GtkWidget *parent = data;

	if (allocation->height > parent->allocation.height)
		{
		GtkAllocation button_allocation;

		button_allocation = button->allocation;
		button_allocation.height = parent->allocation.height;
		button_allocation.y = parent->allocation.y +
			(parent->allocation.height - parent->allocation.height) / 2;
		gtk_widget_size_allocate(button, &button_allocation);
		}
}

static GtkWidget *tab_completion_create_complete_button(GtkWidget *entry, GtkWidget *parent)
{
	GtkWidget *button;
	GtkWidget *icon;
	GdkPixbuf *pixbuf;

	button = gtk_button_new();
	GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	g_signal_connect(G_OBJECT(button), "size_allocate",
			 G_CALLBACK(tab_completion_button_size_allocate), parent);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(tab_completion_button_pressed), entry);

	pixbuf = gdk_pixbuf_new_from_inline(-1, icon_tabcomp, FALSE, NULL);
	icon = gtk_image_new_from_pixbuf(pixbuf);
	gdk_pixbuf_unref(pixbuf);
	gtk_container_add(GTK_CONTAINER(button), icon);
	gtk_widget_show(icon);

	return button;
}

/*
 *----------------------------------------------------------------------------
 * public interface
 *----------------------------------------------------------------------------
 */

GtkWidget *tab_completion_new_with_history(GtkWidget **entry, const gchar *text,
					   const gchar *history_key, gint max_levels,
					   void (*enter_func)(const gchar *, gpointer), gpointer data)
{
	GtkWidget *box;
	GtkWidget *combo;
	GtkWidget *combo_entry;
	GtkWidget *button;
	GList *work;
	TabCompData *td;
	gint n = 0;

	box = gtk_hbox_new(FALSE, 0);

	combo = gtk_combo_box_entry_new_text();
	gtk_box_pack_start(GTK_BOX(box), combo, TRUE, TRUE, 0);
	gtk_widget_show(combo);

	combo_entry = GTK_BIN(combo)->child;
#if 0
	gtk_combo_set_case_sensitive(GTK_COMBO(combo), TRUE);
	gtk_combo_set_use_arrows(GTK_COMBO(combo), FALSE);
#endif

	button = tab_completion_create_complete_button(combo_entry, combo);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	tab_completion_add_to_entry(combo_entry, enter_func, data);

	td = g_object_get_data(G_OBJECT(combo_entry), "tab_completion_data");
	if (!td) return NULL; /* this should never happen! */

	td->combo = combo;
	td->has_history = TRUE;
	td->history_key = g_strdup(history_key);
	td->history_levels = max_levels;

	work = history_list_get_by_key(td->history_key);

	work = history_list_get_by_key(history_key);
	while (work)
		{
		gtk_combo_box_append_text(GTK_COMBO_BOX(combo), (gchar *)work->data);
		work = work->next;
		n++;
		}

	if (text)
		{
		gtk_entry_set_text(GTK_ENTRY(combo_entry), text);
		}
	else if (n > 0)
		{
		gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
		}

	if (entry) *entry = combo_entry;
	return box;
}

const gchar *tab_completion_set_to_last_history(GtkWidget *entry)
{
	TabCompData *td = g_object_get_data(G_OBJECT(entry), "tab_completion_data");
	const gchar *buf;

	if (!td || !td->has_history) return NULL;

	buf = history_list_find_last_path_by_key(td->history_key);
	if (buf)
		{
		gtk_entry_set_text(GTK_ENTRY(td->entry), buf);
		}

	return buf;
}

void tab_completion_append_to_history(GtkWidget *entry, const gchar *path)
{
	TabCompData *td;
	GtkTreeModel *store;
	GList *work;

	td = g_object_get_data(G_OBJECT(entry), "tab_completion_data");

	if (!path) return;

	if (!td || !td->has_history) return;

	history_list_add_to_key(td->history_key, path, td->history_levels);

	gtk_combo_box_set_active(GTK_COMBO_BOX(td->combo), -1);

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(td->combo));
	gtk_list_store_clear(GTK_LIST_STORE(store));

	work = history_list_get_by_key(td->history_key);
	while (work)
		{
		gtk_combo_box_append_text(GTK_COMBO_BOX(td->combo), (gchar *)work->data);
		work = work->next;
		}
}

GtkWidget *tab_completion_new(GtkWidget **entry, const gchar *text,
			      void (*enter_func)(const gchar *, gpointer), gpointer data)
{
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *newentry;

	hbox = gtk_hbox_new(FALSE, 0);

	newentry = gtk_entry_new();
	if (text) gtk_entry_set_text(GTK_ENTRY(newentry), text);
	gtk_box_pack_start(GTK_BOX(hbox), newentry, TRUE, TRUE, 0);
	gtk_widget_show(newentry);

	button = tab_completion_create_complete_button(newentry, newentry);
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	tab_completion_add_to_entry(newentry, enter_func, data);

	if (entry) *entry = newentry;
	return hbox;
}

void tab_completion_add_to_entry(GtkWidget *entry, void (*enter_func)(const gchar *, gpointer), gpointer data)
{
	TabCompData *td;
	if (!entry)
		{
		printf("Tab completion error: entry != NULL\n");
		return;
		}

	td = g_new0(TabCompData, 1);
	td->entry = entry;
	td->dir_path = NULL;
	td->file_list = NULL;
	td->enter_func = enter_func;
	td->enter_data = data;
	td->tab_func = NULL;
	td->tab_data = NULL;

	td->has_history = FALSE;
	td->history_key = NULL;
	td->history_levels = 0;

	g_object_set_data(G_OBJECT(td->entry), "tab_completion_data", td);

	g_signal_connect(G_OBJECT(entry), "key_press_event",
			 G_CALLBACK(tab_completion_key_pressed), td);
	g_signal_connect(G_OBJECT(entry), "destroy",
			 G_CALLBACK(tab_completion_destroy), td);
}

void tab_completion_add_tab_func(GtkWidget *entry, void (*tab_func)(const gchar *, gpointer), gpointer data)
{
	TabCompData *td = g_object_get_data(G_OBJECT(entry), "tab_completion_data");

	if (!td) return;

	td->tab_func = tab_func;
	td->tab_data = data;
}

gchar *remove_trailing_slash(const gchar *path)
{
	gchar *ret;
	gint l;
	if (!path) return NULL;

	ret = g_strdup(path);
	l = strlen(ret);
	if (l > 1 && ret[l - 1] == '/') ret[l - 1] = '\0';

	return ret;
}

static void tab_completion_select_cancel_cb(FileDialog *fd, gpointer data)
{
	TabCompData *td = data;

	td->fd = NULL;
	file_dialog_close(fd);
}

static void tab_completion_select_ok_cb(FileDialog *fd, gpointer data)
{
	TabCompData *td = data;

	gtk_entry_set_text(GTK_ENTRY(td->entry), gtk_entry_get_text(GTK_ENTRY(fd->entry)));

	tab_completion_select_cancel_cb(fd, data);

	tab_completion_emit_enter_signal(td);
}

static void tab_completion_select_show(TabCompData *td)
{
	const gchar *title;
	const gchar *path;

	if (td->fd)
		{
		gtk_window_present(GTK_WINDOW(GENERIC_DIALOG(td->fd)->dialog));
		return;
		}

	title = (td->fd_title) ? td->fd_title : _("Select path");
	td->fd = file_dialog_new(title, PACKAGE, "select_path", td->entry,
				 tab_completion_select_cancel_cb, td);
	file_dialog_add_button(td->fd, GTK_STOCK_OK, NULL,
				 tab_completion_select_ok_cb, TRUE);

	generic_dialog_add_message(GENERIC_DIALOG(td->fd), NULL, title, NULL);

	path = gtk_entry_get_text(GTK_ENTRY(td->entry));
	if (strlen(path) == 0) path = NULL;
	if (td->fd_folders_only)
		{
		file_dialog_add_path_widgets(td->fd, NULL, path, td->history_key, NULL, NULL);
		}
	else
		{
		file_dialog_add_path_widgets(td->fd, NULL, path, td->history_key, "*", _("All files"));
		}

	gtk_widget_show(GENERIC_DIALOG(td->fd)->dialog);
}

static void tab_completion_select_pressed(GtkWidget *widget, gpointer data)
{
	TabCompData *td = data;

	tab_completion_select_show(td);
}

void tab_completion_add_select_button(GtkWidget *entry, const gchar *title, gint folders_only)
{
	TabCompData *td;
	GtkWidget *parent;
	GtkWidget *hbox;

	td = g_object_get_data(G_OBJECT(entry), "tab_completion_data");

	if (!td) return;

	g_free(td->fd_title);
	td->fd_title = g_strdup(title);
	td->fd_folders_only = folders_only;

	if (td->fd_button) return;

	parent = (td->combo) ? td->combo : td->entry;

	hbox = gtk_widget_get_parent(parent);
	if (!GTK_IS_BOX(hbox)) return;

	td->fd_button = gtk_button_new_with_label("...");
	g_signal_connect(G_OBJECT(td->fd_button), "size_allocate",
			 G_CALLBACK(tab_completion_button_size_allocate), parent);
	g_signal_connect(G_OBJECT(td->fd_button), "clicked",
			 G_CALLBACK(tab_completion_select_pressed), td);

	gtk_box_pack_start(GTK_BOX(hbox), td->fd_button, FALSE, FALSE, 0);

	gtk_widget_show(td->fd_button);
}

