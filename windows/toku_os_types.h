/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TOKU_OS_TYPES_H
#define TOKU_OS_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>


// define an OS handle
typedef void *toku_os_handle_t;
typedef int  pid_t; 
typedef int  mode_t;

struct fileid {
    uint32_t st_dev;
    uint64_t st_ino;
};

typedef struct _stati64 toku_struct_stat; 

#if defined(__cplusplus)
};
#endif

#endif

