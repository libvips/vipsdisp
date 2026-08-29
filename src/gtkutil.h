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

#define WHITESPACE " \t\r\b\n"

#define UNPARENT(W) \
	G_STMT_START \
	{ \
		if (W) { \
			gtk_widget_unparent(GTK_WIDGET(W)); \
			(W) = NULL; \
		} \
	} \
	G_STMT_END

#define BIND_RESOURCE(resource) \
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(class), \
		APP_PATH "/" resource);

#define BIND_LAYOUT() \
	gtk_widget_class_set_layout_manager_type(GTK_WIDGET_CLASS(class), \
		GTK_TYPE_BIN_LAYOUT);

#define BIND_VARIABLE(class_name, name) \
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(class), \
		class_name, name);

#define BIND_CALLBACK(name) \
	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(class), \
		name);

/* Like G_CHECK_TYPE, but insist on an exact match.
 */
#define TYPE_EXACT(OBJECT, TYPE) (G_TYPE_FROM_INSTANCE(OBJECT) == (TYPE))

/* Look up an object's parent class dynamically.
 */
#define PARENT_CLASS_DYNAMIC(OBJECT) \
	(g_type_class_peek(g_type_parent(G_TYPE_FROM_INSTANCE(OBJECT))))

// TRUE for in a nested mainloop
extern gboolean in_update;

void set_glabel(GtkWidget *label, const char *fmt, ...);
void set_glabel1(GtkWidget *label, const char *fmt, ...);
void set_gentryv(GtkWidget *edit, const char *fmt, va_list ap);
void set_gentry(GtkWidget *edit, const char *fmt, ...);
gboolean get_geditable_double(GtkWidget *text, double *out);
GtkWidget *build_entry(int nchars);
void set_tooltip(GtkWidget *wid, const char *fmt, ...);
void copy_adj(GtkAdjustment *to, GtkAdjustment *from);


void change_state(GtkWidget *widget, const char *name, GVariant *state);
void set_state(GtkWidget *to, GSettings *settings, const char *name);
GVariant *get_state(GtkWidget *widget, const char *name);
void copy_state(GtkWidget *to, GtkWidget *from, const char *name);

void set_state_bool(GtkWidget *to, const char *name, gboolean value);
void set_state_double(GtkWidget *to, const char *name, double value);
void set_state_int(GtkWidget *to, const char *name, int value);
void set_state_enum(GtkWidget *to, const char *name, const char *value);

gboolean get_state_bool(GtkWidget *from, const char *name);
double get_state_double(GtkWidget *from, const char *name);
int get_state_int(GtkWidget *from, const char *name);

void process_events(void);

void block_scroll(GtkWidget *widget);

gboolean widget_should_animate(GtkWidget *widget);

void action_toggle(GSimpleAction *action,
	GVariant *parameter, gpointer user_data);
void action_radio(GSimpleAction *action,
	GVariant *parameter, gpointer user_data);

int get_dpi(void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(cairo_t, cairo_destroy)
G_DEFINE_AUTOPTR_CLEANUP_FUNC(cairo_surface_t, cairo_surface_destroy)

typedef void (*Yesno)(GtkWindow *window, void *user_data);
void alert_yesno(GtkWindow *window, Yesno yesno, void *user_data,
	const char *message, const char *format, ...);

char *text_view_get_text(GtkTextView *text);
void text_view_set_text(GtkTextView *text, const char *str, gboolean editable);
void text_view_select_text(GtkTextView *text, int start, int end);

gboolean line_clip(VipsRect *rect,
	int x0, int y0, int x1, int y1,
	int *x0_out, int *y0_out, int *x1_out, int *y1_out);

typedef gboolean (*ValueToFilenameFn)(const char *filename, void *user_data);
gboolean value_to_filename(const GValue *value,
	ValueToFilenameFn fn, void *user_data);

void weakref_set(GObject **pointer, GObject *object);
#define WEAKREF_SET(A, B) weakref_set((GObject **) &(A), (GObject *) (B));
