/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All rights reserved.

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
@file include/buf0types.h
The database buffer pool global types for the directory

Created 11/17/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0types_h
#define buf0types_h

#include "dict0types.h" /* index_id_t */
#include "os0event.h"
#include "ut0lock_free_hash.h"
#include "ut0mutex.h"
#include "ut0ut.h"

/** Magic value to use instead of checksums when they are disabled */
#define BUF_NO_CHECKSUM_MAGIC 0xDEADBEEFUL

/** Buffer page (uncompressed or compressed) */
class buf_page_t;
/** Buffer block for which an uncompressed page exists */
struct buf_block_t;
/** Buffer pool chunk comprising buf_block_t */
struct buf_chunk_t;
/** Buffer pool comprising buf_chunk_t */
struct buf_pool_t;
/** Buffer pool statistics struct */
struct buf_pool_stat_t;
/** Buffer pool buddy statistics struct */
struct buf_buddy_stat_t;
/** Doublewrite memory struct */
struct buf_dblwr_t;
/** Flush observer for bulk create index */
class FlushObserver;

/** A buffer frame. @see page_t */
typedef	byte	buf_frame_t;

/** Flags for flush types */
enum buf_flush_t {
	BUF_FLUSH_LRU = 0,		/*!< flush via the LRU list */
	BUF_FLUSH_LIST,			/*!< flush via the flush list
					of dirty blocks */
	BUF_FLUSH_SINGLE_PAGE,		/*!< flush via the LRU list
					but only a single page */
	BUF_FLUSH_N_TYPES		/*!< index of last element + 1  */
};

/** Algorithm to remove the pages for a tablespace from the buffer pool.
See buf_LRU_flush_or_remove_pages(). */
enum buf_remove_t {
	BUF_REMOVE_ALL_NO_WRITE,	/*!< Remove all pages from the buffer
					pool, don't write or sync to disk */
	BUF_REMOVE_FLUSH_NO_WRITE,	/*!< Remove only, from the flush list,
					don't write or sync to disk */
	BUF_REMOVE_FLUSH_WRITE		/*!< Flush dirty pages to disk only
					don't remove from the buffer pool */
};

/** Flags for io_fix types */
enum buf_io_fix {
	BUF_IO_NONE = 0,		/**< no pending I/O */
	BUF_IO_READ,			/**< read pending */
	BUF_IO_WRITE,			/**< write pending */
	BUF_IO_PIN			/**< disallow relocation of
					block and its removal of from
					the flush_list */
};

/** Alternatives for srv_checksum_algorithm, which can be changed by
setting innodb_checksum_algorithm */
enum srv_checksum_algorithm_t {
	SRV_CHECKSUM_ALGORITHM_CRC32,		/*!< Write crc32, allow crc32,
						innodb or none when reading */
	SRV_CHECKSUM_ALGORITHM_STRICT_CRC32,	/*!< Write crc32, allow crc32
						when reading */
	SRV_CHECKSUM_ALGORITHM_INNODB,		/*!< Write innodb, allow crc32,
						innodb or none when reading */
	SRV_CHECKSUM_ALGORITHM_STRICT_INNODB,	/*!< Write innodb, allow
						innodb when reading */
	SRV_CHECKSUM_ALGORITHM_NONE,		/*!< Write none, allow crc32,
						innodb or none when reading */
	SRV_CHECKSUM_ALGORITHM_STRICT_NONE	/*!< Write none, allow none
						when reading */
};

inline
bool
is_checksum_strict(srv_checksum_algorithm_t algo)
{
	return(algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32
	       || algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB
	       || algo == SRV_CHECKSUM_ALGORITHM_STRICT_NONE);
}

inline
bool
is_checksum_strict(ulint algo)
{
	return(algo == SRV_CHECKSUM_ALGORITHM_STRICT_CRC32
	       || algo == SRV_CHECKSUM_ALGORITHM_STRICT_INNODB
	       || algo == SRV_CHECKSUM_ALGORITHM_STRICT_NONE);
}

/** Parameters of binary buddy system for compressed pages (buf0buddy.h) */
/* @{ */
/** Zip shift value for the smallest page size */
#define BUF_BUDDY_LOW_SHIFT	UNIV_ZIP_SIZE_SHIFT_MIN

/** Smallest buddy page size */
#define BUF_BUDDY_LOW		(1U << BUF_BUDDY_LOW_SHIFT)

/** Actual number of buddy sizes based on current page size */
#define BUF_BUDDY_SIZES		(UNIV_PAGE_SIZE_SHIFT - BUF_BUDDY_LOW_SHIFT)

/** Maximum number of buddy sizes based on the max page size */
#define BUF_BUDDY_SIZES_MAX	(UNIV_PAGE_SIZE_SHIFT_MAX	\
				- BUF_BUDDY_LOW_SHIFT)

