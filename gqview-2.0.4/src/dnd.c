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
#include "dnd.h"

#include "collect.h"
#include "image.h"
#include "ui_bookmark.h"
#include "ui_fileops.h"


GtkTargetEntry dnd_file_drag_types[] = {
	{ "text/uri-list", 0, TARGET_URI_LIST },
	{ "text/plain", 0, TARGET_TEXT_PLAIN }
};
gint dnd_file_drag_types_count = 2;

GtkTargetEntry dnd_file_drop_types[] = {
	{ "application/x-gqview-collection-member", 0, TARGET_APP_COLLECTION_MEMBER },
	{ "text/uri-list", 0, TARGET_URI_LIST }
};
gint dnd_file_drop_types_count = 2;


#define DND_ICON_SIZE 48


static void pixbuf_draw_border(GdkPixbuf *pixbuf, gint w, gint h)
{
	gint alpha;
	gint rs;
	guchar *pix;
	guchar *p;
	gint i;

	alpha = gdk_pixbuf_get_has_alpha(pixbuf);
	rs = gdk_pixbuf_get_rowstride(pixbuf);
	pix = gdk_pixbuf_get_pixels(pixbuf);

	p = pix;
	for (i = 0; i < w; i++)
		{
		*p = 0; p++; *p = 0; p++; *p = 0; p++;
		if (alpha) { *p= 255; p++; }
		}
	for (i = 1; i < h - 1; i++)
		{
		p = pix + rs * i;
		*p = 0; p++; *p = 0; p++; *p = 0; p++;
		if (alpha) *p= 255;

		p = pix + rs * i + (w - 1) * ((alpha == TRUE) ? 4 : 3);
		*p = 0; p++; *p = 0; p++; *p = 0; p++;
		if (alpha) *p= 255;
		}
	p = pix + rs * (h - 1);
	for (i = 0; i < w; i++)
		{
		*p = 0; p++; *p = 0; p++; *p = 0; p++;
		if (alpha) { *p= 255; p++; }
		}
}

static void pixbuf_draw_rect(GdkPixbuf *pixbuf, gint x, gint y, gint w, gint h, guint8 val)
{
	gint alpha;
	gint rs;
	guchar *pix;
	guchar *p;
	gint i, j;

	alpha = gdk_pixbuf_get_has_alpha(pixbuf);
	rs = gdk_pixbuf_get_rowstride(pixbuf);
	pix = gdk_pixbuf_get_pixels(pixbuf);

	for (j = 0; j < h; j++)
		{
		p = pix + (rs * (y + j)) + (x * ((alpha) ? 4 : 3));
		for (i = 0; i < w; i++)
			{
			*p = (*p * (256-val)) >> 8; p++;
			*p = (*p * (256-val)) >> 8; p++;
			*p = (*p * (256-val)) >> 8; p++;
			if (alpha) { *p = 255; p++; }
			}
		}
}

void dnd_set_drag_icon(GtkWidget *widget, GdkDragContext *context, GdkPixbuf *pixbuf, gint items)
{
		GdkPixmap *pixmap;
		GdkBitmap *mask;
		GdkPixbuf *dest;
		gint w, h;
		gint sw, sh;
		PangoLayout *layout = NULL;
		gint x, y;

		x = y = 0;

		sw = gdk_pixbuf_get_width(pixbuf);
		sh = gdk_pixbuf_get_height(pixbuf);

		if (sw <= DND_ICON_SIZE && sh <= DND_ICON_SIZE)
			{
			w = sw;
			h = sh;
			}
		else if (sw < sh)
			{
			w = sw * DND_ICON_SIZE / sh;
			h = DND_ICON_SIZE;
			}
		else
			{
			w = DND_ICON_SIZE;
			h = sh * DND_ICON_SIZE / sw;
			}

		dest = gdk_pixbuf_scale_simple(pixbuf, w, h, GDK_INTERP_BILINEAR);
		pixbuf_draw_border(dest, w, h);

		if (items > 1)
			{
			gchar *buf;
			gint lw,lh;

			layout = gtk_widget_create_pango_layout(widget, NULL);
			buf = g_strdup_printf("<small> %d </small>", items);
			pango_layout_set_markup(layout, buf, -1);
			g_free(buf);

			pango_layout_get_pixel_size(layout, &lw, &lh);

			x = MAX(0, w - lw);
			y = MAX(0, h - lh);
			lw = CLAMP(lw, 0, w - x - 1);
			lh = CLAMP(lh, 0, h - y - 1);

			pixbuf_draw_rect(dest, x, y, lw, lh, 128);
			}

		gdk_pixbuf_render_pixmap_and_mask(dest, &pixmap, &mask, 128);
		g_object_unref(dest);

		if (layout)
			{
			gdk_draw_layout(pixmap, widget->style->black_gc, x+1, y+1, layout);
			gdk_draw_layout(pixmap, widget->style->white_gc, x, y, layout);

			g_object_unref(G_OBJECT(layout));
			}

		gtk_drag_set_icon_pixmap(context, gtk_widget_get_colormap(widget), pixmap, mask, -8, -6);

		g_object_unref(pixmap);
		if (mask) g_object_unref(mask);
}
