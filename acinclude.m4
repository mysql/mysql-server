# Local macros for automake & autoconf


AC_DEFUN([MYSQL_CHECK_READLINE_DECLARES_HIST_ENTRY], [
    AC_CACHE_CHECK([HIST_ENTRY is declared in readline/readline.h], mysql_cv_hist_entry_declared,
	AC_TRY_COMPILE(
	    [
		#include "stdio.h"
		#include "readline/readline.h"
	    ],
	    [ 
		HIST_ENTRY entry;
	    ],
	    [
		mysql_cv_hist_entry_declared=yes
		AC_DEFINE_UNQUOTED(HAVE_HIST_ENTRY, [1],
                                   [HIST_ENTRY is defined in the outer libeditreadline])
	    ],
	    [mysql_cv_libedit_interface=no]
        )
    )
])

AC_DEFUN([MYSQL_CHECK_LIBEDIT_INTERFACE], [
    AC_CACHE_CHECK([libedit variant of rl_completion_entry_function], mysql_cv_libedit_interface,
	AC_TRY_COMPILE(
	    [
		#include "stdio.h"
		#include "readline/readline.h"
	    ],
	    [ 
		char res= *(*rl_completion_entry_function)(0,0);
		completion_matches(0,0);
	    ],
	    [
		mysql_cv_libedit_interface=yes
                AC_DEFINE_UNQUOTED([USE_LIBEDIT_INTERFACE], [1],
                                   [used libedit interface (can we dereference result of rl_completion_entry_function)])
	    ],
	    [mysql_cv_libedit_interface=no]
        )
    )
])

AC_DEFUN([MYSQL_CHECK_NEW_RL_INTERFACE], [
    AC_CACHE_CHECK([defined rl_compentry_func_t and rl_completion_func_t], mysql_cv_new_rl_interface,
	AC_TRY_COMPILE(
	    [
		#include "stdio.h"
		#include "readline/readline.h"
	    ],
	    [ 
		rl_completion_func_t *func1= (rl_completion_func_t*)0;
		rl_compentry_func_t *func2= (rl_compentry_func_t*)0;
	    ],
	    [
		mysql_cv_new_rl_interface=yes
                AC_DEFINE_UNQUOTED([USE_NEW_READLINE_INTERFACE], [1],
                                   [used new readline interface (are rl_completion_func_t and rl_compentry_func_t defined)])
	    ],
	    [mysql_cv_new_rl_interface=no]
        )
    )
])

# A local version of AC_CHECK_SIZEOF that includes sys/types.h
dnl MYSQL_CHECK_SIZEOF(TYPE [, CROSS-SIZE])
AC_DEFUN([MYSQL_CHECK_SIZEOF],
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
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME, [ ])
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

#---START: Used in for client configure
AC_DEFUN([MYSQL_TYPE_ACCEPT],
[ac_save_CXXFLAGS="$CXXFLAGS"
AC_CACHE_CHECK([base type of last arg to accept], mysql_cv_btype_last_arg_accept,
AC_LANG_SAVE
AC_LANG_CPLUSPLUS
if test "$ac_cv_prog_gxx" = "yes"
then
  CXXFLAGS=`echo $CXXFLAGS -Werror | sed 's/-fbranch-probabilities//'`
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
if test "$mysql_cv_btype_last_arg_accept" = "none"; then
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
if test "$mysql_cv_btype_last_arg_accept" = "none"; then
mysql_cv_btype_last_arg_accept=int
fi)
AC_LANG_RESTORE
AC_DEFINE_UNQUOTED([SOCKET_SIZE_TYPE], [$mysql_cv_btype_last_arg_accept],
                   [The base type of the last arg to accept])
CXXFLAGS="$ac_save_CXXFLAGS"
])
#---END:

dnl Find type of qsort
AC_DEFUN([MYSQL_TYPE_QSORT],
[AC_CACHE_CHECK([return type of qsort], mysql_cv_type_qsort,
[AC_TRY_COMPILE([#include <stdlib.h>
#ifdef __cplusplus
extern "C"
#endif
void qsort(void *base, size_t nel, size_t width,
 int (*compar) (const void *, const void *));
],
[int i;], mysql_cv_type_qsort=void, mysql_cv_type_qsort=int)])
AC_DEFINE_UNQUOTED([RETQSORTTYPE], [$mysql_cv_type_qsort],
                   [The return type of qsort (int or void).])
if test "$mysql_cv_type_qsort" = "void"
then
 AC_DEFINE_UNQUOTED([QSORT_TYPE_IS_VOID], [1], [qsort returns void])
fi
])

AC_DEFUN([MYSQL_TIMESPEC_TS],
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
  AC_DEFINE([HAVE_TIMESPEC_TS_SEC], [1],
            [Timespec has a ts_sec instead of tv_sev])
fi
])

AC_DEFUN([MYSQL_TZNAME],
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
  AC_DEFINE([HAVE_TZNAME], [1], [Have the tzname variable])
fi
])


dnl Define zlib paths to point at bundled zlib

