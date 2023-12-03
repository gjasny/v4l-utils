qvidcap_option = get_option('qvidcap')

if qvidcap_option.enabled() and not have_qt_opengl
    error('qvidcap enabled but Qt5/Qt6 is missing OpenGL support')
endif

if qvidcap_option.disabled() or not dep_gl.found() or not dep_qt_opengl.found() or not have_qt_opengl
    subdir_done()
endif

qvidcap_sources = files(
    'capture.cpp',
    'capture.h',
    'codec-fwht.c',
    'codec-v4l2-fwht.c',
    'paint.cpp',
    'qvidcap.cpp',
    'qvidcap.h',
    'v4l-stream.c',
    'v4l2-info.cpp',
    'v4l2-tpg-colors.c',
    'v4l2-tpg-core.c',
)

qvidcap_deps = [
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

qvidcap_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

if dep_qt6.found()
qt_files = qt6.preprocess(
    moc_headers : 'capture.h',
    qresources : 'qvidcap.qrc',
)
else
qt_files = qt5.preprocess(
    moc_headers : 'capture.h',
    qresources : 'qvidcap.qrc',
)
endif
qvidcap_sources += qt_files

v4l2_convert_sources = files(
    'v4l2-convert.glsl',
)

configure_file(
    input : 'v4l2-convert.pl',
    output : 'v4l2-convert.h',
    capture : true,
    command : [prog_perl, '@INPUT@', v4l2_convert_sources],
)

qvidcap = executable('qvidcap',
                     sources : qvidcap_sources,
                     install : true,
                     dependencies : qvidcap_deps,
                     override_options : dep_qt_options,
                     include_directories : qvidcap_incdir)

man_pages += [[ meson.current_source_dir(), 'qvidcap', 1 ]]

qvidcap_applications_files = files(
    'qvidcap.desktop',
)

install_data('qvidcap.desktop',
             install_dir : get_option('datadir') / 'applications')

qvidcap_icons_dir = get_option('datadir') / 'icons' / 'hicolor'

qvidcap_icons_files = [
    ['qvidcap_16x16.png', '16x16',    'qvidcap.png'],
    ['qvidcap_24x24.png', '24x24',    'qvidcap.png'],
    ['qvidcap_32x32.png', '32x32',    'qvidcap.png'],
    ['qvidcap_64x64.png', '64x64',    'qvidcap.png'],
    ['qvidcap.svg',       'scalable', 'qvidcap.svg'],
]

foreach f : qvidcap_icons_files
    install_data(f[0],
                 rename: f[1] / 'apps' / f[2],
                 install_dir : qvidcap_icons_dir)
endforeach
