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

dnl  case "$partitiondb" in
dnl    yes )
dnl      AC_DEFINE([HAVE_PARTITION_DB], [1], [Builds Partition DB])
dnl      AC_MSG_RESULT([yes])
dnl      [partitiondb=yes]
dnl      ;;
dnl    * )
dnl      AC_MSG_RESULT([no])
dnl      [partitiondb=no]
dnl      ;;
dnl  esac
      AC_DEFINE([HAVE_PARTITION_DB], [1], [Builds Partition DB])
      AC_MSG_RESULT([yes])
      [partitiondb=yes]

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_PARTITION SECTION
dnl ---------------------------------------------------------------------------

