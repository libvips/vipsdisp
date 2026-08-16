/* show image properties
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

/*
#define DEBUG
 */

#include "nip4.h"

#define NUM_INEXACT_MATCHES (15)

struct _Properties {
	GtkWidget parent_instance;

	Tilesource *tilesource;

	GtkWidget *revealer;
	GtkWidget *properties;
	GtkWidget *search_entry;
	GtkWidget *scrolled_window;

	// build a grid of labels here
	GtkWidget *grid;

	// null, or the string we are matching against
	char *pattern;

	int row_number;
};

G_DEFINE_TYPE(Properties, properties, GTK_TYPE_WIDGET);

enum {
	PROP_TILESOURCE = 1,
	PROP_REVEALED,

	SIG_LAST
};

#ifdef DEBUG
static const char *
properties_property_name(guint prop_id)
{
	switch (prop_id) {
	case PROP_TILESOURCE:
		return "TILESOURCE";
		break;

	case PROP_REVEALED:
		return "REVEALED";
		break;

	default:
		return "<unknown>";
	}
}
#endif /*DEBUG*/

/* Clean up any existing children of @left_column and @right_column and any
 * existing elements of the GList @field_list.
 *
 * @m	Properties *	this
 */
static void
properties_clear(Properties *p)
{
	p->grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(p->grid), 3);
	gtk_grid_set_column_spacing(GTK_GRID(p->grid), 10);
	// will unparent and destroy any old child
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(p->scrolled_window),
		p->grid);
	p->row_number = 0;
}

static void
properties_add_row(Properties *p,
	const char *label, GtkWidget *item)
{
	GtkWidget *t;

	t = gtk_label_new(label);
	// can't set alignment in CSS for some reason
	gtk_widget_set_halign(t, GTK_ALIGN_START);
	gtk_label_set_selectable(GTK_LABEL(t), TRUE);
	if (item)
		gtk_widget_add_css_class(t, "properties-label");
	gtk_widget_add_css_class(t, p->row_number % 2 ? "odd" : "even");
	gtk_grid_attach(GTK_GRID(p->grid), t, 0, p->row_number, 1, 1);

	if (item) {
		gtk_widget_add_css_class(t, "properties-value");
		gtk_widget_add_css_class(item, p->row_number % 2 ? "odd" : "even");
		gtk_grid_attach(GTK_GRID(p->grid), item, 1, p->row_number, 1, 1);
	}

	p->row_number++;
}

static void
properties_add_item(Properties *p, const char *field, GValue *value)
{
	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	vips_buf_appendgv(&buf, value);
	GtkWidget *label = gtk_label_new(vips_buf_all(&buf));
	// can't set alignment in CSS for some reason
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	properties_add_row(p, field, label);
}

static void *
properties_refresh_add_item_cb(VipsImage *image,
	const char *field, GValue *value, void *client)
{
	Properties *p = PROPERTIES(client);

	properties_add_item(p, field, value);

	return NULL;
}

static void *
properties_refresh_add_item_match_cb(VipsImage *image,
	const char *field, GValue *value, void *client)
{
	Properties *p = PROPERTIES(client);

	if (g_strrstr(field, p->pattern))
		properties_add_item(p, field, value);

	return NULL;
}

static void
properties_refresh(Properties *p)
{
	VipsImage *image;

#ifdef DEBUG
	printf("properties_refresh:\n");
#endif

	properties_clear(p);

	if (p->tilesource &&
		(image = tilesource_get_image(p->tilesource))) {
		if (p->pattern) {
			vips_image_map(image, properties_refresh_add_item_match_cb, p);

			properties_add_row(p, "", NULL);
			properties_add_row(p, "Did you mean ...", NULL);

			g_auto(GStrv) fields = vips_image_get_fields(image);
			GSList *matches = fuzzy_match(fields, p->pattern);
			int n_displayed;

			n_displayed = 0;
			for (GSList *i = matches; i; i = i->next) {
				Fuzzy *fuzzy = (Fuzzy *) i->data;

				// don't show fields we have already displayed in the
				// main search area
				if (!g_strrstr(fuzzy->field, p->pattern)) {
					g_auto(GValue) value = { 0 };

					vips_image_get(image, fuzzy->field, &value);
					properties_add_item(p, fuzzy->field, &value);

					if (n_displayed++ > NUM_INEXACT_MATCHES)
						break;
				}
			}

			g_slist_free_full(g_steal_pointer(&matches), g_free);
		}
		else
			vips_image_map(image, properties_refresh_add_item_cb, p);
	}
}

