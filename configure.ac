# -*- Autoconf -*-

# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([v4l-utils],[0.9.0-test1])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([lib/libv4l2/libv4l2.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile
	lib/libv4lconvert/Makefile
	lib/libv4l2/Makefile
	lib/libv4l1/Makefile
	lib/libdvbv5/Makefile

	utils/libv4l2util/Makefile
	utils/libmedia_dev/Makefile
	utils/decode_tm6000/Makefile
	utils/dvb/Makefile
	utils/keytable/Makefile
	utils/rds/Makefile
	utils/v4l2-compliance/Makefile
	utils/v4l2-ctl/Makefile
	utils/v4l2-dbg/Makefile
	utils/v4l2-sysfs-path/Makefile
	utils/xc3028-firmware/Makefile
	utils/qv4l2/Makefile

	contrib/freebsd/Makefile
	contrib/test/Makefile

	v4l-utils.spec
	lib/libv4lconvert/libv4lconvert.pc
	lib/libv4l1/libv4l1.pc
	lib/libv4l2/libv4l2.pc
	lib/libdvbv5/libdvbv5.pc
])

AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-bzip2 -Wno-portability]) # 1.10 is needed for target_LIBTOOLFLAGS

AM_MAINTAINER_MODE
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

# sync-with-kernel and keytable install could make use of them
#AC_PROG_GREP
#AC_PROG_SED
#AC_PROG_MKDIR_P
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
   AC_SUBST(MOC)
   AC_SUBST(UIC)
else
   AC_MSG_WARN(Qt4 is not available)
fi

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

AM_GNU_GETTEXT_VERSION([0.17])
AM_GNU_GETTEXT([external])
AM_ICONV

# directories

AC_ARG_WITH(libv4l1subdir, AS_HELP_STRING(--with-libv4l1subdir=DIR,set libv4l1 library subdir [default=libv4l]),
   libv4l1subdir=$withval, libv4l1subdir="libv4l")

AC_ARG_WITH(libv4l2subdir, AS_HELP_STRING(--with-libv4l2subdir=DIR,set libv4l2 library subdir [default=libv4l]),
   libv4l2subdir=$withval, libv4l2subdir="libv4l")

AC_ARG_WITH(libv4lconvertsubdir, AS_HELP_STRING(--with-libv4lconvertsubdir=DIR,set libv4lconvert library subdir [default=libv4l]),
   libv4lconvertsubdir=$withval, libv4lconvertsubdir="libv4l")

libv4l1privdir="$libdir/$libv4l1subdir"
libv4l2privdir="$libdir/$libv4l2subdir"
libv4l2plugindir="$libv4l2privdir/plugins"
libv4lconvertprivdir="$libdir/$libv4lconvertsubdir"

rootetcdir="/etc"
rootlibdir="/lib"
pkgconfigdir="$libdir/pkgconfig"

AC_SUBST(libv4l1privdir)
AC_SUBST(libv4l2privdir)
AC_SUBST(libv4l2plugindir)
AC_SUBST(libv4lconvertprivdir)
AC_SUBST(rootetcdir)
AC_SUBST(rootlibdir)
AC_SUBST(pkgconfigdir)

AC_DEFINE_UNQUOTED([V4L_UTILS_VERSION], ["$PACKAGE_VERSION"], [v4l-utils version string])
AC_DEFINE_DIR([LIBV4L1_PRIV_DIR], [libv4l1privdir], [libv4l1 private lib directory])
AC_DEFINE_DIR([LIBV4L2_PRIV_DIR], [libv4l2privdir], [libv4l2 private lib directory])
AC_DEFINE_DIR([LIBV4L2_PLUGIN_DIR], [libv4l2plugindir], [libv4l2 plugin directory])
AC_DEFINE_DIR([LIBV4LCONVERT_PRIV_DIR], [libv4lconvertprivdir], [libv4lconvert private lib directory])

# options

AC_ARG_ENABLE(libdvbv5,
  [  --disable-libdvbv5      disable dynamic libdvbv5 compilation],
  [case "${enableval}" in
     yes | no ) with_liblibdvbv5="${enableval}" ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-libdvbv5) ;;
   esac],
  [with_libdvbv5="yes"]
)

AC_ARG_ENABLE(libv4l,
  [  --disable-libv4l        disable dynamic libv4l compilation],
  [case "${enableval}" in
     yes | no ) with_libv4l="${enableval}" ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-libv4l) ;;
   esac],
  [with_libv4l="yes"]
)

AC_ARG_ENABLE(v4l-utils,
  [  --disable-v4l-utils     disable v4l-utils compilation],
  [case "${enableval}" in
     yes | no ) with_v4lutils="${enableval}" ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-v4l-utils) ;;
   esac],
  [with_v4lutils="yes"]
)

AC_ARG_ENABLE(qv4l2,
  [  --disable-qv4l2         disable qv4l2 compilation],
  [case "${enableval}" in
     yes | no ) with_qv4l2="${enableval}" ;;
     *) AC_MSG_ERROR(bad value ${enableval} for --disable-qv4l2) ;;
   esac],
  [with_qv4l2="yes"]
)

AM_CONDITIONAL([WITH_LIBDVBV5], [test x$with_libdvbv5 = xyes])
AM_CONDITIONAL([WITH_LIBV4L], [test x$with_libv4l = xyes])
AM_CONDITIONAL([WITH_V4LUTILS], [test x$with_v4lutils = xyes])
AM_CONDITIONAL([WITH_QV4L2], [test ${qt_pkgconfig} = true -a x$with_qv4l2 = xyes])
AM_CONDITIONAL([WITH_V4L_WRAPPERS], [test x$with_libv4l = xyes -a x$enable_shared = xyes])

# append -static to libtool compile and link command to enforce static libs
AS_IF([test x$with_libdvbv5 != xyes], [AC_SUBST([ENFORCE_LIBDVBV5_STATIC], ["-static"])])
AS_IF([test x$with_libv4l != xyes], [AC_SUBST([ENFORCE_LIBV4L_STATIC], ["-static"])])

# misc

if test "x$linux_os" = "xyes"; then
  CPPFLAGS="-I\$(top_srcdir)/include $CPPFLAGS"
else
  CPPFLAGS="-I\$(top_srcdir)/contrib/freebsd/include $CPPFLAGS"
fi

CPPFLAGS="-I\$(top_srcdir)/lib/include -Wall -Wpointer-arith -D_GNU_SOURCE $CPPFLAGS"

AC_OUTPUT
