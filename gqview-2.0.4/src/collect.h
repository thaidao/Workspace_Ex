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


#ifndef COLLECT_H
#define COLLECT_H


CollectInfo *collection_info_new(const gchar *path, struct stat *st, GdkPixbuf *pixbuf);

void collection_info_free_thumb(CollectInfo *ci);
void collection_info_free(CollectInfo *ci);

void collection_info_set_thumb(CollectInfo *ci, GdkPixbuf *pixbuf);
gint collection_info_load_thumb(CollectInfo *ci);

void collection_list_free(GList *list);

GList *collection_list_sort(GList *list, SortType method);
GList *collection_list_add(GList *list, CollectInfo *ci, SortType method);
GList *collection_list_insert(GList *list, CollectInfo *ci, CollectInfo *insert_ci, SortType method);
GList *collection_list_remove(GList *list, CollectInfo *ci);
CollectInfo *collection_list_find(GList *list, const gchar *path);
GList *collection_list_to_path_list(GList *list);

CollectionData *collection_new(const gchar *path);
void collection_free(CollectionData *cd);

void collection_ref(CollectionData *cd);
void collection_unref(CollectionData *cd);

void collection_path_changed(CollectionData *cd);

gint collection_to_number(CollectionData *cd);
CollectionData *collection_from_number(gint n);

/* pass a NULL pointer to whatever you don't need
 * use free_selected_list to free list, and
 * g_list_free to free info_list, which is a list of
 * CollectInfo pointers into CollectionData
 */
CollectionData *collection_from_dnd_data(const gchar *data, GList **list, GList **info_list);
gchar *collection_info_list_to_dnd_data(CollectionData *cd, GList *list, gint *length);

gint collection_info_valid(CollectionData *cd, CollectInfo *info);

CollectInfo *collection_next_by_info(CollectionData *cd, CollectInfo *info);
CollectInfo *collection_prev_by_info(CollectionData *cd, CollectInfo *info);
CollectInfo *collection_get_first(CollectionData *cd);
CollectInfo *collection_get_last(CollectionData *cd);

void collection_set_sort_method(CollectionData *cd, SortType method);
void collection_set_update_info_func(CollectionData *cd,
				     void (*func)(CollectionData *, CollectInfo *, gpointer), gpointer data);

gint collection_add(CollectionData *cd, const gchar *path, gint sorted);
gint collection_add_check(CollectionData *cd, const gchar *path, gint sorted, gint must_exist);
gint collection_insert(CollectionData *cd, const gchar *path, CollectInfo *insert_ci, gint sorted);
gint collection_remove(CollectionData *cd, const gchar *path);
void collection_remove_by_info_list(CollectionData *cd, GList *list);
gint collection_rename(CollectionData *cd, const gchar *source, const gchar *dest);

void collection_update_geometry(CollectionData *cd);

void collection_maint_removed(const gchar *path);
void collection_maint_renamed(const gchar *source, const gchar *dest);

CollectWindow *collection_window_new(const gchar *path);
void collection_window_close_by_collection(CollectionData *cd);
CollectWindow *collection_window_find(CollectionData *cd);
CollectWindow *collection_window_find_by_path(const gchar *path);
gint collection_window_modified_exists(void);


#endif

