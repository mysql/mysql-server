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

/** @file include/btr0bulk.h
 The B-tree bulk load

 Created 03/11/2014 Shaohua Wang
 *************************************************************************/

#ifndef btr0bulk_h
#define btr0bulk_h

#include <stddef.h>
#include <vector>

#include "dict0dict.h"
#include "page0cur.h"
#include "ut0new.h"

/** Innodb B-tree index fill factor for bulk load. */
extern long innobase_fill_factor;

/*
The proper function call sequence of PageBulk is as below:
-- PageBulk::init
-- PageBulk::insert
-- PageBulk::finish
-- PageBulk::compress(COMPRESSED table only)
-- PageBulk::pageSplit(COMPRESSED table only)
-- PageBulk::commit
*/

class PageBulk {
 public:
  /** Page split point descriptor. */
  struct SplitPoint {
    /** Record being the point of split.
     * All records before this record should stay on current on page.
     * This record and all following records should be moved to new page. */
    rec_t *m_rec;
    /** Number of records before this record. */
    ulint m_n_rec_before;
  };

  /** Constructor
  @param[in]	index		B-tree index
  @param[in]	page_no		page number
  @param[in]	level		page level
  @param[in]	trx_id		transaction id
  @param[in]	observer	flush observer */
  PageBulk(dict_index_t *index, trx_id_t trx_id, page_no_t page_no, ulint level,
           FlushObserver *observer)
      : m_heap(nullptr),
        m_index(index),
        m_mtr(nullptr),
        m_trx_id(trx_id),
        m_block(nullptr),
        m_page(nullptr),
        m_page_zip(nullptr),
        m_cur_rec(nullptr),
        m_page_no(page_no),
        m_level(level),
        m_is_comp(dict_table_is_comp(index->table)),
        m_heap_top(nullptr),
        m_rec_no(0),
        m_free_space(0),
        m_reserved_space(0),
        m_padding_space(0),
#ifdef UNIV_DEBUG
        m_total_data(0),
#endif /* UNIV_DEBUG */
        m_modify_clock(0),
        m_flush_observer(observer),
        m_last_slotted_rec(nullptr),
        m_slotted_rec_no(0),
        m_modified(false) {
    ut_ad(!dict_index_is_spatial(m_index));
  }

  /** Destructor */
  ~PageBulk() {
    if (m_heap) {
      mem_heap_free(m_heap);
    }
  }

  /** Initialize members and allocate page if needed and start mtr.
  @note Must be called and only once right after constructor.
  @return error code */
  dberr_t init() MY_ATTRIBUTE((warn_unused_result));

  /** Insert a tuple in the page.
  @param[in]  tuple     tuple to insert
  @param[in]  big_rec   external record
  @param[in]  rec_size  record size
  @param[in]  n_ext     number of externally stored columns
  @return error code */
  dberr_t insert(const dtuple_t *tuple, const big_rec_t *big_rec,
                 ulint rec_size, ulint n_ext)
      MY_ATTRIBUTE((warn_unused_result));

  /** Mark end of insertion to the page. Scan records to set page dirs,
  and set page header members. The scan is incremental (slots and records
  which assignment could be "finalized" are not checked again. Check the
  m_slotted_rec_no usage, note it could be reset in some cases like
  during split.
  Note: we refer to page_copy_rec_list_end_to_created_page. */
  void finish();

  /** Commit mtr for a page
  @param[in]	success		Flag whether all inserts succeed. */
  void commit(bool success);

  /** Compress if it is compressed table
  @return	true	compress successfully or no need to compress
  @return	false	compress failed. */
  bool compress() MY_ATTRIBUTE((warn_unused_result));

  /** Check whether the record needs to be stored externally.
  @return false if the entire record can be stored locally on the page */
  bool needExt(const dtuple_t *tuple, ulint rec_size) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Get node pointer
  @return node pointer */
  dtuple_t *getNodePtr();

  /** Split the page records between this and given bulk.
   * @param new_page_bulk  The new bulk to store split records. */
  void split(PageBulk &new_page_bulk);

