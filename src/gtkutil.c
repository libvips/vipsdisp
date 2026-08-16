// gtk utility funcs

/*

    Copyright (C) 1991-2003 The National Gallery
    Copyright (C) 2004-2023 libvips.org

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

#include "nip4.h"

// set when we're processing some events to update the GUI ... used to sanity
// check eg. reduce (we mustn't call reduce from inside the nested loop,
// it'll crash into the main reduce thread)
gboolean in_update = FALSE;

void
set_glabel(GtkWidget *label, const char *fmt, ...)
{
    va_list ap;
    char buf[1000];

    va_start(ap, fmt);
    (void) g_vsnprintf(buf, 1000, fmt, ap);
    va_end(ap);

    if (!g_utf8_validate(buf, -1, NULL))
        (void) g_snprintf(buf, 1000, "%s", _("<invalid utf8 string>"));

    gtk_label_set_text(GTK_LABEL(label), buf);
}

/* Like set_glabel(), but don't display multi-line strings (just display the
 * first line).
 */
void
set_glabel1(GtkWidget *label, const char *fmt, ...)
{
    va_list ap;
    char txt[80];
    VipsBuf buf = VIPS_BUF_STATIC(txt);

    va_start(ap, fmt);
    vips_buf_vappendf(&buf, fmt, ap);
    va_end(ap);

    gtk_label_set_text(GTK_LABEL(label), vips_buf_firstline(&buf));
}

/* Set a GtkEditable.
 */
void
set_gentryv(GtkWidget *edit, const char *fmt, va_list ap)
{
    char buf[1000];
    gint position;
    int i;
    int len;

    if (!edit)
        return;

    if (!fmt)
        fmt = "";

    (void) g_vsnprintf(buf, 1000, fmt, ap);

    /* Filter out /n and /t ... they confuse gtkentry terribly
     */
    len = strlen(buf);
    for (i = 0; i < len; i++)
        if (buf[i] == '\n' || buf[i] == '\t')
            buf[i] = ' ';

    gtk_editable_delete_text(GTK_EDITABLE(edit), 0, -1);
    position = 0;
    gtk_editable_insert_text(GTK_EDITABLE(edit), buf, strlen(buf), &position);
}

/* Set a GtkEditable.
 */
void
set_gentry(GtkWidget *edit, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    set_gentryv(edit, fmt, ap);
    va_end(ap);
}

/* Get a geditable as a double.
 */
gboolean
get_geditable_double(GtkWidget *text, double *out)
{
    g_autofree char *txt = gtk_editable_get_chars(GTK_EDITABLE(text), 0, -1);

    char *end;
    double t;

    t = strtod(txt, &end);
    if (end == txt)
        return FALSE;
    if (strspn(end, WHITESPACE) != strlen(end))
        return FALSE;

    *out = t;

    return TRUE;
}

/* Set the tooltip on a widget.
 */
void
set_tooltip(GtkWidget *wid, const char *fmt, ...)
{
    va_list ap;
    char *txt;

    if (!wid)
        return;

    if (!fmt)
        fmt = "";

    va_start(ap, fmt);
    txt = g_strdup_vprintf(fmt, ap);
    va_end(ap);

    gtk_widget_set_tooltip_text(wid, txt);

    g_free(txt);
}

void
copy_adj(GtkAdjustment *to, GtkAdjustment *from)
{
    double value = gtk_adjustment_get_value(from);
    double lower = gtk_adjustment_get_lower(from);
    double upper = gtk_adjustment_get_upper(from);
    double step_increment = gtk_adjustment_get_step_increment(from);
    double page_increment = gtk_adjustment_get_page_increment(from);
    double page_size = gtk_adjustment_get_page_size(from);

    gtk_adjustment_configure(to, value,
        lower, upper,
        step_increment, page_increment, page_size);
}

void
change_state(GtkWidget *widget, const char *name, GVariant *state)
{
    GAction *action = g_action_map_lookup_action(G_ACTION_MAP(widget), name);
    if (action)
        g_action_change_state(action, state);
}

void
set_state(GtkWidget *to, GSettings *settings, const char *name)
{
    g_autoptr(GVariant) var = g_settings_get_value(settings, name);

    change_state(to, name, var);
}

