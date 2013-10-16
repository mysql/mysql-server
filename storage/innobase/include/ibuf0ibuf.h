/*****************************************************************************

Copyright (c) 1997, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/ibuf0ibuf.h
Insert buffer

Created 7/19/1997 Heikki Tuuri
*******************************************************/

#ifndef ibuf0ibuf_h
#define ibuf0ibuf_h

#include "univ.i"

#include "mtr0mtr.h"
#include "dict0mem.h"
#include "fsp0fsp.h"

#ifndef UNIV_HOTBACKUP
# include "ibuf0types.h"

/** Default value for maximum on-disk size of change buffer in terms
of percentage of the buffer pool. */
#define CHANGE_BUFFER_DEFAULT_SIZE	(25)

/* Possible operations buffered in the insert/whatever buffer. See
ibuf_insert(). DO NOT CHANGE THE VALUES OF THESE, THEY ARE STORED ON DISK. */
typedef enum {
	IBUF_OP_INSERT = 0,
	IBUF_OP_DELETE_MARK = 1,
	IBUF_OP_DELETE = 2,

	/* Number of different operation types. */
	IBUF_OP_COUNT = 3
} ibuf_op_t;

/** Combinations of operations that can be buffered.  Because the enum
values are used for indexing innobase_change_buffering_values[], they
should start at 0 and there should not be any gaps. */
typedef enum {
	IBUF_USE_NONE = 0,
	IBUF_USE_INSERT,	/* insert */
	IBUF_USE_DELETE_MARK,	/* delete */
	IBUF_USE_INSERT_DELETE_MARK,	/* insert+delete */
	IBUF_USE_DELETE,	/* delete+purge */
	IBUF_USE_ALL,		/* insert+delete+purge */

	IBUF_USE_COUNT		/* number of entries in ibuf_use_t */
} ibuf_use_t;

/** Operations that can currently be buffered. */
extern ibuf_use_t	ibuf_use;

/** The insert buffer control structure */
extern ibuf_t*		ibuf;

/* The purpose of the insert buffer is to reduce random disk access.
When we wish to insert a record into a non-unique secondary index and
the B-tree leaf page where the record belongs to is not in the buffer
pool, we insert the record into the insert buffer B-tree, indexed by
(space_id, page_no).  When the page is eventually read into the buffer
pool, we look up the insert buffer B-tree for any modifications to the
page, and apply these upon the completion of the read operation.  This
is called the insert buffer merge. */

/* The insert buffer merge must always succeed.  To guarantee this,
the insert buffer subsystem keeps track of the free space in pages for
which it can buffer operations.  Two bits per page in the insert
buffer bitmap indicate the available space in coarse increments.  The
free bits in the insert buffer bitmap must never exceed the free space
on a page.  It is safe to decrement or reset the bits in the bitmap in
a mini-transaction that is committed before the mini-transaction that
affects the free space.  It is unsafe to increment the bits in a
separately committed mini-transaction, because in crash recovery, the
free bits could momentarily be set too high. */

/******************************************************************//**
Creates the insert buffer data structure at a database startup. */
UNIV_INTERN
void
ibuf_init_at_db_start(void);
/*=======================*/
/*********************************************************************//**
Updates the max_size value for ibuf. */
UNIV_INTERN
void
ibuf_max_size_update(
/*=================*/
	ulint	new_val);	/*!< in: new value in terms of
				percentage of the buffer pool size */
/*********************************************************************//**
Reads the biggest tablespace id from the high end of the insert buffer
tree and updates the counter in fil_system. */
UNIV_INTERN
void
ibuf_update_max_tablespace_id(void);
/*===============================*/
/***************************************************************//**
Starts an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_start(
/*===========*/
	mtr_t*	mtr)	/*!< out: mini-transaction */
	__attribute__((nonnull));
/***************************************************************//**
Commits an insert buffer mini-transaction. */
UNIV_INLINE
void
ibuf_mtr_commit(
/*============*/
	mtr_t*	mtr)	/*!< in/out: mini-transaction */
	__attribute__((nonnull));
