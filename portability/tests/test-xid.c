#include <config.h>
#include <stdio.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include "toku_os.h"
#if defined(HAVE_SYSCALL_H)
# include <syscall.h>
#elif defined(HAVE_SYS_SYSCALL_H)
# include <sys/syscall.h>
# if !defined(__NR_gettid) && defined(SYS_gettid)
#  define __NR_gettid SYS_gettid
# endif
#endif

static int gettid(void) {
    return syscall(__NR_gettid);
}

int main(void) {
    assert(toku_os_getpid() == getpid());
    assert(toku_os_gettid() == gettid());
    return 0;
}
