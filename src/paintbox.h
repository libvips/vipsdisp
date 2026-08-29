/* the paintbox bar
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

#ifndef __PAINTBOX_H
#define __PAINTBOX_H

// currently selected tool
typedef enum _PaintboxTool {
	PAINTBOX_TOOL_POINTER,
	PAINTBOX_TOOL_BRUSH,
	PAINTBOX_TOOL_LINE,
	PAINTBOX_TOOL_RECT,
	PAINTBOX_TOOL_CIRCLE,
	PAINTBOX_TOOL_SMUDGE,
	PAINTBOX_TOOL_FLOOD_UNTIL,
	PAINTBOX_TOOL_FLOOD_WHILE,
	PAINTBOX_TOOL_TEXT,
	PAINTBOX_TOOL_DROPPER,

	PAINTBOX_TOOL_LAST
} PaintboxTool;

#define PAINTBOX_TYPE (paintbox_get_type())

G_DECLARE_FINAL_TYPE(Paintbox, paintbox, APP, PAINTBOX, GtkWidget)

#define PAINTBOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), PAINTBOX_TYPE, Paintbox))

Paintbox *paintbox_new(Imagewindow *win);
gboolean paintbox_undo(Paintbox *paintbox);
gboolean paintbox_redo(Paintbox *paintbox);

#endif /* __PAINTBOX_H */
