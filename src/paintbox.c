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

#include "package.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

typedef enum _PaintboxRubber {
	PAINTBOX_RUBBER_NONE,
	PAINTBOX_RUBBER_LINE,
	PAINTBOX_RUBBER_CIRCLE,
	PAINTBOX_RUBBER_RECT,
	PAINTBOX_RUBBER_BOX,
} PaintboxRubber;

typedef enum _PaintboxState {
	PAINTBOX_STATE_WAIT,
	PAINTBOX_STATE_DRAG,
} PaintboxState;

struct _Paintbox {
	GtkWidget parent_instance;

	/* The imagewindow we are on.
	 */
	Imagewindow *win;

	/* The imageui we are drawing on, and the signals we are watching.
	 */
	Imageui *imageui;
	GtkWidget *imagedisplay;
	guint drag_begin_sid;
	guint drag_update_sid;
	guint drag_end_sid;
	guint motion_sid;
	guint enter_sid;
	guint leave_sid;
	guint snapshot_sid;
	guint key_pressed_sid;

	/* Currently selected tool.
	 */
	PaintboxTool tool;

	/* State machine.
	 */
	PaintboxState state;

	double start_x;
	double start_y;

	/* Last position for line draw.
	 */
	int last_x;
	int last_y;

	/* Mask and for drawing.
	 */
	VipsImage *mask;

	/* Widgets.
	 */
	GtkWidget *action_bar;
	// tool select toggles
	GtkWidget *pointer;
	GtkWidget *brush;
	GtkWidget *line;
	GtkWidget *rect;
	GtkWidget *circle;
	GtkWidget *smudge;
	GtkWidget *flood_until;
	GtkWidget *flood_while;
	GtkWidget *text;
	GtkWidget *dropper;
	GtkWidget *tools[PAINTBOX_TOOL_LAST];
	GtkWidget *ink;
	GtkWidget *fill;
	GtkWidget *width;
	GtkWidget *font;
	GtkWidget *text_string;

	/* Our rubber-banding state. All in image cods.
	 */
	PaintboxRubber rubber;
	gboolean hide;
	int x0, y0;
	int x1, y1;
	int a, b;
};

G_DEFINE_TYPE(Paintbox, paintbox, GTK_TYPE_WIDGET);

enum {
	PROP_IMAGEWINDOW = 1,
	PROP_REVEALED,

	SIG_LAST
};

GdkRGBA paintbox_border = { 0.9, 1.0, 0.9, 1 };
GdkRGBA paintbox_shadow = { 0.2, 0.2, 0.2, 0.5 };

static void
paintbox_disconnect(Paintbox *paintbox)
{
	if (paintbox->drag_begin_sid) {
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->drag_begin_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->drag_update_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->drag_end_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->motion_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->enter_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->leave_sid);
		g_signal_handler_disconnect(paintbox->imageui,
			paintbox->key_pressed_sid);

		paintbox->drag_begin_sid = 0;
		paintbox->drag_update_sid = 0;
		paintbox->drag_end_sid = 0;
		paintbox->motion_sid = 0;
		paintbox->enter_sid = 0;
		paintbox->leave_sid = 0;
		paintbox->key_pressed_sid = 0;

		paintbox->imageui = NULL;
	}

	if (paintbox->snapshot_sid) {
		g_signal_handler_disconnect(paintbox->imagedisplay,
			paintbox->snapshot_sid);

		paintbox->imagedisplay = NULL;
	}
}

static void
paintbox_dispose(GObject *object)
{
	Paintbox *paintbox = (Paintbox *) object;

#ifdef DEBUG
	printf("paintbox_dispose:\n");
#endif /*DEBUG*/

	VIPS_UNREF(paintbox->mask);

	VIPS_FREEF(gtk_widget_unparent, paintbox->action_bar);

	G_OBJECT_CLASS(paintbox_parent_class)->dispose(object);
}

static void
paintbox_refresh(Paintbox *paintbox)
{
	GtkToggleButton *button =
		GTK_TOGGLE_BUTTON(paintbox->tools[paintbox->tool]);
	gtk_toggle_button_set_active(button, TRUE);

	// FIXME ... update undo/redo button sensitivity
}

