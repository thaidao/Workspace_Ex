/*
 * GQview
 * (C) 2005 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#include "gqview.h"
#include "dupe.h"

#include "cache.h"
#include "collect.h"
#include "collect-table.h"
#include "dnd.h"
#include "editors.h"
#include "filelist.h"
#include "image-load.h"
#include "img-view.h"
#include "info.h"
#include "layout.h"
#include "layout_image.h"
#include "md5-util.h"
#include "menu.h"
#include "print.h"
#include "thumb.h"
#include "utilops.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"
#include "ui_menu.h"
#include "ui_misc.h"
#include "ui_tree_edit.h"

#include <gdk/gdkkeysyms.h> /* for keyboard values */


#include <math.h>


#define DUPE_DEF_WIDTH 600
#define DUPE_DEF_HEIGHT 400

/* column assignment order (simply change them here) */
enum {
	DUPE_COLUMN_POINTER = 0,
	DUPE_COLUMN_RANK,
	DUPE_COLUMN_THUMB,
	DUPE_COLUMN_NAME,
	DUPE_COLUMN_SIZE,
	DUPE_COLUMN_DATE,
	DUPE_COLUMN_DIMENSIONS,
	DUPE_COLUMN_PATH,
	DUPE_COLUMN_COLOR,
	DUPE_COLUMN_COUNT	/* total columns */
};


static GList *dupe_window_list = NULL;	/* list of open DupeWindow *s */

/*
 * Well, after adding the 'compare two sets' option things got a little sloppy in here
 * because we have to account for two 'modes' everywhere. (be careful).
 */

static void dupe_match_unlink(DupeItem *a, DupeItem *b);
static DupeItem *dupe_match_find_parent(DupeWindow *dw, DupeItem *child);

static gint dupe_match(DupeItem *a, DupeItem *b, DupeMatchType mask, gdouble *rank, gint fast);

static void dupe_thumb_step(DupeWindow *dw);
static gint dupe_check_cb(gpointer data);

static void dupe_second_add(DupeWindow *dw, DupeItem *di);
static void dupe_second_remove(DupeWindow *dw, DupeItem *di);
static GtkWidget *dupe_menu_popup_second(DupeWindow *dw, DupeItem *di);

static void dupe_dnd_init(DupeWindow *dw);

/*
 * ------------------------------------------------------------------
 * Window updates
 * ------------------------------------------------------------------
 */


static void dupe_window_update_count(DupeWindow *dw, gint count_only)
{
	gchar *text;

	if (!dw->list)
		{
		text = g_strdup(_("Drop files to compare them."));
		}
	else if (count_only)
		{
		text = g_strdup_printf(_("%d files"), g_list_length(dw->list));
		}
	else
		{
		text = g_strdup_printf(_("%d matches found in %d files"), g_list_length(dw->dupes), g_list_length(dw->list));
		}

	if (dw->second_set)
		{
		gchar *buf = g_strconcat(text, " ", _("[set 1]"), NULL);
		g_free(text);
		text = buf;
		}
	gtk_label_set_text(GTK_LABEL(dw->status_label), text);

	g_free(text);
}

static guint64 msec_time(void)
{
	struct timeval tv;

	if (gettimeofday(&tv, NULL) == -1) return 0;

	return (guint64)tv.tv_sec * 1000000 + (guint64)tv.tv_usec;
}

static gint dupe_iterations(gint n)
{
	return (n * ((n + 1) / 2));
}

static void dupe_window_update_progress(DupeWindow *dw, const gchar *status, gdouble value, gint force)
{
	const gchar *status_text;

	if (status)
		{
		guint64 new_time = 0;

		if (dw->setup_n % 10 == 0)
			{
			new_time = msec_time() - dw->setup_time;
			}
		
		if (!force &&
		    value != 0.0 &&
		    dw->setup_count > 0 &&
		    new_time > 2000000)
			{
			gchar *buf;
			gint t;
			gint d;
			guint32 rem;

			if (new_time - dw->setup_time_count < 250000) return;
			dw->setup_time_count = new_time;

			if (dw->setup_done)
				{
				if (dw->second_set)
					{
					t = dw->setup_count;
					d = dw->setup_count - dw->setup_n;
					}
				else
					{
					t = dupe_iterations(dw->setup_count);
					d = dupe_iterations(dw->setup_count - dw->setup_n);
					}
				}
			else
				{
				t = dw->setup_count;
				d = dw->setup_count - dw->setup_n;
				}

			rem = (t - d) ? ((gdouble)(dw->setup_time_count / 1000000) / (t - d)) * d : 0;

			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dw->extra_label), value);

			buf = g_strdup_printf("%s %d:%02d ", status, rem / 60, rem % 60);
			gtk_progress_bar_set_text(GTK_PROGRESS_BAR(dw->extra_label), buf);
			g_free(buf);

			return;
			}
		else if (force ||
			 value == 0.0 ||
			 dw->setup_count == 0 ||
			 dw->setup_time_count == 0 ||
			 (new_time > 0 && new_time - dw->setup_time_count >= 250000))
			{
			if (dw->setup_time_count == 0) dw->setup_time_count = 1;
			if (new_time > 0) dw->setup_time_count = new_time;
			gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dw->extra_label), value);
			status_text = status;
			}
		else
			{
			status_text = NULL;
			}
		}	
	else
		{
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dw->extra_label), 0.0);
		status_text = " ";
		}

	if (status_text) gtk_progress_bar_set_text(GTK_PROGRESS_BAR(dw->extra_label), status_text);
}

static void widget_set_cursor(GtkWidget *widget, gint icon)
{
	GdkCursor *cursor;

	if (!widget->window) return;
 
	if (icon == -1)
		{
		cursor = NULL;
		}
	else
		{
		cursor = gdk_cursor_new (icon);
		}
 
	gdk_window_set_cursor(widget->window, cursor);
 
	if (cursor) gdk_cursor_unref(cursor);
}

/*
 * ------------------------------------------------------------------
 * row color utils
 * ------------------------------------------------------------------
 */

static void dupe_listview_realign_colors(DupeWindow *dw)
{
	GtkTreeModel *store;
	GtkTreeIter iter;
	gboolean color_set = TRUE;
	DupeItem *parent = NULL;
	gint valid;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview));
	valid = gtk_tree_model_get_iter_first(store, &iter);
	while (valid)
		{
		DupeItem *child;
		DupeItem *child_parent;

		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &child, -1);
		child_parent = dupe_match_find_parent(dw, child);
		if (!parent || parent != child_parent)
			{
			if (!parent)
				{
				/* keep the first row as it is */
				gtk_tree_model_get(store, &iter, DUPE_COLUMN_COLOR, &color_set, -1);
				}
			else
				{
				color_set = !color_set;
				}
			parent = dupe_match_find_parent(dw, child);
			}
		gtk_list_store_set(GTK_LIST_STORE(store), &iter, DUPE_COLUMN_COLOR, color_set, -1);

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
		}
}

/*
 * ------------------------------------------------------------------
 * Dupe item utils
 * ------------------------------------------------------------------
 */

static DupeItem *dupe_item_new(const gchar *path, gint64 size, time_t date)
{
	DupeItem *di;

	di = g_new0(DupeItem, 1);

	di->path = g_strdup(path);
	di->name = filename_from_path(di->path);
	di->size = size;
	di->date = date;

	di->group = NULL;
	di->group_rank = 0.0;

	di->simd = NULL;
	di->checksum = 0;
	di->md5sum = NULL;
	di->width = 0;
	di->height = 0;

	di->second = FALSE;

	return di;
}

static void dupe_item_free(DupeItem *di)
{
	g_free(di->path);
	image_sim_free(di->simd);
	g_free(di->md5sum);
	if (di->pixbuf) g_object_unref(di->pixbuf);

	g_free(di);
}

static void dupe_list_free(GList *list)
{
	GList *work = list;
	while (work)
		{
		DupeItem *di = work->data;
		work = work->next;
		dupe_item_free(di);
		}
	g_list_free(list);
}

static DupeItem *dupe_item_find_path_by_list(const gchar *path, GList *work)
{
	while (work)
		{
		DupeItem *di = work->data;

		if (strcmp(di->path, path) == 0) return di;

		work = work->next;
		}

	return NULL;
}

static DupeItem *dupe_item_find_path(DupeWindow *dw, const gchar *path)
{
	DupeItem *di;

	di = dupe_item_find_path_by_list(path, dw->list);
	if (!di && dw->second_set) di = dupe_item_find_path_by_list(path, dw->second_list);

	return di;
}

/*
 * ------------------------------------------------------------------
 * Image property cache
 * ------------------------------------------------------------------
 */

static void dupe_item_read_cache(DupeItem *di)
{
	gchar *path;
	CacheData *cd;

	if (!di) return;

	path = cache_find_location(CACHE_TYPE_SIM, di->path);
	if (!path) return;

	if (filetime(di->path) != filetime(path))
		{
		g_free(path);
		return;
		}

	cd = cache_sim_data_load(path);
	g_free(path);

	if (cd)
		{
		if (!di->simd && cd->sim)
			{
			di->simd = cd->sim;
			cd->sim = NULL;
			}
		if (di->width == 0 && di->height == 0 && cd->dimensions)
			{
			di->width = cd->width;
			di->height = cd->height;
			}
		if (di->checksum == 0 && cd->have_checksum)
			{
			di->checksum = cd->checksum;
			}
		if (!di->md5sum && cd->have_md5sum)
			{
			di->md5sum = md5_digest_to_text(cd->md5sum);
			}
		cache_sim_data_free(cd);
		}
}

static void dupe_item_write_cache(DupeItem *di)
{
	gchar *base;
	mode_t mode = 0755;

	if (!di) return;

	base = cache_get_location(CACHE_TYPE_SIM, di->path, FALSE, &mode);
	if (cache_ensure_dir_exists(base, mode))
		{
		CacheData *cd;

		cd = cache_sim_data_new();
		cd->path = cache_get_location(CACHE_TYPE_SIM, di->path, TRUE, NULL);

		if (di->width != 0) cache_sim_data_set_dimensions(cd, di->width, di->height);
		if (di->checksum != 0) cache_sim_data_set_checksum(cd, di->checksum);
		if (di->md5sum)
			{
			guchar digest[16];
			if (md5_digest_from_text(di->md5sum, digest)) cache_sim_data_set_md5sum(cd, digest);
			}
		if (di->simd) cache_sim_data_set_similarity(cd, di->simd);

		if (cache_sim_data_save(cd))
			{
			filetime_set(cd->path, filetime(di->path));
			}
		cache_sim_data_free(cd);
		}
	g_free(base);
}

/*
 * ------------------------------------------------------------------
 * Window list utils
 * ------------------------------------------------------------------
 */

static gint dupe_listview_find_item(GtkListStore *store, DupeItem *item, GtkTreeIter *iter)
{
	gint valid;
	gint row = 0;

	valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), iter);
	while (valid)
		{
		DupeItem *item_n;
		gtk_tree_model_get(GTK_TREE_MODEL(store), iter, DUPE_COLUMN_POINTER, &item_n, -1);
		if (item_n == item) return row;

		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), iter);
		row++;
		}

	return -1;
}

