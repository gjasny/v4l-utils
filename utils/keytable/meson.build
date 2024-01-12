ir_keytable_sources = files(
    'ir-encode.c',
    'ir-encode.h',
    'keymap.c',
    'keymap.h',
    'keytable.c',
    'parse.h',
    'toml.c',
    'toml.h',
)

ir_keytable_deps = [
    dep_argp,
]

ir_keytable_system_dir = udevdir
ir_keytable_user_dir = get_option('sysconfdir') / 'rc_keymaps'

ir_keytable_c_args = [
    '-DIR_KEYTABLE_SYSTEM_DIR="@0@"'.format(ir_keytable_system_dir / 'rc_keymaps'),
    '-DIR_KEYTABLE_USER_DIR="@0@"'.format(ir_keytable_user_dir),
]

ir_bpf_enabled = prog_clang.found() and dep_libbpf.found() and dep_libelf.found()

if ir_bpf_enabled
    ir_keytable_sources += files(
        'bpf_load.c',
        'bpf_load.h',
    )
    ir_keytable_deps += [
        dep_libbpf,
        dep_libelf,
    ]
    ir_keytable_c_args += [
        '-DHAVE_BPF',
    ]
    subdir('bpf_protocols')
endif

ir_keytable_incdir = [
    utils_common_incdir,
    v4l2_utils_incdir,
]

ir_keytable = executable('ir-keytable',
                         sources : ir_keytable_sources,
                         install : true,
                         dependencies : ir_keytable_deps,
                         c_args : ir_keytable_c_args,
                         include_directories : ir_keytable_incdir)

man_pages += [[ meson.current_source_dir(), 'ir-keytable', 1 ]]
man_pages += [[ meson.current_source_dir(), 'rc_keymap', 5 ]]

ir_keytable_sysconf_files = files(
    'rc_maps.cfg',
)
install_data(ir_keytable_sysconf_files,
             install_dir : get_option('sysconfdir'))

subdir('rc_keymaps')
install_data(ir_keytable_rc_keymaps,
             install_dir : ir_keytable_system_dir / 'rc_keymaps')

ir_keytable_udev_rules = files(
    '70-infrared.rules',
)
install_data(ir_keytable_udev_rules,
             install_dir : ir_keytable_system_dir / 'rules.d')

if ir_bpf_enabled
if dep_systemd.found()
if have_udevdsyscallfilter
    ir_keytable_systemd_files = files(
        '50-rc_keymap.conf',
    )
    install_data(ir_keytable_systemd_files,
                 install_dir : systemd_systemdir / 'systemd-udevd.service.d')
endif
endif
endif

# Install non-existing directory to create empty directory structure
# See: https://github.com/mesonbuild/meson/issues/2904
# This should be replaced with install_emptydir() when bumping the minimum meson
# version to 0.60.0.
install_subdir('protocols',
               install_dir : ir_keytable_user_dir)
