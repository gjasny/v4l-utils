utils_common_incdir = include_directories('common')

# Generate targets for media-bus-format-{names,codes}.h
foreach x: [ 'names', 'codes' ]
    output_file = 'media-bus-format-@0@.h'.format(x)
    input_file = 'gen_media_bus_format_@0@.sh'.format(x)
    target = custom_target(
        output_file,
        output : output_file,
        input : input_file,
        command : [
            prog_bash, '@INPUT@', files('..' / 'include' / 'linux' / 'media-bus-format.h'),
        ],
        capture : true,
    )
    set_variable(output_file.underscorify(), target)
endforeach

# Libraries
subdir('libcecutil')
subdir('libmedia_dev')
subdir('libv4l2util')

# Utils
subdir('cec-ctl')
subdir('cec-follower')
subdir('cx18-ctl')
subdir('dvb', if_found : dep_libdvbv5)
subdir('ir-ctl')
subdir('ivtv-ctl')
subdir('keytable')
subdir('media-ctl')
subdir('qv4l2')
subdir('qvidcap')
subdir('rds-ctl')
subdir('v4l2-ctl')
subdir('v4l2-dbg')
subdir('v4l2-sysfs-path')
subdir('v4l2-tracer')

# Compliance tools
subdir('cec-compliance')
subdir('v4l2-compliance')
