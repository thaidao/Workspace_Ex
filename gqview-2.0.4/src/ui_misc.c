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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "ui_misc.h"

#include "ui_bookmark.h"


/*
 *-----------------------------------------------------------------------------
 * widget and layout utilities
 *-----------------------------------------------------------------------------
 */

GtkWidget *pref_box_new(GtkWidget *parent_box, gint fill,
			GtkOrientation orientation, gint padding)
{
	GtkWidget *box;

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
		box = gtk_hbox_new(FALSE, padding);
		}
	else
		{
		box = gtk_vbox_new(FALSE, padding);
		}

	gtk_box_pack_start(GTK_BOX(parent_box), box, fill, fill, 0);
	gtk_widget_show(box);

	return box;
}

GtkWidget *pref_group_new(GtkWidget *parent_box, gint fill,
			  const gchar *text, GtkOrientation orientation)
{
	GtkWidget *box;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;

	vbox = gtk_vbox_new(FALSE, PREF_PAD_GAP);

	/* add additional spacing if necessary */
	if (GTK_IS_VBOX(parent_box) && GTK_BOX(parent_box)->children != NULL)
		{
		pref_spacer(vbox, PREF_PAD_GROUP - PREF_PAD_GAP);
		}

	gtk_box_pack_start(GTK_BOX(parent_box), vbox, fill, fill, 0);
	gtk_widget_show(vbox);

	label = gtk_label_new(text);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	pref_label_bold(label, TRUE, FALSE);

	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	hbox = gtk_hbox_new(FALSE, PREF_PAD_INDENT);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show(hbox);

	/* indent using empty box */
	pref_spacer(hbox, 0);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
		box = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
		}
	else
		{
		box = gtk_vbox_new(FALSE, PREF_PAD_GAP);
		}
	gtk_box_pack_start(GTK_BOX(hbox), box, TRUE, TRUE, 0);
	gtk_widget_show(box);

	g_object_set_data(G_OBJECT(box), "pref_group", vbox);

	return box;
}

GtkWidget *pref_group_parent(GtkWidget *child)
{
	GtkWidget *parent;

	parent = child;
	while (parent)
		{
		GtkWidget *group;

		group = g_object_get_data(G_OBJECT(parent), "pref_group");
		if (group && GTK_IS_WIDGET(group)) return group;

		parent = gtk_widget_get_parent(parent);
		}

	return child;
}

GtkWidget *pref_frame_new(GtkWidget *parent_box, gint fill,
			  const gchar *text,
			  GtkOrientation orientation, gint padding)
{
	GtkWidget *box;
	GtkWidget *frame = NULL;

	frame = gtk_frame_new(text);
	gtk_box_pack_start(GTK_BOX(parent_box), frame, fill, fill, 0);
	gtk_widget_show(frame);

	if (orientation == GTK_ORIENTATION_HORIZONTAL)
		{
		box = gtk_hbox_new(FALSE, padding);
		}
	else
		{
		box = gtk_vbox_new(FALSE, padding);
		}
	gtk_container_add(GTK_CONTAINER(frame), box);
	gtk_container_set_border_width(GTK_CONTAINER(box), PREF_PAD_BORDER);
	gtk_widget_show(box);

	return box;
}

GtkWidget *pref_spacer(GtkWidget *parent_box, gint padding)
{
	GtkWidget *spacer;

	spacer = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(parent_box), spacer, FALSE, FALSE, padding / 2);
	gtk_widget_show(spacer);

	return spacer;
}

GtkWidget *pref_line(GtkWidget *parent_box, gint padding)
{
	GtkWidget *spacer;

	if (GTK_IS_HBOX(parent_box))
		{
		spacer = gtk_vseparator_new();
		}
	else
		{
		spacer = gtk_hseparator_new();
		}

	gtk_box_pack_start(GTK_BOX(parent_box), spacer, FALSE, FALSE, padding / 2);
	gtk_widget_show(spacer);

	return spacer;
}

GtkWidget *pref_label_new(GtkWidget *parent_box, const gchar *text)
{
	GtkWidget *label;

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(parent_box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	return label;
}

GtkWidget *pref_label_new_mnemonic(GtkWidget *parent_box, const gchar *text, GtkWidget *widget)
{
	GtkWidget *label;

	label = gtk_label_new_with_mnemonic(text);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), widget);
	gtk_box_pack_start(GTK_BOX(parent_box), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	return label;
}

void pref_label_bold(GtkWidget *label, gint bold, gint increase_size)
{
	PangoAttrList *pal;
	PangoAttribute *pa;

	if (!bold && !increase_size) return;

	pal = pango_attr_list_new();

	if (bold)
		{
		pa = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
		pa->start_index = 0;
		pa->end_index = G_MAXINT;
		pango_attr_list_insert(pal, pa);
		}

	if (increase_size)
		{
		pa = pango_attr_scale_new(PANGO_SCALE_LARGE);
		pa->start_index = 0;
		pa->end_index = G_MAXINT;
		pango_attr_list_insert(pal, pa);
		}

	gtk_label_set_attributes(GTK_LABEL(label), pal);
	pango_attr_list_unref(pal);
}

