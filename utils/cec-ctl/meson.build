cec_ctl_sources = files(
    'cec-ctl.cpp',
    'cec-ctl.h',
    'cec-pin.cpp',
)

cec_ctl_deps = [
    dep_libcecutil,
    dep_librt,
]

cec_ctl = executable('cec-ctl',
                     cec_ctl_sources,
                     install : true,
                     dependencies : cec_ctl_deps,
                     include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'cec-ctl', 1 ]]
