/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/fsp0fsp.h
File space management

Created 12/18/1995 Heikki Tuuri
*******************************************************/

#ifndef fsp0fsp_h
#define fsp0fsp_h

#include "univ.i"

#ifndef UNIV_INNOCHECKSUM

#include "fsp0space.h"
#include "fut0lst.h"
#include "mtr0mtr.h"
#include "page0types.h"
#include "rem0types.h"
#include "ut0byte.h"

#endif /* !UNIV_INNOCHECKSUM */
#include "fsp0types.h"

/* @defgroup Tablespace Header Constants (moved from fsp0fsp.c) @{ */

/** Offset of the space header within a file page */
#define FSP_HEADER_OFFSET	FIL_PAGE_DATA

/* The data structures in files are defined just as byte strings in C */
typedef	byte	fsp_header_t;
typedef	byte	xdes_t;

/*			SPACE HEADER
			============

File space header data structure: this data structure is contained in the
first page of a space. The space for this header is reserved in every extent
descriptor page, but used only in the first. */

/*-------------------------------------*/
#define FSP_SPACE_ID		0	/* space id */
#define FSP_NOT_USED		4	/* this field contained a value up to
					which we know that the modifications
					in the database have been flushed to
					the file space; not used now */
#define	FSP_SIZE		8	/* Current size of the space in
					pages */
#define	FSP_FREE_LIMIT		12	/* Minimum page number for which the
					free list has not been initialized:
					the pages >= this limit are, by
					definition, free; note that in a
					single-table tablespace where size
					< 64 pages, this number is 64, i.e.,
					we have initialized the space
					about the first extent, but have not
					physically allocated those pages to the
					file */
#define	FSP_SPACE_FLAGS		16	/* fsp_space_t.flags, similar to
					dict_table_t::flags */
#define	FSP_FRAG_N_USED		20	/* number of used pages in the
					FSP_FREE_FRAG list */
#define	FSP_FREE		24	/* list of free extents */
#define	FSP_FREE_FRAG		(24 + FLST_BASE_NODE_SIZE)
					/* list of partially free extents not
					belonging to any segment */
#define	FSP_FULL_FRAG		(24 + 2 * FLST_BASE_NODE_SIZE)
					/* list of full extents not belonging
					to any segment */
#define FSP_SEG_ID		(24 + 3 * FLST_BASE_NODE_SIZE)
					/* 8 bytes which give the first unused
					segment id */
#define FSP_SEG_INODES_FULL	(32 + 3 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where all the segment inode
					slots are reserved */
#define FSP_SEG_INODES_FREE	(32 + 4 * FLST_BASE_NODE_SIZE)
					/* list of pages containing segment
					headers, where not all the segment
					header slots are reserved */
/*-------------------------------------*/
/* File space header size */
#define	FSP_HEADER_SIZE		(32 + 5 * FLST_BASE_NODE_SIZE)

#define	FSP_FREE_ADD		4	/* this many free extents are added
					to the free list from above
					FSP_FREE_LIMIT at a time */
/* @} */

#ifndef UNIV_INNOCHECKSUM

/* @defgroup File Segment Inode Constants (moved from fsp0fsp.c) @{ */

/*			FILE SEGMENT INODE
			==================

Segment inode which is created for each segment in a tablespace. NOTE: in
purge we assume that a segment having only one currently used page can be
freed in a few steps, so that the freeing cannot fill the file buffer with
bufferfixed file pages. */

typedef	byte	fseg_inode_t;

#define FSEG_INODE_PAGE_NODE	FSEG_PAGE_DATA
					/* the list node for linking
					segment inode pages */

#define FSEG_ARR_OFFSET		(FSEG_PAGE_DATA + FLST_NODE_SIZE)
/*-------------------------------------*/
#define	FSEG_ID			0	/* 8 bytes of segment id: if this is 0,
					it means that the header is unused */
#define FSEG_NOT_FULL_N_USED	8
					/* number of used segment pages in
					the FSEG_NOT_FULL list */
#define	FSEG_FREE		12
					/* list of free extents of this
					segment */
#define	FSEG_NOT_FULL		(12 + FLST_BASE_NODE_SIZE)
					/* list of partially free extents */
#define	FSEG_FULL		(12 + 2 * FLST_BASE_NODE_SIZE)
					/* list of full extents */
#define	FSEG_MAGIC_N		(12 + 3 * FLST_BASE_NODE_SIZE)
					/* magic number used in debugging */
