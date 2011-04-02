AC_DEFUN([AC_C_ALIGNMENT],
[AC_CACHE_CHECK(for alignment, ac_cv_c_alignment,
[
  AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([
#include <stdlib.h>
#include <inttypes.h>
    ], [
       char *buf = malloc(32);

       uint64_t *ptr = (uint64_t*)(buf+2);
       // catch sigbus, etc.
       *ptr = 0x1;

       // catch unaligned word access (ARM cpus)
       *buf =  1; *(buf +1) = 2; *(buf + 2) = 2; *(buf + 3) = 3; *(buf + 4) = 4;
       int* i = (int*)(buf+1);
       return (84148994 == i) ? 0 : 1;
    ])
  ],[
    ac_cv_c_alignment=none
  ],[
    ac_cv_c_alignment=need
  ])
])
if test $ac_cv_c_alignment = need; then
  AC_DEFINE(NEED_ALIGN, 1, [Machine need alignment])
fi
])
