# Local macros for automake & autoconf

# A local version of AC_CHECK_SIZEOF that includes sys/types.h
dnl MYSQL_CHECK_SIZEOF(TYPE [, CROSS-SIZE])
AC_DEFUN(MYSQL_CHECK_SIZEOF,
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", sizeof($1));
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$2], , , AC_CV_NAME=$2))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

#---START: Used in for client configure
AC_DEFUN(MYSQL_TYPE_ACCEPT,
[ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([base type of last arg to accept], mysql_cv_btype_last_arg_accept,
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
if test "$ac_cv_prog_gxx" = "yes"
then
  CXXFLAGS="$CXXFLAGS -Werror"
fi
mysql_cv_btype_last_arg_accept=none
[AC_TRY_COMPILE([#if defined(inline)
#undef inline
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
],
[int a = accept(1, (struct sockaddr *) 0, (socklen_t *) 0); return (a != 0);],
mysql_cv_btype_last_arg_accept=socklen_t)]
if test $mysql_cv_btype_last_arg_accept = none; then
[AC_TRY_COMPILE([#if defined(inline)
#undef inline
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
],
[int a = accept(1, (struct sockaddr *) 0, (size_t *) 0); return (a != 0);],
mysql_cv_btype_last_arg_accept=size_t)]
fi
if test $mysql_cv_btype_last_arg_accept = none; then
mysql_cv_btype_last_arg_accept=int
fi)
AC_LANG_RESTORE
AC_DEFINE_UNQUOTED(SOCKET_SIZE_TYPE, $mysql_cv_btype_last_arg_accept)
CXXFLAGS="$ac_save_CXXFLAGS"
])
#---END:

dnl Find type of qsort
AC_DEFUN(MYSQL_TYPE_QSORT,
[AC_CACHE_CHECK([return type of qsort], mysql_cv_type_qsort,
[AC_TRY_COMPILE([#include <stdlib.h>
#ifdef __cplusplus
extern "C"
#endif
void qsort(void *base, size_t nel, size_t width,
 int (*compar) (const void *, const void *));
],
[int i;], mysql_cv_type_qsort=void, mysql_cv_type_qsort=int)])
AC_DEFINE_UNQUOTED(RETQSORTTYPE, $mysql_cv_type_qsort)
if test "$mysql_cv_type_qsort" = "void"
then
 AC_DEFINE_UNQUOTED(QSORT_TYPE_IS_VOID, 1)
fi
])

AC_DEFUN(MYSQL_TIMESPEC_TS,
[AC_CACHE_CHECK([if struct timespec has a ts_sec member], mysql_cv_timespec_ts,
[AC_TRY_COMPILE([#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[struct timespec abstime;

abstime.ts_sec = time(NULL)+1;
abstime.ts_nsec = 0;
], mysql_cv_timespec_ts=yes, mysql_cv_timespec_ts=no)])
if test "$mysql_cv_timespec_ts" = "yes"
then
  AC_DEFINE(HAVE_TIMESPEC_TS_SEC)
fi
])

AC_DEFUN(MYSQL_TZNAME,
[AC_CACHE_CHECK([if we have tzname variable], mysql_cv_tzname,
[AC_TRY_COMPILE([#include <time.h>
#ifdef __cplusplus
extern "C"
#endif
],
[ tzset();
  return tzname[0] != 0;
], mysql_cv_tzname=yes, mysql_cv_tzname=no)])
if test "$mysql_cv_tzname" = "yes"
then
  AC_DEFINE(HAVE_TZNAME)
fi
])

AC_DEFUN(MYSQL_CHECK_ZLIB_WITH_COMPRESS, [
save_LIBS="$LIBS"
LIBS="-l$1 $LIBS"
AC_CACHE_CHECK([if libz with compress], mysql_cv_compress,
[AC_TRY_LINK([#include <zlib.h>
#ifdef __cplusplus
extern "C"
#endif
],
[ return compress(0, (unsigned long*) 0, "", 0);
], mysql_cv_compress=yes, mysql_cv_compress=no)])
if test "$mysql_cv_compress" = "yes"
then
  AC_DEFINE(HAVE_COMPRESS)
else
  LIBS="$save_LIBS"
fi
])

#---START: Used in for client configure
AC_DEFUN(MYSQL_CHECK_ULONG,
[AC_MSG_CHECKING(for type ulong)
AC_CACHE_VAL(ac_cv_ulong,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
main()
{
  ulong foo;
  foo++;
  exit(0);
}], ac_cv_ulong=yes, ac_cv_ulong=no, ac_cv_ulong=no)])
AC_MSG_RESULT($ac_cv_ulong)
if test "$ac_cv_ulong" = "yes"
then
  AC_DEFINE(HAVE_ULONG)
fi
])

AC_DEFUN(MYSQL_CHECK_UCHAR,
[AC_MSG_CHECKING(for type uchar)
AC_CACHE_VAL(ac_cv_uchar,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
main()
{
  uchar foo;
  foo++;
  exit(0);
}], ac_cv_uchar=yes, ac_cv_uchar=no, ac_cv_uchar=no)])
AC_MSG_RESULT($ac_cv_uchar)
if test "$ac_cv_uchar" = "yes"
then
  AC_DEFINE(HAVE_UCHAR)
fi
])

AC_DEFUN(MYSQL_CHECK_UINT,
[AC_MSG_CHECKING(for type uint)
AC_CACHE_VAL(ac_cv_uint,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
main()
{
  uint foo;
  foo++;
  exit(0);
}], ac_cv_uint=yes, ac_cv_uint=no, ac_cv_uint=no)])
AC_MSG_RESULT($ac_cv_uint)
if test "$ac_cv_uint" = "yes"
then
  AC_DEFINE(HAVE_UINT)
fi
])


AC_DEFUN(MYSQL_PTHREAD_YIELD,
[AC_CACHE_CHECK([if pthread_yield takes zero arguments], ac_cv_pthread_yield_zero_arg,
[AC_TRY_LINK([#define _GNU_SOURCE
#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[
  pthread_yield();
], ac_cv_pthread_yield_zero_arg=yes, ac_cv_pthread_yield_zero_arg=yeso)])
if test "$ac_cv_pthread_yield_zero_arg" = "yes"
then
  AC_DEFINE(HAVE_PTHREAD_YIELD_ZERO_ARG)
fi
]
[AC_CACHE_CHECK([if pthread_yield takes 1 argument], ac_cv_pthread_yield_one_arg,
[AC_TRY_LINK([#define _GNU_SOURCE
#include <pthread.h>
#ifdef __cplusplus
extern "C"
#endif
],
[
  pthread_yield(0);
], ac_cv_pthread_yield_one_arg=yes, ac_cv_pthread_yield_one_arg=no)])
if test "$ac_cv_pthread_yield_one_arg" = "yes"
then
  AC_DEFINE(HAVE_PTHREAD_YIELD_ONE_ARG)
fi
]
)



#---END:

AC_DEFUN(MYSQL_CHECK_FP_EXCEPT,
[AC_MSG_CHECKING(for type fp_except)
AC_CACHE_VAL(ac_cv_fp_except,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
#include <ieeefp.h>
main()
{
  fp_except foo;
  foo++;
  exit(0);
}], ac_cv_fp_except=yes, ac_cv_fp_except=no, ac_cv_fp_except=no)])
AC_MSG_RESULT($ac_cv_fp_except)
if test "$ac_cv_fp_except" = "yes"
then
  AC_DEFINE(HAVE_FP_EXCEPT)
