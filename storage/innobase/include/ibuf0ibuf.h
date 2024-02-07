/*****************************************************************************

Copyright (c) 1997, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/ibuf0ibuf.h
 Insert buffer

 Created 7/19/1997 Heikki Tuuri
 *******************************************************/

#ifndef ibuf0ibuf_h
#define ibuf0ibuf_h

#include "univ.i"

#include "dict0mem.h"
#include "fsp0fsp.h"
#include "mtr0mtr.h"

#include "ibuf0types.h"

/** Default value for maximum on-disk size of change buffer in terms
of percentage of the buffer pool. */
constexpr uint32_t CHANGE_BUFFER_DEFAULT_SIZE = 25;

#ifndef UNIV_HOTBACKUP
/* Possible operations buffered in the insert/whatever buffer. See
ibuf_insert(). DO NOT CHANGE THE VALUES OF THESE, THEY ARE STORED ON DISK. */
typedef enum {
  IBUF_OP_INSERT = 0,
  IBUF_OP_DELETE_MARK = 1,
  IBUF_OP_DELETE = 2,

  /* Number of different operation types. */
  IBUF_OP_COUNT = 3
} ibuf_op_t;

/** Combinations of operations that can be buffered.
@see innodb_change_buffering_names */
enum ibuf_use_t {
  IBUF_USE_NONE = 0,
  IBUF_USE_INSERT,             /* insert */
  IBUF_USE_DELETE_MARK,        /* delete */
  IBUF_USE_INSERT_DELETE_MARK, /* insert+delete */
  IBUF_USE_DELETE,             /* delete+purge */
  IBUF_USE_ALL                 /* insert+delete+purge */
};

/** Operations that can currently be buffered. */
extern ulong innodb_change_buffering;

/** The insert buffer control structure */
extern ibuf_t *ibuf;

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

/** Creates the insert buffer data structure at a database startup. */
void ibuf_init_at_db_start(void);
/** Updates the max_size value for ibuf. */
void ibuf_max_size_update(ulint new_val); /*!< in: new value in terms of
                                          percentage of the buffer pool size */
/** Reads the biggest tablespace id from the high end of the insert buffer
 tree and updates the counter in fil_system. */
void ibuf_update_max_tablespace_id(void);
/** Starts an insert buffer mini-transaction. */
static inline void ibuf_mtr_start(mtr_t *mtr); /*!< out: mini-transaction */
/** Commits an insert buffer mini-transaction. */
static inline void ibuf_mtr_commit(mtr_t *mtr); /*!< in/out: mini-transaction */

/** Initializes an ibuf bitmap page. */
void ibuf_bitmap_page_init(buf_block_t *block, /*!< in: bitmap page */
                           mtr_t *mtr /*!< in: mtr */);

/** Resets the free bits of the page in the ibuf bitmap. This is done in a
 separate mini-transaction, hence this operation does not restrict
 further work to only ibuf bitmap operations, which would result if the
 latch to the bitmap page were kept.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to decrement or reset the bits in the bitmap in a mini-transaction
 that is committed before the mini-transaction that affects the free
 space. */
void ibuf_reset_free_bits(
    buf_block_t *block); /*!< in: index page; free bits are set to 0
                         if the index is a non-clustered
                         non-unique, and page level is 0 */

/** Updates the free bits of an uncompressed page in the ibuf bitmap if there
is not enough free on the page any more.  This is done in a separate
mini-transaction, hence this operation does not restrict further work to only
ibuf bitmap operations, which would result if the latch to the bitmap page were
kept.  NOTE: The free bits in the insert buffer bitmap must never exceed the
free space on a page.  It is unsafe to increment the bits in a separately
committed mini-transaction, because in crash recovery, the free bits could
momentarily be set too high.  It is only safe to use this function for
decrementing the free bits.  Should more free space become available, we must
not update the free bits here, because that would break crash recovery.
@param[in]      block           index page to which we have added new records;
                                the free bits are updated if the index is
                                non-clustered and non-unique and the page level
                                is 0, and the page becomes fuller
@param[in]      max_ins_size    value of maximum insert size with reorganize
                                before the latest operation performed to the
                                page
@param[in]      increase        upper limit for the additional space used in
                                the latest operation, if known, or
                                ULINT_UNDEFINED */
static inline void ibuf_update_free_bits_if_full(buf_block_t *block,
                                                 ulint max_ins_size,
                                                 ulint increase);

/** Updates the free bits for an uncompressed page to reflect the present
 state.  Does this in the mtr given, which means that the latching
 order rules virtually prevent any further operations for this OS
 thread until mtr is committed.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to set the free bits in the same mini-transaction that updated the
 page. */