static gboolean
paintbox_make_brush(Paintbox *paintbox)
{
	int size = rint(TSLIDER(paintbox->width)->value);

	VipsImage *mask;
	if (vips_mask_ideal(&mask, size, size, 1.0,
		"optical", TRUE,
		"reject", TRUE,
		"uchar", TRUE,
		NULL))
		return FALSE;

	VIPS_UNREF(paintbox->mask);
	paintbox->mask = mask;

	return TRUE;
}

static gboolean
paintbox_make_text(Paintbox *paintbox)
{
	g_autofree char *text =
		gtk_editable_get_chars(GTK_EDITABLE(paintbox->text_string), 0, -1);
	PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc(
		GTK_FONT_DIALOG_BUTTON(paintbox->font));
	g_autofree char *font = pango_font_description_to_string(desc);

	if (text &&
		strlen(text) > 0) {
		VipsImage *mask;
		if (vips_text(&mask, text, "font", font, NULL))
			return FALSE;

		VIPS_UNREF(paintbox->mask);
		paintbox->mask = mask;
	}

	return TRUE;
}

static void
paintbox_set_rubber(Paintbox *paintbox, PaintboxRubber rubber,
	int x0, int y0, int x1, int y1, int a, int b)
{
	paintbox->rubber = rubber;
	paintbox->hide = FALSE;
	paintbox->x0 = x0;
	paintbox->y0 = y0;
	paintbox->x1 = x1;
	paintbox->y1 = y1;
	paintbox->a = a;
	paintbox->b = b;

	gtk_widget_queue_draw(paintbox->imagedisplay);
}

static void
paintbox_rubber_clear(Paintbox *paintbox)
{
	paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_NONE, 0, 0, 0, 0, 0, 0);
}

static void
paintbox_set_tool(Paintbox *paintbox, PaintboxTool tool)
{
	paintbox_rubber_clear(paintbox);

	if (paintbox->tool != tool) {
		if (tool != PAINTBOX_TOOL_POINTER) {
			Imageui *imageui = imagewindow_get_imageui(paintbox->win);

			if (!imageui_make_paintable(imageui)) {
				imagewindow_error(paintbox->win);
				return;
			}
		}

		paintbox->tool = tool;
		paintbox_refresh(paintbox);
	}
}

static gboolean
paintbox_drag_begin(Imageui *imageui,
	gdouble start_x, gdouble start_y, GtkGestureDrag *drag, gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

	paintbox->start_x = start_x;
	paintbox->start_y = start_y;

	double image_x, image_y;
	imageui_gtk_to_image(imageui, start_x, start_y, &image_x, &image_y);

#ifdef DEBUG_VERBOSE
	printf("paintbox_drag_begin: start_x = %g, start_y = %g\n",
		start_x, start_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->tool != PAINTBOX_TOOL_POINTER &&
		paintbox->state == PAINTBOX_STATE_WAIT) {
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_BRUSH:
			paintbox->last_x = rint(image_x);
			paintbox->last_y = rint(image_y);
			paintbox_make_brush(paintbox);
			break;

		case PAINTBOX_TOOL_LINE:
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_LINE,
				rint(image_x), rint(image_y),
				rint(image_x), rint(image_y),
				0, 0);
			paintbox->last_x = rint(image_x);
			paintbox->last_y = rint(image_y);
			break;

		case PAINTBOX_TOOL_RECT:
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_RECT,
				rint(image_x), rint(image_y),
				rint(image_x), rint(image_y),
				0, 0);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_CIRCLE,
				rint(image_x), rint(image_y),
				0, 0,
				1, 0);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox->last_x = rint(image_x);
			paintbox->last_y = rint(image_y);
			break;

		case PAINTBOX_TOOL_TEXT:
			if (paintbox_make_text(paintbox) &&
				paintbox->mask)
				paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_BOX,
					rint(image_x), rint(image_y),
					0, 0,
					paintbox->mask->Xsize, paintbox->mask->Ysize);
			break;

		default:
			break;
		}

		handled = TRUE;
		paintbox->state = PAINTBOX_STATE_DRAG;
	}

	return handled;
}

