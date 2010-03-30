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

  AC_MSG_NOTICE([checking what libevent library to use])

  AC_ARG_WITH([libevent],
              AC_HELP_STRING([--with-libevent=yes|no|bundled|DIR],
                             [Use libevent and have connection pooling.
                              A location of libevent library can be specified.
                              Given DIR, libevent library is 
                              assumed to be in $DIR/lib and header files
                              in $DIR/include.]),
              [with_libevent=${withval}],
              [with_libevent=no])

  case "$with_libevent" in
    "no")
      with_libevent=disabled
      ;;
    "bundled")
      MYSQL_USE_BUNDLED_LIBEVENT
      ;;
    "" | "yes")
      libevent_includes=""
      libevent_libs="-levent"
      AC_CHECK_LIB(event, evutil_socketpair,[with_libevent=system],
                   [with_libevent=bundled])
      AC_CHECK_HEADER(evutil.h,,[with_libevent=bundled])
      if test "$with_libevent" = "bundled"; then
        MYSQL_USE_BUNDLED_LIBEVENT
      fi
      ;;
    *)
      # Test for libevent using all known library file endings
      if test \( -f "$with_libevent/lib/libevent.a"  -o \
                 -f "$with_libevent/lib/libevent.so" -o \
                 -f "$with_libevent/lib/libevent.sl" -o \
                 -f "$with_libevent/lib/libevent.dylib" \) \
              -a -f "$with_libevent/include/evutil.h"; then
        libevent_includes="-I$with_libevent/include"
        libevent_libs="-L$with_libevent/lib -levent"
        AC_CHECK_LIB(event, evutil_socketpair,[with_libevent=$with_libevent],
                     [with_libevent=no], [$libevent_libs])
      else
        with_libevent=no
      fi
      if test "$with_libevent" = "no"; then 
        AC_MSG_ERROR([libevent headers or binaries were not found])
      fi
      ;;
  esac
  AC_MSG_CHECKING(for libevent)
  AC_MSG_RESULT([$with_libevent])

  if test "$with_libevent" != "disabled"; then
    libevent_test_option="--mysqld=--thread-handling=pool-of-threads"
    AC_SUBST(libevent_libs)
    AC_SUBST(libevent_includes)
    AC_SUBST(libevent_test_option)
    AC_DEFINE([HAVE_LIBEVENT], [1], [If we want to use libevent and have connection pooling])
  fi
  AM_CONDITIONAL([HAVE_LIBEVENT], [ test "$with_libevent" != "disabled" ])
])

