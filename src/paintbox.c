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

/* A fragment of an undo buffer.
 */
typedef struct _Undofragment {
	struct _Undobuffer *undo;		/* Main undo area */
	VipsImage *saved;				/* Saved pixels */
	VipsRect position;				/* Where we took it from */
} Undofragment;

/* Hold a list of the above, a bounding box for this list, and a link back to
 * the main imageinfo.
 */
typedef struct _Undobuffer {
    Paintbox *paintbox;
    GSList *frags;					/* List of paint fragments */
    VipsRect bounds;				/* Bounding box for frags */
} Undobuffer;

struct _Paintbox {
	GtkWidget parent_instance;

	/* The imagewindow we are on.
	 */
	Imagewindow *win;

	/* The imageui we are drawing on, and the signals we are watching.
	 */
	Imageui *imageui;
	GtkWidget *imagedisplay;
	guint snapshot_sid;

	/* Undo/redo buffers.
     */
    GSList *undo;					/* List of undo buffers */
    GSList *redo;					/* List of redo buffers */
    Undobuffer *current_undo;		/* Current buffer */

	/* Currently selected tool.
	 */
	PaintboxTool tool;

	/* State machine.
	 */
	PaintboxState state;

	/* Start of drag.
	 */
	double start_x;
	double start_y;

	/* Last position for line draw.
	 */
	int last_x;
	int last_y;

	/* The selected ink colour converted to a double* that matches the number
	 * of bands in the image.
	 */
	double *dink;
	int n_dink;

	/* Mask and for drawing.
	 */
	VipsImage *mask;

	/* Last measured distance from top of logical rect to baseline and top
	 * topline (top of eg. "o").
	 */
	int baseline;
	int topline;

