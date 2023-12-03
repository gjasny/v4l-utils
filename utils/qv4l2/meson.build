if get_option('qv4l2').disabled() or not dep_qt.found()
    subdir_done()
endif

qv4l2_sources = files(
    'alsa_stream.c',
    'alsa_stream.h',
    'capture-win-gl.cpp',
    'capture-win-gl.h',
    'capture-win-qt.cpp',
    'capture-win-qt.h',
    'capture-win.cpp',
    'capture-win.h',
    'ctrl-tab.cpp',
    'general-tab.cpp',
    'general-tab.h',
    'qv4l2.cpp',
    'qv4l2.h',
    'raw2sliced.cpp',
    'raw2sliced.h',
    'tpg-tab.cpp',
    'v4l2-tpg-colors.c',
    'v4l2-tpg-core.c',
    'vbi-tab.cpp',
    'vbi-tab.h',
)

qv4l2_deps = [
    dep_alsa,
    dep_gl,
    dep_libmedia_dev,
    dep_libv4l2,
    dep_libv4l2util,
    dep_libv4lconvert,
    dep_qt,
    dep_qt_opengl,
    dep_threads,
]

qv4l2_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

if dep_qt6.found()
  qt_files = qt6.preprocess(
      moc_headers : ['qv4l2.h', 'general-tab.h', 'vbi-tab.h', 'capture-win.h'],
      qresources : 'qv4l2.qrc',
  )
else
  qt_files = qt5.preprocess(
      moc_headers : ['qv4l2.h', 'general-tab.h', 'vbi-tab.h', 'capture-win.h'],
      qresources : 'qv4l2.qrc',
  )
endif
qv4l2_sources += qt_files

qv4l2 = executable('qv4l2',
                   sources : qv4l2_sources,
                   install : true,
                   dependencies : qv4l2_deps,
                   override_options : dep_qt_options,
                   include_directories : qv4l2_incdir)

man_pages += [[ meson.current_source_dir(), 'qv4l2', 1 ]]

qv4l2_applications_files = files(
    'qv4l2.desktop',
)

install_data('qv4l2.desktop',
             install_dir : get_option('datadir') / 'applications')

qv4l2_icons_dir = get_option('datadir') / 'icons' / 'hicolor'

qv4l2_icons_files = [
    ['qv4l2_16x16.png', '16x16',    'qv4l2.png'],
    ['qv4l2_24x24.png', '24x24',    'qv4l2.png'],
    ['qv4l2_32x32.png', '32x32',    'qv4l2.png'],
    ['qv4l2_64x64.png', '64x64',    'qv4l2.png'],
    ['qv4l2.svg',       'scalable', 'qv4l2.svg'],
]

foreach f : qv4l2_icons_files
    install_data(f[0],
                 rename: f[1] / 'apps' / f[2],
                 install_dir : qv4l2_icons_dir)
endforeach