AC_DEFUN([MYSQL_USE_BUNDLED_ZLIB], [
ZLIB_INCLUDES="-I\$(top_srcdir)/zlib"
ZLIB_LIBS="\$(top_builddir)/zlib/libz.la"
zlib_dir="zlib"
AC_SUBST([zlib_dir])
mysql_cv_compress="yes"
])

dnl Auxiliary macro to check for zlib at given path

AC_DEFUN([MYSQL_CHECK_ZLIB_DIR], [
save_INCLUDES="$INCLUDES"
save_LIBS="$LIBS"
INCLUDES="$INCLUDES $ZLIB_INCLUDES"
LIBS="$LIBS $ZLIB_LIBS"
AC_CACHE_VAL([mysql_cv_compress],
  [AC_TRY_LINK([#include <zlib.h>],
    [int link_test() { return compress(0, (unsigned long*) 0, "", 0); }],
    [mysql_cv_compress="yes"
    AC_MSG_RESULT([ok])],
    [mysql_cv_compress="no"])
  ])
INCLUDES="$save_INCLUDES"
LIBS="$save_LIBS"
])

dnl MYSQL_CHECK_ZLIB_WITH_COMPRESS
dnl ------------------------------------------------------------------------
dnl @synopsis MYSQL_CHECK_ZLIB_WITH_COMPRESS
dnl
dnl Provides the following configure options:
dnl --with-zlib-dir=DIR
dnl Possible DIR values are:
dnl - "no" - the macro will disable use of compression functions
dnl - "bundled" - means use zlib bundled along with MySQL sources
dnl - empty, or not specified - the macro will try default system
dnl   library (if present), and in case of error will fall back to 
dnl   bundled zlib
dnl - zlib location prefix - given location prefix, the macro expects
dnl   to find the library headers in $prefix/include, and binaries in
dnl   $prefix/lib. If zlib headers or binaries weren't found at $prefix, the
dnl   macro bails out with error.
dnl 
dnl If the library was found, this function #defines HAVE_COMPRESS
dnl and configure variables ZLIB_INCLUDES (i.e. -I/path/to/zlib/include) and
dnl ZLIB_LIBS (i. e. -L/path/to/zlib/lib -lz).

AC_DEFUN([MYSQL_CHECK_ZLIB_WITH_COMPRESS], [
AC_MSG_CHECKING([for zlib compression library])
case $SYSTEM_TYPE in
*netware* | *modesto*)
     AC_MSG_RESULT(ok)
     AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    ;;
  *)
    AC_ARG_WITH([zlib-dir],
                AC_HELP_STRING([--with-zlib-dir=DIR],
                               [Provide MySQL with a custom location of
                               compression library. Given DIR, zlib binary is 
                               assumed to be in $DIR/lib and header files
                               in $DIR/include.]),
                [mysql_zlib_dir=${withval}],
                [mysql_zlib_dir=""])
    case "$mysql_zlib_dir" in
      "no")
        mysql_cv_compress="no"
        AC_MSG_RESULT([disabled])
        ;;
      "bundled")
        MYSQL_USE_BUNDLED_ZLIB
        AC_MSG_RESULT([using bundled zlib])
        ;;
      "")
        ZLIB_INCLUDES=""
        ZLIB_LIBS="-lz"
        MYSQL_CHECK_ZLIB_DIR
        if test "$mysql_cv_compress" = "no"; then
          MYSQL_USE_BUNDLED_ZLIB
          AC_MSG_RESULT([system-wide zlib not found, using one bundled with MySQL])
        fi
        ;;
      *)
        if test -f "$mysql_zlib_dir/lib/libz.a" -a \ 
                -f "$mysql_zlib_dir/include/zlib.h"; then
          ZLIB_INCLUDES="-I$mysql_zlib_dir/include"
          ZLIB_LIBS="-L$mysql_zlib_dir/lib -lz"
          MYSQL_CHECK_ZLIB_DIR
        fi
        if test "x$mysql_cv_compress" != "xyes"; then 
          AC_MSG_ERROR([headers or binaries were not found in $mysql_zlib_dir/{include,lib}])
        fi
        ;;
    esac
    if test "$mysql_cv_compress" = "yes"; then
      AC_SUBST([ZLIB_LIBS])
      AC_SUBST([ZLIB_INCLUDES])
      AC_DEFINE([HAVE_COMPRESS], [1], [Define to enable compression support])
    fi
    ;;
esac
])

dnl ------------------------------------------------------------------------

#---START: Used in for client configure
AC_DEFUN([MYSQL_CHECK_ULONG],
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
  AC_DEFINE([HAVE_ULONG], [1], [system headers define ulong])
fi
])

AC_DEFUN([MYSQL_CHECK_UCHAR],
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
  AC_DEFINE([HAVE_UCHAR], [1], [system headers define uchar])
fi
])

AC_DEFUN([MYSQL_CHECK_UINT],
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
  AC_DEFINE([HAVE_UINT], [1], [system headers define uint])
fi
])


