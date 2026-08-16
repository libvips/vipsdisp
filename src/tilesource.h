/* generate tiles
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

#ifndef __TILESOURCE_H
#define __TILESOURCE_H

#define TILESOURCE_TYPE (tilesource_get_type())
#define TILESOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), TILESOURCE_TYPE, Tilesource))
#define TILESOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), TILESOURCE_TYPE, TilesourceClass))
#define IS_TILESOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), TILESOURCE_TYPE))
#define IS_TILESOURCE_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), TILESOURCE_TYPE))
#define TILESOURCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS((obj), TILESOURCE_TYPE, TilesourceClass))

/* The modes of image display we support.
 *
 * UNSET
 *
 *  No saved value (should be set by sniffing the image).
 *
 * TOILET_ROLL
 *
 *	Just show the whole image (no crop). Page control disabled. Reload on
 *	mag change if there's a pyramid.
 *
 * MULTIPAGE
 *
 *	Behaviour depends on Tilesource details:
 *
 *	->page_pyramid
 *
 *		Disable page controls. No crop. Reload on mag change.
 *
 *	->pages_same_size && ->delay && ->n_pages > 1
 *
 *		Enable page controls, crop in display conversion
 *		to select page.
 *
 *	->n_pages > 1
 *
 *		Enable page control iff > 1 page.
 *
 * ANIMATED
 *
 *	Just like MULTIPAGE, except page flip is driven by a timeout.
 *
 * PAGES_AS_BANDS
 *
 *	Just like toilet roll, except that we chop the image into pages and
 *	bandjoin them all. Handy for OME-TIFF, which has a one-band image
 *	in each page.
 */
typedef enum _TilesourceMode {
	TILESOURCE_MODE_UNSET,
	TILESOURCE_MODE_TOILET_ROLL,
	TILESOURCE_MODE_MULTIPAGE,
	TILESOURCE_MODE_ANIMATED,
	TILESOURCE_MODE_PAGES_AS_BANDS,
	TILESOURCE_MODE_LAST
} TilesourceMode;

/* Max number of levels we allow in a pyramidal image.
 */
#define MAX_LEVELS (256)

typedef struct _Tilesource {
	GObject parent_instance;

	/* We have four versions of the image we are supplying tiles for :(
	 *
	 * - base ... the VipsImage we were given to display, or the result of a
	 *   plain vips_image_new_from_file() on the filename, if we were given a
	 *   filename to display. See tilesource_new_from_file() and
	 *   tilesource_new_from_image().
	 *
	 * - image ... an image for the highest res pyr level, so after view mode
	 *   transforms like histplot, or pages-to-bands. This is used for the
	 *   infobar pixel display. width/height/bands/format etc can be
	 *   different
	 *
	 * - display ... the image that makes tiles for the currently loaded
	 *   level.
	 *
	 * - rgb ... the display image converted to rgb or rgba.
	 */

	/* The loader and the file we have loaded from. We may need to reload
	 * on a zoom or page change. We can't use a VipsSource since they are not
	 * cached and we'd get repeated decode on page change.
	 *
	 * filename is NULL for a computed image.
	 */
	const char *loader;
	char *filename;

	/* Either the VipsImage we were given to display, or the result of a
	 * no-param vips_image_new_from_file() on the filename we are displaying.
	 *
	 * Some properties of this base image
	 */
	VipsImage *base;
	int n_pages;
	int page_height;
	int n_subifds;
	int *delay;
	int n_delay;

	/* This is a TIFF subifd pyramid.
	 */
	gboolean subifd_pyramid;

	/* This is a page pyramid (TIFF, jp2k etc.).
	 */
	gboolean page_pyramid;

	/* If all the pages are the same size and format, we can display as a
	 * toilet roll or animated.
	 */
	gboolean pages_same_size;

	/* If all the pages are the same size and format, and also all mono,
	 * we can display pages as bands.
	 */
	gboolean all_mono;

	/* The pyramid structure in the file. This isn't the same as the pyr of
	 * the image being displayed -- OME-TIFFs can be multipage and pyr, for
	 * example.
	 */
	int level_count;
	int level_width[MAX_LEVELS];
	int level_height[MAX_LEVELS];

	/* Our display mode.
	 */
	TilesourceMode mode;

	/* Display transform parameters.
	 */
	int page;
	gboolean active;
	double scale;
	double offset;
	gboolean falsecolour;
	gboolean log;
	gboolean icc;

	/* The current z for image. We need to rebuild the pipeline on z changes.
	 */
	int current_z;

	/* If set, there's no background display. Synchronous mode is used by eg.
	 * the Colour widget to convert between colourspaces.
	 */
	gboolean synchronous;

	/* When we load an SVG we need to pick a base scale to fir it within 32k x
	 * 32k. Pyr level scales need to be relative to this.
	 */
	double zoom;

	/* The base image after the first stage of conversion for display (just
	 * geometric conversion, no conversion to rgb).
	 */
	VipsImage *image;
	VipsImage *mask;
	VipsRegion *image_region;
	VipsRegion *mask_region;

	/* The size of the level0 image in the current view mode. This is not the
	 * same as level_width[0], since eg. we might be looking at one page of a
	 * larger image.
	 */
	int image_width;
	int image_height;

	/* @image converted to rgb for painting.
	 */
	VipsImage *rgb;
	VipsRegion *rgb_region;

	/* For animations, the timeout we use for page flip.
	 */
	guint page_flip_id;

	/* TRUE when the image has fully loaded (ie. postload has fired) and we
	 * can start looking at pixels.
	 */
	gboolean loaded;

	/* TRUE when the image is visible. GIF animations (for example) pause for
	 * invisible images.
	 */
	gboolean visible;

	/* Set on a background load error, eg. cancel etc.
	 */
	int load_error;
	char *load_message;

	/* Render priority ... lower for thumbnails.
	 */
	int priority;

} Tilesource;

