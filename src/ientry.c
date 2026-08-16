/* a text entry widget with OK, cancel and modified
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
#define DEBUG
 */

enum {
	PROP_TEXT = 1,
	PROP_WIDTH_CHARS,
	PROP_LAST,

	SIG_CHANGED,
	SIG_ACTIVATE,
	SIG_CANCEL,

	SIG_LAST
};

static guint ientry_signals[SIG_LAST] = { 0 };

G_DEFINE_TYPE(iEntry, ientry, GTK_TYPE_WIDGET);

static void
ientry_dispose(GObject *object)
{
	iEntry *ientry = (iEntry *) object;

#ifdef DEBUG
	printf("ientry_dispose:\n");
#endif /*DEBUG*/

	gtk_widget_dispose_template(GTK_WIDGET(ientry), IENTRY_TYPE);

	G_OBJECT_CLASS(ientry_parent_class)->dispose(object);
}

void
ientry_changed(iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_changed\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(ientry), ientry_signals[SIG_CHANGED], 0);
}

/* Activated!
 */
static void
ientry_activate(iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_activate\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(ientry), ientry_signals[SIG_ACTIVATE], 0);
}

/* Just the slider changed.
 */
static void
ientry_cancel(iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_cancel\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(ientry), ientry_signals[SIG_CANCEL], 0);
}

static void
ientry_entry_length_notify(GtkWidget *widget, GParamSpec *pspec, iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_length_notify:\n");
#endif /*DEBUG*/

	ientry_changed(ientry);
}

static void
ientry_init(iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_init:\n");
#endif /*DEBUG*/

	VIPS_SETSTR(ientry->text, "");

	gtk_widget_init_template(GTK_WIDGET(ientry));

	// for some reason notify::text-length on the entry doesn't work ... we
	// need to watch the buffer
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(GTK_ENTRY(ientry->entry));
	g_signal_connect(buffer, "notify::length",
		G_CALLBACK(ientry_entry_length_notify), ientry);
}

static void
ientry_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	iEntry *ientry = (iEntry *) object;
	g_autofree char *text = NULL;

	switch (prop_id) {
	case PROP_TEXT:
		text = gtk_editable_get_chars(GTK_EDITABLE(ientry->entry), 0, -1);
		VIPS_SETSTR(ientry->text, text);
		g_value_set_string(value, text);
		break;

	case PROP_WIDTH_CHARS:
		g_object_get_property(G_OBJECT(ientry->entry), "width-chars", value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
ientry_set_text(iEntry *ientry, const char *text)
{
	GtkEntryBuffer *buffer = gtk_entry_get_buffer(GTK_ENTRY(ientry->entry));

	g_signal_handlers_block_matched(G_OBJECT(buffer),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ientry);

	if (text &&
		!g_str_equal(text, ientry->text)) {
		gtk_entry_buffer_set_text(buffer, text, -1);
		VIPS_SETSTR(ientry->text, text);
	}

	g_signal_handlers_unblock_matched(G_OBJECT(buffer),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ientry);
}

static void
ientry_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	iEntry *ientry = (iEntry *) object;

	switch (prop_id) {
	case PROP_TEXT:
		ientry_set_text(ientry, g_value_get_string(value));
		break;

	case PROP_WIDTH_CHARS:
		g_object_set_property(G_OBJECT(ientry->entry), "width-chars", value);
		g_object_set_property(G_OBJECT(ientry->entry),
			"max-width-chars", value);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* Detect cancel in a text field.
 */
static gboolean
ientry_entry_key_pressed(GtkEventControllerKey *self,
	guint keyval, guint keycode, GdkModifierType state, iEntry *ientry)
{
	gboolean handled;

#ifdef DEBUG
	printf("ientry_entry_key_pressed:\n");
#endif /*DEBUG*/

	handled = FALSE;
	if (keyval == GDK_KEY_Escape) {
		// reset entry, don't trigger "changed"
		g_signal_handlers_block_matched(G_OBJECT(ientry->entry),
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ientry);
		if (ientry->text)
			gtk_editable_set_text(GTK_EDITABLE(ientry->entry), ientry->text);
		g_signal_handlers_unblock_matched(G_OBJECT(ientry->entry),
			G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, ientry);

		ientry_cancel(ientry);

		handled = TRUE;
	}

	return handled;
}

static void
ientry_entry_activate(GtkWidget *wid, iEntry *ientry)
{
#ifdef DEBUG
	printf("ientry_entry_activate:\n");
#endif /*DEBUG*/

	ientry_activate(ientry);
}

static void
ientry_class_init(iEntryClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("ientry_class_init:\n");
#endif /*DEBUG*/

	gobject_class->dispose = ientry_dispose;
	gobject_class->get_property = ientry_get_property;
	gobject_class->set_property = ientry_set_property;

	BIND_RESOURCE("ientry.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(iEntry, entry);

	BIND_CALLBACK(ientry_entry_key_pressed);
	BIND_CALLBACK(ientry_entry_activate);

	g_object_class_install_property(gobject_class, PROP_TEXT,
		g_param_spec_string("text",
			_("Text"),
			_("Text string to edit"),
			"",
			G_PARAM_READWRITE));

	g_object_class_install_property(gobject_class, PROP_WIDTH_CHARS,
		g_param_spec_int("width-chars",
			_("Width"),
			_("Display width in chars"),
			0, 1000, 100,
			G_PARAM_READWRITE));

	ientry_signals[SIG_CHANGED] = g_signal_new("changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	ientry_signals[SIG_ACTIVATE] = g_signal_new("activate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	ientry_signals[SIG_CANCEL] = g_signal_new("cancel",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

iEntry *
ientry_new(void)
{
	iEntry *ientry;

#ifdef DEBUG
	printf("ientry_new:\n");
#endif /*DEBUG*/

	ientry = g_object_new(IENTRY_TYPE, NULL);

	return ientry;
}

// how annoying, there must be an easy way to get grab_focus() on the iEntry
// to grab the enclosed widget
void
ientry_grab_focus(iEntry *ientry)
{
	gtk_widget_grab_focus(GTK_WIDGET(ientry->entry));
}

void
ientry_set_double(iEntry *ientry, int digits, double value)
{
	// this is used for tslider text update, so we need %gf (no e^x notation)
	g_autofree char *text = g_strdup_printf("%.*f",
		VIPS_CLIP(0, digits, 100), value);
	g_object_set(ientry, "text", text, NULL);
}

gboolean
ientry_get_double(iEntry *ientry, double *out)
{
	g_autofree char *text = NULL;
	g_object_get(ientry->entry, "text", &text, NULL);
	if (!text)
		return FALSE;

	char *end;
	double value = g_ascii_strtod(text, &end);
	if (end == text)
		return FALSE;
	if (strspn(end, WHITESPACE) != strlen(end))
		return FALSE;

	*out = value;

	return TRUE;
}