static void
paintbox_update_brush_draw(Paintbox *paintbox, int x, int y)
{
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	const GdkRGBA *rgba =
		gtk_color_dialog_button_get_rgba(
			GTK_COLOR_DIALOG_BUTTON(paintbox->ink));
	double rgb[3] = {
		rgba->red * 255.0,
		rgba->green * 255.0,
		rgba->blue * 255.0,
	};

	if (tilesource &&
		paintbox->mask)
		tilesource_draw_line(tilesource, rgb, 3, paintbox->mask,
			paintbox->last_x, paintbox->last_y, x, y);

	paintbox->last_x = x;
	paintbox->last_y = y;
}

static void
paintbox_update_smudge_draw(Paintbox *paintbox, int x, int y)
{
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	int width = rint(TSLIDER(paintbox->width)->value);

	if (tilesource)
		tilesource_draw_smudge(tilesource, width,
			paintbox->last_x, paintbox->last_y, x, y);

	paintbox->last_x = x;
	paintbox->last_y = y;
}

static gboolean
paintbox_drag_update(Imageui *imageui,
	gdouble offset_x, gdouble offset_y, GtkGestureDrag *drag,
	gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

	double gtk_x = paintbox->start_x + offset_x;
	double gtk_y = paintbox->start_y + offset_y;
	double image_x, image_y;
	imageui_gtk_to_image(imageui, gtk_x, gtk_y, &image_x, &image_y);

#ifdef DEBUG_VERBOSE
	printf("paintbox_drag_update: offset_x = %g, offset_y = %g\n",
		offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->state == PAINTBOX_STATE_DRAG) {
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_BRUSH:
			paintbox_update_brush_draw(paintbox, image_x, image_y);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox_update_smudge_draw(paintbox, image_x, image_y);
			break;

		default:
			break;
		}

		handled = TRUE;
	}

	return handled;
}

#ifdef NIP4
static void
paintbox_update_model(Paintbox *paintbox)
{
	Tilesource *tilesource = imageui_get_tilesource(paintbox->imageui);
	iImage *iimage = imagewindow_get_iimage(paintbox->win);

	VipsImage *image;
	if ((image = tilesource_get_base_image(tilesource)) &&
		iimage->value.ii->image != image) {

		// will be removed on next GC, unless someone takes ownership
		g_object_ref(image);
		Imageinfo *new_ii = imageinfo_new(main_imageinfogroup,
			reduce_context->heap, image, NULL);
		image_value_set(&iimage->value, new_ii);

		// set modified, edited, etc.
		classmodel_update_view(CLASSMODEL(iimage));
	}

	Row *row = HEAPMODEL(iimage)->row;
	(void) expr_dirty(row->expr, link_serial_new());
	symbol_recalculate_all();
}
#endif /*NIP4*/

