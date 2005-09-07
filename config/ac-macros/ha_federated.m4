dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_FEDERATED
dnl Sets HAVE_FEDERATED if --with-federated-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_FEDERATED], [
  AC_ARG_WITH([federated-storage-engine],
              [
  --with-federated-storage-engine
                        Enable the MySQL Federated Storage Engine],
              [federateddb="$withval"],
              [federateddb=no])
  AC_MSG_CHECKING([for MySQL federated storage engine])

  case "$federateddb" in
    yes )
      AC_DEFINE([HAVE_FEDERATED_DB], [1], [Define to enable Federated Handler])
      AC_MSG_RESULT([yes])
      [federateddb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [federateddb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_FEDERATED SECTION
dnl ---------------------------------------------------------------------------