static void dupe_listview_add(DupeWindow *dw, DupeItem *parent, DupeItem *child)
{
	DupeItem *di;
	gint row;
	gchar *text[DUPE_COLUMN_COUNT];
	GtkListStore *store;
	GtkTreeIter iter;
	gboolean color_set = FALSE;
	gint rank;

	if (!parent) return;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));

	if (child)
		{
		DupeMatch *dm;

		row = dupe_listview_find_item(store, parent, &iter);
		gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, DUPE_COLUMN_COLOR, &color_set, -1);

		row++;

		if (child->group)
			{
			dm = child->group->data;
			rank = (gint)floor(dm->rank);
			}
		else
			{
			rank = 1;
			printf("NULL group in item!\n");
			}
		}
	else
		{
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
			{
			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, DUPE_COLUMN_COLOR, &color_set, -1);
			color_set = !color_set;
			}
		else
			{
			color_set = FALSE;
			}
		row = 0;
		rank = 0;
		}

	di = (child) ? child : parent;

	if (!child && dw->second_set)
		{
		text[DUPE_COLUMN_RANK] = g_strdup("[1]");
		}
	else if (rank == 0)
		{
		text[DUPE_COLUMN_RANK] = g_strdup((di->second) ? "(2)" : "");
		}
	else
		{
		text[DUPE_COLUMN_RANK] = g_strdup_printf("%d%s", rank, (di->second) ? " (2)" : "");
		}

	text[DUPE_COLUMN_THUMB] = "";
	text[DUPE_COLUMN_NAME] = (gchar *)di->name;
	text[DUPE_COLUMN_SIZE] = text_from_size(di->size);
	text[DUPE_COLUMN_DATE] = (gchar *)text_from_time(di->date);
	if (di->width > 0 && di->height > 0)
		{
		text[DUPE_COLUMN_DIMENSIONS] = g_strdup_printf("%d x %d", di->width, di->height);
		}
	else
		{
		text[DUPE_COLUMN_DIMENSIONS] = g_strdup("");
		}
	text[DUPE_COLUMN_PATH] = di->path;
	text[DUPE_COLUMN_COLOR] = NULL;

	gtk_list_store_insert(store, &iter, row);
	gtk_list_store_set(store, &iter,
				DUPE_COLUMN_POINTER, di,
				DUPE_COLUMN_RANK, text[DUPE_COLUMN_RANK],
				DUPE_COLUMN_THUMB, NULL,
				DUPE_COLUMN_NAME, text[DUPE_COLUMN_NAME],
				DUPE_COLUMN_SIZE, text[DUPE_COLUMN_SIZE],
				DUPE_COLUMN_DATE, text[DUPE_COLUMN_DATE],
				DUPE_COLUMN_DIMENSIONS, text[DUPE_COLUMN_DIMENSIONS],
				DUPE_COLUMN_PATH, text[DUPE_COLUMN_PATH],
				DUPE_COLUMN_COLOR, color_set,
				-1);

	g_free(text[DUPE_COLUMN_RANK]);
	g_free(text[DUPE_COLUMN_SIZE]);
	g_free(text[DUPE_COLUMN_DIMENSIONS]);
}

static void dupe_listview_populate(DupeWindow *dw)
{
	GtkListStore *store;
	GList *work;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
	gtk_list_store_clear(store);

	work = g_list_last(dw->dupes);
	while (work)
		{
		DupeItem *parent = work->data;
		GList *temp;

		dupe_listview_add(dw, parent, NULL);

		temp = g_list_last(parent->group);
		while (temp)
			{
			DupeMatch *dm = temp->data;
			DupeItem *child;

			child = dm->di;

			dupe_listview_add(dw, parent, child);

			temp = temp->prev;
			}

		work = work->prev;
		}

	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dw->listview));
}

static void dupe_listview_remove(DupeWindow *dw, DupeItem *di)
{
	GtkListStore *store;
	GtkTreeIter iter;
	gint row;

	if (!di) return;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
	row = dupe_listview_find_item(store, di, &iter);
	if (row < 0) return;

	tree_view_move_cursor_away(GTK_TREE_VIEW(dw->listview), &iter, TRUE);
	gtk_list_store_remove(store, &iter);

	if (g_list_find(dw->dupes, di) != NULL)
		{
		if (!dw->color_frozen) dupe_listview_realign_colors(dw);
		}
}


static GList *dupe_listview_get_path_list(DupeWindow *dw, GtkWidget *listview)
{
	GtkTreeModel *store;
	GtkTreeIter iter;
	gint valid;
	GList *list = NULL;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(listview));
	valid = gtk_tree_model_get_iter_first(store, &iter);
	while (valid)
		{
		DupeItem *di;
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		list = g_list_prepend(list, g_strdup(di->path));

		valid = gtk_tree_model_iter_next(store, &iter);
		}

	return g_list_reverse(list);
}


static GList *dupe_listview_get_selection(DupeWindow *dw, GtkWidget *listview)
{
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GList *slist;
	GList *list = NULL;
	GList *work;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	slist = gtk_tree_selection_get_selected_rows(selection, &store);
	work = slist;
	while (work)
		{
		GtkTreePath *tpath = work->data;
		DupeItem *di = NULL;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		if (di)
			{
			list = g_list_prepend(list, g_strdup(di->path));
			}
		work = work->next;
		}
	g_list_foreach(slist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(slist);

	return g_list_reverse(list);
}

static gint dupe_listview_item_is_selected(DupeWindow *dw, DupeItem *di, GtkWidget *listview)
{
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GList *slist;
	GList *work;
	gint found = FALSE;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	slist = gtk_tree_selection_get_selected_rows(selection, &store);
	work = slist;
	while (!found && work)
		{
		GtkTreePath *tpath = work->data;
		DupeItem *di_n;
		GtkTreeIter iter;

		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di_n, -1);
		if (di_n == di) found = TRUE;
		work = work->next;
		}
	g_list_foreach(slist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(slist);

	return found;
}

static void dupe_listview_select_dupes(DupeWindow *dw, gint parents)
{
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	gint valid;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->listview));
	gtk_tree_selection_unselect_all(selection);

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview));
	valid = gtk_tree_model_get_iter_first(store, &iter);
	while (valid)
		{
		DupeItem *di;

		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		if ( (dupe_match_find_parent(dw, di) == di) == (parents) )
			{
			gtk_tree_selection_select_iter(selection, &iter);
			}
		valid = gtk_tree_model_iter_next(store, &iter);
		}
}

/*
 * ------------------------------------------------------------------
 * Match group manipulation
 * ------------------------------------------------------------------
 */

static DupeMatch *dupe_match_find_match(DupeItem *child, DupeItem *parent)
{
	GList *work;

	work = parent->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		if (dm->di == child) return dm;
		work = work->next;
		}
	return NULL;
}

static void dupe_match_link_child(DupeItem *child, DupeItem *parent, gdouble rank)
{
	DupeMatch *dm;

	dm = g_new0(DupeMatch, 1);
	dm->di = child;
	dm->rank = rank;
	parent->group = g_list_append(parent->group, dm);
}

static void dupe_match_link(DupeItem *a, DupeItem *b, gdouble rank)
{
	dupe_match_link_child(a, b, rank);
	dupe_match_link_child(b, a, rank);
}

static void dupe_match_unlink_child(DupeItem *child, DupeItem *parent)
{
	DupeMatch *dm;

	dm = dupe_match_find_match(child, parent);
	if (dm)
		{
		parent->group = g_list_remove(parent->group, dm);
		g_free(dm);
		}
}

static void dupe_match_unlink(DupeItem *a, DupeItem *b)
{
	dupe_match_unlink_child(a, b);
	dupe_match_unlink_child(b, a);
}

static void dupe_match_link_clear(DupeItem *parent, gint unlink_children)
{
	GList *work;
                                                                                                                               
	work = parent->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		work = work->next;

		if (unlink_children) dupe_match_unlink_child(parent, dm->di);

		g_free(dm);
		}

	g_list_free(parent->group);
	parent->group = NULL;
	parent->group_rank = 0.0;
}

static gint dupe_match_link_exists(DupeItem *child, DupeItem *parent)
{
	return (dupe_match_find_match(child, parent) != NULL);
}

static gdouble dupe_match_link_rank(DupeItem *child, DupeItem *parent)
{
	DupeMatch *dm;

	dm = dupe_match_find_match(child, parent);
	if (dm) return dm->rank;

	return 0.0;
}

static DupeItem *dupe_match_highest_rank(DupeItem *child)
{
	DupeMatch *dr;
	GList *work;

	dr = NULL;
	work = child->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		if (!dr || dm->rank > dr->rank) dr = dm;
		work = work->next;
		}

	return (dr) ? dr->di : NULL;
}

static void dupe_match_rank_update(DupeItem *parent)
{
	GList *work;
	gdouble rank = 0.0;
	gint c = 0;

	work = parent->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		work = work->next;
		rank += dm->rank;
		c++;
		}

	if (c > 0)
		{
		parent->group_rank = rank / c;
		}
	else
		{
		parent->group_rank = 0.0;
		}
}

static DupeItem *dupe_match_find_parent(DupeWindow *dw, DupeItem *child)
{
	GList *work;

	if (g_list_find(dw->dupes, child)) return child;

	work = child->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		if (g_list_find(dw->dupes, dm->di)) return dm->di;
		work = work->next;
		}

	return NULL;
}

static void dupe_match_reset_list(GList *work)
{
	while (work)
		{
		DupeItem *di = work->data;
		work = work->next;

		dupe_match_link_clear(di, FALSE);
		}
}

static void dupe_match_reparent(DupeWindow *dw, DupeItem *old, DupeItem *new)
{
	GList *work;

	if (!old || !new || !dupe_match_link_exists(old, new)) return;

	dupe_match_link_clear(new, TRUE);
	work = old->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		dupe_match_unlink_child(old, dm->di);
		dupe_match_link_child(new, dm->di, dm->rank);
		work = work->next;
		}

	new->group = old->group;
	old->group = NULL;

	work = g_list_find(dw->dupes, old);
	if (work) work->data = new;
}

static void dupe_match_print_group(DupeItem *di)
{
	GList *work;

	printf("+ %f %s\n", di->group_rank, di->name);

	work = di->group;
	while (work)
		{
		DupeMatch *dm = work->data;
		work = work->next;

		printf("  %f %s\n", dm->rank, dm->di->name);
		}

	printf("\n");
}

static void dupe_match_print_list(GList *list)
{
	GList *work;

	work = list;
	while (work)
		{
		DupeItem *di = work->data;
		dupe_match_print_group(di);
		work = work->next;
		}
}

/* level 3, unlinking and orphan handling */
static GList *dupe_match_unlink_by_rank(DupeItem *child, DupeItem *parent, GList *list, DupeWindow *dw)
{
	DupeItem *best;

	best = dupe_match_highest_rank(parent);
	if (best == child || dupe_match_highest_rank(child) == parent)
		{
		GList *work;
		gdouble rank;

		if (debug > 1) printf("link found %s to %s [%d]\n", child->name, parent->name, g_list_length(parent->group));

		work = parent->group;
		while (work)
			{
			DupeMatch *dm = work->data;
			DupeItem *orphan;

			work = work->next;
			orphan = dm->di;
			if (orphan != child && g_list_length(orphan->group) < 2)
				{
				dupe_match_link_clear(orphan, TRUE);
				if (!dw->second_set || orphan->second)
					{
					dupe_match(orphan, child, dw->match_mask, &rank, FALSE);
					dupe_match_link(orphan, child, rank);
					}
				list = g_list_remove(list, orphan);
				}
			}
		
		rank = dupe_match_link_rank(child, parent);
		dupe_match_link_clear(parent, TRUE);
		dupe_match_link(child, parent, rank);
		list = g_list_remove(list, parent);
		}
	else
		{
		if (debug > 1) printf("unlinking %s and %s\n", child->name, parent->name);
		
		dupe_match_unlink(child, parent);
		}

	return list;
}

/* level 2 */
static GList *dupe_match_group_filter(GList *list, DupeItem *di, DupeWindow *dw)
{
	GList *work;

	work = g_list_last(di->group);
	while (work)
		{
		DupeMatch *dm = work->data;
		work = work->prev;
		list = dupe_match_unlink_by_rank(di, dm->di, list, dw);
		}

	return list;
}

