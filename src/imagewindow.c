/* an image display window
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

/* We have a number of active imageui (open images) and look them up via the
 * tilesource filename or image pointer.
 *
 * Use these to flip back to an already open image.
 */
typedef struct _Active {
	Imagewindow *win;

	/* This is a ref ... we can't just use the ref in the widget hierarchy or
	 * it'll be freed during gtk_stack_remove() and cause chaos.
	 *
	 * Fetch active
	 */
	Imageui *imageui;

	/* A timestamp we update on every action -- we throw away the oldest views
	 * first.
	 */
	int timestamp;
} Active;

struct _Imagewindow {
	GtkApplicationWindow parent;

	/* A ref to the iimage we came from ... this is what we update on open
	 * etc.
	 */
	iImage *iimage;
	guint iimage_changed_sid;
	guint iimage_destroy_sid;

	/* The imageui we are currently displaying, or NULL. This is not
	 * a reference --- the real refs are in @stack.
	 */
	Imageui *imageui;

	/* The set of filenames we cycle through.
	 */
	char **files;
	int n_files;
	int current_file;
	char *dirname;

	/* Widgets.
	 */
	GtkWidget *right_click_menu;
	GtkWidget *prev;
	GtkWidget *next;
	GtkWidget *refresh;
	GtkWidget *title;
	GtkWidget *subtitle;
	GtkWidget *gears;
	GtkWidget *error_bar;
	GtkWidget *error_label;
	GtkWidget *main_box;
	GtkWidget *stack;
	GtkWidget *properties;
	GtkWidget *displaybar;
	GtkWidget *info_bar;
	GtkWidget *region_menu;

	/* The set of active images in the stack right now. These are not
	 * references.
	 */
	GSList *active;

	/* Set on menu popup on a regionview.
	 */
	View *action_view;

	GSettings *settings;

	/* Next transition hint.
	 */
	GtkStackTransitionType transition;
};

G_DEFINE_TYPE(Imagewindow, imagewindow, GTK_TYPE_APPLICATION_WINDOW);

/* Our signals.
 */
enum {
	SIG_CHANGED,		/* The image has updated within the same imageui */
	SIG_NEW_IMAGE,		/* Completely new image */
	SIG_STATUS_CHANGED, /* New mouse position */
	SIG_LAST
};

static guint imagewindow_signals[SIG_LAST] = { 0 };

/* GTypes we handle in copy/paste and drag/drop paste ... these are in the
 * order we try, so a GFile is preferred to a simple string.
 *
 * gnome file manager pastes as GdkFileList, GFile, gchararray
 * print-screen button pastes as GdkTexture, GdkPixbuf
 *
 * Created in _class_init(), since some of these types are only defined at
 * runtime.
 */
static GType *imagewindow_supported_types;
static int imagewindow_n_supported_types;

static const int imagewindow_max_active = 3;

static void
imagewindow_set_error(Imagewindow *win, const char *message)
{
	int i;

#ifdef DEBUG
	printf("imagewindow_set_error: %s\n", message);
#endif /*DEBUG*/

	/* Remove any trailing \n.
	 */
	g_autofree char *err = g_strdup(message);
	for (i = strlen(err); i > 0 && err[i - 1] == '\n'; i--)
		err[i - 1] = '\0';
	gtk_label_set_text(GTK_LABEL(win->error_label), err);

	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->error_bar), TRUE);
}

static void
imagewindow_error(Imagewindow *win)
{
	imagewindow_set_error(win, vips_error_buffer());
	vips_error_clear();
}

static void
imagewindow_gerror(Imagewindow *win, GError **error)
{
	if (error && *error) {
		imagewindow_set_error(win, (*error)->message);
		g_error_free(*error);
	}
}

static void
imagewindow_error_hide(Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_error_hide:\n");
#endif /*DEBUG*/

	gtk_action_bar_set_revealed(GTK_ACTION_BAR(win->error_bar), FALSE);
}

static void
imagewindow_error_clicked(GtkWidget *button, Imagewindow *win)
{
	imagewindow_error_hide(win);
}

/* Manage the set of active views.
 */

static Active *
imagewindow_active_lookup_by_filename(Imagewindow *win, const char *filename)
{
	for (GSList *p = win->active; p; p = p->next) {
		Active *active = (Active *) p->data;
		Tilesource *tilesource = imageui_get_tilesource(active->imageui);

		if (tilesource->filename &&
			g_str_equal(tilesource->filename, filename))
			return active;
	}

	return NULL;
}

static void
imagewindow_active_touch(Imagewindow *win, Active *active)
{
	static int serial = 0;

	active->timestamp = serial++;
}

