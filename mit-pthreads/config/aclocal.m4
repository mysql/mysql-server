dnl Autoconf extensions for pthreads package.
dnl
ifelse(regexp(AC_DEFINE(xxxxx),.*@@@.*),-1,,[define(IS_AUTOHEADER)])])dnl
dnl
dnl Now, the real stuff needed by the pthreads package.
dnl
AC_DEFUN([PTHREADS_CHECK_ONE_SYSCALL],
[AC_MSG_CHECKING(for syscall $1)
AC_CACHE_VAL(pthreads_cv_syscall_$1,
AC_TRY_LINK([
/* FIXME: This list should be generated from info in configure.in.  */
#ifdef HAVE_SYSCALL_H
#include <syscall.h>
#else
#ifdef HAVE_SYS_SYSCALL_H
#include <sys/syscall.h>
#else
where is your syscall header file??
#endif
#endif
],[
int x;
x = SYS_$1 ;
],
eval pthreads_cv_syscall_$1=yes,
eval pthreads_cv_syscall_$1=no))
if eval test \$pthreads_cv_syscall_$1 = yes ; then
	pthreads_syscall_present=yes
	available_syscalls="$available_syscalls $1"
dnl Can't just do the obvious substitution here or autoheader gets
dnl sorta confused.  (Sigh.)  Getting the requoting of the brackets right
dnl would be a pain too.
	macroname=HAVE_SYSCALL_`echo $1 | tr '[[[a-z]]]' '[[[A-Z]]]'`
	AC_DEFINE_UNQUOTED($macroname)
else
	pthreads_syscall_present=no
	missing_syscalls="$missing_syscalls $1"
fi
AC_MSG_RESULT($pthreads_syscall_present)
])dnl
dnl
AC_DEFUN(PTHREADS_CHECK_SYSCALLS,dnl
ifdef([IS_AUTOHEADER],[#
dnl Need to fake out autoheader, since there's no way to add a new class
dnl of features to generate config.h.in entries for.
@@@syscalls="$1"@@@
@@@funcs="$funcs syscall_`echo $syscalls | sed 's/ / syscall_/g'`"@@@
],
[pthreads_syscall_list="$1"
for pthreads_syscallname in $pthreads_syscall_list ; do
  PTHREADS_CHECK_ONE_SYSCALL([$]pthreads_syscallname)
done
]))dnl
dnl
dnl Requote each argument.
define([requote], [ifelse($#, 0, , $#, 1, "$1",
	"$1" [requote(builtin(shift,$@))])])dnl
dnl
dnl Determine proper typedef value for a typedef name, and define a
dnl C macro to expand to that type.  (A shell variable with that value
dnl is also created.)  If none of the specified types to try match, the
dnl macro is left undefined, and the shell variable empty.  If the
dnl typedef name cannot be found in the specified header files, this
dnl test errors out; perhaps it should be changed to simply leave the
dnl macro undefined...
dnl
dnl PTHREADS_FIND_TYPE(typedefname,varname,includes,possible values...)
dnl
AC_DEFUN(PTHREADS_FIND_TYPE,
ifdef([IS_AUTOHEADER],[#
@@@syms="$syms $2"@@@
],[dnl
AC_MSG_CHECKING(type of $1)
AC_CACHE_VAL(pthreads_cv_type_$1,
[AC_TRY_COMPILE([$3],[ extern $1 foo; ],
[  for try_type in [requote(builtin(shift,builtin(shift,builtin(shift,$@))))] ; do
  AC_TRY_COMPILE([$3],[ extern $1 foo; extern $try_type foo; ],
    [ pthreads_cv_type_$1="$try_type" ; break ])
  done],
  AC_MSG_ERROR(Can't find system typedef for $1.))])
if test -n "$pthreads_cv_type_$1" ; then
  AC_DEFINE_UNQUOTED($2,$pthreads_cv_type_$1)
fi
$2=$pthreads_cv_type_$1
AC_MSG_RESULT($pthreads_cv_type_$1)]))dnl
dnl
dnl
dnl Like above, but the list of types to try is pre-specified.
dnl
AC_DEFUN(PTHREADS_FIND_INTEGRAL_TYPE,[
PTHREADS_FIND_TYPE([$1], [$2], [$3],
	int, unsigned int, long, unsigned long,
	short, unsigned short, char, unsigned char,
	long long, unsigned long long)])dnl
