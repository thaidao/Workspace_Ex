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
#include "similar.h"

/*
 * These functions are intended to find images with similar color content. For
 * example when an image was saved at different compression levels or dimensions
 * (scaled down/up) the contents are similar, but these files do not match by file
 * size, dimensions, or checksum.
 *
 * These functions create a 32 x 32 array for each color channel (red, green, blue).
 * The array represents the average color of each corresponding part of the
 * image. (imagine the image cut into 1024 rectangles, or a 32 x 32 grid.
 * Each grid is then processed for the average color value, this is what
 * is stored in the array)
 *
 * To compare two images, generate a ImageSimilarityData for each image, then
 * pass them to the compare function. The return value is the percent match
 * of the two images. (for this, simple comparisons are used, basically the return
 * is an average of the corresponding array differences)
 *
 * for image_sim_compare(), the return is 0.0 to 1.0:
 *  1.0 for exact matches (an image is compared to itself)
 *  0.0 for exact opposite images (compare an all black to an all white image)
 * generally only a match of > 0.85 are significant at all, and >.95 is useful to
 * find images that have been re-saved to other formats, dimensions, or compression.
 */


/*
 * The experimental (alternate) algorithm is only for testing of new techniques to
 * improve the result, and hopes to reduce false positives.
 */

static gint alternate_enabled = FALSE;

void image_sim_alternate_set(gint enable)
{
	alternate_enabled = enable;
}

gint image_sim_alternate_enabled(void)
{
	return alternate_enabled;
}

ImageSimilarityData *image_sim_new(void)
{
	ImageSimilarityData *sd = g_new0(ImageSimilarityData, 1);

	return sd;
}

void image_sim_free(ImageSimilarityData *sd)
{
	g_free(sd);
}

#if 0
static void image_sim_channel_expand(guint8 *pix, gint len)
{
	guint8 l, h;
	gint i;
	gdouble scale;

	/* set the start values */
	l = h = pix[0];

	/* find min/max */
	for (i = 0; i < len; i++)
		{
		if (pix[i] < l) l = pix[i];
		if (pix[i] > h) h = pix[i];
		}

	/* calc scale from range */
	if (l != h)
		{
		scale = 255.0 / (gdouble)(h - l);
		}
	else
		{
		scale = 1.0;
		}

	for (i = 0; i < len; i++)
		{
		pix[i] = (guint8)((gdouble)pix[i] - l * scale);
		}
}
#endif

static int image_sim_channel_eq_sort_cb(const void *a, const void *b)
{
	gint *pa = (void *)a;
	gint *pb = (void *)b;
	if (pa[1] < pb[1]) return -1;
	if (pa[1] > pb[1]) return 1;
	return 0;
}

static void image_sim_channel_equal(guint8 *pix, gint len)
{
	gint *buf;
	gint i;
	gint p;

	buf = g_new0(gint, len * 2);

	p = 0;
	for (i = 0; i < len; i++)
		{
		buf[p] = i;
		p++;
		buf[p] = (gint)pix[i];
		p++;
		}

	qsort (buf, len, sizeof(gint) * 2, image_sim_channel_eq_sort_cb);

	p = 0;
	for (i = 0; i < len; i++)
		{
		gint n;

		n = buf[p];
		p+= 2;
		pix[n] = (guint8)(255 * i / len);
		}

	g_free(buf);
}

static void image_sim_channel_norm(guint8 *pix, gint len)
{
	guint8 l, h;
	gint i;
	gdouble scale;

	l = h = pix[0];

	for (i = 0; i < len; i++)
		{
		if (pix[i] < l) l = pix[i];
		if (pix[i] > h) h = pix[i];
		}

	scale = (h-l !=0) ? 255.0 / (gdouble)(h - l) : 1.0;

	for (i = 0; i < len; i++)
		{
		pix[i] = (guint8)((gdouble)(pix[i] - l) * scale);
		}
}

/*
 * define these to enable various components of the experimental compare functions
 *
 * Convert the thumbprint to greyscale (ignore all color information when comparing)
 *  #define ALTERNATE_USES_GREYSCALE 1
 *
 * Take into account the difference in change from one pixel to the next
 *  #define ALTERNATE_INCLUDE_COMPARE_CHANGE 1
 */

void image_sim_alternate_processing(ImageSimilarityData *sd)
{
#ifdef ALTERNATE_USES_GREYSCALE
	gint i;
#endif

	if (!alternate_enabled) return;
	
	image_sim_channel_norm(sd->avg_r, sizeof(sd->avg_r));
	image_sim_channel_norm(sd->avg_g, sizeof(sd->avg_g));
	image_sim_channel_norm(sd->avg_b, sizeof(sd->avg_b));

	image_sim_channel_equal(sd->avg_r, sizeof(sd->avg_r));
	image_sim_channel_equal(sd->avg_g, sizeof(sd->avg_g));
	image_sim_channel_equal(sd->avg_b, sizeof(sd->avg_b));

#ifdef ALTERNATE_USES_GREYSCALE
	for (i = 0; i < sizeof(sd->avg_r); i++)
		{
		guint8 n;
		
		n = (guint8)((gint)(sd->avg_r[i] + sd->avg_g[i] + sd->avg_b[i]) / 3);
		sd->avg_r[i] = sd->avg_g[i] = sd->avg_b[i] = n;
		}
#endif
}