	/* Widgets.
	 */
	GtkWidget *action_bar;
	GtkWidget *undo_widget;
	GtkWidget *redo_widget;
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

static const GdkRGBA paintbox_border = { 0.9, 1.0, 0.9, 1 };
static const GdkRGBA paintbox_shadow = { 0.2, 0.2, 0.2, 0.5 };
static const int paintbox_max_undo = 10;

/* Free up an undo fragment.
 */
static void
paintbox_undofragment_free(void *data)
{
	Undofragment *frag = (Undofragment *) data;

    VIPS_UNREF(frag->saved);
    VIPS_FREE(frag);
}

/* Free an undo buffer.
 */
static void
paintbox_undobuffer_free(void *data)
{
	Undobuffer *undo = (Undobuffer *) data;

	g_slist_free_full(g_steal_pointer(&undo->frags),
		paintbox_undofragment_free);
    VIPS_FREE(undo);
}

static void
paintbox_undo_free(Paintbox *paintbox)
{
	g_slist_free_full(g_steal_pointer(&paintbox->undo),
		paintbox_undobuffer_free);
	g_slist_free_full(g_steal_pointer(&paintbox->redo),
		paintbox_undobuffer_free);

    VIPS_FREEF(paintbox_undobuffer_free, paintbox->current_undo);
}

static void
paintbox_disconnect(Paintbox *paintbox)
{
	if (paintbox->snapshot_sid) {
		g_signal_handler_disconnect(paintbox->imagedisplay,
			paintbox->snapshot_sid);

		paintbox->imagedisplay = NULL;
	}

	if (paintbox->imageui) {
		imageui_client_remove(paintbox->imageui, G_OBJECT(paintbox));
		paintbox->imageui = NULL;
	}
}

static void
paintbox_dispose(GObject *object)
{
	Paintbox *paintbox = (Paintbox *) object;

#ifdef DEBUG
	printf("paintbox_dispose:\n");
#endif /*DEBUG*/

	paintbox_undo_free(paintbox);
	VIPS_UNREF(paintbox->mask);
	VIPS_FREEF(gtk_widget_unparent, paintbox->action_bar);
	VIPS_FREE(paintbox->dink);

	G_OBJECT_CLASS(paintbox_parent_class)->dispose(object);
}

static void
paintbox_refresh(Paintbox *paintbox)
{
	GtkToggleButton *button =
		GTK_TOGGLE_BUTTON(paintbox->tools[paintbox->tool]);
	gtk_toggle_button_set_active(button, TRUE);

	gtk_widget_set_sensitive(paintbox->undo_widget, !!paintbox->undo);
	gtk_widget_set_sensitive(paintbox->redo_widget, !!paintbox->redo);
}

static gboolean
paintbox_make_brush(Paintbox *paintbox)
{
	int size = rint(TSLIDER(paintbox->width)->value);

	VIPS_UNREF(paintbox->mask);

	VipsImage *mask;
	if (vips_mask_ideal(&mask, size, size, 1.0,
		"optical", TRUE,
		"reject", TRUE,
		"uchar", TRUE,
		NULL))
		return FALSE;

	paintbox->mask = mask;

	return TRUE;
}

static gboolean
paintbox_make_text(Paintbox *paintbox, const char *text)
{
	PangoFontDescription *desc = gtk_font_dialog_button_get_font_desc(
		GTK_FONT_DIALOG_BUTTON(paintbox->font));
	g_autofree char *font = pango_font_description_to_string(desc);

	VIPS_UNREF(paintbox->mask);

	// render "o" to get the height of a char with no ascenders and no
	// descenders
	VipsImage *o;
	if (vips_text(&o, "o", "font", font, NULL))
		return FALSE;
	// therefore distance from top of logical rect to baseline
	paintbox->baseline = o->Ysize + o->Yoffset;
	paintbox->topline = o->Yoffset;
	VIPS_UNREF(o);

	VipsImage *mask;
	if (vips_text(&mask, text, "font", font, NULL))
		return FALSE;

	paintbox->mask = mask;

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

static void
paintbox_snap_brush(Paintbox *paintbox,
	int x, int y, int r, int *new_x, int *new_y)
{
	VipsRect brush = {x, y, 0, 0};
	vips_rect_marginadjust(&brush, r);
	VipsRect new_brush;
	if (imageui_snap_rect(paintbox->imageui, &brush, &new_brush)) {
		*new_x = new_brush.left + r;
		*new_y = new_brush.top + r;
	}
	else {
		*new_x = x;
		*new_y = y;
	}
}

static gboolean
paintbox_drag_begin(Paintbox *paintbox,
	gdouble start_x, gdouble start_y, GdkModifierType modifiers)
{
	Imageui *imageui = paintbox->imageui;

	/* Don't handle shift- or ctrl-drag.
	 */
	if (modifiers & GDK_CONTROL_MASK ||
		modifiers & GDK_SHIFT_MASK)
		return FALSE;

	paintbox->start_x = start_x;
	paintbox->start_y = start_y;

	double image_x, image_y;
	imageui_gtk_to_image(imageui, start_x, start_y, &image_x, &image_y);
	int x = rint(image_x);
	int y = rint(image_y);

	int radius = rint(TSLIDER(paintbox->width)->value / 2);

#ifdef DEBUG_VERBOSE
	printf("paintbox_drag_begin: start_x = %g, start_y = %g\n",
		start_x, start_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->tool != PAINTBOX_TOOL_POINTER &&
		paintbox->state == PAINTBOX_STATE_WAIT) {
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_BRUSH:
			paintbox_snap_brush(paintbox, x, y, radius, &x, &y);
			paintbox->last_x = x;
			paintbox->last_y = y;
			paintbox_make_brush(paintbox);
			break;

		case PAINTBOX_TOOL_LINE:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->last_x = x;
			paintbox->last_y = y;

			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_LINE,
				x, y,
				x, y,
				0, 0);
			break;

		case PAINTBOX_TOOL_RECT:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->last_x = x;
			paintbox->last_y = y;

			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_RECT,
				x, y,
				x, y,
				0, 0);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->last_x = x;
			paintbox->last_y = y;

			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_CIRCLE,
				x, y,
				0, 0,
				1, 0);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox_snap_brush(paintbox, x, y, radius, &x, &y);
			paintbox->last_x = x;
			paintbox->last_y = y;
			break;

