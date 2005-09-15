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

dnl
dnl check for availability of multibyte characters and functions
dnl (Based on BASH_CHECK_MULTIBYTE in aclocal.m4 of readline-5.0)
dnl
AC_DEFUN([MYSQL_CHECK_MULTIBYTE],
[
AC_CHECK_HEADERS(wctype.h)
AC_CHECK_HEADERS(wchar.h)
AC_CHECK_HEADERS(langinfo.h)

AC_CHECK_FUNC(mbsrtowcs, AC_DEFINE([HAVE_MBSRTOWCS],[],[Define if you have mbsrtowcs]))
AC_CHECK_FUNC(mbrtowc, AC_DEFINE([HAVE_MBRTOWC],[],[Define if you have mbrtowc]))
AC_CHECK_FUNC(mbrlen, AC_DEFINE([HAVE_MBRLEN],[],[Define if you have mbrlen]))
AC_CHECK_FUNC(wctomb, AC_DEFINE([HAVE_WCTOMB],[],[Define if you have wctomb]))
AC_CHECK_FUNC(wcwidth, AC_DEFINE([HAVE_WCWIDTH],[],[Define if you have wcwidth]))
AC_CHECK_FUNC(wcsdup, AC_DEFINE([HAVE_WCSDUP],[],[Define if you check wcsdup]))

AC_CACHE_CHECK([for mbstate_t], mysql_cv_have_mbstate_t,
[AC_TRY_COMPILE([
#include <wchar.h>], [
  mbstate_t ps;
  mbstate_t *psp;
  psp = (mbstate_t *)0;
], mysql_cv_have_mbstate_t=yes,  mysql_cv_have_mbstate_t=no)])
if test $mysql_cv_have_mbstate_t = yes; then
        AC_DEFINE([HAVE_MBSTATE_T],[],[Define if mysql_cv_have_mbstate_t=yes])
fi

AC_CACHE_CHECK([for nl_langinfo and CODESET], mysql_cv_langinfo_codeset,
[AC_TRY_LINK(
[#include <langinfo.h>],
[char* cs = nl_langinfo(CODESET);],
mysql_cv_langinfo_codeset=yes, mysql_cv_langinfo_codeset=no)])
if test $mysql_cv_langinfo_codeset = yes; then
  AC_DEFINE([HAVE_LANGINFO_CODESET],[],[Define if mysql_cv_langinfo_codeset=yes])
fi

])
