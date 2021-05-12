v4l2_sysfs_path_sources = files(
    'v4l2-sysfs-path.c',
)

v4l2_sysfs_path_deps = [
    dep_argp,
    dep_libmedia_dev,
]

v4l2_sysfs_path = executable('v4l2-sysfs-path',
                             v4l2_sysfs_path_sources,
                             install : true,
                             dependencies : v4l2_sysfs_path_deps,
                             include_directories : v4l2_utils_incdir)
