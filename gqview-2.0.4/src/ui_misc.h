/*
 * (SLIK) SimpLIstic sKin functions
 * (C) 2004 John Ellis
 *
 * Author: John Ellis
 *
 * This software is released under the GNU General Public License (GNU GPL).
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at your own risk!
 */


#ifndef UI_MISC_H
#define UI_MISC_H


#include <sys/time.h>
#include <sys/types.h>
#include <time.h>


/* these values are per GNOME HIG */

/* HIG 2.0 chapter 8 defines: */

/* space between elements within control (ex: icon and it's text) */
#define PREF_PAD_GAP     6
/* space between label and control(s) */
#define PREF_PAD_SPACE  12
/* space between window border and controls */
#define PREF_PAD_BORDER 12
/* indent for group members */
#define PREF_PAD_INDENT 12
/* vertical space between groups */
#define PREF_PAD_GROUP  18

/* HIG 2.0 chapter 3.13 defines: */

/* gap between buttons in a dialog */
#define PREF_PAD_BUTTON_GAP 6
/* space between buttons in a dialog and it's contents */
#define PREF_PAD_BUTTON_SPACE 24

/* and these are not in the GNOME HIG */

/* gap between similar toolbar items (buttons) */
#define PREF_PAD_TOOLBAR_GAP 0

/* HIG 2.0 states 6 pixels between icons and text,
 * but GTK's stock buttons ignore this (hard coded to 2), we do it too for consistency
 */
#define PREF_PAD_BUTTON_ICON_GAP 2


GtkWidget *pref_box_new(GtkWidget *parent_box, gint fill,
			GtkOrientation orientation, gint padding);

GtkWidget *pref_group_new(GtkWidget *parent_box, gint fill,
			  const gchar *text, GtkOrientation orientation);
GtkWidget *pref_group_parent(GtkWidget *child);

GtkWidget *pref_frame_new(GtkWidget *parent_box, gint fill,
			  const gchar *text,
			  GtkOrientation orientation, gint padding);

GtkWidget *pref_spacer(GtkWidget *parent_box, gint padding);
GtkWidget *pref_line(GtkWidget *parent_box, gint padding);

GtkWidget *pref_label_new(GtkWidget *parent_box, const gchar *text);
GtkWidget *pref_label_new_mnemonic(GtkWidget *parent_box, const gchar *text, GtkWidget *widget);
void pref_label_bold(GtkWidget *label, gint bold, gint increase_size);

GtkWidget *pref_button_new(GtkWidget *parent_box, const gchar *stock_id,
			   const gchar *text, gint hide_stock_text,
			   GCallback func, gpointer data);

GtkWidget *pref_checkbox_new(GtkWidget *parent_box, const gchar *text, gint active,
			     GCallback func, gpointer data);
GtkWidget *pref_checkbox_new_mnemonic(GtkWidget *parent_box, const gchar *text, gint active,
				      GCallback func, gpointer data);

GtkWidget *pref_checkbox_new_int(GtkWidget *parent_box, const gchar *text, gint active,
				 gint *result);

void pref_checkbox_link_sensitivity(GtkWidget *button, GtkWidget *widget);
void pref_checkbox_link_sensitivity_swap(GtkWidget *button, GtkWidget *widget);

GtkWidget *pref_radiobutton_new(GtkWidget *parent_box, GtkWidget *sibling,
				const gchar *text, gint active,
				GCallback func, gpointer data);
GtkWidget *pref_radiobutton_new_mnemonic(GtkWidget *parent_box, GtkWidget *sibling,
					 const gchar *text, gint active,
					 GCallback func, gpointer data);

GtkWidget *pref_radiobutton_new_int(GtkWidget *parent_box, GtkWidget *sibling,
				    const gchar *text, gint active,
				    gint *result, gint value,
				    GCallback func, gpointer data);

GtkWidget *pref_spin_new(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
			 gdouble min, gdouble max, gdouble step, gint digits,
			 gdouble value,
			 GCallback func, gpointer data);
GtkWidget *pref_spin_new_mnemonic(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
				  gdouble min, gdouble max, gdouble step, gint digits,
				  gdouble value,
				  GCallback func, gpointer data);

GtkWidget *pref_spin_new_int(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
			     gint min, gint max, gint step,
			     gint value, gint *value_var);

void pref_link_sensitivity(GtkWidget *widget, GtkWidget *watch);

void pref_signal_block_data(GtkWidget *widget, gpointer data);
void pref_signal_unblock_data(GtkWidget *widget, gpointer data);


GtkWidget *pref_table_new(GtkWidget *parent_box, gint columns, gint rows,
			  gint homogenious, gint fill);

GtkWidget *pref_table_box(GtkWidget *table, gint column, gint row,
			  GtkOrientation orientation, const gchar *text);

GtkWidget *pref_table_label(GtkWidget *table, gint column, gint row,
			    const gchar *text, gfloat alignment);

GtkWidget *pref_table_button(GtkWidget *table, gint column, gint row,
			     const gchar *stock_id, const gchar *text, gint hide_stock_text,
			     GCallback func, gpointer data);

GtkWidget *pref_table_spin(GtkWidget *table, gint column, gint row,
			   const gchar *text, const gchar *suffix,
			   gdouble min, gdouble max, gdouble step, gint digits,
			   gdouble value,
			   GCallback func, gpointer data);


GtkWidget *pref_toolbar_new(GtkWidget *parent_box, GtkToolbarStyle style);
GtkWidget *pref_toolbar_button(GtkWidget *toolbar, const gchar *stock_id, const gchar *label,
			       const gchar *description,
			       GCallback func, gpointer data);
GtkWidget *pref_toolbar_spacer(GtkWidget *toolbar);


GtkWidget *date_selection_new(void);

void date_selection_set(GtkWidget *widget, gint day, gint month, gint year);
void date_selection_get(GtkWidget *widget, gint *day, gint *month, gint *year);

void date_selection_time_set(GtkWidget *widget, time_t t);
time_t date_selection_time_get(GtkWidget *widget);


typedef enum {
	SIZER_POS_LEFT   = 1 << 0,
	SIZER_POS_RIGHT  = 1 << 1,
	SIZER_POS_TOP    = 1 << 2,
	SIZER_POS_BOTTOM = 1 << 3
} SizerPositionType;

GtkWidget *sizer_new(GtkWidget *parent, GtkWidget *bounding_widget,
		     SizerPositionType position);

void sizer_set_limits(GtkWidget *sizer,
                      gint hsize_min, gint hsize_max,
                      gint vsize_min, gint vsize_max);


void pref_list_int_set(const gchar *group, const gchar *key, gint value);
gint pref_list_int_get(const gchar *group, const gchar *key, gint *result);

void pref_list_double_set(const gchar *group, const gchar *key, gdouble value);
gint pref_list_double_get(const gchar *group, const gchar *key, gdouble *result);

void pref_list_string_set(const gchar *group, const gchar *key, const gchar *value);
gint pref_list_string_get(const gchar *group, const gchar *key, const gchar **result);


#endif

