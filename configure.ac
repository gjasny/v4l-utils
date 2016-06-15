# -*- Autoconf -*-

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([v4l-utils],[1.10.1])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lib/libv4l2/libv4l2.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
	lib/Makefile
	lib/libv4lconvert/Makefile
	lib/libv4l2/Makefile
	lib/libv4l1/Makefile
	lib/libdvbv5/Makefile
	lib/libv4l2rds/Makefile
	lib/libv4l-mplane/Makefile

	utils/Makefile
	utils/libv4l2util/Makefile
	utils/libmedia_dev/Makefile
	utils/decode_tm6000/Makefile
	utils/dvb/Makefile
	utils/keytable/Makefile
	utils/cx18-ctl/Makefile
	utils/ivtv-ctl/Makefile
	utils/media-ctl/Makefile
	utils/rds/Makefile
	utils/v4l2-compliance/Makefile
	utils/v4l2-ctl/Makefile
	utils/v4l2-dbg/Makefile
	utils/v4l2-sysfs-path/Makefile
	utils/xc3028-firmware/Makefile
	utils/qv4l2/Makefile
	utils/rds-ctl/Makefile

	contrib/Makefile
	contrib/freebsd/Makefile
	contrib/test/Makefile
	contrib/gconv/Makefile
	contrib/cobalt-ctl/Makefile

	v4l-utils-po/Makefile.in
	libdvbv5-po/Makefile.in

	v4l-utils.spec
	lib/libv4lconvert/libv4lconvert.pc
	lib/libv4l1/libv4l1.pc
	lib/libv4l2/libv4l2.pc
	lib/libdvbv5/libdvbv5.pc
	lib/libv4l2rds/libv4l2rds.pc
	utils/media-ctl/libmediactl.pc
	utils/media-ctl/libv4l2subdev.pc

	utils/qv4l2/qv4l2.1
	utils/v4l2-compliance/v4l2-compliance.1
	utils/v4l2-ctl/v4l2-ctl.1
	utils/keytable/ir-keytable.1
	utils/dvb/dvb-fe-tool.1
	utils/dvb/dvbv5-scan.1
	utils/dvb/dvb-format-convert.1
	utils/dvb/dvbv5-zap.1
])

AC_GNU_SOURCE

AM_INIT_AUTOMAKE([1.9 subdir-objects no-dist-gzip dist-bzip2 -Wno-portability]) # 1.10 is needed for target_LIBTOOLFLAGS

AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC_C99
AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MKDIR_P
AC_PROG_LN_S
PKG_PROG_PKG_CONFIG
DX_DOT_FEATURE(ON)
DX_INIT_DOXYGEN($PACKAGE_NAME, doxygen_libdvbv5.cfg)
ALL_LINGUAS=""
AM_GNU_GETTEXT([external])

LIBDVBV5_DOMAIN="libdvbv5"
AC_DEFINE([LIBDVBV5_DOMAIN], "libdvbv5", [libdvbv5 domain])
AC_SUBST(LIBDVBV5_DOMAIN)

# Define localedir
AC_DEFUN([V4L_EXPAND_PREFIX], [
	$1=$2
	dnl expanding twice, since from autoconf 2.60 on, $datadir refers to
	dnl $datarootdir which in turn refers to $prefix
	$1=`(	test "x$prefix" = xNONE && prefix="$ac_default_prefix"
		eval tmp_v4l_prefix=\""[$]$1"\"
		eval echo $tmp_v4l_prefix
		)`
])

V4L_EXPAND_PREFIX(v4l_localedir, "$datadir/locale")
localedir="\$(datadir)/locale"

AC_ARG_WITH(localedir,
	[  --with-localedir=PATH      Where the locale files are installed ])
if test "x$with_localedir" != "x"; then
	v4l_localedir="$with_localedir"
	localedir="$with_localedir"
fi

AC_DEFINE_UNQUOTED(LOCALEDIR, "$v4l_localedir", [locale directory])
AC_SUBST(localedir)

