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


#if defined NDB_OSE
#include <ose.h>
#include <mms.sig>
#include <mms_err.h>
#include <string.h>
#include <stdio.h>
#include <NdbOut.hpp>

//  Page size for mp750 is 4096 bytes.
#define PAGE_SIZE 4096

/**
 * NOTE: To use NdbMem from a OSE system ose_mms has to be defined 
 * as a "Required External Process"(see OSE Kernel User's Guide/R1.1(p. 148)),
 * like this in osemain.con:
 * EXT_PROC(ose_mms, ose_mms, 50000)
 * This will create a global variable ose_mms_ that is used from here.
 */

union SIGNAL
{
  SIGSELECT                       sigNo;
  struct MmsAllocateRegionRequest mmsAllocateRegionRequest;
  struct MmsAllocateRegionReply   mmsAllocateRegionReply;
  struct MmsFreeRegionRequest     mmsFreeRegionRequest;
  struct MmsFreeRegionReply       mmsFreeRegionReply;
}; /* union SIGNAL */

extern PROCESS ose_mms_;

void NdbMem_Create(void)
{
  /* Do nothing */
  return;
}

void NdbMem_Destroy(void)
{
  /* Do nothing */
  return;
}

void* NdbMem_Allocate(size_t size)
{
  static SIGSELECT   allocate_sig[]  = {1,MMS_ALLOCATE_REGION_REPLY};
  union SIGNAL           *sig;
  U32 allocatedAdress;

  assert(size > 0);

  // Only allowed to allocate multiples of the page size.
  if(size % PAGE_SIZE != 0) {
    size += PAGE_SIZE - size%PAGE_SIZE;
  }

  /* Allocate a new region in the callers memory segment. */
  sig = alloc(sizeof(struct MmsAllocateRegionRequest),
              MMS_ALLOCATE_REGION_REQUEST);
  /* -1: The callers domain is used */ 
  sig->mmsAllocateRegionRequest.domain = (MemoryDomain)-1;
  sig->mmsAllocateRegionRequest.useAddr = False;
  sig->mmsAllocateRegionRequest.size = size;
  sig->mmsAllocateRegionRequest.access = SuperRW_UserRW;
  sig->mmsAllocateRegionRequest.resident = False;
  sig->mmsAllocateRegionRequest.memory = CodeData;
  sig->mmsAllocateRegionRequest.cache = CopyBack;
  strcpy(sig->mmsAllocateRegionRequest.name, "NDB_DATA");
  send(&sig, ose_mms_);
  sig = receive(allocate_sig);
    
  if (sig->mmsAllocateRegionReply.status != MMS_SUCCESS){
    /* Memory allocation failed, make sure this function returns NULL */
    allocatedAdress = NULL;
  }
  else{
    allocatedAdress = sig->mmsAllocateRegionReply.address;
  }
  free_buf(&sig);
  return (void*)allocatedAdress;
}

void* NdbMem_AllocateAlign(size_t size, size_t alignment)
{
  return NdbMem_Allocate(size);
}


void NdbMem_Free(void* ptr)
{
  static SIGSELECT   free_sig[]  = {1,MMS_FREE_REGION_REPLY};
  union SIGNAL           *sig;

  /* Free a region in the callers domain. */
  sig = alloc(sizeof(struct MmsFreeRegionRequest),
              MMS_FREE_REGION_REQUEST);
  sig->mmsFreeRegionRequest.address = (U32)ptr;
  send(&sig, ose_mms_);
  sig = receive(free_sig);
    
  if (sig->mmsFreeRegionReply.status != MMS_SUCCESS){
    ndbout_c("The MMS Region could not be deallocated.\r\n");
    error(sig->mmsFreeRegionReply.status);
  };
  free_buf(&sig);
}

int NdbMem_MemLockAll(){
  return -1;
}

int NdbMem_MemUnlockAll(){
  return -1;
}

#else
#include <stdlib.h>


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
  return -1;
}

int NdbMem_MemUnlockAll(){
  return -1;
}

#endif