GtkWidget *pref_button_new(GtkWidget *parent_box, const gchar *stock_id,
			   const gchar *text, gint hide_stock_text,
			   GCallback func, gpointer data)
{
	GtkWidget *button;

	if (stock_id && !text && !hide_stock_text)
		{
		button = gtk_button_new_from_stock(stock_id);
		}
	else
		{
		GtkWidget *image = NULL;
		GtkWidget *label = NULL;

		button = gtk_button_new();

		if (stock_id) image = gtk_image_new_from_stock(stock_id, GTK_ICON_SIZE_BUTTON);
		if (text)
			{
			label = gtk_label_new_with_mnemonic(text);
			gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
			gtk_label_set_mnemonic_widget(GTK_LABEL(label), button);
			}

		if (image && label)
			{
			GtkWidget *align;
			GtkWidget *hbox;

			hbox = gtk_hbox_new (FALSE, PREF_PAD_BUTTON_ICON_GAP);

			align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
			gtk_container_add(GTK_CONTAINER(button), align);
			gtk_widget_show(align);

			gtk_container_add(GTK_CONTAINER(align), hbox);
			gtk_widget_show(hbox);

			gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
			gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
			}
		else
			{
			if (image)
				{
				gtk_container_add(GTK_CONTAINER(button), image);
				}
			else if (label)
				{
				gtk_container_add(GTK_CONTAINER(button), label);
				}
			}

		if (image) gtk_widget_show(image);
		if (label) gtk_widget_show(label);
		}

	if (func) g_signal_connect(G_OBJECT(button), "clicked", func, data);

	if (parent_box)
		{
		gtk_box_pack_start(GTK_BOX(parent_box), button, FALSE, FALSE, 0);
		gtk_widget_show(button);
		}

	return button;
}

static GtkWidget *real_pref_checkbox_new(GtkWidget *parent_box, const gchar *text, gint mnemonic_text,
					 gint active, GCallback func, gpointer data)
{
	GtkWidget *button;

	if (mnemonic_text)
		{
		button = gtk_check_button_new_with_mnemonic(text);
		}
	else
		{
		button = gtk_check_button_new_with_label(text);
		}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
	if (func) g_signal_connect(G_OBJECT(button), "clicked", func, data);

	gtk_box_pack_start(GTK_BOX(parent_box), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	return button;
}

GtkWidget *pref_checkbox_new(GtkWidget *parent_box, const gchar *text, gint active,
			     GCallback func, gpointer data)
{
	return real_pref_checkbox_new(parent_box, text, FALSE, active, func, data);
}

GtkWidget *pref_checkbox_new_mnemonic(GtkWidget *parent_box, const gchar *text, gint active,
				      GCallback func, gpointer data)
{
	return real_pref_checkbox_new(parent_box, text, TRUE, active, func, data);
}

static void pref_checkbox_int_cb(GtkWidget *widget, gpointer data)
{
	gint *result = data;

	*result = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

GtkWidget *pref_checkbox_new_int(GtkWidget *parent_box, const gchar *text, gint active,
				 gint *result)
{
	GtkWidget *button;

	button = pref_checkbox_new(parent_box, text, active,
				   G_CALLBACK(pref_checkbox_int_cb), result);
	*result = active;

	return button;
}

static void pref_checkbox_link_sensitivity_cb(GtkWidget *button, gpointer data)
{
	GtkWidget *widget = data;

	gtk_widget_set_sensitive(widget, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void pref_checkbox_link_sensitivity(GtkWidget *button, GtkWidget *widget)
{
	g_signal_connect(G_OBJECT(button), "toggled",
			 G_CALLBACK(pref_checkbox_link_sensitivity_cb), widget);

	pref_checkbox_link_sensitivity_cb(button, widget);
}

static void pref_checkbox_link_sensitivity_swap_cb(GtkWidget *button, gpointer data)
{
	GtkWidget *widget = data;

	gtk_widget_set_sensitive(widget, !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

void pref_checkbox_link_sensitivity_swap(GtkWidget *button, GtkWidget *widget)
{
	g_signal_connect(G_OBJECT(button), "toggled",
			 G_CALLBACK(pref_checkbox_link_sensitivity_swap_cb), widget);

	pref_checkbox_link_sensitivity_swap_cb(button, widget);
}

static GtkWidget *real_pref_radiobutton_new(GtkWidget *parent_box, GtkWidget *sibling,
					    const gchar *text, gint mnemonic_text, gint active,
					    GCallback func, gpointer data)
{
	GtkWidget *button;
	GSList* group;

	if (sibling)
		{
		group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(sibling));
		}
	else
		{
		group = NULL;
		}

	if (mnemonic_text)
		{
		button = gtk_radio_button_new_with_mnemonic(group, text);
		}
	else
		{
		button = gtk_radio_button_new_with_label(group, text);
		}

	if (active) gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
	if (func) g_signal_connect(G_OBJECT(button), "clicked", func, data);

	gtk_box_pack_start(GTK_BOX(parent_box), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	return button;
}

GtkWidget *pref_radiobutton_new(GtkWidget *parent_box, GtkWidget *sibling,
				const gchar *text, gint active,
				GCallback func, gpointer data)
{
	return real_pref_radiobutton_new(parent_box, sibling, text, FALSE, active, func, data);
}

GtkWidget *pref_radiobutton_new_mnemonic(GtkWidget *parent_box, GtkWidget *sibling,
					 const gchar *text, gint active,
					 GCallback func, gpointer data)
{
	return real_pref_radiobutton_new(parent_box, sibling, text, TRUE, active, func, data);
}

#define PREF_RADIO_VALUE_KEY "pref_radio_value"

static void pref_radiobutton_int_cb(GtkWidget *widget, gpointer data)
{
	gint *result = data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
		{
		*result = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), PREF_RADIO_VALUE_KEY));
		}
}

GtkWidget *pref_radiobutton_new_int(GtkWidget *parent_box, GtkWidget *sibling,
				    const gchar *text, gint active,
				    gint *result, gint value,
				    GCallback func, gpointer data)
{
	GtkWidget *button;

	button = pref_radiobutton_new(parent_box, sibling, text, active,
				      G_CALLBACK(pref_radiobutton_int_cb), result);
	g_object_set_data(G_OBJECT(button), PREF_RADIO_VALUE_KEY, GINT_TO_POINTER(value));
	if (active) *result = value;

	return button;
}

static GtkWidget *real_pref_spin_new(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
				     gint mnemonic_text,
				     gdouble min, gdouble max, gdouble step, gint digits,
				     gdouble value,
				     GCallback func, gpointer data)
{
	GtkWidget *spin;
	GtkWidget *box;
	GtkWidget *label;

	box = pref_box_new(parent_box, FALSE, GTK_ORIENTATION_HORIZONTAL, PREF_PAD_SPACE);

	spin = gtk_spin_button_new_with_range(min, max, step);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), digits);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);

	if (func)
		{
		g_signal_connect(G_OBJECT(spin), "value_changed", G_CALLBACK(func), data);
		}

	if (text)
		{
		if (mnemonic_text)
			{
			label = pref_label_new_mnemonic(box, text, spin);
			}
		else
			{
			label = pref_label_new(box, text);
			}
		pref_link_sensitivity(label, spin);
		}

	gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 0);
	gtk_widget_show(spin);

	/* perhaps this should only be PREF_PAD_GAP distance from spinbutton ? */
	if (suffix)
		{
		label =  pref_label_new(box, suffix);
		pref_link_sensitivity(label, spin);
		}

	return spin;
}

