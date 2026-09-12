// Copyright 2026 Google LLC

/** @file vector/kmeans/kmeans0index.cc
 KMeans index object type in MySQL */

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <thread>
#include <algorithm>
#include <cmath>

#include "vector0cfg.h"
#include "vector0pk.h"
#include "vector0build.h"
#include "vector0types.h"
#include "vector0index.h"
#include "kmeans0types.h"
#include "kmeans0index.h"
#include "kmeans0table.h"
#include "kmeans0searcher.h"

#include "my_dbug.h"
#include "row0pread.h"
#include "trx0trx.h"
#include "ut0dbg.h"
#include "ut0log.h"

#include "sql/sql_class.h"
#include "sql/current_thd.h"

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "kmeans_interface/scann_interface.h"

namespace ib_vector {

KMeansIndex::KMeansIndex(const std::string& name, const std::string& base_table,
                     VectorCfg&& cfg)
    : VectorIndex(name, base_table, std::move(cfg)),
      m_quantizer((IndexType)m_config[Options::INDEX_TYPE]) {
  // verify that all necessary data are set
  ut_a(m_config.find(Options::INDEX_TYPE));
  ut_a(m_config.find(Options::VECTOR_DIMENSION));
  ut_a(m_config.find(Options::DIST_MEASURE));

  m_dataset = std::make_shared<Dataset>();
  ut_a(m_dataset != nullptr);

  m_dataset->set_dimensionality((int)m_config[Options::VECTOR_DIMENSION]);
  m_state.store(INDEX_PENDING_TRAIN);
  m_status = "Index object created";
}

index_err_t KMeansIndex::append_to_dataset(Vector& vector) {
  // this function can be called in parallel, proper locking should be
  // acquired by the caller

  if (m_state.load() != INDEX_PENDING_TRAIN) {
    return NOT_PERMITTED;
  }

  ut_ad(m_dataset != nullptr);

  // no normalization required now, it will be done later by train() in batches
  auto d_wrap = kmeans_wrapper::CreateDatapointFromPtr(vector.data(), vector.size());
  VectorDatapointPtr dp = d_wrap.get();

  /* It is possible that training has already been aborted by some other thread
  due to some error. */
  if (m_training_err != SUCCESS) {
    return m_training_err;
  }

  // incrementally preallocate training dataset, by m_dataset_step size
  if (m_dataset->size() % m_dataset_chunk_size == 0) {
    // pre-allocate 1K dataset beforehand, a possible overkill
    size_t incr_training_mem_size = m_dataset_chunk_size * dims() * 4;
    if (!index_registry->request_memory(incr_training_mem_size)) {
      // let the caller to decide if continue or not
      m_training_err = NOT_ENOUGH_MEMORY;
      return m_training_err;
    } else {
      m_training_mem_size += incr_training_mem_size;
    }
  }

  auto res = m_dataset->Append(dp);

  DBUG_EXECUTE_IF("simulate_kmeans_add_dp_error",
                  res = absl::UnknownError("simulated kmeans error"););

  if (!res.ok()) {
    auto index_err = log_and_get_kmeans_error(res);
    m_training_err = index_err;
    return m_training_err;
  }
  ++m_current_size;

  ut_ad(m_training_err == SUCCESS);
  return SUCCESS;
}

dberr_t KMeansIndex::persist(PersistType type, ...) {
  va_list args;
  va_start(args, type);
  auto ret = persist_impl(type, args);
  va_end(args);
  return ret;
}

dberr_t KMeansIndex::persist_impl(PersistType type, va_list args) {
  if (type == PersistType::SIMPLE) {
    // do not have SIMPLE persist implementation right now, the old file based
    // persistence is obsolete after vector feature's GA
    ut_error;
  } else if (type == PersistType::PAGED) {
    que_thr_t* thr = va_arg(args, que_thr_t*);
    CreateVectorIndexInfo* create_info = va_arg(args, CreateVectorIndexInfo*);
    return persist_to_table(thr, create_info);
  } else {
    return DB_UNSUPPORTED;
  }
}

dberr_t KMeansIndex::persist_to_table(que_thr_t* thr,
                                    CreateVectorIndexInfo* create_info) {
  auto writer =
      VectorPersist::create<PersistType::PAGED>(this, thr, create_info);

  return writer->persist();
}

dberr_t KMeansIndex::reload(PersistType type, ...) {
  va_list args;
  va_start(args, type);
  auto ret = reload_impl(type, args);
  va_end(args);
  return ret;
}

dberr_t KMeansIndex::reload_impl(PersistType type, va_list args) {
  if (type == PersistType::SIMPLE) {
    // do not have SIMPLE persist implementation right now, the old file based
    // persistence is obsolete after vector feature's GA
    ut_error;
  } else if (type == PersistType::PAGED) {
    dict_table_t* base_table = va_arg(args, dict_table_t*);
    m_current_size = 1;
    return reload_from_table(base_table);
  } else {
    return DB_UNSUPPORTED;
  }
}

dberr_t KMeansIndex::reload_from_table(dict_table_t* base_table) {
  absl::WriterMutexLock lock(m_mutex);

  // it is possible that some other thread had just loaded it
  if (m_state.load() == INDEX_READY_TO_USE) {
    return DB_SUCCESS;
  }

  auto loader = VectorPersist::create<PersistType::PAGED>(this, base_table);

  return loader->load().first;
}

std::pair<std::shared_ptr<VectorPersist>, dberr_t>
KMeansIndex::sync_mutation(que_thr_t* thr) {
  auto writer =
      VectorPersist::create<PersistType::PAGED>(
          this, thr, (CreateVectorIndexInfo*)nullptr);

  dberr_t err = writer->sync_mutation(thr);

  return std::pair(writer, err);
}

bool KMeansIndex::set_partitioner(const char *buf, size_t len) {
  generate_kmeans_native_cfg();

  std::vector<uint8_t> cfg_buf(m_native_kmeans_config.ByteSizeLong());
  m_native_kmeans_config.SerializeToArray(cfg_buf.data(), cfg_buf.size());

  auto interface_config = kmeans_wrapper::ParseKMeansConfigBinary(
      cfg_buf.data(), cfg_buf.size());

  auto interface_partitioner = kmeans_wrapper::CreatePartitioner(
      reinterpret_cast<const uint8_t *>(buf), len, interface_config.get());

  if (interface_partitioner) {
    if (m_config.find(Options::NUM_TLP)) {
      m_query_partitioner = std::make_shared<TreeBruteForceSecondLevelWrapper<VectorDataT>>(
          std::move(interface_partitioner));
    } else {
      m_query_partitioner = std::make_shared<Partitioner>(std::move(interface_partitioner));
    }
    m_query_partitioner->set_tokenization_mode(UntypedPartitioner::QUERY);

    restore_mutate_partitioner();
    return true;
  }

  return false;
}

void KMeansIndex::restore_mutate_partitioner() {
  // Use the base partitioner wrapped by TLP, instead of TLP itself for mutate

  if (!m_config.find(Options::NUM_TLP)) {
    auto part = m_query_partitioner->Clone();
    part->set_tokenization_mode(UntypedPartitioner::DATABASE);
    m_mutate_partitioner = std::move(part);
  } else {
    auto tlp =
      dynamic_cast<const TreeBruteForceSecondLevelWrapper<VectorDataT>*>(
        m_query_partitioner.get());
    ut_a(tlp);
    auto base = tlp->base();
    auto p2 = base->Clone();
    p2->set_tokenization_mode(UntypedPartitioner::DATABASE);
    m_mutate_partitioner = std::move(p2);
  }
}

bool KMeansIndex::set_multipliers(ConstVectorSpan mult) {
  ut_a(mult.size() == (size_t)m_config[Options::VECTOR_DIMENSION]);
  m_quantizer.set_multipliers(mult);
  return true;
}

index_err_t KMeansIndex::get_neighbors(Vector&& query, Neighbors& neighbors,
                                     VectorSearchOptions search_options,
                                     dict_index_t* sub_table_index,
                                     trx_t* trx) {
  if (m_state.load() != INDEX_READY_TO_USE) {
    return NOT_PERMITTED;
  }

  if (m_current_size == 0) {
    return INDEX_EMPTY;
  }

  // adjust the search parameters if necessary
  auto n_search_parts = search_options.num_leaves_to_search;
  auto n_neighbors = search_options.num_neighbors;
  if (!n_neighbors) {
    ut_ad(m_config.find(Options::NUM_NEIGHBORS));
    n_neighbors = (int)m_config[Options::NUM_NEIGHBORS];
  }
  const auto n_expected_results = n_neighbors;

  // figure out stream query related parameters
  auto stream_id = search_options.stream_id;
  auto stream_start = stream_id ? !has_cached_query(stream_id) : false;

  const auto n_neighbors_limit =
       current_thd->variables.cloudsql_vector_iterative_filtering_max_neighbors;

  std::shared_ptr<KMeansQueryData> kmeans_query_data = nullptr;
  if (stream_id) {
    if (stream_start) {
      ut_a(sub_table_index);
      // init its query cache before the first stream query starts
      kmeans_query_data = std::make_shared<KMeansQueryData>();
      kmeans_query_data->saved_sub_table_handle = sub_table_index->table;
      add_cached_query(stream_id,
                       std::static_pointer_cast<QueryData>(kmeans_query_data));
    } else {
      ut_ad(sub_table_index == nullptr);
      auto query = cached_query(stream_id);
      query->num_stream_calls++;
      // if there are enough results returned already, no more scans
      if (query->num_returned_results >= n_neighbors_limit) {
        return NOT_ENOUGH_DATA_POINTS;
      } else if (!query->cached_results.empty()) {
        // directly return the requested (or remaining) number of results
        pop_cached_results(stream_id, neighbors, n_expected_results);
        return SUCCESS;
      } else {
          sub_table_index = ((dict_table_t*)query->saved_sub_table_handle)
                                ->first_index();
          kmeans_query_data = std::static_pointer_cast<KMeansQueryData>(query);
      }
    }
  }

  // a sub_table scan is needed if reach here

  ut_ad(sub_table_index && trx);

  auto d = kmeans_wrapper::CreateDatapointFromPtr(query.data(), query.size());
  auto dp = normalize_data(d.get());

  bool tokenization_needed = !stream_id || stream_start;
  Partitions parts;
  auto n_tokenized_parts = std::min(n_search_parts,
                                    (int)m_config[Options::NUM_PARTITIONS]);

  n_search_parts = (n_search_parts <= 0)
                 ? (int)m_config[Options::NUM_QUERY_PARTITIONS]
                 : n_search_parts;

  if (tokenization_needed) {
    if (stream_start) {
      // at the beginning of a stream query, rank all partitions in one go
      n_tokenized_parts = m_config[Options::NUM_PARTITIONS];
    }
    auto& p = stream_start ? kmeans_query_data->cached_parts : parts;

    if (n_tokenized_parts > 0) {
      m_query_partitioner.tokenize(dp, &p, n_tokenized_parts);
    } else {
      m_query_partitioner.tokenize(dp, &p);
    }

    if (stream_start) {
      pop_cached_parts(stream_id, parts, n_search_parts);
    }
  } else if (stream_id) {
    // in case partitions run out earlier than dp
    if (kmeans_query_data->cached_parts.empty()) {
      return NOT_ENOUGH_DATA_POINTS;
    }
    pop_cached_parts(stream_id, parts, n_search_parts);
  }

  if (stream_id) {
    // if a sub_table scan is required for a stream query, rank all data points
    n_neighbors = n_neighbors_limit;
  }

  KMeansSearcher searcher(this, d, parts, n_neighbors, sub_table_index, trx);

  auto& ann_results = stream_id
                    ? kmeans_query_data->cached_results
                    : neighbors;

  auto err = searcher.get_neighbors(ann_results);

  if (stream_id) {
    // simulate the sparse partitions behavior for testing purpose
    DBUG_EXECUTE_IF(
      "vector_stream_query_simulate_sparse_partitions", {
      if (kmeans_query_data->num_stream_calls % 2 == 0) {
        kmeans_query_data->cached_results.clear();
        err = DB_VEC_INDEX_NOT_ENOUGH_DATA;
      } else if (!kmeans_query_data->cached_results.empty()) {
        kmeans_query_data->cached_results.erase(
            kmeans_query_data->cached_results.begin(),
            kmeans_query_data->cached_results.end() - 1);
      }
    });

    pop_cached_results(stream_id, neighbors, n_expected_results);
  }

  // if the sub_table scan could not find any rows to scan, i.e., there are no
  // rows with given partition id
  if (err == DB_VEC_INDEX_NOT_ENOUGH_DATA) {
    // it is still retryable in stream mode, but not so otherwise
    return stream_id ? SUCCESS : NOT_ENOUGH_DATA_POINTS;
  } else if (err != DB_SUCCESS) {
    return KMEANS_ERROR;
  }

  return SUCCESS;
}

dberr_t KMeansIndex::collect_training_data(dict_index_t* index, trx_t* trx) {
  ut_a(index->is_clustered());
  ut_a(index_has_vector_col(index));

  size_t parallelism = 4;
  if (current_thd) {
    parallelism = thd_ddl_threads(current_thd);
  }

  // confirm the actual # of threads we can use
  auto n_threads = Parallel_reader::available_threads(parallelism, true);

  // use a big enough range, OK to waste
  using Shards = Counter::Shards<1024>;
  Shards n_recs{};
  Shards n_processed{};
  Counter::clear(n_recs);
  Counter::clear(n_processed);
  std::vector<mem_heap_t *, ut::allocator<mem_heap_t *>> heaps;

  const uint32_t vector_pos = get_vector_col_pos(index);
  ut_a(index->get_n_total_fields() >= vector_pos);
  [[maybe_unused]] const uint32_t dim = get_vector_dim(index);
  ut_ad(valid_dimensions(dim) &&
        (int)dim == (int)m_config[Options::VECTOR_DIMENSION]);
  auto col = index->get_field(vector_pos)->col;
  if (col->mtype != DATA_BLOB) {
    ib::error() << "Wrong vector data type: " << col->mtype;
    return DB_DATA_MISMATCH;
  }

  bool use_inner_trx = (trx == nullptr);
  trx = use_inner_trx ? trx_allocate_for_background() : trx;
  ut_a(trx);
  trx_start_if_not_started(trx, false, UT_LOCATION_HERE);
  trx->op_info = "Scanning table to build vector index";

  size_t min_sample_size = m_min_meaningful_data_size * m_min_sample_ratio;
  int customized_num_partitions = m_config.find(Options::NUM_PARTITIONS)
                                      ? (int)m_config[Options::NUM_PARTITIONS]
                                      : 0;

  auto should_process = [&](uint32_t id) -> bool {
    Counter::inc(n_recs, id);
    double total_processed = static_cast<double>(Counter::total(n_processed));
    // collect at least (10K = 100K * 0.1) data points for training
    // or at least 1 datapoint per partition, if that is configured by cx
    if (total_processed < min_sample_size ||
        total_processed < customized_num_partitions) {
      return true;
    }

    /* Beyond that we calculate the sampling ratio dynamically */
    double total_recs = static_cast<double>(Counter::total(n_recs));
    int num_parts = customized_num_partitions
                  ? customized_num_partitions
                  : compute_num_partitions((size_t)total_recs);

    return (Counter::get(n_processed, id) == 0 ||
            total_processed / total_recs < m_min_sample_ratio ||
            total_processed < (num_parts * m_stable_kmeans_sample_size));
  };

  do {
    heaps.push_back(mem_heap_create(4096, UT_LOCATION_HERE));
  } while (heaps.size() < n_threads);

  ib::info() << "Scanning index: " << index->name
             << " with n_threads: " << n_threads;

  reset_base_table_size();

  dberr_t err{DB_SUCCESS};
  Parallel_reader reader(n_threads);
  const Parallel_reader::Scan_range FULL_SCAN;
  Parallel_reader::Config config(FULL_SCAN, index);

  // This flag tells all scanning threads to stop collecting training data
  // since there is not enough memory. The scanning should go on, though,
  // to get an accurate row count.
  std::atomic<bool> stop_collecting{false};

  err = reader.add_scan(trx, config, [&](const Parallel_reader::Ctx *ctx) {
    const auto rec = ctx->m_rec;
    auto offsets = ctx->m_offsets;
    const auto id = ctx->thread_id();
    auto heap = heaps[id];
    const bool should_insert = should_process(id);

    if (embedding_is_null(index, rec, offsets, vector_pos)) {
      mem_heap_empty(heap);
      return DB_SUCCESS;
    } else {
      // only non-NULL vectors are counted
      inc_base_table_size();
    }

    if (!should_insert || stop_collecting.load()) {
      mem_heap_empty(heap);
      return DB_SUCCESS;
    }

    /* Read vector data. */
    Vector emb;
    ulint len =
        copy_embedding_from_rec(index, rec, offsets, vector_pos, heap, emb);
    ut_a(len != UNIV_SQL_NULL);

    Counter::inc(n_processed, id);
    index_err_t result = SUCCESS;

    // scoped region for exclusive m_dataset access
    {
      absl::WriterMutexLock lock(m_mutex);
      result = stop_collecting.load() ? SUCCESS : append_to_dataset(emb);

      if (result == NOT_ENOUGH_MEMORY) {
        double tot = static_cast<double>(Counter::total(n_processed));
        if (m_config.find(Options::NUM_PARTITIONS) &&
            tot >= customized_num_partitions) {
          if (stop_collecting.load() == false) {
            ib::warn() << "Not enough memory to continue collecting training "
                          "data, sampling is reduced to scanning for vector "
                          "count only";
            stop_collecting.store(true);
            m_training_err = SUCCESS;
          }
        } else {
          // if no explicitly configured num_partitions, we cannot decide
          // whether the collected data is enough or not at this point
          ib::error() << "Not enough memory to collect training dataset";
          m_training_err = NOT_ENOUGH_MEMORY;
          return DB_VEC_INDEX_OUT_OF_MEMORY;
        }
      } else if (result != SUCCESS) {
        ib::error() << "Error encountered during index build: "
                    << IndexErrToString(result);
        return DB_ERROR;
      }
    }

    mem_heap_empty(heap);
    return DB_SUCCESS;
  });

  if (err == DB_SUCCESS) {
    err = reader.run(n_threads);
  }

  if (err == DB_OUT_OF_RESOURCES) {
    ut_a(n_threads > 0);

    ib::warn(ER_INNODB_OUT_OF_RESOURCES)
        << "Resource not available to create threads for parallel scan."
        << " Falling back to single thread mode.";

    err = reader.run(0);
  }

  if (err != DB_SUCCESS) {
    index_registry->release_memory(m_training_mem_size);
  }

  for (auto heap : heaps) {
    mem_heap_free(heap);
  }

  trx->op_info = "";
  if (use_inner_trx) {
    trx_commit(trx);
    trx_free_for_background(trx);
  }

  ib::info() << "Scanned index " << index->name
             << " Scanned rows: " << Counter::total(n_recs)
             << " processed rows: " << Counter::total(n_processed);

  return err;
}

index_err_t KMeansIndex::train() {
  ut_a(m_state.load() == INDEX_PENDING_TRAIN);

  m_status = "Training";

  absl::ReaderMutexLock lock(m_mutex);

  generate_kmeans_native_cfg();

  /** We need at least one datapoint per partition (when user has configured
  the NUM_PARTITIONS option) and at least 1000 datapoints in the dataset. */
  if (initial_size() < (size_t)m_config[Options::NUM_PARTITIONS] ||
      initial_size() < m_min_data_size) {
    index_registry->release_memory(m_training_mem_size);
    return NOT_ENOUGH_DATA_POINTS;
  }

  normalize_data(m_dataset);

  // calculate the K-means partitions
  std::unique_ptr<Partitioner> part = compute_partitions();
  if (!part) {
    ib::error() << "Failed to create partitioner";
    return KMEANS_ERROR;
  }

  // assign query & mutate partitioners
  create_partitioners(std::move(part));

  // calculate multiplier
  auto multipliers = ComputeMaxQuantizationMultipliers(*m_dataset);
  m_quantizer.set_multipliers(multipliers);

  m_dataset.reset();

  m_state.store(INDEX_PENDING_BUILD);
  m_status = "Training complete";

  ib::info() << m_name << ": training completed";

  return SUCCESS;
}

std::unique_ptr<Partitioner> KMeansIndex::compute_partitions() {
  unsigned int n_threads = 0;
  if (current_thd) {
    /* When working with proper MySQL connection pick the user configured degree
    of parallelism. */
    n_threads = thd_ddl_threads(current_thd);
    ib::info() << "n_threads: " << n_threads;
  } else {
    /* Use half of the available cores. */
    n_threads = std::thread::hardware_concurrency() / 2;
  }
  if (n_threads == 0) {
    n_threads = 1;
  }

  SingleMachineFactoryOptions opt;
  if (n_threads > 1) {
    opt.parallelization_pool = std::make_shared<ThreadPool>(
        n_threads,  // # of worker threads
        ThreadPool::Options{
            .name_prefix =
                "kmeans_index_training_threads",    // name for /threadz utils
            .thread_options = thread::Options(),  // "safe" thread stack options
        });
  }

  ib::info() << m_name << ": training started with " << n_threads
             << " thread(s)";


  std::vector<uint8_t> cfg_buf(m_native_kmeans_config.ByteSizeLong());
  m_native_kmeans_config.SerializeToArray(cfg_buf.data(), cfg_buf.size());
  auto interface_config = kmeans_wrapper::ParseKMeansConfigBinary(cfg_buf.data(),
                                                        cfg_buf.size());
  auto interface_partitioner = kmeans_wrapper::TrainPartitioner(
      m_dataset ? m_dataset->get_impl() : nullptr, interface_config.get(),
      opt.parallelization_pool ?
      opt.parallelization_pool->impl_.get() : nullptr);

  DBUG_EXECUTE_IF("simulate_kmeans_training_error",
                  { interface_partitioner.reset(); });

  if (!interface_partitioner) {
    ib::error() << "Failed to create partitioner";
    index_registry->release_memory(m_training_mem_size);
    return nullptr;
  }
  return std::make_unique<Partitioner>(std::move(interface_partitioner));
}

bool KMeansIndex::create_partitioners(std::unique_ptr<Partitioner> part) {
  auto p2 = part->Clone();

  // query partitioner needs to learn the top level tree if TLP is enabled
  if (m_config.find(Options::NUM_TLP)) {
    if (part->get_impl()) {
      auto status = MaybeAddTopLevelPartitioner(
          part, m_native_kmeans_config.partitioning());
      if (absl::OkStatus() != status) {
        ib::error() << "Failed to create a TLP";
        return false;
      }
    }
    ib::info() << "TLP created to replace the traditional query partitioner";
  }

  part->set_tokenization_mode(UntypedPartitioner::QUERY);
  if (m_config.find(Options::NUM_TLP)) {
    m_query_partitioner =
        std::make_unique<TreeBruteForceSecondLevelWrapper<VectorDataT>>(
            std::move(part->impl_));
  } else {
    m_query_partitioner = std::move(part);
  }

  if (!m_config.find(Options::NUM_TLP)) {
    ib::info() << "use the standard database partitioner for index building";
    p2->set_tokenization_mode(UntypedPartitioner::DATABASE);
  } else {
    // Training (also mutate) partitioner only needs to find the top 1 nearest
    // neighbor, i.e., the 1 centroid that is closest to the data point. We
    // can afford to use a much smaller TLP search range w/o the worry of any
    // recall loss.
    PartitioningConfig training_part_cfg;
    training_part_cfg.CopyFrom(m_native_kmeans_config.partitioning());
    auto tlp = training_part_cfg.mutable_bottom_up_top_level_partitioner();
    ut_a(tlp->enabled());

    auto num_search_centroids = tlp->num_centroids_to_search();
    ut_a(num_search_centroids >= m_tlp_scale_down_ratio);
    tlp->set_num_centroids_to_search(
        num_search_centroids / m_tlp_scale_down_ratio);

    auto status = MaybeAddTopLevelPartitioner(p2, training_part_cfg);
    if (absl::OkStatus() != status) {
      ib::error() << "Failed to create a temp TLP for index building";
      return false;
    }
    p2->set_tokenization_mode(UntypedPartitioner::QUERY);
    ib::info() << "use TLP query partitioner for index building";

    // Note: This shrinked TLP search range is temporary, for training only.
    //       The set_partitioner() call after the training is done will use
    //       the traditional database partitioner for mutate.
    // TODO: more benchmark to evaluate the fast TLP is OK for mutate
  }
  m_mutate_partitioner = std::move(p2);

  return true;
}

/** Normalize the dataset before sending to KMeans index
@param[in]      data            the dataset to normalize
@remarks Normalization is required iff COSINE distance measure is used. The
whole dataset is normalized here so no need to normalize them individually
in the append_to_dataset() call to avoid unnecessary CPU cycles. */
void KMeansIndex::normalize_data(std::shared_ptr<Dataset> dataset) {
  if (DistMeasure(m_config[Options::DIST_MEASURE]) == DistMeasure::COSINE) {
    ib::info() << "Dataset normalized, required by COSINE_DISTANCE";
    ut_a(dataset->NormalizeUnitL2().ok());
  }
}

/** Normalize datapoint before using in KMeans
@param[in]      dp              the datapoint to be normalized
@return a DatapointPtr object attached to the normalized vector
@remarks Query input also requires normalization iff COSINE. */
VectorDatapointPtr KMeansIndex::normalize_data(VectorDatapointPtr dp) {
  ut_a(dp->dimensionality() == (size_t)m_config[Options::VECTOR_DIMENSION]);

  if (DistMeasure(m_config[Options::DIST_MEASURE]) == DistMeasure::COSINE) {
    NormalizeUnitL2(dp);
  }

  return dp;
}

/* Reset kmeans index object. */
void KMeansIndex::reset() {
  ut_a(m_state.load() != INDEX_READY_TO_USE);

  ib::info() << "Resetting vector index object.";
  m_dataset.reset();
}

/** Fill in the information schema metrics
@return the structure containing the metrics of this vector index */
void KMeansIndex::set_partition_info(VectorIndexStats &info) const {
  info.partitions_ = 0;
  info.query_partitions_ = 0;

  if (m_config.find(Options::NUM_PARTITIONS)) {
    info.partitions_ = (int)m_config[Options::NUM_PARTITIONS];
  }

  if (m_config.find(Options::NUM_QUERY_PARTITIONS)) {
    info.query_partitions_ = (int)m_config[Options::NUM_QUERY_PARTITIONS];
  }
}

/** Generate a KMeans configuration based on the provided customizations.
This function is reentrant.
@return true if succeeded. */
bool KMeansIndex::generate_kmeans_native_cfg() {
  m_native_kmeans_config.Clear();

  m_native_kmeans_config.set_num_neighbors(m_config[Options::NUM_NEIGHBORS]);
  m_native_kmeans_config.mutable_distance_measure()->set_distance_measure(
      m_config[Options::DIST_MEASURE].value());

  switch ((IndexType)m_config[Options::INDEX_TYPE]) {
    case IndexType::TREE_SQ: {
      /* Use brute force when searching a leaf. Use FP8 compression for
      storing datapoints in leaves. */
      m_native_kmeans_config.mutable_brute_force()->mutable_fixed_point()
          ->set_enabled(true);
      generate_kmeans_partitioning_cfg();
    } break;
  }

  ib::info() << "Generated KMeans index from customization: " <<
      m_config.to_string();

  ib::info() << m_native_kmeans_config.DebugString();

  return true;
}

void KMeansIndex::generate_kmeans_partitioning_cfg() {
  ut_a((IndexType)m_config[Options::INDEX_TYPE] == IndexType::TREE_SQ);

  // if not already configured, compute the partitioning parameters
  if (!m_config.find(Options::NUM_PARTITIONS) ||
      !m_config.find(Options::NUM_QUERY_PARTITIONS)) {
    bool dd_load = m_config.find(Options::NUM_PARTITIONS) &&
                   m_config.find(Options::NUM_QUERY_PARTITIONS);
    int data_size = initial_size();
    int tree_size;
    if (!m_config.find(Options::NUM_PARTITIONS)) {
      tree_size = compute_num_partitions(data_size);
      m_config.set_generated(Options::NUM_PARTITIONS, tree_size);
    } else {
      tree_size = (int)m_config[Options::NUM_PARTITIONS];
    }

    int search_size;
    if (!m_config.find(Options::NUM_QUERY_PARTITIONS)) {
      tree_size = m_config[Options::NUM_PARTITIONS];
      if (data_size < m_query_spilling_size * m_stable_kmeans_partition_size) {
        // if the table is not big enough, search the whole dataset
        search_size = tree_size;
      } else {
        // otherwise, search the predefined range
        search_size = std::min(m_query_spilling_size, tree_size);
      }

      // note NUM_QUERY_PARTITIONS can be overwritten by query-time option
      m_config.set_generated(Options::NUM_QUERY_PARTITIONS, search_size);
    }

    // Apply TLP if there are enough # of partitions
    if (!dd_load && !m_config.find(Options::NUM_TLP)) {
      int tlp_threshold = m_min_tlp_threshold;
      int num_parts = (int)m_config[Options::NUM_PARTITIONS];
      if (num_parts >= tlp_threshold) {
        auto num_tlp = num_parts / m_tlp_fanout_ratio;
        auto num_search_tlp = num_tlp / m_tlp_fanout_ratio;
        m_config.set_generated(Options::NUM_TLP, num_tlp);
        m_config.set_generated(Options::NUM_SEARCH_TLP, num_search_tlp);

        search_size = (int)m_config[Options::NUM_QUERY_PARTITIONS];
        search_size *= 1.2;
        m_config.set_generated(Options::NUM_QUERY_PARTITIONS, search_size);
      }
    }
  }

  auto part = m_native_kmeans_config.mutable_partitioning();
  part->Clear();

  // Partitioning configuration
  part->set_max_num_levels(m_config[Options::TREE_LEVELS]);
  part->set_num_children(m_config[Options::NUM_PARTITIONS]);
  part->set_min_cluster_size(m_config[Options::MIN_CLUSTER_SIZE]);
  part->set_max_clustering_iterations(
      m_config[Options::MAX_CLUSTERING_ITERATION]);
  part->set_single_machine_center_initialization(part->RANDOM_INITIALIZATION);
  part->mutable_partitioning_distance()->set_distance_measure(
      m_config[Options::DIST_MEASURE].value());

  if (m_config.find(Options::NUM_TLP)) {
    auto tlp = part->mutable_bottom_up_top_level_partitioner();
    tlp->set_enabled(true);
    tlp->set_num_centroids((int)m_config[Options::NUM_TLP]);
    tlp->set_num_centroids_to_search((int)m_config[Options::NUM_SEARCH_TLP]);
  }

  // Query spilling inside of Partitioning
  part->mutable_query_spilling()->set_spilling_type(
      part->query_spilling().FIXED_NUMBER_OF_CENTERS);
  part->mutable_query_spilling()->set_max_spill_centers(
      std::min((int)m_config[Options::NUM_PARTITIONS],
               (int)m_config[Options::NUM_QUERY_PARTITIONS]));

  // TODO: further test the dist_measure in centroids level
  part->mutable_query_tokenization_distance_override()->set_distance_measure(
      m_config[Options::DIST_MEASURE].value());

  if (DistMeasure(m_config[Options::DIST_MEASURE]) == DistMeasure::COSINE) {
    part->set_partitioning_type(part->SPHERICAL);
  } else {
    part->set_partitioning_type(part->GENERIC);
  }
  part->set_query_tokenization_type(part->FLOAT);
}

int KMeansIndex::compute_num_partitions(size_t data_size) const {
  int tree_size = 0;

  // 1. use 100 data point per partition if the overall data size < 100K
  if (data_size < m_min_meaningful_data_size) {
    tree_size = data_size / m_stable_kmeans_partition_size;

  // 2. use K = sqrt(10V) otherwise
  } else {
    tree_size = std::sqrt(10 * data_size);
  }

  int num_dp_per_partition = tree_size ? data_size / tree_size : 0;

  // 3. if # of data point exceed 200, cap it to 200
  if (num_dp_per_partition > m_ideal_partition_size) {
    tree_size = data_size / m_ideal_partition_size;
  }

  // 4. if # of partitions exceed 10M, use K = 5sqrt(10V) to curve it
  if (data_size >= m_balanced_tlp_threshold) {
    tree_size = 5 * std::sqrt(10 * data_size);
  }

  return tree_size;
}

void KMeansIndex::pop_cached_results(StreamIdT id, Neighbors& dest, size_t cnt) {
  auto query = cached_query(id);

  // determine how many to actually move
  size_t num_pop = std::min(query->cached_results.size(), cnt);
  if (num_pop == 0) {
    return;
  }

  auto head_itr = query->cached_results.begin();
  auto tail_itr = head_itr + num_pop;

  dest.insert(dest.end(),
              std::make_move_iterator(head_itr),
              std::make_move_iterator(tail_itr));

  query->cached_results.erase(head_itr, tail_itr);
  query->num_returned_results += num_pop;
}

void KMeansIndex::pop_cached_parts(StreamIdT id, Partitions& dest, size_t cnt) {
  auto query = cached_query(id);
  auto& cached_parts =
    std::dynamic_pointer_cast<KMeansQueryData>(query)->cached_parts;

  // determine how many to actually move
  size_t num_pop = std::min(cached_parts.size(), cnt);
  if (num_pop == 0) {
    return;
  }

  auto head_itr = cached_parts.begin();
  auto tail_itr = head_itr + num_pop;

  dest.insert(dest.end(),
              std::make_move_iterator(head_itr),
              std::make_move_iterator(tail_itr));

  cached_parts.erase(head_itr, tail_itr);
}

index_err_t KMeansIndex::log_and_get_kmeans_error(const absl::Status& st) const {
  ib::error() << "KMeans code: " << StatusCodeToString(st.code())
              << " message: " << st.message();
  switch (st.code()) {
    case absl::StatusCode::kNotFound:
      return NOT_FOUND;
    case absl::StatusCode::kAlreadyExists:
      return ALREADY_EXISTS;
    default:
      return KMEANS_ERROR;
  }
}

} /* namespace ib_vector */

