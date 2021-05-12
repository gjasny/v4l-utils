bpf_protocols_files = [
    'grundig',
    'imon_rsc',
    'manchester',
    'pulse_distance',
    'pulse_length',
    'raw',
    'rc_mm',
    'samsung36',
    'xbox-dvd',
]

clang_sys_includes = run_command('clang_sys_includes.sh',
                                 check : true,
                                 env : ['CLANG=' + prog_clang.full_path()])

foreach file : bpf_protocols_files
    output = file + '.o'
    input = file + '.c'
    custom_target(output,
                  output : output,
                  input : input,
                  command : [
                      prog_clang,
                      clang_sys_includes.stdout().split(),
                      '-D__linux__', '-fno-stack-protector', '-target', 'bpf',
                      '-O2', '-c', '@INPUT@', '-o', '@OUTPUT@',
                  ],
                  install : true,
                  install_dir : ir_keytable_system_dir / 'rc_keymaps' / 'protocols')
endforeach
