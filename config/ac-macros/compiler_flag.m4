# option, cache_name, variable,
# code to execute if yes, code to exectute if fail
AC_DEFUN([AC_SYS_COMPILER_FLAG],
[
  AC_MSG_CHECKING($1)
  OLD_CFLAGS="[$]CFLAGS"
  AC_CACHE_VAL(mysql_cv_option_$2,
  [
    CFLAGS="[$]OLD_CFLAGS $1"
    AC_TRY_RUN([int main(){exit(0);}],mysql_cv_option_$2=yes,mysql_cv_option_$2=no,mysql_cv_option_$2=no)
 ])

  CFLAGS="[$]OLD_CFLAGS"
  
  if test x"[$]mysql_cv_option_$2" = "xyes" ; then
    $3="[$]$3 $1"
    AC_MSG_RESULT(yes)
    $5
  else
    AC_MSG_RESULT(no)
    $4
  fi
])

# arch, option, cache_name, variable
AC_DEFUN([AC_SYS_CPU_COMPILER_FLAG],
[
 if test "`uname -m 2>/dev/null`" = "$1" ; then
    AC_SYS_COMPILER_FLAG($2,$3,$4)
 fi
])

# os, option, cache_name, variable
AC_DEFUN([AC_SYS_OS_COMPILER_FLAG],
[
 if test "x$mysql_cv_sys_os" = "x$1" ; then
    AC_SYS_COMPILER_FLAG($2,$3,$4)
 fi
])