fi
])

# From fileutils-3.14/aclocal.m4

# @defmac AC_PROG_CC_STDC
# @maindex PROG_CC_STDC
# @ovindex CC
# If the C compiler in not in ANSI C mode by default, try to add an option
# to output variable @code{CC} to make it so.  This macro tries various
# options that select ANSI C on some system or another.  It considers the
# compiler to be in ANSI C mode if it defines @code{__STDC__} to 1 and
# handles function prototypes correctly.
#
# Patched by monty to only check if __STDC__ is defined. With the original 
# check it's impossible to get things to work with the Sunpro compiler from
# Workshop 4.2
#
# If you use this macro, you should check after calling it whether the C
# compiler has been set to accept ANSI C; if not, the shell variable
# @code{am_cv_prog_cc_stdc} is set to @samp{no}.  If you wrote your source
# code in ANSI C, you can make an un-ANSIfied copy of it by using the
# program @code{ansi2knr}, which comes with Ghostscript.
# @end defmac

AC_DEFUN(AM_PROG_CC_STDC,
[AC_REQUIRE([AC_PROG_CC])
AC_MSG_CHECKING(for ${CC-cc} option to accept ANSI C)
AC_CACHE_VAL(am_cv_prog_cc_stdc,
[am_cv_prog_cc_stdc=no
ac_save_CC="$CC"
# Don't try gcc -ansi; that turns off useful extensions and
# breaks some systems' header files.
# AIX			-qlanglvl=ansi
# Ultrix and OSF/1	-std1
# HP-UX			-Aa -D_HPUX_SOURCE
# SVR4			-Xc -D__EXTENSIONS__
# removed "-Xc -D__EXTENSIONS__" beacause sun c++ does not like it.
for ac_arg in "" -qlanglvl=ansi -std1 "-Aa -D_HPUX_SOURCE" 
do
  CC="$ac_save_CC $ac_arg"
  AC_TRY_COMPILE(
[#if !defined(__STDC__)
choke me
#endif
/* DYNIX/ptx V4.1.3 can't compile sys/stat.h with -Xc -D__EXTENSIONS__. */
#ifdef _SEQUENT_
# include <sys/types.h>
# include <sys/stat.h>
#endif
], [
int test (int i, double x);
struct s1 {int (*f) (int a);};
struct s2 {int (*f) (double a);};],
[am_cv_prog_cc_stdc="$ac_arg"; break])
done
CC="$ac_save_CC"
])
AC_MSG_RESULT($am_cv_prog_cc_stdc)
case "x$am_cv_prog_cc_stdc" in
  x|xno) ;;
  *) CC="$CC $am_cv_prog_cc_stdc" ;;
esac
])

# serial 1

AC_DEFUN(AM_PROG_INSTALL,
[AC_REQUIRE([AC_PROG_INSTALL])
test -z "$INSTALL_SCRIPT" && INSTALL_SCRIPT='${INSTALL_PROGRAM}'
AC_SUBST(INSTALL_SCRIPT)dnl
])

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN(AM_SANITY_CHECK,
[AC_MSG_CHECKING([whether build environment is sane])
sleep 1
echo timestamp > conftestfile
# Do this in a subshell so we don't clobber the current shell's
# arguments.  FIXME: maybe try `-L' hack like GETLOADAVG test?
if (set X `ls -t $srcdir/configure conftestfile`; test "[$]2" = conftestfile)
then
   # Ok.
   :
else
   AC_MSG_ERROR([newly created file is older than distributed files!
Check your system clock])
fi
rm -f conftest*
AC_MSG_RESULT(yes)])

# Orginal from bash-2.0 aclocal.m4, Changed to use termcap last by monty.
 
AC_DEFUN(MYSQL_CHECK_LIB_TERMCAP,
[
AC_CACHE_VAL(mysql_cv_termcap_lib,
[AC_CHECK_LIB(ncurses, tgetent, mysql_cv_termcap_lib=libncurses,
    [AC_CHECK_LIB(curses, tgetent, mysql_cv_termcap_lib=libcurses,
	[AC_CHECK_LIB(termcap, tgetent, mysql_cv_termcap_lib=libtermcap,
	    mysql_cv_termcap_lib=NOT_FOUND)])])])
AC_MSG_CHECKING(for termcap functions library)
if test $mysql_cv_termcap_lib = NOT_FOUND; then
AC_MSG_ERROR([No curses/termcap library found])
elif test $mysql_cv_termcap_lib = libtermcap; then
TERMCAP_LIB=-ltermcap
elif test $mysql_cv_termcap_lib = libncurses; then
TERMCAP_LIB=-lncurses
else
TERMCAP_LIB=-lcurses
fi
AC_MSG_RESULT($TERMCAP_LIB)
])

dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN(MYSQL_SIGNAL_CHECK,
[AC_REQUIRE([AC_TYPE_SIGNAL])
AC_MSG_CHECKING(for type of signal functions)
AC_CACHE_VAL(mysql_cv_signal_vintage,
[
  AC_TRY_LINK([#include <signal.h>],[
    sigset_t ss;
    struct sigaction sa;
    sigemptyset(&ss); sigsuspend(&ss);
    sigaction(SIGINT, &sa, (struct sigaction *) 0);
    sigprocmask(SIG_BLOCK, &ss, (sigset_t *) 0);
  ], mysql_cv_signal_vintage=posix,
  [
    AC_TRY_LINK([#include <signal.h>], [
	int mask = sigmask(SIGINT);
	sigsetmask(mask); sigblock(mask); sigpause(mask);
    ], mysql_cv_signal_vintage=4.2bsd,
    [
      AC_TRY_LINK([
	#include <signal.h>
	RETSIGTYPE foo() { }], [
		int mask = sigmask(SIGINT);
		sigset(SIGINT, foo); sigrelse(SIGINT);
		sighold(SIGINT); sigpause(SIGINT);
        ], mysql_cv_signal_vintage=svr3, mysql_cv_signal_vintage=v7
    )]
  )]
)
])
AC_MSG_RESULT($mysql_cv_signal_vintage)
if test "$mysql_cv_signal_vintage" = posix; then
AC_DEFINE(HAVE_POSIX_SIGNALS)
elif test "$mysql_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE(HAVE_BSD_SIGNALS)
elif test "$mysql_cv_signal_vintage" = svr3; then
AC_DEFINE(HAVE_USG_SIGHOLD)
fi
])

AC_DEFUN(MYSQL_CHECK_GETPW_FUNCS,
[AC_MSG_CHECKING(whether programs are able to redeclare getpw functions)
AC_CACHE_VAL(mysql_cv_can_redecl_getpw,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <pwd.h>
extern struct passwd *getpwent();], [struct passwd *z; z = getpwent();],
  mysql_cv_can_redecl_getpw=yes,mysql_cv_can_redecl_getpw=no)])
AC_MSG_RESULT($mysql_cv_can_redecl_getpw)
if test $mysql_cv_can_redecl_getpw = no; then
AC_DEFINE(HAVE_GETPW_DECLS)
fi
])

AC_DEFUN(MYSQL_HAVE_TIOCGWINSZ,
[AC_MSG_CHECKING(for TIOCGWINSZ in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocgwinsz_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCGWINSZ;],
  mysql_cv_tiocgwinsz_in_ioctl=yes,mysql_cv_tiocgwinsz_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocgwinsz_in_ioctl)
if test $mysql_cv_tiocgwinsz_in_ioctl = yes; then   
AC_DEFINE(GWINSZ_IN_SYS_IOCTL)
fi
])

