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

#include "NdbMem.h"

extern "C"
void NdbMem_Create()
{
}
extern "C"
void NdbMem_Destroy()
{
}

extern "C"
void* NdbMem_Allocate(size_t size)
{
  return new char[size];
}

extern "C"
void* NdbMem_AllocateAlign(size_t size, size_t alignment)
{
  return NdbMem_Allocate(size);
}

extern "C"
void NdbMem_Free(void* ptr)
{
  delete [] (char *)(ptr);
}

int NdbMem_MemLockAll(){
  return -1;
}

int NdbMem_MemUnlockAll(){
  return -1;
}

