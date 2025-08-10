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

bpf_args += run_command('cc_sys_includes.sh',
                        cc.cmd_array(),
                        check : true).stdout().split()

foreach file : bpf_protocols_files
    output = file + '.o'
    input = file + '.c'
    custom_target(output,
                  output : output,
                  input : input,
                  command : [
                      prog_bpf,
                      bpf_args,
                      '-D__linux__', '-fno-stack-protector',
                      '-O2', '-c', '@INPUT@', '-o', '@OUTPUT@',
                  ],
                  install : true,
                  install_dir : ir_keytable_system_dir / 'rc_keymaps' / 'protocols')
endforeach