  /** Copy all records from page.
  @param[in]  src_page  Page with records to copy. */
  void copyAll(const page_t *src_page);

  /** Set next page
  @param[in]	next_page_no	next page no */
  void setNext(page_no_t next_page_no);

  /** Set previous page
  @param[in]	prev_page_no	previous page no */
  void setPrev(page_no_t prev_page_no);

  /** Release block by committing mtr */
  inline void release();

  /** Start mtr and latch block */
  inline void latch();

  /** Check if required space is available in the page for the rec
  to be inserted.	We check fill factor & padding here.
  @param[in]	rec_size	required space
  @return true	if space is available */
  inline bool isSpaceAvailable(ulint rec_size) const;

  /** Get page no */
  page_no_t getPageNo() const { return (m_page_no); }

  /** Get page level */
  ulint getLevel() const { return (m_level); }

  /** Get record no */
  ulint getRecNo() const { return (m_rec_no); }

  /** Get page */
  const page_t *getPage() const { return (m_page); }

  /** Check if table is compressed.
  @return true if table is compressed, false otherwise. */
  bool isTableCompressed() const { return (m_page_zip != nullptr); }

 private:
  /** Get page split point. We split a page in half when compression
  fails, and the split record and all following records should be copied
  to the new page.
  @return split record descriptor */
  SplitPoint getSplitRec();

  /** Copy given and all following records.
  @param[in]  first_rec  first record to copy */
  void copyRecords(const rec_t *first_rec);

  /** Remove all records after split rec including itself.
  @param[in]  split_point  split point descriptor */
  void splitTrim(const SplitPoint &split_point);

  /** Insert a record in the page.
  @param[in]  rec   record
  @param[in]  offsets   record offsets */
  void insert(const rec_t *rec, ulint *offsets);

  /** Store external record
  Since the record is not logged yet, so we don't log update to the record.
  the blob data is logged first, then the record is logged in bulk mode.
  @param[in]  big_rec   external record
  @param[in]  offsets   record offsets
  @return error code */
  dberr_t storeExt(const big_rec_t *big_rec, ulint *offsets)
      MY_ATTRIBUTE((warn_unused_result));

  /** Memory heap for internal allocation */
  mem_heap_t *m_heap;

  /** The index B-tree */
  dict_index_t *m_index;

  /** The min-transaction */
  mtr_t *m_mtr;

  /** The transaction id */
  trx_id_t m_trx_id;

  /** The buffer block */
  buf_block_t *m_block;

  /** The page */
  page_t *m_page;

  /** The page zip descriptor */
  page_zip_des_t *m_page_zip;

  /** The current rec, just before the next insert rec */
  rec_t *m_cur_rec;

  /** The page no */
  page_no_t m_page_no;

  /** The page level in B-tree */
  ulint m_level;

  /** Flag: is page in compact format */
  const bool m_is_comp;

  /** The heap top in page for next insert */
  byte *m_heap_top;

  /** User record no */
  ulint m_rec_no;

  /** The free space left in the page */
  ulint m_free_space;

  /** The reserved space for fill factor */
  ulint m_reserved_space;

  /** The padding space for compressed page */
  ulint m_padding_space;

#ifdef UNIV_DEBUG
  /** Total data in the page */
  ulint m_total_data;
#endif /* UNIV_DEBUG */

  /** The modify clock value of the buffer block
  when the block is re-pinned */
  ib_uint64_t m_modify_clock;

  /** Flush observer */
  FlushObserver *m_flush_observer;

  /** Last record assigned to a slot. */
  rec_t *m_last_slotted_rec;

  /** Number of records assigned to slots. */
  ulint m_slotted_rec_no;

  /** Page modified flag. */
  bool m_modified;
};

class BtrBulk {
 public:
  using page_bulk_vector = std::vector<PageBulk *, ut_allocator<PageBulk *>>;

  /** Constructor
  @param[in]	index		B-tree index
  @param[in]	trx_id		transaction id
  @param[in]	observer	flush observer */
  BtrBulk(dict_index_t *index, trx_id_t trx_id, FlushObserver *observer);

