dnl $Id: programs.m4,v 11.11 2000/03/30 21:20:50 bostic Exp $

dnl Check for programs used in building/installation.
AC_DEFUN(AM_PROGRAMS_SET, [

AC_PATH_PROG(db_cv_path_ar, ar, missing_ar)
if test "$db_cv_path_ar" = missing_ar; then
	AC_MSG_ERROR([No ar utility found.])
fi
AC_PATH_PROG(db_cv_path_chmod, chmod, missing_chmod)
if test "$db_cv_path_chmod" = missing_chmod; then
	AC_MSG_ERROR([No chmod utility found.])
fi
AC_PATH_PROG(db_cv_path_cp, cp, missing_cp)
if test "$db_cv_path_cp" = missing_cp; then
	AC_MSG_ERROR([No cp utility found.])
fi
AC_PATH_PROG(db_cv_path_ln, ln, missing_ln)
if test "$db_cv_path_ln" = missing_ln; then
	AC_MSG_ERROR([No ln utility found.])
fi
AC_PATH_PROG(db_cv_path_mkdir, mkdir, missing_mkdir)
if test "$db_cv_path_mkdir" = missing_mkdir; then
	AC_MSG_ERROR([No mkdir utility found.])
fi
AC_PATH_PROG(db_cv_path_ranlib, ranlib, missing_ranlib)
AC_PATH_PROG(db_cv_path_rm, rm, missing_rm)
if test "$db_cv_path_rm" = missing_rm; then
	AC_MSG_ERROR([No rm utility found.])
fi
AC_PATH_PROG(db_cv_path_sh, sh, missing_sh)
if test "$db_cv_path_sh" = missing_sh; then
	AC_MSG_ERROR([No sh utility found.])
fi
AC_PATH_PROG(db_cv_path_strip, strip, missing_strip)
if test "$db_cv_path_strip" = missing_strip; then
	AC_MSG_ERROR([No strip utility found.])
fi

dnl Check for programs used in testing.
if test "$db_cv_test" = "yes"; then
	AC_PATH_PROG(db_cv_path_kill, kill, missing_kill)
	if test "$db_cv_path_kill" = missing_kill; then
		AC_MSG_ERROR([No kill utility found.])
	fi
fi

])dnl