/*********************************************************************//**
Initializes an ibuf bitmap page. */
UNIV_INTERN
void
ibuf_bitmap_page_init(
/*==================*/
	buf_block_t*	block,	/*!< in: bitmap page */
	mtr_t*		mtr);	/*!< in: mtr */
/************************************************************************//**
Resets the free bits of the page in the ibuf bitmap. This is done in a
separate mini-transaction, hence this operation does not restrict
further work to only ibuf bitmap operations, which would result if the
latch to the bitmap page were kept.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to decrement or reset the bits in the bitmap in a mini-transaction
that is committed before the mini-transaction that affects the free
space. */
UNIV_INTERN
void
ibuf_reset_free_bits(
/*=================*/
	buf_block_t*	block);	/*!< in: index page; free bits are set to 0
				if the index is a non-clustered
				non-unique, and page level is 0 */
/************************************************************************//**
Updates the free bits of an uncompressed page in the ibuf bitmap if
there is not enough free on the page any more.  This is done in a
separate mini-transaction, hence this operation does not restrict
further work to only ibuf bitmap operations, which would result if the
latch to the bitmap page were kept.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is
unsafe to increment the bits in a separately committed
mini-transaction, because in crash recovery, the free bits could
momentarily be set too high.  It is only safe to use this function for
decrementing the free bits.  Should more free space become available,
we must not update the free bits here, because that would break crash
recovery. */
UNIV_INLINE
void
ibuf_update_free_bits_if_full(
/*==========================*/
	buf_block_t*	block,	/*!< in: index page to which we have added new
				records; the free bits are updated if the
				index is non-clustered and non-unique and
				the page level is 0, and the page becomes
				fuller */
	ulint		max_ins_size,/*!< in: value of maximum insert size with
				reorganize before the latest operation
				performed to the page */
	ulint		increase);/*!< in: upper limit for the additional space
				used in the latest operation, if known, or
				ULINT_UNDEFINED */
/**********************************************************************//**
Updates the free bits for an uncompressed page to reflect the present
state.  Does this in the mtr given, which means that the latching
order rules virtually prevent any further operations for this OS
thread until mtr is committed.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to set the free bits in the same mini-transaction that updated the
page. */
UNIV_INTERN
void
ibuf_update_free_bits_low(
/*======================*/
	const buf_block_t*	block,		/*!< in: index page */
	ulint			max_ins_size,	/*!< in: value of
						maximum insert size
						with reorganize before
						the latest operation
						performed to the page */
	mtr_t*			mtr);		/*!< in/out: mtr */
/**********************************************************************//**
Updates the free bits for a compressed page to reflect the present
state.  Does this in the mtr given, which means that the latching
order rules virtually prevent any further operations for this OS
thread until mtr is committed.  NOTE: The free bits in the insert
buffer bitmap must never exceed the free space on a page.  It is safe
to set the free bits in the same mini-transaction that updated the
page. */
UNIV_INTERN
void
ibuf_update_free_bits_zip(
/*======================*/
	buf_block_t*	block,	/*!< in/out: index page */
	mtr_t*		mtr);	/*!< in/out: mtr */
/**********************************************************************//**
Updates the free bits for the two pages to reflect the present state.
Does this in the mtr given, which means that the latching order rules
virtually prevent any further operations until mtr is committed.
NOTE: The free bits in the insert buffer bitmap must never exceed the
free space on a page.  It is safe to set the free bits in the same
mini-transaction that updated the pages. */
UNIV_INTERN
void
ibuf_update_free_bits_for_two_pages_low(
/*====================================*/
	ulint		zip_size,/*!< in: compressed page size in bytes;
				0 for uncompressed pages */
	buf_block_t*	block1,	/*!< in: index page */
	buf_block_t*	block2,	/*!< in: index page */
	mtr_t*		mtr);	/*!< in: mtr */
/**********************************************************************//**
A basic partial test if an insert to the insert buffer could be possible and
recommended. */
UNIV_INLINE
ibool
ibuf_should_try(
/*============*/
	dict_index_t*	index,			/*!< in: index where to insert */
	ulint		ignore_sec_unique);	/*!< in: if != 0, we should
						ignore UNIQUE constraint on
						a secondary index when we
						decide */
