/* display an image in a drawingarea
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

// the focus colour we paint
// FIXME ... we should somehow get this from the theme, I'm not sure how
#define BORDER ((GdkRGBA){ 0.4, 0.4, 0.6, 1 })

struct _Imagedisplay {
	GtkDrawingArea parent_instance;

	/* The tilesource we display.
	 */
	Tilesource *tilesource;

	/* We implement a scrollable interface.
	 */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;
	guint hscroll_policy;
	guint vscroll_policy;

	/* Generate and cache tiles with this.
	 */
	Tilecache *tilecache;

	/* image_rect is the bounds of image space .. 0,0 to image->Xsize,
	 * image->Ysize
	 */
	VipsRect image_rect;

	/* The rect of the widget.
	 */
	VipsRect widget_rect;

	/* The sub-area of widget_rect that we paint. Very zoomed out images
	 * are centred in the widget.
	 */
	VipsRect paint_rect;

	/* How we transform the image_rect to widget space.
	 *
	 * scale is how much we zoom/reduce the image by.
	 * x, y is the position of the top-left of the widget in the scaled
	 * image.
	 */
	double scale;
	double x, y;

	/* The size of physical display pixels in gtk coordinates, eg. for a 200%
	 * desktop this would be 0.5.
	 */
	double pixel_size;

	/* Draw the screen in debug mode.
	 */
	gboolean debug;

	/* _layout will pick a scale to fit the image to the window.
	 */
	gboolean bestfit;

	/* This is set to enable screen paints, for example if a widget is
	 * mapped or has been scrolled into view.
	 */
	gboolean enable;
};

/* imagedisplay is actually a drawing area the size of the widget on screen: we
 * do all scrolling ourselves.
 */
G_DEFINE_TYPE_WITH_CODE(Imagedisplay, imagedisplay, GTK_TYPE_DRAWING_AREA,
	G_IMPLEMENT_INTERFACE(GTK_TYPE_SCROLLABLE, NULL));

enum {
	/* Set the tilesource we display.
	 */
	PROP_TILESOURCE = 1,

	/* The props we implement for the scrollable interface.
	 */
	PROP_HADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VADJUSTMENT,
	PROP_VSCROLL_POLICY,

	/* We need bestfit off for eg. duplicate.
	 */
	PROP_BESTFIT,

	/* Control transform with this.
	 */
	PROP_BACKGROUND,
	PROP_ZOOM,
	PROP_X,
	PROP_Y,

	/* Read out display density with this.
	 */
	PROP_PIXEL_SIZE,

	/* Draw snapshot in debug mode.
	 */
	PROP_DEBUG,

};

enum {
	SIG_CHANGED,  /* x/y/scale has changed */
	SIG_SNAPSHOT, /* draw overlays */
	SIG_LAST
};

static guint imagedisplay_signals[SIG_LAST] = { 0 };

static void
imagedisplay_dispose(GObject *object)
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

#ifdef DEBUG
	printf("imagedisplay_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(imagedisplay->tilecache);
	VIPS_UNREF(imagedisplay->tilesource);

	G_OBJECT_CLASS(imagedisplay_parent_class)->dispose(object);
}

static void
imagedisplay_changed(Imagedisplay *imagedisplay)
{
	g_signal_emit(imagedisplay, imagedisplay_signals[SIG_CHANGED], 0);
}

static void
imagedisplay_overlay_snapshot(Imagedisplay *imagedisplay,
	GtkSnapshot *snapshot)
{
	g_signal_emit(imagedisplay,
		imagedisplay_signals[SIG_SNAPSHOT], 0, snapshot);
}

static void
imagedisplay_set_transform(Imagedisplay *imagedisplay,
	double scale, double x, double y)
{
	/* Sanity limits.
	 */
	if (scale > 100000 ||
		scale < (1.0 / 100000))
		return;
	if (x < -1000 ||
		x > 2 * VIPS_MAX_COORD ||
		y < -1000 ||
		y > 2 * VIPS_MAX_COORD)
		return;

#ifdef DEBUG
	printf("imagedisplay_set_transform: "
		   "x = %g, y = %g, scale = %g\n",
		x, y, scale);
#endif /*DEBUG*/

	imagedisplay->scale = scale;
	imagedisplay->x = x;
	imagedisplay->y = y;

	imagedisplay_changed(imagedisplay);
}

