if host_machine.system() == 'linux'
    subdir('test')
    subdir('xc3028-firmware')
    if have_i2c_dev
        subdir('rds-saa6588')
    endif
endif

subdir('decode_tm6000')

if have_gconv
    subdir('gconv', if_found : [dep_jis, dep_jisx0213])
endif