  /** Destructor */
  ~BtrBulk();

  /** Initialization
  @note Must be called right after constructor. */
  dberr_t init() MY_ATTRIBUTE((warn_unused_result));

  /** Insert a tuple
  @param[in]	tuple	tuple to insert.
  @return error code */
  dberr_t insert(dtuple_t *tuple) MY_ATTRIBUTE((warn_unused_result)) {
    return (insert(tuple, 0));
  }

  /** Btree bulk load finish. We commit the last page in each level
  and copy the last page in top level to the root page of the index
  if no error occurs.
  @param[in]	err	whether bulk load was successful until now
  @return error code  */
  dberr_t finish(dberr_t err) MY_ATTRIBUTE((warn_unused_result));

  /** Release all latches */
  void release();

  /** Re-latch all latches */
  void latch();

 private:
  /** Insert a tuple to a page in a level
  @param[in]	tuple	tuple to insert
  @param[in]	level	B-tree level
  @return error code */
  dberr_t insert(dtuple_t *tuple, ulint level)
      MY_ATTRIBUTE((warn_unused_result));

  /** Split a page
  @param[in]	page_bulk	page to split
  @param[in]	next_page_bulk	next page
  @return	error code */
  dberr_t pageSplit(PageBulk *page_bulk, PageBulk *next_page_bulk)
      MY_ATTRIBUTE((warn_unused_result));

  /** Commit(finish) a page. We set next/prev page no, compress a page of
  compressed table and split the page if compression fails, insert a node
  pointer to father page if needed, and commit mini-transaction.
  @param[in]	page_bulk	page to commit
  @param[in]	next_page_bulk	next page
  @param[in]	insert_father	flag whether need to insert node ptr
  @return	error code */
  dberr_t pageCommit(PageBulk *page_bulk, PageBulk *next_page_bulk,
                     bool insert_father) MY_ATTRIBUTE((warn_unused_result));

  /** Abort a page when an error occurs
  @param[in]	page_bulk	page bulk object
  @note We should call pageAbort for a PageBulk object, which is not in
  m_page_bulks after pageCommit, and we will commit or abort PageBulk
  objects in function "finish". */
  void pageAbort(PageBulk *page_bulk) { page_bulk->commit(false); }

  /** Prepare space to insert a tuple.
  @param[in,out]  page_bulk   page bulk that will be used to store the record.
                              It may be replaced if there is not enough space
                              to hold the record.
  @param[in]  level           B-tree level
  @param[in]  rec_size        record size
  @return error code */
  dberr_t prepareSpace(PageBulk *&page_bulk, ulint level, ulint rec_size)
      MY_ATTRIBUTE((warn_unused_result));

  /** Insert a tuple to a page.
  @param[in]  page_bulk   page bulk object
  @param[in]  tuple       tuple to insert
  @param[in]  big_rec     big record vector, maybe NULL if there is no
                          data to be stored externally.
  @param[in]  rec_size    record size
  @param[in]  n_ext       number of externally stored columns
  @return error code */
  dberr_t insert(PageBulk *page_bulk, dtuple_t *tuple, big_rec_t *big_rec,
                 ulint rec_size, ulint n_ext)
      MY_ATTRIBUTE((warn_unused_result));

  /** Log free check */
  void logFreeCheck();

  /** Btree page bulk load finish. Commits the last page in each level
  if no error occurs. Also releases all page bulks.
  @param[in]  err           whether bulk load was successful until now
  @param[out] last_page_no  last page number
  @return error code  */
  dberr_t finishAllPageBulks(dberr_t err, page_no_t &last_page_no)
      MY_ATTRIBUTE((warn_unused_result));

 private:
  /** B-tree index */
  dict_index_t *m_index;

  /** Transaction id */
  trx_id_t m_trx_id;

  /** Root page level */
  ulint m_root_level;

  /** Flush observer */
  FlushObserver *m_flush_observer;

  /** Page cursor vector for all level */
  page_bulk_vector *m_page_bulks;
};

#endif