#define	FSEG_FRAG_ARR		(16 + 3 * FLST_BASE_NODE_SIZE)
					/* array of individual pages
					belonging to this segment in fsp
					fragment extent lists */
#define FSEG_FRAG_ARR_N_SLOTS	(FSP_EXTENT_SIZE / 2)
					/* number of slots in the array for
					the fragment pages */
#define	FSEG_FRAG_SLOT_SIZE	4	/* a fragment page slot contains its
					page number within space, FIL_NULL
					means that the slot is not in use */
/*-------------------------------------*/
#define FSEG_INODE_SIZE					\
	(16 + 3 * FLST_BASE_NODE_SIZE			\
	 + FSEG_FRAG_ARR_N_SLOTS * FSEG_FRAG_SLOT_SIZE)

#define FSP_SEG_INODES_PER_PAGE(page_size)		\
	((page_size.physical() - FSEG_ARR_OFFSET - 10) / FSEG_INODE_SIZE)
				/* Number of segment inodes which fit on a
				single page */

#define FSEG_MAGIC_N_VALUE	97937874

#define	FSEG_FILLFACTOR		8	/* If this value is x, then if
					the number of unused but reserved
					pages in a segment is less than
					reserved pages * 1/x, and there are
					at least FSEG_FRAG_LIMIT used pages,
					then we allow a new empty extent to
					be added to the segment in
					fseg_alloc_free_page. Otherwise, we
					use unused pages of the segment. */

#define FSEG_FRAG_LIMIT		FSEG_FRAG_ARR_N_SLOTS
					/* If the segment has >= this many
					used pages, it may be expanded by
					allocating extents to the segment;
					until that only individual fragment
					pages are allocated from the space */

#define	FSEG_FREE_LIST_LIMIT	40	/* If the reserved size of a segment
					is at least this many extents, we
					allow extents to be put to the free
					list of the extent: at most
					FSEG_FREE_LIST_MAX_LEN many */
#define	FSEG_FREE_LIST_MAX_LEN	4
/* @} */

/* @defgroup Extent Descriptor Constants (moved from fsp0fsp.c) @{ */

/*			EXTENT DESCRIPTOR
			=================

File extent descriptor data structure: contains bits to tell which pages in
the extent are free and which contain old tuple version to clean. */

/*-------------------------------------*/
#define	XDES_ID			0	/* The identifier of the segment
					to which this extent belongs */
#define XDES_FLST_NODE		8	/* The list node data structure
					for the descriptors */
#define	XDES_STATE		(FLST_NODE_SIZE + 8)
					/* contains state information
					of the extent */
#define	XDES_BITMAP		(FLST_NODE_SIZE + 12)
					/* Descriptor bitmap of the pages
					in the extent */
/*-------------------------------------*/

#define	XDES_BITS_PER_PAGE	2	/* How many bits are there per page */
#define	XDES_FREE_BIT		0	/* Index of the bit which tells if
					the page is free */
#define	XDES_CLEAN_BIT		1	/* NOTE: currently not used!
					Index of the bit which tells if
					there are old versions of tuples
					on the page */
/* States of a descriptor */
#define	XDES_FREE		1	/* extent is in free list of space */
#define	XDES_FREE_FRAG		2	/* extent is in free fragment list of
					space */
#define	XDES_FULL_FRAG		3	/* extent is in full fragment list of
					space */
#define	XDES_FSEG		4	/* extent belongs to a segment */

/** File extent data structure size in bytes. */
#define	XDES_SIZE							\
	(XDES_BITMAP							\
	+ UT_BITS_IN_BYTES(FSP_EXTENT_SIZE * XDES_BITS_PER_PAGE))

/** File extent data structure size in bytes for MAX page size. */
#define	XDES_SIZE_MAX							\
	(XDES_BITMAP							\
	+ UT_BITS_IN_BYTES(FSP_EXTENT_SIZE_MAX * XDES_BITS_PER_PAGE))

/** File extent data structure size in bytes for MIN page size. */
#define	XDES_SIZE_MIN							\
	(XDES_BITMAP							\
	+ UT_BITS_IN_BYTES(FSP_EXTENT_SIZE_MIN * XDES_BITS_PER_PAGE))

/** Offset of the descriptor array on a descriptor page */
#define	XDES_ARR_OFFSET		(FSP_HEADER_OFFSET + FSP_HEADER_SIZE)

