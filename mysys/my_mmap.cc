/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

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
#include "my_inttypes.h"
#include "my_sys.h"

#ifdef HAVE_SYS_MMAN_H

/*
  system msync() only syncs mmap'ed area to fs cache.
  fsync() is required to really sync to disc
*/
int my_msync(int fd, void *addr, size_t len, int flags)
{
  msync(static_cast<char*>(addr), len, flags);
  return my_sync(fd, MYF(0));
}

#elif defined(_WIN32)

static SECURITY_ATTRIBUTES mmap_security_attributes=
  {sizeof(SECURITY_ATTRIBUTES), 0, TRUE};

void *my_mmap(void *addr, size_t len, int prot,
               int flags, File fd, my_off_t offset)
{
  HANDLE hFileMap;
  LPVOID ptr;
  HANDLE hFile= (HANDLE)my_get_osfhandle(fd);
  DBUG_ENTER("my_mmap");
  DBUG_PRINT("mysys", ("map fd: %d", fd));

  if (hFile == INVALID_HANDLE_VALUE)
    DBUG_RETURN(MAP_FAILED);

  hFileMap=CreateFileMapping(hFile, &mmap_security_attributes,
                             PAGE_READWRITE, 0, (DWORD) len, NULL);
  if (hFileMap == 0)
    DBUG_RETURN(MAP_FAILED);

  ptr=MapViewOfFile(hFileMap,
                    prot & PROT_WRITE ? FILE_MAP_WRITE : FILE_MAP_READ,
                    (DWORD)(offset >> 32), (DWORD)offset, len);

  /*
    MSDN explicitly states that it's possible to close File Mapping Object
    even when a view is not unmapped - then the object will be held open
    implicitly until unmap, as every view stores internally a handler of
    a corresponding File Mapping Object
   */
  CloseHandle(hFileMap);

  if (ptr)
  {
    DBUG_PRINT("mysys", ("mapped addr: %p", ptr));
    DBUG_RETURN(ptr);
  }

  DBUG_RETURN(MAP_FAILED);
}

int my_munmap(void *addr, size_t len)
{
  DBUG_ENTER("my_munmap");
  DBUG_PRINT("mysys", ("unmap addr: %p", addr));
  DBUG_RETURN(UnmapViewOfFile(addr) ? 0 : -1);
}

int my_msync(int fd, void *addr, size_t len, int flags)
{
  return FlushViewOfFile(addr, len) ? 0 : -1;
}

#else
#error "no mmap!"
#endif

