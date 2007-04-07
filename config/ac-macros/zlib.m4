dnl Define zlib paths to point at bundled zlib

AC_DEFUN([MYSQL_USE_BUNDLED_ZLIB], [
ZLIB_INCLUDES="-I\$(top_srcdir)/zlib"
ZLIB_LIBS="\$(top_builddir)/zlib/libz.la"
dnl Omit -L$pkglibdir as it's always in the list of mysql_config deps.
ZLIB_DEPS="-lz"
zlib_dir="zlib"
AC_SUBST([zlib_dir])
mysql_cv_compress="yes"
])

dnl Auxiliary macro to check for zlib at given path.
dnl We are strict with the server, as "archive" engine
dnl needs zlibCompileFlags(), but for client only we
dnl are less strict, and take the zlib we find.

AC_DEFUN([MYSQL_CHECK_ZLIB_DIR], [
save_CPPFLAGS="$CPPFLAGS"
save_LIBS="$LIBS"
CPPFLAGS="$ZLIB_INCLUDES $CPPFLAGS"
LIBS="$LIBS $ZLIB_LIBS"
if test X"$with_server" = Xno
then
  zlibsym=zlibVersion
else
  zlibsym=zlibCompileFlags
fi
AC_CACHE_VAL([mysql_cv_compress],
  [AC_TRY_LINK([#include <zlib.h>],
    [return $zlibsym();],
    [mysql_cv_compress="yes"
    AC_MSG_RESULT([ok])],
    [mysql_cv_compress="no"])
  ])
CPPFLAGS="$save_CPPFLAGS"
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
dnl and configure variables ZLIB_INCLUDES (i.e. -I/path/to/zlib/include),
dnl ZLIB_LIBS (i. e. -L/path/to/zlib/lib -lz) and ZLIB_DEPS which is
dnl used in mysql_config and is always the same as ZLIB_LIBS except to
dnl when we use the bundled zlib. In the latter case ZLIB_LIBS points to the
dnl build dir ($top_builddir/zlib), while mysql_config must point to the
dnl installation dir ($pkglibdir), so ZLIB_DEPS is set to point to
dnl $pkglibdir.

AC_DEFUN([MYSQL_CHECK_ZLIB_WITH_COMPRESS], [
AC_MSG_CHECKING([for zlib compression library])
case $SYSTEM_TYPE in
*netware* | *modesto*)
     AC_MSG_RESULT(ok)
     AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    ;;
  *)
    AC_ARG_WITH([zlib-dir],
                AC_HELP_STRING([--with-zlib-dir=no|bundled|DIR],
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
        # Test for libz using all known library file endings
        if test \( -f "$mysql_zlib_dir/lib/libz.a"  -o \
                   -f "$mysql_zlib_dir/lib/libz.so" -o \
                   -f "$mysql_zlib_dir/lib/libz.sl" -o \
                   -f "$mysql_zlib_dir/lib/libz.dylib" \) \
                -a -f "$mysql_zlib_dir/include/zlib.h"; then
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
      if test "x$ZLIB_DEPS" = "x"; then
        ZLIB_DEPS="$ZLIB_LIBS"
      fi
      AC_SUBST([ZLIB_LIBS])
      AC_SUBST([ZLIB_DEPS])
      AC_SUBST([ZLIB_INCLUDES])
      AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    fi
    ;;
esac
if test -n "$zlib_dir"
then
  AC_CONFIG_FILES(zlib/Makefile)
fi
])

dnl ------------------------------------------------------------------------
