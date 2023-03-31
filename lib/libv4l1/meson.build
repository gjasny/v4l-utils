libv4l1_sources = files(
    'libv4l1-priv.h',
    'libv4l1.c',
    'log.c',
)

libv4l1_api = files(
    '../include/libv4l1-videodev.h',
    '../include/libv4l1.h',
)

install_headers(libv4l1_api)

libv4l1_deps = [
    dep_libv4l2,
    dep_threads,
]

libv4l1_c_args = []

if have_visibility
    libv4l1_c_args += ['-fvisibility=hidden']
endif

libv4l1 = library('v4l1',
                  libv4l1_sources,
                  install : true,
                  soversion: '0',
                  version: '0.0.0',
                  dependencies : libv4l1_deps,
                  c_args : libv4l1_c_args,
                  include_directories : v4l2_utils_incdir)

dep_libv4l1 = declare_dependency(link_with : libv4l1)
meson.override_dependency('libv4l1', dep_libv4l1)

pkg.generate(
    libv4l1,
    name : 'libv4l1',
    version : meson.project_version(),
    requires_private : 'libv4l2',
    description : 'v4l1 compatibility library')

if not get_option('v4l-wrappers')
    subdir_done()
endif

v4l1compat_sources = files(
    'v4l1compat.c',
)

v4l1compat_deps = [
    dep_libv4l1,
]

libv4l1privdir = get_option('prefix') / get_option('libdir') / get_option('libv4l1subdir')

v4l1compat = shared_module('v4l1compat',
                           v4l1compat_sources,
                           name_prefix : '',
                           install : true,
                           install_dir : libv4l1privdir,
                           c_args : v4l2_wrapper_args,
                           dependencies : v4l1compat_deps,
                           include_directories : v4l2_utils_incdir)
