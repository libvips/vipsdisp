marshal = gnome.genmarshal(
  'vipsdispmarshal',
  prefix: 'app',
  sources: 'vipsdispmarshal.list',
)

resources = gnome.compile_resources(
  'vipsdisp-gresources',
  'gtk/vipsdisp.gresources.xml',
  source_dir: 'gtk',
)

# compile in an icon for a windows exe
if host_machine.system() == 'windows'
  windows = import('windows')
  resources += windows.compile_resources(
    'vipsdisp.rc',
    depend_files: 'vipsdisp.ico',
  )
endif

# headers for enum scanning
headers = files(
  'tilesource.h',
  'tilecache.h',
  'paintbox.h',
)

sources = files(
  'app.c',
  'displaybar.c',
  'draw_mask.c',
  'fuzzy.c',
  'gtkutil.c',
  'ientry.c',
  'imagedisplay.c',
  'imageui.c',
  'imagewindow.c',
  'infobar.c',
  'paintbox.c',
  'main.c',
  'properties.c',
  'saveoptions.c',
  'tile.c',
  'tilecache.c',
  'tilesource.c',
  'tslider.c',
)

enumtypes = gnome.mkenums(
  'enumtypes',
  sources: headers,
  h_template: 'enumtypes.h.in',
  c_template: 'enumtypes.c.in',
)

executable('vipsdisp',
  sources: [enumtypes, marshal, resources, sources],
  dependencies: vipsdisp_deps,
  win_subsystem: 'windows',
  install: true,
)
