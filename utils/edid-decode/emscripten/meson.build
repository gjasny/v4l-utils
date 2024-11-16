project(
	'edid-decode',
	'cpp',
)

fs = import('fs')

edid_decode_args = [
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
	'-Wimplicit-fallthrough',
	'-Wno-variadic-macros',
	'-include', meson.current_build_dir() / 'config.h',
]
edid_decode_link_args = []

git_sha = ''
git_commit_cnt = ''
git_commit_date = ''

if fs.is_dir('../../../.git')
    prog_git = find_program('git')
    source_root = '../../..'
    git_sha = run_command(prog_git, '-C', source_root, 'rev-parse', '--short=12', 'HEAD',
                          check : true).stdout().strip()
    git_commit_cnt = '-' + run_command(prog_git, '-C', source_root, 'rev-list', '--count', 'HEAD',
                                 check : true).stdout().strip()
    git_commit_date = run_command(prog_git, '-C', source_root, 'show', '--quiet',
                                  '--date=format-local:%F %T', '--format=%cd',
                                  env : ['TZ=UTC'], check : true).stdout().strip()
endif

conf = configuration_data()
conf.set('GIT_SHA', git_sha)
conf.set('GIT_COMMIT_CNT', git_commit_cnt)
conf.set('GIT_COMMIT_DATE', git_commit_date)

configure_file(output : 'config.h', configuration : conf)

if target_machine.system() == 'emscripten'
	edid_decode_link_args += [
		'-sEXPORTED_FUNCTIONS=_parse_edid',
		'-sEXPORTED_RUNTIME_METHODS=ccall,cwrap'
	]

	fs = import('fs')
	foreach filename : ['edid-decode.html', 'edid-decode.ico']
		fs.copyfile(
			filename,
			install: true,
			install_dir: 'bin',
		)
	endforeach
endif

edid_decode_sources = [
	'../calc-gtf-cvt.cpp',
	'../calc-ovt.cpp',
	'../edid-decode.cpp',
	'../parse-base-block.cpp',
	'../parse-cta-block.cpp',
	'../parse-di-ext-block.cpp',
	'../parse-displayid-block.cpp',
	'../parse-ls-ext-block.cpp',
	'../parse-vtb-ext-block.cpp',
	'../parse-if.cpp',
]

edid_decode = executable(
	'edid-decode',
	sources: edid_decode_sources,
	cpp_args: edid_decode_args,
	link_args: edid_decode_link_args,
	install: true,
)

meson.override_find_program(
	'edid-decode',
	edid_decode,
)