AC_DEFUN(MYSQL_HAVE_FIONREAD,
[AC_MSG_CHECKING(for FIONREAD in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_fionread_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = FIONREAD;],
  mysql_cv_fionread_in_ioctl=yes,mysql_cv_fionread_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_fionread_in_ioctl)
if test $mysql_cv_fionread_in_ioctl = yes; then   
AC_DEFINE(FIONREAD_IN_SYS_IOCTL)
fi
])

AC_DEFUN(MYSQL_HAVE_TIOCSTAT,
[AC_MSG_CHECKING(for TIOCSTAT in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocstat_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCSTAT;],
  mysql_cv_tiocstat_in_ioctl=yes,mysql_cv_tiocstat_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocstat_in_ioctl)
if test $mysql_cv_tiocstat_in_ioctl = yes; then   
AC_DEFINE(TIOCSTAT_IN_SYS_IOCTL)
fi
])

AC_DEFUN(MYSQL_STRUCT_DIRENT_D_INO,
[AC_REQUIRE([AC_HEADER_DIRENT])
AC_MSG_CHECKING(if struct dirent has a d_ino member)
AC_CACHE_VAL(mysql_cv_dirent_has_dino,
[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#if defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# define dirent direct
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif /* SYSNDIR */
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif /* SYSDIR */
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif /* HAVE_DIRENT_H */
],[
struct dirent d; int z; z = d.d_ino;
], mysql_cv_dirent_has_dino=yes, mysql_cv_dirent_has_dino=no)])
AC_MSG_RESULT($mysql_cv_dirent_has_dino)
if test $mysql_cv_dirent_has_dino = yes; then
AC_DEFINE(STRUCT_DIRENT_HAS_D_INO)
fi
])

AC_DEFUN(MYSQL_TYPE_SIGHANDLER,
[AC_MSG_CHECKING([whether signal handlers are of type void])
AC_CACHE_VAL(mysql_cv_void_sighandler,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <signal.h>
#ifdef signal
#undef signal
#endif
#ifdef __cplusplus
extern "C"
#endif
void (*signal ()) ();],
[int i;], mysql_cv_void_sighandler=yes, mysql_cv_void_sighandler=no)])dnl
AC_MSG_RESULT($mysql_cv_void_sighandler)
if test $mysql_cv_void_sighandler = yes; then
AC_DEFINE(VOID_SIGHANDLER)
fi
])

AC_DEFUN(MYSQL_CXX_BOOL,
[
AC_REQUIRE([AC_PROG_CXX])
AC_MSG_CHECKING(if ${CXX} supports bool types)
AC_CACHE_VAL(mysql_cv_have_bool,
[
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
AC_TRY_COMPILE(,[bool b = true;],
mysql_cv_have_bool=yes,
mysql_cv_have_bool=no)
AC_LANG_RESTORE
])
AC_MSG_RESULT($mysql_cv_have_bool)
if test "$mysql_cv_have_bool" = yes; then
AC_DEFINE(HAVE_BOOL)
fi
])dnl

AC_DEFUN(MYSQL_STACK_DIRECTION,
 AC_CACHE_CHECK(stack direction for C alloca, ac_cv_c_stack_direction,
 [AC_TRY_RUN([find_stack_direction ()
 {
   static char *addr = 0;
   auto char dummy;
   if (addr == 0)
     {
       addr = &dummy;
       return find_stack_direction ();
     }
   else
     return (&dummy > addr) ? 1 : -1;
 }
 main ()
 {
   exit (find_stack_direction() < 0);
 }], ac_cv_c_stack_direction=1, ac_cv_c_stack_direction=-1,
   ac_cv_c_stack_direction=0)])
 AC_DEFINE_UNQUOTED(STACK_DIRECTION, $ac_cv_c_stack_direction)
)dnl

AC_DEFUN(MYSQL_FUNC_ALLOCA,
[
# Since we have heard that alloca fails on IRIX never define it on a
# SGI machine
if test ! "$host_vendor" = "sgi"
then
 AC_REQUIRE_CPP()dnl Set CPP; we run AC_EGREP_CPP conditionally.
 # The Ultrix 4.2 mips builtin alloca declared by alloca.h only works
 # for constant arguments.  Useless!
 AC_CACHE_CHECK([for working alloca.h], ac_cv_header_alloca_h,
 [AC_TRY_LINK([#include <alloca.h>], [char *p = alloca(2 * sizeof(int));],
   ac_cv_header_alloca_h=yes, ac_cv_header_alloca_h=no)])
 if test $ac_cv_header_alloca_h = yes
 then
	AC_DEFINE(HAVE_ALLOCA)
 fi
 
 AC_CACHE_CHECK([for alloca], ac_cv_func_alloca_works,
 [AC_TRY_LINK([
 #ifdef __GNUC__
 # define alloca __builtin_alloca
 #else
 # if HAVE_ALLOCA_H
 #  include <alloca.h>
 # else
 #  ifdef _AIX
  #pragma alloca
 #  else
 #   ifndef alloca /* predefined by HP cc +Olibcalls */
 char *alloca ();
 #   endif
 #  endif
 # endif
 #endif
 ], [char *p = (char *) alloca(1);],
   ac_cv_func_alloca_works=yes, ac_cv_func_alloca_works=no)])
 if test $ac_cv_func_alloca_works = yes; then
   AC_DEFINE(HAVE_ALLOCA)
 fi
 
 if test $ac_cv_func_alloca_works = no; then
   # The SVR3 libPW and SVR4 libucb both contain incompatible functions
   # that cause trouble.  Some versions do not even contain alloca or
   # contain a buggy version.  If you still want to use their alloca,
   # use ar to extract alloca.o from them instead of compiling alloca.c.
   ALLOCA=alloca.o
   AC_DEFINE(C_ALLOCA)
 
 AC_CACHE_CHECK(whether alloca needs Cray hooks, ac_cv_os_cray,
 [AC_EGREP_CPP(webecray,
 [#if defined(CRAY) && ! defined(CRAY2)
 webecray
 #else
 wenotbecray
 #endif
 ], ac_cv_os_cray=yes, ac_cv_os_cray=no)])
 if test $ac_cv_os_cray = yes; then
 for ac_func in _getb67 GETB67 getb67; do
   AC_CHECK_FUNC($ac_func, [AC_DEFINE_UNQUOTED(CRAY_STACKSEG_END, $ac_func)
   break])
 done
 fi
 fi
 AC_SUBST(ALLOCA)dnl
else
 AC_MSG_RESULT("Skipped alloca tests")
fi
])

AC_DEFUN(MYSQL_CHECK_LONGLONG_TO_FLOAT,
[
AC_MSG_CHECKING(if conversion of longlong to float works)
AC_CACHE_VAL(ac_cv_conv_longlong_to_float,
[AC_TRY_RUN([#include <stdio.h>
typedef long long longlong;
main()
{
  longlong ll=1;
  float f;
  FILE *file=fopen("conftestval", "w");
  f = (float) ll;
  fprintf(file,"%g\n",f);
  close(file);
  exit (0);
}], ac_cv_conv_longlong_to_float=`cat conftestval`, ac_cv_conv_longlong_to_float=0, ifelse([$2], , , ac_cv_conv_longlong_to_float=$2))])dnl
if test "$ac_cv_conv_longlong_to_float" = "1" -o "$ac_cv_conv_longlong_to_float" = "yes"
then
  ac_cv_conv_longlong_to_float=yes
else
  ac_cv_conv_longlong_to_float=no
fi
AC_MSG_RESULT($ac_cv_conv_longlong_to_float)
])

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_BDB
dnl Sets HAVE_BERKELEY_DB if inst library is found
dnl Makes sure db version is correct.
dnl Looks in $srcdir for Berkeley distribution if not told otherwise
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_BDB], [
  AC_ARG_WITH([berkeley-db],
              [\
  --with-berkeley-db[=DIR]
                          Use BerkeleyDB located in DIR],
              [bdb="$withval"],
              [bdb=no])

  AC_ARG_WITH([berkeley-db-includes],
              [\
  --with-berkeley-db-includes=DIR
                          Find Berkeley DB headers in DIR],
              [bdb_includes="$withval"],
              [bdb_includes=default])

  AC_ARG_WITH([berkeley-db-libs],
              [\
  --with-berkeley-db-libs=DIR
                          Find Berkeley DB libraries in DIR],
              [bdb_libs="$withval"],
              [bdb_libs=default])

  AC_MSG_CHECKING([for BerkeleyDB])

dnl     SORT OUT THE SUPPLIED ARGUMENTS TO DETERMINE WHAT TO DO
dnl echo "DBG1: bdb='$bdb'; incl='$bdb_includes'; lib='$bdb_libs'"
  have_berkeley_db=no
  case "$bdb" in
    no )
      mode=no
      AC_MSG_RESULT([no])
      ;;
    yes | default )
      case "$bdb_includes---$bdb_libs" in
        default---default )
          mode=search-$bdb
          AC_MSG_RESULT([searching...])
          ;;
        default---* | *---default | yes---* | *---yes )
          AC_MSG_ERROR([if either 'includes' or 'libs' is specified, both must be specified])
          ;;
        * )
          mode=supplied-two
          AC_MSG_RESULT([supplied])
          ;;
      esac
      ;;
    * )
      mode=supplied-one
      AC_MSG_RESULT([supplied])
      ;;
  esac

