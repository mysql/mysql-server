/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>

#include <NdbMem.h>

void NdbMem_Create()
{
  /* Do nothing */
  return;
}

void NdbMem_Destroy()
{
  /* Do nothing */
  return;
}

void* NdbMem_Allocate(size_t size)
{
  assert(size > 0);
  return (void*)malloc(size);
}

void* NdbMem_AllocateAlign(size_t size, size_t alignment)
{
  /*
    return (void*)memalign(alignment, size);
    TEMP fix
  */
  return (void*)malloc(size);
}


void NdbMem_Free(void* ptr)
{
  free(ptr);
}

 
int NdbMem_MemLockAll(){
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return mlockall(MCL_CURRENT);
#else
  return -1;
#endif
}

int NdbMem_MemUnlockAll(){
#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT)
  return munlockall();
#else
  return -1;
#endif
}