void image_sim_fill_data(ImageSimilarityData *sd, GdkPixbuf *pixbuf)
{
	gint w, h;
	gint rs;
	guchar *pix;
	gint has_alpha;
	gint p_step;

	guchar *p;
	gint i;
	gint j;
	gint x_inc, y_inc;
	gint xs, ys;

	gint x_small = FALSE;	/* if less than 32 w or h, set TRUE */
	gint y_small = FALSE;

	if (!sd || !pixbuf) return;

	w = gdk_pixbuf_get_width(pixbuf);
	h = gdk_pixbuf_get_height(pixbuf);
	rs = gdk_pixbuf_get_rowstride(pixbuf);
	pix = gdk_pixbuf_get_pixels(pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);

	p_step = has_alpha ? 4 : 3;
	x_inc = w / 32;
	y_inc = h / 32;

	if (x_inc < 1)
		{
		x_inc = 1;
		x_small = TRUE;
		}
	if (y_inc < 1)
		{
		y_inc = 1;
		y_small = TRUE;
		}

	j = 0;

	for (ys = 0; ys < 32; ys++)
		{
		if (y_small) j = (gdouble)h / 32 * ys;

		i = 0;

		for (xs = 0; xs < 32; xs++)
			{
			gint x, y;
			gint r, g, b;
			gint t;

			if (x_small) i = (gdouble)w / 32 * xs;

			r = g = b = 0;

			for (y = j; y < j + y_inc; y++)
				{
				p = pix + (y * rs) + (i * p_step);
				for (x = i; x < i + x_inc; x++)
					{
					r += *p; p++;
					g += *p; p++;
					b += *p; p++;
					if (has_alpha) p++;
					}
				}

			t = x_inc * y_inc;
			r /= t;
			g /= t;
			b /= t;

			t = ys * 32 + xs;
			sd->avg_r[t] = r;
			sd->avg_g[t] = g;
			sd->avg_b[t] = b;

			i += x_inc;
			}

		j += y_inc;
		}

	sd->filled = TRUE;
}

ImageSimilarityData *image_sim_new_from_pixbuf(GdkPixbuf *pixbuf)
{
	ImageSimilarityData *sd;

	sd = image_sim_new();
	image_sim_fill_data(sd, pixbuf);

	return sd;
}

#ifdef ALTERNATE_INCLUDE_COMPARE_CHANGE
static gdouble alternate_image_sim_compare_fast(ImageSimilarityData *a, ImageSimilarityData *b, gdouble min)
{
	gint sim;
	gint i;
	gint j;
	gint ld;

	if (!a || !b || !a->filled || !b->filled) return 0.0;


	min = 1.0 - min;
	sim = 0.0;
	ld = 0;

	for (j = 0; j < 1024; j+= 32)
		{
		for (i = j; i < j + 32; i++)
			{
			gint cr, cg, cb;
			gint cd;

			cr = abs(a->avg_r[i] - b->avg_r[i]);
			cg = abs(a->avg_g[i] - b->avg_g[i]);
			cb = abs(a->avg_b[i] - b->avg_b[i]);

			cd = cr + cg + cb;
			sim += cd + abs(cd - ld);
			ld = cd / 3;
			}
		/* check for abort, if so return 0.0 */
		if ((gdouble)sim / (255.0 * 1024.0 * 4.0) > min) return 0.0;
		}

	return (1.0 - ((gdouble)sim / (255.0 * 1024.0 * 4.0)) );
}
#endif

gdouble image_sim_compare(ImageSimilarityData *a, ImageSimilarityData *b)
{
	gint sim;
	gint i;

	if (!a || !b || !a->filled || !b->filled) return 0.0;

	sim = 0.0;

	for (i = 0; i < 1024; i++)
		{
		sim += abs(a->avg_r[i] - b->avg_r[i]);
		sim += abs(a->avg_g[i] - b->avg_g[i]);
		sim += abs(a->avg_b[i] - b->avg_b[i]);
		}

	return 1.0 - ((gdouble)sim / (255.0 * 1024.0 * 3.0));
}

/* this uses a cutoff point so that it can abort early when it gets to
 * a point that can simply no longer make the cut-off point.
 */
gdouble image_sim_compare_fast(ImageSimilarityData *a, ImageSimilarityData *b, gdouble min)
{
	gint sim;
	gint i;
	gint j;

#ifdef ALTERNATE_INCLUDE_COMPARE_CHANGE
	if (alternate_enabled) return alternate_image_sim_compare_fast(a, b, min);
#endif

	if (!a || !b || !a->filled || !b->filled) return 0.0;

	min = 1.0 - min;
	sim = 0.0;

	for (j = 0; j < 1024; j+= 32)
		{
		for (i = j; i < j + 32; i++)
			{
			sim += abs(a->avg_r[i] - b->avg_r[i]);
			sim += abs(a->avg_g[i] - b->avg_g[i]);
			sim += abs(a->avg_b[i] - b->avg_b[i]);
			}
		/* check for abort, if so return 0.0 */
		if ((gdouble)sim / (255.0 * 1024.0 * 3.0) > min) return 0.0;
		}

	return (1.0 - ((gdouble)sim / (255.0 * 1024.0 * 3.0)) );
}
