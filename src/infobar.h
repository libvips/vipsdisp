/* a status bar for the iamge display window
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

#ifndef __INFOBAR_H
#define __INFOBAR_H

#define INFOBAR_TYPE (infobar_get_type())

G_DECLARE_FINAL_TYPE(Infobar, infobar, NIP4, INFOBAR, GtkWidget)

#define INFOBAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), INFOBAR_TYPE, Infobar))

Infobar *infobar_new(Imagewindow *win);

#endif /* __INFOBAR_H */
