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
      innodb_includes="-I\$(top_builddir)/innobase/include"
      innodb_system_libs=""
dnl Some libs are listed several times, in order for gcc to sort out
dnl circular references.
      innodb_libs="\
 \$(top_builddir)/storage/innobase/usr/libusr.a\
 \$(top_builddir)/storage/innobase/srv/libsrv.a\
 \$(top_builddir)/storage/innobase/dict/libdict.a\
 \$(top_builddir)/storage/innobase/que/libque.a\
 \$(top_builddir)/storage/innobase/srv/libsrv.a\
 \$(top_builddir)/storage/innobase/ibuf/libibuf.a\
 \$(top_builddir)/storage/innobase/row/librow.a\
 \$(top_builddir)/storage/innobase/pars/libpars.a\
 \$(top_builddir)/storage/innobase/btr/libbtr.a\
 \$(top_builddir)/storage/innobase/trx/libtrx.a\
 \$(top_builddir)/storage/innobase/read/libread.a\
 \$(top_builddir)/storage/innobase/usr/libusr.a\
 \$(top_builddir)/storage/innobase/buf/libbuf.a\
 \$(top_builddir)/storage/innobase/ibuf/libibuf.a\
 \$(top_builddir)/storage/innobase/eval/libeval.a\
 \$(top_builddir)/storage/innobase/log/liblog.a\
 \$(top_builddir)/storage/innobase/fsp/libfsp.a\
 \$(top_builddir)/storage/innobase/fut/libfut.a\
 \$(top_builddir)/storage/innobase/fil/libfil.a\
 \$(top_builddir)/storage/innobase/lock/liblock.a\
 \$(top_builddir)/storage/innobase/mtr/libmtr.a\
 \$(top_builddir)/storage/innobase/page/libpage.a\
 \$(top_builddir)/storage/innobase/rem/librem.a\
 \$(top_builddir)/storage/innobase/thr/libthr.a\
 \$(top_builddir)/storage/innobase/sync/libsync.a\
 \$(top_builddir)/storage/innobase/data/libdata.a\
 \$(top_builddir)/storage/innobase/mach/libmach.a\
 \$(top_builddir)/storage/innobase/ha/libha.a\
 \$(top_builddir)/storage/innobase/dyn/libdyn.a\
 \$(top_builddir)/storage/innobase/mem/libmem.a\
 \$(top_builddir)/storage/innobase/sync/libsync.a\
 \$(top_builddir)/storage/innobase/ut/libut.a\
 \$(top_builddir)/storage/innobase/os/libos.a\
 \$(top_builddir)/storage/innobase/ut/libut.a"

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
