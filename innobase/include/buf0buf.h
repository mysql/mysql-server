/*   Innobase relational database engine; Copyright (C) 2001 Innobase Oy
     
     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License 2
     as published by the Free Software Foundation in June 1991.
     
     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     GNU General Public License for more details.
     
     You should have received a copy of the GNU General Public License 2
     along with this program (in file COPYING); if not, write to the Free
     Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA. */
/******************************************************
The database buffer pool high-level routines

(c) 1995 Innobase Oy

Created 11/5/1995 Heikki Tuuri
*******************************************************/

#ifndef buf0buf_h
#define buf0buf_h

#include "univ.i"
#include "fil0fil.h"
#include "mtr0types.h"
#include "buf0types.h"
#include "sync0rw.h"
#include "hash0hash.h"
#include "ut0byte.h"

/* Flags for flush types */
#define BUF_FLUSH_LRU		1
#define BUF_FLUSH_SINGLE_PAGE	2
#define BUF_FLUSH_LIST		3	/* An array in the pool struct
					has size BUF_FLUSH_LIST + 1: if you
					add more flush types, put them in
					the middle! */
/* Modes for buf_page_get_gen */
#define BUF_GET			10	/* get always */
#define	BUF_GET_IF_IN_POOL	11	/* get if in pool */
#define	BUF_GET_NOWAIT		12	/* get if can set the latch without
					waiting */
#define BUF_GET_NO_LATCH	14	/* get and bufferfix, but set no latch;
					we have separated this case, because
					it is error-prone programming not to
					set a latch, and it should be used
					with care */
/* Modes for buf_page_get_known_nowait */
#define BUF_MAKE_YOUNG	51
#define BUF_KEEP_OLD	52

extern buf_pool_t* 	buf_pool; 	/* The buffer pool of the database */
extern ibool		buf_debug_prints;/* If this is set TRUE, the program
					prints info whenever read or flush
					occurs */

/************************************************************************
Initializes the buffer pool of the database. */

void
buf_pool_init(
/*==========*/
	ulint	max_size,	/* in: maximum size of the pool in blocks */
	ulint	curr_size);	/* in: current size to use, must be <=
				max_size */
/*************************************************************************
Gets the current size of buffer pool in bytes. */
UNIV_INLINE
ulint
buf_pool_get_curr_size(void);
/*========================*/
			/* out: size in bytes */
/*************************************************************************
Gets the maximum size of buffer pool in bytes. */
UNIV_INLINE
ulint
buf_pool_get_max_size(void);
/*=======================*/
			/* out: size in bytes */
/************************************************************************
Gets the smallest oldest_modification lsn for any page in the pool. Returns
ut_dulint_zero if all modified pages have been flushed to disk. */
UNIV_INLINE
dulint
buf_pool_get_oldest_modification(void);
/*==================================*/
				/* out: oldest modification in pool,
				ut_dulint_zero if none */
/*************************************************************************
Allocates a buffer frame. */

buf_frame_t*
buf_frame_alloc(void);
/*==================*/
				/* out: buffer frame */
/*************************************************************************
Frees a buffer frame which does not contain a file page. */

void
buf_frame_free(
/*===========*/
	buf_frame_t*	frame);	/* in: buffer frame */
/*************************************************************************
Copies contents of a buffer frame to a given buffer. */
UNIV_INLINE
byte*
buf_frame_copy(
/*===========*/
				/* out: buf */
	byte*		buf,	/* in: buffer to copy to */
	buf_frame_t*	frame);	/* in: buffer frame */
/******************************************************************
NOTE! The following macros should be used instead of buf_page_get_gen,
to improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed
in LA! */
#ifdef UNIV_SYNC_DEBUG
#define buf_page_get(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET, IB__FILE__, __LINE__, MTR)
#else
#define buf_page_get(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET, MTR)
#endif
/******************************************************************
Use these macros to bufferfix a page with no latching. Remember not to
read the contents of the page unless you know it is safe. Do not modify
the contents of the page! We have separated this case, because it is
error-prone programming not to set a latch, and it should be used
with care. */
#ifdef UNIV_SYNC_DEBUG
#define buf_page_get_with_no_latch(SP, OF, MTR)    buf_page_get_gen(\
				SP, OF, RW_NO_LATCH, NULL,\
				BUF_GET_NO_LATCH, IB__FILE__, __LINE__, MTR)
