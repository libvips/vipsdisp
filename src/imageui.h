/* a scrolled window containing an imagedisplay
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

#ifndef __IMAGEUI_H
#define __IMAGEUI_H

#define IMAGEUI_TYPE (imageui_get_type())
#define IMAGEUI(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IMAGEUI_TYPE, Imageui))
#define IMAGEUI_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IMAGEUI_TYPE, ImageuiClass))
#define IS_IMAGEUI(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IMAGEUI_TYPE))
#define IS_IMAGEUI_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IMAGEUI_TYPE))
#define IMAGEUI_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IMAGEUI_TYPE, ImageuiClass))

G_DECLARE_FINAL_TYPE(Imageui, imageui, NIP4, IMAGEUI, GtkWidget)

void imageui_queue_draw(Imageui *imageui);
void imageui_add_regionview(Imageui *imageui, Regionview *regionview);
void imageui_remove_regionview(Imageui *imageui, Regionview *regionview);

Tilesource *imageui_get_tilesource(Imageui *imageui);
double imageui_get_scale(Imageui *imageui);
void imageui_get_mouse_position(Imageui *imageui,
	double *image_x, double *image_y);

gboolean imageui_snap_point(Imageui *imageui, int x, int y, int *sx, int *sy);
gboolean imageui_snap_rect(Imageui *imageui, VipsRect *in, VipsRect *out);

Regionview *imageui_pick_regionview(Imageui *imageui, int x, int y);
double imageui_get_zoom(Imageui *imageui);
void imageui_bestfit(Imageui *imageui);
void imageui_magin(Imageui *imageui);
void imageui_magout(Imageui *imageui);
void imageui_oneone(Imageui *imageui);
gboolean imageui_scale(Imageui *imageui);

Imageui *imageui_new(Tilesource *tilesource, iImage *iimage);
Imageui *imageui_duplicate(Tilesource *tilesource, Imageui *old_imageui);

void imageui_image_to_gtk(Imageui *imageui,
	double x_image, double y_image, double *x_gtk, double *y_gtk);
void imageui_gtk_to_image(Imageui *imageui,
	double x_gtk, double y_gtk, double *x_image, double *y_image);
void imageui_image_to_gtk_rect(Imageui *imageui, VipsRect *in, VipsRect *out);
void imageui_gtk_to_image_rect(Imageui *imageui, VipsRect *in, VipsRect *out);

#endif /* __IMAGEUI_H */
