/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#ifndef TOKU_OS_H
#define TOKU_OS_H

#include <dirent.h>
#include <sys/time.h>

#include <toku_os_types.h>

// Returns: the current process id
int toku_os_getpid(void)   __attribute__((__visibility__("default")));

// Returns: the current thread id
int toku_os_gettid(void)  __attribute__((__visibility__("default")));

// Returns: the number of processors in the system
int toku_os_get_number_processors(void);

// Returns: the number of active processors in the system
int toku_os_get_number_active_processors(void);

// Returns: the system page size (in bytes)
int toku_os_get_pagesize(void);

// Returns: the size of physical memory (in bytes)
uint64_t toku_os_get_phys_memory_size(void) __attribute__((__visibility__("default")));

// Returns the processor frequency in Hz
// Returns 0 if success
int toku_os_get_processor_frequency(uint64_t *hz);

// Returns: 0 on success
// sets fsize to the number of bytes in a file
int toku_os_get_file_size(int fildes, int64_t *fsize)   __attribute__((__visibility__("default")));

// Returns: 0 on success
// Initializes id as a unique fileid for fildes on success.
int toku_os_get_unique_file_id(int fildes, struct fileid *id);

//Locks a file (should not be open to begin with).
//Returns: file descriptor (or -1 on error)
int toku_os_lock_file(const char *name);

//Unlocks and closes a file locked by toku_os_lock_on_file
int toku_os_unlock_file(int fildes);

int toku_os_mkdir(const char *pathname, mode_t mode) __attribute__((__visibility__("default")));

// Get the current process user and kernel use times
int toku_os_get_process_times(struct timeval *usertime, struct timeval *kerneltime);

// Get the maximum size of the process data size (in bytes)
// Success: returns 0 and sets *maxdata to the data size
// Fail: returns an error number
int toku_os_get_max_process_data_size(uint64_t *maxdata) __attribute__((__visibility__("default")));

int toku_os_initialize_settings(int verbosity)  __attribute__((__visibility__("default")));

bool toku_os_is_absolute_name(const char* path)  __attribute__((__visibility__("default")));

// Set whether or not writes assert when ENOSPC is returned or they wait for space
void toku_set_assert_on_write_enospc(int do_assert) __attribute__((__visibility__("default")));

// Get file system write information
// *enospc_last_time is the last time ENOSPC was returned by write or pwrite
// *enospc_current   is the number of threads waiting on space
// *enospc_total     is the number of times ENOSPC was returned by write or pwrite
void toku_fs_get_write_info(time_t *enospc_last_time, uint64_t *enospc_current, uint64_t *enospc_total);

void toku_fsync_dirfd_without_accounting(DIR *dirp);

int toku_fsync_dir_by_name_without_accounting(const char *dir_name);

// Get the file system free and total space for the file system that contains a path name
// *avail_size is set to the bytes of free space in the file system available for non-root 
// *free_size is set to the bytes of free space in the file system
// *total_size is set to the total bytes in the file system
// Return 0 on success, otherwise an error number
int toku_get_filesystem_sizes(const char *path, uint64_t *avail_size, uint64_t *free_size, uint64_t *total_size);

#if TOKU_WINDOWS
#include <sys/types.h>
#include <sys/stat.h>
//Test if st_mode (from stat) is a directory
#define S_ISDIR(bitvector)  (((bitvector)&_S_IFDIR)!=0)
#endif

// Portable linux 'stat'
int toku_stat(const char *name, toku_struct_stat *statbuf) __attribute__((__visibility__("default")));
// Portable linux 'fstat'
int toku_fstat(int fd, toku_struct_stat *statbuf) __attribute__((__visibility__("default")));

// Portable linux 'dup2'
int toku_dup2(int fd, int fd2) __attribute__((__visibility__("default")));

#endif /* TOKU_OS_H */