/******************************************************************//**
Returns TRUE if the current OS thread is performing an insert buffer
routine.

For instance, a read-ahead of non-ibuf pages is forbidden by threads
that are executing an insert buffer routine.
@return TRUE if inside an insert buffer routine */
UNIV_INLINE
ibool
ibuf_inside(
/*========*/
	const mtr_t*	mtr)	/*!< in: mini-transaction */
	__attribute__((nonnull, pure));
/***********************************************************************//**
Checks if a page address is an ibuf bitmap page (level 3 page) address.
@return	TRUE if a bitmap page */
UNIV_INLINE
ibool
ibuf_bitmap_page(
/*=============*/
	ulint	zip_size,/*!< in: compressed page size in bytes;
			0 for uncompressed pages */
	ulint	page_no);/*!< in: page number */
/***********************************************************************//**
Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==TRUE.
@return	TRUE if level 2 or level 3 page */
UNIV_INTERN
ibool
ibuf_page_low(
/*==========*/
	ulint		space,	/*!< in: space id */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number */
#ifdef UNIV_DEBUG
	ibool		x_latch,/*!< in: FALSE if relaxed check
				(avoid latching the bitmap page) */
#endif /* UNIV_DEBUG */
	const char*	file,	/*!< in: file name */
	ulint		line,	/*!< in: line where called */
	mtr_t*		mtr)	/*!< in: mtr which will contain an
				x-latch to the bitmap page if the page
				is not one of the fixed address ibuf
				pages, or NULL, in which case a new
				transaction is created. */
	__attribute__((warn_unused_result));
#ifdef UNIV_DEBUG
/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of
pages.  Must not be called when recv_no_ibuf_operations==TRUE.
@param space	tablespace identifier
@param zip_size	compressed page size in bytes, or 0
@param page_no	page number
@param mtr	mini-transaction or NULL
@return TRUE if level 2 or level 3 page */
# define ibuf_page(space, zip_size, page_no, mtr)			\
	ibuf_page_low(space, zip_size, page_no, TRUE, __FILE__, __LINE__, mtr)
#else /* UVIV_DEBUG */
/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of
pages.  Must not be called when recv_no_ibuf_operations==TRUE.
@param space	tablespace identifier
@param zip_size	compressed page size in bytes, or 0
@param page_no	page number
@param mtr	mini-transaction or NULL
@return TRUE if level 2 or level 3 page */
# define ibuf_page(space, zip_size, page_no, mtr)			\
	ibuf_page_low(space, zip_size, page_no, __FILE__, __LINE__, mtr)
#endif /* UVIV_DEBUG */
/***********************************************************************//**
Frees excess pages from the ibuf free list. This function is called when an OS
thread calls fsp services to allocate a new file segment, or a new page to a
file segment, and the thread did not own the fsp latch before this call. */
UNIV_INTERN
void
ibuf_free_excess_pages(void);
/*========================*/
/*********************************************************************//**
Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible. Does not do it if the index
is clustered or unique.
@return	TRUE if success */
UNIV_INTERN
ibool
ibuf_insert(
/*========*/
	ibuf_op_t	op,	/*!< in: operation type */
	const dtuple_t*	entry,	/*!< in: index entry to insert */
	dict_index_t*	index,	/*!< in: index where to insert */
	ulint		space,	/*!< in: space id where to insert */
	ulint		zip_size,/*!< in: compressed page size in bytes, or 0 */
	ulint		page_no,/*!< in: page number where to insert */
	que_thr_t*	thr);	/*!< in: query thread */
/*********************************************************************//**
When an index page is read from a disk to the buffer pool, this function
applies any buffered operations to the page and deletes the entries from the
insert buffer. If the page is not read, but created in the buffer pool, this
function deletes its buffered entries from the insert buffer; there can
exist entries for such a page if the page belonged to an index which
subsequently was dropped. */
UNIV_INTERN
void
ibuf_merge_or_delete_for_page(
/*==========================*/
	buf_block_t*	block,	/*!< in: if page has been read from
				disk, pointer to the page x-latched,
				else NULL */
	ulint		space,	/*!< in: space id of the index page */
	ulint		page_no,/*!< in: page number of the index page */
	ulint		zip_size,/*!< in: compressed page size in bytes,
				or 0 */
	ibool		update_ibuf_bitmap);/*!< in: normally this is set
				to TRUE, but if we have deleted or are
				deleting the tablespace, then we
				naturally do not want to update a
				non-existent bitmap page */
