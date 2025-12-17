master

## 4.1.3 17/12/25

- fix load cancel [keyboqrd]

## 4.1.2 02/08/25

- add an icon to the exe on windows

## 4.1.1 31/07/25

- do our own tile snapping if gtk has no snap mechanism 
- high-dpi support
- fix TIFF pyr detection [euzada]

## 4.1.0 26/07/25

- fix a ref leak in tilesource_new_from_file()
- much better focus indicator in imagedisplay
- update from nip4 again to get faster and prettier repaints
- better complex image display
- much better prev / next / refresh handling

## 4.0.0 31/05/25

- much better performance for images which are very tall and thin or very wide 
  and narrow
- better antialiasing [Linden6]
- copy over improvements from nip4 development, including OME handling

## 3.1.0 28/10/24

- better behaviour for image interpretations like MATRIX
- window menu has next-image / prev-image in the top line [euzada]
- add "preserve view settings on new image"
- add animated JXL support

# 3.0.4 1/3/24

- add next image / previous image
- add drag/drop and copy/paste of files, file lists, textures and strings
- add "reload"
- better event handling in imagewindow
- add multifile load
- keep view settings on next/prev image
- use clang-format
- move to gnome SDK v45

# 2.6.3a, 7/1/24

- fix clang build

# 2.6.3, 7/1/24

- properties display [angstyloop]

# 2.6.2, 28/10/23

- better saveoptions sizing
- add flags support to saveoptions
- better discrete zoom animation

## 2.6.1, 12/10/23

- async infobar updates

## 2.6, 7/9/23

- limit bands in pages-as-bands to prevent 3000 page images causing 
  performance issues
- switch to file based image load (not source) to fix caching
- better conversion to RGB
- animate keypress zoom
- use NN sampling for zoom with gtk 4.10+
- add colour management

## 2.5.1, 23/7/23

- fix progress bar (again)

## 2.5.0, 22/7/23

- save options [angstyloop]

## 2.4.1, 21/8/22

- remove stray printfs
- fix scale + offset sliders

## 2.4, 19/8/22

- match zoom direction and step size on scrollwheel with eog
- basic support for pinch zoom [earboxer]
- new display menu items let you pick image background 
- better visualisation of high bit-depth images
- better redraw tree computation
- much faster info bar updates
- update infobar on page flip
- implement SVG zoom
- better OME tiff support

## 2.3, started 20/10/21

- better initial shrink-to-fit logic

## 2.2, started 14/10/21

- press "d" to enable debug rendering
- another reworking of rendering ... should be faster and smoother again
- added animations for pan/zoom
- shrink to fit on load

## 2.1, started 14/10/21

- switch to gtk4-wayland
- better .desktop

## 2.0, started 28/9/21

- new rendering system: much faster, smooth zoom
- added "pages as bands" view mode
- better file format sniffing and pyramid detection

## 1.0

- first gtk4 release!
