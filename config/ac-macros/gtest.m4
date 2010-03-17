dnl ---------------------------------------------------------------------------
dnl See if we have googletest (most likely in /usr/local/include/gtest/..)
dnl ---------------------------------------------------------------------------

ENABLE_GTEST="no"

AC_DEFUN([MYSQL_LOOK_FOR_GTEST], [
  AC_LANG_PUSH([C++])
  AC_CHECK_HEADERS([gtest/gtest.h], [ ENABLE_GTEST="yes"] )
  AC_LANG_POP([C++])
  AM_CONDITIONAL([HAVE_GTEST], [ test "x$ENABLE_GTEST" = "xyes" ])
])
