gconv_install_dir = get_option('libdir') / 'gconv'

arib_std_b24_sources = files(
    'arib-std-b24.c',
    'jis0201.h',
    'jis0208.h',
    'jisx0213.h',
)

arib_std_b24_deps = [
    dep_jis,
    dep_jisx0213,
]

arib_std_b24 = shared_module('ARIB-STD-B24',
                             arib_std_b24_sources,
                             name_prefix : '',
                             dependencies : arib_std_b24_deps,
                             install : true,
                             install_dir : gconv_install_dir,
                             include_directories : v4l2_utils_incdir)

dep_arib_std_b24 = declare_dependency(link_with : arib_std_b24)

en300_468_tab00_sources = files(
    'en300-468-tab00.c',
)

en300_468_tab00_deps = [
    dep_jis,
    dep_jisx0213,
]

en300_468_tab00 = shared_module('EN300-468-TAB00',
                                en300_468_tab00_sources,
                                name_prefix : '',
                                dependencies : en300_468_tab00_deps,
                                install : true,
                                install_dir : gconv_install_dir,
                                include_directories : v4l2_utils_incdir)

dep_en300_468_tab00 = declare_dependency(link_with : en300_468_tab00)

install_data('gconv-modules', install_dir : gconv_install_dir)
