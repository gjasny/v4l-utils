edid_decode_sources = [
	'calc-gtf-cvt.cpp',
	'calc-ovt.cpp',
	'edid-decode.cpp',
	'parse-base-block.cpp',
	'parse-cta-block.cpp',
	'parse-di-ext-block.cpp',
	'parse-displayid-block.cpp',
	'parse-ls-ext-block.cpp',
	'parse-vtb-ext-block.cpp',
	'parse-if.cpp',
	'parse-eld.cpp',
]

edid_decode_args = []

if cc.has_header('linux/i2c-dev.h')
	edid_decode_sources += ['ddc.cpp']
	edid_decode_args += ['-D__HAS_I2C_DEV__']
endif

edid_decode = executable(
	'edid-decode',
	edid_decode_sources,
	cpp_args: edid_decode_args,
	install: true,
)

meson.override_find_program('edid-decode', edid_decode)

man_pages += [[ meson.current_source_dir(), 'edid-decode', 1 ]]
