
AC_DEFUN([MYSQL_CHECK_MYSQLFS], [
  AC_ARG_WITH([mysqlfs],
              [
  --with-mysqlfs          Include the corba-based MySQL file system],
              [mysqlfs="$withval"],
              [mysqlfs=no])

dnl Call MYSQL_CHECK_ORBIT even if mysqlfs == no, so that @orbit_*@
dnl get substituted.
  MYSQL_CHECK_ORBIT

  AC_MSG_CHECKING(if we should build MySQLFS)
  fs_dirs=""
  if test "$mysqlfs" = "yes"
  then
    if test -n "$orbit_exec_prefix"
    then
      fs_dirs=fs
      AC_MSG_RESULT([yes])
    else
      AC_MSG_RESULT(disabled because ORBIT, the CORBA ORB, was not found)
    fi
  else
    AC_MSG_RESULT([no])
  fi
  AC_SUBST([fs_dirs])
])

AC_DEFUN([MYSQL_CHECK_ORBIT], [
AC_MSG_CHECKING(for ORBit)
orbit_config_path=`which orbit-config`
if test -n "$orbit_config_path" -a $? = 0
then
  orbit_exec_prefix=`orbit-config --exec-prefix`
  orbit_includes=`orbit-config --cflags server`
  orbit_libs=`orbit-config --libs server`
  orbit_idl="$orbit_exec_prefix/bin/orbit-idl"
  AC_MSG_RESULT(found!)
  AC_DEFINE([HAVE_ORBIT], [1], [ORBIT])
else
  orbit_exec_prefix=
  orbit_includes=
  orbit_libs=
  orbit_idl=
  AC_MSG_RESULT(not found)
fi
AC_SUBST(orbit_includes)
AC_SUBST(orbit_libs)
AC_SUBST(orbit_idl)
])
