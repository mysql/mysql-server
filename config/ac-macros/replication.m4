dnl This file contains configuration parameters for replication.

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_REPLICATION
dnl Sets HAVE_ROW_BASED_REPLICATION if --with-row-based-replication is used
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_REPLICATION], [
  AC_ARG_WITH([row-based-replication],
              AC_HELP_STRING([--with-row-based-replication],
                             [Include row-based replication]),
              [row_based="$withval"],
              [row_based=yes])
  AC_MSG_CHECKING([for row-based replication])

  case "$row_based" in
  yes )
    AC_DEFINE([HAVE_ROW_BASED_REPLICATION], [1], [Define to have row-based replication])
    AC_MSG_RESULT([-- including row-based replication])
    [have_row_based=yes]
    ;;
  * )
    AC_MSG_RESULT([-- not including row-based replication])
    [have_row_based=no]
    ;;
  esac
])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_REPLICATION
dnl ---------------------------------------------------------------------------