# sync-with-kernel and keytable install could make use of them
#AC_PROG_GREP
#AC_PROG_SED
gl_PROMOTED_TYPE_MODE_T
gl_VISIBILITY

AC_CHECK_HEADERS([sys/klog.h])
AC_CHECK_FUNCS([klogctl])

AC_CACHE_CHECK([for ioctl with POSIX signature],
  [gl_cv_func_ioctl_posix_signature],
  [AC_COMPILE_IFELSE(
     [AC_LANG_PROGRAM(
        [[#include <sys/ioctl.h>]],
        [[int ioctl (int, int, ...);]])
     ],
     [gl_cv_func_ioctl_posix_signature=yes],
     [gl_cv_func_ioctl_posix_signature=no])
  ])
if test "x$gl_cv_func_ioctl_posix_signature" = xyes; then
  AC_DEFINE([HAVE_POSIX_IOCTL], [1], [Have ioctl with POSIX signature])
fi

AC_CHECK_FUNCS([__secure_getenv secure_getenv])

# Check host os
case "$host_os" in
  linux*)
    linux_os="yes"
    ;;
esac

AM_CONDITIONAL([LINUX_OS], [test x$linux_os = xyes])

# Checks for dependencies

PKG_CHECK_MODULES(X11, [x11], [x11_pkgconfig=yes], [x11_pkgconfig=no])
AC_SUBST([X11_CFLAGS])
AC_SUBST([X11_LIBS])
AM_CONDITIONAL([HAVE_X11], [test x$x11_pkgconfig = xyes])

AS_IF([test "x$x11_pkgconfig" = xyes],
      [PKG_CHECK_MODULES(GL, [gl], [gl_pkgconfig=yes], [gl_pkgconfig=no])])
AC_SUBST([GL_CFLAGS])
AC_SUBST([GL_LIBS])
AM_CONDITIONAL([HAVE_GL], [test x$gl_pkgconfig = xyes])

AS_IF([test "x$gl_pkgconfig" = xyes],
      [PKG_CHECK_MODULES(GLU, [glu], [glu_pkgconfig=yes], [glu_pkgconfig=no])])

AC_SUBST([GLU_CFLAGS])
AC_SUBST([GLU_LIBS])
AM_CONDITIONAL([HAVE_GLU], [test x$glu_pkgconfig = xyes])

AC_ARG_WITH([jpeg],
            AS_HELP_STRING([--without-jpeg], [Do not use jpeg library]),
            [],
            [with_jpeg=yes])

AS_IF([test "x$with_jpeg" != xno],
      [AC_CHECK_HEADER([jpeglib.h],
                       [AC_CHECK_LIB([jpeg],
                                     [jpeg_read_header],
                                     [have_jpeg=:
                                      JPEG_LIBS="-ljpeg"
                                      AC_DEFINE([HAVE_JPEG],[1],[whether we use libjpeg])],
                                     [AC_MSG_ERROR(cannot find libjpeg (v6 or later required))])],
                       [AC_MSG_ERROR(cannot find jpeglib.h)])])

AM_CONDITIONAL([HAVE_JPEG], [$have_jpeg])

PKG_CHECK_MODULES(QT5, [Qt5Core >= 5.0 Qt5Gui >= 5.0 Qt5Widgets >= 5.0], [qt_pkgconfig=true], [qt_pkgconfig=false])
if test "x$qt_pkgconfig" = "xtrue"; then
   QT_CFLAGS="$QT5_CFLAGS -fPIC"
   QT_LIBS="$QT5_LIBS"
   AC_SUBST(QT_CFLAGS)
   AC_SUBST(QT_LIBS)
   AC_CHECK_PROGS(MOC, [moc-qt5 moc])
   AC_CHECK_PROGS(UIC, [uic-qt5 uic])
   AC_CHECK_PROGS(RCC, [rcc-qt5 rcc])
   AC_SUBST(MOC)
   AC_SUBST(UIC)
   AC_SUBST(RCC)
   PKG_CHECK_MODULES(QT5GL, [Qt5OpenGL >= 5.0 gl], [qt_pkgconfig_gl=true], [qt_pkgconfig_gl=false])
   if test "x$qt_pkgconfig_gl" = "xtrue"; then
      QTGL_CFLAGS="$QT5GL_CFLAGS -fPIC"
      QTGL_LIBS="$QT5GL_LIBS"
      AC_SUBST(QT_CFLAGS)
      AC_SUBST(QT_LIBS)
      AC_DEFINE([HAVE_QTGL], [1], [qt has opengl support])
   else
      AC_MSG_WARN(Qt5 OpenGL is not available)
    fi
else
   PKG_CHECK_MODULES(QT, [QtCore >= 4.0 QtGui >= 4.0], [qt_pkgconfig=true], [qt_pkgconfig=false])
   if test "x$qt_pkgconfig" = "xtrue"; then
      MOC=`$PKG_CONFIG --variable=moc_location QtCore`
      UIC=`$PKG_CONFIG --variable=uic_location QtCore`
      RCC=`$PKG_CONFIG --variable=rcc_location QtCore`
      if test -z "$RCC"; then
         RCC="rcc"
      fi
      AC_SUBST(MOC)
      AC_SUBST(UIC)
      AC_SUBST(RCC)
      PKG_CHECK_MODULES(QTGL, [QtOpenGL >= 4.8 gl], [qt_pkgconfig_gl=true], [qt_pkgconfig_gl=false])
      if test "x$qt_pkgconfig_gl" = "xtrue"; then
         AC_DEFINE([HAVE_QTGL], [1], [qt has opengl support])
      else
         AC_MSG_WARN(Qt4 OpenGL is not available)
      fi
   else
      AC_MSG_WARN(Qt4 or higher is not available)
   fi
fi


PKG_CHECK_MODULES(ALSA, [alsa], [alsa_pkgconfig=true], [alsa_pkgconfig=false])
if test "x$alsa_pkgconfig" = "xtrue"; then
   AC_DEFINE([HAVE_ALSA], [1], [alsa library is present])
else
   AC_MSG_WARN(ALSA library not available)
fi

AC_ARG_WITH([libudev],
    AS_HELP_STRING([--with-libudev],
	[Enable libudev to detect a device name]))

AS_IF([test "x$with_libudev" = "xyes"],
    [PKG_CHECK_MODULES(libudev, libudev, have_libudev=yes, have_libudev=no)],
    [have_libudev=no])

AS_IF([test "x$have_libudev" = "xyes"],
    [
	AC_DEFINE([HAVE_LIBUDEV], [], [Use libudev])
	LIBUDEV_CFLAGS="$libudev_CFLAGS"
	LIBUDEV_LIBS="$libudev_LIBS"
	AC_SUBST(LIBUDEV_CFLAGS)
	AC_SUBST(LIBUDEV_LIBS)
    ],
    [AS_IF([test "x$with_libudev" = "xyes"],
	[AC_MSG_ERROR([libudev requested but not found])
    ])
])

AC_SUBST([JPEG_LIBS])

# The dlopen() function is in the C library for *BSD and in
# libdl on GLIBC-based systems

dl_saved_libs=$LIBS
  AC_SEARCH_LIBS([dlopen],
                 [dl],
                 [test "$ac_cv_search_dlopen" = "none required" || DLOPEN_LIBS=$ac_cv_search_dlopen],
                 [AC_MSG_ERROR([unable to find the dlopen() function])])
  AC_SUBST([DLOPEN_LIBS])
LIBS=$dl_saved_libs

AC_CHECK_HEADER([argp.h],,AC_MSG_ERROR(Cannot continue: argp.h not found))
argp_saved_libs=$LIBS
  AC_SEARCH_LIBS([argp_parse],
                 [argp],
                 [test "$ac_cv_search_argp_parse" = "none required" || ARGP_LIBS=$ac_cv_search_argp_parse],
                 [AC_MSG_ERROR([unable to find the argp_parse() function])])
  AC_SUBST([ARGP_LIBS])
LIBS=$argp_saved_libs

AC_CHECK_HEADER([linux/i2c-dev.h], [linux_i2c_dev=yes], [linux_i2c_dev=no])
AM_CONDITIONAL([HAVE_LINUX_I2C_DEV], [test x$linux_i2c_dev = xyes])

AM_ICONV

# directories

AC_ARG_WITH(libv4l1subdir,
	AS_HELP_STRING([--with-libv4l1subdir=DIR], [set libv4l1 library subdir (default=libv4l)]),
	[],
	[with_libv4l1subdir="libv4l"])

AC_ARG_WITH(libv4l2subdir,
	AS_HELP_STRING([--with-libv4l2subdir=DIR], [set libv4l2 library subdir (default=libv4l)]),
   	[],
	[with_libv4l2subdir="libv4l"])

AC_ARG_WITH(libv4lconvertsubdir,
	AS_HELP_STRING([--with-libv4lconvertsubdir=DIR], [set libv4lconvert library subdir (default=libv4l)]),
	[],
	[with_libv4lconvertsubdir="libv4l"])

AC_ARG_WITH(udevdir,
	AS_HELP_STRING([--with-udevdir=DIR], [set udev directory]),
   	[],
	[with_udevdir=`$PKG_CONFIG --variable=udevdir udev || echo /lib/udev`])

def_gconv_dir=`for i in /lib64 /usr/lib64 /usr/local/lib64 /lib /usr/lib /usr/local/lib; do if @<:@ -d \$i/gconv @:>@; then echo \$i/gconv; break; fi; done || echo /usr/lib/gconv`
AC_ARG_WITH(gconvdir,
	AS_HELP_STRING([--with-gconvdir=DIR],
		       [set system's gconv directory (default=/usr/lib/gconv)]),
	[],
	[with_gconvdir=$def_gconv_dir])

AC_SUBST([libv4l1subdir], [$with_libv4l1subdir])
AC_SUBST([libv4l2subdir], [$with_libv4l2subdir])
AC_SUBST([libv4l1privdir], [$libdir/$with_libv4l1subdir])
AC_SUBST([libv4l2privdir], [$libdir/$with_libv4l2subdir])
AC_SUBST([libv4l2plugindir], [$libdir/$with_libv4l2subdir/plugins])
AC_SUBST([libv4lconvertprivdir], [$libdir/$with_libv4lconvertsubdir])
AC_SUBST([keytablesystemdir], [$with_udevdir/rc_keymaps])
AC_SUBST([keytableuserdir], [$sysconfdir/rc_keymaps])
AC_SUBST([udevrulesdir], [$with_udevdir/rules.d])
AC_SUBST([pkgconfigdir], [$libdir/pkgconfig])
AC_SUBST([gconvsysdir], [$with_gconvdir])

AC_DEFINE_UNQUOTED([V4L_UTILS_VERSION], ["$PACKAGE_VERSION"], [v4l-utils version string])
AC_DEFINE_DIR([LIBV4L1_PRIV_DIR], [libv4l1privdir], [libv4l1 private lib directory])
AC_DEFINE_DIR([LIBV4L2_PRIV_DIR], [libv4l2privdir], [libv4l2 private lib directory])
AC_DEFINE_DIR([LIBV4L2_PLUGIN_DIR], [libv4l2plugindir], [libv4l2 plugin directory])
AC_DEFINE_DIR([LIBV4LCONVERT_PRIV_DIR], [libv4lconvertprivdir], [libv4lconvert private lib directory])
AC_DEFINE_DIR([IR_KEYTABLE_SYSTEM_DIR], [keytablesystemdir], [ir-keytable preinstalled tables directory])
AC_DEFINE_DIR([IR_KEYTABLE_USER_DIR], [keytableuserdir], [ir-keytable user defined tables directory])

# options

AC_ARG_ENABLE(libdvbv5,
  AS_HELP_STRING([--disable-libdvbv5], [disable experimental dynamic libdvbv5 compilation]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-libdvbv5) ;;
   esac]
)

AC_ARG_ENABLE(libv4l,
  AS_HELP_STRING([--disable-libv4l], [disable dynamic libv4l compilation]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l) ;;
   esac]
)

AC_ARG_ENABLE(v4l-utils,
  AS_HELP_STRING([--disable-v4l-utils], [disable v4l-utils compilation]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-v4l-utils) ;;
   esac]
)

