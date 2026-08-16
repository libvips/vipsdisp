/* a status bar for the image display window
 */

/*

	Copyright (C) 1991-2003 The National Gallery

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc.,
	51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

 */

/*

	These files are distributed with VIPS - http://www.vips.ecs.soton.ac.uk

*/

#include "nip4.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

struct _Infobar {
	GtkWidget parent_instance;

	/* The imagewindow whose info we display.
	 */
	Imagewindow *win;

	GtkWidget *action_bar;
	GtkWidget *x;
	GtkWidget *y;
	GtkWidget *values;
	GtkWidget *mag;

	GSList *value_widgets;

	// a background pixel value fetch is in progress
	gboolean updating;
};

G_DEFINE_TYPE(Infobar, infobar, GTK_TYPE_WIDGET);

enum {
	PROP_IMAGEWINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

static void
infobar_dispose(GObject *object)
{
	Infobar *infobar = (Infobar *) object;

#ifdef DEBUG
	printf("infobar_dispose:\n");
#endif /*DEBUG*/

	VIPS_FREEF(gtk_widget_unparent, infobar->action_bar);
	VIPS_FREEF(g_slist_free, infobar->value_widgets);

	G_OBJECT_CLASS(infobar_parent_class)->dispose(object);
}

/* For each format, the label width we need, in characters. Complex types are
 * just double bands, so the same width for each band.
 */
static const int infobar_label_width[] = {
	3,	/* uchar */
	4,	/* char */
	5,	/* ushort */
	6,	/* short */
	8,	/* uint */
	9,	/* int */
	10, /* float */
	10, /* complex */
	10, /* double */
	10, /* double complex */
};

/* We have a new image to display -- rebuild widgets.
 */
static void
infobar_new_image(Infobar *infobar)
{
	Tilesource *tilesource;
	if (!(tilesource = imagewindow_get_tilesource(infobar->win)))
		return;

	VipsImage *image;
	if (!(image = tilesource_get_base_image(tilesource)))
		return;

	GSList *p;
	VipsBandFormat format;
	int bands;
	int label_width;
	int max_children;
	int n_children;
	int i;

#ifdef DEBUG
	printf("infobar_new_image:\n");
#endif /*DEBUG*/

	/* Remove all existing children of infobar->values.
	 */
	for (p = infobar->value_widgets; p; p = p->next) {
		GtkWidget *label = GTK_WIDGET(p->data);

		gtk_box_remove(GTK_BOX(infobar->values), label);
	}
	VIPS_FREEF(g_slist_free, infobar->value_widgets);

	switch (image->Coding) {
	case VIPS_CODING_LABQ:
	case VIPS_CODING_RAD:
		format = VIPS_FORMAT_FLOAT;
		bands = 3;
		break;

	case VIPS_CODING_NONE:
	default:
		format = image->BandFmt;
		bands = image->Bands;
		break;
	}

	if (vips_band_format_iscomplex(image->BandFmt))
		bands *= 2;

	label_width = infobar_label_width[format];
	// max of 500 chars ... need to cap it somewhere
	max_children = 500 / label_width;
	n_children = VIPS_MIN(bands, max_children);

	/* Add a new set of labels.
	 */
	for (i = 0; i < n_children; i++) {
		GtkWidget *label;

		label = gtk_label_new("123");
		gtk_label_set_width_chars(GTK_LABEL(label), label_width);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		gtk_box_append(GTK_BOX(infobar->values), label);
		infobar->value_widgets = g_slist_append(infobar->value_widgets, label);
	}
}

static void
infobar_status_value_set_array(Infobar *infobar, double *d)
{
	int i;
	GSList *q;

	for (i = 0, q = infobar->value_widgets; q; q = q->next, i++) {
		GtkWidget *label = GTK_WIDGET(q->data);

		char str[64];
		VipsBuf buf = VIPS_BUF_STATIC(str);

		vips_buf_appendf(&buf, "%g", d[i]);
		gtk_label_set_text(GTK_LABEL(label), vips_buf_all(&buf));
	}
}

/* Asynchronous update of the pixel value ... we need this off the main thread
 * or we get awful hitching for some formats.
 */
typedef struct _PixelUpdate {
	// what we update, where we get the pixel data
	Infobar *infobar;
	VipsImage *image;

	// fetch params
	int image_x;
	int image_y;
	double *vector;
	int n;
	gboolean result;
} PixelUpdate;

static void
infobar_update_free(PixelUpdate *update)
{
	update->infobar->updating = FALSE;

	VIPS_UNREF(update->infobar);
	VIPS_UNREF(update->image);
	VIPS_FREE(update->vector);
	VIPS_FREE(update);
}

// runs back in the main thread again ... update the screen
static gboolean
infobar_update_pixel_idle(void *a)
{
	PixelUpdate *update = (PixelUpdate *) a;

	if (update->result)
		infobar_status_value_set_array(update->infobar, update->vector);

	infobar_update_free(update);

	return FALSE;
}

// runs in a bg thread
static void
infobar_get_pixel(void *a, void *b)
{
	PixelUpdate *update = (PixelUpdate *) a;
	VipsImage *image = update->image;

	/* Clip (x, y) to the image dimensions. It might be a histogram, for
	 * example.
	 */
	update->result = !vips_getpoint(image,
			&update->vector, &update->n,
			VIPS_CLIP(0, update->image_x, image->Xsize - 1),
			VIPS_CLIP(0, update->image_y, image->Ysize - 1),
			"unpack_complex", TRUE,
			NULL);

	g_idle_add(infobar_update_pixel_idle, update);
}

// fetch the mouse position pixel and update the screen in a bg thread
static void
infobar_update_pixel(Infobar *infobar,
	Tilesource *tilesource, double image_x, double image_y)
{
#ifdef DEBUG
	printf("infobar_update_pixel: %g x %g:\n", image_x, image_y);
#endif /*DEBUG*/

	/* We need to fetch pixels from base for histograms, from image
	 * otherwise.
	 */
	VipsImage *base = tilesource_get_base_image(tilesource);
	VipsImage *image = tilesource_get_image(tilesource);
	VipsImage *selected = base->Type == VIPS_INTERPRETATION_HISTOGRAM ?
		base : image;

	if (!infobar->updating &&
		selected) {
		infobar->updating = TRUE;

		PixelUpdate *update = g_new0(PixelUpdate, 1);
		update->infobar = infobar;
		update->image = selected;

		// must stay valid until we are done
		g_object_ref(update->infobar);
		g_object_ref(update->image);

		/* Currently in level0 image coordinates ... we need to scale for the
		 * selected image.
		 */
		int factor = tilesource->image_width / update->image->Xsize;
		update->image_x = image_x / factor;
		update->image_y = image_y / factor;

		if (vips_thread_execute("pixel", infobar_get_pixel, update))
			// if we can't run a bg task, we must free the update
			infobar_update_free(update);
	}
}

static void
infobar_status_update(Infobar *infobar)
{
	char str[64];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	double image_x;
	double image_y;

#ifdef DEBUG
	printf("infobar_status_update:\n");
#endif /*DEBUG*/

	Tilesource *tilesource = imagewindow_get_tilesource(infobar->win);
	imagewindow_get_mouse_position(infobar->win, &image_x, &image_y);

	if (tilesource->image) {
		vips_buf_appendf(&buf, "%d", (int) image_x);
		gtk_label_set_text(GTK_LABEL(infobar->x), vips_buf_all(&buf));
		vips_buf_rewind(&buf);

		vips_buf_appendf(&buf, "%d", (int) image_y);
		gtk_label_set_text(GTK_LABEL(infobar->y), vips_buf_all(&buf));
		vips_buf_rewind(&buf);
	}

	double zoom = imagewindow_get_zoom(infobar->win);
	vips_buf_appendf(&buf, "Magnification %d%%", (int) rint(zoom * 100));
	gtk_label_set_text(GTK_LABEL(infobar->mag), vips_buf_all(&buf));

	// queue bg update of pixel value (this must be off the GUI thread)
	infobar_update_pixel(infobar, tilesource, image_x, image_y);
}

static void
infobar_imagewindow_status_changed(Imagewindow *win, Infobar *infobar)
{
	if (!gtk_action_bar_get_revealed(GTK_ACTION_BAR(infobar->action_bar)) ||
		!imagewindow_get_tilesource(infobar->win))
		return;

#ifdef DEBUG
	printf("infobar_imagewindow_status_changed:\n");
#endif /*DEBUG*/

	infobar_status_update(infobar);
}

static void
infobar_imagewindow_new_image(Imagewindow *win, Infobar *infobar)
{
#ifdef DEBUG
	printf("infobar_imagewindow_new_image:\n");
#endif /*DEBUG*/

	infobar_new_image(infobar);
}

static void
infobar_set_imagewindow(Infobar *infobar, Imagewindow *win)
{
	/* No need to ref ... win holds a ref to us.
	 */
	infobar->win = win;

	g_signal_connect_object(win, "status-changed",
		G_CALLBACK(infobar_imagewindow_status_changed), infobar, 0);
	g_signal_connect_object(win, "new-image",
		G_CALLBACK(infobar_imagewindow_new_image), infobar, 0);
}

static void
infobar_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Infobar *infobar = (Infobar *) object;

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		infobar_set_imagewindow(infobar,
			IMAGEWINDOW(g_value_get_object(value)));
		break;

