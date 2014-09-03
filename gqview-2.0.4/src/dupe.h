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


#ifndef DUPE_H
#define DUPE_H

#include "similar.h"

/* match methods */
typedef enum
{
	DUPE_MATCH_NONE = 0,
	DUPE_MATCH_NAME = 1 << 0,
	DUPE_MATCH_SIZE = 1 << 1,
	DUPE_MATCH_DATE = 1 << 2,
	DUPE_MATCH_DIM  = 1 << 3,	/* image dimensions */
	DUPE_MATCH_SUM  = 1 << 4,	/* simple checksum */
	DUPE_MATCH_PATH = 1 << 5,
	DUPE_MATCH_SIM_HIGH = 1 << 6,	/* similarity */
	DUPE_MATCH_SIM_MED  = 1 << 7,
	DUPE_MATCH_SIM_LOW  = 1 << 8,
	DUPE_MATCH_SIM_CUSTOM = 1 << 9
} DupeMatchType;

typedef struct _DupeItem DupeItem;
struct _DupeItem
{
	CollectionData *collection;	/* NULL if from DupeWindow->files */
	CollectInfo *info;

	gchar *path;
	const gchar *name;		/* we store the pointer into path for the name,
					 * so that we only need to calculate this once,
					 * which significantly speeds up comparing names */
	gint64 size;
	time_t date;

	long checksum;
	gchar *md5sum;
	gint width;
	gint height;

	ImageSimilarityData *simd;

	/* thumb */
	GdkPixbuf *pixbuf;

	GList *group;			/* List of match data */
	gdouble group_rank;

	gint second;
};

typedef struct _DupeMatch DupeMatch;
struct _DupeMatch
{
	DupeItem *di;
	gdouble rank;
};

typedef struct _DupeWindow DupeWindow;
struct _DupeWindow
{
	GList *list;			/* dropped files (DupeItem) */
	GList *dupes;			/* list of dupes (DupeItem, grouping the DupeMatches) */
	DupeMatchType match_mask;	/* mask of things to check for match */

	GtkWidget *window;
	GtkWidget *table;
	GtkWidget *listview;
	GtkWidget *combo;
	GtkWidget *status_label;
	GtkWidget *extra_label;
	GtkWidget *button_thumbs;

	gint show_thumbs;

	gint idle_id;
	GList *working;
	gint setup_done;
	gint setup_count;
	gint setup_n;			/* these are merely for speed optimization */
	GList *setup_point;		/* ditto */
	DupeMatchType setup_mask;	/* ditto */
	guint64 setup_time;
	guint64 setup_time_count;

	DupeItem *click_item;		/* for popup menu */

	ThumbLoader *thumb_loader;
	DupeItem *thumb_item;

	ImageLoader *img_loader;

	/* second set comparison stuff */

	gint second_set;		/* second set enabled ? */
	GList *second_list;		/* second set dropped files */
	gint second_drop;		/* drop is on second set */

	GtkWidget *second_vbox;		/* box of second widgets */
	GtkWidget *second_listview;
	GtkWidget *second_status_label;

	gint color_frozen;
};


DupeWindow *dupe_window_new(DupeMatchType match_mask);

void dupe_window_clear(DupeWindow *dw);
void dupe_window_close(DupeWindow *dw);

void dupe_window_add_collection(DupeWindow *dw, CollectionData *collection);
void dupe_window_add_files(DupeWindow *dw, GList *list, gint recurse);

void dupe_maint_removed(const gchar *path);
void dupe_maint_renamed(const gchar *source, const gchar *dest);


/* cell max with/height hack utility */
void cell_renderer_height_override(GtkCellRenderer *renderer);


#endif