void ibuf_update_free_bits_low(const buf_block_t *block, /*!< in: index page */
                               ulint max_ins_size,       /*!< in: value of
                                                         maximum insert size
                                                         with reorganize before
                                                         the latest operation
                                                         performed to the page */
                               mtr_t *mtr);              /*!< in/out: mtr */
/** Updates the free bits for a compressed page to reflect the present
 state.  Does this in the mtr given, which means that the latching
 order rules virtually prevent any further operations for this OS
 thread until mtr is committed.  NOTE: The free bits in the insert
 buffer bitmap must never exceed the free space on a page.  It is safe
 to set the free bits in the same mini-transaction that updated the
 page. */
void ibuf_update_free_bits_zip(buf_block_t *block, /*!< in/out: index page */
                               mtr_t *mtr);        /*!< in/out: mtr */
/** Updates the free bits for the two pages to reflect the present state.
 Does this in the mtr given, which means that the latching order rules
 virtually prevent any further operations until mtr is committed.
 NOTE: The free bits in the insert buffer bitmap must never exceed the
 free space on a page.  It is safe to set the free bits in the same
 mini-transaction that updated the pages. */
void ibuf_update_free_bits_for_two_pages_low(
    buf_block_t *block1, /*!< in: index page */
    buf_block_t *block2, /*!< in: index page */
    mtr_t *mtr);         /*!< in: mtr */

/** A basic partial test if an insert to the insert buffer could be possible
and recommended.
@param[in]      index                   index where to insert
@param[in]      ignore_sec_unique       if != 0, we should ignore UNIQUE
                                        constraint on a secondary index when
                                        we decide*/
static inline bool ibuf_should_try(dict_index_t *index,
                                   ulint ignore_sec_unique);

/** Returns true if the current OS thread is performing an insert buffer
 routine.

 For instance, a read-ahead of non-ibuf pages is forbidden by threads
 that are executing an insert buffer routine.
 @return true if inside an insert buffer routine */
[[nodiscard]] static inline bool ibuf_inside(
    const mtr_t *mtr); /*!< in: mini-transaction */

/** Checks if a page address is an ibuf bitmap page (level 3 page) address.
@param[in]      page_id         page id
@param[in]      page_size       page size
@return true if a bitmap page */
static inline bool ibuf_bitmap_page(const page_id_t &page_id,
                                    const page_size_t &page_size);

/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]      page_id         page id
@param[in]      page_size       page size
@param[in]      x_latch         false if relaxed check (avoid latching the
bitmap page)
@param[in]      location Location where called
@param[in,out]  mtr             mtr which will contain an x-latch to the
bitmap page if the page is not one of the fixed address ibuf pages, or NULL,
in which case a new transaction is created.
@return true if level 2 or level 3 page */
[[nodiscard]] bool ibuf_page_low(const page_id_t &page_id,
                                 const page_size_t &page_size,
                                 IF_DEBUG(bool x_latch, ) ut::Location location,
                                 mtr_t *mtr) MY_ATTRIBUTE((warn_unused_result));

/** Checks if a page is a level 2 or 3 page in the ibuf hierarchy of pages.
Must not be called when recv_no_ibuf_operations==true.
@param[in]      page_id         Tablespace/page identifier
@param[in]      page_size       Page size
@param[in]  location Location where requested
@param[in,out]  mtr             Mini-transaction or NULL
@return true if level 2 or level 3 page */
inline bool ibuf_page(const page_id_t &page_id, const page_size_t &page_size,
                      ut::Location location, mtr_t *mtr) {
  return ibuf_page_low(page_id, page_size, IF_DEBUG(true, ) location, mtr);
}

/** Frees excess pages from the ibuf free list. This function is called when an
OS thread calls fsp services to allocate a new file segment, or a new page to a
file segment, and the thread did not own the fsp latch before this call. */
void ibuf_free_excess_pages(void);

/** Buffer an operation in the insert/delete buffer, instead of doing it
directly to the disk page, if this is possible. Does not do it if the index
is clustered or unique.
@param[in]      op              operation type
@param[in]      entry           index entry to insert
@param[in,out]  index           index where to insert
@param[in]      page_id         page id where to insert
@param[in]      page_size       page size
@param[in,out]  thr             query thread
@return true if success */
bool ibuf_insert(ibuf_op_t op, const dtuple_t *entry, dict_index_t *index,
                 const page_id_t &page_id, const page_size_t &page_size,
                 que_thr_t *thr);