/** twice the maximum block size of the buddy system;
the underlying memory is aligned by this amount:
this must be equal to UNIV_PAGE_SIZE */
#define BUF_BUDDY_HIGH	(BUF_BUDDY_LOW << BUF_BUDDY_SIZES)
/* @} */

typedef ib_mutex_t BPageMutex;
typedef ib_mutex_t BufPoolMutex;
typedef ib_mutex_t FlushListMutex;

/** Page identifier. */
class page_id_t {
public:

	/** Constructor from (space, page_no).
	@param[in]	space	tablespace id
	@param[in]	page_no	page number */
	page_id_t(ulint space, ulint page_no)
		:
		m_space(static_cast<ib_uint32_t>(space)),
		m_page_no(static_cast<ib_uint32_t>(page_no)),
		m_fold(ULINT_UNDEFINED)
	{
		ut_ad(space <= 0xFFFFFFFFU);
		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Retrieve the tablespace id.
	@return tablespace id */
	inline ib_uint32_t space() const
	{
		return(m_space);
	}

	/** Retrieve the page number.
	@return page number */
	inline ib_uint32_t page_no() const
	{
		return(m_page_no);
	}

	/** Retrieve the fold value.
	@return fold value */
	inline ulint fold() const
	{
		/* Initialize m_fold if it has not been initialized yet. */
		if (m_fold == ULINT_UNDEFINED) {
			m_fold = (m_space << 20) + m_space + m_page_no;
			ut_ad(m_fold != ULINT_UNDEFINED);
		}

		return(m_fold);
	}

	/** Copy the values from a given page_id_t object.
	@param[in]	src	page id object whose values to fetch */
	inline void copy_from(const page_id_t& src)
	{
		m_space = src.space();
		m_page_no = src.page_no();
		m_fold = src.fold();
	}

	/** Reset the values from a (space, page_no).
	@param[in]	space	tablespace id
	@param[in]	page_no	page number */
	inline void reset(ulint space, ulint page_no)
	{
		m_space = static_cast<ib_uint32_t>(space);
		m_page_no = static_cast<ib_uint32_t>(page_no);
		m_fold = ULINT_UNDEFINED;

		ut_ad(space <= 0xFFFFFFFFU);
		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Reset the page number only.
	@param[in]	page_no	page number */
	inline void set_page_no(ulint page_no)
	{
		m_page_no = static_cast<ib_uint32_t>(page_no);
		m_fold = ULINT_UNDEFINED;

		ut_ad(page_no <= 0xFFFFFFFFU);
	}

	/** Check if a given page_id_t object is equal to the current one.
	@param[in]	a	page_id_t object to compare
	@return true if equal */
	inline bool equals_to(const page_id_t& a) const
	{
		return(a.space() == m_space && a.page_no() == m_page_no);
	}

private:

	/** Tablespace id. */
	ib_uint32_t	m_space;

	/** Page number. */
	ib_uint32_t	m_page_no;

	/** A fold value derived from m_space and m_page_no,
	used in hashing. */
	mutable ulint	m_fold;

	/* Disable implicit copying. */
	void operator=(const page_id_t&);

	/** Declare the overloaded global operator<< as a friend of this
	class. Refer to the global declaration for further details.  Print
	the given page_id_t object.
	@param[in,out]	out	the output stream
	@param[in]	page_id	the page_id_t object to be printed
	@return the output stream */
        friend
        std::ostream&
        operator<<(
                std::ostream&           out,
                const page_id_t&        page_id);
};

/** Print the given page_id_t object.
@param[in,out]	out	the output stream
@param[in]	page_id	the page_id_t object to be printed
@return the output stream */
std::ostream&
operator<<(
	std::ostream&		out,
	const page_id_t&	page_id);

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
	@param[in]	index_id	id of the index whose count to increment
	*/
	void
	inc(
		space_index_t	index_id)
	{
		ut_ad(sizeof(space_index_t) >= sizeof(uintptr_t));

		m_store->inc(static_cast<uintptr_t>(index_id));
	}

	/** Decrement the number of pages for a given index with 1.
	@param[in]	index_id	id of the index whose count to decrement
	*/
	void
	dec(
		space_index_t	index_id)
	{
		m_store->dec(static_cast<uintptr_t>(index_id));
	}

	/** Get the number of pages in the buffer pool for a given index.
	@param[in]	index_id	id of the index whose pages to peek
	@return number of pages */
	uintptr_t
	get(
		space_index_t	index_id)
	{
		const uintptr_t	ret
			= m_store->get(static_cast<uintptr_t>(index_id));

		if (ret == ut_lock_free_hash_t::NOT_FOUND) {
			/* If the index is not found in this structure,
			then 0 of its pages are in the buffer pool. */
			return(0);
		}

		return(ret);
	}

private:
	/** (key, value) storage. */
	ut_lock_free_hash_t*	m_store;
};

/** Container for how much pages from each index are contained in the buffer
pool(s). */
extern buf_stat_per_index_t*	buf_stat_per_index;

#endif /* buf0types.h */
