dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_USE_BUNDLED_LIBEVENT
dnl
dnl SYNOPSIS
dnl   MYSQL_USE_BUNDLED_LIBEVENT()
dnl
dnl DESCRIPTION
dnl  Add defines so libevent is built and linked with
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_USE_BUNDLED_LIBEVENT], [

  libevent_dir="libevent"
  AC_SUBST([libevent_dir])

  libevent_libs="\$(top_builddir)/extra/libevent/libevent.a"
  libevent_includes="-I\$(top_srcdir)/extra/libevent"
  libevent_test_option="--mysqld=--thread-handling=pool-of-threads"
  AC_SUBST(libevent_libs)
  AC_SUBST(libevent_includes)
  AC_SUBST(libevent_test_option)

  AC_DEFINE([HAVE_LIBEVENT], [1], [If we want to use libevent and have connection pooling])
  AC_MSG_RESULT([using bundled libevent])

  dnl Get the upstream file with the original libevent configure macros.
  dnl Use builtin include for this, to work around path problems in old versions of aclocal.
  builtin([include],[config/ac-macros/libevent_configure.m4])
])


dnl ------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_LIBEVENT
dnl
dnl SYNOPSIS
dnl   MYSQL_CHECK_LIBEVENT
dnl
dnl ------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_LIBEVENT], [

  AC_CONFIG_FILES(extra/libevent/Makefile)

  AC_MSG_CHECKING(for libevent)
  AC_ARG_WITH([libevent],
      [  --with-libevent         use libevent and have connection pooling],
      [with_libevent=$withval],
      [with_libevent=no]
  )

  if test "$with_libevent" != "no"; then
    MYSQL_USE_BUNDLED_LIBEVENT
  else
    AC_MSG_RESULT([disabled])
  fi
  AM_CONDITIONAL([HAVE_LIBEVENT], [ test "$with_libevent" != "no" ])
])
