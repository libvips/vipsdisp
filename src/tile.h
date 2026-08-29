/* A tile fetched from libvips and painted with gtk.
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

#ifndef __TILE_H
#define __TILE_H

#define TYPE_TILE (tile_get_type())
#define TILE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TILE, Tile))
#define TILE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TILE, TileClass))
#define IS_TILE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TILE))
#define IS_TILE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_TILE))
#define TILE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TILE, TileClass))

typedef struct _Tile {
	GObject parent_instance;

	/* Time we last used the tile, for cache flushing.
	 */
	guint time;

	/* The z layer the tile sits at.
	 */
	int z;

	/* The tile rect, in leval coordinates and in level0 coordinates.
	 */
	VipsRect bounds;
	VipsRect bounds0;

	/* TRUE if we think the texture is up to date.
	 */
	gboolean valid;

	GBytes *bytes;
	GdkTexture *texture;
} Tile;

typedef struct _TileClass {
	GObjectClass parent_class;

} TileClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Tile, g_object_unref)

GType tile_get_type(void);

int tile_get_time(void);
void tile_invalidate(Tile *tile);
void tile_touch(Tile *tile);

/* Make a new tile on the level. left and top are in level0 coordinates.
 */
Tile *tile_new(int left, int top, int z);

/* Set the texture from the data on a region.
 */
void tile_set_texture(Tile *tile, VipsRegion *region);

/* texture lifetime run by tile ... don't unref.
 */
GdkTexture *tile_get_texture(Tile *tile);

#endif /*__TILE_H*/
