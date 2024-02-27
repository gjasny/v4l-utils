if not dep_jsonc.found()
    subdir_done()
endif

# V4L2 tracer library

libv4l2tracer_sources = files(
    'libv4l2tracer.cpp',
    'media-info.cpp',
    'trace-helper.cpp',
    'trace.cpp',
    'trace-gen.cpp',
    'v4l2-info.cpp',
    'v4l2-tracer-common.cpp',
)

libv4l2tracer_deps = [
    dep_jsonc,
    dep_libdl,
]

libv4l2_tracer_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

libv4l2tracer = shared_module('v4l2tracer',
                              libv4l2tracer_sources,
                              install : true,
                              cpp_args : v4l2_wrapper_args,
                              dependencies : libv4l2tracer_deps,
                              include_directories : libv4l2_tracer_incdir)

# Command line tool

v4l2_tracer_sources = files(
    'media-info.cpp',
    'retrace-helper.cpp',
    'retrace.cpp',
    'v4l2-info.cpp',
    'trace-gen.cpp',
    'retrace-gen.cpp',
    'v4l2-tracer-common.cpp',
    'v4l2-tracer.cpp',
)

v4l2_tracer_deps = [
    dep_jsonc,
    dep_librt,
    dep_threads,
]

v4l2_tracer_cpp_args = [
    '-DLIBTRACER_PATH="@0@"'.format(get_option('prefix') / get_option('libdir')),
]

if have_visibility
    v4l2_tracer_cpp_args += ['-fvisibility=hidden']
endif

v4l2_tracer_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

v4l2_tracer = executable('v4l2-tracer',
                         v4l2_tracer_sources,
                         install : true,
                         dependencies : v4l2_tracer_deps,
                         cpp_args : v4l2_tracer_cpp_args,
                         include_directories : v4l2_tracer_incdir)

man_pages += [[ meson.current_source_dir(), 'v4l2-tracer', 1 ]]

# devenv support
env = environment()
env.set('V4L2_TRACER', libv4l2tracer.full_path())
meson.add_devenv(env)
