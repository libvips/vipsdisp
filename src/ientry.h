/* a text extry widget with OK, cancel and modified
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

#ifndef __IENTRY_H
#define __IENTRY_H

#define IENTRY_TYPE (ientry_get_type())
#define IENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), IENTRY_TYPE, iEntry))
#define IENTRY_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), IENTRY_TYPE, iEntryClass))
#define IS_IENTRY(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), IENTRY_TYPE))
#define IS_IENTRY_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), IENTRY_TYPE))
#define IENTRY_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), IENTRY_TYPE, iEntryClass))

typedef struct _iEntry {
	GtkWidget parent_instance;

	char *text;				/* Value as set by our caller */

	GtkWidget *entry;

} iEntry;

typedef struct _iEntryClass {
	GtkWidgetClass parent_class;

	void *padding[12];
} iEntryClass;

GType ientry_get_type();
iEntry *ientry_new(void);
void ientry_grab_focus(iEntry *ientry);
void ientry_set_double(iEntry *ientry, int digits, double value);
gboolean ientry_get_double(iEntry *ientry, double *out);

#endif /* __IENTRY_H */
