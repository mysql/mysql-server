/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file row0pread.h
Parallel read interface.

Created 2018-01-27 by Sunny Bains. */

#ifndef row0par_read_h
#define row0par_read_h

#include <functional>
#include <vector>

#include "univ.i"

// Forward declarations
struct trx_t;
struct mtr_t;
struct btr_pcur_t;
struct buf_block_t;
struct dict_table_t;

#include "db0err.h"
#include "fil0fil.h"
#include "os0event.h"
#include "page0size.h"
#include "rem0types.h"
#include "row0mysql.h"
#include "ut0mpmcbq.h"

/** Start row of the scan range. */
struct Key_reader_row {
  /** Heap used to allocate m_rec, m_tuple and m_pcur. */
  mem_heap_t *m_heap{};

  /** Starting page number. */
  page_no_t m_page_no{FIL_NULL};

  /** m_rec column offsets. */
  const ulint *m_offsets{};

  /** Start scanning from this key. */
  const rec_t *m_rec{};

  /** Column pointers inside m_rec. */
  const dtuple_t *m_tuple;

  /** Number of externally stored columns. */
  ulint m_n_ext{ULINT_UNDEFINED};

  /** Persistent cursor.*/
  btr_pcur_t *m_pcur{};
};

/** Parallel read implementation. */
template <typename T, typename R>
class Reader {
 public:
  /** Callback to process the rows. */
  using Function =
      std::function<dberr_t(size_t thread_id, const buf_block_t *,
                            const rec_t *, dict_index_t *, row_prebuilt_t *)>;

  /** Constructor.
  @param[in]    table        Table to be read.
  @param[in]    trx          Transaction used for parallel read.
  @param[in]    index        Index in table to scan.
  @param[in]    prebuilt     row prebuilt structure
  @param[in]    n_threads    Maximum threads to use for reading. */
  Reader(dict_table_t *table, trx_t *trx, dict_index_t *index,
         row_prebuilt_t *prebuilt, size_t n_threads);

  /** Destructor. */
  virtual ~Reader();

  /** Calculate number of threads that would be spawned for the parallel read.
  @return number of threads
  @retval 0 if the index is empty */
  virtual size_t calc_num_threads() {
    if (!m_partitions.empty()) {
      return (std::min(m_partitions.size(), m_n_threads));
    }

    m_partitions = partition();

    return (std::min(m_partitions.size(), m_n_threads));
  }

  /** Spawn threads and start the parallel load.
  @param[in]    f   Callback to process the rows */
  virtual void start_parallel_load(Function f);

  /** Start the threads to do the parallel read.
  @param[in,out]  f    Callback to process the rows.
  @return error code. */
  virtual dberr_t read(Function &&f) MY_ATTRIBUTE((warn_unused_result));

  /** @return the transaction instance. */
  trx_t *trx() MY_ATTRIBUTE((warn_unused_result)) { return (m_trx); }

  /** @return the index we are scanning. */
  dict_index_t *index() MY_ATTRIBUTE((warn_unused_result)) { return (m_index); }

  /** @return the table we are working on. */
  dict_table_t *table() MY_ATTRIBUTE((warn_unused_result)) { return (m_table); }

  /** Fill the members.
  @param[in]    table   InnoDB partitioned table
  @param[in]    index   InnoDB index of the partition
  @param[in]    trx     InnoDB trx of the partition
  @param[in]    prebuilt        InnoDB row prebuilt structure of the partition
*/
  virtual void set_info(dict_table_t *table, dict_index_t *index, trx_t *trx,
                        row_prebuilt_t *prebuilt) {
    m_table = table;
    m_index = index;
    m_trx = trx;
    m_prebuilt = prebuilt;
    if (table != nullptr) {
      page_size_t page_size(dict_tf_to_fsp_flags(table->flags));
      m_page_size.copy_from(page_size);
    }
  }

  /** @return the max number of threads specified for scanning. */
  size_t n_threads() MY_ATTRIBUTE((warn_unused_result)) {
    return (m_n_threads);
  }

  // Disable copying.
  Reader(const Reader &) = delete;
  Reader(const Reader &&) = delete;
  Reader &operator=(Reader &&) = delete;
  Reader &operator=(const Reader &) = delete;

 protected:
  /** The first cursor should read up to the second cursor. */
  using Range = std::pair<R, R>;

 private:
  // Forward declaration.
  struct Ctx;

  using Contexts = std::vector<Ctx *, ut_allocator<Ctx *>>;
  using Pages = std::vector<page_no_t, ut_allocator<page_no_t>>;
  using Ranges = std::vector<Range, ut_allocator<Range>>;

  /** The sub-trees to scan. Each element (i) of the vector represents the
  subtree of level 'i' and consists of all the pages traversed from the root
  of the subtree to the leaf. */
  using Subtrees = std::vector<Pages, ut_allocator<Pages>>;

