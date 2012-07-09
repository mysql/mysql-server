/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/buf0types.h
The database buffer pool global types for the directory

Created 11/17/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0types_h
#define buf0types_h

#include "page0types.h"

/** Buffer page (uncompressed or compressed) */
typedef	struct buf_page_struct		buf_page_t;
/** Buffer block for which an uncompressed page exists */
typedef	struct buf_block_struct		buf_block_t;
/** Buffer pool chunk comprising buf_block_t */
typedef struct buf_chunk_struct		buf_chunk_t;
/** Buffer pool comprising buf_chunk_t */
typedef	struct buf_pool_struct		buf_pool_t;
/** Buffer pool statistics struct */
typedef	struct buf_pool_stat_struct	buf_pool_stat_t;
/** Buffer pool buddy statistics struct */
typedef	struct buf_buddy_stat_struct	buf_buddy_stat_t;

/** A buffer frame. @see page_t */
typedef	byte	buf_frame_t;

/** Flags for flush types */
enum buf_flush {
	BUF_FLUSH_LRU = 0,		/*!< flush via the LRU list */
	BUF_FLUSH_SINGLE_PAGE,		/*!< flush a single page */
	BUF_FLUSH_LIST,			/*!< flush via the flush list
					of dirty blocks */
	BUF_FLUSH_N_TYPES		/*!< index of last element + 1  */
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

/** Algorithm to remove the pages for a tablespace from the buffer pool.
@See buf_LRU_flush_or_remove_pages(). */
enum buf_remove_t {
	BUF_REMOVE_ALL_NO_WRITE,	/*!< Remove all pages from the buffer
					pool, don't write or sync to disk */
	BUF_REMOVE_FLUSH_NO_WRITE	/*!< Remove only, from the flush list,
					don't write or sync to disk */
};

/** Parameters of binary buddy system for compressed pages (buf0buddy.h) */
/* @{ */
#define BUF_BUDDY_LOW_SHIFT	PAGE_ZIP_MIN_SIZE_SHIFT

#define BUF_BUDDY_LOW		(1 << BUF_BUDDY_LOW_SHIFT)

#define BUF_BUDDY_SIZES		(UNIV_PAGE_SIZE_SHIFT - BUF_BUDDY_LOW_SHIFT)
					/*!< number of buddy sizes */

/** twice the maximum block size of the buddy system;
the underlying memory is aligned by this amount:
this must be equal to UNIV_PAGE_SIZE */
#define BUF_BUDDY_HIGH	(BUF_BUDDY_LOW << BUF_BUDDY_SIZES)
/* @} */

#endif