/* level 1 (top) */
static GList *dupe_match_group_trim(GList *list, DupeWindow *dw)
{
	GList *work;

	work = list;
	while (work)
		{
		DupeItem *di = work->data;
		if (!di->second) list = dupe_match_group_filter(list, di, dw);
		work = work->next;
		if (di->second) list = g_list_remove(list, di);
		}

	return list;
}

static gint dupe_match_sort_groups_cb(gconstpointer a, gconstpointer b)
{
	DupeMatch *da = (DupeMatch *)a;
	DupeMatch *db = (DupeMatch *)b;

	if (da->rank > db->rank) return -1;
	if (da->rank < db->rank) return 1;
	return 0;
}

static void dupe_match_sort_groups(GList *list)
{
	GList *work;

	work = list;
	while (work)
		{
		DupeItem *di = work->data;
		di->group = g_list_sort(di->group, dupe_match_sort_groups_cb);
		work = work->next;
		}
}

static gint dupe_match_rank_sort_cb(gconstpointer a, gconstpointer b)
{
	DupeItem *da = (DupeItem *)a;
	DupeItem *db = (DupeItem *)b;

	if (da->group_rank > db->group_rank) return -1;
	if (da->group_rank < db->group_rank) return 1;
	return 0;
}

/* returns allocated GList of dupes sorted by rank */
static GList *dupe_match_rank_sort(GList *source_list)
{
	GList *list = NULL;
	GList *work;

	work = source_list;
	while (work)
		{
		DupeItem *di = work->data;

		if (di->group)
			{
			dupe_match_rank_update(di);
			list = g_list_prepend(list, di);
			}
			
		work = work->next;
		}

	return g_list_sort(list, dupe_match_rank_sort_cb);
}

static void dupe_match_rank(DupeWindow *dw)
{
	GList *list;

	list = dupe_match_rank_sort(dw->list);

	if (debug > 1) dupe_match_print_list(list);
	
	if (debug) printf("Similar items: %d\n", g_list_length(list));
	list = dupe_match_group_trim(list, dw);
	if (debug) printf("Unique groups: %d\n", g_list_length(list));

	dupe_match_sort_groups(list);

	if (debug) dupe_match_print_list(list);

	list = dupe_match_rank_sort(list);

	g_list_free(dw->dupes);
	dw->dupes = list;
}

/*
 * ------------------------------------------------------------------
 * Match group tests
 * ------------------------------------------------------------------
 */

static gint dupe_match(DupeItem *a, DupeItem *b, DupeMatchType mask, gdouble *rank, gint fast)
{
	*rank = 0.0;

	if (a == b) return FALSE;

	if (mask & DUPE_MATCH_PATH)
		{
		if (strcmp(a->path, b->path) != 0) return FALSE;
		}
	if (mask & DUPE_MATCH_NAME)
		{
		if (strcmp(a->name, b->name) != 0) return FALSE;
		}
	if (mask & DUPE_MATCH_SIZE)
		{
		if (a->size != b->size) return FALSE;
		}
	if (mask & DUPE_MATCH_DATE)
		{
		if (a->date != b->date) return FALSE;
		}
	if (mask & DUPE_MATCH_SUM)
		{
		if (!a->md5sum) a->md5sum = md5_text_from_file_utf8(a->path, "");
		if (!b->md5sum) b->md5sum = md5_text_from_file_utf8(b->path, "");
		if (a->md5sum[0] == '\0' ||
		    b->md5sum[0] == '\0' ||
		    strcmp(a->md5sum, b->md5sum) != 0) return FALSE;
		}
	if (mask & DUPE_MATCH_DIM)
		{
		if (a->width == 0) image_load_dimensions(a->path, &a->width, &a->height);
		if (b->width == 0) image_load_dimensions(b->path, &b->width, &b->height);
		if (a->width != b->width || a->height != b->height) return FALSE;
		}
	if (mask & DUPE_MATCH_SIM_HIGH ||
	    mask & DUPE_MATCH_SIM_MED ||
	    mask & DUPE_MATCH_SIM_LOW ||
	    mask & DUPE_MATCH_SIM_CUSTOM)
		{
		gdouble f;
		gdouble m;

		if (mask & DUPE_MATCH_SIM_HIGH) m = 0.95;
		else if (mask & DUPE_MATCH_SIM_MED) m = 0.90;
		else if (mask & DUPE_MATCH_SIM_CUSTOM) m = (gdouble)dupe_custom_threshold / 100.0;
		else m = 0.85;

		if (fast)
			{
			f = image_sim_compare_fast(a->simd, b->simd, m);
			}
		else
			{
			f = image_sim_compare(a->simd, b->simd);
			}

		*rank = f * 100.0;

		if (f < m) return FALSE;

		if (debug > 2) printf("similar: %32s %32s = %f\n", a->name, b->name, f);
		}

	return TRUE;
}

static void dupe_list_check_match(DupeWindow *dw, DupeItem *needle, GList *start)
{
	GList *work;

	if (dw->second_set)
		{
		work = dw->second_list;
		}
	else if (start)
		{
		work = start;
		}
	else
		{
		work = g_list_last(dw->list);
		}

	while (work)
		{
		DupeItem *di = work->data;

		/* speed opt: forward for second set, back for simple compare */
		if (dw->second_set)
			work = work->next;
		else
			work = work->prev;

		if (!dupe_match_link_exists(needle, di))
			{
			gdouble rank;

			if (dupe_match(di, needle, dw->match_mask, &rank, TRUE))
				{
				dupe_match_link(di, needle, rank);
				}
			}
		}
}

/*
 * ------------------------------------------------------------------
 * Thumbnail handling
 * ------------------------------------------------------------------
 */

static void dupe_listview_set_thumb(DupeWindow *dw, DupeItem *di, GtkTreeIter *iter)
{
	GtkListStore *store;
	GtkTreeIter iter_n;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
	if (!iter)
		{
		if (dupe_listview_find_item(store, di, &iter_n) >= 0)
			{
			iter = &iter_n;
			}
		}

	if (iter) gtk_list_store_set(store, iter, DUPE_COLUMN_THUMB, di->pixbuf, -1);
}

static void dupe_thumb_do(DupeWindow *dw)
{
	DupeItem *di;

	if (!dw->thumb_loader || !dw->thumb_item) return;
	di = dw->thumb_item;

	if (di->pixbuf) g_object_unref(di->pixbuf);
	di->pixbuf = thumb_loader_get_pixbuf(dw->thumb_loader, TRUE);

	dupe_listview_set_thumb(dw, di, NULL);
}

static void dupe_thumb_error_cb(ThumbLoader *tl, gpointer data)
{
	DupeWindow *dw = data;

	dupe_thumb_do(dw);
	dupe_thumb_step(dw);
}

static void dupe_thumb_done_cb(ThumbLoader *tl, gpointer data)
{
	DupeWindow *dw = data;

	dupe_thumb_do(dw);
	dupe_thumb_step(dw);
}

static void dupe_thumb_step(DupeWindow *dw)
{
	GtkTreeModel *store;
	GtkTreeIter iter;
	DupeItem *di = NULL;
	gint valid;
	gint row = 0;
	gint length = 0;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview));
	valid = gtk_tree_model_get_iter_first(store, &iter);

	while (!di && valid)
		{
		GdkPixbuf *pixbuf;

		length++;
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, DUPE_COLUMN_THUMB, &pixbuf, -1);
		if (pixbuf || di->pixbuf)
			{
			if (!pixbuf) gtk_list_store_set(GTK_LIST_STORE(store), &iter, DUPE_COLUMN_THUMB, di->pixbuf, -1);
			row++;
			di = NULL;
			}
		valid = gtk_tree_model_iter_next(store, &iter);
		}
	if (valid)
		{
		while (gtk_tree_model_iter_next(store, &iter)) length++;
		}

	if (!di)
		{
		dw->thumb_item = NULL;
		thumb_loader_free(dw->thumb_loader);
		dw->thumb_loader = NULL;

		dupe_window_update_progress(dw, NULL, 0.0, FALSE);
		return;
		}

	dupe_window_update_progress(dw, _("Loading thumbs..."),
				    length == 0 ? 0.0 : (gdouble)(row) / length, FALSE);

	dw->thumb_item = di;
	thumb_loader_free(dw->thumb_loader);
	dw->thumb_loader = thumb_loader_new(thumb_max_width, thumb_max_height);

	thumb_loader_set_callbacks(dw->thumb_loader,
				   dupe_thumb_done_cb,
				   dupe_thumb_error_cb,
				   NULL,
				   dw);

	/* start it */
	if (!thumb_loader_start(dw->thumb_loader, di->path))
		{
		/* error, handle it, do next */
		if (debug) printf("error loading thumb for %s\n", di->path);
		dupe_thumb_do(dw);
		dupe_thumb_step(dw);
		}
}

/*
 * ------------------------------------------------------------------
 * Dupe checking loop
 * ------------------------------------------------------------------
 */

static void dupe_check_stop(DupeWindow *dw)
{
	if (dw->idle_id != -1 || dw->img_loader || dw->thumb_loader)
		{
		g_source_remove(dw->idle_id);
		dw->idle_id = -1;
		dupe_window_update_progress(dw, NULL, 0.0, FALSE);
		widget_set_cursor(dw->listview, -1);
		}

	thumb_loader_free(dw->thumb_loader);
	dw->thumb_loader = NULL;

	image_loader_free(dw->img_loader);
	dw->img_loader = NULL;
}

static void dupe_loader_done_cb(ImageLoader *il, gpointer data)
{
	DupeWindow *dw = data;
	GdkPixbuf *pixbuf;

	pixbuf = image_loader_get_pixbuf(il);

	if (dw->setup_point)
		{
		DupeItem *di = dw->setup_point->data;

		if (!di->simd)
			{
			di->simd = image_sim_new_from_pixbuf(pixbuf);
			}
		else
			{
			image_sim_fill_data(di->simd, pixbuf);
			}

		if (di->width == 0 && di->height == 0)
			{
			di->width = gdk_pixbuf_get_width(pixbuf);
			di->height = gdk_pixbuf_get_height(pixbuf);
			}
		if (enable_thumb_caching)
			{
			dupe_item_write_cache(di);
			}

		image_sim_alternate_processing(di->simd);
		}

	image_loader_free(dw->img_loader);
	dw->img_loader = NULL;

	dw->idle_id = g_idle_add(dupe_check_cb, dw);
}

static void dupe_setup_reset(DupeWindow *dw)
{
	dw->setup_point = NULL;
	dw->setup_n = 0;
	dw->setup_time = msec_time();
	dw->setup_time_count = 0;
}

static GList *dupe_setup_point_step(DupeWindow *dw, GList *p)
{
	if (!p) return NULL;

	if (p->next) return p->next;

	if (dw->second_set && g_list_first(p) == dw->list) return dw->second_list;

	return NULL;
}

