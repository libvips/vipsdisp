/* the display control widgets
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

struct _Displaybar {
	GtkWidget parent_instance;

	/* The imagewindow we attach to.
	 */
	Imagewindow *win;

	/* A ref to the tilesource we are currently controlling.
	 */
	Tilesource *tilesource;

	/* Not a ref ... the imageui win is displaying.
	 */
	Imageui *imageui;

	GtkWidget *action_bar;
	GtkWidget *gears;
	GtkWidget *page;
	GtkWidget *scale;
	GtkWidget *offset;

	/* We have to disconnect and reconnect these when imagewindow gets a new
	 * tilesource.
	 */
	guint changed_sid;
	guint tiles_changed_sid;
	guint page_changed_sid;
	guint imageui_changed_sid;

	/* Keep view settings on new image.
	 */
	gboolean preserve;

	/* Save and restore view settings here.
	 */
	ViewSettings view_settings;
};

G_DEFINE_TYPE(Displaybar, displaybar, GTK_TYPE_WIDGET);

enum {
	PROP_IMAGEWINDOW = 1,
	PROP_REVEALED,
	PROP_PRESERVE,

	SIG_LAST
};

/* Save and restore view settings.
 */

static void
displaybar_save_view_settings(Displaybar *displaybar)
{
	Tilesource *tilesource = displaybar->tilesource;
	Imageui *imageui = displaybar->imageui;
	ViewSettings *view_settings = &displaybar->view_settings;

	if (tilesource &&
		imageui) {
#ifdef DEBUG
		printf("displaybar_save_view_settings:\n");
#endif /*DEBUG*/

		g_object_get(tilesource,
			"mode", &view_settings->mode,
			"scale", &view_settings->scale,
			"offset", &view_settings->offset,
			"page", &view_settings->page,
			"falsecolour", &view_settings->falsecolour,
			"log", &view_settings->log,
			"icc", &view_settings->icc,
			"active", &view_settings->active,
			NULL);

		g_object_get(imageui,
			"background", &view_settings->background,
			NULL);

		view_settings->valid = TRUE;
	}
}

static void
displaybar_apply_view_settings(Displaybar *displaybar)
{
	ViewSettings *view_settings = &displaybar->view_settings;

	if (view_settings->valid) {
#ifdef DEBUG
		printf("displaybar_apply_view_settings:\n");
#endif /*DEBUG*/

		Tilesource *tilesource = displaybar->tilesource;
		if (tilesource)
			g_object_set(tilesource,
				"mode", view_settings->mode,
				"scale", view_settings->scale,
				"offset", view_settings->offset,
				"page", view_settings->page,
				"falsecolour", view_settings->falsecolour,
				"log", view_settings->log,
				"icc", view_settings->icc,
				"active", TRUE,
				NULL);

		Imageui *imageui = displaybar->imageui;
		if (imageui)
			g_object_set(imageui,
				"background", view_settings->background,
				NULL);
	}
}

static void
displaybar_tilesource_changed(Tilesource *tilesource, Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_tilesource_changed:\n");
#endif /*DEBUG*/

	if (tilesource) {
		g_assert(tilesource == displaybar->tilesource);

		if (TSLIDER(displaybar->scale)->value != tilesource->scale) {
			TSLIDER(displaybar->scale)->value = tilesource->scale;
			tslider_changed(TSLIDER(displaybar->scale));
		}

		if (TSLIDER(displaybar->offset)->value != tilesource->offset) {
			TSLIDER(displaybar->offset)->value = tilesource->offset;
			tslider_changed(TSLIDER(displaybar->offset));
		}

		gtk_spin_button_set_range(GTK_SPIN_BUTTON(displaybar->page),
			0, tilesource->n_pages - 1);
		gtk_widget_set_sensitive(displaybar->page,
			tilesource->n_pages > 1 &&
				tilesource->mode == TILESOURCE_MODE_MULTIPAGE);

		displaybar_save_view_settings(displaybar);
	}
}

static void
displaybar_page_changed(Tilesource *tilesource, Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_page_changed:\n");
#endif /*DEBUG*/

	if (tilesource) {
		g_assert(tilesource == displaybar->tilesource);

		gtk_spin_button_set_value(GTK_SPIN_BUTTON(displaybar->page),
			tilesource->page);

		displaybar_save_view_settings(displaybar);
	}
}