/*********************************************************************//**
Deletes all entries in the insert buffer for a given space id. This is used
in DISCARD TABLESPACE and IMPORT TABLESPACE.
NOTE: this does not update the page free bitmaps in the space. The space will
become CORRUPT when you call this function! */
UNIV_INTERN
void
ibuf_delete_for_discarded_space(
/*============================*/
	ulint	space);	/*!< in: space id */
/*********************************************************************//**
Contracts insert buffer trees by reading pages to the buffer pool.
@return a lower limit for the combined size in bytes of entries which
will be merged from ibuf trees to the pages read, 0 if ibuf is
empty */
UNIV_INTERN
ulint
ibuf_contract_in_background(
/*========================*/
	table_id_t	table_id,	/*!< in: if merge should be done only
					for a specific table, for all tables
					this should be 0 */
	ibool		full);		/*!< in: TRUE if the caller wants to
					do a full contract based on PCT_IO(100).
					If FALSE then the size of contract
					batch is determined based on the
					current size of the ibuf tree. */
#endif /* !UNIV_HOTBACKUP */
/*********************************************************************//**
Parses a redo log record of an ibuf bitmap page init.
@return	end of log record or NULL */
UNIV_INTERN
byte*
ibuf_parse_bitmap_init(
/*===================*/
	byte*		ptr,	/*!< in: buffer */
	byte*		end_ptr,/*!< in: buffer end */
	buf_block_t*	block,	/*!< in: block or NULL */
	mtr_t*		mtr);	/*!< in: mtr or NULL */
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_IBUF_COUNT_DEBUG
/******************************************************************//**
Gets the ibuf count for a given page.
@return number of entries in the insert buffer currently buffered for
this page */
UNIV_INTERN
ulint
ibuf_count_get(
/*===========*/
	ulint	space,	/*!< in: space id */
	ulint	page_no);/*!< in: page number */
#endif
/******************************************************************//**
Looks if the insert buffer is empty.
@return	true if empty */
UNIV_INTERN
bool
ibuf_is_empty(void);
/*===============*/
/******************************************************************//**
Prints info of ibuf. */
UNIV_INTERN
void
ibuf_print(
/*=======*/
	FILE*	file);	/*!< in: file where to print */
/********************************************************************
Read the first two bytes from a record's fourth field (counter field in new
records; something else in older records).
@return	"counter" field, or ULINT_UNDEFINED if for some reason it can't be read */
UNIV_INTERN
ulint
ibuf_rec_get_counter(
/*=================*/
	const rec_t*	rec);	/*!< in: ibuf record */
/******************************************************************//**
Closes insert buffer and frees the data structures. */
UNIV_INTERN
void
ibuf_close(void);
/*============*/

/******************************************************************//**
Checks the insert buffer bitmaps on IMPORT TABLESPACE.
@return DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
ibuf_check_bitmap_on_import(
/*========================*/
	const trx_t*	trx,		/*!< in: transaction */
	ulint		space_id)	/*!< in: tablespace identifier */
	__attribute__((nonnull, warn_unused_result));

#define IBUF_HEADER_PAGE_NO	FSP_IBUF_HEADER_PAGE_NO
#define IBUF_TREE_ROOT_PAGE_NO	FSP_IBUF_TREE_ROOT_PAGE_NO

#endif /* !UNIV_HOTBACKUP */

/* The ibuf header page currently contains only the file segment header
for the file segment from which the pages for the ibuf tree are allocated */
#define IBUF_HEADER		PAGE_DATA
#define	IBUF_TREE_SEG_HEADER	0	/* fseg header for ibuf tree */

/* The insert buffer tree itself is always located in space 0. */
#define IBUF_SPACE_ID		0

#ifndef UNIV_NONINL
#include "ibuf0ibuf.ic"
#endif

#endif
