AC_DEFUN([MYSQL_CHECK_CPU],
[AC_CACHE_CHECK([if compiler supports optimizations for current cpu],
mysql_cv_cpu,[

ac_save_CFLAGS="$CFLAGS"
if test -r /proc/cpuinfo ; then
  cpuinfo="cat /proc/cpuinfo"
  cpu_family=`$cpuinfo | grep 'cpu family' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`
  cpu_vendor=`$cpuinfo | grep 'vendor_id' | cut -d ':' -f 2 | cut -d ' ' -f 2 | head -1`
fi 
if test "$cpu_vendor" = "AuthenticAMD"; then
    if test $cpu_family -ge 6; then
      cpu_set="athlon pentiumpro k5 pentium i486 i386";
    elif test $cpu_family -eq 5; then
      cpu_set="k5 pentium i486 i386";
    elif test $cpu_family -eq 4; then
      cpu_set="i486 i386"
    else
      cpu_set="i386"
    fi
elif test "$cpu_vendor" = "GenuineIntel"; then
    if test $cpu_family -ge 6; then
      cpu_set="pentiumpro pentium i486 i386";
    elif test $cpu_family -eq 5; then
      cpu_set="pentium i486 i386";
    elif test $cpu_family -eq 4; then
      cpu_set="i486 i386"
    else
      cpu_set="i386"
    fi
fi

for ac_arg in $cpu_set;
do
  CFLAGS="$ac_save_CFLAGS -mcpu=$ac_arg -march=$ac_arg -DCPU=$ac_arg" 
  AC_TRY_COMPILE([],[int i],mysql_cv_cpu=$ac_arg; break;, mysql_cv_cpu="unknown")
done

if test "$mysql_cv_cpu" = "unknown"
then
  CFLAGS="$ac_save_CFLAGS"
  AC_MSG_RESULT(none)
else
  AC_MSG_RESULT($mysql_cv_cpu)
fi
]]))