		case PAINTBOX_TOOL_FLOOD_UNTIL:
		case PAINTBOX_TOOL_FLOOD_WHILE:
		case PAINTBOX_TOOL_DROPPER:
			// just note the start point in case there's a single click and no
			// motion
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_NONE,
				x, y,
				0, 0,
				0, 0);
			break;

		case PAINTBOX_TOOL_TEXT:
			g_autofree char *text =
				gtk_editable_get_chars(GTK_EDITABLE(paintbox->text_string),
					0, -1);

			if (!text ||
				strlen(text) == 0) {
				vips_error("Paintbox", "%s", _("empty string"));
				imagewindow_error(paintbox->win);
				return TRUE;
			}

			// the snap box is positioned with no ascenders and no
			// descenders
			paintbox_make_text(paintbox, text);

			VipsRect rect = {
				.left = x,
				.top = y,
				.width = paintbox->mask->Xsize,
				.height = paintbox->baseline - paintbox->topline,
			};
			imageui_snap_rect(paintbox->imageui, &rect, &rect);

			paintbox_set_rubber(paintbox, PAINTBOX_RUBBER_BOX,
				rect.left,
				rect.top,
				0, 0,
				paintbox->mask->Xsize,
				paintbox->baseline - paintbox->topline);

			break;

		default:
			break;
		}

		handled = TRUE;
		paintbox->state = PAINTBOX_STATE_DRAG;
	}

	return handled;
}

static Undofragment *
paintbox_undofragment_new(Undobuffer *undo)
{
    Undofragment *frag = VIPS_NEW(NULL, Undofragment);

    frag->undo = undo;

    return frag;
}

static Undobuffer *
paintbox_undobuffer_new(Paintbox *paintbox)
{
    Undobuffer *undo = VIPS_NEW(NULL, Undobuffer);

    undo->paintbox = paintbox;

    return undo;
}

/* Grab into an undo fragment. Add frag to frag list on undo buffer, expand
 * bounding box.
 */
static Undofragment *
paintbox_undo_grab(Undobuffer *undo, VipsRect *position)
{
    Paintbox *paintbox = undo->paintbox;
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);
    Undofragment *frag = paintbox_undofragment_new(undo);

    if (!(frag->saved = tilesource_draw_copy(tilesource, position))) {
        paintbox_undofragment_free(frag);
		error_vips_all();
        return NULL;
    }

    frag->position = *position;
    undo->frags = g_slist_prepend(undo->frags, frag);
    vips_rect_unionrect(position, &undo->bounds, &undo->bounds);

    return frag;
}

/* Trim the undo/redo buffers if we have more than x items on it.
 */
static void
paintbox_undo_trim(Paintbox *paintbox)
{
    int len = g_slist_length(paintbox->undo);
    if (len > paintbox_max_undo) {
        GSList *l = g_slist_reverse(paintbox->undo);

        for (int i = 0; i < len - paintbox_max_undo; i++) {
            Undobuffer *undo = (Undobuffer *) l->data;

            paintbox_undobuffer_free(undo);
            l = g_slist_remove(l, undo);
        }

        paintbox->undo = g_slist_reverse(l);
    }
}

/* Mark the start or end of an undo session. Copy current undo information
 * to the undo buffers and NULL out the current undo pointer.
 *
 * Junk all redo information: this new undo action makes all that out of date.
 */
void
paintbox_undo_mark(Paintbox *paintbox)
{
    if (paintbox->current_undo) {
        /* Left over from the last undo save. Copy to undo save list
         * and get ready for new undo buffer.
         */
        paintbox->undo =
			g_slist_prepend(paintbox->undo, paintbox->current_undo);
        paintbox->current_undo = NULL;
    }

    /* Junk all redo information, it must be out of date.
     */
    slist_map(paintbox->redo, (SListMapFn) paintbox_undobuffer_free, NULL);
    VIPS_FREEF(g_slist_free, paintbox->redo);

    paintbox_undo_trim(paintbox);

    paintbox_refresh(paintbox);
}

/* Add some pixels to the current undo buffer.
 */