GVariant *
get_state(GtkWidget *widget, const char *name)
{
    GAction *action;

    action = g_action_map_lookup_action(G_ACTION_MAP(widget), name);
    if (!action)
        return NULL;

    return g_action_get_state(action);
}

void
copy_state(GtkWidget *to, GtkWidget *from, const char *name)
{
    g_autoptr(GVariant) state = get_state(from, name);

    if (state)
        change_state(to, name, state);
}

void
set_state_bool(GtkWidget *to, const char *name, gboolean value)
{
    GVariant *state = g_variant_new_boolean(value);

    change_state(to, name, state);
}

void
set_state_double(GtkWidget *to, const char *name, double value)
{
    GVariant *state = g_variant_new_double(value);

    change_state(to, name, state);
}

void
set_state_int(GtkWidget *to, const char *name, int value)
{
    GVariant *state = g_variant_new_int32(value);

    change_state(to, name, state);
}

void
set_state_enum(GtkWidget *to, const char *name, const char *value)
{
    GVariant *state = g_variant_new_string(value);

    change_state(to, name, state);
}

gboolean
get_state_bool(GtkWidget *from, const char *name)
{
    g_autoptr(GVariant) state = get_state(from, name);

    return g_variant_get_boolean(state);
}

double
get_state_double(GtkWidget *from, const char *name)
{
    g_autoptr(GVariant) state = get_state(from, name);

    return g_variant_get_double(state);
}

int
get_state_int(GtkWidget *from, const char *name)
{
    g_autoptr(GVariant) state = get_state(from, name);

    return g_variant_get_int32(state);
}

/* A 'safe' way to run a few events.
 */
void
process_events(void)
{
    /* Max events we process before signalling a timeout. Without this we
     * can get stuck in event loops in some circumstances.
     */
    static const int max_events = 100;

    /* Block too much recursion. 0 is from the top-level, 1 is from a
     * callback, we don't want any more than that.
     *
     * No need to process in batch mode.
     */
    if (!in_update &&
        !main_option_batch &&
        g_main_depth() < 2) {
        int n;

        in_update = TRUE;

        for (n = 0; n < max_events &&
             g_main_context_iteration(NULL, FALSE);
             n++)
            ;

        in_update = FALSE;
    }
}

static gboolean
block_scroll_cb(GtkEventControllerScroll *self,
    gdouble dx, gdouble dy, gpointer user_data)
{
    // TRUE means we handled the event and it should not be propagated
    // further
    return TRUE;
}

/* Stop scroll events (eg. the mousewheel) changing the value of this widget.
 * Handy for eg. scale widgets inside a scrolled window.
 */
void
block_scroll(GtkWidget *widget)
{
    GtkEventController *controller = gtk_event_controller_scroll_new(
        GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);

    gtk_event_controller_set_propagation_phase(controller, GTK_PHASE_CAPTURE);
    g_signal_connect(controller,
        "scroll", G_CALLBACK(block_scroll_cb), NULL);
    gtk_widget_add_controller(widget, controller);
}

gboolean
widget_should_animate(GtkWidget *widget)
{
    gboolean enable_animations;

    g_object_get(gtk_widget_get_settings(widget),
        "gtk-enable-animations", &enable_animations,
        NULL);

    return enable_animations;
}

void
action_toggle(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_autoptr(GVariant) state = g_action_get_state(G_ACTION(action));

    g_action_change_state(G_ACTION(action),
        g_variant_new_boolean(!g_variant_get_boolean(state)));
}

void
action_radio(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    g_action_change_state(G_ACTION(action), parameter);
}

/* Get the default screen DPI.
 */
int
get_dpi(void)
{
    // should be per window? maybe this makes no sense anyway
    printf("get_dpi: FIXME\n");

    return 72;
}

static void
alert_yesno_cb(GObject *source_object,
    GAsyncResult *result, gpointer user_data)
{
    GtkAlertDialog *alert = GTK_ALERT_DIALOG(source_object);
    GtkWindow *window = g_object_get_data(G_OBJECT(alert), "nip4-window");
    Yesno yesno = g_object_get_data(G_OBJECT(alert), "nip4-yesno");
    int choice = gtk_alert_dialog_choose_finish(alert, result, NULL);

    if (choice == 1)
        yesno(window, user_data);
}

