dnl
dnl Autoconf macros for detecting the Google C++ Testing Framework
dnl

dnl MY_PUSH_VAR(variable, value)
AC_DEFUN([MY_PUSH_VAR],
[
  my_save_$1="$[$1]" 
  [$1]="$[$1] [$2]" 
])

dnl MY_POP_VAR(variable)
AC_DEFUN([MY_POP_VAR],
[
  [$1]="$[my_save_$1]" 
])


dnl MY_SET_GTEST_COMMAND(commandline-var, path-to-gtest-config)
dnl On some platforms we need to override /bin/sh in gtest-config,
dnl because it uses some posix features.
AC_DEFUN([MY_SET_GTEST_COMMAND],
[
  case "$target_os" in
    *solaris*)
       $1="/bin/bash $2" ;;
    *)
       $1="$2" ;;
  esac
])


dnl MYSQL_LINK_GTEST(gtest-config-command,
dnl                  [action-if-found],[action-if-not-found])
dnl
dnl Given a command to execute gtest-config, try to compile a gtest program.
dnl Export variables which contain the compilation flags required by gtest.
dnl The exported variables are: 
dnl   GTEST_CPPFLAGS, GTEST_CXXFLAGS, GTEST_LDFLAGS, GTEST_LIBS.
dnl
AC_DEFUN([MYSQL_LINK_GTEST], [
  dnl Retrieve compilation flags
  GTEST_CPPFLAGS=`$1 --cppflags`
  GTEST_CXXFLAGS=`$1  --cxxflags`
  GTEST_LDFLAGS=`$1  --ldflags`
  GTEST_LIBS=`$1  --libs`

  AC_SUBST(GTEST_CPPFLAGS)
  AC_SUBST(GTEST_CXXFLAGS)
  AC_SUBST(GTEST_LDFLAGS)
  AC_SUBST(GTEST_LIBS)

  MY_PUSH_VAR([CPPFLAGS], [${GTEST_CPPFLAGS}])
  MY_PUSH_VAR([CXXFLAGS], [${GTEST_CXXFLAGS} ${CXXFLAGS_OVERRIDE}])
  MY_PUSH_VAR([LDFLAGS], [${GTEST_LDFLAGS}])
  MY_PUSH_VAR([LIBS], [${GTEST_LIBS}])

  AC_LANG_PUSH([C++])
  AC_MSG_CHECKING([for ::testing::InitGoogleTest])

dnl Note that we depend on an installed version of gtest for this to work,
dnl see comments in the gtest-config script which comes with gtest.
dnl 'gtest-config --libs' will return the name of the libtool file if we
dnl are using a non-installed version. For that to work, we would have
dnl to do this linkage test with libtools.
  AC_LINK_IFELSE([
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
      $2
    ],
    [
      AC_MSG_RESULT([no])
      $3
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
dnl If we find it, then try to compile/link a simple unit test application.
AC_DEFUN([MYSQL_CHECK_GTEST_CONFIG], [
  AC_ARG_WITH([gtest],
    [AS_HELP_STRING([--with-gtest[[=/path/to/gtest]]],
                    [Enable unit tests using the Google C++ Testing Framework.])
    ],
    [],
    [with_gtest=check]
  )

  AC_MSG_CHECKING([whether to use the Google C++ Testing Framework])
  if test "x${with_gtest}" = xno; then
    AC_MSG_RESULT([no])
  else
    dnl If a path is supplied, it must exist.
    if test "x$with_gtest" != xcheck; then
      GTEST_CONFIG_PATH="${with_gtest}/bin"
    elif test "x$GTEST_PREFIX" != x; then
      GTEST_CONFIG_PATH="${GTEST_PREFIX}/bin"
    else
      GTEST_CONFIG_PATH=$PATH
    fi

    AC_MSG_RESULT([yes])

    dnl Look for the absolute name of gtest-config.
    AC_PATH_PROG([GTEST_CONFIG], [gtest-config], [], [$GTEST_CONFIG_PATH])

    if test "x$GTEST_CONFIG" = x; then
      dnl Fail if gtest was requested but gtest-config couldn't be found.
      if test "x$with_gtest" != xcheck; then
        AC_MSG_ERROR([Could not find gtest-config in $GTEST_CONFIG_PATH])
      else
        AC_MSG_WARN([Could not find gtest-config in $PATH])
      fi
    else
      MY_SET_GTEST_COMMAND(GTEST_CONFIG_CMD, $GTEST_CONFIG)
      AC_MSG_CHECKING([whether gtest-config is at least version $1])
      if $GTEST_CONFIG_CMD --min-version=$1 >/dev/null 2>&1
      then
        AC_MSG_RESULT([yes])
        MYSQL_LINK_GTEST($GTEST_CONFIG_CMD, [have_gtest=yes], [have_gtest=no])
      else
        AC_MSG_RESULT([no])
        AC_MSG_ERROR(
          [The installed version of the Google Testing Framework is too old.])
      fi
    fi
  fi
  AM_CONDITIONAL([HAVE_GTEST], [test "x$have_gtest" = "xyes"])
])

dnl MYSQL_CHECK_GTEST([minimum version])
dnl
AC_DEFUN([MYSQL_CHECK_GTEST], [
  MYSQL_CHECK_GTEST_CONFIG([$1])
])
