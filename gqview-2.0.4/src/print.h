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


#ifndef PRINT_H
#define PRINT_H


/* do not free selection or list, the print window takes control of them */
void print_window_new(const gchar *path, GList *selection, GList *list, GtkWidget *parent);


#endif

