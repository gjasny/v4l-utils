v4l2_dbg_sources = files(
    'v4l2-dbg-ac97.h',
    'v4l2-dbg-bttv.h',
    'v4l2-dbg-em28xx.h',
    'v4l2-dbg-micron.h',
    'v4l2-dbg-saa7134.h',
    'v4l2-dbg-tvp5150.h',
    'v4l2-dbg.cpp',
    'v4l2-dbg.h',
    'v4l2-info.cpp',
)

v4l2_dbg = executable('v4l2-dbg',
                      v4l2_dbg_sources,
                      install : true,
                      install_dir : 'sbin',
                      include_directories : [
                          v4l2_utils_incdir,
                          utils_common_incdir,
                      ])