	case PROP_REVEALED:
		gtk_action_bar_set_revealed(
			GTK_ACTION_BAR(infobar->action_bar),
			g_value_get_boolean(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
infobar_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Infobar *infobar = (Infobar *) object;
	GtkActionBar *action_bar = GTK_ACTION_BAR(infobar->action_bar);

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		g_value_set_object(value, infobar->win);
		break;

	case PROP_REVEALED:
		g_value_set_boolean(value, gtk_action_bar_get_revealed(action_bar));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
infobar_init(Infobar *infobar)
{
#ifdef DEBUG
	printf("infobar_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(infobar));
}

static void
infobar_class_init(InfobarClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("infobar_class_init:\n");
#endif /*DEBUG*/

	BIND_RESOURCE("infobar.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Infobar, action_bar);
	BIND_VARIABLE(Infobar, x);
	BIND_VARIABLE(Infobar, y);
	BIND_VARIABLE(Infobar, values);
	BIND_VARIABLE(Infobar, mag);

	gobject_class->dispose = infobar_dispose;
	gobject_class->set_property = infobar_set_property;
	gobject_class->get_property = infobar_get_property;

	g_object_class_install_property(gobject_class, PROP_IMAGEWINDOW,
		g_param_spec_object("image-window",
			_("Image window"),
			_("The image window we display"),
			IMAGEWINDOW_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_REVEALED,
		g_param_spec_boolean("revealed",
			_("revealed"),
			_("Show the display control bar"),
			FALSE,
			G_PARAM_READWRITE));
}

Infobar *
infobar_new(Imagewindow *win)
{
	Infobar *infobar;

#ifdef DEBUG
	printf("infobar_new:\n");
#endif /*DEBUG*/

	infobar = g_object_new(infobar_get_type(),
		"image-window", win,
		NULL);

	return infobar;
}
