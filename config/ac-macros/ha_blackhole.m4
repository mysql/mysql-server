dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_BLACKHOLEDB
dnl Sets HAVE_BLACKHOLE_DB if --with-blackhole-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_BLACKHOLEDB], [
  AC_ARG_WITH([blackhole-storage-engine],
              [
  --with-blackhole-storage-engine
                          Enable the Blackhole Storage Engine],
              [blackholedb="$withval"],
              [blackholedb=no])
  AC_MSG_CHECKING([for blackhole storage engine])

  case "$blackholedb" in
    yes )
      AC_DEFINE([HAVE_BLACKHOLE_DB], [1], [Builds Blackhole Storage Engine])
      AC_MSG_RESULT([yes])
      [blackholedb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [blackholedb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_BLACKHOLE SECTION
dnl ---------------------------------------------------------------------------