static gboolean
paintbox_undo_add(Paintbox *paintbox, VipsRect *position)
{
	Undobuffer *undo = paintbox->current_undo;

    if (!undo) {
        paintbox->current_undo = undo = paintbox_undobuffer_new(paintbox);

        return paintbox_undo_grab(undo, position) != NULL;
    }

	/* Do we need to expand our saved area to the right?
     */
    if (VIPS_RECT_RIGHT(position) > VIPS_RECT_RIGHT(&undo->bounds)) {
		VipsRect over = {
			.left = VIPS_RECT_RIGHT(&undo->bounds),
			.top = undo->bounds.top,
			.width = VIPS_RECT_RIGHT(position) - VIPS_RECT_RIGHT(&undo->bounds),
			.height = undo->bounds.height,
		};

        if (!paintbox_undo_grab(undo, &over))
            return FALSE;
    }

    /* Left?
     */
    if (undo->bounds.left > position->left) {
		VipsRect over = {
			.left = position->left,
			.top = undo->bounds.top,
			.width = undo->bounds.left - position->left,
			.height = undo->bounds.height,
		};

        if (!paintbox_undo_grab(undo, &over))
            return FALSE;
    }

    /* Up?
     */
    if (undo->bounds.top > position->top) {
		VipsRect over = {
			.left = undo->bounds.left,
			.top = position->top,
			.width = undo->bounds.width,
			.height = undo->bounds.top - position->top,
		};

        if (!paintbox_undo_grab(undo, &over))
            return FALSE;
    }

    /* Down?
     */
    if (VIPS_RECT_BOTTOM(position) > VIPS_RECT_BOTTOM(&undo->bounds)) {
		VipsRect over = {
			.left = undo->bounds.left,
			.top = VIPS_RECT_BOTTOM(&undo->bounds),
			.width = undo->bounds.width,
			.height = VIPS_RECT_BOTTOM(position) -
				VIPS_RECT_BOTTOM(&undo->bounds)
		};

        if (!paintbox_undo_grab(undo, &over))
            return FALSE;
    }

    return TRUE;
}

/* Paste an undo fragment back into the image.
 */
static void *
paintbox_undofragment_paste(Undofragment *frag)
{
    Undobuffer *undo = frag->undo;
	Paintbox *paintbox = undo->paintbox;
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	tilesource_draw_paste(tilesource, frag->saved, &frag->position);

    return NULL;
}

/* Paste a whole undo buffer back into the image.
 */
static void
paintbox_undobuffer_paste(Undobuffer *undo)
{
    slist_map(undo->frags,
        (SListMapFn) paintbox_undofragment_paste, NULL);
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

/* Undo a paint action.
 */
gboolean
paintbox_undo(Paintbox *paintbox)
{
    Undobuffer *undo;

	/* Do nothing if we're not active.
	 */
	if (paintbox->tool == PAINTBOX_TOOL_POINTER)
        return TRUE;

    /* Find the undo action we are to perform.
     */
    if (!paintbox->undo)
        return TRUE;
    undo = (Undobuffer *) paintbox->undo->data;

    /* We are going to undo the first action on the undo list. We must
     * save the area under the first undo action to the redo list.
     */
    if (!paintbox_undo_add(paintbox, &undo->bounds))
        return FALSE;
    paintbox->redo = g_slist_prepend(paintbox->redo, paintbox->current_undo);
    paintbox->current_undo = NULL;

    /* Paint undo back.
     */
    paintbox_undobuffer_paste(undo);

    /* Junk the undo action we have performed.
     */
    paintbox->undo = g_slist_remove(paintbox->undo, undo);
    paintbox_undobuffer_free(undo);

    paintbox_undo_trim(paintbox);

    paintbox_refresh(paintbox);

#ifdef NIP4
	paintbox_update_model(paintbox);
#endif /*NIP4*/

    return TRUE;
}

/* Redo a paint action, if possible.
 */
gboolean
paintbox_redo(Paintbox *paintbox)
{
    Undobuffer *undo;

	/* Do nothing if we're not active.
	 */
	if (paintbox->tool == PAINTBOX_TOOL_POINTER)
        return TRUE;

    /* Find the redo action we are to perform.
     */
    if (!paintbox->redo)
        return TRUE;
    undo = (Undobuffer *) paintbox->redo->data;

    /* We are going to redo the first action on the redo list. We must
     * save the area under the first redo action to the undo list.
     */
    if (!paintbox_undo_add(paintbox, &undo->bounds))
        return FALSE;
    paintbox->undo = g_slist_prepend(paintbox->undo, paintbox->current_undo);
    paintbox->current_undo = NULL;

    paintbox_undobuffer_paste(undo);

    /* We can junk the head of the undo list now.
     */
    paintbox->redo = g_slist_remove(paintbox->redo, undo);
    paintbox_undobuffer_free(undo);

    paintbox_undo_trim(paintbox);

    paintbox_refresh(paintbox);

#ifdef NIP4
	paintbox_update_model(paintbox);
#endif /*NIP4*/

    return TRUE;
}

static int
paintbox_get_bands(Paintbox *paintbox)
{
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);
	VipsImage *image = tilesource_get_base_image(tilesource);

	// default to rgb if no image is loaded
	return image ? image->Bands : 3;
}

