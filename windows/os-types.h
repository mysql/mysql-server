#if !defined(OS_INTERFACE_WINDOWS_H)
#define OS_INTERFACE_WINDOWS_H
#include <stdlib.h>
#include <direct.h>

// define an OS handle
typedef void *os_handle_t;
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

#endif