GtkWidget *pref_spin_new(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
			 gdouble min, gdouble max, gdouble step, gint digits,
			 gdouble value,
			 GCallback func, gpointer data)
{
	return real_pref_spin_new(parent_box, text, suffix, FALSE,
				  min, max, step, digits, value, func, data);
}

GtkWidget *pref_spin_new_mnemonic(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
				  gdouble min, gdouble max, gdouble step, gint digits,
				  gdouble value,
				  GCallback func, gpointer data)
{
	return real_pref_spin_new(parent_box, text, suffix, TRUE,
				  min, max, step, digits, value, func, data);
}

static void pref_spin_int_cb(GtkWidget *widget, gpointer data)
{
	gint *var = data;
	*var = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

GtkWidget *pref_spin_new_int(GtkWidget *parent_box, const gchar *text, const gchar *suffix,
			     gint min, gint max, gint step,
			     gint value, gint *value_var)
{
	*value_var = value;
	return pref_spin_new(parent_box, text, suffix,
			     (gdouble)min, (gdouble)max, (gdouble)step, 0,
			     value,
			     G_CALLBACK(pref_spin_int_cb), value_var);
}

#if 0
void pref_spin_set_blocking(GtkWidget *spin, gdouble value, gpointer block_data)
{
	g_signal_handlers_block_matched(G_OBJECT(spin), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, block_data);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
	g_signal_handlers_unblock_matched(G_OBJECT(spin), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, block_data);
}
#endif

static void pref_link_sensitivity_cb(GtkWidget *watch, GtkStateType prev_state, gpointer data)
{
	GtkWidget *widget = data;

	gtk_widget_set_sensitive(widget, GTK_WIDGET_IS_SENSITIVE(watch));
}

void pref_link_sensitivity(GtkWidget *widget, GtkWidget *watch)
{
	g_signal_connect(G_OBJECT(watch), "state_changed",
			 G_CALLBACK(pref_link_sensitivity_cb), widget);
}

void pref_signal_block_data(GtkWidget *widget, gpointer data)
{
	g_signal_handlers_block_matched(widget, G_SIGNAL_MATCH_DATA,
					0, 0, NULL, NULL, data);
}

void pref_signal_unblock_data(GtkWidget *widget, gpointer data)
{
	g_signal_handlers_unblock_matched(widget, G_SIGNAL_MATCH_DATA,
					  0, 0, NULL, NULL, data);
}

GtkWidget *pref_table_new(GtkWidget *parent_box, gint columns, gint rows,
			  gint homogenious, gint fill)
{
	GtkWidget *table;

	table = gtk_table_new(rows, columns, homogenious);
	gtk_table_set_row_spacings(GTK_TABLE(table), PREF_PAD_GAP);
	gtk_table_set_col_spacings(GTK_TABLE(table), PREF_PAD_SPACE);

	if (parent_box)
		{
		gtk_box_pack_start(GTK_BOX(parent_box), table, fill, fill, 0);
		gtk_widget_show(table);
		}

	return table;
}

GtkWidget *pref_table_box(GtkWidget *table, gint column, gint row,
			  GtkOrientation orientation, const gchar *text)
{
	GtkWidget *box;
	GtkWidget *shell;

	if (text)
		{
		shell = gtk_vbox_new(FALSE, 0);
		box = pref_group_new(shell, TRUE, text, orientation);
		}
	else
		{
		if (orientation == GTK_ORIENTATION_HORIZONTAL)
			{
			box = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
			}
		else
			{
			box = gtk_vbox_new(FALSE, PREF_PAD_GAP);
			}
		shell = box;
		}

	gtk_table_attach(GTK_TABLE(table), shell, column, column + 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL, 0, 0, 0);

	gtk_widget_show(shell);

	return box;
}

GtkWidget *pref_table_label(GtkWidget *table, gint column, gint row,
			    const gchar *text, gfloat alignment)
{
	GtkWidget *label;
	GtkWidget *align;

	align = gtk_alignment_new(alignment, 0.50, 0.0, 0.0);
	gtk_table_attach(GTK_TABLE(table), align, column, column + 1, row, row + 1,
			 GTK_FILL, 0, 0, 0);
	gtk_widget_show(align);
	label = gtk_label_new(text);
	gtk_container_add(GTK_CONTAINER(align), label);
	gtk_widget_show(label);

	return label;
}

GtkWidget *pref_table_button(GtkWidget *table, gint column, gint row,
			     const gchar *stock_id, const gchar *text, gint hide_stock_text,
			     GCallback func, gpointer data)
{
	GtkWidget *button;

	button = pref_button_new(NULL, stock_id, text, hide_stock_text, func, data);
	gtk_table_attach(GTK_TABLE(table), button, column, column + 1, row, row + 1,
			 GTK_FILL, 0, 0, 0);
	gtk_widget_show(button);

	return button;
}

#if 0
static GtkWidget *pref_table_checkbox(GtkWidget *table, gint column, gint row,
				      const gchar *text, gint active,
				      GCallback func, gpointer data)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_label(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), active);
	if (func) g_signal_connect(G_OBJECT(button), "clicked", func, data);

	gtk_table_attach(GTK_TABLE(table), button, column, column + 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show(button);

	return button;
}
#endif

