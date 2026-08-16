/* a slider with an editable value
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

enum {
	SIG_CHANGED,
	SIG_CANCEL,
	SIG_ACTIVATE,
	SIG_SLIDER_CHANGED,
	SIG_TEXT_CHANGED,

	LAST
};

static guint tslider_signals[LAST] = { 0 };

/* Are two doubles more or less equal. We need this when we check the sliders
 * for update to stop loops. The 0.0001 is a bit of a fudge :-(
 */
#define DEQ(A, B) (VIPS_ABS((A) - (B)) < 0.0001)

G_DEFINE_TYPE(Tslider, tslider, GTK_TYPE_WIDGET);

static void
tslider_dispose(GObject *object)
{
	Tslider *tslider = (Tslider *) object;

#ifdef DEBUG
	printf("tslider_dispose:\n");
#endif /*DEBUG*/

	VIPS_FREEF(gtk_widget_unparent, tslider->box);

	G_OBJECT_CLASS(tslider_parent_class)->dispose(object);
}

/* Map a value to a slider position.
 */
static double
tslider_value_to_slider(Tslider *tslider, double value)
{
	// don't /0
	if (tslider->to == tslider->from)
		return 0;

	/* Map our range to 0-1.
	 */
	const double scale = 1.0 / (tslider->to - tslider->from);
	const double to01 = (value - tslider->from) * scale;

	/* Pass through user fn.
	 */
	const double mapped = tslider->value_to_slider(
		tslider->from, tslider->to, to01);
	const double nvalue = mapped / scale + tslider->from;

#ifdef DEBUG
	printf("tslider_value_to_slider: %g, to %g\n", value, nvalue);
#endif /*DEBUG*/

	/* Map back to main range.
	 */
	return nvalue;
}

/* Map a slider position to a value.
 */
static double
tslider_slider_to_value(Tslider *tslider, double value)
{
	// don't /0
	if (tslider->to == tslider->from)
		return 0;

	/* Map our range to 0-1.
	 */
	const double scale = 1.0 / (tslider->to - tslider->from);
	const double to01 = (value - tslider->from) * scale;

	/* Pass through user fn.
	 */
	const double mapped = tslider->slider_to_value(
		tslider->from, tslider->to, to01);
	const double nvalue = mapped / scale + tslider->from;

#ifdef DEBUG
	printf("tslider_slider_to_value: %g, to %g\n", value, nvalue);
#endif /*DEBUG*/

	/* Map back to main range.
	 */
	return nvalue;
}

/* from/to/value have changed ... update the widgets.
 */
static void
tslider_real_changed(Tslider *tslider)
{
	GtkAdjustment *adj = tslider->adj;
	GtkWidget *ientry = tslider->entry;

#ifdef DEBUG
	printf("tslider_real_changed: %p, val = %g\n", tslider, tslider->value);
#endif /*DEBUG*/

	if (tslider->auto_link)
		tslider->svalue = tslider_value_to_slider(tslider, tslider->value);

	g_signal_handlers_block_matched(G_OBJECT(adj),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tslider);
	g_signal_handlers_block_matched(G_OBJECT(ientry),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tslider);

	ientry_set_double(IENTRY(ientry), tslider->digits, tslider->value);
	gtk_scale_set_digits(GTK_SCALE(tslider->scale), tslider->digits);

	if (!DEQ(tslider->from, tslider->last_from) ||
		!DEQ(tslider->to, tslider->last_to)) {
		double range = tslider->to - tslider->from;

		gtk_adjustment_set_step_increment(adj, range / 100);
		gtk_adjustment_set_page_increment(adj, range / 10);
		gtk_adjustment_set_page_size(adj, range / 10);
		gtk_adjustment_set_lower(adj, tslider->from);
		gtk_adjustment_set_upper(adj,
			tslider->to + gtk_adjustment_get_page_size(adj));

		tslider->last_to = tslider->to;
		tslider->last_from = tslider->from;
	}

	// we can get nan here during init
	if (!isnan(tslider->svalue) &&
		!DEQ(tslider->svalue, tslider->last_svalue))
		gtk_adjustment_set_value(adj, tslider->svalue);

	g_signal_handlers_unblock_matched(G_OBJECT(adj),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tslider);
	g_signal_handlers_unblock_matched(G_OBJECT(ientry),
		G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, tslider);
}

/* From/to/value have changed ... tell everyone.
 */
void
tslider_changed(Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_changed\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(tslider), tslider_signals[SIG_CHANGED], 0);
}

/* Activated!
 */
static void
tslider_activate(Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_activate\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(tslider), tslider_signals[SIG_ACTIVATE], 0);
}

/* Just the slider changed, our caller will need to do some work to map svalue
 * to value.
 */
static void
tslider_slider_changed(Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_slider_changed\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(tslider), tslider_signals[SIG_SLIDER_CHANGED], 0);
}

/* Text has been touched.
 */