AC_DEFUN([MYSQL_CHECK_IN_ADDR_T],
[AC_MSG_CHECKING(for type in_addr_t)
AC_CACHE_VAL(ac_cv_in_addr_t,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv)
{
  in_addr_t foo;
  exit(0);
}], ac_cv_in_addr_t=yes, ac_cv_in_addr_t=no, ac_cv_in_addr_t=no)])
AC_MSG_RESULT($ac_cv_in_addr_t)
if test "$ac_cv_in_addr_t" = "yes"
then
  AC_DEFINE([HAVE_IN_ADDR_T], [1], [system headers define in_addr_t])
fi
])


AC_DEFUN([MYSQL_PTHREAD_YIELD],
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
  AC_DEFINE([HAVE_PTHREAD_YIELD_ZERO_ARG], [1],
            [pthread_yield that doesn't take any arguments])
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
  AC_DEFINE([HAVE_PTHREAD_YIELD_ONE_ARG], [1],
            [pthread_yield function with one argument])
fi
]
)



#---END:

AC_DEFUN([MYSQL_CHECK_FP_EXCEPT],
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
  AC_DEFINE([HAVE_FP_EXCEPT], [1], [fp_except from ieeefp.h])
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

AC_DEFUN([AM_PROG_CC_STDC],
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

#
# Check to make sure that the build environment is sane.
#

AC_DEFUN([AM_SANITY_CHECK],
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
 
AC_DEFUN([MYSQL_CHECK_LIB_TERMCAP],
[
AC_CACHE_VAL(mysql_cv_termcap_lib,
[AC_CHECK_LIB(ncurses, tgetent, mysql_cv_termcap_lib=libncurses,
    [AC_CHECK_LIB(curses, tgetent, mysql_cv_termcap_lib=libcurses,
	[AC_CHECK_LIB(termcap, tgetent, mysql_cv_termcap_lib=libtermcap,
	    mysql_cv_termcap_lib=NOT_FOUND)])])])
AC_MSG_CHECKING(for termcap functions library)
if test "$mysql_cv_termcap_lib" = "NOT_FOUND"; then
AC_MSG_ERROR([No curses/termcap library found])
elif test "$mysql_cv_termcap_lib" = "libtermcap"; then
TERMCAP_LIB=-ltermcap
elif test "$mysql_cv_termcap_lib" = "libncurses"; then
TERMCAP_LIB=-lncurses
else
TERMCAP_LIB=-lcurses
fi
AC_MSG_RESULT($TERMCAP_LIB)
])

dnl Check type of signal routines (posix, 4.2bsd, 4.1bsd or v7)
AC_DEFUN([MYSQL_SIGNAL_CHECK],
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
AC_DEFINE(HAVE_POSIX_SIGNALS, [1],
          [Signal handling is POSIX (sigset/sighold, etc)])
elif test "$mysql_cv_signal_vintage" = "4.2bsd"; then
AC_DEFINE([HAVE_BSD_SIGNALS], [1], [BSD style signals])
elif test "$mysql_cv_signal_vintage" = svr3; then
AC_DEFINE(HAVE_USG_SIGHOLD, [1], [sighold() is present and usable])
fi
])

AC_DEFUN([MYSQL_CHECK_GETPW_FUNCS],
[AC_MSG_CHECKING(whether programs are able to redeclare getpw functions)
AC_CACHE_VAL(mysql_cv_can_redecl_getpw,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <pwd.h>
extern struct passwd *getpwent();], [struct passwd *z; z = getpwent();],
  mysql_cv_can_redecl_getpw=yes,mysql_cv_can_redecl_getpw=no)])
AC_MSG_RESULT($mysql_cv_can_redecl_getpw)
if test "$mysql_cv_can_redecl_getpw" = "no"; then
AC_DEFINE(HAVE_GETPW_DECLS, [1], [getpwent() declaration present])
fi
])

AC_DEFUN([MYSQL_HAVE_TIOCGWINSZ],
[AC_MSG_CHECKING(for TIOCGWINSZ in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocgwinsz_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCGWINSZ;],
  mysql_cv_tiocgwinsz_in_ioctl=yes,mysql_cv_tiocgwinsz_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocgwinsz_in_ioctl)
if test "$mysql_cv_tiocgwinsz_in_ioctl" = "yes"; then   
AC_DEFINE([GWINSZ_IN_SYS_IOCTL], [1],
          [READLINE: your system defines TIOCGWINSZ in sys/ioctl.h.])
fi
])

AC_DEFUN([MYSQL_HAVE_FIONREAD],
[AC_MSG_CHECKING(for FIONREAD in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_fionread_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = FIONREAD;],
  mysql_cv_fionread_in_ioctl=yes,mysql_cv_fionread_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_fionread_in_ioctl)
if test "$mysql_cv_fionread_in_ioctl" = "yes"; then   
AC_DEFINE([FIONREAD_IN_SYS_IOCTL], [1], [Do we have FIONREAD])
fi
])

AC_DEFUN([MYSQL_HAVE_TIOCSTAT],
[AC_MSG_CHECKING(for TIOCSTAT in sys/ioctl.h)
AC_CACHE_VAL(mysql_cv_tiocstat_in_ioctl,
[AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/ioctl.h>], [int x = TIOCSTAT;],
  mysql_cv_tiocstat_in_ioctl=yes,mysql_cv_tiocstat_in_ioctl=no)])
