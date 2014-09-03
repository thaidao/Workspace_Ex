/* cellrenderericon.h, based on:
 *
 * gtkcellrendererpixbuf.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GQV_CELL_RENDERER_ICON_H__
#define __GQV_CELL_RENDERER_ICON_H__

#include <gtk/gtkcellrenderer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GQV_TYPE_CELL_RENDERER_ICON		(gqv_cell_renderer_icon_get_type ())
#define GQV_CELL_RENDERER_ICON(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GQV_TYPE_CELL_RENDERER_ICON, GQvCellRendererIcon))
#define GQV_CELL_RENDERER_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GQV_TYPE_CELL_RENDERER_ICON, GQvCellRendererIconClass))
#define GQV_IS_CELL_RENDERER_ICON(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GQV_TYPE_CELL_RENDERER_ICON))
#define GQV_IS_CELL_RENDERER_ICON_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GQV_TYPE_CELL_RENDERER_ICON))
#define GQV_CELL_RENDERER_ICON_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GQV_TYPE_CELL_RENDERER_ICON, GQvCellRendererIconClass))

typedef struct _GQvCellRendererIcon GQvCellRendererIcon;
typedef struct _GQvCellRendererIconClass GQvCellRendererIconClass;

struct _GQvCellRendererIcon
{
	GtkCellRenderer parent;

	/*< private >*/
	GdkPixbuf *pixbuf;
	gchar *text;
	PangoColor foreground;
	PangoColor background;
	gboolean focused;

	gint fixed_width;
	gint fixed_height;

	gboolean foreground_set;
	gboolean background_set;

	gboolean show_text;
};

struct _GQvCellRendererIconClass
{
	GtkCellRendererClass parent_class;

	/* Padding for future expansion */
	void (*_gtk_reserved1) (void);
	void (*_gtk_reserved2) (void);
	void (*_gtk_reserved3) (void);
	void (*_gtk_reserved4) (void);
};

GType            gqv_cell_renderer_icon_get_type (void);
GtkCellRenderer *gqv_cell_renderer_icon_new      (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GQV_CELL_RENDERER_ICON_H__ */
