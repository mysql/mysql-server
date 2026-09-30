/** @file include/row0pread-range.h
Parallel read for (multiple) range query. */

#ifndef row0pread_range_h
#define row0pread_range_h

#include <cstddef>
#include <utility>
#include <vector>

#include "data0data.h"
#include "db0err.h"
#include "dict0mem.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "row0pread.h"
#include "kmeans0searcher.h"
#include "kmeans0table.h"
#include "kmeans0types.h"
#include "trx0trx.h"

/** parallel Reader SubTable
SubTableReader aims to serve the need for (multiple) range scans. Each scan
constitututes scanning a given partition.

This implementation assigns one thread for one range, in contrast to the base
implementation where there are usually much more slices than threads, and the
slices-thread mapping is FCFS.

The base implementation provides interface as below, which practically only
supports full table scan.
  Parallel_reader reader(n_threads);
  const Parallel_reader::Scan_range FULL_SCAN;
  Parallel_reader::Config config(FULL_SCAN, index);
  reader.add_scan(trx, config, callback);
  reader.run(n_threads);

With this range reader, the interface is slightly modified to signify the fact
that each range is assigned to 1 thread (therefore no further slicing).
  SubTableReader reader(n_threads);
  std::vector<std::pair<const dtuple_t*, const dtuple_t*>> ranges;
  ranges.push_back(std::make_pair(begin_tuple, end_tuple));
  ...
  reader.add_ranges(trx, index, begin_tuple, end_tuple, callback);
  reader.run(n_threads);

The main idea is to add all ranges in one single index locking period and then
spread the slices to worker threads in a later run() call. */
class SubTableReader : public Parallel_reader {
 public:
  /** Forward declarations of nested classes. */
  class SubTableCtx;
  class SubTableScanCtx;

  /** Constructor
  @param[in]    searcher           the searcher that owns this reader  */
  SubTableReader(const ib_vector::KMeansSearcher& searcher);

  /** Add ranges for parallel scan
  @param[in]    trx               the carry trx
  @param[in]    index             the index to be used in the scan
  @param[in]    ranges            the ranges for scan
  @param[in]    callback          the callback on each row scanned
  @return error code or DB_SUCCESS */
  dberr_t add_ranges(
      trx_t* trx, dict_index_t* index,
      std::vector<std::pair<const dtuple_t*, const ib_vector::PartitionIdT>>&
          ranges,
      Parallel_reader::F&& func);

  /** Returns base pk fixed length */
  size_t base_pk_fixed_len() const { return m_base_pk_fixed_len; }

 protected:
  /** Position a page cursor to a given matching tuple
  @param[in]    index             the index used for searching
  @param[in]    tuple             the value used to search a matching record
  @param[out]   page_curor        the page cursor to be positioned
  @param[in]    mtr               mtr
  @param[in]    heap              heap (used to get offsets)
  @return error code or DB_SUCCESS */
  dberr_t get_page_cursor(dict_index_t* index,
                          const dtuple_t* tuple,
                          page_cur_t& page_cursor,
                          mtr_t* mtr,
                          mem_heap_t* heap);

 protected:
  /** Create a range from given tuples
  @param[in]    scan_ctx          the scan context that associates to the range
  @param[in]    begin             the starting point of this range
  @param[out]   range             the range to hold the cursor positions [b, e)
  @param[in]    heap              heap (used to get offsets)
  @return error code or DB_SUCCESS */
  dberr_t create_range_from_tuple(SubTableReader::SubTableScanCtx* scan_ctx,
                                  const dtuple_t* begin,
                                  Parallel_reader::Scan_ctx::Range& range,
                                  mem_heap_t* heap);

 protected:
  /** Stores the length of the base PK column in the sub_table. Zero if base PK
  is not fixed size. */
  size_t m_base_pk_fixed_len{0};
};

/** Extends upstream Scan_ctx to support sub_table specific logic.
First, we need our own implementation of create_context() as that is the method
where SubTableCtx is created.
Second, we need to override get_trx_id() to account for the fact that the trx_id
column is immediately after the PK of the table. If base_pk is fixed size, then
we can directly access the trx_id column instead of going through the metadata
to figure out the length of the base_pk column. */
class SubTableReader::SubTableScanCtx : public Parallel_reader::Scan_ctx {
 public:
  /** Constructor.
  @param[in]  reader          Parallel reader that owns this context.
  @param[in]  id              ID of this scan context.
  @param[in]  trx             Transaction covering the scan.
  @param[in]  config          Range scan config.
  @param[in]  f               Callback function. */
  SubTableScanCtx(Parallel_reader* reader, size_t id, trx_t* trx,
                  const Parallel_reader::Config& config, F&& f)
      : Parallel_reader::Scan_ctx(reader, id, trx, config, std::move(f)) {}

  dberr_t create_context(const Range& range,
                         ib_vector::PartitionIdT partition_id);

