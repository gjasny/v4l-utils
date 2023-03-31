libcecutil_sources = files(
    'cec-htng-funcs.h',
    'cec-htng.h',
    'cec-info.cpp',
    'cec-info.h',
    'cec-log.cpp',
    'cec-log.h',
    'cec-parse.cpp',
    'cec-parse.h',
)

libcecutil_deps = [
    dep_libdl,
    dep_threads,
]

libcecutil_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

cec_gen_sources = files(
    '../../include/linux/cec.h',
    'cec-htng.h',
    '../../include/linux/cec-funcs.h',
    'cec-htng-funcs.h',
)

configure_file(
    input : 'cec-gen.pl',
    output : 'cec-gen-dummy.stamp',
    command : [prog_perl, '@INPUT@', cec_gen_sources, 'utils/libcecutil'],
)

libcecutil = static_library('cecutil',
                            libcecutil_sources,
                            install : false,
                            dependencies : libcecutil_deps,
                            include_directories : libcecutil_incdir)

dep_libcecutil = declare_dependency(
    link_with : libcecutil,
    include_directories : '.',
)
