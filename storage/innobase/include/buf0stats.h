/*****************************************************************************

Copyright (c) 2015, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/buf0stats.h
Buffer pool stats

Created May 22, 2015 Vasil Dimov
*******************************************************/

#ifndef buf0stats_h
#define buf0stats_h

#include "univ.i"

#include "dict0types.h" /* index_id_t, DICT_IBUF_ID_MIN */
#include "fsp0sysspace.h" /* srv_tmp_space */
#include "ibuf0ibuf.h" /* IBUF_SPACE_ID */
#include "ut0new.h" /* UT_NEW(), UT_DELETE() */
#include "ut0lock_free_hash.h" /* ut_lock_free_hash_t */

/** Per index buffer pool statistics - contains how much pages for each index
are cached in the buffer pool(s). This is a key,value store where the key is
the index id and the value is the number of pages in the buffer pool that
belong to this index. */
class buf_stat_per_index_t {
public:
	/** Constructor. */
	buf_stat_per_index_t()
	{
		m_store = UT_NEW(ut_lock_free_hash_t(1024),
				 mem_key_buf_stat_per_index_t);
	}

	/** Destructor. */
	~buf_stat_per_index_t()
	{
		UT_DELETE(m_store);
	}

	/** Increment the number of pages for a given index with 1.
	@param[in]	id	id of the index whose count to increment */
	void
	inc(
		const index_id_t&	id)
	{
		if (should_skip(id)) {
			return;
		}

		m_store->inc(conv_index_id_to_int(id));
	}

	/** Decrement the number of pages for a given index with 1.
	@param[in]	id	id of the index whose count to decrement */
	void
	dec(
		const index_id_t&	id)
	{
		if (should_skip(id)) {
			return;
		}

		m_store->dec(conv_index_id_to_int(id));
	}

	/** Get the number of pages in the buffer pool for a given index.
	@param[in]	id	id of the index whose pages to peek
	@return number of pages */
	uintptr_t
	get(
		const index_id_t&	id)
	{
		if (should_skip(id)) {
			return(0);
		}

		const int64_t	ret = m_store->get(conv_index_id_to_int(id));

		if (ret == ut_lock_free_hash_t::NOT_FOUND) {
			/* If the index is not found in this structure,
			then 0 of its pages are in the buffer pool. */
			return(0);
		}

		return(ret);
	}

	/** Delete the stats for a given index.
	@param[in]	id	id of the index whose stats to delete */
	void
	drop(
		const index_id_t&	id)
	{
		if (should_skip(id)) {
			return;
		}

		m_store->del(conv_index_id_to_int(id));
	}

private:
	/** Convert an index_id to a 64 bit integer.
	@param[in]	id	index_id to convert
	@return a 64 bit integer */
	uint64_t
	conv_index_id_to_int(
		const index_id_t&	id)
	{
		ut_ad((id.m_index_id & 0xFFFFFFFF00000000ULL) == 0);

		return(static_cast<uint64_t>(id.m_space_id) << 32
		       | id.m_index_id);
	}

	/** Assess if we should skip a page from accounting.
	@param[in]	id	index_id of the page
	@return true if it should not be accounted */
	bool
	should_skip(
		const index_id_t&	id)
	{
		const bool	is_ibuf
			= id.m_space_id == IBUF_SPACE_ID
			&& id.m_index_id == DICT_IBUF_ID_MIN + IBUF_SPACE_ID;

		const bool	is_temp
			= id.m_space_id == srv_tmp_space.space_id();

		return(is_ibuf || is_temp
		       || (id.m_index_id & 0xFFFFFFFF00000000ULL) != 0);
	}

	/** (key, value) storage. */
	ut_lock_free_hash_t*	m_store;
};

/** Container for how much pages from each index are contained in the buffer
pool(s). */
extern buf_stat_per_index_t*	buf_stat_per_index;

#endif /* buf0stats_h */