static gint dupe_check_cb(gpointer data)
{
	DupeWindow *dw = data;

	if (dw->idle_id == -1) return FALSE;

	if (!dw->setup_done)
		{
		if ((dw->match_mask & DUPE_MATCH_SUM) &&
		    !(dw->setup_mask & DUPE_MATCH_SUM) )
			{
			if (!dw->setup_point) dw->setup_point = dw->list;

			while (dw->setup_point)
				{
				DupeItem *di = dw->setup_point->data;

				dw->setup_point = dupe_setup_point_step(dw, dw->setup_point);
				dw->setup_n++;

				if (!di->md5sum)
					{
					dupe_window_update_progress(dw, _("Reading checksums..."),
						dw->setup_count == 0 ? 0.0 : (gdouble)(dw->setup_n - 1) / dw->setup_count, FALSE);

					if (enable_thumb_caching)
						{
						dupe_item_read_cache(di);
						if (di->md5sum) return TRUE;
						}

					di->md5sum = md5_text_from_file_utf8(di->path, "");
					if (enable_thumb_caching)
						{
						dupe_item_write_cache(di);
						}
					return TRUE;
					}
				}
			dw->setup_mask |= DUPE_MATCH_SUM;
			dupe_setup_reset(dw);
			}
		if ((dw->match_mask & DUPE_MATCH_DIM) &&
		    !(dw->setup_mask & DUPE_MATCH_DIM) )
			{
			if (!dw->setup_point) dw->setup_point = dw->list;

			while (dw->setup_point)
				{
				DupeItem *di = dw->setup_point->data;

				dw->setup_point = dupe_setup_point_step(dw, dw->setup_point);
				dw->setup_n++;
				if (di->width == 0 && di->height == 0)
					{
					dupe_window_update_progress(dw, _("Reading dimensions..."),
						dw->setup_count == 0 ? 0.0 : (gdouble)(dw->setup_n - 1) / dw->setup_count, FALSE);

					if (enable_thumb_caching)
						{
						dupe_item_read_cache(di);
						if (di->width != 0 || di->height != 0) return TRUE;
						}

					image_load_dimensions(di->path, &di->width, &di->height);
					if (enable_thumb_caching)
						{
						dupe_item_write_cache(di);
						}
					return TRUE;
					}
				}
			dw->setup_mask |= DUPE_MATCH_DIM;
			dupe_setup_reset(dw);
			}
		if ((dw->match_mask & DUPE_MATCH_SIM_HIGH ||
		     dw->match_mask & DUPE_MATCH_SIM_MED ||
		     dw->match_mask & DUPE_MATCH_SIM_LOW ||
		     dw->match_mask & DUPE_MATCH_SIM_CUSTOM) &&
		    !(dw->setup_mask & DUPE_MATCH_SIM_MED) )
			{
			if (!dw->setup_point) dw->setup_point = dw->list;

			while (dw->setup_point)
				{
				DupeItem *di = dw->setup_point->data;

				if (!di->simd)
					{
					dupe_window_update_progress(dw, _("Reading similarity data..."),
						dw->setup_count == 0 ? 0.0 : (gdouble)dw->setup_n / dw->setup_count, FALSE);

					if (enable_thumb_caching)
						{
						dupe_item_read_cache(di);
						if (cache_sim_data_filled(di->simd))
							{
							image_sim_alternate_processing(di->simd);
							return TRUE;
							}
						}

					dw->img_loader = image_loader_new(di->path);
					image_loader_set_buffer_size(dw->img_loader, 8);
					image_loader_set_error_func(dw->img_loader, dupe_loader_done_cb, dw);

					if (!image_loader_start(dw->img_loader, dupe_loader_done_cb, dw))
						{
						image_sim_free(di->simd);
						di->simd = image_sim_new();
						image_loader_free(dw->img_loader);
						dw->img_loader = NULL;
						return TRUE;
						}
					dw->idle_id = -1;
					return FALSE;
					}

				dw->setup_point = dupe_setup_point_step(dw, dw->setup_point);
				dw->setup_n++;
				}
			dw->setup_mask |= DUPE_MATCH_SIM_MED;
			dupe_setup_reset(dw);
			}
		dupe_window_update_progress(dw, _("Comparing..."), 0.0, FALSE);
		dw->setup_done = TRUE;
		dupe_setup_reset(dw);
		dw->setup_count = g_list_length(dw->list);
		}

	if (!dw->working)
		{
		if (dw->setup_count > 0)
			{
			dw->setup_count = 0;
			dupe_window_update_progress(dw, _("Sorting..."), 1.0, TRUE);
			return TRUE;
			}
		dw->idle_id = -1;
		dupe_window_update_progress(dw, NULL, 0.0, FALSE);

		dupe_match_rank(dw);
		dupe_window_update_count(dw, FALSE);

		dupe_listview_populate(dw);

		/* check thumbs */
		if (dw->show_thumbs) dupe_thumb_step(dw);

		widget_set_cursor(dw->listview, -1);

		return FALSE;
		}

	dupe_list_check_match(dw, (DupeItem *)dw->working->data, dw->working);
	dupe_window_update_progress(dw, _("Comparing..."), dw->setup_count == 0 ? 0.0 : (gdouble) dw->setup_n / dw->setup_count, FALSE);
	dw->setup_n++;

	dw->working = dw->working->prev;

	return TRUE;
}

static void dupe_check_start(DupeWindow *dw)
{
	dw->setup_done = FALSE;

	dw->setup_count = g_list_length(dw->list);
	if (dw->second_set) dw->setup_count += g_list_length(dw->second_list);

	dw->setup_mask = 0;
	dupe_setup_reset(dw);

	dw->working = g_list_last(dw->list);

	dupe_window_update_count(dw, TRUE);
	widget_set_cursor(dw->listview, GDK_WATCH);

	if (dw->idle_id != -1) return;

	dw->idle_id = g_idle_add(dupe_check_cb, dw);
}

/*
 * ------------------------------------------------------------------
 * Item addition, removal
 * ------------------------------------------------------------------
 */

static void dupe_item_remove(DupeWindow *dw, DupeItem *di)
{
	if (!di) return;

	/* handle things that may be in progress... */
	if (dw->working && dw->working->data == di)
		{
		dw->working = dw->working->prev;
		}
	if (dw->thumb_loader && dw->thumb_item == di)
		{
		dupe_thumb_step(dw);
		}
	if (dw->setup_point && dw->setup_point->data == di)
		{
		dw->setup_point = dupe_setup_point_step(dw, dw->setup_point);
		if (dw->img_loader)
			{
			image_loader_free(dw->img_loader);
			dw->img_loader = NULL;
			dw->idle_id = g_idle_add(dupe_check_cb, dw);
			}
		}

	if (di->group && dw->dupes)
		{
		/* is a dupe, must remove from group/reset children if a parent */
		DupeItem *parent;

		parent = dupe_match_find_parent(dw, di);
		if (di == parent)
			{
			if (g_list_length(parent->group) < 2)
				{
				DupeItem *child;

				child = dupe_match_highest_rank(parent);
				dupe_match_link_clear(child, TRUE);
				dupe_listview_remove(dw, child);

				dupe_match_link_clear(parent, TRUE);
				dupe_listview_remove(dw, parent);
				dw->dupes = g_list_remove(dw->dupes, parent);
				}
			else
				{
				DupeItem *new_parent;
				DupeMatch *dm;
				
				dm = parent->group->data;
				new_parent = dm->di;
				dupe_match_reparent(dw, parent, new_parent);
				dupe_listview_remove(dw, parent);
				}
			}
		else
			{
			if (g_list_length(parent->group) < 2)
				{
				dupe_match_link_clear(parent, TRUE);
				dupe_listview_remove(dw, parent);
				dw->dupes = g_list_remove(dw->dupes, parent);
				}
			dupe_match_link_clear(di, TRUE);
			dupe_listview_remove(dw, di);
			}
		}
	else
		{
		/* not a dupe, or not sorted yet, simply reset */
		dupe_match_link_clear(di, TRUE);
		}

	if (dw->second_list && g_list_find(dw->second_list, di))
		{
		dupe_second_remove(dw, di);
		}
	else
		{
		dw->list = g_list_remove(dw->list, di);
		}
	dupe_item_free(di);

	dupe_window_update_count(dw, FALSE);
}

static gint dupe_item_remove_by_path(DupeWindow *dw, const gchar *path)
{
	DupeItem *di;

	di = dupe_item_find_path(dw, path);
	if (!di) return FALSE;

	dupe_item_remove(dw, di);

	return TRUE;
}

static void dupe_files_add(DupeWindow *dw, CollectionData *collection, CollectInfo *info,
			   const gchar *path, gint recurse)
{
	DupeItem *di = NULL;

	if (info)
		{
		di = dupe_item_new(info->path, info->size, info->date);
		}
	else if (path)
		{
		if (isfile(path))
			{
			di = dupe_item_new(path, filesize(path), filetime(path));
			}
		else if (isdir(path) && recurse)
			{
			GList *f, *d;
			if (path_list(path, &f, &d))
				{
				GList *work;

				f = path_list_filter(f, FALSE);
				d = path_list_filter(d, TRUE);

				work = f;
				while(work)
					{
					dupe_files_add(dw, NULL, NULL, (gchar *)work->data, TRUE);
					work = work->next;
					}
				path_list_free(f);
				work = d;
				while(work)
					{
					dupe_files_add(dw, NULL, NULL, (gchar *)work->data, TRUE);
					work = work->next;
					}
				path_list_free(d);
				}
			}
		}

	if (!di) return;

	if (dw->second_drop)
		{
		dupe_second_add(dw, di);
		}
	else
		{
		dw->list = g_list_prepend(dw->list, di);
		}
}

void dupe_window_add_collection(DupeWindow *dw, CollectionData *collection)
{
	CollectInfo *info;

	info = collection_get_first(collection);
	while(info)
		{
		dupe_files_add(dw, collection, info, NULL, FALSE);
		info = collection_next_by_info(collection, info);
		}

	dupe_check_start(dw);
}

void dupe_window_add_files(DupeWindow *dw, GList *list, gint recurse)
{
	GList *work;

	work = list;
	while(work)
		{
		gchar *path = work->data;
		work = work->next;

		dupe_files_add(dw, NULL, NULL, path, recurse);
		}

	dupe_check_start(dw);
}

static gint dupe_item_set_path(DupeWindow *dw, const gchar *source, const gchar *dest)
{
	DupeItem *di;

	di = dupe_item_find_path(dw, source);
	if (!di) return FALSE;

	if ( (dw->match_mask & DUPE_MATCH_NAME) || (dw->match_mask & DUPE_MATCH_PATH) )
		{
		/* only effects matches on name or path */
		gint second;

		second = di->second;
		dupe_item_remove(dw, di);

		dw->second_drop = second;
		dupe_files_add(dw, NULL, NULL, dest, FALSE);
		dw->second_drop = FALSE;

		dupe_check_start(dw);
		}
	else
		{
		GtkListStore *store;
		GtkTreeIter iter;
		gint row;

		g_free(di->path);
		di->path = g_strdup(dest);
		di->name = filename_from_path(di->path);

		/* update the listview(s) */

		store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
		row = dupe_listview_find_item(store, di, &iter);
		if (row >= 0)
			{
			gtk_list_store_set(store, &iter,
					   DUPE_COLUMN_NAME, di->name,
					   DUPE_COLUMN_PATH, di->path, -1);
			}

		if (dw->second_listview)
			{
			store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->second_listview)));
			row = dupe_listview_find_item(store, di, &iter);
			if (row >= 0)
				{
				gtk_list_store_set(store, &iter, 1, di->path, -1);
				}
			}
		}

	return TRUE;
}

/*
 * ------------------------------------------------------------------
 * Misc.
 * ------------------------------------------------------------------
 */

static GtkWidget *dupe_display_label(GtkWidget *vbox, const gchar *description, const gchar *text)
{
	GtkWidget *hbox;
	GtkWidget *label;

	hbox = gtk_hbox_new(FALSE, 10);

	label = gtk_label_new(description);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	return label;
}