static void
imagedisplay_adjustment_changed(GtkAdjustment *adjustment,
	Imagedisplay *imagedisplay)
{
	if (gtk_widget_get_realized(GTK_WIDGET(imagedisplay))) {
		double left = gtk_adjustment_get_value(imagedisplay->hadj);
		double top = gtk_adjustment_get_value(imagedisplay->vadj);

#ifdef DEBUG
		printf("imagedisplay_adjustment_changed: %g x %g\n", left, top);
#endif /*DEBUG*/

		imagedisplay_set_transform(imagedisplay,
			imagedisplay->scale, left, top);
		gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
	}
}

static gboolean
imagedisplay_set_adjustment(Imagedisplay *imagedisplay,
	GtkAdjustment **adjustment_slot, GtkAdjustment *new_adjustment)
{
#ifdef DEBUG
	printf("imagedisplay_set_adjustment:\n");
#endif /*DEBUG*/

	if (new_adjustment &&
		*adjustment_slot == new_adjustment)
		return FALSE;

	if (*adjustment_slot) {
		g_signal_handlers_disconnect_by_func(*adjustment_slot,
			imagedisplay_adjustment_changed, imagedisplay);
		VIPS_UNREF(*adjustment_slot);
	}

	if (!new_adjustment)
		new_adjustment = gtk_adjustment_new(0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	g_signal_connect(new_adjustment, "value-changed",
		G_CALLBACK(imagedisplay_adjustment_changed), imagedisplay);
	*adjustment_slot = g_object_ref_sink(new_adjustment);

	return TRUE;
}

static void
imagedisplay_set_adjustment_values(Imagedisplay *imagedisplay,
	GtkAdjustment *adjustment, int axis_size, int window_size)
{
	double old_value;
	double new_value;
	double new_upper;
	double page_size;

#ifdef DEBUG
	printf("imagedisplay_set_adjustment_values: axis_size = %d, "
		   "window_size = %d\n",
		axis_size, window_size);
#endif /*DEBUG*/

	old_value = gtk_adjustment_get_value(adjustment);
	page_size = window_size;
	new_upper = VIPS_MAX(axis_size, page_size);

	g_object_set(adjustment,
		"lower", 0.0,
		"upper", new_upper,
		"page-size", page_size,
		"step-increment", 10.0,
		"page-increment", page_size * 0.9,
		NULL);

	new_value = VIPS_CLIP(0, old_value, new_upper - page_size);
	if (new_value != old_value)
		gtk_adjustment_set_value(adjustment, new_value);
}

static void
imagedisplay_set_hadjustment_values(Imagedisplay *imagedisplay)
{
	imagedisplay_set_adjustment_values(imagedisplay,
		imagedisplay->hadj,
		imagedisplay->image_rect.width * imagedisplay->scale,
		imagedisplay->paint_rect.width);
}

static void
imagedisplay_set_vadjustment_values(Imagedisplay *imagedisplay)
{
	imagedisplay_set_adjustment_values(imagedisplay,
		imagedisplay->vadj,
		imagedisplay->image_rect.height * imagedisplay->scale,
		imagedisplay->paint_rect.height);
}

static void
imagedisplay_layout(Imagedisplay *imagedisplay)
{
#ifdef DEBUG
	printf("imagedisplay_layout:\n");
#endif /*DEBUG*/

	imagedisplay->widget_rect.width =
		gtk_widget_get_width(GTK_WIDGET(imagedisplay));
	imagedisplay->widget_rect.height =
		gtk_widget_get_height(GTK_WIDGET(imagedisplay));

	/* width and height will be 0 if _layout runs too early to be useful.
	 */
	if (!imagedisplay->widget_rect.width ||
		!imagedisplay->widget_rect.height)
		return;

	if (imagedisplay->bestfit) {
		double hscale = (double) imagedisplay->widget_rect.width /
			imagedisplay->image_rect.width;
		double vscale = (double) imagedisplay->widget_rect.height /
			imagedisplay->image_rect.height;

		imagedisplay_set_transform(imagedisplay,
			VIPS_MIN(hscale, vscale),
			imagedisplay->x,
			imagedisplay->y);

#ifdef DEBUG
		printf("imagedisplay_layout: bestfit sets scale = %g\n",
			imagedisplay->scale);
#endif /*DEBUG*/
	}

	imagedisplay->paint_rect.width = VIPS_MIN(
		imagedisplay->widget_rect.width,
		imagedisplay->image_rect.width * imagedisplay->scale);
	imagedisplay->paint_rect.height = VIPS_MIN(
		imagedisplay->widget_rect.height,
		imagedisplay->image_rect.height * imagedisplay->scale);

	/* If we've zoomed right out, centre the image in the window.
	 */
	imagedisplay->paint_rect.left = VIPS_MAX(0,
		(imagedisplay->widget_rect.width -
			imagedisplay->paint_rect.width) /
			2);
	imagedisplay->paint_rect.top = VIPS_MAX(0,
		(imagedisplay->widget_rect.height -
			imagedisplay->paint_rect.height) /
			2);

	imagedisplay_set_hadjustment_values(imagedisplay);
	imagedisplay_set_vadjustment_values(imagedisplay);
}

/* Large change, we need to relayout.
 */
static void
imagedisplay_tilecache_changed(Tilecache *tilecache,
	Imagedisplay *imagedisplay)
{
#ifdef DEBUG
	printf("imagedisplay_tilecache_changed: imagedisplay = %p\n",
		imagedisplay);
#endif /*DEBUG*/

	imagedisplay->image_rect.width = tilecache->tilesource->image_width;
	imagedisplay->image_rect.height = tilecache->tilesource->image_height;
	imagedisplay_layout(imagedisplay);

	gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

/* Tiles have changed, but not image geometry. Perhaps falsecolour.
 */
static void
imagedisplay_tilecache_tiles_changed(Tilecache *tilecache,
	Imagedisplay *imagedisplay)
{
#ifdef DEBUG
	printf("imagedisplay_tilecache_tiles_changed:\n");
#endif /*DEBUG*/

	gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

static void
imagedisplay_tilecache_area_changed(Tilecache *tilecache,
       VipsRect *dirty, int z, Imagedisplay *imagedisplay)
{
#ifdef DEBUG_VERBOSE
       printf("imagedisplay_tilecache_area_changed: "
                  "at %d x %d, size %d x %d, z = %d\n",
               dirty->left, dirty->top,
               dirty->width, dirty->height,
               z);
#endif /*DEBUG_VERBOSE*/

       /* Sadly, gtk4 only has this and we can't redraw areas. Perhaps we
        * could just regenerate part of the snapshot?
        */
       gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

static void
imagedisplay_set_tilesource(Imagedisplay *imagedisplay, Tilesource *tilesource)
{
	VIPS_UNREF(imagedisplay->tilesource);

	if (tilesource) {
		imagedisplay->tilesource = tilesource;
		g_object_ref(imagedisplay->tilesource);
	}

	if (imagedisplay->tilecache)
		g_object_set(imagedisplay->tilecache,
			"tilesource", tilesource,
			NULL);

	/* Do initial paint.
	 */
	gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

#ifdef DEBUG
static const char *
imagedisplay_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILESOURCE:
		return "TILESOURCE";
		break;

	case PROP_HADJUSTMENT:
		return "HADJUSTMENT";
		break;

	case PROP_HSCROLL_POLICY:
		return "HSCROLL_POLICY";
		break;

	case PROP_VADJUSTMENT:
		return "VADJUSTMENT";
		break;

	case PROP_VSCROLL_POLICY:
		return "VSCROLL_POLICY";
		break;

	case PROP_BESTFIT:
		return "BESTFIT";
		break;

	case PROP_BACKGROUND:
		return "BACKGROUND";
		break;

	case PROP_ZOOM:
		return "ZOOM";
		break;

	case PROP_X:
		return "X";
		break;

	case PROP_Y:
		return "Y";
		break;

	case PROP_PIXEL_SIZE:
		return "PIXEL_SIZE";
		break;

	case PROP_DEBUG:
		return "DEBUG";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG*/

static void
imagedisplay_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	double d;

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("imagedisplay_set_property: %s = %s\n",
			imagedisplay_property_name(prop_id), str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_HADJUSTMENT:
		if (imagedisplay_set_adjustment(imagedisplay,
				&imagedisplay->hadj,
				g_value_get_object(value))) {
			imagedisplay_set_hadjustment_values(imagedisplay);
			g_object_notify(G_OBJECT(imagedisplay),
				"hadjustment");
		}
		break;

	case PROP_VADJUSTMENT:
		if (imagedisplay_set_adjustment(imagedisplay,
				&imagedisplay->vadj,
				g_value_get_object(value))) {
			imagedisplay_set_vadjustment_values(imagedisplay);
			g_object_notify(G_OBJECT(imagedisplay),
				"vadjustment");
		}
		break;

	case PROP_HSCROLL_POLICY:
		if (imagedisplay->hscroll_policy !=
			g_value_get_enum(value)) {
			imagedisplay->hscroll_policy =
				g_value_get_enum(value);
			gtk_widget_queue_resize(GTK_WIDGET(imagedisplay));
			g_object_notify_by_pspec(object, pspec);
		}
		break;

	case PROP_VSCROLL_POLICY:
		if (imagedisplay->vscroll_policy !=
			g_value_get_enum(value)) {
			imagedisplay->vscroll_policy =
				g_value_get_enum(value);
			gtk_widget_queue_resize(GTK_WIDGET(imagedisplay));
			g_object_notify_by_pspec(object, pspec);
		}
		break;

	case PROP_TILESOURCE:
		imagedisplay_set_tilesource(imagedisplay, g_value_get_object(value));
		break;

	case PROP_BESTFIT:
		imagedisplay->bestfit = g_value_get_boolean(value);
		break;

	case PROP_BACKGROUND:
		if (imagedisplay->tilecache)
			g_object_set(imagedisplay->tilecache,
				"background", g_value_get_int(value),
				NULL);
		break;

	case PROP_ZOOM:
		g_object_set(imagedisplay, "bestfit", FALSE, NULL);
		imagedisplay_set_transform(imagedisplay,
			g_value_get_double(value),
			imagedisplay->x,
			imagedisplay->y);
		imagedisplay_layout(imagedisplay);
		gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
		break;

	case PROP_X:
		g_object_set(imagedisplay, "bestfit", FALSE, NULL);
		imagedisplay_set_transform(imagedisplay,
			imagedisplay->scale,
			g_value_get_double(value),
			imagedisplay->y);
		gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
		break;

	case PROP_Y:
		g_object_set(imagedisplay, "bestfit", FALSE, NULL);
		imagedisplay_set_transform(imagedisplay,
			imagedisplay->scale,
			imagedisplay->x,
			g_value_get_double(value));
		gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
		break;

	case PROP_PIXEL_SIZE:
		d = g_value_get_double(value);
		if (imagedisplay->pixel_size != d) {
			imagedisplay->pixel_size = d;
			imagedisplay_layout(imagedisplay);
			gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
		}
		break;

	case PROP_DEBUG:
		imagedisplay->debug = g_value_get_boolean(value);
		gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
imagedisplay_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Imagedisplay *imagedisplay = (Imagedisplay *) object;

	switch (prop_id) {
	case PROP_HADJUSTMENT:
		g_value_set_object(value, imagedisplay->hadj);
		break;

	case PROP_VADJUSTMENT:
		g_value_set_object(value, imagedisplay->vadj);
		break;

	case PROP_HSCROLL_POLICY:
		g_value_set_enum(value, imagedisplay->hscroll_policy);
		break;

	case PROP_VSCROLL_POLICY:
		g_value_set_enum(value, imagedisplay->vscroll_policy);
		break;

	case PROP_TILESOURCE:
		g_value_set_object(value, imagedisplay->tilesource);
		break;

	case PROP_BESTFIT:
		g_value_set_boolean(value, imagedisplay->bestfit);
		break;

	case PROP_BACKGROUND:
		if (imagedisplay->tilecache)
			g_object_get_property(G_OBJECT(imagedisplay->tilecache),
				"background", value);
		break;

	case PROP_ZOOM:
		g_value_set_double(value, imagedisplay->scale);
		break;

	case PROP_X:
		g_value_set_double(value, imagedisplay->x);
		break;

	case PROP_Y:
		g_value_set_double(value, imagedisplay->y);
		break;

	case PROP_PIXEL_SIZE:
		g_value_set_double(value, imagedisplay->pixel_size);
		break;

	case PROP_DEBUG:
		g_value_set_boolean(value, imagedisplay->debug);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
imagedisplay_snapshot(GtkWidget *widget, GtkSnapshot *snapshot)
{
	Imagedisplay *imagedisplay = IMAGEDISPLAY(widget);

#ifdef DEBUG
	printf("imagedisplay_snapshot:\n");
#endif /*DEBUG*/

	GTK_WIDGET_CLASS(imagedisplay_parent_class)->snapshot(widget, snapshot);

	/* Clip to the widget area, or we may paint over the display control
	 * bar.
	 */
	gtk_snapshot_push_clip(snapshot,
		&GRAPHENE_RECT_INIT(0, 0,
			gtk_widget_get_width(widget),
			gtk_widget_get_height(widget)));

	gtk_snapshot_save(snapshot);

	/* This can change on each repaint as windows are dragged.
	*/
	GtkNative *native = gtk_widget_get_native(widget);
	GdkSurface *surface = gtk_native_get_surface(native);
	double pixel_size = 1.0 / gdk_surface_get_scale(surface);
	g_object_set(imagedisplay, "pixel-size", pixel_size, NULL);

#ifdef HAVE_GTK_SNAPSHOT_SET_SNAP
	gtk_snapshot_set_snap(snapshot, GSK_RECT_SNAP_ROUND);
#endif /*HAVE_GTK_SNAPSHOT_SET_SNAP*/

	gtk_snapshot_scale(snapshot, pixel_size, pixel_size);

	graphene_rect_t paint;
	paint.origin.x = imagedisplay->paint_rect.left / pixel_size;
	paint.origin.y = imagedisplay->paint_rect.top / pixel_size;
	paint.size.width = imagedisplay->paint_rect.width / pixel_size;
	paint.size.height = imagedisplay->paint_rect.height / pixel_size;

	if (imagedisplay->tilecache &&
		imagedisplay->tilecache->n_levels > 0)
		tilecache_snapshot(imagedisplay->tilecache, snapshot,
			imagedisplay->scale / pixel_size,
			imagedisplay->x / pixel_size,
			imagedisplay->y / pixel_size,
			&paint, imagedisplay->debug);

	// back to a non-scaled snapshot
	gtk_snapshot_restore(snapshot);

	// draw any overlays
	imagedisplay_overlay_snapshot(imagedisplay, snapshot);

	// end of clip
	gtk_snapshot_pop(snapshot);
}

static void
imagedisplay_resize(GtkWidget *widget, int width, int height)
{
	Imagedisplay *imagedisplay = (Imagedisplay *) widget;

#ifdef DEBUG
	printf("imagedisplay_resize: %d x %d\n", width, height);
#endif /*DEBUG*/

	imagedisplay_layout(imagedisplay);

	gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

static void
imagedisplay_focus_notify(GtkEventControllerFocus *focus,
	GParamSpec *pspec, gpointer user_data)
{
	Imagedisplay *imagedisplay = (Imagedisplay *) user_data;

#ifdef DEBUG
	printf("imagedisplay_focus_notify: %s\n", pspec->name);
#endif /*DEBUG*/

	gtk_widget_queue_draw(GTK_WIDGET(imagedisplay));
}

static void
imagedisplay_click(GtkEventController *controller,
	int n_press, double x, double y, Imagedisplay *imagedisplay)
{
	gtk_widget_grab_focus(GTK_WIDGET(imagedisplay));
}

static void
imagedisplay_init(Imagedisplay *imagedisplay)
{
	GtkEventController *controller;

#ifdef DEBUG
	printf("imagedisplay_init:\n");
#endif /*DEBUG*/

	imagedisplay->scale = 1;

	g_signal_connect(GTK_DRAWING_AREA(imagedisplay), "resize",
		G_CALLBACK(imagedisplay_resize), NULL);

	gtk_widget_set_focusable(GTK_WIDGET(imagedisplay), TRUE);
	controller = gtk_event_controller_focus_new();
	gtk_widget_add_controller(GTK_WIDGET(imagedisplay), controller);
	g_signal_connect(controller, "notify::is-focus",
		G_CALLBACK(imagedisplay_focus_notify), imagedisplay);

	controller = GTK_EVENT_CONTROLLER(gtk_gesture_click_new());
	g_signal_connect(controller, "pressed",
		G_CALLBACK(imagedisplay_click), imagedisplay);
	gtk_widget_add_controller(GTK_WIDGET(imagedisplay), controller);

	imagedisplay->bestfit = TRUE;

	// the tilesource attaches to the tilecache in
	// imagedisplay_set_tilesource()
	imagedisplay->tilecache = tilecache_new();

	g_signal_connect_object(imagedisplay->tilecache, "changed",
		G_CALLBACK(imagedisplay_tilecache_changed),
		imagedisplay, 0);
	g_signal_connect_object(imagedisplay->tilecache, "tiles-changed",
		G_CALLBACK(imagedisplay_tilecache_tiles_changed),
		imagedisplay, 0);
	g_signal_connect_object(imagedisplay->tilecache, "area-changed",
		G_CALLBACK(imagedisplay_tilecache_area_changed),
		imagedisplay, 0);
}

static void
imagedisplay_class_init(ImagedisplayClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(class);

#ifdef DEBUG
	printf("imagedisplay_class_init:\n");
#endif /*DEBUG*/

	gobject_class->dispose = imagedisplay_dispose;
	gobject_class->set_property = imagedisplay_set_property;
	gobject_class->get_property = imagedisplay_get_property;

	widget_class->snapshot = imagedisplay_snapshot;

	g_object_class_install_property(gobject_class, PROP_TILESOURCE,
		g_param_spec_object("tilesource",
			_("Tile source"),
			_("The tile source to be displayed"),
			TILESOURCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BESTFIT,
		g_param_spec_boolean("bestfit",
			_("Bestfit"),
			_("Shrink on first display"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_BACKGROUND,
		g_param_spec_int("background",
			_("Background"),
			_("Background mode"),
			0, TILECACHE_BACKGROUND_LAST - 1,
			TILECACHE_BACKGROUND_CHECKERBOARD,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_ZOOM,
		g_param_spec_double("zoom",
			_("Zoom"),
			_("Zoom of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_X,
		g_param_spec_double("x",
			_("x"),
			_("Horizontal position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_Y,
		g_param_spec_double("y",
			_("y"),
			_("Vertical position of viewport"),
			-VIPS_MAX_COORD, VIPS_MAX_COORD, 0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_PIXEL_SIZE,
		g_param_spec_double("pixel-size",
			_("Pixel size"),
			_("Size of hardware display pixels in gtk coordinates"),
			0.0, 10.0, 0.0,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_DEBUG,
		g_param_spec_boolean("debug",
			_("Debug"),
			_("Render snapshot in debug mode"),
			FALSE,
			G_PARAM_READWRITE));

	g_object_class_override_property(gobject_class,
		PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property(gobject_class,
		PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property(gobject_class,
		PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property(gobject_class,
		PROP_VSCROLL_POLICY, "vscroll-policy");

	imagedisplay_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	imagedisplay_signals[SIG_SNAPSHOT] = g_signal_new("snapshot",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_SNAPSHOT);

	gtk_widget_class_set_css_name(widget_class, "imagedisplay");
}

Imagedisplay *
imagedisplay_new(Tilesource *tilesource)
{
	Imagedisplay *imagedisplay;

#ifdef DEBUG
	printf("imagedisplay_new:\n");
#endif /*DEBUG*/

	imagedisplay = g_object_new(imagedisplay_get_type(),
		"tilesource", tilesource,
		NULL);

	return imagedisplay;
}

/* image	level0 image coordinates ... this is the coordinate space we
 *			pass down to tilecache
 *
 * gtk		screen cods, so the coordinates we use to render tiles
 */

void
imagedisplay_image_to_gtk(Imagedisplay *imagedisplay,
	double x_image, double y_image, double *x_gtk, double *y_gtk)
{
	*x_gtk = x_image * imagedisplay->scale -
		imagedisplay->x + imagedisplay->paint_rect.left;
	*y_gtk = y_image * imagedisplay->scale -
		imagedisplay->y + imagedisplay->paint_rect.top;
}

void
imagedisplay_gtk_to_image(Imagedisplay *imagedisplay,
	double x_gtk, double y_gtk, double *x_image, double *y_image)
{
	*x_image = (imagedisplay->x +
				   x_gtk - imagedisplay->paint_rect.left) /
		imagedisplay->scale;
	*y_image = (imagedisplay->y +
				   y_gtk - imagedisplay->paint_rect.top) /
		imagedisplay->scale;

	*x_image = VIPS_CLIP(0, *x_image, imagedisplay->image_rect.width - 1);
	*y_image = VIPS_CLIP(0, *y_image, imagedisplay->image_rect.height - 1);
}