static gboolean
paintbox_drag_end(Imageui *imageui,
	gdouble offset_x, gdouble offset_y, GtkGestureDrag *drag,
	gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	double gtk_x = paintbox->start_x + offset_x;
	double gtk_y = paintbox->start_y + offset_y;
	double image_x, image_y;
	imageui_gtk_to_image(imageui, gtk_x, gtk_y, &image_x, &image_y);

	const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba(
			GTK_COLOR_DIALOG_BUTTON(paintbox->ink));
	double rgb[3] = {
		rgba->red * 255.0,
		rgba->green * 255.0,
		rgba->blue * 255.0,
	};

	gboolean fill =
		gtk_check_button_get_active(GTK_CHECK_BUTTON(paintbox->fill));

#ifdef DEBUG_VERBOSE
	printf("paintbox_drag_end: offset_x = %g, offset_y = %g\n",
		offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->state == PAINTBOX_STATE_DRAG) {
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_BRUSH:
			paintbox_update_brush_draw(paintbox, image_x, image_y);
			break;

		case PAINTBOX_TOOL_LINE:
			paintbox_make_brush(paintbox);
			paintbox_update_brush_draw(paintbox, image_x, image_y);
			break;

		case PAINTBOX_TOOL_RECT:
			if (tilesource)
				tilesource_draw_rect(tilesource,
					rgb, 3, fill,
					paintbox->x0, paintbox->y0,
					paintbox->x1 - paintbox->x0, paintbox->y1 - paintbox->y0);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			if (tilesource)
				tilesource_draw_circle(tilesource,
					rgb, 3, fill, paintbox->x0, paintbox->y0, paintbox->a);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox_update_smudge_draw(paintbox, image_x, image_y);
			break;

		case PAINTBOX_TOOL_FLOOD_UNTIL:
			if (tilesource)
				tilesource_draw_flood(tilesource,
					rgb, 3, FALSE, image_x, image_y);
			break;

		case PAINTBOX_TOOL_FLOOD_WHILE:
			if (tilesource)
				tilesource_draw_flood(tilesource,
					rgb, 3, TRUE, image_x, image_y);
			break;

		case PAINTBOX_TOOL_TEXT:
			if (tilesource &&
				paintbox->mask)
				tilesource_draw_mask(tilesource,
					rgb, 3, paintbox->mask, image_x, image_y);
			break;

		default:
			break;
		}

		handled = TRUE;
		paintbox_rubber_clear(paintbox);
		paintbox->state = PAINTBOX_STATE_WAIT;

#ifdef NIP4
		paintbox_update_model(paintbox);
#endif /*NIP4*/
	}

	return handled;
}

static gboolean
paintbox_motion(Imageui *imageui,
	gdouble gtk_x, gdouble gtk_y, GtkEventControllerMotion *motion,
	gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

	double image_x, image_y;
	imageui_gtk_to_image(imageui, gtk_x, gtk_y, &image_x, &image_y);

#ifdef DEBUG_VERBOSE
	printf("paintbox_motion: image_x = %g, image_y = %g\n",
		image_x, image_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->state == PAINTBOX_STATE_WAIT)
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_SMUDGE:
		case PAINTBOX_TOOL_BRUSH:
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_CIRCLE,
				rint(image_x), rint(image_y), 0, 0,
				rint(TSLIDER(paintbox->width)->value) / 2, 0);
			break;

		default:
			break;
		}
	else
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_SMUDGE:
		case PAINTBOX_TOOL_BRUSH:
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_CIRCLE,
				rint(image_x), rint(image_y), 0, 0,
				rint(TSLIDER(paintbox->width)->value) / 2, 0);
			break;

		case PAINTBOX_TOOL_LINE:
			paintbox->x1 = rint(image_x);
			paintbox->y1 = rint(image_y);
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_RECT:
			paintbox->x1 = rint(image_x);
			paintbox->y1 = rint(image_y);
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			double dx = paintbox->x0 - image_x;
			double dy = paintbox->y0 - image_y;
			paintbox->a = rint(sqrt(dx * dx + dy * dy));
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_TEXT:
			paintbox->x0 = rint(image_x);
			paintbox->y0 = rint(image_y);
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		default:
			break;
		}

	return handled;
}

static void
paintbox_enter(Imageui *imageui, gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

	paintbox->hide = FALSE;
	gtk_widget_queue_draw(paintbox->imagedisplay);
}

static void
paintbox_leave(Imageui *imageui, gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

	paintbox->hide = TRUE;
	gtk_widget_queue_draw(paintbox->imagedisplay);
}