static void
displaybar_imageui_changed(Imageui *imageui, Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_imageui_changed:\n");
#endif /*DEBUG*/

	g_assert(imageui == displaybar->imageui);

	displaybar_save_view_settings(displaybar);
}

static void
displaybar_disconnect(Displaybar *displaybar)
{
	if (displaybar->tilesource) {
		FREESID(displaybar->changed_sid, displaybar->tilesource);
		FREESID(displaybar->tiles_changed_sid, displaybar->tilesource);
		FREESID(displaybar->page_changed_sid, displaybar->tilesource);

		VIPS_UNREF(displaybar->tilesource);
	}

	if (displaybar->imageui) {
		FREESID(displaybar->imageui_changed_sid, displaybar->imageui);
		displaybar->imageui = NULL;
	}
}

static void
displaybar_set_tilesource(Displaybar *displaybar, Tilesource *new_tilesource)
{
	if (new_tilesource) {
		displaybar->changed_sid = g_signal_connect(new_tilesource,
			"changed",
			G_CALLBACK(displaybar_tilesource_changed), displaybar);
		displaybar->tiles_changed_sid = g_signal_connect(new_tilesource,
			"tiles-changed",
			G_CALLBACK(displaybar_tilesource_changed), displaybar);
		displaybar->page_changed_sid = g_signal_connect(new_tilesource,
			"page-changed",
			G_CALLBACK(displaybar_page_changed), displaybar);

		displaybar->tilesource = new_tilesource;
		g_object_ref(new_tilesource);
	}
}

static void
displaybar_set_imageui(Displaybar *displaybar)
{
	Imageui *imageui = imagewindow_get_imageui(displaybar->win);

	displaybar->imageui = imageui;
	if (imageui)
		displaybar->imageui_changed_sid = g_signal_connect(imageui,
			"changed",
			G_CALLBACK(displaybar_imageui_changed), displaybar);
}

/* Imagewindow has a new image, eg. after < > in titlebar.
 */
static void
displaybar_imagewindow_new_image(Imagewindow *win, Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_imagewindow_new_image:\n");
#endif /*DEBUG*/

	displaybar_disconnect(displaybar);

	displaybar_set_tilesource(displaybar, imagewindow_get_tilesource(win));
	displaybar_set_imageui(displaybar);

	if (displaybar->preserve)
		displaybar_apply_view_settings(displaybar);
	else {
		/* Init displaybar from new source.
		 */
		displaybar_tilesource_changed(displaybar->tilesource, displaybar);
		displaybar_page_changed(displaybar->tilesource, displaybar);
	}
}

/* Imagewindow has changed the image eg. after a recalc. There's a new
 * tilesource.
 */
static void
displaybar_imagewindow_changed(Imagewindow *win, Displaybar *displaybar)
{
#ifdef DEBUG
	printf("displaybar_imagewindow_changed:\n");
#endif /*DEBUG*/

	displaybar_disconnect(displaybar);

	displaybar_set_tilesource(displaybar, imagewindow_get_tilesource(win));
	displaybar_set_imageui(displaybar);

	displaybar_apply_view_settings(displaybar);
}

static void
displaybar_set_imagewindow(Displaybar *displaybar, Imagewindow *win)
{
#ifdef DEBUG
	printf("displaybar_set_imagewindow:\n");
#endif /*DEBUG*/

	/* No need to ref ... win holds a ref to us.
	 */
	displaybar->win = win;

	g_signal_connect_object(win, "changed",
		G_CALLBACK(displaybar_imagewindow_changed), displaybar, 0);
	g_signal_connect_object(win, "new-image",
		G_CALLBACK(displaybar_imagewindow_new_image), displaybar, 0);

	displaybar_imagewindow_new_image(win, displaybar);
}

