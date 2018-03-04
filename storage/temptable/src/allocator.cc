/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file storage/temptable/src/allocator.cc
TempTable custom allocator implementation. */

#include <atomic>  /* std::atomic */
#include <cstddef> /* size_t */
#include <cstdint> /* uint8_t */

#include "storage/temptable/include/temptable/allocator.h"

namespace temptable {

#ifdef TEMPTABLE_PFS_MEMORY
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
PSI_memory_key mem_key_logical;
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */

PSI_memory_key mem_key_physical_disk;

PSI_memory_key mem_key_physical_ram;

PSI_memory_info pfs_info[] = {
#ifdef TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL
    {&mem_key_logical, "logical", 0, 0, PSI_DOCUMENT_ME},
#endif /* TEMPTABLE_PFS_MEMORY_COUNT_LOGICAL */
    {&mem_key_physical_disk, "physical_disk", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_physical_ram, "physical_ram", 0, 0, PSI_DOCUMENT_ME},
};
#endif /* TEMPTABLE_PFS_MEMORY */

#ifdef TEMPTABLE_PFS_MEMORY
const size_t pfs_info_num_elements = sizeof(pfs_info) / sizeof(pfs_info[0]);
#endif /* TEMPTABLE_PFS_MEMORY */

thread_local uint8_t* shared_block = nullptr;

std::atomic<size_t> bytes_allocated_in_ram(0);

#ifdef TEMPTABLE_USE_LINUX_NUMA
bool linux_numa_available = false;
#endif /* TEMPTABLE_USE_LINUX_NUMA */

} /* namespace temptable */
