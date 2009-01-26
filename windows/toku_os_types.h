#ifndef TOKU_OS_TYPES_H
#define TOKU_OS_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <direct.h>

// define an OS handle
typedef void *toku_os_handle_t;
typedef int  pid_t; 
typedef int  mode_t;

struct fileid {
    uint32_t st_dev;
    uint64_t st_ino;
};


#if defined(__cplusplus)
};
#endif

#endif

