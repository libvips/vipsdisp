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

#ifndef __IMAGEWINDOW_H
#define __IMAGEWINDOW_H

#define IMAGEWINDOW_TYPE (imagewindow_get_type())
#define IMAGEWINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGEWINDOW_TYPE, Imagewindow))
#define IMAGEWINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGEWINDOW_TYPE, ImagewindowClass))
#define IS_IMAGEWINDOW(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEWINDOW_TYPE))
#define IS_IMAGEWINDOW_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEWINDOW_TYPE))
#define IMAGEWINDOW_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGEWINDOW_TYPE, ImagewindowClass))

G_DECLARE_FINAL_TYPE(Imagewindow, imagewindow,
	NIP4, IMAGEWINDOW, GtkApplicationWindow)

Imagewindow *imagewindow_new(App *app);

double imagewindow_get_zoom(Imagewindow *win);
void imagewindow_get_mouse_position(Imagewindow *win,
	double *image_x, double *image_y);
Tilesource *imagewindow_get_tilesource(Imagewindow *win);
GtkWidget *imagewindow_get_main_box(Imagewindow *win);
GSettings *imagewindow_get_settings(Imagewindow *win);
Imageui *imagewindow_get_imageui(Imagewindow *win);

void imagewindow_set_iimage(Imagewindow *win, iImage *iimage);

#endif /* __IMAGEWINDOW_H */
