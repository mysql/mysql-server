dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_EXAMPLEDB
dnl Sets HAVE_EXAMPLE_DB if --with-example-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_EXAMPLEDB], [
  AC_ARG_WITH([example-storage-engine],
              [
  --with-example-storage-engine
                          Enable the Example Storage Engine],
              [exampledb="$withval"],
              [exampledb=no])
  AC_MSG_CHECKING([for example storage engine])

  case "$exampledb" in
    yes )
      AC_DEFINE([HAVE_EXAMPLE_DB], [1], [Builds Example DB])
      AC_MSG_RESULT([yes])
      [exampledb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [exampledb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_EXAMPLE SECTION
dnl ---------------------------------------------------------------------------

