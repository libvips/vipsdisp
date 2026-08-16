project('vipsdisp', 'c',
  version: '4.1.4',
  license: 'MIT',
  meson_version: '>=0.56',
  default_options: [
    # glib uses this, so we do too
    'c_std=gnu11',
    # do a release (optimised) build by default
    'buildtype=release',
    # turn off asserts etc. in release mode
    'b_ndebug=if-release'
  ]
)

application_id = 'org.libvips.vipsdisp'

pkg = import('pkgconfig')
gnome = import('gnome')

# if we're optimising (eg. release mode) we turn off cast checks and g_asserts
if get_option('optimization') not in ['0', 'g']
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: ['cpp', 'c'])
  add_project_arguments('-DG_DISABLE_CHECKS', language: ['cpp', 'c'])
  add_project_arguments('-DG_DISABLE_ASSERT', language: ['cpp', 'c'])
endif

if get_option('flatpak')
  add_project_arguments('-DFLATPAK', language: ['cpp', 'c'])
endif

cc = meson.get_compiler('c')
config_h = configuration_data()

set_defines = [
  ['APPLICATION_ID', application_id],
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_UPPER', meson.project_name().to_upper()],
  ['VERSION', meson.project_version()],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

vipsdisp_deps = [cc.find_library('m')]

gtk_dep = dependency('gtk4', version: '>=4.14')
# use this to fix tile alignment, added in 4.23.1
config_h.set('HAVE_GTK_SNAPSHOT_SET_SNAP', cc.has_function('gtk_snapshot_set_snap', prefix: '#include <gtk/gtk.h>', dependencies: gtk_dep))

config_file = configure_file(
  output: 'config.h',
  configuration: config_h,
)

vipsdisp_deps += declare_dependency(
  sources: config_file,
  include_directories: include_directories('.'),
  compile_args: '-DHAVE_CONFIG_H',
)

# need draw_line with the "draw-point" option
vipsdisp_deps += dependency('vips', version: '>=8.19')
vipsdisp_deps += gtk_dep

# Courtesy check that the compiler supports the cleanup attribute
glib_dep = dependency('glib-2.0')
if not cc.has_header_symbol('glib.h', 'g_autofree', dependencies: glib_dep)
  error('vipsdisp requires the GNU C "cleanup" attribute.')
endif

install_data(application_id + '.gschema.xml',
  install_dir: get_option('datadir') / 'glib-2.0' / 'schemas')

install_data(application_id + '.png',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / '128x128' / 'apps')
install_data(application_id + '.desktop',
  install_dir: get_option('datadir') / 'applications')
install_data(application_id + '.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo')

meson.add_install_script('meson_post_install.py')

subdir('src')