  using Queue = mpmc_bq<Ctx *>;

  /** Fetch a block from the buffer pool and acquire an S latch on it.
  @param[in]      page_id    Page ID.
  @param[in,out]  mtr        Mini transaction covering the fetch.
  @return the block fetched from the buffer pool. */
  buf_block_t *block_get_s_latched(const page_id_t &page_id, mtr_t *mtr) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Poll for requests and execute.
  @param[in]      id         Thread ID
  @param[in,out]  ctxq       Queue with requests.
  @param[in]      f          Callback function. */
  virtual dberr_t worker(size_t id, Queue &ctxq, Function &f);

  /** Iterate over the linked list of blocks of internal nodes starting
  at the given page number until the end.
  @param[in]      page_no    Iteration starting point.
  @param[in,out]  subtrees   Subtrees in the internal node pointers. */
  void iterate_internal_blocks(page_no_t page_no, Subtrees &subtrees) const;

  /** Partition the BTree for parallel read.
  @return the partition scan ranges. */
  Ranges partition() MY_ATTRIBUTE((warn_unused_result));

  /** Find the page number of the left most "pointer" in an internal node.
  @param[in]  block       Page to look in.
  @return the left child page number. */
  page_no_t left_child(const buf_block_t *block) const
      MY_ATTRIBUTE((warn_unused_result));

  /** Traverse from given sub-tree page number to left most leaf page
  from the given page number.
  @param[in]  page_no    Page number of sub-tree.
  @param[in,out]  mtr    Mini-transaction.
  @return nodes traversed to get to the leaf. */
  Pages left_leaf(page_no_t page_no, mtr_t *mtr) const
      MY_ATTRIBUTE((warn_unused_result));

 private:
  /** Table to read. */
  dict_table_t *m_table{};

  /** Cluster index on m_table. */
  dict_index_t *m_index{};

  row_prebuilt_t *m_prebuilt{};

  /** True if compact row format. */
  bool m_is_compact{};

  /** Trasaction covering the read. */
  trx_t *m_trx{};

  /** Context per thread. */
  Contexts m_ctxs{};

  /** Partition scan ranges. */
  Ranges m_partitions{};

  /** The data page size. */
  page_size_t m_page_size;

  /** Maximum threads to use for the parallel read. */
  size_t m_n_threads{};

  /** For signalling worker threads about events. */
  os_event_t m_event{};

  /** Total tasks executed so far. */
  std::atomic_size_t m_n_completed{};

  friend T;
  friend class Parallel_reader_adapter;
  friend class Parallel_partition_reader_adapter;
};

/** Traverse an index in the leaf page block list order. */
class Phy_reader : public Reader<Phy_reader, page_no_t> {
  using Row = page_no_t;
  using Ctx = Reader<Phy_reader, Row>::Ctx;
  using F = Reader<Phy_reader, Row>::Function;
  using Ranges = Reader<Phy_reader, Row>::Ranges;

  /** The sub-trees to scan. Row is the page_no of a leaf node from where
  the scan will start for that sub-tree. */
  using Subtrees = Reader<Phy_reader, Row>::Subtrees;

 public:
  /** Constructor.
  @param[in]    table        Table to be read
  @param[in]    trx          Transaction used for parallel read
  @param[in]    index        Index in table to scan.
  @param[in]    prebuilt     row prebuilt structure
  @param[in]    n_threads    Maximum threads to use for reading */
  Phy_reader(dict_table_t *table, trx_t *trx, dict_index_t *index,
             row_prebuilt_t *prebuilt, size_t n_threads)
      : Reader<Phy_reader, Row>(table, trx, index, prebuilt, n_threads) {}

  /** Destructor. */
  ~Phy_reader();

 private:
  /** Traverse the pages by leaf block list order.
  @param[in]      id         Thread ID.
  @param[in]      ctx        Thread context.
  @param[in]      f          Callback function.
  @return DB_SUCCESS or error code. */
  dberr_t traverse(size_t id, Ctx &ctx, F &f)
      MY_ATTRIBUTE((warn_unused_result));

  /** Create the ranges that will be used by the threads to iterate over
  the index.
  @param[in]    subtrees     Subtree root nodes starting at the selected
                             Btree level.
  @return the ranges to read at the leaf level. */
  Ranges create_ranges(const Subtrees &subtrees)
      MY_ATTRIBUTE((warn_unused_result));

  /** Iterate over the records in a block. Skip the delete mark records and
  call 'f' for the non-delete marked records.
  @return the next block number to traverse. */
  page_no_t iterate_recs(size_t id, Ctx &ctx, mtr_t *mtr,
                         const buf_block_t *block, F &f)
      MY_ATTRIBUTE((warn_unused_result));

  friend Reader<Phy_reader, Row>;
};