/** When an index page is read from a disk to the buffer pool, this function
applies any buffered operations to the page and deletes the entries from the
insert buffer. If the page is not read, but created in the buffer pool, this
function deletes its buffered entries from the insert buffer; there can
exist entries for such a page if the page belonged to an index which
subsequently was dropped.
@param[in,out]  block                   if page has been read from disk,
pointer to the page x-latched, else NULL
@param[in]      page_id                 page id of the index page
@param[in]      update_ibuf_bitmap      normally this is set to true, but
if we have deleted or are deleting the tablespace, then we naturally do not
want to update a non-existent bitmap page
@param[in]      page_size               page size */
void ibuf_merge_or_delete_for_page(buf_block_t *block, const page_id_t &page_id,
                                   const page_size_t *page_size,
                                   bool update_ibuf_bitmap);

/** Deletes all entries in the insert buffer for a given space id. This is used
in DISCARD TABLESPACE and IMPORT TABLESPACE.
NOTE: this does not update the page free bitmaps in the space. The space will
become CORRUPT when you call this function! */
void ibuf_delete_for_discarded_space(space_id_t space); /*!< in: space id */
/** Contract the change buffer by reading pages to the buffer pool.
@param[in]      full            If true, do a full contraction based on
PCT_IO(100). If false, the size of contract batch is determined based on the
current size of the change buffer.
@return a lower limit for the combined size in bytes of entries which will be
merged from ibuf trees to the pages read, 0 if ibuf is empty */
ulint ibuf_merge_in_background(bool full);

/** Contracts insert buffer trees by reading pages referring to space_id
to the buffer pool.
@returns number of pages merged.*/
ulint ibuf_merge_space(space_id_t space); /*!< in: space id */

#endif /* !UNIV_HOTBACKUP */
/** Parses a redo log record of an ibuf bitmap page init.
@param[in] ptr Buffer.
@param[in] end_ptr Buffer end.
@param[in] block Block or nullptr.
@param[in] mtr MTR or nullptr.
@return end of log record or NULL */
const byte *ibuf_parse_bitmap_init(const byte *ptr, const byte *end_ptr,
                                   buf_block_t *block, mtr_t *mtr);
#ifndef UNIV_HOTBACKUP
#ifdef UNIV_IBUF_COUNT_DEBUG

/** Gets the ibuf count for a given page.
@param[in]      page_id page id
@return number of entries in the insert buffer currently buffered for
this page */
ulint ibuf_count_get(const page_id_t &page_id);

#endif /* UNIV_IBUF_COUNT_DEBUG */

/** Looks if the insert buffer is empty.
 @return true if empty */
bool ibuf_is_empty(void);
/** Prints info of ibuf. */
void ibuf_print(FILE *file); /*!< in: file where to print */
/********************************************************************
Read the first two bytes from a record's fourth field (counter field in new
records; something else in older records).
@return "counter" field, or ULINT_UNDEFINED if for some reason it can't be read
*/
ulint ibuf_rec_get_counter(const rec_t *rec); /*!< in: ibuf record */

/** Determine if there is any multi-value field data on the change buffer
record
@param[in]      rec     ibuf record
@return true if there is any multi-value field in the record */
bool ibuf_rec_has_multi_value(const rec_t *rec);

/** Closes insert buffer and frees the data structures. */
void ibuf_close(void);

/** Checks the insert buffer bitmaps on IMPORT TABLESPACE.
 @return DB_SUCCESS or error code */
[[nodiscard]] dberr_t ibuf_check_bitmap_on_import(
    const trx_t *trx,     /*!< in: transaction */
    space_id_t space_id); /*!< in: tablespace identifier */

/** Updates free bits and buffered bits for bulk loaded page.
@param[in]      block   index page
@param[in]      reset   flag if reset free val */
void ibuf_set_bitmap_for_bulk_load(buf_block_t *block, bool reset);

constexpr uint32_t IBUF_HEADER_PAGE_NO = FSP_IBUF_HEADER_PAGE_NO;
constexpr uint32_t IBUF_TREE_ROOT_PAGE_NO = FSP_IBUF_TREE_ROOT_PAGE_NO;

#endif /* !UNIV_HOTBACKUP */

/* The ibuf header page currently contains only the file segment header
for the file segment from which the pages for the ibuf tree are allocated */
constexpr uint32_t IBUF_HEADER = PAGE_DATA;
/** fseg header for ibuf tree */
constexpr uint32_t IBUF_TREE_SEG_HEADER = 0;

#include "ibuf0ibuf.ic"

#endif
