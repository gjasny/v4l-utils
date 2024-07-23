project('v4l-utils', 'c', 'cpp',
    version: '1.28.1',
    meson_version : '>= 0.57',
    default_options : [
        'buildtype=debugoptimized',
        'warning_level=1',
        'c_std=gnu99',
        'cpp_std=gnu++11',
    ],
    license : 'LGPL 2.1+')

# meson >= 0.56 can use instead meson.project_source_root()
source_root = meson.current_source_dir()

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')
have_m32 = cc.has_link_argument('-m32')
have_visibility = cc.has_argument('-fvisibility=hidden') and \
                  cc.has_function_attribute('visibility:default')

sys_root = meson.get_external_property('sys_root', '/')

fs = import('fs')
i18n = import('i18n')
pkg = import('pkgconfig')
qt6 = import('qt6')
qt5 = import('qt5')

as_version = meson.project_version()
ver_arr = as_version.split('.')
as_major_version = ver_arr[0]
as_minor_version = ver_arr[1]
as_patch_version = ver_arr[2]

conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', as_version)
conf.set_quoted('V4L_UTILS_VERSION', as_version)
conf.set('MAN_PACKAGE_VERSION', as_version)
conf.set('MAJOR', as_major_version)
conf.set('MINOR', as_minor_version)
conf.set('PATCH', as_patch_version)

common_arguments = [
    '-Wpointer-arith',
    '-D_GNU_SOURCE',
    '-DPROMOTED_MODE_T=int',
    '-DENABLE_NLS',
    '-include', meson.current_build_dir() / 'config.h',
]

v4l2_wrapper_args = [
    # As the library needs to provide both 32-bit and 64-bit versions
    # of file operations, disable transparent large file support (fixes
    # 'Error: symbol `open64/mmap64' is already defined' compile failure
    # otherwise)
    '-U_FILE_OFFSET_BITS',
    '-D_FILE_OFFSET_BITS=32',
    '-D_LARGEFILE64_SOURCE',
]

v4l2_utils_incdir_arr = [
    'lib' / 'include',
]

if host_machine.system() == 'linux'
    v4l2_utils_incdir_arr += 'include'
elif host_machine.system() == 'freebsd'
    v4l2_utils_incdir_arr += 'contrib/freebsd/include'
endif

# These platforms default to __[us]64 as long. Add this define to get long long
# as is done elsewhere.
if host_machine.cpu_family() in ['alpha', 'mips64', 'ppc64']
  add_project_arguments('-D__SANE_USERSPACE_TYPES__', language: ['c', 'cpp'])
endif

v4l2_utils_incdir = include_directories(v4l2_utils_incdir_arr)

prog_bash = find_program('bash')
prog_clang = find_program('clang', required : get_option('bpf'))
prog_doxygen = find_program('doxygen', required : get_option('doxygen-doc'))
prog_grep = find_program('grep')
prog_perl = find_program('perl')

dep_alsa = dependency('alsa', required : false)
if dep_alsa.found()
    conf.set('HAVE_ALSA', 1)
endif

dep_gl = dependency('gl', required : get_option('qvidcap').enabled())
dep_glu = dependency('glu', required : false)

dep_jsonc = dependency('json-c', required : get_option('v4l2-tracer'), version : '>=0.15')

dep_libdl = cc.find_library('dl')
dep_libelf = cc.find_library('elf', required : get_option('bpf'))
dep_libm = cc.find_library('m')
dep_librt = cc.find_library('rt')
dep_qt6 = dependency('qt6', modules: ['Core', 'Gui', 'Widgets', 'Core5Compat'], required: false)
if dep_qt6.found()
  dep_qt_opengl = dependency('qt6', modules: ['OpenGL', 'OpenGLWidgets'],
                             required : get_option('qvidcap').enabled())
  dep_qt = dep_qt6
  dep_qt_version = 'Qt6'
  dep_qt_options = ['cpp_std=gnu++17']
  dep_qt_args = ['-std=gnu++17']
