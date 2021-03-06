#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([DJINN], [0.1.0], [mk819@cam.ac.uk])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
: ${CXXFLAGS=-O3}


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LN_S

# Checks for libraries.
LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_PROG_LIBTOOL(libtool)


AC_ARG_WITH([zlib-path],
  [AS_HELP_STRING([--with-zlib-path=PATH],
                  [search path to ZLIB library])],
  [case $withval in
     yes) AC_MSG_ERROR([no path specified for --with-zlib-path]) ;;
     no)  with_zlib_path= ;;
   esac],
  [with_zlib_path=$with_zlib_path])
AC_SUBST([ZLIB_PATH], $with_zlib_path)
AM_CONDITIONAL([HAVE_ZLIB_PATH], [test $with_zlib_path])

AC_ARG_WITH([zstd-path],
  [AS_HELP_STRING([--with-zstd-path=PATH],
                  [search path to ZSTD library])],
  [case $withval in
     yes) AC_MSG_ERROR([no path specified for --with-zstd-path]) ;;
     no)  with_zstd_path= ;;
   esac],
  [with_zstd_path=$with_zstd_path])
AC_SUBST([ZSTD_PATH], $with_zstd_path)
AM_CONDITIONAL([HAVE_ZSTD_PATH], [test $with_zstd_path])

AC_ARG_WITH([lz4-path],
  [AS_HELP_STRING([--with-lz4-path=PATH],
                  [search path to LZ4 library])],
  [case $withval in
     yes) AC_MSG_ERROR([no path specified for --with-lz4-path]) ;;
     no)  with_lz4_path= ;;
   esac],
  [with_lz4_path=$with_lz4_path])
AC_SUBST([LZ4_PATH], $with_lz4_path)
AM_CONDITIONAL([HAVE_LZ4_PATH], [test $with_lz4_path])

AC_ARG_WITH([htslib-path],
  [AS_HELP_STRING([--with-htslib-path=PATH],
                  [search path to HtsLib library])],
  [case $withval in
     yes) AC_MSG_ERROR([no path specified for --with-htslib-path]) ;;
     no)  with_htslib_path= ;;
   esac],
  [with_htslib_path=$with_htslib_path])
AC_SUBST([HTSLIB_PATH], $with_htslib_path)
AM_CONDITIONAL([HAVE_HTSLIB_PATH], [test $with_htslib_path])

zlib_devel=ok
AC_CHECK_HEADER([zlib.h], [], [zlib_devel=missing], [;])
AC_CHECK_LIB(z, inflate,  [], [zlib_devel=missing])

if test $zlib_devel != ok; then
  AC_MSG_NOTICE([zlib development files not found

Djinn and HTSlib uses compression routines from the zlib library <http://zlib.net>.
Building Djinn does not require zlib development files to be installed on the build
machine but is required for full functionality; you may want to ensure a package 
such as zlib1g-dev (on Debian or Ubuntu Linux) or zlib-devel (on RPM-based Linux 
distributions or Cygwin) is installed.])
fi

hts_devel=ok
AC_CHECK_HEADER([htslib/vcf.h], [], [hts_devel=missing], [;])
AC_CHECK_LIB(hts, bcf_hdr_read, [], [hts_devel=missing])

lz4_devel=ok
AC_CHECK_HEADER([lz4.h], [], [lz4_devel=missing], [;])
AC_CHECK_LIB(lz4, LZ4_compress_HC,  [], [lz4_devel=missing])

if test $lz4_devel != ok; then
  AC_MSG_NOTICE([lz4 development files not found

Djinn uses compression routines from the lz4 library <https://lz4.github.io/lz4/>.
Building Djinn does not require lz4 development files to be installed on the build
machine but is required for full functionality; you may want to ensure that this
package is installed.])
fi

zstd_devel=ok
AC_CHECK_HEADER([zstd.h], [], [zstd_devel=missing], [;])
AC_CHECK_LIB(zstd, ZSTD_compress,  [], [zstd_devel=missing])

if test $zstd_devel != ok; then
  AC_MSG_NOTICE([zstd development files not found

Djinn uses compression routines from the zstd library <https://facebook.github.io/zstd>.
Building Djinn does not require zstd development files to be installed on the build
machine but is required for full functionality; you may want to ensure a package 
such as zstd (on Debian or Ubuntu Linux) or zstd (on RPM-based Linux 
distributions or Cygwin) is installed.])
fi

AM_CONDITIONAL([ZSTD_AVAIL], [test $zstd_devel == ok])
AM_CONDITIONAL([LZ4_AVAIL], [test $lz4_devel == ok])
AM_CONDITIONAL([HTS_AVAIL], [test $hts_devel == ok])
AM_CONDITIONAL([ZLIB_AVAIL], [test $zlib_devel == ok])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
