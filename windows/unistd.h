#ifndef _TOKUWIN_UNISTD_H
#define _TOKUWIN_UNISTD_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <io.h>
#include <stdio.h>
#include <stdint.h>
#include <getopt.h>

int fsync(int fildes);

int
ftruncate(int fildes, toku_off_t offset);

int
truncate(const char *path, toku_off_t length);

int64_t
pwrite(int fildes, const void *buf, size_t nbyte, int64_t offset);

int64_t
pread(int fildes, void *buf, size_t nbyte, int64_t offset);

unsigned int
sleep(unsigned int);

int
usleep(unsigned int);

#if defined(__cplusplus)
};
#endif

#endif

