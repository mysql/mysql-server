dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_INNODB
dnl Sets HAVE_INNOBASE_DB if --with-innodb is used
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_INNODB], [
  AC_ARG_WITH([innodb],
              [
  --without-innodb        Do not include the InnoDB table handler],
              [innodb="$withval"],
              [innodb=yes])

  AC_MSG_CHECKING([for Innodb])

  have_innodb=no
  innodb_includes=
  innodb_libs=
  case "$innodb" in
    yes )
      AC_MSG_RESULT([Using Innodb])
      AC_DEFINE([HAVE_INNOBASE_DB], [1], [Using Innobase DB])
      have_innodb="yes"
      innodb_includes="-I../innobase/include"
      innodb_system_libs=""
dnl Some libs are listed several times, in order for gcc to sort out
dnl circular references.
      innodb_libs="\
 \$(top_builddir)/innobase/usr/libusr.a\
 \$(top_builddir)/innobase/srv/libsrv.a\
 \$(top_builddir)/innobase/dict/libdict.a\
 \$(top_builddir)/innobase/que/libque.a\
 \$(top_builddir)/innobase/srv/libsrv.a\
 \$(top_builddir)/innobase/ibuf/libibuf.a\
 \$(top_builddir)/innobase/row/librow.a\
 \$(top_builddir)/innobase/pars/libpars.a\
 \$(top_builddir)/innobase/btr/libbtr.a\
 \$(top_builddir)/innobase/trx/libtrx.a\
 \$(top_builddir)/innobase/read/libread.a\
 \$(top_builddir)/innobase/usr/libusr.a\
 \$(top_builddir)/innobase/buf/libbuf.a\
 \$(top_builddir)/innobase/ibuf/libibuf.a\
 \$(top_builddir)/innobase/eval/libeval.a\
 \$(top_builddir)/innobase/log/liblog.a\
 \$(top_builddir)/innobase/fsp/libfsp.a\
 \$(top_builddir)/innobase/fut/libfut.a\
 \$(top_builddir)/innobase/fil/libfil.a\
 \$(top_builddir)/innobase/lock/liblock.a\
 \$(top_builddir)/innobase/mtr/libmtr.a\
 \$(top_builddir)/innobase/page/libpage.a\
 \$(top_builddir)/innobase/rem/librem.a\
 \$(top_builddir)/innobase/thr/libthr.a\
 \$(top_builddir)/innobase/sync/libsync.a\
 \$(top_builddir)/innobase/data/libdata.a\
 \$(top_builddir)/innobase/mach/libmach.a\
 \$(top_builddir)/innobase/ha/libha.a\
 \$(top_builddir)/innobase/dyn/libdyn.a\
 \$(top_builddir)/innobase/mem/libmem.a\
 \$(top_builddir)/innobase/sync/libsync.a\
 \$(top_builddir)/innobase/ut/libut.a\
 \$(top_builddir)/innobase/os/libos.a\
 \$(top_builddir)/innobase/ut/libut.a"

      AC_CHECK_LIB(rt, aio_read, [innodb_system_libs="-lrt"])
      ;;
    * )
      AC_MSG_RESULT([Not using Innodb])
      ;;
  esac

  AC_SUBST(innodb_includes)
  AC_SUBST(innodb_libs)
  AC_SUBST(innodb_system_libs)
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_INNODB SECTION
dnl ---------------------------------------------------------------------------