/* This is called when the Tilesource changes. In particular, a new VipsImage
 * might have been loaded, or there might no image loaded. Clean up any
 * existing input widgets and create new ones. This is the callback for the
 * "changed" event on @p->imagewindow->tilesource.
 *
 * @tilesource		The new tilesource, which is currently held by
 * 			p->imagewindow.
 *
 * @m	Properties *	this
 */
static void
properties_tilesource_changed(Tilesource *tilesource, Properties *p)
{
#ifdef DEBUG
	printf("properties_tilesource_changed:\n");
#endif

	properties_refresh(p);
}

static void
properties_set_tilesource(Properties *p, Tilesource *tilesource)
{
#ifdef DEBUG
	printf("properties_set_tilesource:\n");
#endif

	/* No need to ref ... the enclosing window holds a ref to us.
	 */
	p->tilesource = tilesource;
	g_signal_connect_object(tilesource,
		"changed", G_CALLBACK(properties_tilesource_changed),
		p, 0);

	// first refresh
	properties_refresh(p);
}

static void
properties_search_changed(GtkWidget *search_entry, gpointer user_data)
{
	Properties *p = PROPERTIES(user_data);
	const char *patt = gtk_editable_get_text(GTK_EDITABLE(search_entry));

	VIPS_FREE(p->pattern);
	if (patt &&
		g_ascii_strcasecmp(patt, "") != 0 &&
		strspn(patt, " \t\n") != strlen(patt)) {
		p->pattern = g_strdup(patt);
	}

#ifdef DEBUG
	printf("properties_search_changed: pattern = %s\n", p->pattern);
#endif

	properties_refresh(p);
}

static void
properties_dispose(GObject *p_)
{
	Properties *p = PROPERTIES(p_);

#ifdef DEBUG
	printf("properties_dispose:\n");
#endif

	VIPS_FREEF(gtk_widget_unparent, p->revealer);
	VIPS_FREE(p->pattern);

	G_OBJECT_CLASS(properties_parent_class)->dispose(p_);
}

static void
properties_set_property(GObject *object, guint prop_id,
	const GValue *value, GParamSpec *pspec)
{
	Properties *p = PROPERTIES(object);

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);

		str = g_strdup_value_contents(value);
		printf("properties_set_property: %s %s\n",
			properties_property_name(prop_id), str);
	}
#endif /*DEBUG*/

	switch (prop_id) {
	case PROP_TILESOURCE:
		properties_set_tilesource(p,
			TILESOURCE(g_value_get_object(value)));
		break;

	case PROP_REVEALED: {
		gboolean revealed = g_value_get_boolean(value);
		gboolean current_revealed =
			gtk_revealer_get_reveal_child(GTK_REVEALER(p->revealer));

		if (current_revealed != revealed) {
			gtk_revealer_set_reveal_child(GTK_REVEALER(p->revealer), revealed);
			g_object_notify_by_pspec(object, pspec);
		}
	} break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(p, prop_id, pspec);
	}
}

static void
properties_get_property(GObject *p_,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Properties *p = PROPERTIES(p_);

	switch (prop_id) {
	case PROP_TILESOURCE:
		g_value_set_object(value, p->tilesource);
		break;

	case PROP_REVEALED:
		g_value_set_boolean(value,
			gtk_revealer_get_reveal_child(GTK_REVEALER(p->revealer)));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(p_, prop_id, pspec);
	}

#ifdef DEBUG
	{
		g_autofree char *str = g_strdup_value_contents(value);
		printf("properties_get_property: %s %s\n",
			properties_property_name(prop_id), str);
	}
#endif /*DEBUG*/
}

static void
properties_init(Properties *p)
{
#ifdef DEBUG
	printf("properties_init:\n");
#endif

	gtk_widget_init_template(GTK_WIDGET(p));
}

/* Initialize PropertiesClass, which GObject defines for us if we used the
 * boilerplate macros and code correctly.
 *
 * @class	PropertiesClass *	this class
 *
 * (GObject boilerplate).
 */
static void
properties_class_init(PropertiesClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("properties_class_init:\n");
#endif

	BIND_RESOURCE("properties.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Properties, revealer);
	BIND_VARIABLE(Properties, properties);
	BIND_VARIABLE(Properties, search_entry);
	BIND_VARIABLE(Properties, scrolled_window);

	BIND_CALLBACK(properties_search_changed);

	gobject_class->dispose = properties_dispose;
	gobject_class->set_property = properties_set_property;
	gobject_class->get_property = properties_get_property;

	g_object_class_install_property(gobject_class, PROP_TILESOURCE,
		g_param_spec_object("tilesource",
			_("Tile source"),
			_("The tile source whose properties we display"),
			TILESOURCE_TYPE,
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_REVEALED,
		g_param_spec_boolean("revealed", NULL, NULL,
			TRUE,
			G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
}