AC_ARG_ENABLE(v4l2-compliance-libv4l,
  AS_HELP_STRING([--disable-v4l2-compliance-libv4l], [disable use of libv4l in v4l2-compliance]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-v4l2-compliance-libv4l) ;;
   esac]
)

AC_ARG_ENABLE(v4l2-ctl-libv4l,
  AS_HELP_STRING([--disable-v4l2-ctl-libv4l], [disable use of libv4l in v4l2-ctl]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-v4l2-ctl-libv4l) ;;
   esac]
)

AC_ARG_ENABLE(qv4l2,
  AS_HELP_STRING([--disable-qv4l2], [disable qv4l2 compilation]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-qv4l2) ;;
   esac]
)

AC_ARG_ENABLE(gconv,
  AS_HELP_STRING([--enable-gconv], [enable compilation of gconv modules]),
  [case "${enableval}" in
    yes | no ) [test x$enable_shared == xno -o x$LIBICONV != x && enable_gconv = no] ;;
    *) AC_MSG_ERROR(bad value ${enableval} for --enable-gconv) ;;
   esac]
)

AM_CONDITIONAL([WITH_LIBDVBV5],     [test x$enable_libdvbv5  != xno])
AM_CONDITIONAL([WITH_LIBV4L],       [test x$enable_libv4l    != xno])
AM_CONDITIONAL([WITH_V4LUTILS],	    [test x$enable_v4l_utils != xno -a x$linux_os = xyes])
AM_CONDITIONAL([WITH_QV4L2],	    [test ${qt_pkgconfig}  = true -a x$enable_qv4l2 != xno])
AM_CONDITIONAL([WITH_V4L_PLUGINS],  [test x$enable_libv4l != xno -a x$enable_shared != xno])
AM_CONDITIONAL([WITH_V4L_WRAPPERS], [test x$enable_libv4l != xno -a x$enable_shared != xno])
AM_CONDITIONAL([WITH_QTGL],	    [test ${qt_pkgconfig_gl} = true])
AM_CONDITIONAL([WITH_GCONV],        [test x${enable_gconv} = xyes])
AM_CONDITIONAL([WITH_V4L2_CTL_LIBV4L], [test x${enable_v4l2_ctl_libv4l} != xno])
AM_CONDITIONAL([WITH_V4L2_COMPLIANCE_LIBV4L], [test x${enable_v4l2_compliance_libv4l} != xno])

# append -static to libtool compile and link command to enforce static libs
AS_IF([test x$enable_libdvbv5 = xno], [AC_SUBST([ENFORCE_LIBDVBV5_STATIC], ["-static"])])
AS_IF([test x$enable_libv4l = xno],   [AC_SUBST([ENFORCE_LIBV4L_STATIC],   ["-static"])])

# misc

if test "x$linux_os" = "xyes"; then
  CPPFLAGS="-I\$(top_srcdir)/include $CPPFLAGS"
else
  CPPFLAGS="-I\$(top_srcdir)/contrib/freebsd/include $CPPFLAGS"
fi

CPPFLAGS="-I\$(top_srcdir)/lib/include -Wall -Wpointer-arith -D_GNU_SOURCE $CPPFLAGS"

AC_OUTPUT