AC_MSG_RESULT($mysql_cv_tiocstat_in_ioctl)
if test "$mysql_cv_tiocstat_in_ioctl" = "yes"; then   
AC_DEFINE(TIOCSTAT_IN_SYS_IOCTL, [1],
          [declaration of TIOCSTAT in sys/ioctl.h])
fi
])

AC_DEFUN([MYSQL_STRUCT_DIRENT_D_INO],
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
if test "$mysql_cv_dirent_has_dino" = "yes"; then
AC_DEFINE(STRUCT_DIRENT_HAS_D_INO, [1],
          [d_ino member present in struct dirent])
fi
])

AC_DEFUN([MYSQL_STRUCT_DIRENT_D_NAMLEN],
[AC_REQUIRE([AC_HEADER_DIRENT])
AC_MSG_CHECKING(if struct dirent has a d_namlen member)
AC_CACHE_VAL(mysql_cv_dirent_has_dnamlen,
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
struct dirent d; int z; z = (int)d.d_namlen;
], mysql_cv_dirent_has_dnamlen=yes, mysql_cv_dirent_has_dnamlen=no)])
AC_MSG_RESULT($mysql_cv_dirent_has_dnamlen)
if test "$mysql_cv_dirent_has_dnamlen" = "yes"; then
AC_DEFINE(STRUCT_DIRENT_HAS_D_NAMLEN, [1],
          [d_namlen member present in struct dirent])
fi
])


AC_DEFUN([MYSQL_TYPE_SIGHANDLER],
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
if test "$mysql_cv_void_sighandler" = "yes"; then
AC_DEFINE(VOID_SIGHANDLER, [1], [sighandler type is void (*signal ()) ();])
fi
])

AC_DEFUN([MYSQL_CXX_BOOL],
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
AC_DEFINE([HAVE_BOOL], [1], [bool is not defined by all C++ compilators])
fi
])dnl

AC_DEFUN([MYSQL_STACK_DIRECTION],
 [AC_CACHE_CHECK(stack direction for C alloca, ac_cv_c_stack_direction,
 [AC_TRY_RUN([#include <stdlib.h>
 int find_stack_direction ()
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
 int main ()
 {
   exit (find_stack_direction() < 0);
 }], ac_cv_c_stack_direction=1, ac_cv_c_stack_direction=-1,
   ac_cv_c_stack_direction=0)])
 AC_DEFINE_UNQUOTED(STACK_DIRECTION, $ac_cv_c_stack_direction)
])dnl

AC_DEFUN([MYSQL_FUNC_ALLOCA],
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
 if test "$ac_cv_header_alloca_h" = "yes"
 then
	AC_DEFINE(HAVE_ALLOCA, 1)
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
 if test "$ac_cv_func_alloca_works" = "yes"; then
   AC_DEFINE([HAVE_ALLOCA], [1], [If we have a working alloca() implementation])
 fi
 
 if test "$ac_cv_func_alloca_works" = "no"; then
   # The SVR3 libPW and SVR4 libucb both contain incompatible functions
   # that cause trouble.  Some versions do not even contain alloca or
   # contain a buggy version.  If you still want to use their alloca,
   # use ar to extract alloca.o from them instead of compiling alloca.c.
   ALLOCA=alloca.o
   AC_DEFINE(C_ALLOCA, 1)
 
 AC_CACHE_CHECK(whether alloca needs Cray hooks, ac_cv_os_cray,
 [AC_EGREP_CPP(webecray,
 [#if defined(CRAY) && ! defined(CRAY2)
 webecray
 #else
 wenotbecray
 #endif
 ], ac_cv_os_cray=yes, ac_cv_os_cray=no)])
 if test "$ac_cv_os_cray" = "yes"; then
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

AC_DEFUN([MYSQL_CHECK_LONGLONG_TO_FLOAT],
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
  fclose(file);
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

AC_DEFUN([MYSQL_CHECK_VIO], [
  AC_ARG_WITH([vio],
              [  --with-vio              Include the Virtual IO support],
              [vio="$withval"],
              [vio=no])

  if test "$vio" = "yes"
  then
    vio_dir="vio"
    vio_libs="../vio/libvio.la"
    AC_DEFINE(HAVE_VIO, 1)
  else
    vio_dir=""
    vio_libs=""
  fi
  AC_SUBST([vio_dir])
  AC_SUBST([vio_libs])
])

AC_DEFUN([MYSQL_FIND_OPENSSL], [
  incs="$1"
  libs="$2"
  case "$incs---$libs" in
    ---)
      for d in /usr/ssl/include /usr/local/ssl/include /usr/include \
/usr/include/ssl /opt/ssl/include /opt/openssl/include \
/usr/local/ssl/include /usr/local/include /usr/freeware/include ; do
       if test -f $d/openssl/ssl.h  ; then
         OPENSSL_INCLUDE=-I$d
       fi
      done

      for d in /usr/ssl/lib /usr/local/ssl/lib /usr/lib/openssl \
/usr/lib /usr/lib64 /opt/ssl/lib /opt/openssl/lib \
/usr/freeware/lib32 /usr/local/lib/ ; do
      if test -f $d/libssl.a || test -f $d/libssl.so || test -f $d/libssl.dylib ; then
        OPENSSL_LIB=$d
      fi
      done
      ;;
    ---* | *---)
      AC_MSG_ERROR([if either 'includes' or 'libs' is specified, both must be specified])
      ;;
    * )
      if test -f $incs/openssl/ssl.h  ; then
        OPENSSL_INCLUDE=-I$incs
      fi
      if test -f $libs/libssl.a || test -f $libs/libssl.so || test -f $libs/libssl.dylib ; then
        OPENSSL_LIB=$libs
      fi
      ;;
  esac

  # On RedHat 9 we need kerberos to compile openssl
  for d in /usr/kerberos/include
  do
   if test -f $d/krb5.h  ; then
     OPENSSL_KERBEROS_INCLUDE="$d"
   fi
  done


 if test -z "$OPENSSL_LIB" -o -z "$OPENSSL_INCLUDE" ; then
   echo "Could not find an installation of OpenSSL"
   if test -n "$OPENSSL_LIB" ; then
    if test "$IS_LINUX" = "true"; then
      echo "Looks like you've forgotten to install OpenSSL development RPM"
    fi
   fi
  exit 1
 fi

])