 private:
  /** This gets called for every record we traverse during visibility checking.
  If we have fixed size base_pk then we can compute the offset of trx_id column
  directly. Otherwise, we delegate to the base implementation.
  @param[in]  rec              Record to get the trx_id of.
  @param[in]  offsets          Offsets of the record.
  @return the trx_id of the current record. */
  [[nodiscard]] inline trx_id_t get_trx_id(
      const rec_t* rec, const ulint* offsets) const override {
    auto base_pk_len =
        reinterpret_cast<SubTableReader*>(m_reader)->base_pk_fixed_len();
    if (base_pk_len) {
      /* The trx id column is immediately after the PK of the table. In case
      of sub_table, the PK is partition_id + base_pk. */
      return trx_read_trx_id(rec + ib_vector::KMeansTable::partition_id_len +
                             base_pk_len);
    }
    return Parallel_reader::Scan_ctx::get_trx_id(rec, offsets);
  }
};

/** This is specialized context for sub table scan. It is not meant for general
purpose range scan. We rely on the knowledge of the sub table schema to optimize
the range scan.  */
class SubTableReader::SubTableCtx : public Parallel_reader::Ctx {
 public:
  /** Constructor
  @param[in]    ctx_id            the ID of this context
  @param[in]    scan_ctx          the scan context that associates to this
                                  context
  @param[in]    range             the range that this context has to read
  @param[in]    partition_id      the partition id of the range
  @param[in]    pk_fixed_len      the length of the base PK column in the
                                  sub_table. Zero if base PK is not fixed size.
*/
  SubTableCtx(size_t ctx_id, SubTableScanCtx* scan_ctx,
              const Parallel_reader::Scan_ctx::Range& range,
              ib_vector::PartitionIdT partition_id, size_t pk_fixed_len)
      : Parallel_reader::Ctx(ctx_id, scan_ctx, range),
        m_next_partition_id(partition_id + 1),
        m_base_pk_fixed_len(pk_fixed_len) {}

 protected:
  /** @return the offsets of the current record.
  Keep this simple to allow for inlining.
  @param[in]  rec              Record to get the offsets of.
  @param[in]  index            Index of the record.
  @param[in]  offsets          Offsets of the record.
  @param[in]  heap             Heap to use if offsets need to be built.
  @return the offsets of the current record. */
  [[nodiscard]] ulint* get_offsets(const rec_t* rec, const dict_index_t* index,
                                   ulint* offsets, mem_heap_t* heap) override {
    if (m_offsets_computed) {
      rec_offs_make_valid(rec, index, m_offs);
      ut_ad(offsets_unchanged());
      UNIV_PREFETCH_R(m_offs);
      return m_offs;
    }

    return get_offsets_slow(rec, index, offsets, heap);
  }

  /** Compare the current record to see if we have reached the end of the range.
  @param[in]  rec              Record to compare.
  @param[in]  index            Index of the record.
  @param[in]  offsets          Offsets of the record.
  @return the comparison result
  @retval 0 if rec belongs to the next partition
  @retval negative if rec is of a partition greater than the next partition
  @retval positive if rec is of a partition less than the next partition */
  [[nodiscard]] inline int cmp_range_end(const rec_t* rec,
                                         const dict_index_t* index,
                                         const ulint* offsets) const override {
    /* We use a dummy iterator (same as begin) for end tuple. */
    ut_ad(m_range.first == m_range.second);

    /* The partition id is the first field of the sub_table. */
    auto part = static_cast<ib_vector::PartitionIdT>(
        mach_read_int_type(rec, ib_vector::KMeansTable::partition_id_len, false));

    ut_ad(part >= 0);
    return m_next_partition_id - part;
  }

 protected:
  /** @return the offsets of the current record. This is the slow path.
  @param[in]  rec              Record to get the offsets of.
  @param[in]  index            Index of the record.
  @param[in]  offsets          Offsets of the record.
  @param[in]  heap             Heap to use if offsets need to be built.
  @return the offsets of the current record. */
  [[nodiscard]] ulint* get_offsets_slow(const rec_t* rec,
                                        const dict_index_t* index,
                                        ulint* offsets, mem_heap_t* heap);
#ifdef UNIV_DEBUG
  bool offsets_unchanged() {
    if (m_offsets_computed) {
      return rec_offs_cmp(m_offs, m_offs_copy);
    }
    return true;
  }
#endif
  /** Next partittion id. Terminate the scan when we reach this partition. */
  const ib_vector::PartitionIdT m_next_partition_id{-1};

  /** offsets array. We want this not to share cache lines with other fields. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) ulint m_offs[REC_OFFS_NORMAL_SIZE];

  /** The length of the PK column in the base table. Only set when base_pk
  is fixed size. If we know that base_pk is fixed size then our rec size
  in sub_table is fixed and we don't need to recompute the offsets for each
  row.

  Why is it safe to not call rec_get_offsets() for every record in the
  sub_table?
  * sub_table has no row versions
  * sub_table has no virtual columns
  * sub_table has no extern fields. We check for this and only use this
    optimization when a sub_table record fits in the page.
  * base_pk is fixed size.

  What above implies is that because all fields in the sub_table are fixed size
  and no EXTRA_BYTES in the record metadata are being used, we can reuse the
  offsets once they are computed. */
  alignas(ut::INNODB_CACHE_LINE_SIZE) size_t m_base_pk_fixed_len{0};

  /** Flag indicating if offsets are already computed. */
  bool m_offsets_computed{false};

#ifdef UNIV_DEBUG
  /** A copy of offsets array to verify they stay the same. */
  ulint m_offs_copy[REC_OFFS_NORMAL_SIZE];
#endif
};

#endif /* !row0pread_range_h */
