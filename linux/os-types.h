#if !defined(OS_INTERFACE_LINUX_H)
#define OS_INTERFACE_LINUX_H
#include <sys/types.h>

typedef int os_handle_t;

struct fileid {
    dev_t st_dev; /* device and inode are enough to uniquely identify a file in unix. */
    ino_t st_ino;
};

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

#endif
