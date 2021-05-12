libv4l2plugindir = get_option('prefix') / get_option('libdir') / get_option('libv4l2subdir') / 'plugins'

subdir('libdvbv5')
subdir('libv4lconvert')
subdir('libv4l2')
subdir('libv4l1')
subdir('libv4l2rds')

if get_option('v4l-plugins')
    subdir('libv4l-mplane')
endif