else
  dep_qt = dependency('qt5', modules: ['Core', 'Gui', 'Widgets'],
                      required : get_option('qvidcap').enabled() or get_option('qv4l2').enabled())
  if dep_qt.found()
    dep_qt_version = 'Qt5'
  else
    dep_qt_version = ''
  endif
  dep_qt_opengl = dependency('qt5', modules: ['OpenGL'],
                             required : get_option('qvidcap').enabled())
  dep_qt_options = []
  dep_qt_args = []
endif

dep_libbpf = dependency('libbpf', required : get_option('bpf'), version : '>=0.7')

dep_sdl = dependency('SDL2', required: false)
if not dep_sdl.found()
    dep_sdl = cc.find_library('SDL2', has_headers: 'SDL2/SDL.h', required: false)
endif

dep_sdl_image = dependency('SDL2_image', required: false)
if not dep_sdl_image.found()
    dep_sdl_image = cc.find_library('SDL2_image', has_headers: 'SDL2/SDL_image.h', required: false)
endif

dep_threads = dependency('threads')
dep_x11 = dependency('x11', required : false)
dep_xmlrpc = dependency('xmlrpc', required : false)

have_fork = cc.has_function('fork', prefix: '#include <unistd.h>')
have_i2c_dev = cc.has_header('linux/i2c-dev.h')

if have_visibility
    conf.set('HAVE_VISIBILITY', 1)
endif

if cc.has_function('klogctl')
    conf.set('HAVE_KLOGCTL', 1)
endif

if cc.has_function('secure_getenv')
    conf.set('HAVE_SECURE_GETENV', 1)
endif

if cc.has_function('__secure_getenv')
    conf.set('HAVE___SECURE_GETENV', 1)
endif

if cc.has_header('sys/klog.h')
    conf.set('HAVE_SYS_KLOG_H', 1)
endif

if cc.has_header_symbol('execinfo.h', 'backtrace')
    conf.set('HAVE_BACKTRACE', 1)
endif

if cc.has_function('argp_parse')
    dep_argp = dependency('', required : false)
else
    dep_argp = cc.find_library('argp')
endif

if cc.has_function('strerrorname_np')
    conf.set('HAVE_STRERRORNAME_NP', 1)
endif

conf.set_quoted('LOCALEDIR', get_option('prefix') / get_option('datadir') / 'locale')

# Meson 0.60 handles the iconv dependency natively. For older versions, fall
# back to manual checks for the iconv_open function in libc, or finding an
# external library otherwise.
if meson.version().version_compare('>= 0.60')
    dep_iconv = dependency('iconv', required : false)
    found_iconv = dep_iconv.found()
else
    if cc.has_function('iconv_open')
        dep_iconv = []
        found_iconv = true
    else
        dep_iconv = [cc.find_library('iconv', required : false)]
        found_iconv = dep_iconv.found()
    endif
endif

if found_iconv
    conf.set('HAVE_ICONV', 1)
    iconv_const_test = '''#include <iconv.h>
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
'''
    if cc.compiles(iconv_const_test, dependencies : dep_iconv)
        conf.set('ICONV_CONST', '')
    else
        conf.set('ICONV_CONST', 'const')
    endif
endif

have_gconv = cc.has_header('gconv.h', required : get_option('gconv'))

# Detect system gconv directory
gconvsysdir = get_option('gconvsysdir')

# Generic check: works with most distributions
gconv_dirs_generic = [
    'lib64',
    'usr/lib64',
    'lib',
    'usr/lib',
    'usr/local/lib64',
    'usr/local/lib',
]

if gconvsysdir == ''
    foreach dir : gconv_dirs_generic
        dir = sys_root / dir / 'gconv'
        if fs.is_dir(dir)
            gconvsysdir = dir
            break
        endif
    endforeach
endif

