/* Copyright (c) 2019, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/block.cc */

#include <assert.h>

#include "my_psi_config.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/psi_memory.h"

#ifdef HAVE_PSI_MEMORY_INTERFACE
#define TEMPTABLE_PFS_MEMORY

/* Enabling this causes ~ 4% performance drop in sysbench distinct ranges. */
//#define TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
#endif /* HAVE_PSI_MEMORY_INTERFACE */

namespace temptable {

#ifdef TEMPTABLE_PFS_MEMORY
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
/** PFS key to account logical memory allocations and deallocations. Logical
 * is any request for new memory that arrives to the allocator. */
PSI_memory_key mem_key_logical;
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */

/** PFS key to account physical allocations and deallocations from disk. After
 * we have allocated more than `temptable_max_ram` we start taking memory from
 * the OS disk, using mmap()'ed files. */
PSI_memory_key mem_key_physical_disk;

/** PFS key to account physical allocations and deallocations from RAM. Before
 * we have allocated more than `temptable_max_ram` we take memory from the OS
 * RAM, using e.g. malloc(). */
PSI_memory_key mem_key_physical_ram;

/** Array of PFS keys. */
PSI_memory_info pfs_info[] = {
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
    {&mem_key_logical, "logical", 0, 0, PSI_DOCUMENT_ME},
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */
    {&mem_key_physical_disk, "physical_disk", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_physical_ram, "physical_ram", PSI_FLAG_MEM_COLLECT, 0,
     PSI_DOCUMENT_ME},
};

/** Number of elements inside `pfs_info[]`. */
const size_t pfs_info_num_elements = sizeof(pfs_info) / sizeof(pfs_info[0]);
#endif /* TEMPTABLE_PFS_MEMORY */

void Block_PSI_init() {
#ifdef TEMPTABLE_PFS_MEMORY
  PSI_MEMORY_CALL(register_memory)
  ("temptable", pfs_info, pfs_info_num_elements);
#endif /* TEMPTABLE_PFS_MEMORY */
}

void Block_PSI_track_logical_allocation(size_t size) {
  (void)size;
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
  PSI_thread *owner_thread;

#ifndef NDEBUG
  PSI_memory_key key =
#endif /* NDEBUG */
      PSI_MEMORY_CALL(memory_alloc)(mem_key_logical, size, &owner_thread);

  assert(key == mem_key_logical || key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */
}

void Block_PSI_track_logical_deallocation(size_t size) {
  (void)size;
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
  PSI_MEMORY_CALL(memory_free)
  (mem_key_logical, size, nullptr);
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */
}

void Block_PSI_track_physical_ram_allocation(void *ptr [[maybe_unused]],
                                             size_t size [[maybe_unused]]) {
#ifdef TEMPTABLE_PFS_MEMORY
  my_memory_header *mh = (my_memory_header *)ptr;
  mh->m_magic = PSI_MEMORY_MAGIC;
  mh->m_size = size;
  mh->m_key = PSI_MEMORY_CALL(memory_alloc)(
      mem_key_physical_ram, size + PSI_HEADER_SIZE, &mh->m_owner);
  assert(PSI_REAL_MEM_KEY(mh->m_key) == mem_key_physical_ram ||
         mh->m_key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY */
}

void Block_PSI_track_physical_ram_deallocation(uint8_t *ptr [[maybe_unused]]) {
#ifdef TEMPTABLE_PFS_MEMORY
  my_memory_header *mh = (my_memory_header *)ptr;
  PSI_MEMORY_CALL(memory_free)
  (mh->m_key, mh->m_size + PSI_HEADER_SIZE, mh->m_owner);
  assert(PSI_REAL_MEM_KEY(mh->m_key) == mem_key_physical_ram ||
         mh->m_key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY */
}

void Block_PSI_track_physical_disk_allocation(void *ptr [[maybe_unused]],
                                              size_t size [[maybe_unused]]) {
#ifdef TEMPTABLE_PFS_MEMORY
  my_memory_header *mh = (my_memory_header *)ptr;
  mh->m_magic = PSI_MEMORY_MAGIC;
  mh->m_size = size;
  mh->m_key = PSI_MEMORY_CALL(memory_alloc)(
      mem_key_physical_disk, size + PSI_HEADER_SIZE, &mh->m_owner);
  assert(PSI_REAL_MEM_KEY(mh->m_key) == mem_key_physical_disk ||
         mh->m_key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY */
}

void Block_PSI_track_physical_disk_deallocation(uint8_t *ptr [[maybe_unused]]) {
#ifdef TEMPTABLE_PFS_MEMORY
  my_memory_header *mh = (my_memory_header *)ptr;
  PSI_MEMORY_CALL(memory_free)
  (mh->m_key, mh->m_size + PSI_HEADER_SIZE, mh->m_owner);

  assert(PSI_REAL_MEM_KEY(mh->m_key) == mem_key_physical_disk ||
         mh->m_key == PSI_NOT_INSTRUMENTED);
#endif /* TEMPTABLE_PFS_MEMORY */
}

} /* namespace temptable */
