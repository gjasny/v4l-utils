libv4l2util_sources = files(
    'frequencies.c',
    'libv4l2util.h',
    'v4l2_driver.c',
    'v4l2_driver.h',
)

libv4l2util = static_library('v4l2util',
                             libv4l2util_sources,
                             install : false,
                             include_directories : v4l2_utils_incdir)

dep_libv4l2util = declare_dependency(
    link_with : libv4l2util,
    include_directories : '.',
)
