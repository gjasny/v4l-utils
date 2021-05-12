dvb_common_deps =  [
    dep_argp,
    dep_libdvbv5,
    dep_libudev,
    dep_threads,
    dep_xmlrpc,
]

dvb_fe_tool_sources = files(
    'dvb-fe-tool.c',
)

dvb_fe_tool = executable('dvb-fe-tool',
                         dvb_fe_tool_sources,
                         install : true,
                         dependencies : dvb_common_deps,
                         include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'dvb-fe-tool', 1 ]]

dvbv5_zap_sources = files(
    'dvbv5-zap.c',
)

dvbv5_zap = executable('dvbv5-zap',
                       dvbv5_zap_sources,
                       install : true,
                       dependencies : dvb_common_deps,
                       include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'dvbv5-zap', 1 ]]

dvbv5_scan_sources = files(
    'dvbv5-scan.c',
)

dvbv5_scan = executable('dvbv5-scan',
                        dvbv5_scan_sources,
                        install : true,
                        dependencies : dvb_common_deps,
                        include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'dvbv5-scan', 1 ]]

dvb_format_convert_sources = files(
    'dvb-format-convert.c',
)

dvb_format_convert = executable('dvb-format-convert',
                                dvb_format_convert_sources,
                                install : true,
                                dependencies : dvb_common_deps,
                                include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'dvb-format-convert', 1 ]]

dvbv5_daemon_sources = files(
    'dvbv5-daemon.c',
)

dvbv5_daemon_c_args = [
    '-DHAVE_DVBV5_REMOTE',
]

dvbv5_daemon = executable('dvbv5-daemon',
                          dvbv5_daemon_sources,
                          install : true,
                          dependencies : dvb_common_deps,
                          c_args : dvbv5_daemon_c_args,
                          include_directories : v4l2_utils_incdir)
