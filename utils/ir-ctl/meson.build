ir_ctl_sources = files(
    'bpf_encoder.c',
    'bpf_encoder.h',
    'ir-ctl.c',
    'ir-encode.c',
    'ir-encode.h',
    'keymap.c',
    'keymap.h',
    'toml.c',
    'toml.h',
)

ir_ctl_deps =  [
    dep_argp,
]

ir_ctl = executable('ir-ctl',
                    ir_ctl_sources,
                    install : true,
                    dependencies : ir_ctl_deps,
                    include_directories : v4l2_utils_incdir)

man_pages += [[ meson.current_source_dir(), 'ir-ctl', 1 ]]