static void
imagewindow_active_remove(Imagewindow *win, Active *active)
{
	gtk_stack_remove(GTK_STACK(win->stack), GTK_WIDGET(active->imageui));

	if (active->imageui == win->imageui)
		win->imageui = NULL;

	VIPS_UNREF(active->imageui);

	win->active = g_slist_remove(win->active, active);

	g_free(active);
}

static void
imagewindow_active_add(Imagewindow *win, Imageui *imageui)
{
	Active *active = VIPS_NEW(NULL, Active);
	active->win = win;
	active->imageui = imageui;

	// we hold a true reference
	g_object_ref(imageui);

	win->active = g_slist_prepend(win->active, active);

	imagewindow_active_touch(win, active);

	while (g_slist_length(win->active) > imagewindow_max_active) {
		Active *oldest;

		oldest = NULL;
		for (GSList *p = win->active; p; p = p->next) {
			Active *active = (Active *) p->data;

			if (!oldest ||
				active->timestamp < oldest->timestamp)
				oldest = active;
		}

		imagewindow_active_remove(win, oldest);
	}
}

/* Manage the set of filenames we have lined up to view.
 */

static void
imagewindow_files_free(Imagewindow *win)
{
	VIPS_FREEF(g_strfreev, win->files);
	VIPS_FREE(win->dirname);
	win->n_files = 0;
	win->current_file = 0;
}

static int
sort_filenames(const void *a, const void *b)
{
	const char *f1 = (const char *) a;
	const char *f2 = (const char *) b;

	return g_ascii_strcasecmp(f1, f2);
}

static void
imagewindow_files_set_list(Imagewindow *win, GSList *files)
{
	GSList *p;
	int i;

	imagewindow_files_free(win);

	win->n_files = g_slist_length(files);
	win->files = VIPS_ARRAY(NULL, win->n_files + 1, char *);
	for (i = 0, p = files; i < win->n_files; i++, p = p->next)
		win->files[i] = g_strdup((char *) p->data);

#ifdef DEBUG
	printf("imagewindow_files_set_list: %d files\n", win->n_files);
#endif /*DEBUG*/
}

static void
imagewindow_files_set_path(Imagewindow *win, const char *path)
{
	g_autofree char *dirname = g_path_get_dirname(path);
	g_autoptr(GFile) file = g_file_new_for_path(path);

	GError *error = NULL;

	const char *filename;

#ifdef DEBUG
	printf("imagewindow_files_set_path:\n");
#endif /*DEBUG*/

	// same dir as last time? do nothing
	if (win->dirname &&
		g_str_equal(dirname, win->dirname))
		return;

	imagewindow_files_free(win);

	win->dirname = g_steal_pointer(&dirname);
	g_autoptr(GDir) dir = g_dir_open(win->dirname, 0, &error);
	if (!dir) {
		imagewindow_gerror(win, &error);
		return;
	}

	GSList *files = NULL;

	// always add the passed-in file, even if it doesn't exist
	files = g_slist_prepend(files, g_strdup(path));

	while ((filename = g_dir_read_name(dir))) {
		g_autofree char *path = g_build_path("/", win->dirname, filename, NULL);
		g_autoptr(GFile) this_file = g_file_new_for_path(path);

		// - never add the the passed-in filename (we add it above)
		// - avoid directories and dotfiles
		if (!g_file_equal(file, this_file) &&
			!vips_isprefix(".", filename) &&
			!g_file_test(path, G_FILE_TEST_IS_DIR))
			files = g_slist_prepend(files, g_steal_pointer(&path));
	}

	files = g_slist_sort(files, (GCompareFunc) sort_filenames);

	imagewindow_files_set_list(win, files);

	// it's be great to use g_autoslist(), but I don't see how :(
	g_slist_free_full(g_steal_pointer(&files), g_free);

	for (int i = 0; i < win->n_files; i++) {
		g_autoptr(GFile) file2 = g_file_new_for_path(win->files[i]);

		if (g_file_equal(file, file2)) {
			win->current_file = i;
			break;
		}
	}
}

static void
imagewindow_files_set(Imagewindow *win,
	const char **files, int n_files, gboolean force)
{
	if (n_files == 1) {
		// on a forced update, make sure we regenerate everything
		if (force)
			imagewindow_files_free(win);

		imagewindow_files_set_path(win, files[0]);
	}
	else if (n_files > 1) {
		imagewindow_files_free(win);

		win->n_files = n_files;
		win->files = VIPS_ARRAY(NULL, n_files + 1, char *);
		for (int i = 0; i < win->n_files; i++)
			win->files[i] = g_strdup(files[i]);

		win->current_file = 0;
	}
}