static gboolean
paintbox_key_pressed(Imageui *imageui,
	guint keyval, guint keycode, GdkModifierType state,
	GtkEventControllerKey *key, gpointer user_data)
{
	Paintbox *paintbox = PAINTBOX(user_data);

#ifdef DEBUG_VERBOSE
	printf("paintbox_key_pressed_real: keyval = %d, state = %d\n",
		keyval, state);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (keyval == GDK_KEY_Escape)
		switch (paintbox->state) {
		case PAINTBOX_STATE_DRAG:
			switch (paintbox->tool) {
			case PAINTBOX_TOOL_LINE:
			case PAINTBOX_TOOL_RECT:
			case PAINTBOX_TOOL_CIRCLE:
			case PAINTBOX_TOOL_FLOOD_UNTIL:
			case PAINTBOX_TOOL_FLOOD_WHILE:
			case PAINTBOX_TOOL_TEXT:
				handled = TRUE;
				paintbox->state = PAINTBOX_STATE_WAIT;
				paintbox_rubber_clear(paintbox);
				break;

			default:
				break;
			}

			break;

		default:
			break;
		}

	return handled;
}

static void
paintbox_stroke_rubber(Paintbox *paintbox, GtkSnapshot *snapshot, GskPath *path)
{
	GskStroke *stroke;

	stroke = gsk_stroke_new(3);
	gsk_stroke_set_dash(stroke, (float[1]){ 10 }, 1);
	gtk_snapshot_append_stroke(snapshot, path, stroke, &paintbox_border);
	gsk_stroke_free(stroke);

	stroke = gsk_stroke_new(3);
	gsk_stroke_set_dash(stroke, (float[1]){ 10 }, 1);
	gsk_stroke_set_dash_offset(stroke, 10);
	gtk_snapshot_append_stroke(snapshot, path, stroke, &paintbox_shadow);
	gsk_stroke_free(stroke);
}

static void
paintbox_snapshot(Imagedisplay *imagedisplay,
	GtkSnapshot *snapshot, Paintbox *paintbox)
{
	Imageui *imageui = paintbox->imageui;

	if (paintbox->hide)
		return;

	double x0_gtk, y0_gtk;
	imageui_image_to_gtk(imageui, paintbox->x0 + 0.5, paintbox->y0 + 0.5,
		&x0_gtk, &y0_gtk);

	double x1_gtk, y1_gtk;
	imageui_image_to_gtk(imageui, paintbox->x1 + 0.5, paintbox->y1 + 0.5,
		&x1_gtk, &y1_gtk);

	double scale = imagedisplay_get_scale(imagedisplay);
	int a = paintbox->a * scale;
	int b = paintbox->b * scale;

	switch (paintbox->rubber) {
	case PAINTBOX_RUBBER_LINE:
		{
			GskPathBuilder *builder = gsk_path_builder_new();
			gsk_path_builder_move_to(builder, x0_gtk, y0_gtk);
			gsk_path_builder_line_to(builder, x1_gtk, y1_gtk);
			g_autoptr(GskPath) path = gsk_path_builder_free_to_path(builder);

			paintbox_stroke_rubber(paintbox, snapshot, path);
		}
		break;

	case PAINTBOX_RUBBER_CIRCLE:
		{
			GskPathBuilder *builder = gsk_path_builder_new();
			graphene_point_t center = GRAPHENE_POINT_INIT(x0_gtk, y0_gtk);
			gsk_path_builder_add_circle(builder, &center, a);
			g_autoptr(GskPath) path = gsk_path_builder_free_to_path(builder);

			paintbox_stroke_rubber(paintbox, snapshot, path);
		}
		break;

	case PAINTBOX_RUBBER_RECT:
		{
			GskPathBuilder *builder = gsk_path_builder_new();
			gsk_path_builder_move_to(builder, x0_gtk, y0_gtk);
			gsk_path_builder_line_to(builder, x1_gtk, y0_gtk);
			gsk_path_builder_line_to(builder, x1_gtk, y1_gtk);
			gsk_path_builder_line_to(builder, x0_gtk, y1_gtk);
			gsk_path_builder_line_to(builder, x0_gtk, y0_gtk);
			g_autoptr(GskPath) path = gsk_path_builder_free_to_path(builder);

			paintbox_stroke_rubber(paintbox, snapshot, path);
		}
		break;

	case PAINTBOX_RUBBER_BOX:
		{
			GskPathBuilder *builder = gsk_path_builder_new();
			gsk_path_builder_move_to(builder, x0_gtk, y0_gtk);
			gsk_path_builder_line_to(builder, x0_gtk + a, y0_gtk);
			gsk_path_builder_line_to(builder, x0_gtk + a, y0_gtk + b);
			gsk_path_builder_line_to(builder, x0_gtk, y0_gtk + b);
			gsk_path_builder_line_to(builder, x0_gtk, y0_gtk);
			g_autoptr(GskPath) path = gsk_path_builder_free_to_path(builder);

			paintbox_stroke_rubber(paintbox, snapshot, path);
		}
		break;

	default:
		break;
	}
}