static void
paintbox_get_ink(Paintbox *paintbox)
{
	VIPS_FREE(paintbox->dink);

	paintbox->n_dink = paintbox_get_bands(paintbox);
	paintbox->dink = VIPS_ARRAY(NULL, paintbox->n_dink, double);

	const GdkRGBA *rgba =
		gtk_color_dialog_button_get_rgba(
			GTK_COLOR_DIALOG_BUTTON(paintbox->ink));
	double dink[4] = {
		rgba->red * 255.0,
		rgba->green * 255.0,
		rgba->blue * 255.0,
		rgba->alpha * 255.0,
	};

	for (int i = 0; i < VIPS_MIN(4, paintbox->n_dink); i++)
		paintbox->dink[i] = dink[i];
}

static void
paintbox_update_brush_draw(Paintbox *paintbox, int x, int y)
{
	Imageui *imageui = imagewindow_get_imageui(paintbox->win);
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	paintbox_get_ink(paintbox);

	if (tilesource &&
		paintbox->mask) {
		if (rint(TSLIDER(paintbox->width)->value < 2))
			tilesource_draw_line1(tilesource,
				paintbox->dink, paintbox->n_dink,
				paintbox->last_x, paintbox->last_y, x, y,
				(TilesourceSaveFn) paintbox_undo_add, paintbox);
		else
			tilesource_draw_line(tilesource,
				paintbox->dink, paintbox->n_dink,
				paintbox->mask,
				paintbox->last_x, paintbox->last_y, x, y,
				(TilesourceSaveFn) paintbox_undo_add, paintbox);
	}

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
			paintbox->last_x, paintbox->last_y, x, y,
			(TilesourceSaveFn) paintbox_undo_add, paintbox);

	paintbox->last_x = x;
	paintbox->last_y = y;
}

static gboolean
paintbox_drag_update(Paintbox *paintbox,
	gdouble offset_x, gdouble offset_y, GdkModifierType modifiers)
{
#ifdef DEBUG_VERBOSE
	printf("paintbox_drag_update: offset_x = %g, offset_y = %g\n",
		offset_x, offset_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->state == PAINTBOX_STATE_DRAG) {
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_BRUSH:
			paintbox_update_brush_draw(paintbox, paintbox->x0, paintbox->y0);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox_update_smudge_draw(paintbox, paintbox->x0, paintbox->y0);
			break;

		default:
			break;
		}

		handled = TRUE;
	}

	return handled;
}

