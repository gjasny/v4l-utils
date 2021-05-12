ivtv_ctl_sources = files(
    'ivtv-ctl.c',
)

ivtv_ctl_deps = [
    dep_libm,
]

ivtv_ctl = executable('ivtv-ctl',
                      ivtv_ctl_sources,
                      install : true,
                      dependencies : ivtv_ctl_deps,
                      include_directories : v4l2_utils_incdir)
