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
# Since we have heard that alloca fails on IRIX never define it on a SGI machine
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
dnl Makes sure db version is >= 3.2.3
dnl Looks in $srcdir for Berkeley distribution not told otherwise
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_BDB], [
  AC_ARG_WITH([berkeley-db],
              [\
  --with-berkeley-db[=DIR]
                          Use BerkeleyDB located in DIR],
              [bdb="$withval"],
              [bdb=default])

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
dnl Is added to @sql_server_dirs@ in configure.in
      MYSQL_TOP_BUILDDIR([have_berkeley_db])
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

  for test_dir in db-*.*.* ../db-*.*.* /usr/local/BerkeleyDB*; do
    MYSQL_CHECK_BDB_DIR([$test_dir])
    if test X"$bdb_dir_ok" = Xsource || test X"$bdb_dir_ok" = Xinstalled; then
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

  if test $db_major -gt 3
  then
    bdb_version_ok=yes
  elif test $db_major -eq 3 && test $db_minor -gt 2
  then
    bdb_version_ok=yes
  elif test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -gt 3
  then
    bdb_version_ok=yes
  # This is ugly, but about as good as it can get
  elif test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 3 &&\
       grep 'DB_VERSION_STRING\>.*g: (' [$1] > /dev/null
  then
    bdb_version_ok=yes
  else
    bdb_version_ok="invalid version $db_major.$db_minor.$db_patch"
    bdb_version_ok="$bdb_version_ok (must be at least version 3.2.3g)"
  fi
])

AC_DEFUN([MYSQL_TOP_BUILDDIR], [
  case $[$1] in
    /* )        ;;      # already an absolute path
    *  )        [$1]="'\$(top_builddir)/'$[$1]" ;;
  esac
  if X"$[$1]" != "/"
  then
    [$1]=`echo $[$1] | sed -e 's,/$,,'`
  fi
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_BDB SECTION
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
     [  --disable-large-files   Omit support for large files])
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
