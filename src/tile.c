/* a tile for display
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

#include "nip4.h"

/*
#define DEBUG_VERBOSE
#define DEBUG
 */

/* Increment this regularly.
 */
static int tile_ticks = 0;

G_DEFINE_TYPE(Tile, tile, G_TYPE_OBJECT);

static void
tile_dispose(GObject *object)
{
	Tile *tile = (Tile *) object;

#ifdef DEBUG
	printf("tile_dispose: %p\n", object);
#endif /*DEBUG*/

	VIPS_UNREF(tile->texture);
	VIPS_FREEF(g_bytes_unref, tile->bytes);

	G_OBJECT_CLASS(tile_parent_class)->dispose(object);
}

static void
tile_init(Tile *tile)
{
}

static void
tile_class_init(TileClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(class);

	gobject_class->dispose = tile_dispose;
}

/* Get the current time ... handy for mark-sweep.
 */
int
tile_get_time(void)
{
	return tile_ticks;
}

/* The pixels in the region have changed. We must regenerate the texture on
 * next use.
 */
void
tile_invalidate(Tile *tile)
{
	tile->valid = FALSE;
}

/* Update the timestamp on a tile.
 */
void
tile_touch(Tile *tile)
{
	tile->time = tile_ticks++;
}

/* Make a tile on an image. left/top are in level0 coordinates.
 */
Tile *
tile_new(int left, int top, int z)
{
	g_autoptr(Tile) tile = g_object_new(TYPE_TILE, NULL);

	tile->z = z;
	tile->bounds.left = left >> z;
	tile->bounds.top = top >> z;
	tile->bounds.width = TILE_SIZE;
	tile->bounds.height = TILE_SIZE;
	tile->bounds0.left = left;
	tile->bounds0.top = top;
	tile->bounds0.width = TILE_SIZE << z;
	tile->bounds0.height = TILE_SIZE << z;
	tile->valid = FALSE;

	tile_touch(tile);

	return g_steal_pointer(&tile);
}

/* Set the texture from the pixels in a VipsRegion. The region can be less
 * than TILE_SIZE x TILE_SIZE for edge tiles, and can be RGB or RGBA (we
 * always make full size RGBA textures).
 */
void
tile_set_texture(Tile *tile, VipsRegion *region)
{
	g_assert(region->valid.left == tile->bounds.left);
	g_assert(region->valid.top == tile->bounds.top);
	g_assert(region->valid.width <= tile->bounds.width);
	g_assert(region->valid.height <= tile->bounds.height);
	g_assert(region->im->Bands == 3 || region->im->Bands == 4);
	g_assert(region->im->BandFmt == VIPS_FORMAT_UCHAR);
	g_assert(region->im->Type == VIPS_INTERPRETATION_sRGB);

	// textures are immutable, we we must always reallocate, we can't update
	VIPS_FREEF(g_bytes_unref, tile->bytes);
	VIPS_UNREF(tile->texture);

	// always a full tile of RGBA pixels
	gsize length = TILE_SIZE * TILE_SIZE * 4;
	unsigned char *data = g_malloc0(length);

	for (int y = 0; y < region->valid.height; y++) {
		VipsPel *p =
			VIPS_REGION_ADDR(region, region->valid.left, region->valid.top + y);
		VipsPel *q = data + 4 * TILE_SIZE * y;

		if (region->im->Bands == 4)
			memcpy(q, p, VIPS_REGION_SIZEOF_LINE(region));
		else
			// RGB to RGBA
			for (int x = 0; x < region->valid.width; x++) {
				q[0] = p[0];
				q[1] = p[1];
				q[2] = p[2];
				q[3] = 255;

				q += 4;
				p += 3;
			}
	}

	tile->bytes = g_bytes_new_take(data, length);
	tile->texture = gdk_memory_texture_new(TILE_SIZE, TILE_SIZE,
		GDK_MEMORY_R8G8B8A8, tile->bytes, 4 * TILE_SIZE);

	tile->valid = TRUE;
	tile_touch(tile);
}

GdkTexture *
tile_get_texture(Tile *tile)
{
	tile_touch(tile);

	return tile->texture;
}