GtkWidget *pref_table_spin(GtkWidget *table, gint column, gint row,
			   const gchar *text, const gchar *suffix,
			   gdouble min, gdouble max, gdouble step, gint digits,
			   gdouble value,
			   GCallback func, gpointer data)
{
	GtkWidget *spin;
	GtkWidget *box;
	GtkWidget *label;

	spin = gtk_spin_button_new_with_range(min, max, step);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), digits);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
	if (func)
		{
		g_signal_connect(G_OBJECT(spin), "value_changed", G_CALLBACK(func), data);
		}

	if (text)
		{
		label = pref_table_label(table, column, row, text, 1.0);
		pref_link_sensitivity(label, spin);
		column++;
		}

	if (suffix)
		{
		box = gtk_hbox_new(FALSE, PREF_PAD_SPACE);
		gtk_box_pack_start(GTK_BOX(box), spin, FALSE, FALSE, 0);
		gtk_widget_show(spin);

		label = pref_label_new(box, suffix);
		pref_link_sensitivity(label, spin);
		}
	else
		{
		box = spin;
		}

	gtk_table_attach(GTK_TABLE(table), box, column, column + 1, row, row + 1,
			 GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show(box);

	return spin;
}

static void pref_toolbar_destroy_cb(GtkWidget *widget, gpointer data)
{
	GtkTooltips *tips = data;

	g_object_unref(G_OBJECT(tips));
}

GtkWidget *pref_toolbar_new(GtkWidget *parent_box, GtkToolbarStyle style)
{
	GtkWidget *tbar;
	GtkTooltips *tips;

	tbar = gtk_toolbar_new();
	gtk_toolbar_set_style(GTK_TOOLBAR(tbar), style);

	if (parent_box)
		{
		gtk_box_pack_start(GTK_BOX(parent_box), tbar, FALSE, FALSE, 0);
		gtk_widget_show(tbar);
		}

	tips = gtk_tooltips_new();

	/* take ownership of tooltips */
#ifdef GTK_OBJECT_FLOATING
	/* GTK+ < 2.10 */
	g_object_ref(G_OBJECT(tips));
	gtk_object_sink(GTK_OBJECT(tips));
#else
	/* GTK+ >= 2.10 */
	g_object_ref_sink(G_OBJECT(tips));
#endif

	g_object_set_data(G_OBJECT(tbar), "tooltips", tips);
	g_signal_connect(G_OBJECT(tbar), "destroy",
			 G_CALLBACK(pref_toolbar_destroy_cb), tips);

	gtk_tooltips_enable(tips);

	return tbar;
}

GtkWidget *pref_toolbar_button(GtkWidget *toolbar, const gchar *stock_id, const gchar *label,
			       const gchar *description,
			       GCallback func, gpointer data)
{
	GtkWidget *item;

	item = GTK_WIDGET(gtk_tool_button_new_from_stock(stock_id));
	if (label) gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), label);

	if (func) g_signal_connect(item, "clicked", func, data);
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	gtk_widget_show(item);

	if (description)
		{
		GtkTooltips *tips;

		tips = g_object_get_data(G_OBJECT(toolbar), "tooltips");
		gtk_tool_item_set_tooltip(GTK_TOOL_ITEM(item), tips, description, NULL);
		}

	return item;
}

GtkWidget *pref_toolbar_spacer(GtkWidget *toolbar)
{
	GtkWidget *item;

	item = GTK_WIDGET(gtk_separator_tool_item_new());
	gtk_container_add(GTK_CONTAINER(toolbar), item);
	gtk_widget_show(item);

	return item;
}


/*
 *-----------------------------------------------------------------------------
 * date selection entry
 *-----------------------------------------------------------------------------
 */

#define DATE_SELECION_KEY "date_selection_data"