double
imagewindow_get_zoom(Imagewindow *win)
{
	double zoom;

	if (win->imageui)
		g_object_get(win->imageui,
			"zoom", &zoom,
			NULL);
	else
		zoom = 1.0;

	double pixel_size;
	if (win->imageui)
		g_object_get(win->imageui,
			"pixel_size", &pixel_size,
			NULL);
	else
		pixel_size = 1.0;

	return zoom / pixel_size;
}

void
imagewindow_get_mouse_position(Imagewindow *win,
	double *image_x, double *image_y)
{
	*image_x = 0.0;
	*image_y = 0.0;
	if (win->imageui)
		imageui_get_mouse_position(win->imageui, image_x, image_y);
}

Tilesource *
imagewindow_get_tilesource(Imagewindow *win)
{
	return win->imageui ? imageui_get_tilesource(win->imageui) : NULL;
}

Imageui *
imagewindow_get_imageui(Imagewindow *win)
{
	return win->imageui;
}

static void
imagewindow_reset_view(Imagewindow *win)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		set_state_bool(GTK_WIDGET(win), "falsecolour", FALSE);
		set_state_bool(GTK_WIDGET(win), "log", FALSE);
		set_state_bool(GTK_WIDGET(win), "icc", FALSE);
		set_state_bool(GTK_WIDGET(win), "preserve", FALSE);
		set_state_enum(GTK_WIDGET(win), "mode", "multipage");
		set_state_enum(GTK_WIDGET(win), "background", "checkerboard");

		g_object_set(tilesource,
			"scale", 1.0,
			"offset", 0.0,
			"page", 0,
			NULL);
	}
}

static void
imagewindow_tilesource_changed(Tilesource *tilesource, Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_tilesource_changed:\n");
#endif /*DEBUG*/

	if (tilesource->load_error)
		imagewindow_set_error(win, tilesource->load_message);

	// update win settings from new tile source
	GVariant *state;

	state = g_variant_new_boolean(tilesource->falsecolour);
	change_state(GTK_WIDGET(win), "falsecolour", state);

	state = g_variant_new_boolean(tilesource->log);
	change_state(GTK_WIDGET(win), "log", state);

	state = g_variant_new_boolean(tilesource->icc);
	change_state(GTK_WIDGET(win), "icc", state);

	const char *name = vips_enum_nick(TILESOURCE_MODE_TYPE, tilesource->mode);
	state = g_variant_new_string(name);
	change_state(GTK_WIDGET(win), "mode", state);
}

static void
imagewindow_status_changed(Imagewindow *win)
{
	g_signal_emit(win, imagewindow_signals[SIG_STATUS_CHANGED], 0);
}

static void
imagewindow_changed(Imagewindow *win)
{
	g_signal_emit(win, imagewindow_signals[SIG_CHANGED], 0);
}

static void
imagewindow_new_image(Imagewindow *win)
{
	g_signal_emit(win, imagewindow_signals[SIG_NEW_IMAGE], 0);
}

static void
imagewindow_imageui_changed(Imageui *imageui, Imagewindow *win)
{
	// come here for mouse drag, ie. we should update the infobar
	imagewindow_status_changed(win);
}

/* Add an imageui to the stack.
 */
static void
imagewindow_imageui_add(Imagewindow *win, Imageui *imageui)
{
	Tilesource *tilesource = imageui_get_tilesource(imageui);

	g_signal_connect_object(tilesource, "changed",
		G_CALLBACK(imagewindow_tilesource_changed), win, 0);
	g_signal_connect_object(imageui, "changed",
		G_CALLBACK(imagewindow_imageui_changed), win, 0);

	gtk_stack_add_child(GTK_STACK(win->stack), GTK_WIDGET(imageui));

	imagewindow_active_add(win, imageui);

	tilesource_background_load(tilesource);
}

/* Change the image we are manipulating. The imageui is in the stack already.
 */
