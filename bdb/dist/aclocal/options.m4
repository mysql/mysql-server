dnl $Id: options.m4,v 11.10 2000/07/07 15:50:39 bostic Exp $

dnl Process user-specified options.
AC_DEFUN(AM_OPTIONS_SET, [

AC_MSG_CHECKING(if --disable-bigfile option specified)
AC_ARG_ENABLE(bigfile,
	[  --disable-bigfile       Disable AIX, HP/UX, Solaris big files.],
	[db_cv_bigfile="yes"], [db_cv_bigfile="no"])
AC_MSG_RESULT($db_cv_bigfile)

AC_MSG_CHECKING(if --enable-compat185 option specified)
AC_ARG_ENABLE(compat185,
	[  --enable-compat185      Build DB 1.85 compatibility API.],
	[db_cv_compat185="$enable_compat185"], [db_cv_compat185="no"])
AC_MSG_RESULT($db_cv_compat185)

AC_MSG_CHECKING(if --enable-cxx option specified)
AC_ARG_ENABLE(cxx,
	[  --enable-cxx            Build C++ API.],
	[db_cv_cxx="$enable_cxx"], [db_cv_cxx="no"])
AC_MSG_RESULT($db_cv_cxx)

AC_MSG_CHECKING(if --enable-debug option specified)
AC_ARG_ENABLE(debug,
	[  --enable-debug          Build a debugging version.],
	[db_cv_debug="$enable_debug"], [db_cv_debug="no"])
AC_MSG_RESULT($db_cv_debug)

AC_MSG_CHECKING(if --enable-debug_rop option specified)
AC_ARG_ENABLE(debug_rop,
	[  --enable-debug_rop      Build a version that logs read operations.],
	[db_cv_debug_rop="$enable_debug_rop"], [db_cv_debug_rop="no"])
AC_MSG_RESULT($db_cv_debug_rop)

AC_MSG_CHECKING(if --enable-debug_wop option specified)
AC_ARG_ENABLE(debug_wop,
	[  --enable-debug_wop      Build a version that logs write operations.],
	[db_cv_debug_wop="$enable_debug_wop"], [db_cv_debug_wop="no"])
AC_MSG_RESULT($db_cv_debug_wop)

AC_MSG_CHECKING(if --enable-diagnostic option specified)
AC_ARG_ENABLE(diagnostic,
	[  --enable-diagnostic     Build a version with run-time diagnostics.],
	[db_cv_diagnostic="$enable_diagnostic"], [db_cv_diagnostic="no"])
AC_MSG_RESULT($db_cv_diagnostic)

AC_MSG_CHECKING(if --enable-dump185 option specified)
AC_ARG_ENABLE(dump185,
	[  --enable-dump185        Build db_dump185(1) to dump 1.85 databases.],
	[db_cv_dump185="$enable_dump185"], [db_cv_dump185="no"])
AC_MSG_RESULT($db_cv_dump185)

AC_MSG_CHECKING(if --enable-dynamic option specified)
AC_ARG_ENABLE(dynamic,
	[  --enable-dynamic        Build with dynamic libraries.],
	[db_cv_dynamic="$enable_dynamic"], [db_cv_dynamic="no"])
AC_MSG_RESULT($db_cv_dynamic)

AC_MSG_CHECKING(if --enable-java option specified)
AC_ARG_ENABLE(java,
	[  --enable-java           Build Java API.],
	[db_cv_java="$enable_java"], [db_cv_java="no"])
AC_MSG_RESULT($db_cv_java)

AC_MSG_CHECKING(if --enable-posixmutexes option specified)
AC_ARG_ENABLE(posixmutexes,
	[  --enable-posixmutexes   Force use of POSIX standard mutexes.],
	[db_cv_posixmutexes="$enable_posixmutexes"], [db_cv_posixmutexes="no"])
AC_MSG_RESULT($db_cv_posixmutexes)

AC_MSG_CHECKING(if --enable-rpc option specified)
AC_ARG_ENABLE(rpc,
	[  --enable-rpc            Build RPC client/server.],
	[db_cv_rpc="$enable_rpc"], [db_cv_rpc="no"])
AC_MSG_RESULT($db_cv_rpc)

dnl --enable-shared is an alias for --enable-dynamic. We support it for
dnl compatibility with other applications, e.g., Tcl.
AC_MSG_CHECKING(if --enable-shared option specified)
AC_ARG_ENABLE(shared,
	[  --enable-shared         Build with dynamic libraries.],
	[db_cv_shared="$enable_shared"], [db_cv_shared="no"])
AC_MSG_RESULT($db_cv_shared)
if test "$db_cv_shared" != "no"; then
	db_cv_dynamic="yes"
fi

AC_MSG_CHECKING(if --enable-tcl option specified)
AC_ARG_ENABLE(tcl,
	[  --enable-tcl            Build Tcl API.],
	[db_cv_tcl="$enable_tcl"], [db_cv_tcl="no"])
AC_MSG_RESULT($db_cv_tcl)

AC_MSG_CHECKING(if --enable-test option specified)
AC_ARG_ENABLE(test,
	[  --enable-test           Configure to run the test suite.],
	[db_cv_test="$enable_test"], [db_cv_test="no"])
AC_MSG_RESULT($db_cv_test)

AC_MSG_CHECKING(if --enable-uimutexes option specified)
AC_ARG_ENABLE(uimutexes,
	[  --enable-uimutexes      Force use of Unix International mutexes.],
	[db_cv_uimutexes="$enable_uimutexes"], [db_cv_uimutexes="no"])
AC_MSG_RESULT($db_cv_uimutexes)

AC_MSG_CHECKING(if --enable-umrw option specified)
AC_ARG_ENABLE(umrw,
	[  --enable-umrw           Mask harmless unitialized memory read/writes.],
	[db_cv_umrw="$enable_umrw"], [db_cv_umrw="no"])
AC_MSG_RESULT($db_cv_umrw)

AC_MSG_CHECKING([if --with-tcl option specified])
AC_ARG_WITH(tcl,
	[  --with-tcl=DIR          Directory location of tclConfig.sh.],
	with_tclconfig=${withval}, with_tclconfig="no")
AC_MSG_RESULT($with_tclconfig)
if test "$with_tclconfig" != "no"; then
	db_cv_tcl="yes"
fi
])dnl