static gboolean
paintbox_drag_end(Paintbox *paintbox,
	gdouble offset_x, gdouble offset_y, GdkModifierType modifiers)
{
	Imageui *imageui = paintbox->imageui;
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	paintbox_get_ink(paintbox);

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
			paintbox_update_brush_draw(paintbox, paintbox->x0, paintbox->y0);
			break;

		case PAINTBOX_TOOL_LINE:
			paintbox_make_brush(paintbox);
			paintbox_update_brush_draw(paintbox, paintbox->x1, paintbox->y1);
			break;

		case PAINTBOX_TOOL_RECT:
			if (tilesource)
				tilesource_draw_rect(tilesource,
					paintbox->dink, paintbox->n_dink,
					fill,
					paintbox->x0, paintbox->y0,
					paintbox->x1 - paintbox->x0, paintbox->y1 - paintbox->y0,
					(TilesourceSaveFn) paintbox_undo_add, paintbox);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			if (tilesource)
				tilesource_draw_circle(tilesource,
					paintbox->dink, paintbox->n_dink,
					fill, paintbox->x0, paintbox->y0, paintbox->a,
					(TilesourceSaveFn) paintbox_undo_add, paintbox);
			break;

		case PAINTBOX_TOOL_SMUDGE:
			paintbox_update_smudge_draw(paintbox, paintbox->x0, paintbox->y0);
			break;

		case PAINTBOX_TOOL_FLOOD_UNTIL:
			if (tilesource)
				tilesource_draw_flood(tilesource,
					paintbox->dink, paintbox->n_dink,
					FALSE, paintbox->x0, paintbox->y0,
					(TilesourceSaveFn) paintbox_undo_add, paintbox);
			break;

		case PAINTBOX_TOOL_FLOOD_WHILE:
			if (tilesource)
				tilesource_draw_flood(tilesource,
					paintbox->dink, paintbox->n_dink,
					TRUE, paintbox->x0, paintbox->y0,
					(TilesourceSaveFn) paintbox_undo_add, paintbox);
			break;

		case PAINTBOX_TOOL_TEXT:
			if (tilesource &&
				paintbox->mask)
				tilesource_draw_mask(tilesource,
					paintbox->dink, paintbox->n_dink,
					paintbox->mask,
					paintbox->x0,
					paintbox->y0 - paintbox->topline + paintbox->mask->Yoffset,
					(TilesourceSaveFn) paintbox_undo_add, paintbox);
			break;

		default:
			break;
		}

		handled = TRUE;
		paintbox_rubber_clear(paintbox);
		paintbox->state = PAINTBOX_STATE_WAIT;
		paintbox_undo_mark(paintbox);

#ifdef NIP4
		paintbox_update_model(paintbox);
#endif /*NIP4*/
	}

	return handled;
}

static gboolean
paintbox_motion(Paintbox *paintbox, gdouble gtk_x, gdouble gtk_y)
{
	Imageui *imageui = paintbox->imageui;
	int radius = rint(TSLIDER(paintbox->width)->value / 2);

	double image_x;
	double image_y;
	imageui_gtk_to_image(imageui, gtk_x, gtk_y, &image_x, &image_y);
	int x = rint(image_x);
	int y = rint(image_y);

#ifdef DEBUG_VERBOSE
	printf("paintbox_motion: image_x = %g, image_y = %g\n",
		image_x, image_y);
#endif /*DEBUG_VERBOSE*/

	gboolean handled = FALSE;

	if (paintbox->state == PAINTBOX_STATE_WAIT)
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_SMUDGE:
		case PAINTBOX_TOOL_BRUSH:
			paintbox_snap_brush(paintbox, x, y, radius, &x, &y);
			paintbox_set_rubber(paintbox,
				PAINTBOX_RUBBER_CIRCLE, x, y, 0, 0, radius, 0);
			break;

		default:
			break;
		}
	else
		switch (paintbox->tool) {
		case PAINTBOX_TOOL_SMUDGE:
		case PAINTBOX_TOOL_BRUSH:
			paintbox_snap_brush(paintbox, x, y, radius, &x, &y);
			paintbox_set_rubber(paintbox,
				PAINTBOX_RUBBER_CIRCLE, x, y, 0, 0, radius, 0);
			break;

		case PAINTBOX_TOOL_LINE:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->x1 = x;
			paintbox->y1 = y;
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_RECT:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->x1 = x;
			paintbox->y1 = y;
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_CIRCLE:
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			double dx = paintbox->x0 - x;
			double dy = paintbox->y0 - y;
			paintbox->a = rint(sqrt(dx * dx + dy * dy));
			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		case PAINTBOX_TOOL_FLOOD_UNTIL:
		case PAINTBOX_TOOL_FLOOD_WHILE:
		case PAINTBOX_TOOL_DROPPER:
			// only note the new position
			imageui_snap_point(paintbox->imageui, x, y, &x, &y);
			paintbox->x0 = x;
			paintbox->y0 = y;
			break;

		case PAINTBOX_TOOL_TEXT:
			// the snap box is positioned with no ascenders and no descenders
			VipsRect text = {
				.left = x,
				.top = y,
				.width = paintbox->mask->Xsize,
				.height = paintbox->baseline - paintbox->topline,
			};
			imageui_snap_rect(paintbox->imageui, &text, &text);
			paintbox->x0 = text.left;
			paintbox->y0 = text.top;

			gtk_widget_queue_draw(paintbox->imagedisplay);
			break;

		default:
			break;
		}

	return handled;
}