static void
imagewindow_imageui_set_visible(Imagewindow *win, Imageui *imageui)
{
	Tilesource *old_tilesource = imagewindow_get_tilesource(win);
	Tilesource *new_tilesource =
		imageui ? imageui_get_tilesource(imageui) : NULL;

	char str[256];
	VipsBuf buf = VIPS_BUF_STATIC(str);

	VipsImage *image;

	if (old_tilesource)
		g_object_set(old_tilesource,
			"visible", FALSE,
			NULL);

	g_object_set(win->properties,
		"tilesource", new_tilesource,
		NULL);

	/* Update title and subtitle.
	 */
	const char *title;
	title = tilesource_get_path(new_tilesource);
	if (!title &&
		HEAPMODEL(win->iimage)->row) {
		vips_buf_rewind(&buf);
		row_qualified_name(HEAPMODEL(win->iimage)->row, &buf);
		title = vips_buf_all(&buf);
	}
	if (!title)
		title = "Untitled";

	gtk_label_set_text(GTK_LABEL(win->title), title);

	if (new_tilesource &&
		(image = tilesource_get_base_image(new_tilesource))) {
		char str[256];
		VipsBuf buf = VIPS_BUF_STATIC(str);

		vips_buf_appendf(&buf, "%dx%d, ", image->Xsize, image->Ysize);
		if (new_tilesource->n_pages > 1)
			vips_buf_appendf(&buf, "%d pages, ", new_tilesource->n_pages);
		if (vips_image_get_coding(image) == VIPS_CODING_NONE)
			vips_buf_appendf(&buf,
				g_dngettext(GETTEXT_PACKAGE,
					" %s, %d band, %s",
					" %s, %d bands, %s",
					image->Bands),
				vips_enum_nick(VIPS_TYPE_BAND_FORMAT, image->BandFmt),
				vips_image_get_bands(image),
				vips_enum_nick(VIPS_TYPE_INTERPRETATION, image->Type));
		else
			vips_buf_appendf(&buf, "%s",
				vips_enum_nick(VIPS_TYPE_CODING,
					vips_image_get_coding(image)));
		vips_buf_appends(&buf, ", ");

		vips_buf_append_size(&buf,
			(double) image->Ysize * VIPS_IMAGE_SIZEOF_LINE(image));
		vips_buf_appends(&buf, ", ");

		vips_buf_appendf(&buf, "%g x %g p/mm", image->Xres, image->Yres);
		gtk_label_set_text(GTK_LABEL(win->subtitle), vips_buf_all(&buf));
	}
	else
		gtk_label_set_text(GTK_LABEL(win->subtitle), "");

	if (imageui) {
		gtk_stack_set_transition_type(GTK_STACK(win->stack), win->transition);
		gtk_stack_set_visible_child(GTK_STACK(win->stack), GTK_WIDGET(imageui));

		/* Enable the control settings, if the displaycontrolbar is on.
		 */
		g_autoptr(GVariant) control =
			g_settings_get_value(win->settings, "control");

		g_object_set(new_tilesource,
			"active", g_variant_get_boolean(control),
			"visible", TRUE,
			NULL);
	}

	if (win->imageui != imageui) {
		// not a ref, so we can just overwrite it
		win->imageui = imageui;

		// tell everyone there's a new imageui
		imagewindow_new_image(win);
	}
	else
		// the same image, just updated
		imagewindow_changed(win);

	// update the menus
	imagewindow_tilesource_changed(new_tilesource, win);

	TilecacheBackground background;
	g_object_get(win->imageui,
		"background", &background,
		NULL);
	const char *name = vips_enum_nick(TILECACHE_BACKGROUND_TYPE, background);
	GVariant *state = g_variant_new_string(name);
	change_state(GTK_WIDGET(win), "background", state);
}

static void
imagewindow_dispose(GObject *object)
{
	Imagewindow *win = IMAGEWINDOW(object);

#ifdef DEBUG
	printf("imagewindow_dispose:\n");
#endif /*DEBUG*/

	FREESID(win->iimage_changed_sid, win->iimage);
	FREESID(win->iimage_destroy_sid, win->iimage);
	VIPS_FREEF(gtk_widget_unparent, win->right_click_menu);

	iimage_update_view_settings(win->iimage,
		displaybar_get_view_settings(DISPLAYBAR(win->displaybar)));

	while (win->active) {
		Active *active = (Active *) win->active->data;

		imagewindow_active_remove(win, active);
	}

	imagewindow_files_free(win);

	G_OBJECT_CLASS(imagewindow_parent_class)->dispose(object);
}

static GdkTexture *
texture_new_from_image(VipsImage *image)
{
	GdkTexture *texture;

	// don't convert huge images to textures, we'll kill the machine
	if ((gint64) image->Xsize * image->Ysize > 5000 * 5000) {
		vips_error("Convert to texture",
			_("Image too large to convert to texture"));
		return NULL;
	}

	void *buf;
	size_t len;
	if (vips_tiffsave_buffer(image, &buf, &len, NULL))
		return NULL;

	g_autoptr(GBytes) bytes = g_bytes_new_take(buf, len);
	GError *error = NULL;
	texture = gdk_texture_new_from_bytes(bytes, &error);
	if (!texture) {
		vips_error("Convert to texture", "%s", error->message);
		g_error_free(error);
	}

	return texture;
}

static void
imagewindow_copy_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(win));
		g_autoptr(GFile) file = tilesource_get_file(tilesource);

		VipsImage *image;

		if (file)
			gdk_clipboard_set(clipboard, G_TYPE_FILE, file);
		else if ((image = tilesource_get_base_image(tilesource))) {
			g_autoptr(GdkTexture) texture = texture_new_from_image(image);

			if (texture)
				gdk_clipboard_set(clipboard, GDK_TYPE_TEXTURE, texture);
			else
				imagewindow_error(win);
		}
	}
}

