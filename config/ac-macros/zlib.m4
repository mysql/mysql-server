dnl Define zlib paths to point at bundled zlib

AC_DEFUN([MYSQL_USE_BUNDLED_ZLIB], [
ZLIB_INCLUDES="-I\$(top_srcdir)/zlib"
ZLIB_LIBS="\$(top_builddir)/zlib/libz.la"
zlib_dir="zlib"
AC_SUBST([zlib_dir])
mysql_cv_compress="yes"
])

dnl Auxiliary macro to check for zlib at given path

AC_DEFUN([MYSQL_CHECK_ZLIB_DIR], [
save_INCLUDES="$INCLUDES"
save_LIBS="$LIBS"
INCLUDES="$INCLUDES $ZLIB_INCLUDES"
LIBS="$LIBS $ZLIB_LIBS"
AC_CACHE_VAL([mysql_cv_compress],
  [AC_TRY_LINK([#include <zlib.h>],
    [return compress(0, (unsigned long*) 0, "", 0);],
    [mysql_cv_compress="yes"
    AC_MSG_RESULT([ok])],
    [mysql_cv_compress="no"])
  ])
INCLUDES="$save_INCLUDES"
LIBS="$save_LIBS"
])

dnl MYSQL_CHECK_ZLIB_WITH_COMPRESS
dnl ------------------------------------------------------------------------
dnl @synopsis MYSQL_CHECK_ZLIB_WITH_COMPRESS
dnl
dnl Provides the following configure options:
dnl --with-zlib-dir=DIR
dnl Possible DIR values are:
dnl - "no" - the macro will disable use of compression functions
dnl - "bundled" - means use zlib bundled along with MySQL sources
dnl - empty, or not specified - the macro will try default system
dnl   library (if present), and in case of error will fall back to 
dnl   bundled zlib
dnl - zlib location prefix - given location prefix, the macro expects
dnl   to find the library headers in $prefix/include, and binaries in
dnl   $prefix/lib. If zlib headers or binaries weren't found at $prefix, the
dnl   macro bails out with error.
dnl 
dnl If the library was found, this function #defines HAVE_COMPRESS
dnl and configure variables ZLIB_INCLUDES (i.e. -I/path/to/zlib/include) and
dnl ZLIB_LIBS (i. e. -L/path/to/zlib/lib -lz).

AC_DEFUN([MYSQL_CHECK_ZLIB_WITH_COMPRESS], [
AC_MSG_CHECKING([for zlib compression library])
case $SYSTEM_TYPE in
*netware* | *modesto*)
     AC_MSG_RESULT(ok)
     AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    ;;
  *)
    AC_ARG_WITH([zlib-dir],
                AC_HELP_STRING([--with-zlib-dir=DIR],
                               [Provide MySQL with a custom location of
                               compression library. Given DIR, zlib binary is 
                               assumed to be in $DIR/lib and header files
                               in $DIR/include.]),
                [mysql_zlib_dir=${withval}],
                [mysql_zlib_dir=""])
    case "$mysql_zlib_dir" in
      "no")
        mysql_cv_compress="no"
        AC_MSG_RESULT([disabled])
        ;;
      "bundled")
        MYSQL_USE_BUNDLED_ZLIB
        AC_MSG_RESULT([using bundled zlib])
        ;;
      "")
        ZLIB_INCLUDES=""
        ZLIB_LIBS="-lz"
        MYSQL_CHECK_ZLIB_DIR
        if test "$mysql_cv_compress" = "no"; then
          MYSQL_USE_BUNDLED_ZLIB
          AC_MSG_RESULT([system-wide zlib not found, using one bundled with MySQL])
        fi
        ;;
      *)
        if test -f "$mysql_zlib_dir/lib/libz.a" -a \ 
                -f "$mysql_zlib_dir/include/zlib.h"; then
          ZLIB_INCLUDES="-I$mysql_zlib_dir/include"
          ZLIB_LIBS="-L$mysql_zlib_dir/lib -lz"
          MYSQL_CHECK_ZLIB_DIR
        fi
        if test "x$mysql_cv_compress" != "xyes"; then 
          AC_MSG_ERROR([headers or binaries were not found in $mysql_zlib_dir/{include,lib}])
        fi
        ;;
    esac
    if test "$mysql_cv_compress" = "yes"; then
      AC_SUBST([ZLIB_LIBS])
      AC_SUBST([ZLIB_INCLUDES])
      AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    fi
    ;;
esac
])

dnl ------------------------------------------------------------------------
