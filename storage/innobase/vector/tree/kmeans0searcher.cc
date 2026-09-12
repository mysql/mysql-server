// Copyright 2026 Google LLC

/** @file vector/kmeans0searcher.cc
 KMeans searcher wrapper */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <utility>

#include <vector0cfg.h>
#include <vector0types.h>
#include <vector0build.h>
#include <kmeans0types.h>
#include <kmeans0table.h>
#include <kmeans0searcher.h>

#include "data0data.h"
#include "db0err.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "ha_innodb.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "page0cur.h"
#include "rem/rec.h"
#include "rem0cmp.h"
#include "rem0wrec.h"
#include "rem0types.h"
#include "row0pread.h"
#include "row0pread-range.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0dbg.h"

#include "absl/synchronization/mutex.h"


namespace ib_vector {

KMeansSearcher::KMeansSearcher(const KMeansIndex* kmeans_index, VectorDatapoint& query,
                           const Partitions& partitions, size_t num_neighbors,
                           dict_index_t* sub_table_index, trx_t* trx)
    : m_query(query),
      m_base_pk_fixed_len(kmeans_index->pk_fixed_len()),
      m_dist_measure(kmeans_index->dist_measure()),
      m_dims(kmeans_index->dims()),
      m_num_total_partitions(kmeans_index->num_partitions()),
      m_num_neighbors(num_neighbors),
      m_partitions(partitions),
      m_sub_table_index(sub_table_index),
      m_trx(trx) {
  m_neighbors.Init(m_num_neighbors);
  m_heap = mem_heap_create(KMeansTable::base_pk_max_len, UT_LOCATION_HERE);

  if (m_dist_measure == DistMeasure::L2_SQUARED) {
    m_query_l2_norm = SquaredL2Norm(m_query.get());
  }

  auto dspan = absl::MakeSpan(
      m_query->mutable_float_values(), m_query->dimensionality());
  kmeans_index->inverse_quantize(dspan);
}

KMeansSearcher::~KMeansSearcher() {
  mem_heap_free(m_heap);
  for (auto h : m_thread_heaps) {
    mem_heap_free(h);
  }
}

size_t KMeansSearcher::get_parallelism() const {
  auto thd = current_thd;
  bool enabled = thd_parallel_vector_search_enabled(thd);
  if (!enabled) {
    if (!opt_cloudsql_vector_test_mode) {
      return 0;
    } else {
      /* When running in test mode, we randomly enable parallel search. In terms
      of results it should be the same as in single thread mode. */
      std::mt19937 rng(std::random_device{}());
      bool rand_bool = std::uniform_int_distribution<>{0, 1}(rng);
      if (rand_bool == 0) {
        return 0;
      }
    }
  }

  auto parallelism = thd_parallel_read_threads(thd);
  if (parallelism <= 1) {
    /* We might as well do it inline */
    return 0;
  }

  /* Try getting threads from the pool */
  size_t n_threads = Parallel_reader::available_threads(parallelism, false);
  if (n_threads == 1) {
    /* Might as well do it inline */
    Parallel_reader::release_threads(n_threads);
    return 0;
  }

  return n_threads;
}

void KMeansSearcher::init_for_scan() {
  m_n_threads = get_parallelism();
  if (m_n_threads == 0) {
    MONITOR_INC(MONITOR_VECTOR_INDEX_SINGLE_THREAD_QUERIES);
  }

  do {
    m_thread_heaps.push_back(mem_heap_create(4096, UT_LOCATION_HERE));
    m_page_ctxs.emplace_back(std::make_shared<Page_ctx>(m_base_pk_fixed_len));
  } while (m_thread_heaps.size() < m_n_threads);

  m_neighbors.AcquireMutator(&m_neighbors_mutator);
  m_epsilon = m_neighbors_mutator.epsilon();
}

void KMeansSearcher::process_results(Neighbors& neighbors) {
  // copy the results to the return container
  m_neighbors_mutator.Release();
  auto list = m_neighbors.FinishSorted();
  size_t num_results = std::min(m_num_neighbors, list.first.size());
  for (size_t i = 0; i < num_results; i++) {
    neighbors.push_back(std::make_pair(*(list.first[i]), list.second[i]));
  }
}

void KMeansSearcher::page_finish_cbk(const Parallel_reader::Thread_ctx* ctx) {
  auto& page_ctx = m_page_ctxs[ctx->m_thread_id];
  switch (ctx->get_state()) {
    case Parallel_reader::State::PAGE:
      /* End of page. Process the rows in the page. */
      if (page_ctx->m_current_index) {
        get_distances(page_ctx);
        page_ctx->m_current_index = 0;
      }

      /* Empty the heap. */
      if (m_thread_heaps[ctx->m_thread_id]) {
        mem_heap_empty(m_thread_heaps[ctx->m_thread_id]);
      }
      break;
    case Parallel_reader::State::THREAD:
    case Parallel_reader::State::CTX:
      /* There is one spurious callback at the end of the scan. */
      return;
    case Parallel_reader::State::UNKNOWN:
      /* Should never happen */
      ut_error;
      break;
  }
}

void KMeansSearcher::get_distances(const Page_ctx_ptr ctx) {
  auto n_records = ctx->m_current_index;
  const size_t vec_len = this->vec_len();
  const auto vec_data_ptr = ctx->m_vec_ptrs;

  std::vector<VectorDistT> distances;
  distances.reserve(n_records);
  distances.resize(n_records);

  /* lambda function to get the l2_norm of the datapoint */
  auto get_l2_norm = [&](int i) -> VectorDistT {
    if (m_dist_measure == DistMeasure::L2_SQUARED) {
      return *reinterpret_cast<const VectorDistT*>(vec_data_ptr[i] + vec_len);
    }
    return 0.0;
  };

  /* lambda function to measure the distance between a datapoint and query */
  auto measure_distance = [&](int i) -> VectorDistT {
    double dist = -kmeans_wrapper::DenseDotProductRaw(
        m_query.get(), reinterpret_cast<QuantizedDataT*>(
        vec_data_ptr[i]), vec_len);
    switch (m_dist_measure) {
      case DistMeasure::DOT_PRODUCT: {
        break;
      }
      case DistMeasure::L2_SQUARED: {
        dist = 2.0f * dist + m_query_l2_norm + get_l2_norm(i);

        break;
      }
      case DistMeasure::COSINE: {
        dist += 1.0f;
        break;
      }
    }

    return (VectorDistT)dist;
  };

  for (size_t i = 0; i < n_records; ++i) {
    distances[i] = measure_distance(i);
  }

  for (size_t i = 0; i < n_records; ++i) {
    if (is_close_enough(distances[i])) {
      auto pk_len = m_base_pk_fixed_len;
      if (!m_base_pk_fixed_len) {
        /* Variable length PK */
        ut_a(ctx->m_pk_len_buf);
        pk_len = ctx->m_pk_len_buf[i];
      }
      auto pk_data = ctx->m_pk_ptrs[i];
      auto pk = std::make_shared<std::string>(pk_data, pk_data + pk_len);
      add_neighbor_measurement(pk, distances[i]);
    }
  }
}

dberr_t KMeansSearcher::run_scan() {
  ut_a(m_trx);

  SubTableReader sub_table_scanner(*this);

  // set up the lambda function as callback for each row scanned
  auto row_scanner_lambda = [&](const Parallel_reader::Ctx* ctx) -> dberr_t {
    retrieve(ctx);
    return DB_SUCCESS;
  };

  // set up the lambda function as callback called at end of each page and also
  // at the end of the scan
  sub_table_scanner.set_finish_callback(
      [&](const Parallel_reader::Thread_ctx* thread_ctx) -> dberr_t {
        page_finish_cbk(thread_ctx);
        return DB_SUCCESS;
      });

  // We define a range for each partition by building a tuple with the partition
  // id. We also pass along the scalar partition id value as the scan is suppose
  // to end when we hit the next partition. We don't need to build a tuple for
  // end condition.
  std::vector<std::pair<const dtuple_t*, const PartitionIdT>> ranges;
  for (auto const &part : m_partitions) {
    dtuple_t* begin_tuple = build_tuple_from_partition_id(part);
    ranges.push_back({begin_tuple, part});
  }

  // add ranges and register the callback
  dberr_t err = sub_table_scanner.add_ranges(m_trx, m_sub_table_index, ranges,
                                             row_scanner_lambda);
  if (err != DB_SUCCESS) {
    if (err == DB_VEC_INDEX_NOT_ENOUGH_DATA) {
      return err;
    }

    // serves as an alert to handle future added error types
    ut_a(err == DB_OUT_OF_MEMORY);
    m_n_threads = 0;
  }

  ut_a(m_n_threads == 0 || m_thread_heaps.size() == m_n_threads);

  err = sub_table_scanner.run(m_n_threads);
  if (err == DB_OUT_OF_RESOURCES) {
    /* Should only happen if we are trying to run multiple threads */
    ut_a(m_n_threads > 0);
    ib::warn(ER_INNODB_OUT_OF_RESOURCES)
        << "Resource not available to create threads for parallel scan."
        << " Falling back to single thread mode.";

    err = sub_table_scanner.run(0);
  }

  return err;
}

dberr_t KMeansSearcher::get_neighbors(Neighbors& neighbors) {
  init_for_scan();
  auto err = run_scan();
  process_results(neighbors);
  return err;
}

dtuple_t* KMeansSearcher::build_tuple_from_partition_id(PartitionIdT part) {
  dtuple_t* entry = dtuple_create(m_heap, 1);
  dtuple_set_n_fields_cmp(entry, 1);
  dict_index_copy_types(entry, m_sub_table_index, 1);

  auto buf = static_cast<byte*>(mem_heap_alloc(m_heap,
                                               KMeansTable::partition_id_len));
  uint64_t pid = part;
  mach_write_int_type(buf,
                      (const byte*)&pid,
                      KMeansTable::partition_id_len,
                      false);
  dfield_set_data(dtuple_get_nth_field(entry, 0),
                  buf,
                  KMeansTable::partition_id_len);

  return entry;
}

void KMeansSearcher::retrieve(const Parallel_reader::Ctx* ctx) {
  const auto rec = ctx->m_rec;
  auto offsets = ctx->m_offsets;
  auto heap = m_thread_heaps[ctx->thread_id()];
  auto page_ctx = m_page_ctxs[ctx->thread_id()];
  ut_a(page_ctx->m_current_index < Page_ctx::kMaxRowsPerPage);

  size_t cur_index = page_ctx->m_current_index;
  [[maybe_unused]] const size_t expected_vec_len = this->content_len();
  ulint len = UNIV_SQL_NULL;
  ulint pos = 0;
  byte* data = nullptr;

  // just to be on the safe side
  ut_ad(validate_row_partition_id(rec, m_sub_table_index,
                                  ctx->range().first->m_tuple, offsets));

  // retrieve quantized vector
  pos = KMeansTable::ClustIndexFields::CLUST_CONTENT;
  std::tie(data, len) =
      get_embedding_from_rec(m_sub_table_index, rec, offsets, pos, heap);
  ut_ad(len != UNIV_SQL_NULL);
  if (unlikely(len == 0)) {
    /* Can happen in READ_UNCOMMITTED mode */
    return;
  }

  /* Store pointer to the quantized vector. */
  page_ctx->m_vec_ptrs[cur_index] = data;
  /* Verify the length of the quantized vector. */
  ut_ad(len == expected_vec_len);

  /* Store pointer to the PK data. */
  pos = KMeansTable::ClustIndexFields::CLUST_BASE_PK;
  data = const_cast<byte*>(
      rec_get_nth_field(m_sub_table_index, rec, offsets, pos, &len));
  ut_ad(len != UNIV_SQL_NULL);
  ut_ad(len == m_base_pk_fixed_len || m_base_pk_fixed_len == 0);
  page_ctx->m_pk_ptrs[cur_index] = static_cast<byte*>(data);

  /** If variable length PK is used, store the length of the PK. */
  if (!m_base_pk_fixed_len) {
    ut_a(page_ctx->m_pk_len_buf);
    page_ctx->m_pk_len_buf[cur_index] = static_cast<uint16_t>(len);
  }

  page_ctx->m_current_index++;
}

bool KMeansSearcher::validate_row_partition_id(const rec_t* rec,
                                             const dict_index_t* index,
                                             const dtuple_t* tuple,
                                             const ulint* offsets) {
  // Parallel_reader::Scan_ctx::create_persistent_cursor() generates a tuple
  // from the exact record as m_tuple field of its begin iter. This tuple
  // is different to the tuple we use to position the begin iter using
  // SubTableReader::add_ranges(), because the latter one only has one
  // n_fields_cmp (partition_id) while the former one has 2 (the composite PK
  // of sub_table).
  // For a meaningful check, only 1 field, partition_id, should be compared
  ulint matched = 0;
  return (cmp_dtuple_rec_with_match_low(tuple, rec, index, offsets, 1,
                                        &matched) == 0);
}

void KMeansSearcher::add_neighbor_measurement(std::shared_ptr<std::string> pk,
                                            VectorDistT distance) {
  absl::WriterMutexLock lock(m_mutex);
  if (m_neighbors_mutator.PushNoEpsilonCheck(pk, distance)) {
    m_neighbors_mutator.GarbageCollect();
    m_epsilon.store(m_neighbors_mutator.epsilon());
  }
}

KMeansSearcher::Page_ctx::Page_ctx(bool base_pk_fixed_len) {
  if (!base_pk_fixed_len) {
    /** Allocate buffer to store PK length */
    m_pk_len_buf = static_cast<uint16_t*>(
        ut::zalloc(sizeof(uint16_t) * Page_ctx::kMaxRowsPerPage));
    ut_a(m_pk_len_buf);
  }

  m_pk_ptrs = static_cast<byte**>(
      ut::zalloc(sizeof(byte*) * Page_ctx::kMaxRowsPerPage));
  ut_a(m_pk_ptrs);

  m_vec_ptrs = static_cast<byte**>(
      ut::zalloc(sizeof(byte*) * Page_ctx::kMaxRowsPerPage));
  ut_a(m_vec_ptrs);
}

} /* namespace ib_vector */