// win->imageui has changed
static void
paintbox_imagewindow_new_image(Imagewindow *win, Paintbox *paintbox)
{
#ifdef DEBUG
	printf("paintbox_imagewindow_new_image:\n");
#endif /*DEBUG*/

	paintbox_disconnect(paintbox);

	paintbox->imageui = imagewindow_get_imageui(win);
	paintbox->drag_begin_sid = g_signal_connect(paintbox->imageui,
		"drag-begin", G_CALLBACK(paintbox_drag_begin), paintbox);
	paintbox->drag_update_sid = g_signal_connect(paintbox->imageui,
		"drag-update", G_CALLBACK(paintbox_drag_update), paintbox);
	paintbox->drag_end_sid = g_signal_connect(paintbox->imageui,
		"drag-end", G_CALLBACK(paintbox_drag_end), paintbox);
	paintbox->motion_sid = g_signal_connect(paintbox->imageui,
		"motion", G_CALLBACK(paintbox_motion), paintbox);
	paintbox->enter_sid = g_signal_connect(paintbox->imageui,
		"enter", G_CALLBACK(paintbox_enter), paintbox);
	paintbox->leave_sid = g_signal_connect(paintbox->imageui,
		"leave", G_CALLBACK(paintbox_leave), paintbox);
	paintbox->key_pressed_sid = g_signal_connect(paintbox->imageui,
		"key_pressed", G_CALLBACK(paintbox_key_pressed), paintbox);

	paintbox->imagedisplay = imageui_get_imagedisplay(paintbox->imageui);
	paintbox->snapshot_sid = g_signal_connect(paintbox->imagedisplay,
		"snapshot", G_CALLBACK(paintbox_snapshot), paintbox);

	// reset tool to SELECT, since the new imageui might not be paintable
	paintbox_set_tool(paintbox, PAINTBOX_TOOL_POINTER);
}

static void
paintbox_set_imagewindow(Paintbox *paintbox, Imagewindow *win)
{
	// only support set once
	g_assert(!paintbox->win);

	/* No need to ref ... win holds a ref to us.
	 */
	paintbox->win = win;

	g_signal_connect_object(win, "new-image",
		G_CALLBACK(paintbox_imagewindow_new_image), paintbox, 0);
}