#else
#define buf_page_get_with_no_latch(SP, OF, MTR)    buf_page_get_gen(\
				SP, OF, RW_NO_LATCH, NULL,\
				BUF_GET_NO_LATCH, MTR)
#endif
/******************************************************************
NOTE! The following macros should be used instead of buf_page_get_gen, to
improve debugging. Only values RW_S_LATCH and RW_X_LATCH are allowed as LA! */
#ifdef UNIV_SYNC_DEBUG
#define buf_page_get_nowait(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET_NOWAIT, IB__FILE__, __LINE__, MTR)
#else
#define buf_page_get_nowait(SP, OF, LA, MTR)    buf_page_get_gen(\
				SP, OF, LA, NULL,\
				BUF_GET_NOWAIT, MTR)
#endif
/******************************************************************
NOTE! The following macros should be used instead of
buf_page_optimistic_get_func, to improve debugging. Only values RW_S_LATCH and
RW_X_LATCH are allowed as LA! */
#ifdef UNIV_SYNC_DEBUG
#define buf_page_optimistic_get(LA, G, MC, MTR) buf_page_optimistic_get_func(\
				LA, G, MC, IB__FILE__, __LINE__, MTR)
#else
#define buf_page_optimistic_get(LA, G, MC, MTR) buf_page_optimistic_get_func(\
				LA, G, MC, MTR)
#endif
/************************************************************************
This is the general function used to get optimistic access to a database
page. */

