project('nip4', 'c',
  # ie. a major after nip2 8.9 for workspace save file versioning
  version: '9.1.6',
  license: 'GPL',
  meson_version: '>=0.64',
  default_options: [
    'c_std=c2x',
    # do a release (optimised) build by default
    'buildtype=release',
    # turn off asserts etc. in release mode
    'b_ndebug=if-release'
  ]
)

application_id = 'org.libvips.nip4'

version_parts = meson.project_version().split('.')
version_major = version_parts[0]
version_minor = version_parts[1]
version_micro = version_parts[2]

prefix_dir = get_option('prefix')
lib_dir = prefix_dir / get_option('libdir')
project_source_root = meson.current_source_dir()
project_build_root = meson.current_build_dir()

pkg = import('pkgconfig')
gnome = import('gnome')

# if we're optimising (eg. release mode) we turn off cast checks and g_asserts
if get_option('optimization') not in ['0', 'g']
  add_project_arguments('-DG_DISABLE_CAST_CHECKS', language: ['cpp', 'c'])
  add_project_arguments('-DG_DISABLE_CHECKS', language: ['cpp', 'c'])
  add_project_arguments('-DG_DISABLE_ASSERT', language: ['cpp', 'c'])
  add_project_arguments('-DRELEASE', language: ['cpp', 'c'])
endif

if get_option('flatpak')
  add_project_arguments('-DFLATPAK', language: ['cpp', 'c'])
endif

# we should probably have more flexibility here
lex = find_program('flex')
yacc = find_program('bison')

cc = meson.get_compiler('c')
config_h = configuration_data()

set_defines = [
  ['APPLICATION_ID', application_id],
  ['PACKAGE', meson.project_name()],
  ['VERSION', meson.project_version()],
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# Needed for reallocarray()
default_source_flag = ['-D_DEFAULT_SOURCE']
add_project_arguments(default_source_flag, language: 'c')

nip4_deps = [cc.find_library('m')]

gettext_domain = 'nip4@0@.@1@'.format(version_major, version_minor)
config_h.set_quoted('GETTEXT_PACKAGE', gettext_domain)
config_h.set_quoted('PREFIX', prefix_dir)
config_h.set_quoted('LIBDIR', lib_dir)
config_h.set('MAJOR_VERSION', version_major)
config_h.set('MINOR_VERSION', version_minor)
config_h.set('MICRO_VERSION', version_micro)

libintl_dep = dependency('intl', required: false)
have_bind_textdomain_codeset = false
if libintl_dep.found()
  nip4_deps += libintl_dep
  config_h.set('ENABLE_NLS', true)
  have_bind_textdomain_codeset = cc.has_function('bind_textdomain_codeset', prefix: '#include <libintl.h>', dependencies: libintl_dep)
endif
config_h.set('HAVE_BIND_TEXTDOMAIN_CODESET', have_bind_textdomain_codeset)

config_h.set('HAVE_SYS_RESOURCE_H', cc.has_header('sys/resource.h'))
config_h.set('HAVE_SYS_STATVFS_H', cc.has_header('sys/statvfs.h'))
config_h.set('HAVE_SYS_VFS_H', cc.has_header('sys/vfs.h'))
config_h.set('HAVE_SYS_MOUNT_H', cc.has_header('sys/mount.h'))
config_h.set('HAVE_GETRLIMIT', cc.has_function('getrlimit'))
config_h.set('HAVE_REALLOCARRAY', cc.has_function('reallocarray', prefix: '#include <stdlib.h>', args: default_source_flag))

gtk_dep = dependency('gtk4', version: '>=4.14')
# use this to fix tile alignment, not yet merged
config_h.set('HAVE_GTK_SNAPSHOT_SET_SNAP', cc.has_function('gtk_snapshot_set_snap', prefix: '#include <gtk/gtk.h>', dependencies: gtk_dep))

config_file = configure_file(
  output: 'config.h',
  configuration: config_h,
)

nip4_deps += declare_dependency(
  sources: config_file,
  include_directories: include_directories('.'),
  compile_args: '-DHAVE_CONFIG_H',
)

# need vips_thread_execute()
nip4_deps += dependency('vips', version: '>=8.18')
nip4_deps += gtk_dep
nip4_deps += dependency('gsl')
nip4_deps += dependency('libxml-2.0')

# Courtesy check that the compiler supports the cleanup attribute
glib_dep = dependency('glib-2.0')
if not cc.has_header_symbol('glib.h', 'g_autofree', dependencies : glib_dep)
  error('nip4 requires the GNU C "cleanup" attribute.')
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
subdir('share')
subdir('test')
subdir('man')
