dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_ARCHIVEDB
dnl Sets HAVE_ARCHIVE_DB if --with-archive-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_ARCHIVEDB], [
  AC_ARG_WITH([archive-storage-engine],
              [
  --with-archive-storage-engine
                          Enable the Archive Storage Engine],
              [archivedb="$withval"],
              [archivedb=no])
  AC_MSG_CHECKING([for archive storage engine])

  case "$archivedb" in
    yes )
      AC_DEFINE([HAVE_ARCHIVE_DB], [1], [Builds Archive Storage Engine])
      AC_MSG_RESULT([yes])
      [archivedb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [archivedb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_ARCHIVE SECTION
dnl ---------------------------------------------------------------------------