typedef struct _DateSelection DateSelection;
struct _DateSelection
{
	GtkWidget *box;

	GtkWidget *spin_d;
	GtkWidget *spin_m;
	GtkWidget *spin_y;

	GtkWidget *button;

	GtkWidget *window;
	GtkWidget *calendar;
};


static void date_selection_popup_hide(DateSelection *ds)
{
	if (!ds->window) return;

	if (GTK_WIDGET_HAS_GRAB(ds->window))
		{
		gtk_grab_remove(ds->window);
		gdk_keyboard_ungrab(GDK_CURRENT_TIME);
		gdk_pointer_ungrab(GDK_CURRENT_TIME);
		}

	gtk_widget_hide(ds->window);

	gtk_widget_destroy(ds->window);
	ds->window = NULL;
	ds->calendar = NULL;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ds->button), FALSE);
}

static gint date_selection_popup_release_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	DateSelection *ds = data;

	date_selection_popup_hide(ds);
	return TRUE;
}

static gint date_selection_popup_press_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	DateSelection *ds = data;
	gint x, y;
	gint w, h;
	gint xr, yr;

	xr = (gint)event->x_root;
	yr = (gint)event->y_root;

	gdk_window_get_origin(ds->window->window, &x, &y);
	gdk_drawable_get_size(ds->window->window, &w, &h);

	if (xr < x || yr < y || xr > x + w || yr > y + h)
		{
		g_signal_connect(G_OBJECT(ds->window), "button_release_event",
				 G_CALLBACK(date_selection_popup_release_cb), ds);
		return TRUE;
		}

	return FALSE;
}

static void date_selection_popup_sync(DateSelection *ds)
{
	guint day, month, year;

	gtk_calendar_get_date(GTK_CALENDAR(ds->calendar), &year, &month, &day);
	date_selection_set(ds->box, day, month + 1, year);
}

static gint date_selection_popup_keypress_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	DateSelection *ds = data;

	switch (event->keyval)
		{
		case GDK_Return:
		case GDK_KP_Enter:
		case GDK_Tab:
		case GDK_ISO_Left_Tab:
			date_selection_popup_sync(ds);
			date_selection_popup_hide(ds);
			break;
		case GDK_Escape:
			date_selection_popup_hide(ds);
			break;
		default:
			break;
		}

	return FALSE;
}

static void date_selection_day_cb(GtkWidget *widget, gpointer data)
{
	DateSelection *ds = data;

	date_selection_popup_sync(ds);
}

static void date_selection_doubleclick_cb(GtkWidget *widget, gpointer data)
{
	DateSelection *ds = data;

	date_selection_popup_hide(ds);
}

static void date_selection_popup(DateSelection *ds)
{
	gint x, y;
	gint wx, wy;
	gint day, month, year;

	if (ds->window) return;

	ds->window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_resizable(GTK_WINDOW(ds->window), FALSE);
	g_signal_connect(G_OBJECT(ds->window), "button_press_event",
			 G_CALLBACK(date_selection_popup_press_cb), ds);
	g_signal_connect(G_OBJECT(ds->window), "key_press_event",
			 G_CALLBACK(date_selection_popup_keypress_cb), ds);

	ds->calendar = gtk_calendar_new();
	gtk_container_add(GTK_CONTAINER(ds->window), ds->calendar);
	gtk_widget_show(ds->calendar);

	date_selection_get(ds->box, &day, &month, &year);
	gtk_calendar_select_month(GTK_CALENDAR(ds->calendar), month - 1, year);
	gtk_calendar_select_day(GTK_CALENDAR(ds->calendar), day);

	g_signal_connect(G_OBJECT(ds->calendar), "day_selected",
			 G_CALLBACK(date_selection_day_cb), ds);
	g_signal_connect(G_OBJECT(ds->calendar), "day_selected_double_click",
			G_CALLBACK(date_selection_doubleclick_cb), ds);

	gtk_widget_realize(ds->window);

	gdk_window_get_origin(ds->button->window, &wx, &wy);

	x = wx + ds->button->allocation.x + ds->button->allocation.width - ds->window->allocation.width;
	y = wy + ds->button->allocation.y + ds->button->allocation.height;

	if (y + ds->window->allocation.height > gdk_screen_height())
		{
		y = wy + ds->button->allocation.y - ds->window->allocation.height;
		}
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	gtk_window_move(GTK_WINDOW(ds->window), x, y);
	gtk_widget_show(ds->window);

	gtk_widget_grab_focus(ds->calendar);
	gdk_pointer_grab(ds->window->window, TRUE,
			 GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_MOTION_MASK,
			 NULL, NULL, GDK_CURRENT_TIME);
	gdk_keyboard_grab(ds->window->window, TRUE, GDK_CURRENT_TIME);
	gtk_grab_add(ds->window);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ds->button), TRUE);
}

static void date_selection_button_cb(GtkWidget *widget, gpointer data)
{
	DateSelection *ds = data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ds->button)) == (!ds->window))
		{
		date_selection_popup(ds);
		}
}

static void button_size_allocate_cb(GtkWidget *button, GtkAllocation *allocation, gpointer data)
{
	GtkWidget *spin = data;

	if (allocation->height > spin->requisition.height)
		{
		GtkAllocation button_allocation;

		button_allocation = button->allocation;
		button_allocation.height = spin->requisition.height;
		button_allocation.y = spin->allocation.y +
			(spin->allocation.height - spin->requisition.height) / 2;
		gtk_widget_size_allocate(button, &button_allocation);
		}
}

