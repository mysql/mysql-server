# C++ checks to determine what style of headers to use and
# whether to use "using" clauses.

AC_DEFUN(AC_CXX_HAVE_STDHEADERS, [
AC_SUBST(cxx_have_stdheaders)
AC_CACHE_CHECK([whether C++ supports the ISO C++ standard includes],
db_cv_cxx_have_stdheaders,
[AC_LANG_SAVE
 AC_LANG_CPLUSPLUS
 AC_TRY_COMPILE([#include <iostream>
],[std::ostream *o; return 0;],
 db_cv_cxx_have_stdheaders=yes, db_cv_cxx_have_stdheaders=no)
 AC_LANG_RESTORE
])
if test "$db_cv_cxx_have_stdheaders" = yes; then
	cxx_have_stdheaders="#define	HAVE_CXX_STDHEADERS 1"
fi])