static void dupe_display_stats(DupeWindow *dw, DupeItem *di)
{
	GenericDialog *gd;
	gchar *buf;
	
	if (!di) return;

	gd = file_util_gen_dlg("Image thumbprint debug info", "GQview", "thumbprint",
			       dw->window, TRUE,
			       NULL, NULL);
	generic_dialog_add_button(gd, GTK_STOCK_CLOSE, NULL, NULL, TRUE);

	dupe_display_label(gd->vbox, "name:", di->name);
	buf = text_from_size(di->size);
	dupe_display_label(gd->vbox, "size:", buf);
	g_free(buf);
	dupe_display_label(gd->vbox, "date:", text_from_time(di->date));
	buf = g_strdup_printf("%d x %d", di->width, di->height);
	dupe_display_label(gd->vbox, "dimensions:", buf);
	g_free(buf);
	dupe_display_label(gd->vbox, "md5sum:", (di->md5sum) ? di->md5sum : "not generated");

	dupe_display_label(gd->vbox, "thumbprint:", (di->simd) ? "" : "not generated");
	if (di->simd)
		{
		GtkWidget *image;
		GdkPixbuf *pixbuf;
		gint x, y;
		guchar *d_pix;
		guchar *dp;
		gint rs;
		gint sp;
		
		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 32, 32);
		rs = gdk_pixbuf_get_rowstride(pixbuf);
		d_pix = gdk_pixbuf_get_pixels(pixbuf);

		for (y = 0; y < 32; y++)
			{
			dp = d_pix + (y * rs);
			sp = y * 32;
			for (x = 0; x < 32; x++)
				{
				*(dp++) = di->simd->avg_r[sp + x];
				*(dp++) = di->simd->avg_g[sp + x];
				*(dp++) = di->simd->avg_b[sp + x];
				}
			}

		image = gtk_image_new_from_pixbuf(pixbuf);
		gtk_box_pack_start(GTK_BOX(gd->vbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);
		
		gdk_pixbuf_unref(pixbuf);
		}
	
	gtk_widget_show(gd->dialog);
}

static void dupe_window_recompare(DupeWindow *dw)
{
	GtkListStore *store;

	dupe_check_stop(dw);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
	gtk_list_store_clear(store);

	g_list_free(dw->dupes);
	dw->dupes = NULL;

	dupe_match_reset_list(dw->list);
	dupe_match_reset_list(dw->second_list);

	dupe_check_start(dw);
}

static void dupe_menu_view(DupeWindow *dw, DupeItem *di, GtkWidget *listview, gint new_window)
{
	if (!di) return;

	if (di->collection && collection_info_valid(di->collection, di->info))
		{
		if (new_window)
			{
			view_window_new_from_collection(di->collection, di->info);
			}
		else
			{
			layout_image_set_collection(NULL, di->collection, di->info);
			}
		}
	else
		{
		if (new_window)
			{
			GList *list;

			list = dupe_listview_get_selection(dw, listview);
			view_window_new_from_list(list);
			path_list_free(list);
			}
		else
			{
			layout_image_set_path(NULL, di->path);
			}
		}
}

static void dupe_window_remove_selection(DupeWindow *dw, GtkWidget *listview)
{
	GtkTreeSelection *selection;
	GtkTreeModel *store;
	GtkTreeIter iter;
	GList *slist;
	GList *list = NULL;
	GList *work;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	slist = gtk_tree_selection_get_selected_rows(selection, &store);
	work = slist;
	while (work)
		{
		GtkTreePath *tpath = work->data;
		DupeItem *di = NULL;

		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		if (di) list = g_list_prepend(list, di);
		work = work->next;
		}
	g_list_foreach(slist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(slist);

	dw->color_frozen = TRUE;
	work = list;
	while (work)
		{
		DupeItem *di;

		di = work->data;
		work = work->next;
		dupe_item_remove(dw, di);
		}
	dw->color_frozen = FALSE;

	g_list_free(list);

	dupe_listview_realign_colors(dw);
}

static void dupe_window_edit_selected(DupeWindow *dw, gint n)
{
	GList *list;

	list = dupe_listview_get_selection(dw, dw->listview);

	start_editor_from_path_list(n, list);

	path_list_free(list);
}

static void dupe_window_collection_from_selection(DupeWindow *dw)
{
	CollectWindow *w;
	GList *list;

	list = dupe_listview_get_selection(dw, dw->listview);
	w = collection_window_new(NULL);
	collection_table_add_path_list(w->table, list);
	path_list_free(list);
}

static void dupe_window_append_file_list(DupeWindow *dw, gint on_second)
{
	GList *list;

	dw->second_drop = (dw->second_set && on_second);

	list = layout_list(NULL);
	dupe_window_add_files(dw, list, FALSE);
	path_list_free(list);
}

/*
 *-------------------------------------------------------------------
 * main pop-up menu callbacks
 *-------------------------------------------------------------------
 */

static void dupe_menu_view_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	if (dw->click_item) dupe_menu_view(dw, dw->click_item, dw->listview, FALSE);
}

static void dupe_menu_viewnew_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	if (dw->click_item) dupe_menu_view(dw, dw->click_item, dw->listview, TRUE);
}

static void dupe_menu_select_all_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->listview));
	gtk_tree_selection_select_all(selection);
}

static void dupe_menu_select_none_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->listview));
	gtk_tree_selection_unselect_all(selection);
}

static void dupe_menu_select_dupes_set1_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_listview_select_dupes(dw, TRUE);
}

static void dupe_menu_select_dupes_set2_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_listview_select_dupes(dw, FALSE);
}

static void dupe_menu_edit_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw;
	gint n;

	dw = submenu_item_get_data(widget);
	n = GPOINTER_TO_INT(data);
	if (!dw) return;

	dupe_window_edit_selected(dw, n);
}

static void dupe_menu_info_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	info_window_new(NULL, dupe_listview_get_selection(dw, dw->listview));
}

static void dupe_menu_collection_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_window_collection_from_selection(dw);
}

static void dupe_menu_print_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;
	const gchar *path;

	path = (dw->click_item) ? dw->click_item->path : NULL;

	print_window_new(path,
			 dupe_listview_get_selection(dw, dw->listview),
			 dupe_listview_get_path_list(dw, dw->listview), dw->window);
}

static void dupe_menu_copy_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	file_util_copy(NULL, dupe_listview_get_selection(dw, dw->listview), NULL, dw->window);
}

static void dupe_menu_move_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	file_util_move(NULL, dupe_listview_get_selection(dw, dw->listview), NULL, dw->window);
}

static void dupe_menu_rename_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	file_util_rename(NULL, dupe_listview_get_selection(dw, dw->listview), dw->window);
}

static void dupe_menu_delete_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	file_util_delete(NULL, dupe_listview_get_selection(dw, dw->listview), dw->window);
}

static void dupe_menu_remove_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_window_remove_selection(dw, dw->listview);
}

static void dupe_menu_clear_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_window_clear(dw);
}

static void dupe_menu_close_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_window_close(dw);
}

static GtkWidget *dupe_menu_popup_main(DupeWindow *dw, DupeItem *di)
{
	GtkWidget *menu;
	GtkWidget *item;
	gint on_row;

	on_row = (di != NULL);

	menu = popup_menu_short_lived();
	menu_item_add_sensitive(menu, _("_View"), on_row,
				G_CALLBACK(dupe_menu_view_cb), dw);
	menu_item_add_stock_sensitive(menu, _("View in _new window"), GTK_STOCK_NEW, on_row,
				G_CALLBACK(dupe_menu_viewnew_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_sensitive(menu, _("Select all"), (dw->dupes != NULL),
				G_CALLBACK(dupe_menu_select_all_cb), dw);
	menu_item_add_sensitive(menu, _("Select none"), (dw->dupes != NULL),
				G_CALLBACK(dupe_menu_select_none_cb), dw);
	menu_item_add_sensitive(menu, _("Select group _1 duplicates"), (dw->dupes != NULL),
				G_CALLBACK(dupe_menu_select_dupes_set1_cb), dw);
	menu_item_add_sensitive(menu, _("Select group _2 duplicates"), (dw->dupes != NULL),
				G_CALLBACK(dupe_menu_select_dupes_set2_cb), dw);
	menu_item_add_divider(menu);
	submenu_add_edit(menu, &item, G_CALLBACK(dupe_menu_edit_cb), dw);
	if (!on_row) gtk_widget_set_sensitive(item, FALSE);
	menu_item_add_stock_sensitive(menu, _("_Properties"), GTK_STOCK_PROPERTIES, on_row,
				G_CALLBACK(dupe_menu_info_cb), dw);
	menu_item_add_stock_sensitive(menu, _("Add to new collection"), GTK_STOCK_INDEX, on_row,
				G_CALLBACK(dupe_menu_collection_cb), dw);
	menu_item_add_stock_sensitive(menu, _("Print..."), GTK_STOCK_PRINT, on_row,
				G_CALLBACK(dupe_menu_print_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_stock_sensitive(menu, _("_Copy..."), GTK_STOCK_COPY, on_row,
				G_CALLBACK(dupe_menu_copy_cb), dw);
	menu_item_add_sensitive(menu, _("_Move..."), on_row,
				G_CALLBACK(dupe_menu_move_cb), dw);
	menu_item_add_sensitive(menu, _("_Rename..."), on_row,
				G_CALLBACK(dupe_menu_rename_cb), dw);
	menu_item_add_stock_sensitive(menu, _("_Delete..."), GTK_STOCK_DELETE, on_row,
				G_CALLBACK(dupe_menu_delete_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_stock_sensitive(menu, _("Rem_ove"), GTK_STOCK_REMOVE, on_row,
				G_CALLBACK(dupe_menu_remove_cb), dw);
	menu_item_add_stock_sensitive(menu, _("C_lear"), GTK_STOCK_CLEAR, (dw->list != NULL),
				G_CALLBACK(dupe_menu_clear_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("Close _window"), GTK_STOCK_CLOSE,
			    G_CALLBACK(dupe_menu_close_cb), dw);

	return menu;
}

static gint dupe_listview_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	DupeWindow *dw = data;
	GtkTreeModel *store;
	GtkTreePath *tpath;
	GtkTreeIter iter;
	DupeItem *di = NULL;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

	if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bevent->x, bevent->y,
					  &tpath, NULL, NULL, NULL))
		{
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		gtk_tree_path_free(tpath);
		}

	dw->click_item = di;

	if (bevent->button == 3)
		{
		/* right click menu */
		GtkWidget *menu;

		if (bevent->state & GDK_CONTROL_MASK && bevent->state & GDK_SHIFT_MASK)
			{
			dupe_display_stats(dw, di);
			return TRUE;
			}
		if (widget == dw->listview)
			{
			menu = dupe_menu_popup_main(dw, di);
			}
		else
			{
			menu = dupe_menu_popup_second(dw, di);
			}
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, bevent->button, bevent->time);
		}

	if (!di) return FALSE;

	if (bevent->button == 1 && bevent->type == GDK_2BUTTON_PRESS)
		{
		dupe_menu_view(dw, di, widget, FALSE);
		}

	if (bevent->button == 2) return TRUE;

	if (bevent->button == 3)
		{
		if (!dupe_listview_item_is_selected(dw, di, widget))
			{
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_iter(selection, &iter);

			tpath = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), tpath, NULL, FALSE);
			gtk_tree_path_free(tpath);
			}

		return TRUE;
		}

	if (bevent->button == 1 && bevent->type == GDK_BUTTON_PRESS &&
	    !(bevent->state & GDK_SHIFT_MASK ) &&
	    !(bevent->state & GDK_CONTROL_MASK ) &&
	    dupe_listview_item_is_selected(dw, di, widget))
		{
		/* this selection handled on release_cb */
		gtk_widget_grab_focus(widget);
		return TRUE;
		}

	return FALSE;
}

static gint dupe_listview_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	DupeWindow *dw = data;
	GtkTreeModel *store;
	GtkTreePath *tpath;
	GtkTreeIter iter;
	DupeItem *di = NULL;

	if (bevent->button != 1 && bevent->button != 2) return TRUE;

	store = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));

	if ((bevent->x != 0 || bevent->y != 0) &&
	    gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bevent->x, bevent->y,
					  &tpath, NULL, NULL, NULL))
		{
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		gtk_tree_path_free(tpath);
		}

	if (bevent->button == 2)
		{
		if (di && dw->click_item == di)
			{
			GtkTreeSelection *selection;

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
			if (dupe_listview_item_is_selected(dw, di, widget))
				{
				gtk_tree_selection_unselect_iter(selection, &iter);
				}
			else
				{
				gtk_tree_selection_select_iter(selection, &iter);
				}
			}
		return TRUE;
		}

	if (di && dw->click_item == di &&
	    !(bevent->state & GDK_SHIFT_MASK ) &&
	    !(bevent->state & GDK_CONTROL_MASK ) &&
	    dupe_listview_item_is_selected(dw, di, widget))
		{
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		gtk_tree_selection_unselect_all(selection);
		gtk_tree_selection_select_iter(selection, &iter);

		tpath = gtk_tree_model_get_path(store, &iter);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), tpath, NULL, FALSE);
		gtk_tree_path_free(tpath);

		return TRUE;
		}

	return FALSE;
}

