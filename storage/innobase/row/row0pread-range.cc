/** @file row/row0pread-range.cc
Parallel range reader implementation */

#include <cstring>
#include <memory>
#include <vector>
#include <utility>

#include "row0pread-range.h"
#include "ha_innodb.h"

#include "btr0btr.h"
#include "data0data.h"
#include "data0types.h"
#include "db0err.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "mem0mem.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "page0types.h"
#include "rem/rec.h"
#include "rem0types.h"
#include "row0pread.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0dbg.h"
#include "ut0new.h"

SubTableReader::SubTableReader(const ib_vector::KMeansSearcher& searcher)
    : Parallel_reader(searcher.max_threads()),
      m_base_pk_fixed_len(searcher.base_pk_fixed_len()) {
  /** The optimizations we do with fixed size PK e.g.: not calling
  rec_get_offsets() for each record are only valid if we know that the
  record will fit in a page. It might work in case the content column is
  a blob but we play safe here and disable the optimizations.

  A record in sub_table has 5 fields:
  1. partition_id
  2. base_pk
  3. trx_id
  4. Rollback pointer
  5. content */
  if (m_base_pk_fixed_len) {
    const size_t total_rec_size = ib_vector::KMeansTable::partition_id_len +
                                  m_base_pk_fixed_len + DATA_TRX_ID_LEN +
                                  DATA_ROLL_PTR_LEN + searcher.content_len();
    size_t page_rec_max;
    size_t page_ptr_max;
    const dict_index_t* index = searcher.sub_table_index();
    get_permissible_max_size(index->table, index, page_rec_max, page_ptr_max);
    if (total_rec_size > page_rec_max) {
      m_base_pk_fixed_len = 0;
    }
  }
}

dberr_t SubTableReader::add_ranges(
    trx_t* trx, dict_index_t* index,
    std::vector<std::pair<const dtuple_t*, const ib_vector::PartitionIdT>>&
        ranges,
    Parallel_reader::F&& func) {
  // Parallel_reader::Config is to hold the range information in the base
  // Parallel_reader implementation. A dummy range is given here since the
  // actual range information will be directly calculated to avoid the
  // sophisticated slicing logic in Parallel_reader::Scan_ctx::partition().
  // The only purpose of this dummy range is to serve as the vehicle of index.
  Parallel_reader::Scan_range dummy_range;
  Parallel_reader::Config config(dummy_range, index);
  auto scan_ctx = std::shared_ptr<SubTableReader::SubTableScanCtx>(
      ut::new_withkey<SubTableReader::SubTableScanCtx>(UT_NEW_THIS_FILE_PSI_KEY,
                                                       this, m_scan_ctx_id, trx,
                                                       config, std::move(func)),
      [](Parallel_reader::Scan_ctx* scan_ctx) { ut::delete_(scan_ctx); });

  if (!scan_ctx) {
    return DB_OUT_OF_MEMORY;
  }

  m_scan_ctxs.push_back(scan_ctx);
  ++m_scan_ctx_id;

  auto heap = mem_heap_create(1024, UT_LOCATION_HERE);

  scan_ctx->index_s_lock();

  dberr_t err = DB_SUCCESS;
  bool at_least_one_range_added = false;
  for (auto const r : ranges) {
    // manually specify the range
    Parallel_reader::Scan_ctx::Range range;
    create_range_from_tuple(scan_ctx.get(), r.first, range, heap);

    // add this range to Parallel_read::Ctx iff it is valid
    if (range.first) {
      err = scan_ctx->create_context(range, r.second);
      if (err != DB_SUCCESS) {
        break;
      }
      at_least_one_range_added = true;
    }

    mem_heap_empty(heap);
  }

  scan_ctx->index_s_unlock();

  mem_heap_free(heap);
  if (!at_least_one_range_added && err == DB_SUCCESS) {
    return DB_VEC_INDEX_NOT_ENOUGH_DATA;
  }

  // The only possible error codes are either DB_SUCCESS or DB_OUT_OF_MEMORY.
  // The latter can come from either creating Scan_ctx or from the call of
  // scan_ctx->create_context.
  return err;
}

