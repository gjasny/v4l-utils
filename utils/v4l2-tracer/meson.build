if not dep_jsonc.found()
    subdir_done()
endif

# Generated sources

v4l2_tracer_gen = files('v4l2-tracer-gen.pl')

# Don't reorder the inputs The order of the input headers matters
v4l2_tracer_gen_inputs = files(
    '..' / '..' / 'include' / 'linux' / 'v4l2-controls.h',
    '..' / '..' / 'include' / 'linux' / 'videodev2.h',
    '..' / '..' / 'include' / 'linux' / 'media.h',
    '..' / '..' / 'include' / 'linux' / 'v4l2-common.h',
)

v4l2_tracer_gen_common_sources = custom_target('v4l2-tracer-gen-common-sources',
                                               input : v4l2_tracer_gen_inputs,
                                               output : ['v4l2-tracer-info-gen.h'],
                                               command : [
                                                   v4l2_tracer_gen,
                                                  '-o', meson.current_build_dir(),
                                                    '-t', 'common',
                                                   '@INPUT@',
                                               ])

v4l2_tracer_gen_retrace_sources = custom_target('v4l2-tracer-gen-retrace-sources',
                                                input : v4l2_tracer_gen_inputs,
                                                output : ['retrace-gen.cpp', 'retrace-gen.h'],
                                                command : [
                                                    v4l2_tracer_gen,
                                                    '-o', meson.current_build_dir(),
                                                    '-t', 'retrace',
                                                    '@INPUT@',
                                                ])

v4l2_tracer_gen_trace_sources = custom_target('v4l2-tracer-gen-trace-sources',
                                              input : v4l2_tracer_gen_inputs,
                                              output : ['trace-gen.cpp', 'trace-gen.h'],
                                              command : [
                                                  v4l2_tracer_gen,
                                                  '-o', meson.current_build_dir(),
                                                  '-t', 'trace',
                                                  '@INPUT@',
                                              ])

# V4L2 tracer library

libv4l2tracer_sources = files(
    'libv4l2tracer.cpp',
    'media-info.cpp',
    'trace-helper.cpp',
    'trace.cpp',
    'v4l2-info.cpp',
    'v4l2-tracer-common.cpp',
)

libv4l2tracer_sources += [
    v4l2_tracer_gen_common_sources,
    v4l2_tracer_gen_trace_sources,
]

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
    'v4l2-tracer-common.cpp',
    'v4l2-tracer.cpp',
)

v4l2_tracer_sources += [
    v4l2_tracer_gen_common_sources,
    v4l2_tracer_gen_retrace_sources,
]

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
