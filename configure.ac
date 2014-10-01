# -*- Autoconf -*-

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([v4l-utils],[1.5.90])

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

	v4l-utils.spec
	lib/libv4lconvert/libv4lconvert.pc
	lib/libv4l1/libv4l1.pc
	lib/libv4l2/libv4l2.pc
	lib/libdvbv5/libdvbv5.pc
	lib/libv4l2rds/libv4l2rds.pc
	utils/media-ctl/libmediactl.pc
	utils/media-ctl/libv4l2subdev.pc
])

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

# sync-with-kernel and keytable install could make use of them
#AC_PROG_GREP
#AC_PROG_SED
gl_PROMOTED_TYPE_MODE_T
gl_VISIBILITY

AC_CHECK_HEADERS([sys/klog.h])
AC_CHECK_FUNCS([klogctl])

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

PKG_CHECK_MODULES(QT, [QtCore >= 4.4 QtGui >= 4.4], [qt_pkgconfig=true], [qt_pkgconfig=false])
if test "x$qt_pkgconfig" = "xtrue"; then
   AC_SUBST(QT_CFLAGS)
   AC_SUBST(QT_LIBS)
   MOC=`$PKG_CONFIG --variable=moc_location QtCore`
   UIC=`$PKG_CONFIG --variable=uic_location QtCore`
   RCC=`$PKG_CONFIG --variable=rcc_location QtCore`
   if test -z "$RCC"; then
      RCC="rcc"
   fi
   AC_SUBST(MOC)
   AC_SUBST(UIC)
   AC_SUBST(RCC)
else
   AC_MSG_WARN(Qt4 or higher is not available)
fi

PKG_CHECK_MODULES(QTGL, [QtOpenGL >= 4.4 gl], [qt_pkgconfig_gl=true], [qt_pkgconfig_gl=false])
if test "x$qt_pkgconfig_gl" = "xtrue"; then
   AC_DEFINE([HAVE_QTGL], [1], [qt has opengl support])
else
   AC_MSG_WARN(Qt4 OpenGL or higher is not available)
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

AC_ARG_ENABLE(qv4l2,
  AS_HELP_STRING([--disable-qv4l2], [disable qv4l2 compilation]),
  [case "${enableval}" in
     yes | no ) ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --enable-qv4l2) ;;
   esac]
)

AM_CONDITIONAL([WITH_LIBDVBV5],     [test x$enable_libdvbv5  != xno])
AM_CONDITIONAL([WITH_LIBV4L],       [test x$enable_libv4l    != xno])
AM_CONDITIONAL([WITH_V4LUTILS],	    [test x$enable_v4l_utils != xno -a x$linux_os = xyes])
AM_CONDITIONAL([WITH_QV4L2],	    [test ${qt_pkgconfig}  = true -a x$enable_qv4l2 != xno])
AM_CONDITIONAL([WITH_V4L_PLUGINS],  [test x$enable_libv4l != xno -a x$enable_shared != xno])
AM_CONDITIONAL([WITH_V4L_WRAPPERS], [test x$enable_libv4l != xno -a x$enable_shared != xno])
AM_CONDITIONAL([WITH_QTGL],	    [test ${qt_pkgconfig_gl} = true])

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
