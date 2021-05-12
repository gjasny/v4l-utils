decode_tm6000_sources = files(
    'decode_tm6000.c',
)

decode_tm6000_deps = [
    dep_argp,
    dep_libv4l2util,
]

decode_tm6000 = executable('decode_tm6000',
                           decode_tm6000_sources,
                           install : true,
                           dependencies: decode_tm6000_deps,
                           include_directories : v4l2_utils_incdir)
