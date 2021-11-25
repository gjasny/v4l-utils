#ifndef __V4L_ANDROID_CONFIG_H__
#define __V4L_ANDROID_CONFIG_H__
/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* alsa library is present */
/* #undef HAVE_ALSA */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* whether we use libjpeg */
/* #undef HAVE_JPEG */

/* Define to 1 if you have the `klogctl' function. */
#define HAVE_KLOGCTL 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* qt has opengl support */
/* #undef HAVE_QTGL */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/klog.h> header file. */
#define HAVE_SYS_KLOG_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#define HAVE_VISIBILITY 1

/* Define as const if the declaration of iconv() needs const. */
/* #undef ICONV_CONST */

/* ir-keytable preinstalled tables directory */
#define IR_KEYTABLE_SYSTEM_DIR "/lib/udev/rc_keymaps"

/* ir-keytable user defined tables directory */
#define IR_KEYTABLE_USER_DIR "/system/etc/rc_keymaps"

/* libv4l1 private lib directory */
#define LIBV4L1_PRIV_DIR "/system/lib/libv4l"

/* libv4l2 plugin directory */
#define LIBV4L2_PLUGIN_DIR "/system/lib/libv4l/plugins"

/* libv4l2 private lib directory */
#define LIBV4L2_PRIV_DIR "/system/lib/libv4l"

/* libv4lconvert private lib directory */
#define LIBV4LCONVERT_PRIV_DIR "/system/lib/libv4l"

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "v4l-utils"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "v4l-utils"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "v4l-utils 1.1.0"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "v4l-utils"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.1.0"

/* Define to the type that is the result of default argument promotions of
   type mode_t. */
#define PROMOTED_MODE_T int

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* v4l-utils version string */
#define V4L_UTILS_VERSION "1.1.0"

/* Version number of package */
#define VERSION "1.1.0"

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

#endif