typedef struct _TilesourceClass {
	GObjectClass parent_class;

	/* Signal image load.
	 */
	void (*preeval)(Tilesource *tilesource, VipsProgress *progress);
	void (*eval)(Tilesource *tilesource, VipsProgress *progress);
	void (*posteval)(Tilesource *tilesource, VipsProgress *progress);

	/* Everything has changed, so image geometry and pixels. Perhaps a
	 * new page in a multi-page TIFF where pages change in size.
	 */
	void (*changed)(Tilesource *tilesource);

	/* All tiles have changed, but image geometry has not. Falsecolour,
	 * page flip in a GIF, etc.
	 */
	void (*tiles_changed)(Tilesource *tilesource);

	/* A tile has been computed by a bg worker and must be collected from the
	 * end of the pipeline.
	 */
	void (*collect)(Tilesource *tilesource, VipsRect *area, int z);

	/* The page has changed. Just for updating the page number display.
	 */
	void (*page_changed)(Tilesource *tilesource);

	/* The image has loaded and you can fetch pixels (ie. you should repaint,
	 * but don't invalidate any cache, since the pixels will not have changed
	 * since last time).
	 */
	void (*loaded)(Tilesource *tilesource);

} TilesourceClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Tilesource, g_object_unref)

GType tilesource_get_type(void);

Tilesource *tilesource_new_from_file(const char *filename);
Tilesource *tilesource_new_from_image(VipsImage *image);
Tilesource *tilesource_new_from_imageinfo(Imageinfo *ii);
Tilesource *tilesource_new_from_iimage(iImage *iimage, int priority);
gboolean tilesource_has_imageinfo(Tilesource *tilesource, Imageinfo *ii);

void tilesource_background_load(Tilesource *tilesource);

int tilesource_request_tile(Tilesource *tilesource, Tile *tile);
int tilesource_collect_tile(Tilesource *tilesource, Tile *tile);

const char *tilesource_get_path(Tilesource *tilesource);
GFile *tilesource_get_file(Tilesource *tilesource);

VipsImage *tilesource_get_image(Tilesource *tilesource);
VipsImage *tilesource_get_base_image(Tilesource *tilesource);
Tilesource *tilesource_duplicate(Tilesource *tilesource);
void tilesource_changed(Tilesource *tilesource);

void tilesource_set_synchronous(Tilesource *source, gboolean synchronous);

#endif /*__TILESOURCE_H*/
