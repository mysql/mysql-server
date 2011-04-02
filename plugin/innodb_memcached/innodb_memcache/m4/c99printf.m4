dnl **********************************************************************
dnl DETECT_UINT64_SUPPORT
dnl
dnl check if we can use a uint64_t
dnl **********************************************************************
AC_DEFUN([AC_C_DETECT_UINT64_SUPPORT],
[
    AC_CACHE_CHECK([for print macros for integers (C99 section 7.8.1)],
        [ac_cv_c_uint64_support],
        [AC_TRY_COMPILE(
            [
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#include <stdio.h>
            ], [
  uint64_t val = 0;
  fprintf(stderr, "%" PRIu64 "\n", val);
            ],
            [ ac_cv_c_uint64_support=yes ],
            [ ac_cv_c_uint64_support=no ])
        ])
])
