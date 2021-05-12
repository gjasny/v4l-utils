libmediactl_sources = files(
    'libmediactl.c',
    'mediactl-priv.h',
)

libmediactl_deps = [
    dep_libudev,
]

libmediactl = static_library('mediactl',
                             libmediactl_sources,
                             dependencies : libmediactl_deps,
                             include_directories : v4l2_utils_incdir)

dep_libmediactl = declare_dependency(link_with : libmediactl)

libv4l2subdev_sources = files('libv4l2subdev.c')
libv4l2subdev_sources += media_bus_format_names_h
libv4l2subdev_sources += media_bus_format_codes_h

libv4l2subdev = static_library('v4l2subdev',
                               libv4l2subdev_sources,
                               include_directories : v4l2_utils_incdir)

dep_libv4l2subdev = declare_dependency(link_with : libv4l2subdev)

media_ctl_sources = files(
    'media-ctl.c',
    'options.c',
    'options.h',
    'tools.h',
)

media_ctl_deps = [
    dep_libmediactl,
    dep_libv4l2subdev,
]

media_ctl = executable('media-ctl',
                       media_ctl_sources,
                       dependencies : media_ctl_deps,
                       install : true,
                       include_directories : v4l2_utils_incdir)