AC_DEFUN([MYSQL_CHECK_OPENSSL], [
AC_MSG_CHECKING(for OpenSSL)
  AC_ARG_WITH([openssl],
              [  --with-openssl[=DIR]    Include the OpenSSL support],
              [openssl="$withval"],
              [openssl=no])

  AC_ARG_WITH([openssl-includes],
              [
  --with-openssl-includes=DIR
                          Find OpenSSL headers in DIR],
              [openssl_includes="$withval"],
              [openssl_includes=""])

  AC_ARG_WITH([openssl-libs],
              [
  --with-openssl-libs=DIR
                          Find OpenSSL libraries in DIR],
              [openssl_libs="$withval"],
              [openssl_libs=""])

  if test "$openssl" != "no"
  then
	if test "$openssl" != "yes"
	then
		if test -z "$openssl_includes" 
		then
			openssl_includes="$openssl/include"
		fi
		if test -z "$openssl_libs" 
		then
			openssl_libs="$openssl/lib"
		fi
	fi
    MYSQL_FIND_OPENSSL([$openssl_includes], [$openssl_libs])
    #force VIO use
    vio_dir="vio"
    vio_libs="../vio/libvio.la"
    AC_DEFINE([HAVE_VIO], [1], [Virtual IO])
    AC_MSG_RESULT(yes)
    openssl_libs="-L$OPENSSL_LIB -lssl -lcrypto"
    # Don't set openssl_includes to /usr/include as this gives us a lot of
    # compiler warnings when using gcc 3.x
    openssl_includes=""
    if test "$OPENSSL_INCLUDE" != "-I/usr/include"
    then
	openssl_includes="$OPENSSL_INCLUDE"
    fi
    if test "$OPENSSL_KERBEROS_INCLUDE"
    then
    	openssl_includes="$openssl_includes -I$OPENSSL_KERBEROS_INCLUDE"
    fi
    AC_DEFINE([HAVE_OPENSSL], [1], [OpenSSL])

    # openssl-devel-0.9.6 requires dlopen() and we can't link staticly
    # on many platforms (We should actually test this here, but it's quite
    # hard) to do as we are doing libtool for linking.
    using_static=""
    case "$CLIENT_EXTRA_LDFLAGS $MYSQLD_EXTRA_LDFLAGS" in
	*-all-static*) using_static="yes" ;;
    esac
    if test "$using_static" = "yes"
    then
      echo "You can't use the --all-static link option when using openssl."
      exit 1
    fi
    NON_THREADED_CLIENT_LIBS="$NON_THREADED_CLIENT_LIBS $openssl_libs"
  else
    AC_MSG_RESULT(no)
	if test ! -z "$openssl_includes"
	then
		AC_MSG_ERROR(Can't have --with-openssl-includes without --with-openssl);
	fi
	if test ! -z "$openssl_libs"
	then
		AC_MSG_ERROR(Can't have --with-openssl-libs without --with-openssl);
	fi
  fi
  AC_SUBST(openssl_libs)
  AC_SUBST(openssl_includes)
])


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

AC_DEFUN([MYSQL_CHECK_ISAM], [
  AC_ARG_WITH([isam], [
  --with-isam             Enable the ISAM table type],
    [with_isam="$withval"],
    [with_isam=no])

  isam_libs=
  if test X"$with_isam" = X"yes"
  then
    AC_DEFINE([HAVE_ISAM], [1], [Using old ISAM tables])
    isam_libs="\$(top_builddir)/isam/libnisam.a\
 \$(top_builddir)/merge/libmerge.a"
  fi
  AC_SUBST(isam_libs)
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_BDB
dnl Sets HAVE_BERKELEY_DB if inst library is found
dnl Makes sure db version is correct.
dnl Looks in $srcdir for Berkeley distribution if not told otherwise
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_BDB], [
  AC_ARG_WITH([berkeley-db],
              [
  --with-berkeley-db[=DIR]
                          Use BerkeleyDB located in DIR],
              [bdb="$withval"],
              [bdb=no])

  AC_ARG_WITH([berkeley-db-includes],
              [
  --with-berkeley-db-includes=DIR
                          Find Berkeley DB headers in DIR],
              [bdb_includes="$withval"],
              [bdb_includes=default])

  AC_ARG_WITH([berkeley-db-libs],
              [
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
      bdb_libs_with_path=
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
	 bdb_libs_with_path=
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
  AC_SUBST(bdb_libs_with_path)
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
        bdb_libs_with_path="$lib/libdb.a"
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
	bdb_libs_with_path="$dir/build_unix/libdb.a"
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
#  mysql_bdb=
#  if test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 3
#  then
#    mysql_bdb=h
#  elif test $db_major -eq 3 && test $db_minor -eq 2 && test $db_patch -eq 9
#  then
#    want_bdb_version="3.2.9a"	# hopefully this will stay up-to-date
#    mysql_bdb=a
#  fi

dnl RAM:
want_bdb_version="4.1.24"
bdb_version_ok=yes

#  if test -n "$mysql_bdb" && \
#	grep "DB_VERSION_STRING.*:.*$mysql_bdb: " [$1] > /dev/null
#  then
#    bdb_version_ok=yes
#  else
#    bdb_version_ok="invalid version $db_major.$db_minor.$db_patch"
#    bdb_version_ok="$bdb_version_ok (must be version 3.2.3h or $want_bdb_version)"
#  fi
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
              [
  --without-innodb        Do not include the InnoDB table handler],
              [innodb="$withval"],
              [innodb=yes])

  AC_MSG_CHECKING([for Innodb])

  have_innodb=no
  innodb_includes=
  innodb_libs=
  case "$innodb" in
    yes )
      AC_MSG_RESULT([Using Innodb])
      AC_DEFINE([HAVE_INNOBASE_DB], [1], [Using Innobase DB])
      have_innodb="yes"
      innodb_includes="-I../innobase/include"
      innodb_system_libs=""
dnl Some libs are listed several times, in order for gcc to sort out
dnl circular references.
      innodb_libs="\
 \$(top_builddir)/innobase/usr/libusr.a\
 \$(top_builddir)/innobase/srv/libsrv.a\
 \$(top_builddir)/innobase/dict/libdict.a\
 \$(top_builddir)/innobase/que/libque.a\
 \$(top_builddir)/innobase/srv/libsrv.a\
 \$(top_builddir)/innobase/ibuf/libibuf.a\
 \$(top_builddir)/innobase/row/librow.a\
 \$(top_builddir)/innobase/pars/libpars.a\
 \$(top_builddir)/innobase/btr/libbtr.a\
 \$(top_builddir)/innobase/trx/libtrx.a\
 \$(top_builddir)/innobase/read/libread.a\
 \$(top_builddir)/innobase/usr/libusr.a\
 \$(top_builddir)/innobase/buf/libbuf.a\
 \$(top_builddir)/innobase/ibuf/libibuf.a\
 \$(top_builddir)/innobase/eval/libeval.a\
 \$(top_builddir)/innobase/log/liblog.a\
 \$(top_builddir)/innobase/fsp/libfsp.a\
 \$(top_builddir)/innobase/fut/libfut.a\
 \$(top_builddir)/innobase/fil/libfil.a\
 \$(top_builddir)/innobase/lock/liblock.a\
 \$(top_builddir)/innobase/mtr/libmtr.a\
 \$(top_builddir)/innobase/page/libpage.a\
 \$(top_builddir)/innobase/rem/librem.a\
 \$(top_builddir)/innobase/thr/libthr.a\
 \$(top_builddir)/innobase/sync/libsync.a\
 \$(top_builddir)/innobase/data/libdata.a\
 \$(top_builddir)/innobase/mach/libmach.a\
 \$(top_builddir)/innobase/ha/libha.a\
 \$(top_builddir)/innobase/dyn/libdyn.a\
 \$(top_builddir)/innobase/mem/libmem.a\
 \$(top_builddir)/innobase/sync/libsync.a\
 \$(top_builddir)/innobase/ut/libut.a\
 \$(top_builddir)/innobase/os/libos.a\
 \$(top_builddir)/innobase/ut/libut.a"

      AC_CHECK_LIB(rt, aio_read, [innodb_system_libs="-lrt"])
      ;;
    * )
      AC_MSG_RESULT([Not using Innodb])
      ;;
  esac

  AC_SUBST(innodb_includes)
  AC_SUBST(innodb_libs)
  AC_SUBST(innodb_system_libs)
])

dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_INNODB SECTION
dnl ---------------------------------------------------------------------------

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

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_CSVDB
dnl Sets HAVE_CSV_DB if --with-csv-storage-engine is used
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_CSVDB], [
  AC_ARG_WITH([csv-storage-engine],
              [
  --with-csv-storage-engine
                          Enable the CSV Storage Engine],
              [csvdb="$withval"],
              [csvdb=no])
  AC_MSG_CHECKING([for csv storage engine])

  case "$csvdb" in
    yes )
      AC_DEFINE([HAVE_CSV_DB], [1], [Builds the CSV Storage Engine])
      AC_MSG_RESULT([yes])
      [csvdb=yes]
      ;;
    * )
      AC_MSG_RESULT([no])
      [csvdb=no]
      ;;
  esac

])
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_CSV SECTION
dnl ---------------------------------------------------------------------------


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_NDBCLUSTER
dnl Sets HAVE_NDBCLUSTER_DB if --with-ndbcluster is used
dnl ---------------------------------------------------------------------------
                                                                                
AC_DEFUN([MYSQL_CHECK_NDB_OPTIONS], [
  AC_ARG_WITH([ndb-sci],
              AC_HELP_STRING([--with-ndb-sci=DIR],
                             [Provide MySQL with a custom location of
                             sci library. Given DIR, sci library is 
                             assumed to be in $DIR/lib and header files
                             in $DIR/include.]),
              [mysql_sci_dir=${withval}],
              [mysql_sci_dir=""])

  case "$mysql_sci_dir" in
    "no" )
      have_ndb_sci=no
      AC_MSG_RESULT([-- not including sci transporter])
      ;;
    * )
      if test -f "$mysql_sci_dir/lib/libsisci.a" -a \ 
              -f "$mysql_sci_dir/include/sisci_api.h"; then
        NDB_SCI_INCLUDES="-I$mysql_sci_dir/include"
        NDB_SCI_LIBS="-L$mysql_sci_dir/lib -lsisci"
        AC_MSG_RESULT([-- including sci transporter])
        AC_DEFINE([NDB_SCI_TRANSPORTER], [1],
                  [Including Ndb Cluster DB sci transporter])
        AC_SUBST(NDB_SCI_INCLUDES)
        AC_SUBST(NDB_SCI_LIBS)
        have_ndb_sci="yes"
        AC_MSG_RESULT([found sci transporter in $mysql_sci_dir/{include, lib}])
      else
        AC_MSG_RESULT([could not find sci transporter in $mysql_sci_dir/{include, lib}])
      fi
      ;;
  esac

  AC_ARG_WITH([ndb-shm],
              [
  --with-ndb-shm        Include the NDB Cluster shared memory transporter],
              [ndb_shm="$withval"],
              [ndb_shm=no])
  AC_ARG_WITH([ndb-test],
              [
  --with-ndb-test       Include the NDB Cluster ndbapi test programs],
              [ndb_test="$withval"],
              [ndb_test=no])
  AC_ARG_WITH([ndb-docs],
              [
  --with-ndb-docs       Include the NDB Cluster ndbapi and mgmapi documentation],
              [ndb_docs="$withval"],
              [ndb_docs=no])
  AC_ARG_WITH([ndb-port-base],
              [
  --with-ndb-port-base  Base port for NDB Cluster],
              [ndb_port_base="$withval"],
              [ndb_port_base="default"])
                                                                                
  AC_MSG_CHECKING([for NDB Cluster options])
  AC_MSG_RESULT([])
                                                                                
  have_ndb_shm=no
  case "$ndb_shm" in
    yes )
      AC_MSG_RESULT([-- including shared memory transporter])
      AC_DEFINE([NDB_SHM_TRANSPORTER], [1],
                [Including Ndb Cluster DB shared memory transporter])
      have_ndb_shm="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including shared memory transporter])
      ;;
  esac

  have_ndb_test=no
  case "$ndb_test" in
    yes )
      AC_MSG_RESULT([-- including ndbapi test programs])
      have_ndb_test="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi test programs])
      ;;
  esac

  have_ndb_docs=no
  case "$ndb_docs" in
    yes )
      AC_MSG_RESULT([-- including ndbapi and mgmapi documentation])
      have_ndb_docs="yes"
      ;;
    * )
      AC_MSG_RESULT([-- not including ndbapi and mgmapi documentation])
      ;;
  esac

  AC_MSG_RESULT([done.])
])