static gboolean
imagewindow_paste_filename(const char *filename, void *user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	win->transition = GTK_STACK_TRANSITION_TYPE_SLIDE_DOWN;
	iimage_replace(win->iimage, filename);
	symbol_recalculate_all();

	return TRUE;
}

static void
imagewindow_paste_action_ready(GObject *source_object,
	GAsyncResult *res, gpointer user_data)
{
	GdkClipboard *clipboard = GDK_CLIPBOARD(source_object);
	Imagewindow *win = IMAGEWINDOW(user_data);

	const GValue *value;
	GError *error = NULL;
	value = gdk_clipboard_read_value_finish(clipboard, res, &error);
	if (error) {
		imagewindow_gerror(win, &error);
		return;
	}

	if (value &&
		!value_to_filename(value, imagewindow_paste_filename, win))
		imagewindow_error(win);
}

static void
imagewindow_paste_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	GdkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(win));

	GdkContentFormats *formats = gdk_clipboard_get_formats(clipboard);
	gsize n_types;
	const GType *types = gdk_content_formats_get_gtypes(formats, &n_types);

#ifdef DEBUG
	printf("clipboard in %lu formats\n", n_types);
	for (gsize i = 0; i < n_types; i++)
		printf("%lu - %s\n", i, g_type_name(types[i]));
#endif /*DEBUG*/

	gboolean handled = FALSE;
	for (gsize i = 0; i < n_types; i++) {
		for (int j = 0; j < imagewindow_n_supported_types; j++)
			if (types[i] == imagewindow_supported_types[j]) {
				gdk_clipboard_read_value_async(clipboard,
					imagewindow_supported_types[j],
					G_PRIORITY_DEFAULT,
					NULL,
					imagewindow_paste_action_ready,
					win);
				handled = TRUE;
				break;
			}

		if (handled)
			break;
	}
}

static void
imagewindow_magin_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_magin(win->imageui);
}

static void
imagewindow_magout_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_magout(win->imageui);
}

static void
imagewindow_bestfit_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_bestfit(win->imageui);
}

static void
imagewindow_oneone_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui)
		imageui_oneone(win->imageui);
}

static void
imagewindow_reload_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		const char *path = tilesource_get_path(tilesource);

		if (path)
			imagewindow_files_set(win, &path, 1, TRUE);
	}
}

static void
imagewindow_replace_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->iimage)
		classmodel_graphic_replace(CLASSMODEL(win->iimage), GTK_WINDOW(win));
}

static void
imagewindow_saveas_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->iimage)
		classmodel_graphic_save(CLASSMODEL(win->iimage), GTK_WINDOW(win));
}

static void
imagewindow_close_action(GSimpleAction *action,
	GVariant *parameter, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	gtk_window_destroy(GTK_WINDOW(win));
}

