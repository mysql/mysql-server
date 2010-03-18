dnl
dnl Autoconf macros for detecting the Google C++ Testing Framework
dnl

AC_DEFUN([MY_PUSH_VAR],
[
  my_save_$1="$[$1]" 
  [$1]="$[$1] [$2]" 
])

AC_DEFUN([MY_POP_VAR],
[
  [$1]="$[my_save_$1]" 
])

dnl MYSQL_COMPILE_GTEST([action-if-found],[action-if-not-found])
dnl
dnl Given the absolute pathname to gtest-config, try to compile a gtest program.
dnl Export variables which contain the compilation flags required by gtest.
dnl The exported variables are: GTEST_CPPFLAGS, GTEST_CXXFLAGS, GTEST_LIBS.
dnl
AC_DEFUN([MYSQL_COMPILE_GTEST], [
  dnl Retrieve compilation flags
  GTEST_CPPFLAGS=`${POSIX_SHELL} ${GTEST_CONFIG} --cppflags`
  GTEST_CXXFLAGS=`${POSIX_SHELL} ${GTEST_CONFIG}  --cxxflags`
  GTEST_LDFLAGS=`${POSIX_SHELL} ${GTEST_CONFIG}  --ldflags`
  GTEST_LIBS=`${POSIX_SHELL} ${GTEST_CONFIG}  --libs`

  AC_SUBST(GTEST_CPPFLAGS)
  AC_SUBST(GTEST_CXXFLAGS)
  AC_SUBST(GTEST_LDFLAGS)
  AC_SUBST(GTEST_LIBS)

  MY_PUSH_VAR([CPPFLAGS], [${GTEST_CPPFLAGS}])
  MY_PUSH_VAR([CXXFLAGS], [${GTEST_CXXFLAGS}])
  MY_PUSH_VAR([LDFLAGS], [${GTEST_LDFLAGS}])
  MY_PUSH_VAR([LIBS], [${GTEST_LIBS}])

  AC_LANG_PUSH([C++])
  AC_MSG_CHECKING([for ::testing::InitGoogleTest])

dnl Would like to use AC_LINK_IFELSE here but dnl gtest-config --libs
dnl returns name of libtools library rather than real library.
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[
      #include <gtest/gtest.h>
      TEST(foo, bar) {}
    ]], [[
      int c;
      char **v= NULL;
      ::testing::InitGoogleTest(&c, v);
    ]])],
    [
      AC_MSG_RESULT([yes])
      $1
    ],
    [
      AC_MSG_RESULT([no])
      $2
    ])

  MY_POP_VAR([CPPFLAGS])
  MY_POP_VAR([CXXFLAGS])
  MY_POP_VAR([LDFLAGS])
  MY_POP_VAR([LIBS])

  AC_LANG_POP([C++])
])

dnl MYSQL_CHECK_GTEST_CONFIG([minimum version])
dnl
dnl Look for gtest-config and verify the minimum required version.
dnl
AC_DEFUN([MYSQL_CHECK_GTEST_CONFIG], [
  AC_ARG_WITH([gtest],
  [AS_HELP_STRING([--with-gtest],
                  [Enable unit tests using the Google C++ Testing Framework.])])
  AC_MSG_CHECKING([whether to use the Google C++ Testing Framework])
  if test "x${with_gtest}" != xno; then
    AC_MSG_RESULT([yes])

    dnl If a path is supplied, it must exist there.
    if test "x${with_gtest}" != x && test "x${with_gtest}" != xyes; then
      GTEST_CONFIG_PATH="${with_gtest}/bin/:${with_gtest}/scripts"
    else
      GTEST_CONFIG_PATH=$PATH
    fi

    dnl Look for the absolute name of gtest-config.
    AC_PATH_PROG([GTEST_CONFIG], [gtest-config], [], [$GTEST_CONFIG_PATH])
    dnl Consider using posix-shell.m4 to find a shell.
    POSIX_SHELL="/bin/bash"

    if test "x$GTEST_CONFIG" = x; then
      dnl Fail if gtest was requested but gtest-config couldn't be found.
      if test "x${with_gtest}" != x; then
        AC_MSG_ERROR(
          [Google Test was enabled, but gtest-config could not be found.])
      fi
    else
      AC_MSG_CHECKING([whether gtest-config is at least version $1])
      if $POSIX_SHELL $GTEST_CONFIG --min-version=$1 >/dev/null 2>&1
      then :
      else
        AC_MSG_RESULT([no])
        AC_MSG_ERROR(
          [The installed version of the Google Testing Framework is too old.])
      fi
      AC_MSG_RESULT([yes])
      MYSQL_COMPILE_GTEST([have_gtest=yes], [have_gtest=no])
    fi
  else
    AC_MSG_RESULT([no])
  fi
  AM_CONDITIONAL([HAVE_GTEST], [test "x$have_gtest" = "xyes"])
])

dnl MYSQL_CHECK_GTEST([minimum version])
dnl
AC_DEFUN([MYSQL_CHECK_GTEST], [
  MYSQL_CHECK_GTEST_CONFIG([$1])
])
