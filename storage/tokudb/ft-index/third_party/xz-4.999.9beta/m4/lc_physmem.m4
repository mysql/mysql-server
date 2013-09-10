dnl ###########################################################################
dnl
dnl lc_PHYSMEM - Check how to find out the amount of physical memory
dnl
dnl - sysconf() gives all the needed info on GNU+Linux and Solaris.
dnl - BSDs use sysctl().
dnl - sysinfo() works on Linux/dietlibc and probably on other Linux systems
dnl   whose libc may lack sysconf().
dnl
dnl ###########################################################################
dnl
dnl Author: Lasse Collin
dnl
dnl This file has been put into the public domain.
dnl You can do whatever you want with this file.
dnl
dnl ###########################################################################
AC_DEFUN([lc_PHYSMEM], [
AC_MSG_CHECKING([how to detect the amount of physical memory])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <unistd.h>
int
main(void)
{
	long i;
	i = sysconf(_SC_PAGESIZE);
	i = sysconf(_SC_PHYS_PAGES);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCONF], [1],
		[Define to 1 if the amount of physical memory can be detected
		with sysconf(_SC_PAGESIZE) and sysconf(_SC_PHYS_PAGES).])
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
	int name[2] = { CTL_HW, HW_PHYSMEM };
	unsigned long mem;
	size_t mem_ptr_size = sizeof(mem);
	sysctl(name, 2, &mem, &mem_ptr_size, NULL, NULL);
	return 0;
}
]])], [
	AC_DEFINE([HAVE_PHYSMEM_SYSCTL], [1],
		[Define to 1 if the amount of physical memory can be detected
		with sysctl().])
	AC_MSG_RESULT([sysctl])
], [
dnl sysinfo() is Linux-specific. Some non-Linux systems have
dnl incompatible sysinfo() so we must check $host_os.
case $host_os in
	linux*)
		AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
#include <sys/sysinfo.h>
int
main(void)
{
	struct sysinfo si;
	sysinfo(&si);
	return 0;
}
		]])], [
			AC_DEFINE([HAVE_PHYSMEM_SYSINFO], [1],
				[Define to 1 if the amount of physical memory
				can be detected with Linux sysinfo().])
			AC_MSG_RESULT([sysinfo])
		], [
			AC_MSG_RESULT([unknown])
		])
		;;
	*)
		AC_MSG_RESULT([unknown])
		;;
esac
])])
])dnl lc_PHYSMEM
