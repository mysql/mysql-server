/*****************************************************************************

Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

/** @file include/btr0load.h
 The B-tree bulk load

 Created 03/11/2014 Shaohua Wang
 *************************************************************************/

#ifndef btr0load_h
#define btr0load_h

#include <stddef.h>
#include <vector>

#include "dict0dict.h"
#include "page0cur.h"
#include "ut0class_life_cycle.h"
#include "ut0new.h"

// Forward declaration.
class Page_load;

/** @note We should call commit(false) for a Page_load object, which is not in
m_page_loaders after page_commit, and we will commit or abort Page_load
objects in function "finish". */
class Btree_load : private ut::Non_copyable {
 public:
  /** Interface to consume from. */
  struct Cursor {
    /** Constructor. */
    Cursor() = default;

    /** Destructor. */
    virtual ~Cursor() = default;

    /** Fetch the current row as a tuple.
    @param[out] dtuple          Row represented as a tuple.
    @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
    [[nodiscard]] virtual dberr_t fetch(dtuple_t *&dtuple) noexcept = 0;

    /** @return true if duplicates detected. */
    virtual bool duplicates_detected() const noexcept = 0;

    /** Move to the next record.
    @return DB_SUCCESS, DB_END_OF_INDEX or error code. */
    [[nodiscard]] virtual dberr_t next() noexcept = 0;
  };

 public:
  using Page_loaders = std::vector<Page_load *, ut::allocator<Page_load *>>;

  /** Constructor
  @param[in]    index                     B-tree index.
  @param[in]    trx_id                  Transaction id.
  @param[in]    observer                Flush observer */
  Btree_load(dict_index_t *index, trx_id_t trx_id,
             Flush_observer *observer) noexcept;

  /** Destructor */
  ~Btree_load() noexcept;

  /** Load the btree from the cursor.
  @param[in,out] cursor         Cursor to read tuples from.
  @return DB_SUCCESS or error code. */
  [[nodiscard]] dberr_t build(Cursor &cursor) noexcept;

  /** Btree bulk load finish. We commit the last page in each level
  and copy the last page in top level to the root page of the index
  if no error occurs.
  @param[in]    err                   Whether bulk load was successful until now
  @return error code  */
  [[nodiscard]] dberr_t finish(dberr_t err) noexcept;

  /** Release latch on the rightmost leaf page in the index tree. */
  void release() noexcept;

  /** Re-latch latch on the rightmost leaf page in the index tree. */
  void latch() noexcept;

  /** Insert a tuple to a page in a level
  @param[in] dtuple             Tuple to insert
  @param[in] level                  B-tree level
  @return error code */
  [[nodiscard]] dberr_t insert(dtuple_t *dtuple, size_t level) noexcept;

 private:
  /** Set the root page on completion.
  @param[in] last_page_no       Last page number (the new root).
  @return DB_SUCCESS or error code. */
  dberr_t load_root_page(page_no_t last_page_no) noexcept;

  /** Split a page
  @param[in]    page_load               Page to split
  @param[in]    next_page_load    Next page
  @return       error code */
  [[nodiscard]] dberr_t page_split(Page_load *page_load,
                                   Page_load *next_page_load) noexcept;

  /** Commit(finish) a page. We set next/prev page no, compress a page of
  compressed table and split the page if compression fails, insert a node
  pointer to father page if needed, and commit mini-transaction.
  @param[in]    page_load               Page to commit
  @param[in]    next_page_load    Next page
  @param[in]    insert_father       Flag whether need to insert node ptr
  @return       error code */
  [[nodiscard]] dberr_t page_commit(Page_load *page_load,
                                    Page_load *next_page_load,
                                    bool insert_father) noexcept;

  /** Prepare space to insert a tuple.
  @param[in,out] page_load      Page bulk that will be used to store the record.
                                It may be replaced if there is not enough space
                                to hold the record.
  @param[in]  level             B-tree level
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t prepare_space(Page_load *&page_load, size_t level,
                                      size_t rec_size) noexcept;

  /** Insert a tuple to a page.
  @param[in]  page_load         Page bulk object
  @param[in]  tuple             Tuple to insert
  @param[in]  big_rec           Big record vector, maybe NULL if there is no
                                Data to be stored externally.
  @param[in]  rec_size          Record size
  @return error code */
  [[nodiscard]] dberr_t insert(Page_load *page_load, dtuple_t *tuple,
                               big_rec_t *big_rec, size_t rec_size) noexcept;

  /** Log free check */
  void log_free_check() noexcept;

  /** Btree page bulk load finish. Commits the last page in each level
  if no error occurs. Also releases all page bulks.
  @param[in]  err               Whether bulk load was successful until now
  @param[out] last_page_no      Last page number
  @return error code  */
  [[nodiscard]] dberr_t finalize_page_loads(dberr_t err,
                                            page_no_t &last_page_no) noexcept;

 private:
  /** Number of records inserted. */
  uint64_t m_n_recs{};

  /** B-tree index */
  dict_index_t *m_index{};

  /** Transaction id */
  trx_id_t m_trx_id{};

  /** Root page level */
  size_t m_root_level{};

  /** Flush observer */
  Flush_observer *m_flush_observer{};

  /** Page cursor vector for all level */
  Page_loaders m_page_loaders{};

  /** State of the index. Used for asserting at the end of a
  bulk load operation to ensure that the online status of the
  index does not change */
  IF_DEBUG(unsigned m_index_online{};)
};

#endif /* btr0load_h */
