dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_CSVDB
dnl Sets HAVE_CSV_DB if --with-csv-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_CSVDB], [
  AC_ARG_WITH([csv-storage-engine],
              [
  --with-csv-storage-engine
                          Enable the CSV Storage Engine],
              [csvdb="$withval"],
              [csvdb=no])
  AC_MSG_CHECKING([for csv storage engine])

  case "$csvdb" in
    yes )
      AC_DEFINE([HAVE_CSV_DB], [1], [Builds the CSV Storage Engine])
      AC_MSG_RESULT([yes])
      [csvdb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [csvdb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_CSV SECTION
dnl ---------------------------------------------------------------------------
