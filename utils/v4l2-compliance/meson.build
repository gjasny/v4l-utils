if not have_fork
    subdir_done()
endif

v4l2_compliance_sources = files(
    'media-info.cpp',
    'v4l2-compliance.cpp',
    'v4l2-compliance.h',
    'v4l2-info.cpp',
    'v4l2-test-buffers.cpp',
    'v4l2-test-codecs.cpp',
    'v4l2-test-colors.cpp',
    'v4l2-test-controls.cpp',
    'v4l2-test-debug.cpp',
    'v4l2-test-formats.cpp',
    'v4l2-test-input-output.cpp',
    'v4l2-test-io-config.cpp',
    'v4l2-test-media.cpp',
    'v4l2-test-subdevs.cpp',
    'v4l2-test-time32-64.cpp',
)

v4l2_compliance_deps = [
    dep_librt,
    dep_threads,
]

v4l2_compliance_cpp_args = []

if get_option('v4l2-compliance-libv4l')
    v4l2_compliance_deps += [
        dep_libv4lconvert,
        dep_libv4l2,
    ]
else
    v4l2_compliance_cpp_args += '-DNO_LIBV4L2'
endif

v4l2_compliance_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

v4l2_compliance = executable('v4l2-compliance',
                             v4l2_compliance_sources,
                             install : true,
                             dependencies : v4l2_compliance_deps,
                             cpp_args : v4l2_compliance_cpp_args,
                             include_directories : v4l2_compliance_incdir)

man_pages += [[ meson.current_source_dir(), 'v4l2-compliance', 1 ]]

v4l2_compliance_32_cpp_args = ['-m32', '-DNO_LIBV4L2']
v4l2_compliance_32_link_args = ['-m32', '-static']

if get_option('v4l2-compliance-32') and have_m32
    v4l2_compliance_32 = executable('v4l2-compliance-32',
                                    v4l2_compliance_sources,
                                    install : true,
                                    cpp_args : v4l2_compliance_32_cpp_args,
                                    link_args : v4l2_compliance_32_link_args,
                                    include_directories : v4l2_compliance_incdir)
endif
