/*****************************************************************************

Copyright (c) 2014, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file ut/ut0new.cc
Instrumented memory allocator.

Created May 26, 2014 Vasil Dimov
*******************************************************/

#include "univ.i"

#include "ut0new.h"

/** Maximum number of retries to allocate memory. */
const size_t	alloc_max_retries = 60;

/** Keys for registering allocations with performance schema.
Keep this list alphabetically sorted. */
PSI_memory_key	mem_key_ahi;
PSI_memory_key	mem_key_buf_buf_pool;
PSI_memory_key	mem_key_dict_stats_bg_recalc_pool_t;
PSI_memory_key	mem_key_dict_stats_index_map_t;
PSI_memory_key	mem_key_dict_stats_n_diff_on_level;
PSI_memory_key	mem_key_other;
PSI_memory_key	mem_key_row_log_buf;
PSI_memory_key	mem_key_row_merge_sort;
PSI_memory_key	mem_key_std;
PSI_memory_key	mem_key_trx_sys_t_rw_trx_ids;
PSI_memory_key	mem_key_partitioning;

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
static PSI_memory_info	pfs_info[] = {
	{&mem_key_ahi, "adaptive hash index", 0},
	{&mem_key_buf_buf_pool, "buf_buf_pool", 0},
	{&mem_key_dict_stats_bg_recalc_pool_t, "dict_stats_bg_recalc_pool_t", 0},
	{&mem_key_dict_stats_index_map_t, "dict_stats_index_map_t", 0},
	{&mem_key_dict_stats_n_diff_on_level, "dict_stats_n_diff_on_level", 0},
	{&mem_key_other, "other", 0},
	{&mem_key_row_log_buf, "row_log_buf", 0},
	{&mem_key_row_merge_sort, "row_merge_sort", 0},
	{&mem_key_std, "std", 0},
	{&mem_key_trx_sys_t_rw_trx_ids, "trx_sys_t::rw_trx_ids", 0},
	{&mem_key_partitioning, "partitioning", 0},
};

/** Map used for default performance schema keys, based on file name of the
caller. The key is the file name of the caller and the value is a pointer
to a PSI_memory_key variable to be passed to performance schema methods.
We use ut_strcmp_functor because by default std::map will compare the pointers
themselves (cont char*) and not do strcmp(). */
typedef std::map<const char*, PSI_memory_key*, ut_strcmp_functor>
	mem_keys_auto_t;

/** Map of filename/pfskey, used for tracing allocations that have not
provided a manually created pfs key. This map is only ever modified (bulk
insert) at startup in a single-threaded environment by ut_new_boot().
Later it is only read (only std::map::find() is called) from multithreaded
environment, thus it is not protected by any latch. */
static mem_keys_auto_t	mem_keys_auto;

#endif /* UNIV_PFS_MEMORY */

/** Setup the internal objects needed for UT_NEW() to operate.
This must be called before the first call to UT_NEW(). */
void
ut_new_boot()
{
#ifdef UNIV_PFS_MEMORY
	static const char*	auto_event_names[] = {
		/* Keep this list alphabetically sorted. */
		"api0api",
		"btr0btr",
		"btr0bulk",
		"btr0cur",
		"btr0pcur",
		"btr0sea",
		"buf0buf",
		"buf0dblwr",
		"buf0dump",
		"buf0flu",
		"buf0lru",
		"dict0dict",
		"dict0mem",
		"dict0stats",
		"dict0stats_bg",
		"eval0eval",
		"fil0fil",
		"fsp0file",
		"fsp0space",
		"fsp0sysspace",
		"fts0ast",
		"fts0config",
		"fts0fts",
		"fts0opt",
		"fts0pars",
		"fts0que",
		"fts0sql",
		"gis0sea",
		"ha0ha",
		"ha_innodb",
		"handler0alter",
		"hash0hash",
		"i_s",
		"ibuf0ibuf",
		"lexyy",
		"lock0lock",
		"log0log",
		"log0recv",
		"mem0mem",
		"os0event",
		"os0file",
		"page0cur",
		"page0zip",
		"pars0lex",
		"read0read",
		"rem0rec",
		"row0ftsort",
		"row0import",
		"row0log",
		"row0merge",
		"row0mysql",
		"row0sel",
		"row0trunc",
		"srv0conc",
		"srv0srv",
		"srv0start",
		"sync0arr",
		"sync0debug",
		"sync0rw",
		"sync0types",
		"trx0i_s",
		"trx0purge",
		"trx0roll",
		"trx0rseg",
		"trx0sys",
		"trx0trx",
		"trx0undo",
		"usr0sess",
		"ut0list",
		"ut0mem",
		"ut0mutex",
		"ut0pool",
		"ut0rbt",
		"ut0wqueue",
	};
	static const size_t	n_auto = UT_ARR_SIZE(auto_event_names);
	static PSI_memory_key	auto_event_keys[n_auto];
	static PSI_memory_info	pfs_info_auto[n_auto];

	for (size_t i = 0; i < n_auto; i++) {

		const std::pair<mem_keys_auto_t::iterator, bool>	ret
			MY_ATTRIBUTE((unused))
			= mem_keys_auto.insert(
			mem_keys_auto_t::value_type(auto_event_names[i],
						    &auto_event_keys[i]));

		/* ret.second is true if new element has been inserted */
		ut_a(ret.second);

		/* e.g. "btr0btr" */
		pfs_info_auto[i].m_name = auto_event_names[i];

		/* a pointer to the pfs key */
		pfs_info_auto[i].m_key = &auto_event_keys[i];

		pfs_info_auto[i].m_flags = 0;
	}

	PSI_MEMORY_CALL(register_memory)("innodb",
					 pfs_info,
					 UT_ARR_SIZE(pfs_info));
	PSI_MEMORY_CALL(register_memory)("innodb",
					 pfs_info_auto,
					 n_auto);
#endif /* UNIV_PFS_MEMORY */
}

#ifdef UNIV_PFS_MEMORY

/** Retrieve a memory key (registered with PFS), given a portion of the file
name of the caller.
@param[in]	file	portion of the filename - basename without an extension
@return registered memory key or PSI_NOT_INSTRUMENTED if not found */
PSI_memory_key
ut_new_get_key_by_file(
	const char*	file)
{
	mem_keys_auto_t::const_iterator	el = mem_keys_auto.find(file);

	if (el != mem_keys_auto.end()) {
		return(*(el->second));
	}

	return(PSI_NOT_INSTRUMENTED);
}

#endif /* UNIV_PFS_MEMORY */
