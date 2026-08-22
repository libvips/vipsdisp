#ifndef __PACKAGE_H
#define __PACKAGE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /*HAVE_CONFIG_H*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define APP_PATH "/org/libvips/" PACKAGE

#include <gtk/gtk.h>

#include <vips/vips.h>

/* i18n placeholder.
 */
#define _(S) (S)
#define GETTEXT_PACKAGE PACKAGE

/* The tile size for image rendering.
 */
#define TILE_SIZE (256)

/* Cache size -- enough for two 4k displays.
 */
#define MAX_TILES (2 * (4096 / TILE_SIZE) * (2048 / TILE_SIZE))

#define FREESID(SID, OBJ) \
	G_STMT_START \
	{ \
		if ((SID) && (OBJ)) { \
			g_signal_handler_disconnect((OBJ), (SID)); \
			(SID) = 0; \
		} \
	} \
	G_STMT_END

#include "enumtypes.h"
#include "gtkutil.h"
#include "ientry.h"
#include "tslider.h"
#include "vipsdispapp.h"
#include "vipsdispmarshal.h"
#include "tile.h"
#include "tilesource.h"
#include "tilecache.h"
#include "imagedisplay.h"
#include "imageui.h"
#include "imagewindow.h"
#include "infobar.h"
#include "displaybar.h"
#include "saveoptions.h"
#include "properties.h"
#include "fuzzy.h"

#endif /*__PACKAGE_H*/