/* @} */

/**********************************************************************//**
Initializes the file space system. */
void
fsp_init(void);
/*==========*/

/**********************************************************************//**
Gets the size of the system tablespace from the tablespace header.  If
we do not have an auto-extending data file, this should be equal to
the size of the data files.  If there is an auto-extending data file,
this can be smaller.
@return size in pages */
ulint
fsp_header_get_tablespace_size(void);
/*================================*/

/** Calculate the number of pages to extend a datafile.
We extend single-table and general tablespaces first one extent at a time,
but 4 at a time for bigger tablespaces. It is not enough to extend always
by one extent, because we need to add at least one extent to FSP_FREE.
A single extent descriptor page will track many extents. And the extent
that uses its extent descriptor page is put onto the FSP_FREE_FRAG list.
Extents that do not use their extent descriptor page are added to FSP_FREE.
The physical page size is used to determine how many extents are tracked
on one extent descriptor page. See xdes_calc_descriptor_page().
@param[in]	page_size	page_size of the datafile
@param[in]	size		current number of pages in the datafile
@return number of pages to extend the file. */
ulint
fsp_get_pages_to_extend_ibd(
	const page_size_t&	page_size,
	ulint			size);

/** Calculate the number of physical pages in an extent for this file.
@param[in]	page_size	page_size of the datafile
@return number of pages in an extent for this file. */
UNIV_INLINE
ulint
fsp_get_extent_size_in_pages(const page_size_t&	page_size)
{
	return(FSP_EXTENT_SIZE * UNIV_PAGE_SIZE / page_size.physical());
}

/**********************************************************************//**
Reads the space id from the first page of a tablespace.
@return space id, ULINT UNDEFINED if error */
ulint
fsp_header_get_space_id(
/*====================*/
	const page_t*	page);	/*!< in: first page of a tablespace */

/** Read a tablespace header field.
@param[in]	page	first page of a tablespace
@param[in]	field	the header field
@return the contents of the header field */
inline
ulint
fsp_header_get_field(const page_t* page, ulint field)
{
	return(mach_read_from_4(FSP_HEADER_OFFSET + field + page));
}

/** Read the flags from the tablespace header page.
@param[in]	page	first page of a tablespace
@return the contents of FSP_SPACE_FLAGS */
inline
ulint
fsp_header_get_flags(const page_t* page)
{
	return(fsp_header_get_field(page, FSP_SPACE_FLAGS));
}

/** Reads the page size from the first page of a tablespace.
@param[in]	page	first page of a tablespace
@return page size */
page_size_t
fsp_header_get_page_size(
	const page_t*	page);

/** Decoding the encryption info
from the first page of a tablespace.
@param[in/out]	key		key
@param[in/out]	iv		iv
@param[in]	encryption_info	encrytion info.
@return true if success */
bool
fsp_header_decode_encryption_info(
	byte*		key,
	byte*		iv,
	byte*		encryption_info);

/** Reads the encryption key from the first page of a tablespace.
@param[in]	fsp_flags	tablespace flags
@param[in/out]	key		tablespace key
@param[in/out]	iv		tablespace iv
@param[in]	page	first page of a tablespace
@return true if success */
bool
fsp_header_get_encryption_key(
	ulint		fsp_flags,
	byte*		key,
	byte*		iv,
	page_t*		page);

/** Check the encryption key from the first page of a tablespace.
@param[in]	fsp_flags	tablespace flags
@param[in]	page		first page of a tablespace
@return true if success */
bool
fsp_header_check_encryption_key(
	ulint			fsp_flags,
	page_t*			page);

/**********************************************************************//**
Writes the space id and flags to a tablespace header.  The flags contain
row type, physical/compressed page size, and logical/uncompressed page
size of the tablespace. */
void
fsp_header_init_fields(
/*===================*/
	page_t*	page,		/*!< in/out: first page in the space */
	ulint	space_id,	/*!< in: space id */
	ulint	flags);		/*!< in: tablespace flags (FSP_SPACE_FLAGS):
				0, or table->flags if newer than COMPACT */

/** Rotate the encryption info in the space header.
@param[in]	space		tablespace
@param[in]      encrypt_info	buffer for re-encrypt key.
@param[in,out]	mtr		mini-transaction
@return true if success. */
bool
fsp_header_rotate_encryption(
	fil_space_t*		space,
	byte*			encrypt_info,
	mtr_t*			mtr);

