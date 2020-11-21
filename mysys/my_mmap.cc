/* Copyright (c) 2000, 2019, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file mysys/my_mmap.cc
*/

#include "my_config.h"

#include <stddef.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include "my_config.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"

#ifdef HAVE_SYS_MMAN_H

/*
  system msync() only syncs mmap'ed area to fs cache.
  fsync() is required to really sync to disc
*/
int my_msync(int fd, void *addr, size_t len, int flags) {
  msync(static_cast<char *>(addr), len, flags);
  return my_sync(fd, MYF(0));
}

#elif defined(_WIN32)

static SECURITY_ATTRIBUTES mmap_security_attributes = {
    sizeof(SECURITY_ATTRIBUTES), 0, TRUE};

void *my_mmap(void *addr, size_t len, int prot, int flags, File fd,
              my_off_t offset) {
  HANDLE hFileMap;
  LPVOID ptr;
  HANDLE hFile = my_get_osfhandle(fd);
  DBUG_TRACE;
  DBUG_PRINT("mysys", ("map fd: %d", fd));

  if (hFile == INVALID_HANDLE_VALUE) return MAP_FAILED;

  hFileMap = CreateFileMapping(hFile, &mmap_security_attributes, PAGE_READWRITE,
                               0, (DWORD)len, NULL);
  if (hFileMap == 0) return MAP_FAILED;

  ptr = MapViewOfFile(hFileMap,
                      prot & PROT_WRITE ? FILE_MAP_WRITE : FILE_MAP_READ,
                      (DWORD)(offset >> 32), (DWORD)offset, len);

  /*
    MSDN explicitly states that it's possible to close File Mapping Object
    even when a view is not unmapped - then the object will be held open
    implicitly until unmap, as every view stores internally a handler of
    a corresponding File Mapping Object
   */
  CloseHandle(hFileMap);

  if (ptr) {
    DBUG_PRINT("mysys", ("mapped addr: %p", ptr));
    return ptr;
  }

  return MAP_FAILED;
}

int my_munmap(void *addr, size_t len) {
  DBUG_TRACE;
  DBUG_PRINT("mysys", ("unmap addr: %p", addr));
  return UnmapViewOfFile(addr) ? 0 : -1;
}

int my_msync(int fd, void *addr, size_t len, int flags) {
  return FlushViewOfFile(addr, len) ? 0 : -1;
}

#else
#error "no mmap!"
#endif

int init_mmap_info(MMAP_INFO *info, File file, size_t mmap_length,
                   my_off_t seek_offset) {
  DBUG_TRACE;
  DBUG_ASSERT(file > 0);
  info->file = file;
  void *addr = mmap(NULL, mmap_length, PROT_WRITE, MAP_SHARED, file, 0);
  if (addr == MAP_FAILED) {
    DBUG_PRINT("mysys", ("mmap failed when init mmap info"));
    return 1;
  }

  info->addr = static_cast<uchar *>(addr);
  info->mmap_length = mmap_length;
  info->mmap_end = info->addr + mmap_length;

  // for mmap file, we always mmap from 0, forward the pointers seek_offset byte
  info->end_pos_of_file = seek_offset;
  info->write_pos = info->addr + seek_offset;
  info->sync_pos = info->addr + seek_offset;

  DBUG_PRINT("info", ("init_map_info: addr = %p, mmap_length = %lu",
             info->addr, static_cast<ulong>(mmap_length)));

  return 0;
}

int end_mmap_info(MMAP_INFO *info) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("mmap info: %p", info));
  int ret = 0;
  if (info->write_pos > info->sync_pos) {
    ret = my_msync(info->file, info->sync_pos,
                   static_cast<size_t>(info->write_pos - info->sync_pos),
                   MS_SYNC);
    info->sync_pos = info->write_pos;
  }

  return ret || my_munmap(info->addr, info->mmap_length);
}