/* Ask before doing something.
 */
void
alert_yesno(GtkWindow *window, Yesno yesno, void *user_data,
    const char *message, const char *format, ...)
{
    va_list ap;
    char buf[1000];

    va_start(ap, format);
    (void) g_vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);

    const char *labels[] = { "Cancel", "OK", NULL };

    GtkAlertDialog *alert = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_set_detail(alert, buf);
    gtk_alert_dialog_set_buttons(alert, labels);
    gtk_alert_dialog_set_modal(alert, TRUE);
    g_object_set_data(G_OBJECT(alert), "nip4-window", window);
    g_object_set_data(G_OBJECT(alert), "nip4-yesno", yesno);
    gtk_alert_dialog_choose(alert, window, NULL, alert_yesno_cb, user_data);
}

char *
text_view_get_text(GtkTextView *text)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

    GtkTextIter start;
    GtkTextIter end;

    gtk_text_buffer_get_start_iter(buffer, &start);
    gtk_text_buffer_get_end_iter(buffer, &end);

    return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

void
text_view_set_text(GtkTextView *text, const char *str, gboolean editable)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

    gtk_text_buffer_set_text(buffer, str ? str : "", -1);

    gtk_text_view_set_editable(text, editable);
    gtk_text_view_set_cursor_visible(text, editable);
}

void
text_view_select_text(GtkTextView *text, int start, int end)
{
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

    GtkTextMark *mark = gtk_text_buffer_get_insert(buffer);
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
    gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
    gtk_text_buffer_select_range(buffer, &start_iter, &end_iter);
    gtk_text_view_scroll_mark_onscreen(text, mark);
}

/* Cohen–Sutherland line clipping, see:
 *
 *  https://en.wikipedia.org/wiki/Cohen%E2%80%93Sutherland_algorithm
 *
 */

static const int INSIDE = 0b0000;
static const int LEFT   = 0b0001;
static const int RIGHT  = 0b0010;
static const int BOTTOM = 0b0100;
static const int TOP    = 0b1000;

// Compute the bit code for a point (x, y) using the clip rectangle
// rect

static int
compute_out_code(VipsRect *rect, int x, int y)
{
    int code;

    code = INSIDE;
    if (x < rect->left)
        code |= LEFT;
    else if (x > VIPS_RECT_RIGHT(rect))
        code |= RIGHT;
    if (y < rect->top)
        code |= TOP;
    else if (y > VIPS_RECT_BOTTOM(rect))
        code |= BOTTOM;

    return code;
}

// Cohen–Sutherland clipping algorithm clips a line from
// P0 = (x0, y0) to P1 = (x1, y1) against a rectangle with
// diagonal from (xmin, ymin) to (xmax, ymax).
gboolean
line_clip(VipsRect *rect,
    int x0, int y0, int x1, int y1,
    int *x0_out, int *y0_out, int *x1_out, int *y1_out)
{
    int bottom = VIPS_RECT_BOTTOM(rect);
    int right = VIPS_RECT_RIGHT(rect);

    int outcode0;
    int outcode1;
    gboolean accept;

    outcode0 = compute_out_code(rect, x0, y0);
    outcode1 = compute_out_code(rect, x1, y1);
    accept = FALSE;
    for (;;) {
        if (!(outcode0 | outcode1)) {
            // bitwise OR is 0, both points inside window
            // trivially accept and exit
            accept = TRUE;
            break;
        }
        else if (outcode0 & outcode1) {
            // bitwise AND is not 0: both points share an outside zone
            // (LEFT, RIGHT, TOP, or BOTTOM), so both must be outside window
            accept = FALSE;
            break;
        }
        else {
            // failed both tests, so calculate the line segment to clip
            // from an outside point to an intersection with clip edge
            int x, y;

            // At least one endpoint is outside the clip rectangle; pick it.
            int outcode_out = outcode1 > outcode0 ? outcode1 : outcode0;

            // Now find the intersection point;
            // use formulas:
            //   slope = (y1 - y0) / (x1 - x0)
            //   x = x0 + (1 / slope) * (ym - y0), where ym is ymin or ymax
            //   y = y0 + slope * (xm - x0), where xm is xmin or xmax
            // No need to worry about divide-by-zero because, in each case, the
            // outcode bit being tested guarantees the denominator is non-zero

            if (outcode_out & TOP) {
                // above the clip window
                x = x0 + (x1 - x0) * (rect->top - y0) / (y1 - y0);
                y = rect->top;
            }
            else if (outcode_out & BOTTOM) {
                // below the clip window
                x = x0 + (x1 - x0) * (bottom - y0) / (y1 - y0);
                y = bottom;
            }
            else if (outcode_out & RIGHT) {
                // to the right of clip window
                y = y0 + (y1 - y0) * (right - x0) / (x1 - x0);
                x = right;
            }
            else {
                // point must be to the left of clip window
                y = y0 + (y1 - y0) * (rect->left - x0) / (x1 - x0);
                x = rect->left;
            }

            // Now we move outside point to intersection point to clip
            // and get ready for next pass.
            if (outcode_out == outcode0) {
                x0 = x;
                y0 = y;
                outcode0 = compute_out_code(rect, x0, y0);
            } else {
                x1 = x;
                y1 = y;
                outcode1 = compute_out_code(rect, x1, y1);
            }
        }
    }

    *x0_out = x0;
    *y0_out = y0;
    *x1_out = x1;
    *y1_out = y1;

    return accept;
}