static void spin_increase(GtkWidget *spin, gint value)
{
	GtkRequisition req;

	gtk_widget_size_request(spin, &req);
	gtk_widget_set_size_request(spin, req.width + value, -1);
}

static void date_selection_destroy_cb(GtkWidget *widget, gpointer data)
{
	DateSelection *ds = data;

	date_selection_popup_hide(ds);

	g_free(ds);
}

GtkWidget *date_selection_new(void)
{
	DateSelection *ds;
	GtkWidget *arrow;

	ds = g_new0(DateSelection, 1);

	ds->box = gtk_hbox_new(FALSE, 2);
	g_signal_connect(G_OBJECT(ds->box), "destroy",
			 G_CALLBACK(date_selection_destroy_cb), ds);

	/* FIXME: use option menu with text format of month instead of a spin button */
	ds->spin_m = pref_spin_new(ds->box, NULL, NULL, 1, 12, 1, 0, 1, NULL, NULL);
	ds->spin_d = pref_spin_new(ds->box, NULL, NULL, 1, 31, 1, 0, 1, NULL, NULL);
	ds->spin_y = pref_spin_new(ds->box, NULL, NULL, 1900, 9999, 1, 0, 1900, NULL, NULL);
	spin_increase(ds->spin_y, 5);

	ds->button = gtk_toggle_button_new();
	g_signal_connect(G_OBJECT(ds->button), "size_allocate",
			 G_CALLBACK(button_size_allocate_cb), ds->spin_y);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_container_add(GTK_CONTAINER(ds->button), arrow);
	gtk_widget_show(arrow);

	gtk_box_pack_start(GTK_BOX(ds->box), ds->button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(ds->button), "clicked",
			 G_CALLBACK(date_selection_button_cb), ds);
	gtk_widget_show(ds->button);

	g_object_set_data(G_OBJECT(ds->box), DATE_SELECION_KEY, ds);

	return ds->box;
}
                                                                                                                        
void date_selection_set(GtkWidget *widget, gint day, gint month, gint year)
{
	DateSelection *ds;

	ds = g_object_get_data(G_OBJECT(widget), DATE_SELECION_KEY);
	if (!ds) return;

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ds->spin_d), (gdouble)day);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ds->spin_m), (gdouble)month);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ds->spin_y), (gdouble)year);
}


void date_selection_get(GtkWidget *widget, gint *day, gint *month, gint *year)
{
	DateSelection *ds;

	ds = g_object_get_data(G_OBJECT(widget), DATE_SELECION_KEY);
	if (!ds) return;

	if (day) *day = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ds->spin_d));
	if (month) *month = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ds->spin_m));
	if (year) *year = gtk_spin_button_get_value(GTK_SPIN_BUTTON(ds->spin_y));
}
                                                                                                                        
void date_selection_time_set(GtkWidget *widget, time_t t)
{
	struct tm *lt;

	lt = localtime(&t);
	if (!lt) return;

	date_selection_set(widget, lt->tm_mday, lt->tm_mon + 1, lt->tm_year + 1900);
}

time_t date_selection_time_get(GtkWidget *widget)
{
	struct tm lt;
	gint day = 0;
	gint month = 0;
	gint year = 0;

	date_selection_get(widget, &day, &month ,&year);

	lt.tm_sec = 0;
	lt.tm_min = 0;
	lt.tm_hour = 0;
	lt.tm_mday = day;
	lt.tm_mon = month - 1;
	lt.tm_year = year - 1900;
	lt.tm_isdst = 0;

	return mktime(&lt);
}


/*
 *-----------------------------------------------------------------------------
 * Sizer, without using a GtkPaned
 *-----------------------------------------------------------------------------
 */

#define SIZER_DATA_KEY "sizer_data"

typedef struct _SizerData SizerData;
struct _SizerData
{
	GtkWidget *sizer;
	GtkWidget *parent;
	GtkWidget *bounding_widget;
	SizerPositionType position;

	gint hsize_min;
	gint hsize_max;
	gint vsize_min;
	gint vsize_max;

	gint in_drag;
	gint press_x;
	gint press_y;
	gint press_width;
	gint press_height;

	gint handle_prelit;
};


static gint sizer_default_handle_size(void)
{
	gint handle_size = 5;
	GtkWidget *paned;
	GtkStyle *style;

	paned = gtk_hpaned_new();

	style = gtk_rc_get_style(paned);
	gtk_widget_set_style(paned, style);
	gtk_widget_style_get(paned, "handle_size", &handle_size, NULL);

	gtk_widget_destroy(paned);

	return handle_size;
}