static gboolean
paintbox_enter(Paintbox *paintbox)
{
	paintbox->hide = FALSE;
	gtk_widget_queue_draw(paintbox->imagedisplay);

	return FALSE;
}

static gboolean
paintbox_leave(Paintbox *paintbox)
{
	paintbox->hide = TRUE;
	gtk_widget_queue_draw(paintbox->imagedisplay);

	return FALSE;
}

static gboolean
paintbox_key_pressed(Paintbox *paintbox,
	guint keyval, guint keycode, GdkModifierType state)
{
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

static gboolean
paintbox_event(GObject *object, const char *signal_name,
	double x, double y, int keyval, int keycode, GdkModifierType modifiers)
{
	Paintbox *paintbox = PAINTBOX(object);

	if (g_str_equal(signal_name, "motion"))
		return paintbox_motion(paintbox, x, y);
	else if (g_str_equal(signal_name, "drag-begin"))
		return paintbox_drag_begin(paintbox, x, y, modifiers);
	else if (g_str_equal(signal_name, "drag-update"))
		return paintbox_drag_update(paintbox, x, y, modifiers);
	else if (g_str_equal(signal_name, "drag-end"))
		return paintbox_drag_end(paintbox, x, y, modifiers);
	else if (g_str_equal(signal_name, "key-pressed"))
		return paintbox_key_pressed(paintbox, keyval, keycode, modifiers);
	else if (g_str_equal(signal_name, "enter"))
		return paintbox_enter(paintbox);
	else if (g_str_equal(signal_name, "leave"))
		return paintbox_leave(paintbox);

	return FALSE;
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
	imageui_client_add(paintbox->imageui, G_OBJECT(paintbox),
		100, paintbox_event);

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
	width->from = 1;
	width->to = 100;
	width->value = 5;
	width->digits = 0;
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
paintbox_undo_clicked(GtkToggleButton *button, Paintbox *paintbox)
{
	paintbox_undo(paintbox);
}

static void
paintbox_redo_clicked(GtkToggleButton *button, Paintbox *paintbox)
{
	paintbox_redo(paintbox);
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
	BIND_VARIABLE(Paintbox, undo_widget);
	BIND_VARIABLE(Paintbox, redo_widget);
	BIND_VARIABLE(Paintbox, pointer);
	BIND_VARIABLE(Paintbox, brush);
	BIND_VARIABLE(Paintbox, line);
	BIND_VARIABLE(Paintbox, rect);
	BIND_VARIABLE(Paintbox, circle);
	BIND_VARIABLE(Paintbox, smudge);
	BIND_VARIABLE(Paintbox, flood_while);
	BIND_VARIABLE(Paintbox, flood_until);
	BIND_VARIABLE(Paintbox, text);
	//// commented out for now, since we have not done the backend yet
	// BIND_VARIABLE(Paintbox, dropper);
	BIND_VARIABLE(Paintbox, ink);
	BIND_VARIABLE(Paintbox, fill);
	BIND_VARIABLE(Paintbox, width);
	BIND_VARIABLE(Paintbox, font);
	BIND_VARIABLE(Paintbox, text_string);

	BIND_CALLBACK(paintbox_toggled);
	BIND_CALLBACK(paintbox_undo_clicked);
	BIND_CALLBACK(paintbox_redo_clicked);

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
