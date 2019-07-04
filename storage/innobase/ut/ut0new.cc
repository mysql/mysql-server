/*****************************************************************************

Copyright (c) 2014, 2018, Oracle and/or its affiliates. All Rights Reserved.

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
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file ut/ut0new.cc
 Instrumented memory allocator.

 Created May 26, 2014 Vasil Dimov
 *******************************************************/

#include "univ.i"

#include "my_compiler.h"
#include "ut0new.h"

/** Maximum number of retries to allocate memory. */
const size_t alloc_max_retries = 60;

/** Keys for registering allocations with performance schema.
Keep this list alphabetically sorted. */
PSI_memory_key mem_key_ahi;
PSI_memory_key mem_key_archive;
PSI_memory_key mem_key_buf_buf_pool;
PSI_memory_key mem_key_buf_stat_per_index_t;
/** Memory key for clone */
PSI_memory_key mem_key_clone;
PSI_memory_key mem_key_dict_stats_bg_recalc_pool_t;
PSI_memory_key mem_key_dict_stats_index_map_t;
PSI_memory_key mem_key_dict_stats_n_diff_on_level;
PSI_memory_key mem_key_other;
PSI_memory_key mem_key_partitioning;
PSI_memory_key mem_key_row_log_buf;
PSI_memory_key mem_key_row_merge_sort;
PSI_memory_key mem_key_std;
PSI_memory_key mem_key_trx_sys_t_rw_trx_ids;
PSI_memory_key mem_key_undo_spaces;
PSI_memory_key mem_key_ut_lock_free_hash_t;
/* Please obey alphabetical order in the definitions above. */

#ifdef UNIV_PFS_MEMORY

/** Auxiliary array of performance schema 'PSI_memory_info'.
Each allocation appears in
performance_schema.memory_summary_global_by_event_name (and alike) in the form
of e.g. 'memory/innodb/NAME' where the last component NAME is picked from
the list below:
1. If key is specified, then the respective name is used
2. Without a specified key, allocations from inside std::* containers use
   mem_key_std
3. Without a specified key, allocations from outside std::* pick up the key
   based on the file name, and if file name is not found in the predefined list
   (in ut_new_boot()) then mem_key_other is used.
Keep this list alphabetically sorted. */
// TODO: check if keys other than buf_buf_pool should be also global
//       (should use PSI_FLAG_ONLY_GLOBAL_STAT)
static PSI_memory_info pfs_info[] = {
    {&mem_key_ahi, "adaptive hash index", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_archive, "log and page archiver", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_buf_buf_pool, "buf_buf_pool", PSI_FLAG_ONLY_GLOBAL_STAT, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_buf_stat_per_index_t, "buf_stat_per_index_t", 0, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_clone, "clone", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_dict_stats_bg_recalc_pool_t, "dict_stats_bg_recalc_pool_t", 0, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_dict_stats_index_map_t, "dict_stats_index_map_t", 0, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_dict_stats_n_diff_on_level, "dict_stats_n_diff_on_level", 0, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_other, "other", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_partitioning, "partitioning", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_row_log_buf, "row_log_buf", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_row_merge_sort, "row_merge_sort", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_std, "std", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_trx_sys_t_rw_trx_ids, "trx_sys_t::rw_trx_ids", 0, 0,
     PSI_DOCUMENT_ME},
    {&mem_key_undo_spaces, "undo::Tablespaces", 0, 0, PSI_DOCUMENT_ME},
    {&mem_key_ut_lock_free_hash_t, "ut_lock_free_hash_t", 0, 0,
     PSI_DOCUMENT_ME},
    /* Please obey alphabetical order in the definitions above. */
};

PSI_memory_key auto_event_keys[n_auto];
PSI_memory_info pfs_info_auto[n_auto];

#endif /* UNIV_PFS_MEMORY */

/** Setup the internal objects needed for UT_NEW() to operate.
This must be called before the first call to UT_NEW(). */
void ut_new_boot() {
#ifdef UNIV_PFS_MEMORY
  for (size_t i = 0; i < n_auto; i++) {
    /* e.g. "btr0btr" */
    pfs_info_auto[i].m_name = auto_event_names[i];

    /* a pointer to the pfs key */
    pfs_info_auto[i].m_key = &auto_event_keys[i];

    pfs_info_auto[i].m_flags = 0;
    pfs_info_auto[i].m_volatility = PSI_VOLATILITY_UNKNOWN;
    pfs_info_auto[i].m_documentation = PSI_DOCUMENT_ME;
  }

  PSI_MEMORY_CALL(register_memory)("innodb", pfs_info, UT_ARR_SIZE(pfs_info));
  PSI_MEMORY_CALL(register_memory)("innodb", pfs_info_auto, n_auto);
#endif /* UNIV_PFS_MEMORY */
}

void ut_new_boot_safe() {
  static bool ut_new_boot_called = false;

  if (!ut_new_boot_called) {
    ut_new_boot();
    ut_new_boot_called = true;
  }
}