static gint sizer_motion_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	SizerData *sd = data;
	gint x, y;
	gint w, h;

	if (!sd->in_drag) return FALSE;

	x = sd->press_x - bevent->x_root;
	y = sd->press_y - bevent->y_root;

	w = sd->press_width;
	h = sd->press_height;

	if (sd->position & SIZER_POS_LEFT)
		{
		w += x;
		}
	else if (sd->position & SIZER_POS_RIGHT)
		{
		w -= x;
		}

	if (sd->position & SIZER_POS_TOP)
		{
		h += y;
		}
	else if (sd->position & SIZER_POS_BOTTOM)
		{
		h -= y;
		}

	if (sd->hsize_min >= 0) w = MAX(w, sd->hsize_min);
	if (sd->vsize_min >= 0) h = MAX(h, sd->vsize_min);

	if (sd->bounding_widget)
		{
		w = CLAMP(w, sd->sizer->allocation.width, sd->bounding_widget->allocation.width);
		h = CLAMP(h, sd->sizer->allocation.height, sd->bounding_widget->allocation.height);
		}
	else
		{
		if (w < sd->sizer->allocation.width) w = sd->sizer->allocation.width;
		if (h < sd->sizer->allocation.height) h = sd->sizer->allocation.height;
		}

	if (sd->hsize_max >= 0) w = MIN(w, sd->hsize_max);
	if (sd->vsize_max >= 0) h = MIN(h, sd->vsize_max);

	if (w == sd->parent->allocation.width) w = -1;
	if (h == sd->parent->allocation.height) h = -1;

	if (w > 0 || h > 0) gtk_widget_set_size_request(sd->parent, w, h);

	return TRUE;
}

static gint sizer_press_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	SizerData *sd = data;

	if (bevent->button != 1) return FALSE;

	sd->in_drag = TRUE;
	sd->press_x = bevent->x_root;
	sd->press_y = bevent->y_root;

	sd->press_width = sd->parent->allocation.width;
	sd->press_height = sd->parent->allocation.height;

	gdk_pointer_grab(sd->sizer->window, FALSE,
			 GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			 NULL, NULL, bevent->time);
	gtk_grab_add(sd->sizer);

	return TRUE;
}

static gint sizer_release_cb(GtkWidget *widget, GdkEventButton *bevent, gpointer data)
{
	SizerData *sd = data;

	if (bevent->button != 1) return FALSE;

	if (gdk_pointer_is_grabbed() && GTK_WIDGET_HAS_GRAB(sd->sizer))
		{
		gtk_grab_remove(sd->sizer);
		gdk_pointer_ungrab(bevent->time);
		}

	sd->in_drag = FALSE;

	return TRUE;
}

static void sizer_set_prelight(SizerData *sd, gint prelit)
{
	sd->handle_prelit = prelit;
	gtk_widget_queue_draw_area(sd->sizer, 0, 0,
				   sd->sizer->allocation.width, sd->sizer->allocation.height);
}

static gint sizer_enter_cb(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	SizerData *sd = data;

	sizer_set_prelight(sd, TRUE);
	return TRUE;
}

static gint sizer_leave_cb(GtkWidget *widget, GdkEventCrossing *event, gpointer data)
{
	SizerData *sd = data;

	sizer_set_prelight(sd, FALSE);
	return TRUE;
}

static gint sizer_expose_cb(GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
	SizerData *sd = data;
	GdkRectangle clip;
	GtkOrientation orientation;
	GtkStateType state;

	gdk_region_get_clipbox(event->region, &clip);

	if (sd->position & SIZER_POS_LEFT || sd->position & SIZER_POS_RIGHT)
		{
		orientation = GTK_ORIENTATION_VERTICAL;
		}
	else
		{
		orientation = GTK_ORIENTATION_HORIZONTAL;
		}

	if (sd->handle_prelit)
		{
		state = GTK_STATE_PRELIGHT;
		}
	else
		{
		state = widget->state;
		}

	gtk_paint_handle(widget->style, widget->window, state,
			 GTK_SHADOW_NONE, &clip, widget, "paned",
			 0, 0,
			 widget->allocation.width, widget->allocation.height,
			 orientation);

	return TRUE;
}

static void sizer_realize_cb(GtkWidget *widget, gpointer data)
{
	SizerData *sd = data;
	GdkCursorType n;

	n = 0;
	if (sd->position & SIZER_POS_TOP || sd->position & SIZER_POS_BOTTOM)
		{
		n = GDK_SB_V_DOUBLE_ARROW;
		}
	if (sd->position & SIZER_POS_LEFT || sd->position & SIZER_POS_RIGHT)
		{
		n = (n != 0) ? GDK_FLEUR : GDK_SB_H_DOUBLE_ARROW;
		}

	if (n != 0 && widget->window)
		{
		GdkCursor *cursor;
		cursor = gdk_cursor_new(n);
		gdk_window_set_cursor(widget->window, cursor);
		gdk_cursor_unref(cursor);
		}
}

static void sizer_destroy_cb(GtkWidget *widget, gpointer data)
{
	SizerData *sd = data;

	g_free(sd);
}

