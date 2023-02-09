libmks_sources = [
  'mks-init.c',
  'mks-device.c',
  'mks-keyboard.c',
  'mks-mouse.c',
  'mks-screen.c',
  'mks-session.c',
]

libmks_headers = [
  'libmks.h',
  'mks-device.h',
  'mks-init.h',
  'mks-keyboard.h',
  'mks-mouse.h',
  'mks-screen.h',
  'mks-session.h',
  'mks-types.h',
]

libmks_enums = gnome.mkenums_simple('mks-enums',
            sources: libmks_headers,
     install_header: true,
        install_dir: join_paths(get_option('libdir'), 'libmks-@0@'.format(api_version)),
  identifier_prefix: 'Mks',
      symbol_prefix: 'mks',
)

libmks_private_sources = [
  'mks-read-only-list-model.c',

  gnome.gdbus_codegen('mks-qemu',
         autocleanup: 'all',
    interface_prefix: 'org.qemu.Display1.',
           namespace: 'MksQemu',
             sources: 'dbus-display1.xml',
  )
]

# Setup mks-version.h for version checking
version_split = meson.project_version().split('.')
version_conf = configuration_data()
version_conf.set('VERSION', meson.project_version())
version_conf.set('MAJOR_VERSION', version_split[0])
version_conf.set('MINOR_VERSION', version_split[1])
version_conf.set('MICRO_VERSION', version_split[2])
configure_file(
          input: 'mks-version.h.in',
         output: 'mks-version.h',
  configuration: version_conf,
        install: true,
    install_dir: join_paths(get_option('includedir'), 'libmks-@0@'.format(api_version))
)

libmks_deps = [
  libgio_dep,
  libgiounix_dep,
  libgtk_dep,
]

libmks = shared_library('mks-' + api_version,
  libmks_sources + [libmks_enums[0]] + libmks_private_sources,
  dependencies: libmks_deps,
       install: true,
)

libmks_dep = declare_dependency(
            link_with: libmks,
         dependencies: libmks_deps,
  include_directories: include_directories('.'),
)

install_headers(libmks_headers, subdir: 'libmks-@0@'.format(api_version))

pkg.generate(
  description: 'A shared library for Mouse, Keyboard, and Screen to Qemu',
    libraries: libmks,
         name: 'libmks',
     filebase: 'libmks-' + api_version,
      version: meson.project_version(),
      subdirs: 'libmks-@0@'.format(api_version),
     requires: ['gio-2.0', 'gio-unix-2.0', 'gtk4'],
  install_dir: join_paths(get_option('libdir'), 'pkgconfig')
)

libmks_gir = gnome.generate_gir(libmks,
              sources: libmks_sources + libmks_headers,
            nsversion: api_version,
            namespace: 'Mks',
        symbol_prefix: 'mks',
    identifier_prefix: 'Mks',
             includes: ['Gio-2.0', 'Gtk-4.0'],
              install: true,
)