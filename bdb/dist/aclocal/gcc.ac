# Version 2.96 of gcc (shipped with RedHat Linux 7.[01] and Mandrake) had
# serious problems.
AC_DEFUN(AC_GCC_CONFIG1, [
AC_CACHE_CHECK([whether we are using gcc version 2.96],
db_cv_gcc_2_96, [
db_cv_gcc_2_96=no
if test "$GCC" = "yes"; then
	GCC_VERSION=`${MAKEFILE_CC} --version`
	case ${GCC_VERSION} in
	2.96*)
		db_cv_gcc_2_96=yes;;
	esac
fi])
if test "$db_cv_gcc_2_96" = "yes"; then
	CFLAGS=`echo "$CFLAGS" | sed 's/-O2/-O/'`
	CXXFLAGS=`echo "$CXXFLAGS" | sed 's/-O2/-O/'`
	AC_MSG_WARN([INSTALLED GCC COMPILER HAS SERIOUS BUGS; PLEASE UPGRADE.])
	AC_MSG_WARN([GCC OPTIMIZATION LEVEL SET TO -O.])
fi])

# Versions of g++ up to 2.8.0 required -fhandle-exceptions, but it is
# renamed as -fexceptions and is the default in versions 2.8.0 and after.
AC_DEFUN(AC_GCC_CONFIG2, [
AC_CACHE_CHECK([whether g++ requires -fhandle-exceptions],
db_cv_gxx_except, [
db_cv_gxx_except=no;
if test "$GXX" = "yes"; then
	GXX_VERSION=`${MAKEFILE_CXX} --version`
	case ${GXX_VERSION} in
	1.*|2.[[01234567]].*|*-1.*|*-2.[[01234567]].*)
		db_cv_gxx_except=yes;;
	esac
fi])
if test "$db_cv_gxx_except" = "yes"; then
	CXXFLAGS="$CXXFLAGS -fhandle-exceptions"
fi])
