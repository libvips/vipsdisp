/* the display control widgets
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

#ifndef __DISPLAYBAR_H
#define __DISPLAYBAR_H

/* All the view settings we save and restore.
 */
typedef struct _ViewSettings {
	gboolean valid;

	TilesourceMode mode;
	double scale;
	double offset;
	int page;
	gboolean falsecolour;
	gboolean log;
	gboolean icc;
	gboolean active;
	TilecacheBackground background;
} ViewSettings;

#define DISPLAYBAR_TYPE (displaybar_get_type())

G_DECLARE_FINAL_TYPE(Displaybar, displaybar, NIP4, DISPLAYBAR, GtkWidget)

#define DISPLAYBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), DISPLAYBAR_TYPE, Displaybar))

Displaybar *displaybar_new(Imagewindow *win);
ViewSettings *displaybar_get_view_settings(Displaybar *displaybar);
void displaybar_set_view_settings(Displaybar *displaybar,
	ViewSettings *view_settings);

#endif /* __DISPLAYBAR_H */