/*
 *-------------------------------------------------------------------
 * second set stuff
 *-------------------------------------------------------------------
 */

static void dupe_second_update_status(DupeWindow *dw)
{
	gchar *buf;

	buf = g_strdup_printf(_("%d files (set 2)"), g_list_length(dw->second_list));
	gtk_label_set_text(GTK_LABEL(dw->second_status_label), buf);
	g_free(buf);
}

static void dupe_second_add(DupeWindow *dw, DupeItem *di)
{
	GtkListStore *store;
	GtkTreeIter iter;

	if (!di) return;

	di->second = TRUE;
	dw->second_list = g_list_prepend(dw->second_list, di);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->second_listview)));
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, DUPE_COLUMN_POINTER, di, 1, di->path, -1);

	dupe_second_update_status(dw);
}

static void dupe_second_remove(DupeWindow *dw, DupeItem *di)
{
	GtkListStore *store;
	GtkTreeIter iter;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->second_listview)));
	if (dupe_listview_find_item(store, di, &iter) >= 0)
		{
		tree_view_move_cursor_away(GTK_TREE_VIEW(dw->second_listview), &iter, TRUE);
		gtk_list_store_remove(store, &iter);
		}

	dw->second_list = g_list_remove(dw->second_list, di);

	dupe_second_update_status(dw);
}

static void dupe_second_clear(DupeWindow *dw)
{
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->second_listview)));
	gtk_list_store_clear(store);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dw->second_listview));

	g_list_free(dw->dupes);
	dw->dupes = NULL;

	dupe_list_free(dw->second_list);
	dw->second_list = NULL;

	dupe_match_reset_list(dw->list);

	dupe_second_update_status(dw);
}

static void dupe_second_menu_view_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	if (dw->click_item) dupe_menu_view(dw, dw->click_item, dw->second_listview, FALSE);
}

static void dupe_second_menu_viewnew_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	if (dw->click_item) dupe_menu_view(dw, dw->click_item, dw->second_listview, TRUE);
}

static void dupe_second_menu_select_all_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeSelection *selection;
	DupeWindow *dw = data;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->second_listview));
	gtk_tree_selection_select_all(selection);
}

static void dupe_second_menu_select_none_cb(GtkWidget *widget, gpointer data)
{
	GtkTreeSelection *selection;
	DupeWindow *dw = data;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->second_listview));
	gtk_tree_selection_unselect_all(selection);
}

static void dupe_second_menu_remove_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_window_remove_selection(dw, dw->second_listview);
}

static void dupe_second_menu_clear_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dupe_second_clear(dw);
	dupe_window_recompare(dw);
}

static GtkWidget *dupe_menu_popup_second(DupeWindow *dw, DupeItem *di)
{
	GtkWidget *menu;
	gint notempty;
	gint on_row;

	on_row = (di != NULL);
	notempty = (dw->second_list != NULL);

	menu = popup_menu_short_lived();
	menu_item_add_sensitive(menu, _("_View"), on_row,
				G_CALLBACK(dupe_second_menu_view_cb), dw);
	menu_item_add_stock_sensitive(menu, _("View in _new window"), GTK_STOCK_NEW, on_row,
				G_CALLBACK(dupe_second_menu_viewnew_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_sensitive(menu, _("Select all"), notempty,
				G_CALLBACK(dupe_second_menu_select_all_cb), dw);
	menu_item_add_sensitive(menu, _("Select none"), notempty,
				G_CALLBACK(dupe_second_menu_select_none_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_stock_sensitive(menu, _("Rem_ove"), GTK_STOCK_REMOVE, on_row,
				      G_CALLBACK(dupe_second_menu_remove_cb), dw);
	menu_item_add_stock_sensitive(menu, _("C_lear"), GTK_STOCK_CLEAR, notempty,
				   G_CALLBACK(dupe_second_menu_clear_cb), dw);
	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("Close _window"), GTK_STOCK_CLOSE,
			    G_CALLBACK(dupe_menu_close_cb), dw);

	return menu;
}

static void dupe_second_set_toggle_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dw->second_set = GTK_TOGGLE_BUTTON(widget)->active;

	if (dw->second_set)
		{
		dupe_second_update_status(dw);
		gtk_table_set_col_spacings(GTK_TABLE(dw->table), PREF_PAD_GAP);
		gtk_widget_show(dw->second_vbox);
		}
	else
		{
		gtk_table_set_col_spacings(GTK_TABLE(dw->table), 0);
		gtk_widget_hide(dw->second_vbox);
		dupe_second_clear(dw);
		}

	dupe_window_recompare(dw);
}

/*
 *-------------------------------------------------------------------
 * match type menu
 *-------------------------------------------------------------------
 */

enum {
	DUPE_MENU_COLUMN_NAME = 0,
	DUPE_MENU_COLUMN_MASK
};

static void dupe_menu_type_cb(GtkWidget *combo, gpointer data)
{
	DupeWindow *dw = data;
	GtkTreeModel *store;
	GtkTreeIter iter;

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter)) return;
	gtk_tree_model_get(store, &iter, DUPE_MENU_COLUMN_MASK, &dw->match_mask, -1);

	dupe_window_recompare(dw);
}

static void dupe_menu_add_item(GtkListStore *store, const gchar *text, DupeMatchType type, DupeWindow *dw)
{
	GtkTreeIter iter;

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, DUPE_MENU_COLUMN_NAME, text,
					 DUPE_MENU_COLUMN_MASK, type, -1);

	if (dw->match_mask == type) gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dw->combo), &iter);
}

static void dupe_menu_setup(DupeWindow *dw)
{
	GtkListStore *store;
	GtkCellRenderer *renderer;

	store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
	dw->combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dw->combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dw->combo), renderer,
				       "text", DUPE_MENU_COLUMN_NAME, NULL);

	dupe_menu_add_item(store, _("Name"), DUPE_MATCH_NAME, dw);
	dupe_menu_add_item(store, _("Size"), DUPE_MATCH_SIZE, dw);
	dupe_menu_add_item(store, _("Date"), DUPE_MATCH_DATE, dw);
	dupe_menu_add_item(store, _("Dimensions"), DUPE_MATCH_DIM, dw);
	dupe_menu_add_item(store, _("Checksum"), DUPE_MATCH_SUM, dw);
	dupe_menu_add_item(store, _("Path"), DUPE_MATCH_PATH, dw);
	dupe_menu_add_item(store, _("Similarity (high)"), DUPE_MATCH_SIM_HIGH, dw);
	dupe_menu_add_item(store, _("Similarity"), DUPE_MATCH_SIM_MED, dw);
	dupe_menu_add_item(store, _("Similarity (low)"), DUPE_MATCH_SIM_LOW, dw);
	dupe_menu_add_item(store, _("Similarity (custom)"), DUPE_MATCH_SIM_CUSTOM, dw);

	g_signal_connect(G_OBJECT(dw->combo), "changed",
			 G_CALLBACK(dupe_menu_type_cb), dw);
}

/*
 *-------------------------------------------------------------------
 * list view columns
 *-------------------------------------------------------------------
 */

/* this overrides the low default of a GtkCellRenderer from 100 to CELL_HEIGHT_OVERRIDE, something sane for our purposes */

#define CELL_HEIGHT_OVERRIDE 512
                                                                                                                               
void cell_renderer_height_override(GtkCellRenderer *renderer)
{
	GParamSpec *spec;

	spec = g_object_class_find_property(G_OBJECT_GET_CLASS(G_OBJECT(renderer)), "height");
	if (spec && G_IS_PARAM_SPEC_INT(spec))
		{
		GParamSpecInt *spec_int;

		spec_int = G_PARAM_SPEC_INT(spec);
		if (spec_int->maximum < CELL_HEIGHT_OVERRIDE) spec_int->maximum = CELL_HEIGHT_OVERRIDE;
		}
}

static GdkColor *dupe_listview_color_shifted(GtkWidget *widget)
{
	static GdkColor color;
	static GtkWidget *done = NULL;

	if (done != widget)
		{
		GtkStyle *style;

		style = gtk_widget_get_style(widget);
		memcpy(&color, &style->base[GTK_STATE_NORMAL], sizeof(color));
		shift_color(&color, -1, 0);
		done = widget;
		}

	return &color;
}

static void dupe_listview_color_cb(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
				   GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
	DupeWindow *dw = data;
	gboolean set;

	gtk_tree_model_get(tree_model, iter, DUPE_COLUMN_COLOR, &set, -1);
	g_object_set(G_OBJECT(cell),
		     "cell-background-gdk", dupe_listview_color_shifted(dw->listview),
		     "cell-background-set", set, NULL);
}

static void dupe_listview_add_column(DupeWindow *dw, GtkWidget *listview, gint n, const gchar *title, gint image, gint right_justify)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, title);
	gtk_tree_view_column_set_min_width(column, 4);

	if (n != DUPE_COLUMN_RANK &&
	    n != DUPE_COLUMN_THUMB)
		{
		gtk_tree_view_column_set_resizable(column, TRUE);
		}

	if (!image)
		{
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
		renderer = gtk_cell_renderer_text_new();
		if (right_justify)
			{
			g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);
			}
		gtk_tree_view_column_pack_start(column, renderer, TRUE);
		gtk_tree_view_column_add_attribute(column, renderer, "text", n);
		}
	else
		{
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
		renderer = gtk_cell_renderer_pixbuf_new();
		cell_renderer_height_override(renderer);
		gtk_tree_view_column_pack_start(column, renderer, TRUE);
		gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", n);
		}

	if (listview == dw->listview)
		{
		/* sets background before rendering */
		gtk_tree_view_column_set_cell_data_func(column, renderer, dupe_listview_color_cb, dw, NULL);
		}

	gtk_tree_view_append_column(GTK_TREE_VIEW(listview), column);
}

static void dupe_listview_set_height(GtkWidget *listview, gint thumb)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell;
	GList *list;

	column = gtk_tree_view_get_column(GTK_TREE_VIEW(listview), DUPE_COLUMN_THUMB - 1);
	if (!column) return;

	gtk_tree_view_column_set_fixed_width(column, (thumb) ? thumb_max_width : 4);
	
	list = gtk_tree_view_column_get_cell_renderers(column);
	if (!list) return;
	cell = list->data;
	g_list_free(list);

	g_object_set(G_OBJECT(cell), "height", (thumb) ? thumb_max_height : -1, NULL);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(listview));
}


/*
 *-------------------------------------------------------------------
 * misc cb
 *-------------------------------------------------------------------
 */