dnl echo "DBG2: [$mode] bdb='$bdb'; incl='$bdb_includes'; lib='$bdb_libs'"

  case $mode in
    no )
      bdb_includes=
      bdb_libs=
      ;;
    supplied-two )
      MYSQL_CHECK_INSTALLED_BDB([$bdb_includes], [$bdb_libs])
      case $bdb_dir_ok in
        installed ) mode=yes ;;
        * ) AC_MSG_ERROR([didn't find valid BerkeleyDB: $bdb_dir_ok]) ;;
      esac
      ;;
    supplied-one )
      MYSQL_CHECK_BDB_DIR([$bdb])
      case $bdb_dir_ok in
        source ) mode=compile ;;
        installed ) mode=yes ;;
        * ) AC_MSG_ERROR([didn't find valid BerkeleyDB: $bdb_dir_ok]) ;;
      esac
      ;;
    search-* )
      MYSQL_SEARCH_FOR_BDB
      case $bdb_dir_ok in
        source ) mode=compile ;;
        installed ) mode=yes ;;
        * )
          # not found
          case $mode in
            *-yes ) AC_MSG_ERROR([no suitable BerkeleyDB found]) ;;
            * ) mode=no ;;
          esac
         bdb_includes=
         bdb_libs=
          ;;
      esac
      ;;
    *)
      AC_MSG_ERROR([impossible case condition '$mode': please report this to bugs@lists.mysql.com])
      ;;
  esac

dnl echo "DBG3: [$mode] bdb='$bdb'; incl='$bdb_includes'; lib='$bdb_libs'"
  case $mode in
    no )
      AC_MSG_RESULT([Not using Berkeley DB])
      ;;
    yes )
      have_berkeley_db="yes"
      AC_MSG_RESULT([Using Berkeley DB in '$bdb_includes'])
      ;;
    compile )
      have_berkeley_db="$bdb"
      AC_MSG_RESULT([Compiling Berekeley DB in '$have_berkeley_db'])
      ;;
    * )
      AC_MSG_ERROR([impossible case condition '$mode': please report this to bugs@lists.mysql.com])
      ;;
  esac

  AC_SUBST(bdb_includes)
  AC_SUBST(bdb_libs)
])

AC_DEFUN([MYSQL_CHECK_INSTALLED_BDB], [
dnl echo ["MYSQL_CHECK_INSTALLED_BDB ($1) ($2)"]
  inc="$1"
  lib="$2"
  if test -f "$inc/db.h"
  then
    MYSQL_CHECK_BDB_VERSION([$inc/db.h],
      [.*#define[ 	]*], [[ 	][ 	]*])

    if test X"$bdb_version_ok" = Xyes; then
      save_LDFLAGS="$LDFLAGS"
      LDFLAGS="-L$lib $LDFLAGS"
      AC_CHECK_LIB(db,db_env_create, [
        bdb_dir_ok=installed
        MYSQL_TOP_BUILDDIR([inc])
        MYSQL_TOP_BUILDDIR([lib])
        bdb_includes="-I$inc"
        bdb_libs="-L$lib -ldb"
      ])
      LDFLAGS="$save_LDFLAGS"
    else
      bdb_dir_ok="$bdb_version_ok"
    fi
  else
    bdb_dir_ok="no db.h file in '$inc'"
  fi
])

AC_DEFUN([MYSQL_CHECK_BDB_DIR], [
dnl ([$bdb])
dnl echo ["MYSQL_CHECK_BDB_DIR ($1)"]
  dir="$1"

  MYSQL_CHECK_INSTALLED_BDB([$dir/include], [$dir/lib])

  if test X"$bdb_dir_ok" != Xinstalled; then
    # test to see if it's a source dir
    rel="$dir/dist/RELEASE"
    if test -f "$rel"; then
      MYSQL_CHECK_BDB_VERSION([$rel], [], [=])
      if test X"$bdb_version_ok" = Xyes; then
        bdb_dir_ok=source
        bdb="$dir"
        MYSQL_TOP_BUILDDIR([dir])
        bdb_includes="-I$dir/build_unix"
        bdb_libs="-L$dir/build_unix -ldb"
      else
        bdb_dir_ok="$bdb_version_ok"
      fi
    else
      bdb_dir_ok="'$dir' doesn't look like a BDB directory ($bdb_dir_ok)"
    fi
  fi
])

AC_DEFUN([MYSQL_SEARCH_FOR_BDB], [
dnl echo ["MYSQL_SEARCH_FOR_BDB"]
  bdb_dir_ok="no BerkeleyDB found"

  for test_dir in $srcdir/bdb $srcdir/db-*.*.* /usr/local/BerkeleyDB*; do
dnl    echo "-----------> Looking at ($test_dir; `cd $test_dir && pwd`)"
    MYSQL_CHECK_BDB_DIR([$test_dir])
    if test X"$bdb_dir_ok" = Xsource || test X"$bdb_dir_ok" = Xinstalled; then
dnl	echo "-----------> Found it ($bdb), ($srcdir)"
dnl     This is needed so that 'make distcheck' works properly (VPATH build).
dnl     VPATH build won't work if bdb is not under the source tree; but in
dnl     that case, hopefully people will just make and install inside the
dnl     tree, or install BDB first, and then use the installed version.
	case "$bdb" in
	"$srcdir/"* ) bdb=`echo "$bdb" | sed -e "s,^$srcdir/,,"` ;;
	esac
        break
    fi
  done
])

dnl MYSQL_CHECK_BDB_VERSION takes 3 arguments:
dnl     1)  the file to look in
dnl     2)  the search pattern before DB_VERSION_XXX
dnl     3)  the search pattern between DB_VERSION_XXX and the number
dnl It assumes that the number is the last thing on the line
AC_DEFUN([MYSQL_CHECK_BDB_VERSION], [
  db_major=`sed -e '/^[$2]DB_VERSION_MAJOR[$3]/ !d' -e 's///' [$1]`
  db_minor=`sed -e '/^[$2]DB_VERSION_MINOR[$3]/ !d' -e 's///' [$1]`
  db_patch=`sed -e '/^[$2]DB_VERSION_PATCH[$3]/ !d' -e 's///' [$1]`
  test -z "$db_major" && db_major=0
  test -z "$db_minor" && db_minor=0
  test -z "$db_patch" && db_patch=0

  # This is ugly, but about as good as it can get
  mysql_bdb=
  if test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 3
  then
    mysql_bdb=h
  elif test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 9
  then
    want_bdb_version="3.2.9a"	# hopefully this will stay up-to-date
    mysql_bdb=a
  fi

  if test -n "$mysql_bdb" && \
	grep "DB_VERSION_STRING.*:.*$mysql_bdb: " [$1] > /dev/null
  then
    bdb_version_ok=yes
  else
    bdb_version_ok="invalid version $db_major.$db_minor.$db_patch"
    bdb_version_ok="$bdb_version_ok (must be version 3.2.3h or $want_bdb_version)"
  fi
])

