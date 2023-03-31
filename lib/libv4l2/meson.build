libv4l2_sources = files(
    'libv4l2-priv.h',
    'libv4l2.c',
    'log.c',
)

libv4l2_api = files(
    '../include/libv4l-plugin.h',
    '../include/libv4l2.h',
)

install_headers(libv4l2_api)

libv4l2_deps = [
    dep_libdl,
    dep_libv4lconvert,
    dep_threads,
]

libv4l2_c_args = []

if get_option('v4l-plugins')
    libv4l2_sources += files('v4l2-plugin.c')
    libv4l2_c_args += [
        '-DHAVE_V4L_PLUGINS',
        '-DLIBV4L2_PLUGIN_DIR="@0@"'.format(libv4l2plugindir)
    ]
endif

if have_visibility
    libv4l2_c_args += ['-fvisibility=hidden']
endif

libv4l2 = library('v4l2',
                  libv4l2_sources,
                  soversion: '0',
                  version: '0.0.0',
                  install : true,
                  dependencies : libv4l2_deps,
                  c_args : libv4l2_c_args,
                  include_directories : v4l2_utils_incdir)

dep_libv4l2 = declare_dependency(link_with : libv4l2)
meson.override_dependency('libv4l2', dep_libv4l2)

pkg.generate(
    libv4l2,
    name : 'libv4l2',
    version : meson.project_version(),
    requires_private : 'libv4lconvert',
    description : 'v4l2 device access library')

if not get_option('v4l-wrappers')
    subdir_done()
endif

v4l2convert_sources = files(
    'v4l2convert.c',
)

v4l2convert_deps = [
    dep_libv4l2,
]

libv4l2privdir = get_option('prefix') / get_option('libdir') / get_option('libv4l2subdir')

v4l2convert = shared_module('v4l2convert',
                            v4l2convert_sources,
                            name_prefix : '',
                            install : true,
                            install_dir : libv4l2privdir,
                            c_args : v4l2_wrapper_args,
                            dependencies : v4l2convert_deps,
                            include_directories : v4l2_utils_incdir)
