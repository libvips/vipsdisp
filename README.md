# vipsdisp

Vipsdisp can display huge (many, many gigabyte) images quickly and without
using much memory. As well as the usual PNG, JPG, TIF, PDF, SVG, WEBP,
JP2, JXL and GIF, it supports many scientific and technical image formats,
including RAW, SVS, MRXS, OpenEXR, FITS, Matlab, NIfTI, Analyze, and
PFM. It can also load camera RAW images, and UltraHDR.

Vipsdisp supports pixel types from 1 bit mono to 128-bit double precision
complex, you can convert image formats, it support copy-paste and drag-drop,
and it has a range of useful visualisation tools and display modes.

The vipsdisp image widget is [used by the image processing spreadsheet
nip4](https://github.com/jcupitt/nip4) for image display, so that might be
a better program if you want to do more than just view images.

## Screenshots

It all works, though see the TODO list below.

[![Screenshot](screenshots/overview.png)](screenshots/overview.png)

[![Screenshot](screenshots/visualisation.png)](screenshots/visualisation.png)

[![Screenshot](screenshots/save-as.png)](screenshots/save-as.png)

[![Screenshot](screenshots/animation.png)](screenshots/animation.png)

https://youtu.be/KSoBkO2_HtE

## Install

This program is on flathub, see:

https://flathub.org/apps/org.libvips.vipsdisp

Just click "install", or enter:

```shell
flatpak install flathub org.libvips.vipsdisp
```

It's also on homebrew for macOS, and there are [x64 and arm64 Windows binaries 
for each release](https://github.com/jcupitt/vipsdisp/releases).

## Features

* It supports many scientific and technical image formats, including TIFF,
  WEBP, JP2K, JXL, RAW, HEIC, AVIF, PNG, JPEG, SVS, MRXS, OpenEXR, GIF, PDF, 
  UHDR, SVG, FITS, Matlab, NIfTI, Analyze, etc. It supports many numeric pixel 
  types, any number of image bands, many colour spaces.

* It doesn't need to keep the whole image in memory. It will only read parts 
  that it needs for display, and it understands most pyramidal image formats.
  This means you can open and view huge images quickly.

* It has threaded, asynchronous display repaint, so pixels are
  computed in the background by a pool of workers. The interface stays live
  even under very heavy load.

* It keeps a sparse pyramid of computed tiles as textures on the GPU. Each
  frame, it computes the set of visible tiles, and then the GPU scales,
  positions and composites just those tiles to the screen. CPU load should
  be low (except for the background workers heh). Hold down i (for "in")
  or + to do a smooth zoom on the cursor. If you press "d" it toggles a
  debug display mode which shows the tiles being computed.

* It supports high-DPI displays, so selecting 1:1 zoom will map one image
  pixel to one display hardware pixel. 

* Select *Display control bar* from the top-right menu and a useful
  set of visualization options appear. It supports four main display modes:
  Toilet roll (sorry), Multipage, Animated, and Pages as Bands.

* In Toilet roll mode, a multi-page image is presented as a tall, thin strip
  of images. In Multipage, you see a single page at a time, with a page-select
  spinner (you can also use the `crtl-<` and `ctrl->` keys to flip pages). In
  animated mode, pages flip automatically on a timeout. In pages-as-bands
  mode, many-page single-band images (eg. OME-TIFF) are presented as a 
  single colour image.

* You can select falsecolour and log-scale filters, useful for many scientific
  images. Scale and offset sliders let you adjust image brightness to see into
  darker areas (useful for HDR and many scientific images).

* Select Save as to write an image. It can write most common formats, and lets
  you set file save options. It can write things like DeepZoom pyramids, PFM,
  OpenEXR, and so on.

* Select Properties (alt-Enter) to see image metadata. It can display large
  amounts of metadata (useful for eg. DICOM) with filtering.

* Use alt-Left and alt-Right to move between the set of images being
  displayed, handy for browsing a directory of images. If you drag or load a
  set of images, it'll flip between the images in the set. If you drag or load
  a single image, it'll flip between all the images in that directory.

* It keeps the most recent three views live, so you can flip between them very
  quickly, and all view settings are preserved. This is handy for comparing
  details on two images.

* `Duplicate window` crtl-D makes a copy of the window, so you can compare two
  images side by side. 

* It supports copy/paste and drag/drop of filenames, sets of filenames, and
  textures. You can paste from the screenshot tool, or drag-drop images from
  your file browser, for example.

* It uses the gtk4 GUI toolkit, so the interface is fast, attractive
  and nicely animated. The image is rendered with the GPU, so display ought to
  be quick.

## Shortcuts

* Cursor keys to scroll around
* Cursor keys plus shift to move by a screen size
* Cursor keys plus ctrl to move to image edges
* Number keys to pick a particular magnification
* Ctrl + number keys to pick a particular zoom out
* 0 for best fit
* d, to toggle debug rendering mode
* i, + / o, - to zoom in and out
* ctrl-< / ctrl->. prev page, next page
* alt-Left / alt-Right. prev image, next image
* Mouse drag to pan
* Mousewheel to zoom
* Mousewheel + shift/ctrl to pan
* ^O replace image
* ^S save image
* ^N new viewer
* ^D duplicate view
* alt-Enter toggle properties
* ^C / ^V copy paste of filenames, lists of filenames and textures
* F11 fullscreen

Additionally, you can drag and drop filenames, lists of filenames and
textures.

## Build from source

```shell
cd vipsdisp
meson setup build --prefix=~/vips
cd build
ninja
ninja install
```

And to run:

```shell
export GSETTINGS_SCHEMA_DIR=/home/john/vips/share/glib-2.0/schemas
vipsdisp ~/pics/k2.jpg
```

## Version bump checklist

Version needs updating in the following places:

- **`CHANGELOG.md`**

- **`meson.build`**

- **`org.libvips.vipsdisp.metainfo.xml`** and some release notes and a date as
  well. Be exttremely careful with date formatting, and include leading zeros
  on the day and month.

- **`org.libvips.vipsdisp.json`** needs the version number as a git tag.

## flatpak

Add the `flathub` repo:

```shell
flatpak remote-add --if-not-exists \
  flathub https://flathub.org/repo/flathub.flatpakrepo
```

Install the gtk4 SDK and runtime:

```shell
flatpak install org.gnome.Sdk//49
flatpak install org.gnome.Platform//49
```

Allow file. Recent security changes to git will cause submodule checkout
to fail inside flatpak. If you get errors like `fatal: transport 'file'
not allowed`, re-enable file transport with:

```shell
git config --global protocol.file.allow always
```

Build and try running it:

```shell
flatpak-builder --force-clean --user --install build-dir org.libvips.vipsdisp.json
flatpak run org.libvips.vipsdisp ~/pics/k2.jpg
```

Force a complete redownload and rebuild (should only rarely be necessary) with:

```shell
rm -rf .flatpak-builder
```

Check the files that are in the flatpak you built with:

```shell
ls build-dir/files
```

Uninstall with:

```shell
flatpak uninstall vipsdisp
```

## Notes on flatpak build process

- niftiio is annoying to build, skip it.

- x265 is annoying to build, skip it

- we skip imagemagick as well, too huge

## Packaging for flathub

Install the appdata checker:

```shell
flatpak install flathub org.freedesktop.appstream-glib
flatpak run org.freedesktop.appstream-glib validate org.libvips.vipsdisp.metainfo.xml
```

Also:

```shell
desktop-file-validate org.libvips.vipsdisp.desktop 
```

## Uploading to flathub

Make a PR on:

        https://github.com/flathub/org.libvips.vipsdisp 

then check the build status here:

        https://flathub.org/builds/#/apps/org.libvips.vipsdisp

On success, merge to master.

## Build for Windows

Use the libvips Windows build system:

https://github.com/libvips/build-win64-mxe/tree/master

Clone and run:

```
./build.sh --target x86_64-w64-mingw32.shared vipsdisp
```

## TODO

- skip unknown files on next / prev?

    - need to add the test to next-image, not glob, since we can't test the
      whole dir on startup

    - need to use next-image to pick the start image (not always the first)

    - need to *not* do this if only one file specified, eg. `vipsdisp x.txt`

- ^C during a slow load leaves a file in /tmp, can we improve this?

- batch save?

- have a look at the new 4.14 `gtk_print_dialog_new()`

    works and is in branch add-print-dialog, but it needs 4.14, and the new
    print API is still unstable

- allow eg. "vipsdisp x.svg[scale=10]", the load dialog should have a
  "load options" expander

- can we move `new_from_file` into a bg thread? the GUI will pause on huge
  SVGs on zoom change right now

    very difficult with the current structure -- we'd need to separate
    tilesource and tilecache completely, and perhaps link them with signals

- need to separate page and zoom for ome-tiff, since we have many-page
  subifd pyramids

    - useful for fixing PDF zoom in the way we fixed SVG zoom too

- zooming:

    - better pinch zoom support

        we should take the coordinates of the pinch gesture into account

        can test this now I have a multitouch trackpad

    - we could shrink tiles on zoom out (we only expand tiles now)

    - before we can do fancy PDF zooming, we'll need to split page and zoom in
      tile_source_open()

- how should we handle images which include labels, macros, thumbnails?

    - load options? save options? 

    - display images in properties? perhaps a thumbnail? set
      attach-associated?

      appear as eg.:

        openslide.associated.thumbnail    1024x732 uchar, 4 bands, srgb

