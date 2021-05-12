libv4l2rds_sources = files(
    'libv4l2rds.c',
)

libv4l2rds_api = files(
    '../include/libv4l2rds.h',
)

install_headers(libv4l2rds_api)

libv4l2rds_deps = [
    dep_threads,
]

libv4l2rds_c_args = []

if have_visibility
    libv4l2rds_c_args += ['-fvisibility=hidden']
endif

libv4l2rds = library('v4l2rds',
                     libv4l2rds_sources,
                     soversion: '0',
                     version: '0.0.0',
                     install : true,
                     dependencies : libv4l2rds_deps,
                     c_args : libv4l2rds_c_args,
                     include_directories : v4l2_utils_incdir)

dep_libv4l2rds = declare_dependency(link_with : libv4l2rds)
meson.override_dependency('libv4l2rds', dep_libv4l2rds)

pkg.generate(
    libv4l2rds,
    name : 'libv4l2rds',
    version : meson.project_version(),
    description : 'v4l2 RDS decode library')
