rds_ctl_sources = files(
    'rds-ctl.cpp',
    'v4l2-info.cpp',
)

rds_ctl_deps = [
    dep_libv4l2rds,
]

rds_ctl = executable('rds-ctl',
                     rds_ctl_sources,
                     install : true,
                     dependencies : rds_ctl_deps,
                     include_directories : [
                         v4l2_utils_incdir,
                         utils_common_incdir,
                     ])