AC_DEFUN([MYSQL_TOP_BUILDDIR], [
  case "$[$1]" in
    /* ) ;;		# don't do anything with an absolute path
    "$srcdir"/* )
      # If BDB is under the source directory, we need to look under the
      # build directory for bdb/build_unix.
      # NOTE: I'm being lazy, and assuming the user did not specify
      # something like --with-berkeley-db=bdb (it would be missing "./").
      [$1]="\$(top_builddir)/"`echo "$[$1]" | sed -e "s,^$srcdir/,,"`
      ;;
    * )
      AC_MSG_ERROR([The BDB directory must be directly under the MySQL source directory, or be specified using the full path. ('$srcdir'; '$[$1]')])
      ;;
  esac
  if test X"$[$1]" != "/"
  then
    [$1]=`echo $[$1] | sed -e 's,/$,,'`
  fi
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_BDB SECTION
dnl ---------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_INNODB
dnl Sets HAVE_INNOBASE_DB if --with-innodb is used
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_INNODB], [
  AC_ARG_WITH([innodb],
              [\
  --with-innodb         Use Innodb],
              [innodb="$withval"],
              [innodb=no])

  AC_MSG_CHECKING([for Innodb])

  have_innodb=no
  innodb_includes=
  innodb_libs=
  case "$innodb" in
    yes )
      AC_MSG_RESULT([Using Innodb])
      AC_DEFINE(HAVE_INNOBASE_DB)
      have_innodb="yes"
      innodb_includes="-I../innobase/include"
dnl Some libs are listed several times, in order for gcc to sort out
dnl circular references.
      innodb_libs="\
 ../innobase/usr/libusr.a\
 ../innobase/odbc/libodbc.a\
 ../innobase/srv/libsrv.a\
 ../innobase/que/libque.a\
 ../innobase/srv/libsrv.a\
 ../innobase/dict/libdict.a\
 ../innobase/ibuf/libibuf.a\
 ../innobase/row/librow.a\
 ../innobase/pars/libpars.a\
 ../innobase/btr/libbtr.a\
 ../innobase/trx/libtrx.a\
 ../innobase/read/libread.a\
 ../innobase/usr/libusr.a\
 ../innobase/buf/libbuf.a\
 ../innobase/ibuf/libibuf.a\
 ../innobase/eval/libeval.a\
 ../innobase/log/liblog.a\
 ../innobase/fsp/libfsp.a\
 ../innobase/fut/libfut.a\
 ../innobase/fil/libfil.a\
 ../innobase/lock/liblock.a\
 ../innobase/mtr/libmtr.a\
 ../innobase/page/libpage.a\
 ../innobase/rem/librem.a\
 ../innobase/thr/libthr.a\
 ../innobase/com/libcom.a\
 ../innobase/sync/libsync.a\
 ../innobase/data/libdata.a\
 ../innobase/mach/libmach.a\
 ../innobase/ha/libha.a\
 ../innobase/dyn/libdyn.a\
 ../innobase/mem/libmem.a\
 ../innobase/sync/libsync.a\
 ../innobase/ut/libut.a\
 ../innobase/os/libos.a\
 ../innobase/ut/libut.a"

      AC_CHECK_LIB(rt, aio_read, [innodb_libs="$innodb_libs -lrt"])
      ;;
    * )
      AC_MSG_RESULT([Not using Innodb])
      ;;
  esac

  AC_SUBST(innodb_includes)
  AC_SUBST(innodb_libs)
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_INNODB SECTION
dnl ---------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_GEMINI
dnl Sets HAVE_GEMINI_DB if --with-gemini is used
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_GEMINI], [
  AC_ARG_WITH([gemini],
              [\
  --with-gemini[=DIR] Use Gemini DB located in DIR],
              [gemini="$withval"],
              [gemini=no])

  AC_MSG_CHECKING([for Gemini DB])

dnl     SORT OUT THE SUPPLIED ARGUMENTS TO DETERMINE WHAT TO DO
dnl echo "DBG_GEM1: gemini='$gemini'"
  have_gemini_db=no
  gemini_includes=
  gemini_libs=
  case "$gemini" in
    no) 
      AC_MSG_RESULT([Not using Gemini DB])
      ;;
    yes | default | *)
      have_gemini_db="yes"
      gemini_includes="-I../gemini/incl -I../gemini"
      gemini_libs="\
 ../gemini/api/libapi.a\
 ../gemini/db/libdb.a\
 ../gemini/dbut/libdbut.a"
      AC_MSG_RESULT([Using Gemini DB])
      ;;
  esac

  AC_SUBST(gemini_includes)
  AC_SUBST(gemini_libs)
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_GEMINI SECTION
dnl ---------------------------------------------------------------------------

dnl ---------------------------------------------------------------------------
dnl Got this from the GNU tar 1.13.11 distribution
dnl by Paul Eggert <eggert@twinsun.com>
dnl ---------------------------------------------------------------------------

dnl By default, many hosts won't let programs access large files;
dnl one must use special compiler options to get large-file access to work.
dnl For more details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html

dnl Written by Paul Eggert <eggert@twinsun.com>.

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_FLAGS(FLAGSNAME)
AC_DEFUN(AC_SYS_LARGEFILE_FLAGS,
  [AC_CACHE_CHECK([for $1 value to request large file support],
     ac_cv_sys_largefile_$1,
     [if ($GETCONF LFS_$1) >conftest.1 2>conftest.2 && test ! -s conftest.2
      then
        ac_cv_sys_largefile_$1=`cat conftest.1`
      else
	ac_cv_sys_largefile_$1=no
	ifelse($1, CFLAGS,
	  [case "$host_os" in
	   # HP-UX 10.20 requires -D__STDC_EXT__ with gcc 2.95.1.
changequote(, )dnl
	   hpux10.[2-9][0-9]* | hpux1[1-9]* | hpux[2-9][0-9]*)
changequote([, ])dnl
	     if test "$GCC" = yes; then
	       ac_cv_sys_largefile_CFLAGS=-D__STDC_EXT__
	     fi
	     ;;
	   # IRIX 6.2 and later require cc -n32.
changequote(, )dnl
	   irix6.[2-9]* | irix6.1[0-9]* | irix[7-9].* | irix[1-9][0-9]*)
changequote([, ])dnl
	     if test "$GCC" != yes; then
	       ac_cv_sys_largefile_CFLAGS=-n32
	     fi
	   esac
	   if test "$ac_cv_sys_largefile_CFLAGS" != no; then
	     ac_save_CC="$CC"
	     CC="$CC $ac_cv_sys_largefile_CFLAGS"
	     AC_TRY_LINK(, , , ac_cv_sys_largefile_CFLAGS=no)
	     CC="$ac_save_CC"
	   fi])
      fi
      rm -f conftest*])])

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_SPACE_APPEND(VAR, VAL)
AC_DEFUN(AC_SYS_LARGEFILE_SPACE_APPEND,
  [case $2 in
   no) ;;
   ?*)
     case "[$]$1" in
     '') $1=$2 ;;
     *) $1=[$]$1' '$2 ;;
     esac ;;
   esac])

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_MACRO_VALUE(C-MACRO, CACHE-VAR, COMMENT, CODE-TO-SET-DEFAULT)
AC_DEFUN(AC_SYS_LARGEFILE_MACRO_VALUE,
  [AC_CACHE_CHECK([for $1], $2,
     [$2=no
changequote(, )dnl
      $4
      for ac_flag in $ac_cv_sys_largefile_CFLAGS no; do
	case "$ac_flag" in
	-D$1)
	  $2=1 ;;
	-D$1=*)
	  $2=`expr " $ac_flag" : '[^=]*=\(.*\)'` ;;
	esac
      done
changequote([, ])dnl
      ])
   if test "[$]$2" != no; then
     AC_DEFINE_UNQUOTED([$1], [$]$2, [$3])
   fi])

AC_DEFUN(AC_SYS_LARGEFILE,
  [AC_REQUIRE([AC_CANONICAL_HOST])
   AC_ARG_ENABLE(largefile,
     [  --disable-largefile    Omit support for large files])
   if test "$enable_largefile" != no; then
     AC_CHECK_TOOL(GETCONF, getconf)
     AC_SYS_LARGEFILE_FLAGS(CFLAGS)
     AC_SYS_LARGEFILE_FLAGS(LDFLAGS)
     AC_SYS_LARGEFILE_FLAGS(LIBS)

     for ac_flag in $ac_cv_sys_largefile_CFLAGS no; do
       case "$ac_flag" in
       no) ;;
       -D_FILE_OFFSET_BITS=*) ;;
       -D_LARGEFILE_SOURCE | -D_LARGEFILE_SOURCE=*) ;;
       -D_LARGE_FILES | -D_LARGE_FILES=*) ;;
       -D?* | -I?*)
	 AC_SYS_LARGEFILE_SPACE_APPEND(CPPFLAGS, "$ac_flag") ;;
       *)
	 AC_SYS_LARGEFILE_SPACE_APPEND(CFLAGS, "$ac_flag") ;;
       esac
     done
     AC_SYS_LARGEFILE_SPACE_APPEND(LDFLAGS, "$ac_cv_sys_largefile_LDFLAGS")
     AC_SYS_LARGEFILE_SPACE_APPEND(LIBS, "$ac_cv_sys_largefile_LIBS")
     AC_SYS_LARGEFILE_MACRO_VALUE(_FILE_OFFSET_BITS,
       ac_cv_sys_file_offset_bits,
       [Number of bits in a file offset, on hosts where this is settable.],
       [case "$host_os" in
	# HP-UX 10.20 and later
	hpux10.[2-9][0-9]* | hpux1[1-9]* | hpux[2-9][0-9]*)
	  ac_cv_sys_file_offset_bits=64 ;;
	esac])
     AC_SYS_LARGEFILE_MACRO_VALUE(_LARGEFILE_SOURCE,
       ac_cv_sys_largefile_source,
       [Define to make fseeko etc. visible, on some hosts.],
       [case "$host_os" in
	# HP-UX 10.20 and later
	hpux10.[2-9][0-9]* | hpux1[1-9]* | hpux[2-9][0-9]*)
	  ac_cv_sys_largefile_source=1 ;;
	esac])
     AC_SYS_LARGEFILE_MACRO_VALUE(_LARGE_FILES,
       ac_cv_sys_large_files,
       [Define for large files, on AIX-style hosts.],
       [case "$host_os" in
	# AIX 4.2 and later
	aix4.[2-9]* | aix4.1[0-9]* | aix[5-9].* | aix[1-9][0-9]*)
	  ac_cv_sys_large_files=1 ;;
	esac])
   fi
  ])


## libtool.m4 - Configure libtool for the target system. -*-Shell-script-*-
## Copyright (C) 1996-1999 Free Software Foundation, Inc.
## Originally by Gordon Matzigkeit <gord@gnu.ai.mit.edu>, 1996
##
## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
## General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
##
## As a special exception to the GNU General Public License, if you
## distribute this file as part of a program that contains a
## configuration script generated by Autoconf, you may include it under
## the same distribution terms that you use for the rest of that program.

# serial 40 AC_PROG_LIBTOOL
AC_DEFUN(AC_PROG_LIBTOOL,
[AC_REQUIRE([AC_LIBTOOL_SETUP])dnl

# Save cache, so that ltconfig can load it
AC_CACHE_SAVE

# Actually configure libtool.  ac_aux_dir is where install-sh is found.
CC="$CC" CFLAGS="$CFLAGS" CPPFLAGS="$CPPFLAGS" \
LD="$LD" LDFLAGS="$LDFLAGS" LIBS="$LIBS" \
LN_S="$LN_S" NM="$NM" RANLIB="$RANLIB" \
DLLTOOL="$DLLTOOL" AS="$AS" OBJDUMP="$OBJDUMP" \
${CONFIG_SHELL-/bin/sh} $ac_aux_dir/ltconfig --no-reexec \
$libtool_flags --no-verify $ac_aux_dir/ltmain.sh $lt_target \
|| AC_MSG_ERROR([libtool configure failed])

# Reload cache, that may have been modified by ltconfig
AC_CACHE_LOAD

# This can be used to rebuild libtool when needed
LIBTOOL_DEPS="$ac_aux_dir/ltconfig $ac_aux_dir/ltmain.sh"

# Always use our own libtool.
LIBTOOL='$(SHELL) $(top_builddir)/libtool'
AC_SUBST(LIBTOOL)dnl

# Redirect the config.log output again, so that the ltconfig log is not
# clobbered by the next message.
exec 5>>./config.log
])

AC_DEFUN(AC_LIBTOOL_SETUP,
[AC_PREREQ(2.13)dnl
AC_REQUIRE([AC_ENABLE_SHARED])dnl
AC_REQUIRE([AC_ENABLE_STATIC])dnl
AC_REQUIRE([AC_ENABLE_FAST_INSTALL])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
AC_REQUIRE([AC_PROG_RANLIB])dnl
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_PROG_LD])dnl
AC_REQUIRE([AC_PROG_NM])dnl
AC_REQUIRE([AC_PROG_LN_S])dnl
dnl

case "$target" in
NONE) lt_target="$host" ;;
*) lt_target="$target" ;;
esac

# Check for any special flags to pass to ltconfig.
libtool_flags="--cache-file=$cache_file"
test "$enable_shared" = no && libtool_flags="$libtool_flags --disable-shared"
test "$enable_static" = no && libtool_flags="$libtool_flags --disable-static"
test "$enable_fast_install" = no && libtool_flags="$libtool_flags --disable-fast-install"
test "$ac_cv_prog_gcc" = yes && libtool_flags="$libtool_flags --with-gcc"
test "$ac_cv_prog_gnu_ld" = yes && libtool_flags="$libtool_flags --with-gnu-ld"
ifdef([AC_PROVIDE_AC_LIBTOOL_DLOPEN],
[libtool_flags="$libtool_flags --enable-dlopen"])
ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[libtool_flags="$libtool_flags --enable-win32-dll"])
AC_ARG_ENABLE(libtool-lock,
  [  --disable-libtool-lock  avoid locking (might break parallel builds)])
test "x$enable_libtool_lock" = xno && libtool_flags="$libtool_flags --disable-lock"
test x"$silent" = xyes && libtool_flags="$libtool_flags --silent"

# Some flags need to be propagated to the compiler or linker for good
# libtool support.
case "$lt_target" in
*-*-irix6*)
  # Find out which ABI we are using.
  echo '[#]line __oline__ "configure"' > conftest.$ac_ext
  if AC_TRY_EVAL(ac_compile); then
    case "`/usr/bin/file conftest.o`" in
    *32-bit*)
      LD="${LD-ld} -32"
      ;;
    *N32*)
      LD="${LD-ld} -n32"
      ;;
    *64-bit*)
      LD="${LD-ld} -64"
      ;;
    esac
  fi
  rm -rf conftest*
  ;;

*-*-sco3.2v5*)
  # On SCO OpenServer 5, we need -belf to get full-featured binaries.
  SAVE_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS -belf"
  AC_CACHE_CHECK([whether the C compiler needs -belf], lt_cv_cc_needs_belf,
    [AC_TRY_LINK([],[],[lt_cv_cc_needs_belf=yes],[lt_cv_cc_needs_belf=no])])
  if test x"$lt_cv_cc_needs_belf" != x"yes"; then
    # this is probably gcc 2.8.0, egcs 1.0 or newer; no need for -belf
    CFLAGS="$SAVE_CFLAGS"
  fi
  ;;

ifdef([AC_PROVIDE_AC_LIBTOOL_WIN32_DLL],
[*-*-cygwin* | *-*-mingw*)
  AC_CHECK_TOOL(DLLTOOL, dlltool, false)
  AC_CHECK_TOOL(AS, as, false)
  AC_CHECK_TOOL(OBJDUMP, objdump, false)
  ;;
])
esac
])

# AC_LIBTOOL_DLOPEN - enable checks for dlopen support
AC_DEFUN(AC_LIBTOOL_DLOPEN, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])])

# AC_LIBTOOL_WIN32_DLL - declare package support for building win32 dll's
AC_DEFUN(AC_LIBTOOL_WIN32_DLL, [AC_BEFORE([$0], [AC_LIBTOOL_SETUP])])

# AC_ENABLE_SHARED - implement the --enable-shared flag
# Usage: AC_ENABLE_SHARED[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_SHARED, [dnl
define([AC_ENABLE_SHARED_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(shared,
changequote(<<, >>)dnl
<<  --enable-shared[=PKGS]  build shared libraries [default=>>AC_ENABLE_SHARED_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_shared=yes ;;
no) enable_shared=no ;;
*)
  enable_shared=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_shared=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_shared=AC_ENABLE_SHARED_DEFAULT)dnl
])

# AC_DISABLE_SHARED - set the default shared flag to --disable-shared
AC_DEFUN(AC_DISABLE_SHARED, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_SHARED(no)])

# AC_ENABLE_STATIC - implement the --enable-static flag
# Usage: AC_ENABLE_STATIC[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_STATIC, [dnl
define([AC_ENABLE_STATIC_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(static,
changequote(<<, >>)dnl
<<  --enable-static[=PKGS]  build static libraries [default=>>AC_ENABLE_STATIC_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_static=yes ;;
no) enable_static=no ;;
*)
  enable_static=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_static=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_static=AC_ENABLE_STATIC_DEFAULT)dnl
])

# AC_DISABLE_STATIC - set the default static flag to --disable-static
AC_DEFUN(AC_DISABLE_STATIC, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_STATIC(no)])


# AC_ENABLE_FAST_INSTALL - implement the --enable-fast-install flag
# Usage: AC_ENABLE_FAST_INSTALL[(DEFAULT)]
#   Where DEFAULT is either `yes' or `no'.  If omitted, it defaults to
#   `yes'.
AC_DEFUN(AC_ENABLE_FAST_INSTALL, [dnl
define([AC_ENABLE_FAST_INSTALL_DEFAULT], ifelse($1, no, no, yes))dnl
AC_ARG_ENABLE(fast-install,
changequote(<<, >>)dnl
<<  --enable-fast-install[=PKGS]  optimize for fast installation [default=>>AC_ENABLE_FAST_INSTALL_DEFAULT],
changequote([, ])dnl
[p=${PACKAGE-default}
case "$enableval" in
yes) enable_fast_install=yes ;;
no) enable_fast_install=no ;;
*)
  enable_fast_install=no
  # Look at the argument we got.  We use all the common list separators.
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:,"
  for pkg in $enableval; do
    if test "X$pkg" = "X$p"; then
      enable_fast_install=yes
    fi
  done
  IFS="$ac_save_ifs"
  ;;
esac],
enable_fast_install=AC_ENABLE_FAST_INSTALL_DEFAULT)dnl
])

# AC_ENABLE_FAST_INSTALL - set the default to --disable-fast-install
AC_DEFUN(AC_DISABLE_FAST_INSTALL, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
AC_ENABLE_FAST_INSTALL(no)])

# AC_PROG_LD - find the path to the GNU or non-GNU linker
AC_DEFUN(AC_PROG_LD,
[AC_ARG_WITH(gnu-ld,
[  --with-gnu-ld           assume the C compiler uses GNU ld [default=no]],
test "$withval" = no || with_gnu_ld=yes, with_gnu_ld=no)
AC_REQUIRE([AC_PROG_CC])dnl
AC_REQUIRE([AC_CANONICAL_HOST])dnl
AC_REQUIRE([AC_CANONICAL_BUILD])dnl
ac_prog=ld
if test "$ac_cv_prog_gcc" = yes; then
  # Check if gcc -print-prog-name=ld gives a path.
  AC_MSG_CHECKING([for ld used by GCC])
  ac_prog=`($CC -print-prog-name=ld) 2>&5`
  case "$ac_prog" in
    # Accept absolute paths.
changequote(,)dnl
    [\\/]* | [A-Za-z]:[\\/]*)
      re_direlt='/[^/][^/]*/\.\./'
changequote([,])dnl
      # Canonicalize the path of ld
      ac_prog=`echo $ac_prog| sed 's%\\\\%/%g'`
      while echo $ac_prog | grep "$re_direlt" > /dev/null 2>&1; do
	ac_prog=`echo $ac_prog| sed "s%$re_direlt%/%"`
      done
      test -z "$LD" && LD="$ac_prog"
      ;;
  "")
    # If it fails, then pretend we aren't using GCC.
    ac_prog=ld
    ;;
  *)
    # If it is relative, then search for the first ld in PATH.
    with_gnu_ld=unknown
    ;;
  esac
