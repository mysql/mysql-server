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
    uint64_t st_creat;
};

enum {
    DT_UNKNOWN = 0,
    DT_DIR     = 4,
    DT_REG     = 8
};

struct dirent {
    char          d_name[_MAX_PATH];
    unsigned char d_type;
};

struct __toku_windir;
typedef struct __toku_windir DIR;

#if defined(__cplusplus)
};
#endif

#endif