static void
displaybar_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Displaybar *displaybar = (Displaybar *) object;

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		displaybar_set_imagewindow(displaybar, g_value_get_object(value));
		break;

	case PROP_REVEALED:
		gtk_action_bar_set_revealed(GTK_ACTION_BAR(displaybar->action_bar),
			g_value_get_boolean(value));
		break;

	case PROP_PRESERVE:
		displaybar->preserve = g_value_get_boolean(value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
displaybar_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Displaybar *displaybar = (Displaybar *) object;
	GtkActionBar *action_bar = GTK_ACTION_BAR(displaybar->action_bar);

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		g_value_set_object(value, displaybar->win);
		break;

	case PROP_REVEALED:
		g_value_set_boolean(value, gtk_action_bar_get_revealed(action_bar));
		break;

	case PROP_PRESERVE:
		g_value_set_boolean(value, displaybar->preserve);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
displaybar_dispose(GObject *object)
{
	Displaybar *displaybar = (Displaybar *) object;

#ifdef DEBUG
	printf("displaybar_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(displaybar->tilesource);
	VIPS_FREEF(gtk_widget_unparent, displaybar->action_bar);

	G_OBJECT_CLASS(displaybar_parent_class)->dispose(object);
}

static void
displaybar_page_value_changed(GtkSpinButton *spin_button,
	Displaybar *displaybar)
{
	Tilesource *tilesource = displaybar->tilesource;
	int new_page = gtk_spin_button_get_value_as_int(spin_button);

#ifdef DEBUG
	printf("displaybar_page_value_changed: %d\n", new_page);
#endif /*DEBUG*/

	if (tilesource)
		g_object_set(tilesource,
			"page", new_page,
			NULL);
}

static void
displaybar_scale_value_changed(Tslider *slider, Displaybar *displaybar)
{
	Tilesource *tilesource = displaybar->tilesource;

	if (tilesource)
		g_object_set(tilesource,
			"scale", slider->value,
			NULL);
}

static void
displaybar_offset_value_changed(Tslider *slider, Displaybar *displaybar)
{
	Tilesource *tilesource = displaybar->tilesource;

	if (tilesource)
		g_object_set(tilesource,
			"offset", slider->value,
			NULL);
}

// handy for debugging menu actions, see gtk/displaybar.ui
static void
displaybar_test_clicked(GtkButton *test, Displaybar *displaybar)
{
	Tilesource *tilesource = displaybar->tilesource;

	if (tilesource)
		g_object_set(tilesource,
			//"mode", TILESOURCE_MODE_TOILET_ROLL,
			"log", TRUE,
			NULL);
}

static void
displaybar_init(Displaybar *displaybar)
{
	Tslider *tslider;

#ifdef DEBUG
	printf("displaybar_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(displaybar));

	tslider = TSLIDER(displaybar->scale);
	tslider_set_conversions(tslider,
		tslider_log_value_to_slider, tslider_log_slider_to_value);
	tslider->from = 0.001;
	tslider->to = 255;
	tslider->value = 1.0;
	tslider->svalue = 128;
	tslider->digits = 3;
	tslider_changed(tslider);

	tslider = TSLIDER(displaybar->offset);
	tslider->from = -128;
	tslider->to = 128;
	tslider->value = 0;
	tslider->svalue = 0;
	tslider->digits = 4;
	tslider_changed(tslider);
}

static void
displaybar_class_init(DisplaybarClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("displaybar_class_init:\n");
#endif /*DEBUG*/

	BIND_RESOURCE("displaybar.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Displaybar, action_bar);
	BIND_VARIABLE(Displaybar, gears);
	BIND_VARIABLE(Displaybar, page);
	BIND_VARIABLE(Displaybar, scale);
	BIND_VARIABLE(Displaybar, offset);
	BIND_VARIABLE(Displaybar, offset);

	BIND_CALLBACK(displaybar_page_value_changed);
	BIND_CALLBACK(displaybar_scale_value_changed);
	BIND_CALLBACK(displaybar_offset_value_changed);
	BIND_CALLBACK(displaybar_test_clicked);

	gobject_class->dispose = displaybar_dispose;
	gobject_class->set_property = displaybar_set_property;
	gobject_class->get_property = displaybar_get_property;

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

	g_object_class_install_property(gobject_class, PROP_PRESERVE,
		g_param_spec_boolean("preserve",
			_("preserve"),
			_("Preserve view settings on new image"),
			FALSE,
			G_PARAM_READWRITE));

}

Displaybar *
displaybar_new(Imagewindow *win)
{
	Displaybar *displaybar;

#ifdef DEBUG
	printf("displaybar_new:\n");
#endif /*DEBUG*/

	displaybar = g_object_new(displaybar_get_type(),
		"image-window", win,
		NULL);

	return displaybar;
}

ViewSettings *
displaybar_get_view_settings(Displaybar *displaybar)
{
	return &displaybar->view_settings;
}

void
displaybar_set_view_settings(Displaybar *displaybar,
	ViewSettings *view_settings)
{
	displaybar->view_settings = *view_settings;
	displaybar_apply_view_settings(displaybar);
}