# Debian/Ubuntu-specific check: should be aligned with the debian package
gconv_dirs_debian = [
    'aarch64-linux-gnu',
    'alphaev67-linux-gnu',
    'arm-linux-gnueabi',
    'arm-linux-gnueabihf',
    'i686-kfreebsd-gnu',
    'i686-linux-gnu',
    'mips-linux-gnu',
    'mips64-linux-gnuabi64',
    'mips64-linux-gnuabin32',
    'mips64el-linux-gnuabi64',
    'mips64el-linux-gnuabin32',
    'mipsel-linux-gnu',
    'mipsisa32r6-linux-gnu',
    'mipsisa32r6el-linux-gnu',
    'mipsisa64r6-linux-gnuabi64',
    'mipsisa64r6-linux-gnuabin32',
    'mipsisa64r6el-linux-gnuabi64',
    'mipsisa64r6el-linux-gnuabin32',
    'powerpc-linux-gnu',
    'powerpc64-linux-gnu',
    's390-linux-gnu',
    'sparc64-linux-gnu',
    'sparcv9-linux-gnu',
    'x86_64-linux-gnu',
    'x86_64-linux-gnux32',
]

if gconvsysdir == ''
    foreach dir : gconv_dirs_debian
        dir = sys_root / 'usr' / 'lib' / dir / 'gconv'
        if fs.is_dir(dir)
            gconvsysdir = dir
            break
        endif
    endforeach
endif

if gconvsysdir == ''
    dep_jis = cc.find_library('JIS', required : get_option('gconv'))
    dep_jisx0213 = cc.find_library('JISX0213', required : get_option('gconv'))
else
    dep_jis = cc.find_library('JIS', required : get_option('gconv'), dirs : gconvsysdir)
    dep_jisx0213 = cc.find_library('JISX0213', required : get_option('gconv'), dirs : gconvsysdir)
endif

dep_jpeg = dependency('libjpeg', required : get_option('jpeg'))
if dep_jpeg.found()
    dep_jpeg_priv_libs = '-ljpeg'
endif

dep_systemd = dependency('systemd', required : false)
systemd_systemdir = get_option('systemdsystemunitdir')
if systemd_systemdir == ''
    if dep_systemd.found()
        systemd_systemdir = dep_systemd.get_variable(pkgconfig : 'systemdsystemunitdir')
    endif
endif
if systemd_systemdir == ''
    systemd_systemdir = '/lib/systemd/system'
endif
# Since systemd v239, udevd is not allowed to execute BPF systems calls;
# add an override to allow bpf(2) in that case. On earlier versions, the
# override will restrict udevd to bpf syscall only and will stop the system
# from booting. This is also true on current debian versions.
have_udevdsyscallfilter = run_command(prog_grep, '-s', 'SystemCallFilter',
                                      systemd_systemdir / 'systemd-udevd.service',
                                      check : false).returncode() == 0

dep_libudev = dependency('libudev', required : get_option('libdvbv5'))
if dep_libudev.found()
    conf.set('HAVE_LIBUDEV', 1)
endif

udevdir = get_option('udevdir')
if udevdir == ''
    dep_udev = dependency('udev', required : false)
    if dep_udev.found()
        udevdir = dep_udev.get_variable(pkgconfig : 'udevdir')
    endif
endif
if udevdir == ''
    udevdir = '/lib/udev'
endif

docdir = get_option('docdir')
if docdir == ''
    docdir = get_option('datadir') / 'doc' / '@0@'.format(meson.project_name())
endif

qt_opengl_test = '''
#define GL_GLEXT_PROTOTYPES
#define QT_NO_OPENGL_ES_2

#include <QtCore>
#if QT_VERSION < 0x060000
#include <QGLWidget>
#include <QGLShader>
#include <QGLShaderProgram>
#include <QGLFunctions>
#else
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#endif

#ifndef GL_UNSIGNED_INT_8_8_8_8
#error Missing OpenGL Extensions
#endif
'''
have_qt_opengl = cpp.compiles(qt_opengl_test,
                               dependencies : [dep_gl, dep_qt, dep_qt_opengl],
                               args : ['-fPIC'] + dep_qt_args)
if have_qt_opengl
    conf.set('HAVE_QTGL', 1)
endif

ioctl_posix_test = '''
#include <sys/ioctl.h>
int ioctl (int, int, ...);
'''
if cc.compiles(ioctl_posix_test)
    conf.set('HAVE_POSIX_IOCTL', 1)