dberr_t SubTableReader::create_range_from_tuple(
    SubTableReader::SubTableScanCtx* scan_ctx, const dtuple_t* begin,
    Parallel_reader::Scan_ctx::Range& range, mem_heap_t* heap) {
  ut_a(scan_ctx);

  auto index = scan_ctx->m_config.m_index;

  // Here only one range is added inside this MTR. The main reason is to avoid
  // double locking of a single buffer page if 2 ranges land on the same one.
  // When Parallel_reader::Scan_ctx::create_persistence_cursor() does a
  // store_position(), it locks the page so the next lock will hit assertion.
  mtr_t mtr;
  mtr_start(&mtr);

  // we want to scan the whole range with the given partition id, so set the
  // positioning mode to GE.
  page_cur_t cursor_begin;
  dberr_t gpc_err = get_page_cursor(index, begin, cursor_begin, &mtr, heap);
  if (gpc_err == DB_SUCCESS) {
    cursor_begin.index = index;
    auto iter_begin = scan_ctx->create_persistent_cursor(cursor_begin, &mtr);
    ut_a(iter_begin->m_pcur);

    mtr_commit(&mtr);

    range.first = iter_begin;
    /* There is no end tuple to terminate the range scan in sub_table. */
    range.second = iter_begin;
  } else {
    // It is not an error if the starting point of the range cannot be found.
    // The caller might just picked a value that does not exist in the table.
    // Just set the start position to be NULL to signify this invalid range.
    range.first = nullptr;
    mtr_commit(&mtr);
  }

  return DB_SUCCESS;
}

dberr_t SubTableReader::get_page_cursor(dict_index_t* index,
                                        const dtuple_t* tuple,
                                        page_cur_t& page_cursor, mtr_t* mtr,
                                        mem_heap_t* heap) {
  btr_pcur_t pcur;
  pcur.open_on_user_rec(index,
                        tuple,
                        PAGE_CUR_GE,
                        BTR_SEARCH_LEAF,
                        mtr,
                        UT_LOCATION_HERE);

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint *offsets = offsets_;

  rec_offs_init(offsets_);

  const rec_t* rec = pcur.get_rec();
  offsets = rec_get_offsets(rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &heap);

  const bool valid = page_rec_is_user_rec(rec) &&
                     (tuple->compare(rec, index, offsets) == 0);

  if (valid) {
    memcpy(&page_cursor, pcur.get_page_cur(), sizeof(page_cursor));
  }

  pcur.close();

  return valid ? DB_SUCCESS : DB_RECORD_NOT_FOUND;
}

dberr_t SubTableReader::SubTableScanCtx::create_context(
    const Range& range, ib_vector::PartitionIdT partition_id) {
  auto ctx_id = m_reader->m_ctx_id.fetch_add(1, std::memory_order_relaxed);

  auto reader = reinterpret_cast<SubTableReader*>(m_reader);
  auto ctx = std::shared_ptr<SubTableCtx>(
      ut::new_withkey<SubTableCtx>(UT_NEW_THIS_FILE_PSI_KEY, ctx_id, this,
                                   range, partition_id,
                                   reader->base_pk_fixed_len()),
      [](SubTableCtx* ctx) { ut::delete_(ctx); });

  dberr_t err{DB_SUCCESS};

  if (ctx == nullptr) {
    m_reader->m_ctx_id.fetch_sub(1, std::memory_order_relaxed);
    return (DB_OUT_OF_MEMORY);
  } else {
    ctx->m_split = false;
    m_reader->enqueue(ctx);
  }

  return (err);
}

ulint* SubTableReader::SubTableCtx::get_offsets_slow(const rec_t* rec,
                                                     const dict_index_t* index,
                                                     ulint* offsets,
                                                     mem_heap_t* heap) {
  ut_ad(!m_offsets_computed);

  if (!m_base_pk_fixed_len) {
    return Parallel_reader::Ctx::get_offsets(rec, index, offsets, heap);
  }

  rec_offs_init(m_offs);
  mem_heap_t* offs_heap{nullptr};
  ulint* ret_offsets = rec_get_offsets(rec, index, m_offs, ULINT_UNDEFINED,
                                       UT_LOCATION_HERE, &offs_heap);
  /* Should never happen in case of sub_table as we have just five fields. */
  ut_a(offs_heap == nullptr);
  ut_a(ret_offsets == m_offs);

  m_offsets_computed = true;

#ifdef UNIV_DEBUG
  memcpy(m_offs_copy, m_offs, sizeof(m_offs_copy));
#endif
  return m_offs;
}