elif test "$with_gnu_ld" = yes; then
  AC_MSG_CHECKING([for GNU ld])
else
  AC_MSG_CHECKING([for non-GNU ld])
fi
AC_CACHE_VAL(ac_cv_path_LD,
[if test -z "$LD"; then
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH; do
    test -z "$ac_dir" && ac_dir=.
    if test -f "$ac_dir/$ac_prog" || test -f "$ac_dir/$ac_prog$ac_exeext"; then
      ac_cv_path_LD="$ac_dir/$ac_prog"
      # Check to see if the program is GNU ld.  I'd rather use --version,
      # but apparently some GNU ld's only accept -v.
      # Break only if it was the GNU/non-GNU ld that we prefer.
      if "$ac_cv_path_LD" -v 2>&1 < /dev/null | egrep '(GNU|with BFD)' > /dev/null; then
	test "$with_gnu_ld" != no && break
      else
	test "$with_gnu_ld" != yes && break
      fi
    fi
  done
  IFS="$ac_save_ifs"
else
  ac_cv_path_LD="$LD" # Let the user override the test with a path.
fi])
LD="$ac_cv_path_LD"
if test -n "$LD"; then
  AC_MSG_RESULT($LD)
else
  AC_MSG_RESULT(no)
fi
test -z "$LD" && AC_MSG_ERROR([no acceptable ld found in \$PATH])
AC_PROG_LD_GNU
])

