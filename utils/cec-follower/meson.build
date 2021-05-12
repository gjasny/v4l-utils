cec_follower_sources = files(
    'cec-follower.cpp',
    'cec-follower.h',
    'cec-processing.cpp',
    'cec-tuner.cpp',
)

cec_follower_deps = [
    dep_libcecutil,
    dep_librt,
]

cec_follower = executable('cec-follower',
                          cec_follower_sources,
                          install : true,
                          dependencies : cec_follower_deps,
                          include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'cec-follower', 1 ]]