template <typename T, typename R>
struct Reader<T, R>::Ctx {
  /** Constructor.
  @param[in]    id      Thread ID.
  @param[in]    range   Range that the thread has to read.
  @param[in]	table   table that the thread has to read
  @param[in]    index	index that the thread has to read
  @param[in]	trx		trx to use for reading
  @param[in]	prebuilt	row prebuilt structure */
  Ctx(size_t id, const Range &range, dict_table_t *table, dict_index_t *index,
      trx_t *trx, row_prebuilt_t *prebuilt)
      : m_id(id),
        m_range(range),
        m_table(table),
        m_index(index),
        m_trx(trx),
        m_prebuilt(prebuilt) {}

  /** Destructor. */
  ~Ctx();

  /** Destroy the persistent cursor. */
  static void destroy(R &row);

  /** Context ID. */
  size_t m_id{std::numeric_limits<size_t>::max()};

  /** Error during parallel read. */
  dberr_t m_err{DB_SUCCESS};

  /** Range to read in this contxt. */
  Range m_range{};

  /** Table to which the index belongs to.
  @note this is mainly required by an adapter where parallel scan can happen for
  partitioned tables as well and each thread could be working on different
  partitions and hence different index. */
  dict_table_t *m_table;

  /** Index to which the records in the range belong to.
  @note this is mainly required by an adapter where parallel scan can happen for
  partitioned tables as well and each thread could be working on different
  partitions and hence different index. */
  dict_index_t *m_index;

  /** Transaction used for parallel read of the index.
  @note this is mainly required by an adapter where parallel scan can happen for
  partitioned tables as well and each thread could be working on different
  partitions and hence different index. */
  trx_t *m_trx;

  /** Row prebuilt structure. */
  row_prebuilt_t *m_prebuilt;
};

/** Traverse an index using a persistent cursor in key order. */
class Key_reader : public Reader<Key_reader, Key_reader_row> {
  using Row = Key_reader_row;
  using Ctx = Reader<Key_reader, Row>::Ctx;
  using F = Reader<Key_reader, Row>::Function;
  using Ranges = Reader<Key_reader, Row>::Ranges;

  /** The subtrees to scan. Row is the cursor on the leaf page from where
  the scan will start. */
  using Subtrees = Reader<Key_reader, Row>::Subtrees;

 public:
  /** Constructor.
  @param[in]    table        Table to be read
  @param[in]    trx          Transaction used for parallel read
  @param[in]    index        Index in table to scan.
  @param[in]    prebuilt     row prebuilt structure
  @param[in]    n_threads    Maximum threads to use for reading */
  Key_reader(dict_table_t *table, trx_t *trx, dict_index_t *index,
             row_prebuilt_t *prebuilt, size_t n_threads)
      : Reader<Key_reader, Row>(table, trx, index, prebuilt, n_threads) {}

  /** Destructor. */
  ~Key_reader();

 private:
  /** Create the ranges that will be used by the threads to iterate over
  the index.
  @param[in]    subtrees     Subtree root nodes starting at the selected
                             Btree level.
  @return the ranges to read at the leaf level. */
  Ranges create_ranges(const Subtrees &subtrees);

  /** Build a dtuple_t from rec_t.
  @param[in]      rec         Build the dtuple from this record.
  @param[in,out]  row         Build in this row.
  @param[in]      copy        If true then make a copy of rec. */
  void build_row(const rec_t *rec, Row &row, bool copy);

  /** Open the cursor that will be used to traverse the partition and
  position on the the start row.
  @param[in]  page_no   Starting page number.
  @return Start row. */
  Row open_cursor(page_no_t page_no) MY_ATTRIBUTE((warn_unused_result));

  /** Build an old version of the row if required.
  @param[in,out]  rec       Current row read from the index. This can be modifie
                            by this method if an older version needs to be
                            built.
  @param[in,out]  offsets   Same as above but pertains to the rec offsets
  @param[in,out]  heap      Heap to use if a previous version needs to be
                            built from the undo log.
  @param[in,out]  mtr       Mini transaction covering the read.
  @param[in]	  table	    table the index belongs to
  @param[in]	  index	    index the record belongs to
  @param[in]	  trx	    transcation handle
  @return on success. */
  bool check_visibility(const rec_t *&rec, ulint *&offsets, mem_heap_t *&heap,
                        mtr_t *mtr, dict_table_t *table, dict_index_t *index,
                        trx_t *trx) MY_ATTRIBUTE((warn_unused_result));

  /** Traverse the pages by key order.
  @param[in]      id         Thread ID.
  @param[in]      ctx        Thread context.
  @param[in]      f          Callback function.
  @return DB_SUCCESS or error code. */
  dberr_t traverse(size_t id, Ctx &ctx, F &f)
      MY_ATTRIBUTE((warn_unused_result));

  friend Reader<Key_reader, Row>;
  friend class Parallel_reader_adapter;
  friend class Parallel_partition_reader_adapter;
};

#endif /* !row0par_read_h */
