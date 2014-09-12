/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

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