static void
paintbox_set_property(GObject *object,
	guint prop_id, const GValue *value, GParamSpec *pspec)
{
	Paintbox *paintbox = (Paintbox *) object;

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		paintbox_set_imagewindow(paintbox,
			IMAGEWINDOW(g_value_get_object(value)));
		break;

	case PROP_REVEALED:
		gtk_action_bar_set_revealed(GTK_ACTION_BAR(paintbox->action_bar),
			g_value_get_boolean(value));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
paintbox_get_property(GObject *object,
	guint prop_id, GValue *value, GParamSpec *pspec)
{
	Paintbox *paintbox = (Paintbox *) object;
	GtkActionBar *action_bar = GTK_ACTION_BAR(paintbox->action_bar);

	switch (prop_id) {
	case PROP_IMAGEWINDOW:
		g_value_set_object(value, paintbox->win);
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
paintbox_notify_revealed(GtkWidget *widget,
	GParamSpec *pspec, Paintbox *paintbox)
{
#ifdef DEBUG
	printf("paintbox_notify_revealed:\n");
#endif /*DEBUG*/

	/* Turn off the paintbox when it's hidden.
	 */
	if (!gtk_action_bar_get_revealed(GTK_ACTION_BAR(widget)))
		paintbox_set_tool(paintbox, PAINTBOX_TOOL_POINTER);
}

static void
paintbox_init(Paintbox *paintbox)
{
#ifdef DEBUG
	printf("paintbox_init:\n");
#endif /*DEBUG*/

	gtk_widget_init_template(GTK_WIDGET(paintbox));

	paintbox->tools[PAINTBOX_TOOL_POINTER] = paintbox->pointer;
	paintbox->tools[PAINTBOX_TOOL_BRUSH] = paintbox->brush;
	paintbox->tools[PAINTBOX_TOOL_LINE] = paintbox->line;
	paintbox->tools[PAINTBOX_TOOL_RECT] = paintbox->rect;
	paintbox->tools[PAINTBOX_TOOL_CIRCLE] = paintbox->circle;
	paintbox->tools[PAINTBOX_TOOL_SMUDGE] = paintbox->smudge;
	paintbox->tools[PAINTBOX_TOOL_FLOOD_WHILE] = paintbox->flood_while;
	paintbox->tools[PAINTBOX_TOOL_FLOOD_UNTIL] = paintbox->flood_until;
	paintbox->tools[PAINTBOX_TOOL_TEXT] = paintbox->text;
	paintbox->tools[PAINTBOX_TOOL_DROPPER] = paintbox->dropper;

	Tslider *width = TSLIDER(paintbox->width);
	width->from = 0;
	width->to = 100;
	width->value = 5;
	width->digits = 1;
	tslider_changed(width);

	g_signal_connect(paintbox->action_bar, "notify::revealed",
		G_CALLBACK(paintbox_notify_revealed), paintbox);

	paintbox_refresh(paintbox);
}

static void
paintbox_toggled(GtkToggleButton *button, Paintbox *paintbox)
{
	const char *id = gtk_buildable_get_buildable_id(GTK_BUILDABLE(button));

#ifdef DEBUG
	printf("paintbox_toggled: id = %s\n", id);
#endif /*DEBUG*/

	if (gtk_toggle_button_get_active(button)) {
		int value =
			vips_enum_from_nick("paintbox_toggled", PAINTBOX_TOOL_TYPE, id);

		if (value < 0) {
			printf("paintbox_toggled: unknown id\n");
			return;
		}

		paintbox_set_tool(paintbox, value);
	}
}

static void
paintbox_class_init(PaintboxClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

#ifdef DEBUG
	printf("paintbox_class_init:\n");
#endif /*DEBUG*/

	BIND_RESOURCE("paintbox.ui");
	BIND_LAYOUT();

	BIND_VARIABLE(Paintbox, action_bar);
	BIND_VARIABLE(Paintbox, pointer);
	BIND_VARIABLE(Paintbox, brush);
	BIND_VARIABLE(Paintbox, line);
	BIND_VARIABLE(Paintbox, rect);
	BIND_VARIABLE(Paintbox, circle);
	BIND_VARIABLE(Paintbox, smudge);
	BIND_VARIABLE(Paintbox, flood_while);
	BIND_VARIABLE(Paintbox, flood_until);
	BIND_VARIABLE(Paintbox, text);
	BIND_VARIABLE(Paintbox, dropper);
	BIND_VARIABLE(Paintbox, ink);
	BIND_VARIABLE(Paintbox, fill);
	BIND_VARIABLE(Paintbox, width);
	BIND_VARIABLE(Paintbox, font);
	BIND_VARIABLE(Paintbox, text_string);

	BIND_CALLBACK(paintbox_toggled);

	gobject_class->dispose = paintbox_dispose;
	gobject_class->set_property = paintbox_set_property;
	gobject_class->get_property = paintbox_get_property;

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

Paintbox *
paintbox_new(Imagewindow *win)
{
	Paintbox *paintbox;

#ifdef DEBUG
	printf("paintbox_new:\n");
#endif /*DEBUG*/

	paintbox = g_object_new(paintbox_get_type(),
		"image-window", win,
		NULL);

	return paintbox;
}
