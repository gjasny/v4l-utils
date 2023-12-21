ioctl_test_sources = files(
    'ioctl-test.c',
    'ioctl-test.h',
    'ioctl_32.h',
    'ioctl_64.h',
)

ioctl_test = executable('ioctl-test',
                        ioctl_test_sources,
                        include_directories : v4l2_utils_incdir)

sliced_vbi_test_sources = files(
    'sliced-vbi-test.c',
)

sliced_vbi_test = executable('sliced-vbi-test',
                             sliced_vbi_test_sources,
                             include_directories : v4l2_utils_incdir)

sliced_vbi_detect_sources = files(
    'sliced-vbi-detect.c',
)

sliced_vbi_detect = executable('sliced-vbi-detect',
                               sliced_vbi_detect_sources,
                               include_directories : v4l2_utils_incdir)

v4l2grab_sources = files(
    'v4l2grab.c',

)

v4l2grab_deps = [
    dep_argp,
    dep_libv4l2,
    dep_libv4lconvert,
    dep_threads,
]

v4l2grab = executable('v4l2grab',
                      v4l2grab_sources,
                      dependencies : v4l2grab_deps,
                      include_directories : v4l2_utils_incdir)

driver_test_sources = files(
    'driver-test.c',

)

driver_test_deps = [
    dep_libv4l2util,
]

driver_test = executable('driver-test',
                         driver_test_sources,
                         dependencies : driver_test_deps,
                         include_directories : v4l2_utils_incdir)

mc_nextgen_test_sources = files(
    'mc_nextgen_test.c',
)

mc_nextgen_test_deps = [
    dep_argp,
    dep_libudev,
]

mc_nextgen_test = executable('mc-nextgen-test',
                             mc_nextgen_test_sources,
                             dependencies : mc_nextgen_test_deps,
                             include_directories : v4l2_utils_incdir)

stress_buffer_sources = files(
    'stress-buffer.c',
)

stress_buffer = executable('stress-buffer',
                           stress_buffer_sources,
                           include_directories : v4l2_utils_incdir)

capture_example_sources = files(
    'capture-example.c',
)

capture_example = executable('capture-example',
                             capture_example_sources,
                             include_directories : v4l2_utils_incdir)

if dep_x11.found()
    pixfmt_test_sources = files(
        'pixfmt-test.c',
    )

    pixfmt_test_deps = [
        dep_x11,
    ]

    pixfmt_test = executable('pixfmt-test',
                             pixfmt_test_sources,
                             dependencies : pixfmt_test_deps,
                             include_directories : v4l2_utils_incdir)
endif

if dep_gl.found() and dep_glu.found() and dep_x11.found()
    v4l2gl_sources = files(
        'v4l2gl.c',
    )

    v4l2gl_deps = [
        dep_argp,
        dep_gl,
        dep_glu,
        dep_libv4l2,
        dep_libv4lconvert,
        dep_x11,
    ]

    v4l2gl = executable('v4l2gl',
                        v4l2gl_sources,
                        dependencies : v4l2gl_deps,
                        include_directories : v4l2_utils_incdir)
endif

if dep_jpeg.found() and dep_sdl.found() and dep_sdl_image.found()
    sdlcam_sources = files(
        'sdlcam.c',
    )

    sdlcam_deps = [
        dep_jpeg,
        dep_libdl,
        dep_libm,
        dep_librt,
        dep_libv4l2,
        dep_libv4lconvert,
        dep_sdl,
        dep_sdl_image,
    ]

    sdlcam = executable('sdlcam',
                        sdlcam_sources,
                        dependencies : sdlcam_deps,
                        include_directories : v4l2_utils_incdir)
endif
