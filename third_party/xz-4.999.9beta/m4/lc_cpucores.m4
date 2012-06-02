dnl ###########################################################################
dnl
dnl lc_CPUCORES - Check how to find out the number of online CPU cores
dnl
dnl Check how to find out the number of available CPU cores in the system.
dnl sysconf(_SC_NPROCESSORS_ONLN) works on most systems, except that BSDs
dnl use sysctl().
dnl
dnl ###########################################################################
dnl
dnl Author: Lasse Collin
dnl
dnl This file has been put into the public domain.
dnl You can do whatever you want with this file.
dnl
dnl ###########################################################################
AC_DEFUN([lc_CPUCORES], [
AC_MSG_CHECKING([how to detect the number of available CPU cores])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main(void)
{
	long i;
	i = sysconf(_SC_NPROCESSORS_ONLN);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_CPUCORES_SYSCONF], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysconf(_SC_NPROCESSORS_ONLN).])
	AC_MSG_RESULT([sysconf])
], [
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/types.h>
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
#include <sys/sysctl.h>
int
main(void)
{
	int name[2] = { CTL_HW, HW_NCPU };
	int cpus;
	size_t cpus_size = sizeof(cpus);
	sysctl(name, 2, &cpus, &cpus_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_CPUCORES_SYSCTL], [1],
		[Define to 1 if the number of available CPU cores can be
		detected with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
	AC_MSG_RESULT([unknown])
])])
])dnl lc_CPUCORES