AC_DEFUN([MYSQL_CHECK_NDBCLUSTER], [
  AC_ARG_WITH([ndbcluster],
              [
  --with-ndbcluster        Include the NDB Cluster table handler],
              [ndbcluster="$withval"],
              [ndbcluster=no])
                                                                                
  AC_MSG_CHECKING([for NDB Cluster])
                                                                                
  have_ndbcluster=no
  ndbcluster_includes=
  ndbcluster_libs=
  case "$ndbcluster" in
    yes )
      AC_MSG_RESULT([Using NDB Cluster])
      AC_DEFINE([HAVE_NDBCLUSTER_DB], [1], [Using Ndb Cluster DB])
      have_ndbcluster="yes"
      ndbcluster_includes="-I../ndb/include -I../ndb/include/ndbapi"
      ndbcluster_libs="\$(top_builddir)/ndb/src/.libs/libndbclient.a"
      ndbcluster_system_libs=""
      MYSQL_CHECK_NDB_OPTIONS
      ;;
    * )
      AC_MSG_RESULT([Not using NDB Cluster])
      ;;
  esac

  AM_CONDITIONAL([HAVE_NDBCLUSTER_DB], [ test "$have_ndbcluster" = "yes" ])
  AC_SUBST(ndbcluster_includes)
  AC_SUBST(ndbcluster_libs)
  AC_SUBST(ndbcluster_system_libs)
])
                                                                                