/* Used for copy-paste, so it needs to support that set of GValues.
 */
gboolean
value_to_filename(const GValue *value, ValueToFilenameFn fn, void *user_data)
{
    if (G_VALUE_TYPE(value) == GDK_TYPE_FILE_LIST) {
        GdkFileList *file_list = g_value_get_boxed(value);
        g_autoptr(GSList) files = gdk_file_list_get_files(file_list);

        for (GSList *p = files; p; p = p->next) {
            GFile *file = G_FILE(p->data);
            g_autofree char *path = g_file_get_path(file);
            g_autofree char *strip_path = g_strstrip(g_strdup(path));

            if (!fn(strip_path, user_data))
                return FALSE;
        }
    }
    else if (G_VALUE_TYPE(value) == G_TYPE_FILE) {
        GFile *file = g_value_get_object(value);
        g_autofree char *path = g_file_get_path(file);
        g_autofree char *strip_path = g_strstrip(g_strdup(path));

        if (!fn(strip_path, user_data))
            return FALSE;
    }
    else if (G_VALUE_TYPE(value) == G_TYPE_STRING) {
        g_autofree char *strip_path =
            g_strstrip(g_strdup(g_value_get_string(value)));

        if (!fn(strip_path, user_data))
            return FALSE;
    }
    else if (G_VALUE_TYPE(value) == GDK_TYPE_TEXTURE) {
        GdkTexture *texture = g_value_get_object(value);

        Imageinfo *ii =
            imageinfo_new_from_texture(main_imageinfogroup, NULL, texture);
        if (!ii)
            return FALSE;

        char filename[VIPS_PATH_MAX];
        if (!temp_name(filename, NULL, "v"))
            return FALSE;
        if (vips_image_write_to_file(ii->image, filename, NULL))
            return FALSE;

        Imageinfo *temp =
            imageinfo_new_input(main_imageinfogroup, NULL, NULL, filename);
        imageinfo_set_delete_on_close(temp);
        // will be unreffed on the next GC, so fn() below will need to
        // grab it
        MANAGED_UNREF(temp);

        if (!fn(filename, user_data))
            return FALSE;
    }

    return TRUE;
}

static void
weakref_notify(void *user_data, GObject *where_the_object_was)
{
    GObject **pointer = (GObject **) user_data;

    if (pointer)
        *pointer = NULL;
}

void
weakref_set(GObject **pointer, GObject *object)
{
    if (*pointer)
        g_object_weak_unref(*pointer, weakref_notify, pointer);
    if (pointer)
        *pointer = object;
    if (*pointer)
        g_object_weak_ref(*pointer, weakref_notify, pointer);
}

#define WEAKREF_SET(A, B) weakref_set((GObject **) &(A), (GObject *) (B));