/** Initializes the space header of a new created space and creates also the
insert buffer tree root if space == 0.
@param[in]	space_id	space id
@param[in]	size		current size in blocks
@param[in,out]	mtr		min-transaction
@return	true on success, otherwise false. */
bool
fsp_header_init(
	ulint	space_id,
	ulint	size,
	mtr_t*	mtr);

/**********************************************************************//**
Increases the space size field of a space. */
void
fsp_header_inc_size(
/*================*/
	ulint	space_id,	/*!< in: space id */
	ulint	size_inc,	/*!< in: size increment in pages */
	mtr_t*	mtr);		/*!< in/out: mini-transaction */
/**********************************************************************//**
Creates a new segment.
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
buf_block_t*
fseg_create(
/*========*/
	ulint	space_id,/*!< in: space id */
	ulint	page,	/*!< in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /*!< in: byte offset of the created segment header
			on the page */
	mtr_t*	mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Creates a new segment.
@return the block where the segment header is placed, x-latched, NULL
if could not create segment because of lack of space */
buf_block_t*
fseg_create_general(
/*================*/
	ulint	space_id,/*!< in: space id */
	ulint	page,	/*!< in: page where the segment header is placed: if
			this is != 0, the page must belong to another segment,
			if this is 0, a new page will be allocated and it
			will belong to the created segment */
	ulint	byte_offset, /*!< in: byte offset of the created segment header
			on the page */
	ibool	has_done_reservation, /*!< in: TRUE if the caller has already
			done the reservation for the pages with
			fsp_reserve_free_extents (at least 2 extents: one for
			the inode and the other for the segment) then there is
			no need to do the check for this individual
			operation */
	mtr_t*	mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Calculates the number of pages reserved by a segment, and how many pages are
currently used.
@return number of reserved pages */
ulint
fseg_n_reserved_pages(
/*==================*/
	fseg_header_t*	header,	/*!< in: segment header */
	ulint*		used,	/*!< out: number of pages used (<= reserved) */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize
file space fragmentation.
@param[in,out] seg_header segment header
@param[in] hint hint of which page would be desirable
@param[in] direction if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR
@param[in,out] mtr mini-transaction
@return X-latched block, or NULL if no page could be allocated */
#define fseg_alloc_free_page(seg_header, hint, direction, mtr)		\
	fseg_alloc_free_page_general(seg_header, hint, direction,	\
				     FALSE, mtr, mtr)
/**********************************************************************//**
Allocates a single free page from a segment. This function implements
the intelligent allocation strategy which tries to minimize file space
fragmentation.
@retval NULL if no page could be allocated
@retval block, rw_lock_x_lock_count(&block->lock) == 1 if allocation succeeded
(init_mtr == mtr, or the page was not previously freed in mtr)
@retval block (not allocated or initialized) otherwise */
buf_block_t*
fseg_alloc_free_page_general(
/*=========================*/
	fseg_header_t*	seg_header,/*!< in/out: segment header */
	ulint		hint,	/*!< in: hint of which page would be
				desirable */
	byte		direction,/*!< in: if the new page is needed because
				of an index page split, and records are
				inserted there in order, into which
				direction they go alphabetically: FSP_DOWN,
				FSP_UP, FSP_NO_DIR */
	ibool		has_done_reservation, /*!< in: TRUE if the caller has
				already done the reservation for the page
				with fsp_reserve_free_extents, then there
				is no need to do the check for this individual
				page */
	mtr_t*		mtr,	/*!< in/out: mini-transaction */
	mtr_t*		init_mtr)/*!< in/out: mtr or another mini-transaction
				in which the page should be initialized.
				If init_mtr!=mtr, but the page is already
				latched in mtr, do not initialize the page. */
	MY_ATTRIBUTE((warn_unused_result));

/** Reserves free pages from a tablespace. All mini-transactions which may
use several pages from the tablespace should call this function beforehand
and reserve enough free extents so that they certainly will be able
to do their operation, like a B-tree page split, fully. Reservations
must be released with function fil_space_release_free_extents!

The alloc_type below has the following meaning: FSP_NORMAL means an
operation which will probably result in more space usage, like an
insert in a B-tree; FSP_UNDO means allocation to undo logs: if we are
deleting rows, then this allocation will in the long run result in
less space usage (after a purge); FSP_CLEANING means allocation done
in a physical record delete (like in a purge) or other cleaning operation
which will result in less space usage in the long run. We prefer the latter
two types of allocation: when space is scarce, FSP_NORMAL allocations
will not succeed, but the latter two allocations will succeed, if possible.
The purpose is to avoid dead end where the database is full but the
user cannot free any space because these freeing operations temporarily
reserve some space.

Single-table tablespaces whose size is < FSP_EXTENT_SIZE pages are a special
case. In this function we would liberally reserve several extents for
every page split or merge in a B-tree. But we do not want to waste disk space
if the table only occupies < FSP_EXTENT_SIZE pages. That is why we apply
different rules in that special case, just ensuring that there are n_pages
free pages available.

@param[out]	n_reserved	number of extents actually reserved; if we
				return true and the tablespace size is <
				FSP_EXTENT_SIZE pages, then this can be 0,
				otherwise it is n_ext
@param[in]	space_id	tablespace identifier
@param[in]	n_ext		number of extents to reserve
@param[in]	alloc_type	page reservation type (FSP_BLOB, etc)
@param[in,out]	mtr		the mini transaction
@param[in]	n_pages		for small tablespaces (tablespace size is
				less than FSP_EXTENT_SIZE), number of free
				pages to reserve.
@return true if we were able to make the reservation */
bool
fsp_reserve_free_extents(
	ulint*		n_reserved,
	ulint		space_id,
	ulint		n_ext,
	fsp_reserve_t	alloc_type,
	mtr_t*		mtr,
	ulint		n_pages = 2);

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space.
@param[in]	space_id	tablespace ID
@return available space in KiB
@retval UINTMAX_MAX if unknown */
uintmax_t
fsp_get_available_space_in_free_extents(
	ulint		space_id);

/** Calculate how many KiB of new data we will be able to insert to the
tablespace without running out of space. Start with a space object that has
been acquired by the caller who holds it for the calculation,
@param[in]	space		tablespace object from fil_space_acquire()
@return available space in KiB */
uintmax_t
fsp_get_available_space_in_free_extents(
	const fil_space_t*	space);

/**********************************************************************//**
Frees a single page of a segment. */
void
fseg_free_page(
/*===========*/
	fseg_header_t*	seg_header, /*!< in: segment header */
	ulint		space_id, /*!< in: space id */
	ulint		page,	/*!< in: page offset */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
/**********************************************************************//**
Checks if a single page of a segment is free.
@return true if free */
bool
fseg_page_is_free(
/*==============*/
	fseg_header_t*	seg_header,	/*!< in: segment header */
	ulint		space_id,	/*!< in: space id */
	ulint		page)		/*!< in: page offset */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Frees part of a segment. This function can be used to free a segment
by repeatedly calling this function in different mini-transactions.
Doing the freeing in a single mini-transaction might result in
too big a mini-transaction.
@return TRUE if freeing completed */
ibool
fseg_free_step(
/*===========*/
	fseg_header_t*	header,	/*!< in, own: segment header; NOTE: if the header
				resides on the first page of the frag list
				of the segment, this pointer becomes obsolete
				after the last freeing step */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	MY_ATTRIBUTE((warn_unused_result));
/**********************************************************************//**
Frees part of a segment. Differs from fseg_free_step because this function
leaves the header page unfreed.
@return TRUE if freeing completed, except the header page */
ibool
fseg_free_step_not_header(
/*======================*/
	fseg_header_t*	header,	/*!< in: segment header which must reside on
				the first fragment page of the segment */
	bool		ahi,	/*!< in: whether we may need to drop
				the adaptive hash index */
	mtr_t*		mtr)	/*!< in/out: mini-transaction */
	MY_ATTRIBUTE((warn_unused_result));

/** Checks if a page address is an extent descriptor page address.
@param[in]	page_id		page id
@param[in]	page_size	page size
@return TRUE if a descriptor page */
UNIV_INLINE
ibool
fsp_descr_page(
	const page_id_t&	page_id,
	const page_size_t&	page_size);

/***********************************************************//**
Parses a redo log record of a file page init.
@return end of log record or NULL */
byte*
fsp_parse_init_file_page(
/*=====================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr, /*!< in: buffer end */
	buf_block_t*	block);	/*!< in: block or NULL */
#ifdef UNIV_DEBUG
/*******************************************************************//**
Validates a segment.
@return TRUE if ok */
ibool
fseg_validate(
/*==========*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
#endif /* UNIV_DEBUG */
#ifdef UNIV_BTR_PRINT
/*******************************************************************//**
Writes info of a segment. */
void
fseg_print(
/*=======*/
	fseg_header_t*	header, /*!< in: segment header */
	mtr_t*		mtr);	/*!< in/out: mini-transaction */
#endif /* UNIV_BTR_PRINT */

/** Determine if the tablespace is compressed from tablespace flags.
@param[in]	flags	Tablespace flags
@return true if compressed, false if not compressed */
UNIV_INLINE
bool
fsp_flags_is_compressed(
	ulint	flags);

/** Determine if two tablespaces are equivalent or compatible.
@param[in]	flags1	First tablespace flags
@param[in]	flags2	Second tablespace flags
@return true the flags are compatible, false if not */
UNIV_INLINE
bool
fsp_flags_are_equal(
	ulint	flags1,
	ulint	flags2);

/** Initialize an FSP flags integer.
@param[in]	page_size	page sizes in bytes and compression flag.
@param[in]	atomic_blobs	Used by Dynammic and Compressed.
@param[in]	has_data_dir	This tablespace is in a remote location.
@param[in]	is_shared	This tablespace can be shared by many tables.
@param[in]	is_temporary	This tablespace is temporary.
@param[in]	is_encrypted	This tablespace is encrypted.
@return tablespace flags after initialization */
UNIV_INLINE
ulint
fsp_flags_init(
	const page_size_t&	page_size,
	bool			atomic_blobs,
	bool			has_data_dir,
	bool			is_shared,
	bool			is_temporary,
	bool			is_encrypted = false);

/** Convert a 32 bit integer tablespace flags to the 32 bit table flags.
This can only be done for a tablespace that was built as a file-per-table
tablespace. Note that the fsp_flags cannot show the difference between a
Compact and Redundant table, so an extra Compact boolean must be supplied.
			Low order bit
                    | REDUNDANT | COMPACT | COMPRESSED | DYNAMIC
fil_space_t::flags  |     0     |    0    |     1      |    1
dict_table_t::flags |     0     |    1    |     1      |    1
@param[in]	fsp_flags	fil_space_t::flags
@param[in]	compact		true if not Redundant row format
@return tablespace flags (fil_space_t::flags) */
ulint
fsp_flags_to_dict_tf(
	ulint	fsp_flags,
	bool	compact);

/** Calculates the descriptor index within a descriptor page.
@param[in]	page_size	page size
@param[in]	offset		page offset
@return descriptor index */
UNIV_INLINE
ulint
xdes_calc_descriptor_index(
	const page_size_t&	page_size,
	ulint			offset);

/** Gets pointer to a the extent descriptor of a page.
The page where the extent descriptor resides is x-locked. If the page offset
is equal to the free limit of the space, adds new extents from above the free
limit to the space free list, if not free limit == space size. This adding
is necessary to make the descriptor defined, as they are uninitialized
above the free limit.
@param[in]	space_id	space id
@param[in]	offset		page offset; if equal to the free limit, we
try to add new extents to the space free list
@param[in]	page_size	page size
@param[in,out]	mtr		mini-transaction
@return pointer to the extent descriptor, NULL if the page does not
exist in the space or if the offset exceeds the free limit */
xdes_t*
xdes_get_descriptor(
	ulint			space_id,
	ulint			offset,
	const page_size_t&	page_size,
	mtr_t*			mtr)
MY_ATTRIBUTE((warn_unused_result));

/**********************************************************************//**
Gets a descriptor bit of a page.
@return TRUE if free */
UNIV_INLINE
ibool
xdes_get_bit(
/*=========*/
	const xdes_t*	descr,	/*!< in: descriptor */
	ulint		bit,	/*!< in: XDES_FREE_BIT or XDES_CLEAN_BIT */
	ulint		offset);/*!< in: page offset within extent:
				0 ... FSP_EXTENT_SIZE - 1 */

/** Calculates the page where the descriptor of a page resides.
@param[in]	page_size	page size
@param[in]	offset		page offset
@return descriptor page offset */
UNIV_INLINE
ulint
xdes_calc_descriptor_page(
	const page_size_t&	page_size,
	ulint			offset);
#endif /* !UNIV_INNOCHECKSUM */

#ifndef UNIV_NONINL
#include "fsp0fsp.ic"
#endif

#endif