AC_DEFUN(AC_PROG_LD_GNU,
[AC_CACHE_CHECK([if the linker ($LD) is GNU ld], ac_cv_prog_gnu_ld,
[# I'd rather use --version here, but apparently some GNU ld's only accept -v.
if $LD -v 2>&1 </dev/null | egrep '(GNU|with BFD)' 1>&5; then
  ac_cv_prog_gnu_ld=yes
else
  ac_cv_prog_gnu_ld=no
fi])
])

# AC_PROG_NM - find the path to a BSD-compatible name lister
AC_DEFUN(AC_PROG_NM,
[AC_MSG_CHECKING([for BSD-compatible nm])
AC_CACHE_VAL(ac_cv_path_NM,
[if test -n "$NM"; then
  # Let the user override the test.
  ac_cv_path_NM="$NM"
else
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}${PATH_SEPARATOR-:}"
  for ac_dir in $PATH /usr/ccs/bin /usr/ucb /bin; do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/nm || test -f $ac_dir/nm$ac_exeext ; then
      # Check to see if the nm accepts a BSD-compat flag.
      # Adding the `sed 1q' prevents false positives on HP-UX, which says:
      #   nm: unknown option "B" ignored
      if ($ac_dir/nm -B /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -B"
	break
      elif ($ac_dir/nm -p /dev/null 2>&1 | sed '1q'; exit 0) | egrep /dev/null >/dev/null; then
	ac_cv_path_NM="$ac_dir/nm -p"
	break
      else
	ac_cv_path_NM=${ac_cv_path_NM="$ac_dir/nm"} # keep the first match, but
	continue # so that we can try to find one that supports BSD flags
      fi
    fi
  done
  IFS="$ac_save_ifs"
  test -z "$ac_cv_path_NM" && ac_cv_path_NM=nm
fi])
NM="$ac_cv_path_NM"
AC_MSG_RESULT([$NM])
])