endif

c_arguments = []
cpp_arguments = []

c_arguments += common_arguments
cpp_arguments += common_arguments

add_project_arguments(c_arguments, language : 'c')
add_project_arguments(cpp_arguments, language : 'cpp')
add_project_link_arguments(cpp_arguments, language : 'cpp')

git_sha = ''
git_commit_cnt = ''
git_commit_date = ''

if fs.is_dir('.git')
    prog_git = find_program('git')
    git_sha = run_command(prog_git, '-C', source_root, 'rev-parse', '--short=12', 'HEAD',
                          check : true).stdout().strip()
    git_commit_cnt = '-' + run_command(prog_git, '-C', source_root, 'rev-list', '--count', 'HEAD',
                                 check : true).stdout().strip()
    git_commit_date = run_command(prog_git, '-C', source_root, 'show', '--quiet',
                                  '--date=format-local:%F %T', '--format=%cd',
                                  env : ['TZ=UTC'], check : true).stdout().strip()
endif

conf.set('GIT_SHA', git_sha)
conf.set('GIT_COMMIT_CNT', git_commit_cnt)
conf.set('GIT_COMMIT_DATE', git_commit_date)

man_pages = []

i18n_gettext_arguments = ['--directory=' + source_root,
                          '--keyword=_', '--keyword=N_', '--keyword=P_:1,2']
subdir('libdvbv5-po')
subdir('v4l-utils-po')

subdir('lib')

if get_option('v4l-utils')
    subdir('utils')
    subdir('contrib')
else
    ir_bpf_enabled = false
endif

subdir('doc')

configure_file(output : 'config.h', configuration : conf)

foreach m : man_pages
    configure_file(input : join_paths(m[0], '@0@.@1@.in'.format(m[1], m[2])),
                   output : '@0@.@1@'.format(m[1], m[2]),
                   install_dir : join_paths(get_option('mandir'), 'man@0@'.format(m[2])),
                   configuration : conf)
endforeach

configure_file(input : 'v4l-utils.spec.in', output : 'v4l-utils.spec',
               configuration : conf)

summary({
            'ALSA' : dep_alsa.found(),
            'GL' : dep_gl.found(),
            'GLU' : dep_glu.found(),
            'JSON-C' : dep_jsonc.found(),
            'Qt5/Qt6' : [
                dep_qt.found(),
                dep_qt_version + (have_qt_opengl ? ' with QtGL' : ' without QtGL'),
            ],
            'SDL' : dep_sdl.found(),
            'X11' : dep_x11.found(),
            'gconv' : have_gconv,
            'iconv' : found_iconv,
            'libjpeg' : dep_jpeg.found(),
            'libudev' : dep_libudev.found(),
            'threads' : dep_threads.found(),
        }, bool_yn : true, section : 'Dependencies')

summary({
            'udevdir' : udevdir,
        }, section : 'Directories')

summary({
            'libdvbv5' : is_variable('libdvbv5'),
            'v4l-plugins' : get_option('v4l-plugins'),
            'v4l-wrappers' : get_option('v4l-wrappers'),
        }, bool_yn : true, section : 'Libraries')

summary({
            'BPF IR decoders' : ir_bpf_enabled,
            'qv4l2' : is_variable('qv4l2'),
            'qvidcap' : is_variable('qvidcap'),
            'v4l-utils' : get_option('v4l-utils'),
            'v4l2-compliance' : [
                is_variable('v4l2_compliance'),
                get_option('v4l2-compliance-libv4l') ? 'with libv4l' : 'without libv4l',
            ],
            'v4l2-compliance-32' : is_variable('v4l2_compliance_32'),
            'v4l2-ctl' : [
                is_variable('v4l2_ctl'),
                get_option('v4l2-ctl-libv4l') ? 'with libv4l' : 'without libv4l',
            ],
            'v4l2-ctl-32' : is_variable('v4l2_ctl_32'),
            'v4l2-tracer' : is_variable('v4l2_tracer'),
        }, bool_yn : true, section : 'Applications')