static void
imagewindow_fullscreen(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	g_object_set(win,
		"fullscreened", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_control(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	g_object_set(win->displaybar,
		"revealed", g_variant_get_boolean(state),
		NULL);

	/* Disable most visualisation settings if the controls are hidden. It's
	 * much too confusing.
	 */
	if (tilesource)
		g_object_set(tilesource,
			"active", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_info(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	g_object_set(win->info_bar,
		"revealed", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

// is an image being background-loaded?
static gboolean
imagewindow_loading(Imagewindow *win)
{
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		gboolean loaded;

		g_object_get(tilesource, "loaded", &loaded, NULL);
		return !loaded;
	}

	return FALSE;
}

static void
imagewindow_next_image(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	// if there's a background load active, do nothing
	// we want to prevent many bg loads queueing up
	if (imagewindow_loading(win))
		return;

	if (win->n_files > 0) {
		win->transition = GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT;
		win->current_file = (win->current_file + 1) % win->n_files;
		iimage_replace(win->iimage, win->files[win->current_file]);
		symbol_recalculate_all();
	}
}

static void
imagewindow_prev_image(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	// if there's a background load active, then do nothing
	// we want to prevent many bg loads queueing up
	if (imagewindow_loading(win))
		return;

	if (win->n_files > 0) {
		win->transition = GTK_STACK_TRANSITION_TYPE_SLIDE_RIGHT;
		win->current_file =
			(win->current_file + win->n_files - 1) % win->n_files;
		iimage_replace(win->iimage, win->files[win->current_file]);
		symbol_recalculate_all();
	}
}

static void
imagewindow_next(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		int n_pages = tilesource->n_pages;
		int page = VIPS_CLIP(0, tilesource->page, n_pages - 1);

		g_object_set(tilesource,
			"page", (page + 1) % n_pages,
			NULL);
	}
}

static void
imagewindow_prev(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource) {
		int n_pages = tilesource->n_pages;
		int page = VIPS_CLIP(0, tilesource->page, n_pages - 1);

		g_object_set(tilesource,
			"page", page == 0 ? n_pages - 1 : page - 1,
			NULL);
	}
}

static void
imagewindow_scale(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->imageui) {
		if (!imageui_scale(win->imageui))
			imagewindow_error(win);
	}
}

static void
imagewindow_log(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"log", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_icc(GSimpleAction *action, GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"icc", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_falsecolour(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);

	if (tilesource)
		g_object_set(tilesource,
			"falsecolour", g_variant_get_boolean(state),
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_preserve(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	g_object_set(win->displaybar,
		"preserve", g_variant_get_boolean(state),
		NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_mode(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	Tilesource *tilesource = imagewindow_get_tilesource(win);
	const gchar *str = g_variant_get_string(state, NULL);
	TilesourceMode mode =
		vips_enum_from_nick("nip4", TILESOURCE_MODE_TYPE, str);

	if (tilesource)
		g_object_set(tilesource,
			"mode", mode,
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_background(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	const gchar *str = g_variant_get_string(state, NULL);
	TilecacheBackground background =
		vips_enum_from_nick("vipsdisp", TILECACHE_BACKGROUND_TYPE, str);

	if (win->imageui)
		g_object_set(win->imageui,
			"background", background,
			NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_reset(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	imagewindow_reset_view(win);
}

static void
imagewindow_properties(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);
	gboolean revealed = g_variant_get_boolean(state);

#ifdef DEBUG
	printf("imagewindow_properties:\n");
#endif /*DEBUG*/

	g_object_set(win->properties,
		"revealed", revealed,
		NULL);

	g_simple_action_set_state(action, state);
}

static void
imagewindow_region_action(GSimpleAction *action,
	GVariant *state, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	if (win->action_view &&
		IS_REGIONVIEW(win->action_view)) {
		const char *name = g_action_get_name(G_ACTION(action));
		Regionview *regionview = REGIONVIEW(win->action_view);
		Row *row = HEAPMODEL(regionview->classmodel)->row;
		Workspace *ws = row->ws;

		if (g_str_equal(name, "region-duplicate")) {
			row_select(row);
			if (!workspace_selected_duplicate(ws))
				workspace_show_error(ws);
			workspace_deselect_all(ws);

			symbol_recalculate_all();
		}
		else if (g_str_equal(name, "region-reset")) {
			(void) icontainer_map_all(ICONTAINER(row),
				(icontainer_map_fn) model_clear_edited, NULL);

			symbol_recalculate_all();
		}
		else if (g_str_equal(name, "region-saveas")) {
			Model *graphic = row->child_rhs->graphic;

			classmodel_graphic_save(CLASSMODEL(graphic), GTK_WINDOW(win));
		}
		else if (g_str_equal(name, "region-delete"))
			IDESTROY(row->sym);

		win->action_view = NULL;
	}
}

static GActionEntry imagewindow_entries[] = {
	{ "copy", imagewindow_copy_action },
	{ "paste", imagewindow_paste_action },

	{ "magin", imagewindow_magin_action },
	{ "magout", imagewindow_magout_action },
	{ "bestfit", imagewindow_bestfit_action },
	{ "oneone", imagewindow_oneone_action },

	{ "reload", imagewindow_reload_action },
	{ "replace", imagewindow_replace_action },
	{ "saveas", imagewindow_saveas_action },
	{ "close", imagewindow_close_action },

	{ "fullscreen", action_toggle, NULL, "false", imagewindow_fullscreen },
	{ "control", action_toggle, NULL, "false", imagewindow_control },
	{ "info", action_toggle, NULL, "false", imagewindow_info },
	{ "properties", action_toggle, NULL, "false", imagewindow_properties },

	{ "next_image", imagewindow_next_image },
	{ "prev_image", imagewindow_prev_image },

	{ "next", imagewindow_next },
	{ "prev", imagewindow_prev },
	{ "scale", imagewindow_scale },
	{ "log", action_toggle, NULL, "false", imagewindow_log },
	{ "icc", action_toggle, NULL, "false", imagewindow_icc },
	{ "falsecolour", action_toggle, NULL, "false", imagewindow_falsecolour },
	{ "preserve", action_toggle, NULL, "false", imagewindow_preserve },
	{ "mode", action_radio, "s", "'multipage'", imagewindow_mode },
	{ "background", action_radio, "s", "'checkerboard'",
		imagewindow_background },

	{ "reset", imagewindow_reset },

	{ "region-duplicate", imagewindow_region_action },
	{ "region-reset", imagewindow_region_action },
	{ "region-saveas", imagewindow_region_action },
	{ "region-delete", imagewindow_region_action },

};

static void
imagewindow_properties_leave(GtkEventControllerFocus *self,
	gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	gboolean revealed;

	g_object_get(win->properties,
		"revealed", &revealed,
		NULL);

	// if the props pane had the focus and it's being hidden, we must refocus
	if (!revealed)
		gtk_widget_grab_focus(GTK_WIDGET(win->imageui));
}

static gboolean
imagewindow_dnd_drop(GtkDropTarget *target,
	const GValue *value, double x, double y, gpointer user_data)
{
	Imagewindow *win = IMAGEWINDOW(user_data);

	return value_to_filename(value, imagewindow_paste_filename, win);
}

static void
imagewindow_init(Imagewindow *win)
{
	GtkEventController *controller;

#ifdef DEBUG
	puts("imagewindow_init");
#endif /*DEBUG*/

	win->settings = g_settings_new(APPLICATION_ID);
	win->transition = GTK_STACK_TRANSITION_TYPE_NONE;

	gtk_widget_init_template(GTK_WIDGET(win));

	g_object_set(win->displaybar,
		"image-window", win,
		NULL);
	g_object_set(win->info_bar,
		"image-window", win,
		NULL);

	g_action_map_add_action_entries(G_ACTION_MAP(win),
		imagewindow_entries, G_N_ELEMENTS(imagewindow_entries),
		win);

	/* We need to know if the props pane has the focus so we can refocus on
	 * hide.
	 */
	controller = GTK_EVENT_CONTROLLER(gtk_event_controller_focus_new());
	g_signal_connect(controller, "leave",
		G_CALLBACK(imagewindow_properties_leave), win);
	gtk_widget_add_controller(win->properties, controller);

	g_settings_bind(win->settings, "control",
		G_OBJECT(win->displaybar),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind(win->settings, "info",
		G_OBJECT(win->info_bar),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind(win->settings, "properties",
		G_OBJECT(win->properties),
		"revealed",
		G_SETTINGS_BIND_DEFAULT);

	/* We are a drop target for filenames and images.
	 */
	controller = GTK_EVENT_CONTROLLER(
		gtk_drop_target_new(G_TYPE_INVALID, GDK_ACTION_COPY));
	gtk_drop_target_set_gtypes(GTK_DROP_TARGET(controller),
		imagewindow_supported_types,
		imagewindow_n_supported_types);
	g_signal_connect(controller, "drop",
		G_CALLBACK(imagewindow_dnd_drop), win);
	gtk_widget_add_controller(win->main_box, controller);

	/* We can't be a drag source, we use drag for pan. Copy/paste images out
	 * instead.
	 */

	/* Menu state from settings.
	 */
	set_state(GTK_WIDGET(win), win->settings, "properties");
	set_state(GTK_WIDGET(win), win->settings, "control");
	set_state(GTK_WIDGET(win), win->settings, "info");

	// some kind of gtk bug? hexpand on properties can't be set from .ui or in
	// properties.c, but must be set after adding to a parent
	g_object_set(win->properties, "hexpand", FALSE, NULL);
}

static void
imagewindow_pressed(GtkGestureClick *gesture,
	guint n_press, double x, double y, Imagewindow *win)
{
	Imageui *imageui = win->imageui;

	GtkWidget *menu;
	Regionview *regionview;

	menu = NULL;
	if (imageui &&
		(regionview = imageui_pick_regionview(imageui, x, y))) {
		menu = win->region_menu;
		win->action_view = VIEW(regionview);
	}
	else {
		menu = win->right_click_menu;
		win->action_view = NULL;
	}

	gtk_popover_set_pointing_to(GTK_POPOVER(menu),
		&(const GdkRectangle){ x, y, 1, 1 });

	gtk_popover_popup(GTK_POPOVER(menu));
}

static void
imagewindow_class_init(ImagewindowClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	BIND_RESOURCE("imagewindow.ui");

	BIND_VARIABLE(Imagewindow, right_click_menu);
	BIND_VARIABLE(Imagewindow, prev);
	BIND_VARIABLE(Imagewindow, next);
	BIND_VARIABLE(Imagewindow, refresh);
	BIND_VARIABLE(Imagewindow, title);
	BIND_VARIABLE(Imagewindow, subtitle);
	BIND_VARIABLE(Imagewindow, gears);
	BIND_VARIABLE(Imagewindow, error_bar);
	BIND_VARIABLE(Imagewindow, error_label);
	BIND_VARIABLE(Imagewindow, main_box);
	BIND_VARIABLE(Imagewindow, stack);
	BIND_VARIABLE(Imagewindow, properties);
	BIND_VARIABLE(Imagewindow, displaybar);
	BIND_VARIABLE(Imagewindow, info_bar);
	BIND_VARIABLE(Imagewindow, region_menu);

	BIND_CALLBACK(imagewindow_pressed);
	BIND_CALLBACK(imagewindow_error_clicked);

	gobject_class->dispose = imagewindow_dispose;

	imagewindow_signals[SIG_STATUS_CHANGED] = g_signal_new(
		"status-changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	imagewindow_signals[SIG_CHANGED] = g_signal_new("changed",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	imagewindow_signals[SIG_NEW_IMAGE] = g_signal_new("new-image",
		G_TYPE_FROM_CLASS(class),
		G_SIGNAL_RUN_LAST,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	GType supported_types[] = {
		GDK_TYPE_FILE_LIST,
		G_TYPE_FILE,
		GDK_TYPE_TEXTURE,
		G_TYPE_STRING,
	};

	imagewindow_n_supported_types = VIPS_NUMBER(supported_types);
	imagewindow_supported_types =
		VIPS_ARRAY(NULL, imagewindow_n_supported_types + 1, GType);
	for (int i = 0; i < imagewindow_n_supported_types; i++)
		imagewindow_supported_types[i] = supported_types[i];
}

Imagewindow *
imagewindow_new(App *app)
{
	return g_object_new(IMAGEWINDOW_TYPE, "application", app, NULL);
}

GSettings *
imagewindow_get_settings(Imagewindow *win)
{
	return win->settings;
}

static void
imagewindow_iimage_changed(iImage *iimage, Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_iimage_changed:\n");
#endif /*DEBUG*/

	Imageinfo *imageinfo = iimage->value.ii;
	if (!imageinfo)
		return;

	if (imageinfo_is_from_file(imageinfo)) {
		char filename[VIPS_PATH_MAX];

		Active *active;
		Imageui *imageui;

		// look up the filename in the set of active views
		expand_variables(IOBJECT(imageinfo)->name, filename);
		if ((active = imagewindow_active_lookup_by_filename(win, filename))) {
			imagewindow_active_touch(win, active);
			imageui = active->imageui;
		}
		else {
			// new file image
			g_autoptr(Tilesource) tilesource =
				tilesource_new_from_file(filename);
			if (!tilesource) {
				imagewindow_error(win);
				return;
			}

			imageui = imageui_new(tilesource, iimage);
			if (!imageui) {
				imagewindow_error(win);
				return;
			}

			imagewindow_imageui_add(win, imageui);
		}

		const char *filenames = filename;
		imagewindow_files_set(win, &filenames, 1, FALSE);

		imagewindow_imageui_set_visible(win, imageui);
	}
	else {
		// not a file ... just display the single image
		g_autoptr(Tilesource) tilesource =
			tilesource_new_from_iimage(iimage, 0);

		if (win->imageui) {
			g_object_set(win->imageui,
				"tilesource", tilesource,
				NULL);

			imagewindow_imageui_set_visible(win, win->imageui);
		}
		else {
			Imageui *imageui = imageui_new(tilesource, iimage);
			if (!imageui) {
				imagewindow_error(win);
				return;
			}

			imagewindow_imageui_add(win, imageui);

			imagewindow_imageui_set_visible(win, imageui);
		}

		imagewindow_files_free(win);
	}

	gtk_widget_set_sensitive(win->prev, win->n_files > 1);
	gtk_widget_set_sensitive(win->next, win->n_files > 1);
	gtk_widget_set_sensitive(win->refresh, win->n_files > 0);
}

static void
imagewindow_iimage_destroy(iImage *iimage, Imagewindow *win)
{
#ifdef DEBUG
	printf("imagewindow_iimage_destroy:\n");
#endif /*DEBUG*/

	gtk_window_destroy(GTK_WINDOW(win));
}

void
imagewindow_set_iimage(Imagewindow *win, iImage *iimage)
{
	g_assert(!win->iimage);

#ifdef DEBUG
	printf("imagewindow_set_iimage:\n");
#endif /*DEBUG*/

	FREESID(win->iimage_changed_sid, win->iimage);
	FREESID(win->iimage_destroy_sid, win->iimage);

	win->iimage = iimage;

	win->iimage_changed_sid = g_signal_connect(iimage, "changed",
		G_CALLBACK(imagewindow_iimage_changed), win);
	win->iimage_destroy_sid = g_signal_connect(iimage, "destroy",
		G_CALLBACK(imagewindow_iimage_destroy), win);

	iobject_changed(IOBJECT(iimage));

	displaybar_set_view_settings(DISPLAYBAR(win->displaybar),
		&iimage->view_settings);
}
