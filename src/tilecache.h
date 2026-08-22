/* a sparse pyramid of tiles
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
#ifndef __TILECACHE_H
#define __TILECACHE_H

/* The background modes we support.
 */
typedef enum _TilecacheBackground {
	TILECACHE_BACKGROUND_CHECKERBOARD,
	TILECACHE_BACKGROUND_WHITE,
	TILECACHE_BACKGROUND_BLACK,
	TILECACHE_BACKGROUND_LAST
} TilecacheBackground;

#define TILECACHE_TYPE (tilecache_get_type())
#define TILECACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TILECACHE, Tilecache))
#define TILECACHE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TILECACHE, TilecacheClass))
#define IS_TILECACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TILECACHE))
#define IS_TILECACHE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_TILECACHE))
#define TILECACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TILECACHE, TilecacheClass))

typedef struct _Tilecache {
	GObject parent_instance;

	/* Background rendering style.
	 */
	TilecacheBackground background;

	/* Fetch tiles from here.
	 */
	Tilesource *tilesource;

	/* The pyramid levels, with 0 as the full res.
	 */
	int level_width[MAX_LEVELS];
	int level_height[MAX_LEVELS];
	int n_levels;

	/* For each level, a list of all the RGBA tiles on that level. This is the
	 * list that holds the tile references.
	 */
	GSList *tiles[MAX_LEVELS];

	/* The result of the visibility test: for each level, the list of
	 * valid tiles which touch the viewport and which are not
	 * obscured.
	 */
	GSList *visible[MAX_LEVELS];

	/* For each level, the valid but invisible tiles we are
	 * keeping for now on LRU.
	 */
	GSList *free[MAX_LEVELS];

	/* Paint the backdrop with this.
	 */
	GdkTexture *background_texture;

	/* The signals we watch tilesource with.
	 */
	guint tilesource_changed_sid;
	guint tilesource_loaded_sid;
	guint tilesource_tiles_changed_sid;
	guint tilesource_collect_sid;
	guint tilesource_invalidate_area_sid;

} Tilecache;

typedef struct _TilecacheClass {
	GObjectClass parent_class;

} TilecacheClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Tilecache, g_object_unref)

GType tilecache_get_type(void);

Tilecache *tilecache_new();

/* Render the tiles to a snapshot.
 */
void tilecache_snapshot(Tilecache *tilecache, GtkSnapshot *snapshot,
	double scale, double x, double y, graphene_rect_t *paint, gboolean debug);

#endif /*__TILECACHE_H*/
