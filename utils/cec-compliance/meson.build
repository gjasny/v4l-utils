cec_compliance_sources = files(
    'cec-compliance.cpp',
    'cec-compliance.h',
    'cec-test-adapter.cpp',
    'cec-test-audio.cpp',
    'cec-test-fuzzing.cpp',
    'cec-test-power.cpp',
    'cec-test-tuner-record-timer.cpp',
    'cec-test.cpp',
)

cec_compliance_deps = [
    dep_libcecutil,
    dep_librt,
]

cec_compliance = executable('cec-compliance',
                            cec_compliance_sources,
                            install : true,
                            dependencies : cec_compliance_deps,
                            include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'cec-compliance', 1 ]]