static void dupe_window_show_thumb_cb(GtkWidget *widget, gpointer data)
{
	DupeWindow *dw = data;

	dw->show_thumbs = GTK_TOGGLE_BUTTON(widget)->active;

	if (dw->show_thumbs)
		{
		if (!dw->working) dupe_thumb_step(dw);
		}
	else
		{
		GtkTreeModel *store;
		GtkTreeIter iter;
		gint valid;

		thumb_loader_free(dw->thumb_loader);
		dw->thumb_loader = NULL;

		store = gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview));
		valid = gtk_tree_model_get_iter_first(store, &iter);

		while (valid)
			{
			gtk_list_store_set(GTK_LIST_STORE(store), &iter, DUPE_COLUMN_THUMB, NULL, -1);
			valid = gtk_tree_model_iter_next(store, &iter);
			}
		dupe_window_update_progress(dw, NULL, 0.0, FALSE);
		}

	dupe_listview_set_height(dw->listview, dw->show_thumbs);
}

static void dupe_popup_menu_pos_cb(GtkMenu *menu, gint *x, gint *y, gboolean *push_in, gpointer data)
{
	GtkWidget *view = data;
	GtkTreePath *tpath;
	gint cx, cy, cw, ch;
	gint column;

	gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &tpath, NULL);
	if (!tpath) return;

	if (gtk_tree_view_get_column(GTK_TREE_VIEW(view), DUPE_COLUMN_NAME - 1) != NULL)
		{
		column = DUPE_COLUMN_NAME - 1;
		}
	else
		{
		/* dw->second_listview */
		column = 0;
		}
	tree_view_get_cell_clamped(GTK_TREE_VIEW(view), tpath, column, TRUE, &cx, &cy, &cw, &ch);
	gtk_tree_path_free(tpath);
	cy += ch;
	popup_menu_position_clamp(menu, &cx, &cy, 0);
	*x = cx;
	*y = cy;
}

static gint dupe_window_keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	DupeWindow *dw = data;
	gint stop_signal = FALSE;
	gint on_second;
	GtkWidget *listview;
	GtkTreeModel *store;
	GtkTreeSelection *selection;
	GList *slist;
	DupeItem *di = NULL;

	on_second = GTK_WIDGET_HAS_FOCUS(dw->second_listview);

	if (on_second)
		{
		listview = dw->second_listview;
		}
	else
		{
		listview = dw->listview;
		}

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(listview));
	slist = gtk_tree_selection_get_selected_rows(selection, &store);
	if (slist)
		{
		GtkTreePath *tpath;
		GtkTreeIter iter;
		GList *last;

		last = g_list_last(slist);
		tpath = last->data;

		/* last is newest selected file */
		gtk_tree_model_get_iter(store, &iter, tpath);
		gtk_tree_model_get(store, &iter, DUPE_COLUMN_POINTER, &di, -1);
		}
	g_list_foreach(slist, (GFunc)gtk_tree_path_free, NULL);
	g_list_free(slist);

	if (event->state & GDK_CONTROL_MASK)
		{
		gint edit_val = -1;

		if (!on_second)
		    switch (event->keyval)
			{
			case '1':
				edit_val = 0;
				break;
			case '2':
				edit_val = 1;
				break;
			case '3':
				edit_val = 2;
				break;
			case '4':
				edit_val = 3;
				break;
			case '5':
				edit_val = 4;
				break;
			case '6':
				edit_val = 5;
				break;
			case '7':
				edit_val = 6;
				break;
			case '8':
				edit_val = 7;
				break;
			case '9':
				edit_val = 8;
				break;
			case '0':
				edit_val = 9;
				break;
			case 'C': case 'c':
				stop_signal = TRUE;
				file_util_copy(NULL, dupe_listview_get_selection(dw, listview), NULL, dw->window);
				break;
			case 'M': case 'm':
				file_util_move(NULL, dupe_listview_get_selection(dw, listview), NULL, dw->window);
				stop_signal = TRUE;
				break;
			case 'R': case 'r':
				file_util_rename(NULL, dupe_listview_get_selection(dw, listview), dw->window);
				stop_signal = TRUE;
				break;
			case 'D': case 'd':
				file_util_delete(NULL, dupe_listview_get_selection(dw, listview), dw->window);
				stop_signal = TRUE;
				break;
			case 'P': case 'p':
				info_window_new(NULL, dupe_listview_get_selection(dw, listview));
				stop_signal = TRUE;
				break;
			default:
				break;
			}

		switch (event->keyval)
			{
			case 'A': case 'a':
				if (event->state & GDK_SHIFT_MASK)
					{
					gtk_tree_selection_unselect_all(selection);
					}
				else
					{
					gtk_tree_selection_select_all(selection);
					}
				stop_signal = TRUE;
				break;
			case GDK_Delete: case GDK_KP_Delete:
				if (on_second)
					{
					dupe_second_clear(dw);
					dupe_window_recompare(dw);
					}
				else
					{
					dupe_window_clear(dw);
					}
				stop_signal = TRUE;
				break;
			case 'L': case 'l':
				dupe_window_append_file_list(dw, FALSE);
				stop_signal = TRUE;
				break;
			case 'T': case 't':
				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dw->button_thumbs),
					!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dw->button_thumbs)));
				stop_signal = TRUE;
				break;
			case 'W': case 'w':
				dupe_window_close(dw);
				stop_signal = TRUE;
				break;
			default:
				break;
			}

		if (edit_val >= 0)
			{
			dupe_window_edit_selected(dw, edit_val);
			stop_signal = TRUE;
			}
		}
	else
		{
		switch (event->keyval)
			{
			case GDK_Return: case GDK_KP_Enter:
				dupe_menu_view(dw, di, listview, FALSE);
				stop_signal = TRUE;
				break;
			case 'V': case 'v':
				stop_signal = TRUE;
				dupe_menu_view(dw, di, listview, TRUE);
				break;
			case GDK_Delete: case GDK_KP_Delete:
				dupe_window_remove_selection(dw, listview);
				stop_signal = TRUE;
				break;
			case 'C': case 'c':
				if (!on_second)
					{
					dupe_window_collection_from_selection(dw);
					stop_signal = TRUE;
					}
				break;
			case '1':
				dupe_listview_select_dupes(dw, TRUE);
				stop_signal = TRUE;
				break;
			case '2':
				dupe_listview_select_dupes(dw, FALSE);
				stop_signal = TRUE;
				break;
			case GDK_Menu:
			case GDK_F10:
				if (!on_second)
					{
					GtkWidget *menu;

					menu = dupe_menu_popup_main(dw, di);
					gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
						       dupe_popup_menu_pos_cb, listview, 0, GDK_CURRENT_TIME);
					}
				else
					{
					GtkWidget *menu;

					menu = dupe_menu_popup_second(dw, di);
					gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
						       dupe_popup_menu_pos_cb, listview, 0, GDK_CURRENT_TIME);
					}
				break;
			default:
				break;
			}
		}

	return stop_signal;
}


void dupe_window_clear(DupeWindow *dw)
{
	GtkListStore *store;

	dupe_check_stop(dw);

	store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(dw->listview)));
	gtk_list_store_clear(store);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(dw->listview));

	g_list_free(dw->dupes);
	dw->dupes = NULL;

	dupe_list_free(dw->list);
	dw->list = NULL;

	dupe_match_reset_list(dw->second_list);

	dupe_window_update_count(dw, FALSE);
	dupe_window_update_progress(dw, NULL, 0.0, FALSE);
}

void dupe_window_close(DupeWindow *dw)
{
	dupe_check_stop(dw);

	dupe_window_list = g_list_remove(dupe_window_list, dw);
	gtk_widget_destroy(dw->window);

	g_list_free(dw->dupes);
	dupe_list_free(dw->list);

	dupe_list_free(dw->second_list);

	g_free(dw);
}

static gint dupe_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	DupeWindow *dw = data;
	dupe_window_close(dw);

	return TRUE;
}

/* collection and files can be NULL */
DupeWindow *dupe_window_new(DupeMatchType match_mask)
{
	DupeWindow *dw;
	GtkWidget *vbox;
	GtkWidget *scrolled;
	GtkWidget *frame;
	GtkWidget *status_box;
	GtkWidget *label;
	GtkWidget *button;
	GtkListStore *store;
	GtkTreeSelection *selection;
	GdkGeometry geometry;

	dw = g_new0(DupeWindow, 1);

	dw->list = NULL;
	dw->dupes = NULL;
	dw->match_mask = match_mask;
	dw->show_thumbs = FALSE;

	dw->idle_id = -1;

	dw->second_set = FALSE;

	dw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        window_set_icon(dw->window, NULL, NULL);

	geometry.min_width = 32;
	geometry.min_height = 32;
	geometry.base_width = DUPE_DEF_WIDTH;
	geometry.base_height = DUPE_DEF_HEIGHT;
	gtk_window_set_geometry_hints(GTK_WINDOW(dw->window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);

	gtk_window_set_default_size(GTK_WINDOW(dw->window), DUPE_DEF_WIDTH, DUPE_DEF_HEIGHT);

	gtk_window_set_resizable(GTK_WINDOW(dw->window), TRUE);
	gtk_window_set_title(GTK_WINDOW(dw->window), _("Find duplicates - GQview"));
        gtk_window_set_wmclass(GTK_WINDOW(dw->window), "dupe", "GQview");
        gtk_container_set_border_width (GTK_CONTAINER (dw->window), 0);

        g_signal_connect(G_OBJECT(dw->window), "delete_event",
			 G_CALLBACK(dupe_window_delete), dw);
	g_signal_connect(G_OBJECT(dw->window), "key_press_event",
			 G_CALLBACK(dupe_window_keypress_cb), dw);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(dw->window), vbox);
	gtk_widget_show(vbox);

	dw->table = gtk_table_new(1, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), dw->table, TRUE, TRUE, 0);
	gtk_widget_show(dw->table);

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_table_attach_defaults(GTK_TABLE(dw->table), scrolled, 0, 2, 0, 1);
        gtk_widget_show(scrolled);

	store = gtk_list_store_new(9, G_TYPE_POINTER, G_TYPE_STRING, GDK_TYPE_PIXBUF,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
	dw->listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
	g_object_unref(store);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->listview));
	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection), GTK_SELECTION_MULTIPLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dw->listview), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(dw->listview), FALSE);

	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_RANK, "", FALSE, TRUE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_THUMB, "", TRUE, FALSE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_NAME, _("Name"), FALSE, FALSE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_SIZE, _("Size"), FALSE, TRUE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_DATE, _("Date"), FALSE, TRUE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_DIMENSIONS, _("Dimensions"), FALSE, FALSE);
	dupe_listview_add_column(dw, dw->listview, DUPE_COLUMN_PATH, _("Path"), FALSE, FALSE);

	gtk_container_add(GTK_CONTAINER(scrolled), dw->listview);
	gtk_widget_show(dw->listview);

	dw->second_vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach_defaults(GTK_TABLE(dw->table), dw->second_vbox, 2, 3, 0, 1);
	if (dw->second_set)
		{
		gtk_table_set_col_spacings(GTK_TABLE(dw->table), PREF_PAD_GAP);
		gtk_widget_show(dw->second_vbox);
		}
	else
		{
		gtk_table_set_col_spacings(GTK_TABLE(dw->table), 0);
		}

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(dw->second_vbox), scrolled, TRUE, TRUE, 0);
        gtk_widget_show(scrolled);

	store = gtk_list_store_new(2, G_TYPE_POINTER, G_TYPE_STRING);
	dw->second_listview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dw->second_listview));
	gtk_tree_selection_set_mode(GTK_TREE_SELECTION(selection), GTK_SELECTION_MULTIPLE);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(dw->second_listview), TRUE);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(dw->second_listview), FALSE);

	dupe_listview_add_column(dw, dw->second_listview, 1, _("Compare to:"), FALSE, FALSE);

	gtk_container_add(GTK_CONTAINER(scrolled), dw->second_listview);
        gtk_widget_show(dw->second_listview);

	dw->second_status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(dw->second_vbox), dw->second_status_label, FALSE, FALSE, 0);
	gtk_widget_show(dw->second_status_label);

	pref_line(dw->second_vbox, GTK_ORIENTATION_HORIZONTAL);

	status_box = pref_box_new(vbox, FALSE, GTK_ORIENTATION_HORIZONTAL, 0);

	label = gtk_label_new(_("Compare by:"));
	gtk_box_pack_start(GTK_BOX(status_box), label, FALSE, FALSE, PREF_PAD_SPACE);
	gtk_widget_show(label);

	dupe_menu_setup(dw);
	gtk_box_pack_start(GTK_BOX(status_box), dw->combo, FALSE, FALSE, 0);
	gtk_widget_show(dw->combo);

	dw->button_thumbs = gtk_check_button_new_with_label(_("Thumbnails"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dw->button_thumbs), dw->show_thumbs);
	g_signal_connect(G_OBJECT(dw->button_thumbs), "toggled",
			 G_CALLBACK(dupe_window_show_thumb_cb), dw);
	gtk_box_pack_start(GTK_BOX(status_box), dw->button_thumbs, FALSE, FALSE, PREF_PAD_SPACE);
	gtk_widget_show(dw->button_thumbs);

	button = gtk_check_button_new_with_label(_("Compare two file sets"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), dw->second_set);
	g_signal_connect(G_OBJECT(button), "toggled",
			 G_CALLBACK(dupe_second_set_toggle_cb), dw);
	gtk_box_pack_end(GTK_BOX(status_box), button, FALSE, FALSE, PREF_PAD_SPACE);
	gtk_widget_show(button);

	status_box = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), status_box, FALSE, FALSE, 0);
        gtk_widget_show(status_box);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(status_box), frame, TRUE, TRUE, 0);
	gtk_widget_show(frame);

	dw->status_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(frame), dw->status_label);
	gtk_widget_show(dw->status_label);

	dw->extra_label = gtk_progress_bar_new();
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dw->extra_label), 0.0);
	gtk_box_pack_end(GTK_BOX(status_box), dw->extra_label, FALSE, FALSE, 0);
	gtk_widget_show(dw->extra_label);

	dupe_dnd_init(dw);

	/* order is important here, dnd_init should be seeing mouse
	 * presses before we possibly handle (and stop) the signal
	 */
	g_signal_connect(G_OBJECT(dw->listview), "button_press_event",
			 G_CALLBACK(dupe_listview_press_cb), dw);
	g_signal_connect(G_OBJECT(dw->listview), "button_release_event",
			 G_CALLBACK(dupe_listview_release_cb), dw);
	g_signal_connect(G_OBJECT(dw->second_listview), "button_press_event",
			 G_CALLBACK(dupe_listview_press_cb), dw);
	g_signal_connect(G_OBJECT(dw->second_listview), "button_release_event",
			 G_CALLBACK(dupe_listview_release_cb), dw);

	gtk_widget_show(dw->window);

	dupe_window_update_count(dw, TRUE);
	dupe_window_update_progress(dw, NULL, 0.0, FALSE);

	dupe_window_list = g_list_append(dupe_window_list, dw);

	return dw;
}

