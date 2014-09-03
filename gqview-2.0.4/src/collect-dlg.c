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
#include "collect.h"
#include "collect-dlg.h"

#include "collect-io.h"
#include "utilops.h"
#include "ui_fileops.h"
#include "ui_tabcomp.h"
#include "ui_utildlg.h"


enum {
	DIALOG_SAVE,
	DIALOG_SAVE_CLOSE,
	DIALOG_LOAD,
	DIALOG_APPEND
};


static gint collection_save_confirmed(FileDialog *fd, gint overwrite, CollectionData *cd);


static void collection_confirm_ok_cb(GenericDialog *gd, gpointer data)
{
	FileDialog *fd = data;
	CollectionData *cd = GENERIC_DIALOG(fd)->data;

	if (!collection_save_confirmed(fd, TRUE, cd))
		{
		collection_unref(cd);
		file_dialog_close(fd);
		}
}

static void collection_confirm_cancel_cb(GenericDialog *gd, gpointer data)
{
	/* this is a no-op, so the cancel button is added */
}

static gint collection_save_confirmed(FileDialog *fd, gint overwrite, CollectionData *cd)
{
	gchar *buf;

	if (isdir(fd->dest_path))
		{
		buf = g_strdup_printf(_("Specified path:\n%s\nis a folder, collections are files"), fd->dest_path);
		file_util_warning_dialog(_("Invalid filename"), buf, GTK_STOCK_DIALOG_INFO, GENERIC_DIALOG(fd)->dialog);
		g_free(buf);
		return FALSE;
		}

	if (!overwrite && isfile(fd->dest_path))
		{
		GenericDialog *gd;

		gd = file_util_gen_dlg(_("Overwrite File"), "GQview", "dlg_confirm",
					GENERIC_DIALOG(fd)->dialog, TRUE,
					collection_confirm_cancel_cb, fd);

		generic_dialog_add_message(gd, GTK_STOCK_DIALOG_QUESTION,
					   _("Overwrite existing file?"), fd->dest_path);

		generic_dialog_add_button(gd, GTK_STOCK_OK, _("_Overwrite"), collection_confirm_ok_cb, TRUE);

		gtk_widget_show(gd->dialog);

		return TRUE;
		}

	if (!collection_save(cd, fd->dest_path))
		{
		buf = g_strdup_printf(_("Failed to save the collection:\n%s"), fd->dest_path);
		file_util_warning_dialog(_("Save Failed"), buf, GTK_STOCK_DIALOG_ERROR, GENERIC_DIALOG(fd)->dialog);
		g_free(buf);
		}

	collection_unref(cd);
	file_dialog_sync_history(fd, TRUE);

	if (fd->type == DIALOG_SAVE_CLOSE) collection_window_close_by_collection(cd);
	file_dialog_close(fd);

	return TRUE;
}

static void collection_save_cb(FileDialog *fd, gpointer data)
{
	CollectionData *cd = data;
	const gchar *path;

	path = fd->dest_path;
	
	if (!(strlen(path) > 7 && strncasecmp(path + (strlen(path) - 4), ".gqv", 4) == 0))
		{
		gchar *buf;
		buf = g_strconcat(path, ".gqv", NULL);
		gtk_entry_set_text(GTK_ENTRY(fd->entry), buf);
		g_free(buf);
		}

	collection_save_confirmed(fd, FALSE, cd);
}

static void real_collection_button_pressed(FileDialog *fd, gpointer data, gint append)
{
	CollectionData *cd = data;

	if (!fd->dest_path || isdir(fd->dest_path)) return;

	if (append)
		{
		collection_load(cd, fd->dest_path, TRUE);
		collection_unref(cd);
		}
	else
		{
		collection_window_new(fd->dest_path);
		}

	file_dialog_sync_history(fd, TRUE);
	file_dialog_close(fd);
}

static void collection_load_cb(FileDialog *fd, gpointer data)
{
	real_collection_button_pressed(fd, data, FALSE);
}

static void collection_append_cb(FileDialog *fd, gpointer data)
{
	real_collection_button_pressed(fd, data, TRUE);
}

static void collection_save_or_load_dialog_close_cb(FileDialog *fd, gpointer data)
{
	CollectionData *cd = data;

	if (cd) collection_unref(cd);
	file_dialog_close(fd);
}

static void collection_save_or_load_dialog(const gchar *path,
					   gint type, CollectionData *cd)
{
	FileDialog *fd;
	GtkWidget *parent = NULL;
	CollectWindow *cw;
	const gchar *title;
	const gchar *btntext;
	void *btnfunc;
	gchar *base;
	const gchar *stock_id;

	if (type == DIALOG_SAVE || type == DIALOG_SAVE_CLOSE)
		{
		if (!cd) return;
		title = _("Save collection");
		btntext = NULL;
		btnfunc = collection_save_cb;
		stock_id = GTK_STOCK_SAVE;
		}
	else if (type == DIALOG_LOAD)
		{
		title = _("Open collection");
		btntext = NULL;
		btnfunc = collection_load_cb;
		stock_id = GTK_STOCK_OPEN;
		}
	else
		{
		if (!cd) return;
		title = _("Append collection");
		btntext = _("_Append");
		btnfunc = collection_append_cb;
		stock_id = GTK_STOCK_ADD;
		}

	if (cd) collection_ref(cd);

	cw = collection_window_find(cd);
	if (cw) parent = cw->window;

	fd = file_util_file_dlg(title, "GQview", "dlg_collection", parent,
			     collection_save_or_load_dialog_close_cb, cd);

	generic_dialog_add_message(GENERIC_DIALOG(fd), NULL, title, NULL);
	file_dialog_add_button(fd, stock_id, btntext, btnfunc, TRUE);

	base = g_strconcat(homedir(), "/", GQVIEW_RC_DIR_COLLECTIONS, NULL);
	file_dialog_add_path_widgets(fd, base, path,
				     "collection_load_save", ".gqv", _("Collection Files"));
	g_free(base);

	fd->type = type;

	gtk_widget_show(GENERIC_DIALOG(fd)->dialog);
}

void collection_dialog_save_as(gchar *path, CollectionData *cd)
{
#if 0
	if (!cd->list)
		{
		GtkWidget *parent = NULL;
		CollectWindow *cw;

		cw = collection_window_find(cd);
		if (cw) parent = cw->window;
		file_util_warning_dialog(_("Collection empty"),
					 _("The current collection is empty, save aborted."),
					 GTK_STOCK_DIALOG_INFO, parent);
		return;
		}
#endif

	if (!path) path = cd->path;
	if (!path) path = cd->name;

	collection_save_or_load_dialog(path, DIALOG_SAVE, cd);
}

void collection_dialog_save_close(gchar *path, CollectionData *cd)
{
	if (!path) path = cd->path;
	if (!path) path = cd->name;

	collection_save_or_load_dialog(path, DIALOG_SAVE_CLOSE, cd);
}

void collection_dialog_load(gchar *path)
{
	collection_save_or_load_dialog(path, DIALOG_LOAD, NULL);
}

void collection_dialog_append(gchar *path, CollectionData *cd)
{
	collection_save_or_load_dialog(path, DIALOG_APPEND, cd);
}