ibool
buf_page_optimistic_get_func(
/*=========================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame */
	dulint		modify_clock,/* in: modify clock value if mode is
				..._GUESS_ON_CLOCK */
#ifdef UNIV_SYNC_DEBUG
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
#endif
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
Tries to get the page, but if file io is required, releases all latches
in mtr down to the given savepoint. If io is required, this function
retrieves the page to buffer buf_pool, but does not bufferfix it or latch
it. */
UNIV_INLINE
buf_frame_t*
buf_page_get_release_on_io(
/*=======================*/
				/* out: pointer to the frame, or NULL
				if not in buffer buf_pool */
	ulint	space,		/* in: space id */
	ulint	offset,		/* in: offset of the page within space
				in units of a page */
	buf_frame_t* guess,	/* in: guessed frame or NULL */
	ulint	rw_latch,	/* in: RW_X_LATCH, RW_S_LATCH,
				or RW_NO_LATCH */
	ulint	savepoint,	/* in: mtr savepoint */
	mtr_t*	mtr);		/* in: mtr */
/************************************************************************
This is used to get access to a known database page, when no waiting can be
done. */

ibool
buf_page_get_known_nowait(
/*======================*/
				/* out: TRUE if success */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH */
	buf_frame_t*	guess,	/* in: the known page frame */
	ulint		mode,	/* in: BUF_MAKE_YOUNG or BUF_KEEP_OLD */
#ifdef UNIV_SYNC_DEBUG
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
#endif
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
This is the general function used to get access to a database page. */

buf_frame_t*
buf_page_get_gen(
/*=============*/
				/* out: pointer to the frame or NULL */
	ulint		space,	/* in: space id */
	ulint		offset,	/* in: page number */
	ulint		rw_latch,/* in: RW_S_LATCH, RW_X_LATCH, RW_NO_LATCH */
	buf_frame_t*	guess,	/* in: guessed frame or NULL */
	ulint		mode,	/* in: BUF_GET, BUF_GET_IF_IN_POOL,
				BUF_GET_NO_LATCH */
#ifdef UNIV_SYNC_DEBUG
	char*		file,	/* in: file name */
	ulint		line,	/* in: line where called */
#endif
	mtr_t*		mtr);	/* in: mini-transaction */
/************************************************************************
Initializes a page to the buffer buf_pool. The page is usually not read
from a file even if it cannot be found in the buffer buf_pool. This is one
of the functions which perform to a block a state transition NOT_USED =>
FILE_PAGE (the other is buf_page_init_for_read above). */

buf_frame_t*
buf_page_create(
/*============*/
			/* out: pointer to the frame, page bufferfixed */
	ulint	space,	/* in: space id */
	ulint	offset,	/* in: offset of the page within space in units of
			a page */
	mtr_t*	mtr);	/* in: mini-transaction handle */
/************************************************************************
Decrements the bufferfix count of a buffer control block and releases
a latch, if specified. */
UNIV_INLINE
void
buf_page_release(
/*=============*/
	buf_block_t*	block,		/* in: buffer block */
	ulint		rw_latch,	/* in: RW_S_LATCH, RW_X_LATCH,
					RW_NO_LATCH */
	mtr_t*		mtr);		/* in: mtr */
/************************************************************************
Moves a page to the start of the buffer pool LRU list. This high-level
function can be used to prevent an important page from from slipping out of
the buffer pool. */

void
buf_page_make_young(
/*=================*/
	buf_frame_t*	frame);	/* in: buffer frame of a file page */
/************************************************************************
Returns TRUE if the page can be found in the buffer pool hash table. NOTE
that it is possible that the page is not yet read from disk, though. */

ibool
buf_page_peek(
/*==========*/
			/* out: TRUE if found from page hash table,
			NOTE that the page is not necessarily yet read
			from disk! */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Returns the buffer control block if the page can be found in the buffer
pool. NOTE that it is possible that the page is not yet read
from disk, though. This is a very low-level function: use with care! */

buf_block_t*
buf_page_peek_block(
/*================*/
			/* out: control block if found from page hash table,
			otherwise NULL; NOTE that the page is not necessarily
			yet read from disk! */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Sets file_page_was_freed TRUE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */

buf_block_t*
buf_page_set_file_page_was_freed(
/*=============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset);	/* in: page number */
/************************************************************************
Sets file_page_was_freed FALSE if the page is found in the buffer pool.
This function should be called when we free a file page and want the
debug version to check that it is not accessed any more unless
reallocated. */

buf_block_t*
buf_page_reset_file_page_was_freed(
/*===============================*/
			/* out: control block if found from page hash table,
			otherwise NULL */
	ulint	space,	/* in: space id */
	ulint	offset);	/* in: page number */
/************************************************************************
Recommends a move of a block to the start of the LRU list if there is danger
of dropping from the buffer pool. NOTE: does not reserve the buffer pool
mutex. */
UNIV_INLINE
ibool
buf_block_peek_if_too_old(
/*======================*/
				/* out: TRUE if should be made younger */
	buf_block_t*	block);	/* in: block to make younger */
/************************************************************************
Returns the current state of is_hashed of a page. FALSE if the page is
not in the pool. NOTE that this operation does not fix the page in the
pool if it is found there. */

ibool
buf_page_peek_if_search_hashed(
/*===========================*/
			/* out: TRUE if page hash index is built in search
			system */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Gets the youngest modification log sequence number for a frame.
Returns zero if not file page or no modification occurred yet. */
UNIV_INLINE
dulint
buf_frame_get_newest_modification(
/*==============================*/
				/* out: newest modification to page */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Increments the modify clock of a frame by 1. The caller must (1) own the
pool mutex and block bufferfix count has to be zero, (2) or own an x-lock
on the block. */
UNIV_INLINE
dulint
buf_frame_modify_clock_inc(
/*=======================*/
				/* out: new value */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Returns the value of the modify clock. The caller must have an s-lock 
or x-lock on the block. */
UNIV_INLINE
dulint
buf_frame_get_modify_clock(
/*=======================*/
				/* out: value */
	buf_frame_t*	frame);	/* in: pointer to a frame */
/************************************************************************
Calculates a page checksum which is stored to the page when it is written
to a file. Note that we must be careful to calculate the same value
on 32-bit and 64-bit architectures. */

ulint
buf_calc_page_checksum(
/*===================*/
		       /* out: checksum */
	byte*   page); /* in: buffer page */
/**************************************************************************
Gets the page number of a pointer pointing within a buffer frame containing
a file page. */
UNIV_INLINE
ulint
buf_frame_get_page_no(
/*==================*/
			/* out: page number */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the space id of a pointer pointing within a buffer frame containing a
file page. */
UNIV_INLINE
ulint
buf_frame_get_space_id(
/*===================*/
			/* out: space id */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the space id, page offset, and byte offset within page of a
pointer pointing to a buffer frame containing a file page. */
UNIV_INLINE
void
buf_ptr_get_fsp_addr(
/*=================*/
	byte*		ptr,	/* in: pointer to a buffer frame */
	ulint*		space,	/* out: space id */
	fil_addr_t*	addr);	/* out: page offset and byte offset */
/**************************************************************************
Gets the hash value of the page the pointer is pointing to. This can be used
in searches in the lock hash table. */
UNIV_INLINE
ulint
buf_frame_get_lock_hash_val(
/*========================*/
			/* out: lock hash value */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/**************************************************************************
Gets the mutex number protecting the page record lock hash chain in the lock
table. */
UNIV_INLINE
mutex_t*
buf_frame_get_lock_mutex(
/*=====================*/
			/* out: mutex */
	byte*	ptr);	/* in: pointer to within a buffer frame */
/***********************************************************************
Gets the frame the pointer is pointing to. */
UNIV_INLINE
buf_frame_t*
buf_frame_align(
/*============*/
			/* out: pointer to block */
	byte*	ptr);	/* in: pointer to a frame */
/***********************************************************************
Checks if a pointer points to the block array of the buffer pool (blocks, not
the frames). */
UNIV_INLINE
ibool
buf_pool_is_block(
/*==============*/
			/* out: TRUE if pointer to block */
	void*	ptr);	/* in: pointer to memory */
/*************************************************************************
Validates the buffer pool data structure. */

ibool
buf_validate(void);
/*==============*/
/*************************************************************************
Prints info of the buffer pool data structure. */

void
buf_print(void);
/*===========*/
/*************************************************************************
Prints info of the buffer i/o. */

void
buf_print_io(void);
/*==============*/
/*************************************************************************
Checks that all file pages in the buffer are in a replaceable state. */

ibool
buf_all_freed(void);
/*===============*/
/*************************************************************************
Checks that there currently are no pending i/o-operations for the buffer
pool. */

ibool
buf_pool_check_no_pending_io(void);
/*==============================*/
				/* out: TRUE if there is no pending i/o */
/*************************************************************************
Invalidates the file pages in the buffer pool when an archive recovery is
completed. All the file pages buffered must be in a replaceable state when
this function is called: not latched and not modified. */

void
buf_pool_invalidate(void);
/*=====================*/

/*========================================================================
--------------------------- LOWER LEVEL ROUTINES -------------------------
=========================================================================*/

/*************************************************************************
Adds latch level info for the rw-lock protecting the buffer frame. This
should be called in the debug version after a successful latching of a
page if we know the latching order level of the acquired latch. If
UNIV_SYNC_DEBUG is not defined, compiles to an empty function. */
UNIV_INLINE
void
buf_page_dbg_add_level(
/*===================*/
	buf_frame_t*	frame,	/* in: buffer page where we have acquired
				a latch */
	ulint		level);	/* in: latching order level */
/*************************************************************************
Gets a pointer to the memory frame of a block. */
UNIV_INLINE
buf_frame_t*
buf_block_get_frame(
/*================*/
				/* out: pointer to the frame */
	buf_block_t*	block);	/* in: pointer to the control block */
/*************************************************************************
Gets the space id of a block. */
UNIV_INLINE
ulint
buf_block_get_space(
/*================*/
				/* out: space id */
	buf_block_t*	block);	/* in: pointer to the control block */
/*************************************************************************
Gets the page number of a block. */
UNIV_INLINE
ulint
buf_block_get_page_no(
/*==================*/
				/* out: page number */
	buf_block_t*	block);	/* in: pointer to the control block */
/***********************************************************************
Gets the block to whose frame the pointer is pointing to. */
UNIV_INLINE
buf_block_t*
buf_block_align(
/*============*/
			/* out: pointer to block */
	byte*	ptr);	/* in: pointer to a frame */
/************************************************************************
This function is used to get info if there is an io operation
going on on a buffer page. */
UNIV_INLINE
ibool
buf_page_io_query(
/*==============*/
				/* out: TRUE if io going on */
	buf_block_t*	block);	/* in: pool block, must be bufferfixed */
/***********************************************************************
Accessor function for block array. */
UNIV_INLINE
buf_block_t*
buf_pool_get_nth_block(
/*===================*/
				/* out: pointer to block */
	buf_pool_t*	pool,	/* in: pool */
	ulint		i);	/* in: index of the block */
/************************************************************************
Function which inits a page for read to the buffer buf_pool. If the page is
already in buf_pool, does nothing. Sets the io_fix flag to BUF_IO_READ and
sets a non-recursive exclusive lock on the buffer frame. The io-handler must
take care that the flag is cleared and the lock released later. This is one
of the functions which perform the state transition NOT_USED => FILE_PAGE to
a block (the other is buf_page_create). */ 

buf_block_t*
buf_page_init_for_read(
/*===================*/
			/* out: pointer to the block */
	ulint	mode,	/* in: BUF_READ_IBUF_PAGES_ONLY, ... */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: page number */
/************************************************************************
Completes an asynchronous read or write request of a file page to or from
the buffer pool. */

void
buf_page_io_complete(
/*=================*/
	buf_block_t*	block);	/* in: pointer to the block in question */
/************************************************************************
Calculates a folded value of a file page address to use in the page hash
table. */
UNIV_INLINE
ulint
buf_page_address_fold(
/*==================*/
			/* out: the folded value */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: offset of the page within space */
/**********************************************************************
Returns the control block of a file page, NULL if not found. */
UNIV_INLINE
buf_block_t*
buf_page_hash_get(
/*==============*/
			/* out: block, NULL if not found */
	ulint	space,	/* in: space id */
	ulint	offset);/* in: offset of the page within space */
/***********************************************************************
Increments the pool clock by one and returns its new value. Remember that
in the 32 bit version the clock wraps around at 4 billion! */
UNIV_INLINE
ulint
buf_pool_clock_tic(void);
/*====================*/
			/* out: new clock value */
/*************************************************************************
Gets the current length of the free list of buffer blocks. */

ulint
buf_get_free_list_len(void);
/*=======================*/


			
/* The buffer control block structure */

struct buf_block_struct{

	/* 1. General fields */

	ulint		state;		/* state of the control block:
					BUF_BLOCK_NOT_USED, ... */
	byte*		frame;		/* pointer to buffer frame which
					is of size UNIV_PAGE_SIZE, and
					aligned to an address divisible by
					UNIV_PAGE_SIZE */
	ulint		space;		/* space id of the page */
	ulint		offset;		/* page number within the space */
	ulint		lock_hash_val;	/* hashed value of the page address
					in the record lock hash table */
	mutex_t*	lock_mutex;	/* mutex protecting the chain in the
					record lock hash table */
	rw_lock_t	lock;		/* read-write lock of the buffer
					frame */
	rw_lock_t	read_lock;	/* rw-lock reserved when a page read
					to the frame is requested; a thread
					can wait for this rw-lock if it wants
					to wait for the read to complete;
					the usual way is to wait for lock,
					but if the thread just wants a
					bufferfix and no latch on the page,
					then it can wait for this rw-lock */
	buf_block_t*	hash;		/* node used in chaining to the page
					hash table */
	/* 2. Page flushing fields */

	UT_LIST_NODE_T(buf_block_t) flush_list;
					/* node of the modified, not yet
					flushed blocks list */
	dulint		newest_modification;
					/* log sequence number of the youngest
					modification to this block, zero if
					not modified */
	dulint		oldest_modification;
					/* log sequence number of the START of
					the log entry written of the oldest
					modification to this block which has
					not yet been flushed on disk; zero if
					all modifications are on disk */
	ulint		flush_type;	/* if this block is currently being
					flushed to disk, this tells the
					flush_type: BUF_FLUSH_LRU or
					BUF_FLUSH_LIST */

	/* 3. LRU replacement algorithm fields */

	UT_LIST_NODE_T(buf_block_t) free;
					/* node of the free block list */
	UT_LIST_NODE_T(buf_block_t) LRU;
					/* node of the LRU list */
	ulint		LRU_position;	/* value which monotonically
					decreases (or may stay constant if
					the block is in the old blocks) toward
					the end of the LRU list, if the pool
					ulint_clock has not wrapped around:
					NOTE that this value can only be used
					in heuristic algorithms, because of
					the possibility of a wrap-around! */
	ulint		freed_page_clock;/* the value of freed_page_clock
					buffer pool when this block was
					last time put to the head of the
					LRU list */
	ibool		old;		/* TRUE if the block is in the old
					blocks in the LRU list */
	ibool		accessed;	/* TRUE if the page has been accessed
					while in the buffer pool: read-ahead
					may read in pages which have not been
					accessed yet */
	ulint		buf_fix_count;	/* count of how manyfold this block
					is currently bufferfixed */
	ulint		io_fix;		/* if a read is pending to the frame,
					io_fix is BUF_IO_READ, in the case
					of a write BUF_IO_WRITE, otherwise 0 */
	/* 4. Optimistic search field */

	dulint		modify_clock;	/* this clock is incremented every
					time a pointer to a record on the
					page may become obsolete; this is
					used in the optimistic cursor
					positioning: if the modify clock has
					not changed, we know that the pointer
					is still valid; this field may be
					changed if the thread (1) owns the
					pool mutex and the page is not
					bufferfixed, or (2) the thread has an
					x-latch on the block */

	/* 5. Hash search fields: NOTE that these fields are protected by
	btr_search_mutex */
	
	ulint		n_hash_helps;	/* counter which controls building
					of a new hash index for the page */
	ulint		n_fields;	/* recommended prefix length for hash
					search: number of full fields */
	ulint		n_bytes;	/* recommended prefix: number of bytes
					in an incomplete field */
	ulint		side;		/* BTR_SEARCH_LEFT_SIDE or
					BTR_SEARCH_RIGHT_SIDE, depending on
					whether the leftmost record of several
					records with the same prefix should be
					indexed in the hash index */
	ibool		is_hashed;	/* TRUE if hash index has already been
					built on this page; note that it does
					not guarantee that the index is
					complete, though: there may have been
					hash collisions, record deletions,
					etc. */
	ulint		curr_n_fields;	/* prefix length for hash indexing:
					number of full fields */
	ulint		curr_n_bytes;	/* number of bytes in hash indexing */
	ulint		curr_side;	/* BTR_SEARCH_LEFT_SIDE or
					BTR_SEARCH_RIGHT_SIDE in hash
					indexing */
	/* 6. Debug fields */

	rw_lock_t	debug_latch;	/* in the debug version, each thread
					which bufferfixes the block acquires
					an s-latch here; so we can use the
					debug utilities in sync0rw */
        ibool           file_page_was_freed;
                                        /* this is set to TRUE when fsp
                                        frees a page in buffer pool */
};

/* The buffer pool structure. NOTE! The definition appears here only for
other modules of this directory (buf) to see it. Do not use from outside! */

struct buf_pool_struct{

	/* 1. General fields */

	mutex_t		mutex;		/* mutex protecting the buffer pool
					struct and control blocks, except the
					read-write lock in them */
	byte*		frame_mem;	/* pointer to the memory area which
					was allocated for the frames */
	byte*		frame_zero;	/* pointer to the first buffer frame:
					this may differ from frame_mem, because
					this is aligned by the frame size */
	buf_block_t*	blocks;		/* array of buffer control blocks */
	ulint		max_size;	/* number of control blocks ==
					maximum pool size in pages */
	ulint		curr_size;	/* current pool size in pages */
	hash_table_t*	page_hash;	/* hash table of the file pages */

	ulint		n_pend_reads;	/* number of pending read operations */
	ulint		n_pages_read;	/* number read operations */
	ulint		n_pages_written;/* number write operations */
	ulint		n_pages_created;/* number of pages created in the pool
					with no read */
	/* 2. Page flushing algorithm fields */

	UT_LIST_BASE_NODE_T(buf_block_t) flush_list;
					/* base node of the modified block
					list */
	ibool		init_flush[BUF_FLUSH_LIST + 1];
					/* this is TRUE when a flush of the
					given type is being initialized */
	ulint		n_flush[BUF_FLUSH_LIST + 1];
					/* this is the number of pending
					writes in the given flush type */
	os_event_t	no_flush[BUF_FLUSH_LIST + 1];
					/* this is in the set state when there
					is no flush batch of the given type
					running */
	ulint		ulint_clock;	/* a sequence number used to count
					time. NOTE! This counter wraps
					around at 4 billion (if ulint ==
					32 bits)! */
	ulint		freed_page_clock;/* a sequence number used to count the
					number of buffer blocks removed from
					the end of the LRU list; NOTE that
					this counter may wrap around at 4
					billion! */
	ulint		LRU_flush_ended;/* when an LRU flush ends for a page,
					this is incremented by one; this is
					set to zero when a buffer block is
					allocated */

	/* 3. LRU replacement algorithm fields */

	UT_LIST_BASE_NODE_T(buf_block_t) free;
					/* base node of the free block list */
	UT_LIST_BASE_NODE_T(buf_block_t) LRU;
					/* base node of the LRU list */
	buf_block_t*	LRU_old; 	/* pointer to the about 3/8 oldest
					blocks in the LRU list; NULL if LRU
					length less than BUF_LRU_OLD_MIN_LEN */
	ulint		LRU_old_len;	/* length of the LRU list from
					the block to which LRU_old points
					onward, including that block;
					see buf0lru.c for the restrictions
					on this value; not defined if
					LRU_old == NULL */
};

/* States of a control block */
#define	BUF_BLOCK_NOT_USED	211	/* is in the free list */
#define BUF_BLOCK_READY_FOR_USE	212	/* when buf_get_free_block returns
					a block, it is in this state */
#define	BUF_BLOCK_FILE_PAGE	213	/* contains a buffered file page */
#define	BUF_BLOCK_MEMORY	214	/* contains some main memory object */
#define BUF_BLOCK_REMOVE_HASH	215	/* hash index should be removed
					before putting to the free list */

/* Io_fix states of a control block; these must be != 0 */
#define BUF_IO_READ		561
#define BUF_IO_WRITE		562

/************************************************************************
Let us list the consistency conditions for different control block states.

NOT_USED:	is in free list, not in LRU list, not in flush list, nor
		page hash table
READY_FOR_USE:	is not in free list, LRU list, or flush list, nor page
		hash table
MEMORY:		is not in free list, LRU list, or flush list, nor page
		hash table
FILE_PAGE:	space and offset are defined, is in page hash table
		if io_fix == BUF_IO_WRITE,
			pool: no_flush[block->flush_type] is in reset state,
			pool: n_flush[block->flush_type] > 0			
		
		(1) if buf_fix_count == 0, then
			is in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			is x-locked,
				if and only if io_fix == BUF_IO_READ
			is s-locked,
				if and only if io_fix == BUF_IO_WRITE
						
		(2) if buf_fix_count > 0, then
			is not in LRU list, not in free list
			is in flush list,
				if and only if oldest_modification > 0
			if io_fix == BUF_IO_READ,		
				is x-locked
			if io_fix == BUF_IO_WRITE,
				is s-locked
			
State transitions:

NOT_USED => READY_FOR_USE
READY_FOR_USE => MEMORY
READY_FOR_USE => FILE_PAGE
MEMORY => NOT_USED
FILE_PAGE => NOT_USED	NOTE: This transition is allowed if and only if 
				(1) buf_fix_count == 0,
				(2) oldest_modification == 0, and
				(3) io_fix == 0.
*/

#ifndef UNIV_NONINL
#include "buf0buf.ic"
#endif

#endif