dnl ---------------------------------------------------------------------------
dnl END OF MYSQL_CHECK_NDBCLUSTER SECTION
dnl ---------------------------------------------------------------------------


dnl By default, many hosts won't let programs access large files;
dnl one must use special compiler options to get large-file access to work.
dnl For more details about this brain damage please see:
dnl http://www.sas.com/standards/large.file/x_open.20Mar96.html

dnl Written by Paul Eggert <eggert@twinsun.com>.

dnl Internal subroutine of AC_SYS_LARGEFILE.
dnl AC_SYS_LARGEFILE_FLAGS(FLAGSNAME)
AC_DEFUN([AC_SYS_LARGEFILE_FLAGS],
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
	        case `$CC --version 2>/dev/null` in
	          2.95.*) ac_cv_sys_largefile_CFLAGS=-D__STDC_EXT__ ;;
		esac
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
AC_DEFUN([AC_SYS_LARGEFILE_SPACE_APPEND],
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
AC_DEFUN([AC_SYS_LARGEFILE_MACRO_VALUE],
  [AC_CACHE_CHECK([for $1], $2,
     [$2=no
changequote(, )dnl
      for ac_flag in $ac_cv_sys_largefile_CFLAGS no; do
	case "$ac_flag" in
	-D$1)
	  $2=1 ;;
	-D$1=*)
	  $2=`expr " $ac_flag" : '[^=]*=\(.*\)'` ;;
	esac
      done
      $4
changequote([, ])dnl
      ])
   if test "[$]$2" != no; then
     AC_DEFINE_UNQUOTED([$1], [$]$2, [$3])
   fi])

AC_DEFUN([MYSQL_SYS_LARGEFILE],
  [AC_REQUIRE([AC_CANONICAL_HOST])
  AC_ARG_ENABLE(largefile,
     [  --disable-largefile     Omit support for large files])
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
	# We can't declare _FILE_OFFSET_BITS here as this will cause
	# compile errors as AC_PROG_CC adds include files in confdefs.h
	# We solve this (until autoconf is fixed) by instead declaring it
	# as define instead
	solaris2.[8,9])
	  CFLAGS="$CFLAGS -D_FILE_OFFSET_BITS=64"
	  CXXFLAGS="$CXXFLAGS -D_FILE_OFFSET_BITS=64"
	  ac_cv_sys_file_offset_bits=no ;;
	esac])
     AC_SYS_LARGEFILE_MACRO_VALUE(_LARGEFILE_SOURCE,
       ac_cv_sys_largefile_source,
       [makes fseeko etc. visible, on some hosts.],
       [case "$host_os" in
	# HP-UX 10.20 and later
	hpux10.[2-9][0-9]* | hpux1[1-9]* | hpux[2-9][0-9]*)
	  ac_cv_sys_largefile_source=1 ;;
	esac])
     AC_SYS_LARGEFILE_MACRO_VALUE(_LARGE_FILES,
       ac_cv_sys_large_files,
       [Large files support on AIX-style hosts.],
       [case "$host_os" in
	# AIX 4.2 and later
	aix4.[2-9]* | aix4.1[0-9]* | aix[5-9].* | aix[1-9][0-9]*)
	  ac_cv_sys_large_files=1 ;;
	esac])
   fi
  ])


# Local version of _AC_PROG_CXX_EXIT_DECLARATION that does not
# include #stdlib.h as default as this breaks things on Solaris
# (Conflicts with pthreads and big file handling)

m4_define([_AC_PROG_CXX_EXIT_DECLARATION],
[for ac_declaration in \
   ''\
   'extern "C" void std::exit (int) throw (); using std::exit;' \
   'extern "C" void std::exit (int); using std::exit;' \
   'extern "C" void exit (int) throw ();' \
   'extern "C" void exit (int);' \
   'void exit (int);' \
   '#include <stdlib.h>'
do
  _AC_COMPILE_IFELSE([AC_LANG_PROGRAM([@%:@include <stdlib.h>
$ac_declaration],
                                      [exit (42);])],
                     [],
                     [continue])
  _AC_COMPILE_IFELSE([AC_LANG_PROGRAM([$ac_declaration],
                                      [exit (42);])],
                     [break])
done
rm -f conftest*
if test -n "$ac_declaration"; then
  echo '#ifdef __cplusplus' >>confdefs.h
  echo $ac_declaration      >>confdefs.h
  echo '#endif'             >>confdefs.h
fi
])# _AC_PROG_CXX_EXIT_DECLARATION

dnl ---------------------------------------------------------------------------

