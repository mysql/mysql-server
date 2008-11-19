#if !defined(TOKU_OS_TYPES_H)
#define TOKU_OS_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <sys/types.h>

typedef int toku_os_handle_t;

struct fileid {
    dev_t st_dev; /* device and inode are enough to uniquely identify a file in unix. */
    ino_t st_ino;
};

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

#if defined(__cplusplus)
};
#endif

#endif