GtkWidget *sizer_new(GtkWidget *parent, GtkWidget *bounding_widget,
		     SizerPositionType position)
{
	SizerData *sd;
	gint handle_size;

	sd = g_new0(SizerData, 1);

	sd->sizer = gtk_event_box_new();
	sd->parent = parent;
	sd->bounding_widget = bounding_widget;
	sd->position = position;
	sd->hsize_min = -1;
	sd->hsize_max = -1;
	sd->vsize_min = -1;
	sd->vsize_max = -1;

	sd->in_drag = FALSE;
	sd->handle_prelit = FALSE;

	g_signal_connect(G_OBJECT(sd->sizer), "destroy",
			 G_CALLBACK(sizer_destroy_cb), sd);

	g_signal_connect(G_OBJECT(sd->sizer), "motion_notify_event",
			 G_CALLBACK(sizer_motion_cb), sd);
	g_signal_connect(G_OBJECT(sd->sizer), "button_press_event",
			 G_CALLBACK(sizer_press_cb), sd);
	g_signal_connect(G_OBJECT(sd->sizer), "button_release_event",
			 G_CALLBACK(sizer_release_cb), sd);

	g_signal_connect(G_OBJECT(sd->sizer), "enter_notify_event",
			 G_CALLBACK(sizer_enter_cb), sd);
	g_signal_connect(G_OBJECT(sd->sizer), "leave_notify_event",
			 G_CALLBACK(sizer_leave_cb), sd);

	gtk_widget_set_events(sd->sizer, GDK_POINTER_MOTION_MASK |
					 GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK |
					 GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);

	g_signal_connect(sd->sizer, "realize",
			 G_CALLBACK(sizer_realize_cb), sd);
	g_signal_connect(sd->sizer, "expose_event",
			 G_CALLBACK(sizer_expose_cb), sd);

	handle_size = sizer_default_handle_size();

	gtk_widget_set_size_request(sd->sizer, handle_size, handle_size);
#if 0
	/* use this if you add a shadow border to the handle */
	gtk_widget_set_size_request(sd->sizer, handle_size + sd->sizer->style->xthickness * 2,
					       handle_size + sd->sizer->style->ythickness * 2);
#endif

	g_object_set_data(G_OBJECT(sd->sizer), SIZER_DATA_KEY,sd);

	return sd->sizer;
}

void sizer_set_limits(GtkWidget *sizer,
		      gint hsize_min, gint hsize_max,
		      gint vsize_min, gint vsize_max)
{
	SizerData *sd;

	sd = g_object_get_data(G_OBJECT(sizer), SIZER_DATA_KEY);
	if (!sd) return;

	sd->hsize_min = hsize_min;
	sd->hsize_max = hsize_max;
	sd->vsize_min = vsize_min;
	sd->vsize_max = vsize_max;
}


/*
 *-----------------------------------------------------------------------------
 * storing data in a history list with key,data pairs
 *-----------------------------------------------------------------------------
 */

#define PREF_LIST_MARKER_INT "[INT]:"
#define PREF_LIST_MARKER_DOUBLE "[DOUBLE]:"
#define PREF_LIST_MARKER_STRING "[STRING]:"

static GList *pref_list_find(const gchar *group, const gchar *token)
{
	GList *work;
	gint l;

	l = strlen(token);

	work = history_list_get_by_key(group);
	while (work)
		{
		const gchar *text = work->data;

		if (strncmp(text, token, l) == 0) return work;

		work = work->next;
		}

	return NULL;
}

static gint pref_list_get(const gchar *group, const gchar *key, const gchar *marker, const gchar **result)
{
	gchar *token;
	GList *work;
	gint ret;

	if (!group || !key || !marker)
		{
		*result = NULL;
		return FALSE;
		}

	token = g_strconcat(key, marker, NULL);

	work = pref_list_find(group, token);
	if (work)
		{
		*result = (const gchar *)work->data + strlen(token);
		if (strlen(*result) == 0) *result = NULL;
		ret = TRUE;
		}
	else
		{
		*result = NULL;
		ret = FALSE;
		}

	g_free(token);

	return ret; 
}

static void pref_list_set(const gchar *group, const gchar *key, const gchar *marker, const gchar *text)
{
	gchar *token;
	gchar *path;
	GList *work;

	if (!group || !key || !marker) return;

	token = g_strconcat(key, marker, NULL);
	path = g_strconcat(token, text, NULL);

	work = pref_list_find(group, token);
	if (work)
		{
		gchar *old_path = work->data;

		if (text)
			{
			work->data = path;
			path = NULL;

			g_free(old_path);
			}
		else
			{
			history_list_item_remove(group, old_path);
			}
		}
	else if (text)
		{
		history_list_add_to_key(group, path, 0);
		}

	g_free(path);
	g_free(token);
}

void pref_list_int_set(const gchar *group, const gchar *key, gint value)
{
	gchar *text;

	text = g_strdup_printf("%d", value);
	pref_list_set(group, key, PREF_LIST_MARKER_INT, text);
	g_free(text);
}

gint pref_list_int_get(const gchar *group, const gchar *key, gint *result)
{
	const gchar *text;

	if (!group || !key)
		{
		*result = 0;
		return FALSE;
		}

	if (pref_list_get(group, key, PREF_LIST_MARKER_INT, &text) && text)
		{
		*result = (gint)strtol(text, NULL, 10);
		return TRUE;
		}

	*result = 0;
	return FALSE;
}

void pref_list_double_set(const gchar *group, const gchar *key, gdouble value)
{
	gchar text[G_ASCII_DTOSTR_BUF_SIZE];

	g_ascii_dtostr(text, sizeof(text), value);
	pref_list_set(group, key, PREF_LIST_MARKER_DOUBLE, text);
}

gint pref_list_double_get(const gchar *group, const gchar *key, gdouble *result)
{
	const gchar *text;

	if (!group || !key)
		{
		*result = 0;
		return FALSE;
		}

	if (pref_list_get(group, key, PREF_LIST_MARKER_DOUBLE, &text) && text)
		{
		*result = g_ascii_strtod(text, NULL);
		return TRUE;
		}

	*result = 0;
	return FALSE;
}

void pref_list_string_set(const gchar *group, const gchar *key, const gchar *value)
{
	pref_list_set(group, key, PREF_LIST_MARKER_STRING, value);
}

gint pref_list_string_get(const gchar *group, const gchar *key, const gchar **result)
{
	return pref_list_get(group, key, PREF_LIST_MARKER_STRING, result);
}


