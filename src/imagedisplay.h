/* a drawing area that draws an image
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

#ifndef __IMAGEDISPLAY_H
#define __IMAGEDISPLAY_H

#define IMAGEDISPLAY_TYPE (imagedisplay_get_type())
#define IMAGEDISPLAY NIP4_IMAGEDISPLAY

G_DECLARE_FINAL_TYPE(Imagedisplay, imagedisplay,
	NIP4, IMAGEDISPLAY, GtkDrawingArea)

void imagedisplay_image_to_gtk(Imagedisplay *imagedisplay,
	double x_image, double y_image, double *x_gtk, double *y_gtk);
void imagedisplay_gtk_to_image(Imagedisplay *imagedisplay,
	double x_gtk, double y_gtk, double *x_image, double *y_image);
double imagedisplay_get_scale(Imagedisplay *imagedisplay);

Imagedisplay *imagedisplay_new(Tilesource *tilesource);

#endif /* __IMAGEDISPLAY_H */
