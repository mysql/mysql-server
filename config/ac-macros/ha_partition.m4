dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_PARTITIONDB
dnl Sets HAVE_PARTITION_DB if --with-partition is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_PARTITIONDB], [
  AC_ARG_WITH([partition],
              [
  --with-partition
                        Enable the Partition Storage Engine],
              [partitiondb="$withval"],
              [partitiondb=no])
  AC_MSG_CHECKING([for partition])

  case "$partitiondb" in
    yes )
      AC_DEFINE([HAVE_PARTITION_DB], [1], [Builds Partition DB])
      AC_MSG_RESULT([yes])
      [partitiondb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [partitiondb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_PARTITION SECTION
dnl ---------------------------------------------------------------------------

