dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_MARIA
dnl Sets HAVE_MARIA_DB if --with-maria-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_MARIA], [
  AC_ARG_WITH([maria-storage-engine],
              [
  --with-maria-storage-engine
                          Enable the Maria Storage Engine],
              [mariadb="$withval"],
              [mariadb=no])
  AC_MSG_CHECKING([for Maria storage engine])

  case "$mariadb" in
    yes )
      AC_DEFINE([HAVE_MARIA_DB], [1], [Builds Maria Storage Engine])
      AC_MSG_RESULT([yes])
      [mariadb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [mariadb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_MARIA SECTION
dnl ---------------------------------------------------------------------------