static void
tslider_text_changed(Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_text_changed\n");
#endif /*DEBUG*/

	g_signal_emit(G_OBJECT(tslider), tslider_signals[SIG_TEXT_CHANGED], 0);
}

/* Drag on slider.
 */
static void
tslider_value_changed(GtkAdjustment *adj, Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_value_changed:\n");
#endif /*DEBUG*/

	if (tslider->svalue != gtk_adjustment_get_value(adj)) {
		tslider->svalue = gtk_adjustment_get_value(adj);

		if (tslider->auto_link) {
			tslider->value = tslider_slider_to_value( tslider, tslider->svalue);
			tslider_changed(tslider);
		}
		else
			tslider_slider_changed(tslider);
	}
}

/* Default identity conversion.
 */
static double
tslider_conversion_id(double from, double to, double value)
{
	return value;
}

static void
tslider_init(Tslider *tslider)
{
#ifdef DEBUG
	printf("tslider_init:\n");
#endif /*DEBUG*/

	/* Any old start values ... overridden later.
	 */
	tslider->from = -1;
	tslider->to = -1;
	tslider->value = -1;
	tslider->svalue = -1;
	tslider->digits = -1;
	tslider->last_to = -1;
	tslider->last_from = -1;
	tslider->last_svalue = -1;

	gtk_widget_init_template(GTK_WIDGET(tslider));

	g_signal_connect(tslider->adj, "value_changed",
		G_CALLBACK(tslider_value_changed), tslider);

	tslider->auto_link = TRUE;
	tslider->slider_to_value = tslider_conversion_id;
	tslider->value_to_slider = tslider_conversion_id;

	tslider_changed(tslider);
}

static void
tslider_ientry_changed(iEntry *ientry, Tslider *tslider)
{
	tslider_text_changed(tslider);
}

static void
tslider_ientry_activate(iEntry *ientry, Tslider *tslider)
{
	double value;

#ifdef DEBUG
	printf("tslider_ientry_activate:\n");
#endif /*DEBUG*/

	if (ientry_get_double(ientry, &value) &&
		tslider->value != value) {
		tslider->value = value;

		if (tslider->auto_link)
			tslider_changed(tslider);
		else
			tslider_activate(tslider);
	}
}

static void
tslider_class_init(TsliderClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("tslider_class_init:\n");
#endif /*DEBUG*/

	G_OBJECT_CLASS(class)->dispose = tslider_dispose;

	class->changed = tslider_real_changed;

	BIND_RESOURCE("tslider.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Tslider, adj);
	BIND_VARIABLE(Tslider, box);
	BIND_VARIABLE(Tslider, entry);
	BIND_VARIABLE(Tslider, scale);

	BIND_CALLBACK(tslider_ientry_changed);
	BIND_CALLBACK(tslider_ientry_activate);

	/* Create signals.
	 */
	tslider_signals[SIG_CHANGED] = g_signal_new("changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET(TsliderClass, changed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	tslider_signals[SIG_ACTIVATE] = g_signal_new("activate",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	tslider_signals[SIG_SLIDER_CHANGED] = g_signal_new("slider_changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
	tslider_signals[SIG_TEXT_CHANGED] = g_signal_new("text_changed",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* We never emit this, but we have it so that tslider has the same set of
	 * signals as ientry and we can use the same code to link to them.
	 */
	tslider_signals[SIG_CANCEL] = g_signal_new("cancel",
		G_OBJECT_CLASS_TYPE(gobject_class),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

GtkWidget *
tslider_new(double from, double to, int digits)
{
	Tslider *tslider;

#ifdef DEBUG
	printf("tslider_new:\n");
#endif /*DEBUG*/

	tslider = g_object_new(TSLIDER_TYPE, NULL);
	tslider->from = from;
	tslider->to = to;
	tslider->digits = digits;

	return GTK_WIDGET(tslider);
}

void
tslider_set_conversions(Tslider *tslider,
	TsliderFn value_to_slider, TsliderFn slider_to_value)
{
	tslider->value_to_slider = value_to_slider;
	tslider->slider_to_value = slider_to_value;

	tslider->auto_link = value_to_slider && slider_to_value;
}

double
tslider_log_value_to_slider(double from, double to, double value)
{
	// don't /0
	if (to == from)
		return 0;

	/* What does 1.0 map to on our [0,1] scale?
	 */
	const double mapped1 = (1.0 - from) / (to - from);

	/* We want an exponent which maps the mid point on the slider to 1.
	 */
	const double a = log(mapped1) / log(0.5);

	const double nvalue = pow(value, 1.0 / a);

	return nvalue;
}

double
tslider_log_slider_to_value(double from, double to, double value)
{
	// don't /0
	if (to == from)
		return 0;

	/* What does 1.0 map to on our [0,1] scale?
	 */
	const double mapped1 = (1.0 - from) / (to - from);

	/* We want an exponent which maps the mid point on the slider to 1.
	 */
	const double a = log(mapped1) / log(0.5);

	const double nvalue = pow(value, a);

	return nvalue;
}