# AC_CHECK_LIBM - check for math library
AC_DEFUN(AC_CHECK_LIBM,
[AC_REQUIRE([AC_CANONICAL_HOST])dnl
LIBM=
case "$lt_target" in
*-*-beos* | *-*-cygwin*)
  # These system don't have libm
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM="-lmw")
  AC_CHECK_LIB(m, main, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, main, LIBM="-lm")
  ;;
esac
])

# AC_LIBLTDL_CONVENIENCE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl convenience library, adds --enable-ltdl-convenience to
# the configure arguments.  Note that LIBLTDL is not AC_SUBSTed, nor
# is AC_CONFIG_SUBDIRS called.  If DIR is not provided, it is assumed
# to be `${top_builddir}/libltdl'.  Make sure you start DIR with
# '${top_builddir}/' (note the single quotes!) if your package is not
# flat, and, if you're not using automake, define top_builddir as
# appropriate in the Makefiles.
AC_DEFUN(AC_LIBLTDL_CONVENIENCE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  case "$enable_ltdl_convenience" in
  no) AC_MSG_ERROR([this package needs a convenience libltdl]) ;;
  "") enable_ltdl_convenience=yes
      ac_configure_args="$ac_configure_args --enable-ltdl-convenience" ;;
  esac
  LIBLTDL=ifelse($#,1,$1,['${top_builddir}/libltdl'])/libltdlc.la
  INCLTDL=ifelse($#,1,-I$1,['-I${top_builddir}/libltdl'])
])

# AC_LIBLTDL_INSTALLABLE[(dir)] - sets LIBLTDL to the link flags for
# the libltdl installable library, and adds --enable-ltdl-install to
# the configure arguments.  Note that LIBLTDL is not AC_SUBSTed, nor
# is AC_CONFIG_SUBDIRS called.  If DIR is not provided, it is assumed
# to be `${top_builddir}/libltdl'.  Make sure you start DIR with
# '${top_builddir}/' (note the single quotes!) if your package is not
# flat, and, if you're not using automake, define top_builddir as
# appropriate in the Makefiles.
# In the future, this macro may have to be called after AC_PROG_LIBTOOL.
AC_DEFUN(AC_LIBLTDL_INSTALLABLE, [AC_BEFORE([$0],[AC_LIBTOOL_SETUP])dnl
  AC_CHECK_LIB(ltdl, main,
  [test x"$enable_ltdl_install" != xyes && enable_ltdl_install=no],
  [if test x"$enable_ltdl_install" = xno; then
     AC_MSG_WARN([libltdl not installed, but installation disabled])
   else
     enable_ltdl_install=yes
   fi
  ])
  if test x"$enable_ltdl_install" = x"yes"; then
    ac_configure_args="$ac_configure_args --enable-ltdl-install"
    LIBLTDL=ifelse($#,1,$1,['${top_builddir}/libltdl'])/libltdl.la
    INCLTDL=ifelse($#,1,-I$1,['-I${top_builddir}/libltdl'])
  else
    ac_configure_args="$ac_configure_args --enable-ltdl-install=no"
    LIBLTDL="-lltdl"
    INCLTDL=
  fi
])

dnl old names
AC_DEFUN(AM_PROG_LIBTOOL, [indir([AC_PROG_LIBTOOL])])dnl
AC_DEFUN(AM_ENABLE_SHARED, [indir([AC_ENABLE_SHARED], $@)])dnl
AC_DEFUN(AM_ENABLE_STATIC, [indir([AC_ENABLE_STATIC], $@)])dnl
AC_DEFUN(AM_DISABLE_SHARED, [indir([AC_DISABLE_SHARED], $@)])dnl
AC_DEFUN(AM_DISABLE_STATIC, [indir([AC_DISABLE_STATIC], $@)])dnl
AC_DEFUN(AM_PROG_LD, [indir([AC_PROG_LD])])dnl
AC_DEFUN(AM_PROG_NM, [indir([AC_PROG_NM])])dnl

dnl This is just to silence aclocal about the macro not being used
ifelse([AC_DISABLE_FAST_INSTALL])dnl