/*
 *-------------------------------------------------------------------
 * dnd confirm dir
 *-------------------------------------------------------------------
 */

typedef struct {
	DupeWindow *dw;
	GList *list;
} CDupeConfirmD;

static void confirm_dir_list_cancel(GtkWidget *widget, gpointer data)
{
	/* do nothing */
}

static void confirm_dir_list_add(GtkWidget *widget, gpointer data)
{
	CDupeConfirmD *d = data;
	GList *work;

	dupe_window_add_files(d->dw, d->list, FALSE);

	work = d->list;
	while (work)
		{
		gchar *path = work->data;
		work = work->next;
		if (isdir(path))
			{
			GList *list = NULL;

			path_list(path, &list, NULL);
			list = path_list_filter(list, FALSE);
			if (list)
				{
				dupe_window_add_files(d->dw, list, FALSE);
				path_list_free(list);
				}
			}
		}
}

static void confirm_dir_list_recurse(GtkWidget *widget, gpointer data)
{
	CDupeConfirmD *d = data;
	dupe_window_add_files(d->dw, d->list, TRUE);
}

static void confirm_dir_list_skip(GtkWidget *widget, gpointer data)
{
	CDupeConfirmD *d = data;
	dupe_window_add_files(d->dw, d->list, FALSE);
}

static void confirm_dir_list_destroy(GtkWidget *widget, gpointer data)
{
	CDupeConfirmD *d = data;
	path_list_free(d->list);
	g_free(d);
}

static GtkWidget *dupe_confirm_dir_list(DupeWindow *dw, GList *list)
{
	GtkWidget *menu;
	CDupeConfirmD *d;

	d = g_new0(CDupeConfirmD, 1);
	d->dw = dw;
	d->list = list;

	menu = popup_menu_short_lived();
	g_signal_connect(G_OBJECT(menu), "destroy",
			 G_CALLBACK(confirm_dir_list_destroy), d);

	menu_item_add_stock(menu, _("Dropped list includes folders."), GTK_STOCK_DND_MULTIPLE, NULL, NULL);
	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("_Add contents"), GTK_STOCK_OK, G_CALLBACK(confirm_dir_list_add), d);
	menu_item_add_stock(menu, _("Add contents _recursive"), GTK_STOCK_ADD, G_CALLBACK(confirm_dir_list_recurse), d);
	menu_item_add_stock(menu, _("_Skip folders"), GTK_STOCK_REMOVE, G_CALLBACK(confirm_dir_list_skip), d);
	menu_item_add_divider(menu);
	menu_item_add_stock(menu, _("Cancel"), GTK_STOCK_CANCEL, G_CALLBACK(confirm_dir_list_cancel), d);

	return menu;
}

/*
 *-------------------------------------------------------------------
 * dnd
 *-------------------------------------------------------------------
 */

static GtkTargetEntry dupe_drag_types[] = {
        { "text/uri-list", 0, TARGET_URI_LIST },
        { "text/plain", 0, TARGET_TEXT_PLAIN }
};
static gint n_dupe_drag_types = 2;

static GtkTargetEntry dupe_drop_types[] = {
        { "application/x-gqview-collection-member", 0, TARGET_APP_COLLECTION_MEMBER },
        { "text/uri-list", 0, TARGET_URI_LIST }
};
static gint n_dupe_drop_types = 2;

static void dupe_dnd_data_set(GtkWidget *widget, GdkDragContext *context,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data)
{
	DupeWindow *dw = data;
	gchar *uri_text;
	gint length;
	GList *list;

	switch (info)
		{
		case TARGET_URI_LIST:
		case TARGET_TEXT_PLAIN:
			list = dupe_listview_get_selection(dw, widget);
			if (!list) return;
			uri_text = uri_text_from_list(list, &length, (info == TARGET_TEXT_PLAIN));
			path_list_free(list);
			break;
		default:
			uri_text = NULL;
			break;
		}

	if (uri_text) gtk_selection_data_set(selection_data, selection_data->target,
					     8, (guchar *)uri_text, length);
	g_free(uri_text);
}

static void dupe_dnd_data_get(GtkWidget *widget, GdkDragContext *context,
			      gint x, gint y,
			      GtkSelectionData *selection_data, guint info,
			      guint time, gpointer data)
{
	DupeWindow *dw = data;
	GtkWidget *source;
	GList *list = NULL;
	GList *work;

	source = gtk_drag_get_source_widget(context);
	if (source == dw->listview || source == dw->second_listview) return;

	dw->second_drop = (dw->second_set && widget == dw->second_listview);

	switch (info)
		{
		case TARGET_APP_COLLECTION_MEMBER:
			collection_from_dnd_data((gchar *)selection_data->data, &list, NULL);
			break;
		case TARGET_URI_LIST:
			list = uri_list_from_text((gchar *)selection_data->data, TRUE);
			work = list;
			while(work)
				{
				if (isdir((gchar *)work->data))
					{
					GtkWidget *menu;
					menu = dupe_confirm_dir_list(dw, list);
					gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 0, time);
					return;
					}
				work = work->next;
				}
			break;
		default:
			list = NULL;
			break;
		}

	if (list)
		{
		dupe_window_add_files(dw, list, FALSE);
		path_list_free(list);
		}
}

static void dupe_dest_set(GtkWidget *widget, gint enable)
{
	if (enable)
		{
		gtk_drag_dest_set(widget,
			GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
			dupe_drop_types, n_dupe_drop_types,
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_ASK);

		}
	else
		{
		gtk_drag_dest_unset(widget);
		}
}

static void dupe_dnd_begin(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	DupeWindow *dw = data;
	dupe_dest_set(dw->listview, FALSE);
	dupe_dest_set(dw->second_listview, FALSE);

	if (dw->click_item && !dupe_listview_item_is_selected(dw, dw->click_item, widget))
		{
		GtkListStore *store;
		GtkTreeIter iter;

		store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(widget)));
		if (dupe_listview_find_item(store, dw->click_item, &iter) >= 0)
			{
			GtkTreeSelection *selection;
			GtkTreePath *tpath;

			selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_iter(selection, &iter);

			tpath = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &iter);
			gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), tpath, NULL, FALSE);
			gtk_tree_path_free(tpath);
			}
		}

	if (dw->show_thumbs &&
	    widget == dw->listview &&
	    dw->click_item && dw->click_item->pixbuf)
		{
		GtkTreeSelection *selection;
		gint items;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		items = gtk_tree_selection_count_selected_rows(selection);
		dnd_set_drag_icon(widget, context, dw->click_item->pixbuf, items);
		}
}

static void dupe_dnd_end(GtkWidget *widget, GdkDragContext *context, gpointer data)
{
	DupeWindow *dw = data;
	dupe_dest_set(dw->listview, TRUE);
	dupe_dest_set(dw->second_listview, TRUE);
}

static void dupe_dnd_init(DupeWindow *dw)
{
	gtk_drag_source_set(dw->listview, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    dupe_drag_types, n_dupe_drag_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(dw->listview), "drag_data_get",
			 G_CALLBACK(dupe_dnd_data_set), dw);
	g_signal_connect(G_OBJECT(dw->listview), "drag_begin",
			 G_CALLBACK(dupe_dnd_begin), dw);
	g_signal_connect(G_OBJECT(dw->listview), "drag_end",
			 G_CALLBACK(dupe_dnd_end), dw);

	dupe_dest_set(dw->listview, TRUE);
	g_signal_connect(G_OBJECT(dw->listview), "drag_data_received",
			 G_CALLBACK(dupe_dnd_data_get), dw);

	gtk_drag_source_set(dw->second_listview, GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			    dupe_drag_types, n_dupe_drag_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect(G_OBJECT(dw->second_listview), "drag_data_get",
			 G_CALLBACK(dupe_dnd_data_set), dw);
	g_signal_connect(G_OBJECT(dw->second_listview), "drag_begin",
			 G_CALLBACK(dupe_dnd_begin), dw);
	g_signal_connect(G_OBJECT(dw->second_listview), "drag_end",
			 G_CALLBACK(dupe_dnd_end), dw);

	dupe_dest_set(dw->second_listview, TRUE);
	g_signal_connect(G_OBJECT(dw->second_listview), "drag_data_received",
			 G_CALLBACK(dupe_dnd_data_get), dw);
}

/*
 *-------------------------------------------------------------------
 * maintenance (move, delete, etc.)
 *-------------------------------------------------------------------
 */

void dupe_maint_removed(const gchar *path)
{
	GList *work;

	work = dupe_window_list;
	while (work)
		{
		DupeWindow *dw = work->data;
		work = work->next;

		while (dupe_item_remove_by_path(dw, path));
		}
}

void dupe_maint_renamed(const gchar *source, const gchar *dest)
{
	GList *work;

	work = dupe_window_list;
	while (work)
		{
		DupeWindow *dw = work->data;
		work = work->next;

		while (dupe_item_set_path(dw, source, dest));
		}

}

