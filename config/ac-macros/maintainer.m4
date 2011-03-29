#
# Control aspects of the development environment which are
# specific to MySQL maintainers and developers.
#
AC_DEFUN([MY_MAINTAINER_MODE], [
  AC_MSG_CHECKING([whether to enable the maintainer-specific development environment])
  AC_ARG_ENABLE([mysql-maintainer-mode],
    [AS_HELP_STRING([--enable-mysql-maintainer-mode],
                    [Enable a MySQL maintainer-specific development environment])],
    [USE_MYSQL_MAINTAINER_MODE=$enableval],
    [AS_IF([test "$with_debug" != "no"],
      [USE_MYSQL_MAINTAINER_MODE=yes], [USE_MYSQL_MAINTAINER_MODE=no])])
  AC_MSG_RESULT([$USE_MYSQL_MAINTAINER_MODE])
])

# Set warning options required under maintainer mode.
AC_DEFUN([MY_MAINTAINER_MODE_WARNINGS], [
  # Detect ICC posing as GCC.
  AC_EGREP_CPP([^__INTEL_COMPILER], [__INTEL_COMPILER],
               [INTEL_COMPILER=no], [INTEL_COMPILER=yes])
  # Setup GCC warning options.
  AS_IF([test "$GCC" = "yes" -a "$INTEL_COMPILER" = "no"], [
    C_WARNINGS="-Wall -Wextra -Wunused -Wwrite-strings -Wno-strict-aliasing -Werror"
    CXX_WARNINGS="${C_WARNINGS} -Wno-unused-parameter"
    C_WARNINGS="${C_WARNINGS} -Wdeclaration-after-statement"
  ])

  # Test whether the warning options work.
  # Test C options
  AS_IF([test -n "$C_WARNINGS"], [
    save_CFLAGS="$CFLAGS"
    AC_MSG_CHECKING([whether to use C warning options ${C_WARNINGS}])
    AC_LANG_PUSH(C)
    CFLAGS="$CFLAGS ${C_WARNINGS}"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [myac_c_warning_flags=yes],
                      [myac_c_warning_flags=no])
    AC_LANG_POP()
    AC_MSG_RESULT([$myac_c_warning_flags])
    CFLAGS="$save_CFLAGS"
  ])

  # Test C++ options
  AS_IF([test -n "$CXX_WARNINGS"], [
    save_CXXFLAGS="$CXXFLAGS"
    AC_MSG_CHECKING([whether to use C++ warning options ${CXX_WARNINGS}])
    AC_LANG_PUSH(C++)
    CXXFLAGS="$CXXFLAGS ${CXX_WARNINGS}"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], [myac_cxx_warning_flags=yes],
                      [myac_cxx_warning_flags=no])
    AC_LANG_POP()
    AC_MSG_RESULT([$myac_cxx_warning_flags])
    CXXFLAGS="$save_CXXFLAGS"
  ])

  # Set compile flag variables.
  AS_IF([test "$myac_c_warning_flags" = "yes"], [
    AM_CFLAGS="${AM_CFLAGS} ${C_WARNINGS}"
    AC_SUBST([AM_CFLAGS])])
  AS_IF([test "$myac_cxx_warning_flags" = "yes"], [
    AM_CXXFLAGS="${AM_CXXFLAGS} ${CXX_WARNINGS}"
    AC_SUBST([AM_CXXFLAGS])])
])


# Set compiler flags required under maintainer mode.
AC_DEFUN([MY_MAINTAINER_MODE_SETUP], [
  AS_IF([test "$USE_MYSQL_MAINTAINER_MODE" = "yes"],
        [MY_MAINTAINER_MODE_WARNINGS])
])
