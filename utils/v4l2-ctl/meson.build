v4l2_ctl_sources = files(
    'codec-fwht.c',
    'codec-v4l2-fwht.c',
    'media-info.cpp',
    'v4l-stream.c',
    'v4l2-ctl-common.cpp',
    'v4l2-ctl-edid.cpp',
    'v4l2-ctl-io.cpp',
    'v4l2-ctl-meta.cpp',
    'v4l2-ctl-misc.cpp',
    'v4l2-ctl-modes.cpp',
    'v4l2-ctl-overlay.cpp',
    'v4l2-ctl-sdr.cpp',
    'v4l2-ctl-selection.cpp',
    'v4l2-ctl-stds.cpp',
    'v4l2-ctl-streaming.cpp',
    'v4l2-ctl-subdev.cpp',
    'v4l2-ctl-tuner.cpp',
    'v4l2-ctl-vbi.cpp',
    'v4l2-ctl-vidcap.cpp',
    'v4l2-ctl-vidout.cpp',
    'v4l2-ctl.cpp',
    'v4l2-ctl.h',
    'v4l2-info.cpp',
    'v4l2-tpg-colors.c',
    'v4l2-tpg-core.c',
)
v4l2_ctl_sources += media_bus_format_names_h

v4l2_ctl_deps = [
    dep_librt,
    dep_threads,
]

v4l2_ctl_cpp_args = []

if get_option('v4l2-ctl-libv4l')
    v4l2_ctl_deps += [
        dep_libv4l2,
        dep_libv4lconvert,
    ]
else
    v4l2_ctl_cpp_args += '-DNO_LIBV4L2'
endif

if not get_option('v4l2-ctl-stream-to')
    v4l2_ctl_cpp_args += '-DNO_STREAM_TO'
endif

v4l2_ctl_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

v4l2_ctl = executable('v4l2-ctl',
                      v4l2_ctl_sources,
                      install : true,
                      dependencies : v4l2_ctl_deps,
                      cpp_args : v4l2_ctl_cpp_args,
                      include_directories : v4l2_ctl_incdir)

man_pages += [[ meson.current_source_dir(), 'v4l2-ctl', 1 ]]

v4l2_ctl_32_c_cpp_args = ['-m32', '-DNO_LIBV4L2']
v4l2_ctl_32_link_args = ['-m32', '-static']

if get_option('v4l2-ctl-32') and have_m32
    v4l2_ctl_32 = executable('v4l2-ctl-32',
                             v4l2_ctl_sources,
                             install : true,
                             c_args : v4l2_ctl_32_c_cpp_args,
                             cpp_args : v4l2_ctl_32_c_cpp_args,
                             link_args : v4l2_ctl_32_link_args,
                             include_directories : v4l2_ctl_incdir)
endif
