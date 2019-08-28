/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_MEM_H
#define NDB_MEM_H

#include <ndb_global.h>

#ifdef	__cplusplus
extern "C" {
#endif


/**
 * NdbMem_Create 
 * Create and initalise internal data structures for Ndb
 */
void NdbMem_Create(void);


/**
 * NdbMem_Destroy
 * Destroy all memory allocated by NdbMem
 */
void NdbMem_Destroy(void);

/**
 * NdbMem_Allocate
 * Allocate size of memory
 * @parameter size - size in bytes of memory to allocate
 * @returns - pointer to memory if succesful otherwise NULL
 */
void* NdbMem_Allocate(size_t size);

/**
 * NdbMem_AllocateAlign
 * Allocate size of memory
 * @parameter size - size in bytes of memory to allocate
 * @paramter alignment - byte boundary to align the data at
 * @returns - pointer to memory if succesful otherwise NULL
 */
void* NdbMem_AllocateAlign(size_t size, size_t alignment);


/** 
 * NdbMem_Free
 *  Free the memory that ptr points to 
 *  @parameter ptr - pointer to the memory to free
 */
void NdbMem_Free(void* ptr);

/**
 * NdbMem_MemLockAll
 *   Locks virtual memory in main memory
 */
int NdbMem_MemLockAll(int);

/**
 * NdbMem_MemUnlockAll
 *   Unlocks virtual memory
 */
int NdbMem_MemUnlockAll(void);

/**
 * Memlock region
 */
int NdbMem_MemLock(const void * ptr, size_t len);

#ifdef	__cplusplus
}
#endif

#endif
