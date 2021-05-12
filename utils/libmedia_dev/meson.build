libmedia_dev_sources = files(
    'get_media_devices.c',
    'get_media_devices.h',
)

libmedia_dev = static_library('media_dev',
                              libmedia_dev_sources,
                              install : false,
                              include_directories : v4l2_utils_incdir)

dep_libmedia_dev = declare_dependency(
    link_with : libmedia_dev,
    include_directories : '.',
)
