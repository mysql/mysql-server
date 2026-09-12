// Copyright 2026 Google LLC

/** @file include/kmeans0index.h
 KMeans index object type in MySQL */

#ifndef _INCLUDE_KMEANS0INDEX_H_
#define _INCLUDE_KMEANS0INDEX_H_

#include <cstdarg>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include <vector0index.h>
#include <vector0types.h>
#include <vector0cfg.h>
#include <vector0persist.h>
#include <vector0quantizer.h>
#include <kmeans0types.h>
#include <kmeans0partitioner.h>

#include <db0err.h>
#include <dict0mem.h>
#include <que0que.h>
#include <trx0trx.h>

#include "absl/status/status.h"


namespace ib_vector {

/* Forward declaration */
struct CreateVectorIndexInfo;

/** Main class for KMeans index in MySQL
This class interfaces with KMeans and MySQL. It packages all KMeans functions and
provides interfaces to other MySQL callers. Each KMeansIndex object represents
one vector index that corresponding to one (no such a concept as combo vector
index at this point) vector column in a table. KMeansIndex owns the native KMeans
search tree (a.k.a searcher). */
class KMeansIndex : public VectorIndex {

 public:
  using VectorIndex::operator=;
  /** Constructor
  @param[in]    idx_name        the index name
  @param[in]    base_table      base table name in 'db_name/table_name' format
  @param[in]    cfg             the index configuration */
  KMeansIndex(const std::string& name, const std::string& base_table,
            VectorCfg&& cfg);

  // getters & setters (for index metadata)

  /** Should be called when first pass is finished and second pass has not
  started yet
  @return true if second pass is needed */
  bool second_pass_needed() const {
    ut_a(m_state.load() == INDEX_PENDING_BUILD);
    ut_a(m_base_table_size_at_build.load() >= m_current_size);
    return m_base_table_size_at_build.load() > m_current_size;
  }

  /** Set the flag that native config has been generated */
  void set_native_config_generated() { m_config_generated.store(true); }

  /** Get a serialized paritioner for persistence
  @param[in]    buf               the buffer to hold the serialized content
  @return true if it succeeded */
  void serialized_partitioner(std::string& buf) const {
    if (m_query_partitioner) {
      m_query_partitioner->SerializeToString(&buf);
    } else {
      buf.clear();
    }
  }

  /** Get multipliers
  @return the multipliers */
  ConstVectorSpan multipliers() const { return m_quantizer.multipliers(); }

  /** Get inverse multipliers
  @return the inverse multipliers */
  ConstVectorSpan inverse_multipliers() const {
    return m_quantizer.inverse_multipliers();
  }

  /** Set the parititioner */
  bool set_partitioner(const char* buf, size_t len);

  /** Restore the mutate partitioner based on query partitioner */
  void restore_mutate_partitioner();

  /** Set the multipliers */
  bool set_multipliers(ConstVectorSpan mult);

  // query interface

  virtual index_err_t get_neighbors(Vector&& query, Neighbors& neighbors,
                                    VectorSearchOptions search_options,
                                    dict_index_t* sub_table_index,
                                    trx_t* trx) override;

  // index modifiers

  index_err_t append_to_dataset(Vector& vector);
  index_err_t train();

  dberr_t collect_training_data(dict_index_t* index, trx_t* trx);

  /** Persist the KMeans index
  @param[in]    type            type of the persistence
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code or DB_SUCCESS
  @remarks The index is only persisted if persistence is supported for the
           corresponding IndexType and the index is in READY_TO_USE state.*/
  virtual dberr_t persist(PersistType type, ...) override;

  /** Reload a KMeans index from a file.
  @param[in]    type            type of the persistence
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code or DB_SUCCESS */
  virtual dberr_t reload(PersistType type, ...) override;

  /** Release the memory */
  virtual void release_memory() override {
    if (releasable()) {
      m_mutate_partitioner.release();
      m_query_partitioner.release();
      MONITOR_INC(MONITOR_VECTOR_INDEXES_NUM_TREE_UNLOAD);
    }
  }

  /* Return the size of the memory used in training */
  virtual size_t training_memory_size() override { return m_training_mem_size; }

  virtual bool releasable() const override { return m_mutate_partitioner; }

  virtual std::pair<std::shared_ptr<VectorPersist>, dberr_t>
  sync_mutation(que_thr_t* thr) override;

  /** Tokenize a vector
  @param[in]    vector          the vector to tokenize
  @param[out]   result          the partition(s) the vector is mapped to
  @param[in]    num_results     override of the number of results, or -1
                                if no overriding
  @return error code if any */
  index_err_t tokenize(const Vector& vector,
                       Partitions* result,
                       int32_t num_results = -1) {
    auto d = kmeans_wrapper::CreateDatapointFromPtr(vector.data(), vector.size());
    auto dp = normalize_data(d.get());
    return m_mutate_partitioner.tokenize(dp, result, num_results);
  }

  /** Quantize a vector
  @param[in]    src             the vector to quantize
  @param[out]   ret             the quantized vector */
  void quantize(ConstVectorSpan src, QuantizedSpan ret) {
    m_quantizer.quantize(src, ret);
  }

  /** Inverse quantize a vector
  @param[in,out]    vector        the vector to be inverse quantized */
  void inverse_quantize(VectorSpan& vector) const {
    m_quantizer.inverse_quantize(vector);
  }

  /** Normalize datapoint before using in KMeans
  @param[in]    dp              the datapoint to be normalized
  @return a DatapointPtr object attached to the normalized data point */
  VectorDatapointPtr normalize_data(VectorDatapointPtr dp);

  // configuration & stats

  /** Set partition information */
  virtual void set_partition_info(VectorIndexStats &info) const override;

  /** Generate a KMeans configuration based on the provided customizations.
  @return true if succeeded. */
  bool generate_kmeans_native_cfg();

  /** Get the native kmeans config */
  const KMeansConfig& native_kmeans_config() const { return m_native_kmeans_config; }

  /** Reset kmeans index object. */
  virtual void reset() override;

  static constexpr uint32_t max_sample_size =
      std::numeric_limits<uint32_t>::max();

 protected:
  /** Persist index depending on the persistence type
  @param[in]    type            the type of persistence
  @param[in]    args            variadic arguments for the detailed impl
  @return error code if any or DB_SUCCESS */
  dberr_t persist_impl(PersistType type, va_list args);

  /** Persist the index to a table
  @param[in]    thr             the thread context (not NULL when doing DML)
  @param[in]    create_info     the create vector index info (not NULL when
                                doing backfill)
  @return error code if any or DB_SUCCESS */
  dberr_t persist_to_table(que_thr_t* thr, CreateVectorIndexInfo* create_info);

  /** Reload index depending on the persistence type
  @param[in]    type            the type of persistence
  @param[in]    args            variadic arguments for the detailed impl
  @return error code if any or DB_SUCCESS */
  dberr_t reload_impl(PersistType type, va_list args);

  /** Reload the index from a table
  @param[in]    base_table      the base table of this index
  @return error code if any or DB_SUCCESS */
  dberr_t reload_from_table(dict_table_t* base_table);

  /** Compuate the K-Means partitions based on the given training data
  @return the pointer to a Partitioner (the actual object is of type
          KMeansTreeLikePartitioner) object, or nullptr if error */
  std::unique_ptr<Partitioner> compute_partitions();

  /** Create query & mutate partitioners
  @param[in]    part            raw partitions computed from training data
  @return true if succeeded */
  bool create_partitioners(std::unique_ptr<Partitioner> part);

  /** Normalize the dataset before sending to KMeans index
  @param[in]    data            the dataset to normalize */
  void normalize_data(std::shared_ptr<Dataset> dataset);

  /** Generate the partitioning section of the KMeans configuration.
  @remarks This is to be called after scanning the base table. */
  void generate_kmeans_partitioning_cfg();

  /** Compute the K partitioning based on the total # of embeddings
  @param[in]    data_size       the total # of embeddings
  @return the number of partitions to use for K-Means partitioning
  @remarks This function is a no-op if num_leaves is already configured by the
           customer. It returns 0 in that case. */
  int compute_num_partitions(size_t data_size) const;

  /** Pop the cached partitions from the cache
  @param[in]    id              the stream id
  @param[out]   dest            the destination partitions
  @param[in]    cnt             the number of partitions to pop
  @remarks In the stream mode query, the first get_neighbors() search in one
           stream series will do the tokenization work. All partitions are
           ranked and cached. They are used in batches if needed. */
  void pop_cached_parts(StreamIdT id, Partitions& dest, size_t cnt);

  /** Pop the cached results from the cache
  @param[in]    id              the stream id
  @param[out]   dest            the destination neighbors
  @param[in]    cnt             the number of neighbors to pop
  @remarks In the stream mode query, nearest neighbors are queries in a larger
           batch than the request size (num_neighbors). Unused results are
           cached for future queries. */
  void pop_cached_results(StreamIdT id, Neighbors& dest, size_t cnt);

  index_err_t log_and_get_kmeans_error(const absl::Status& st) const;

 private:
  // Configuration constants

  /** Minimal number of data points for a supported index */
  static constexpr size_t m_min_data_size = 1000;

  /** Minimal number of data points for a meaningful KMeans index */
  static constexpr size_t m_min_meaningful_data_size = 100 * 1000;

  /** Minimal number of partitions to apply TLP */
  static constexpr int m_min_tlp_threshold = 10000;

  /** Number of centroids to bias on balanced TLP partitioning */
  static constexpr int m_balanced_tlp_threshold = 10 * 1000 * 1000;

  /** TLP fan-out ratio */
  static constexpr int m_tlp_fanout_ratio = 10;

  /** Scale down the num_search_tlp by this factor, when a TLP is used in
  index building */
  static constexpr int m_tlp_scale_down_ratio = 10;

  /** Minimal sampling ratio */
  static constexpr float m_min_sample_ratio = 0.1;

  /** Minimal number of data point to form a meaningful partition */
  static constexpr int m_stable_kmeans_partition_size = 100;

  /** Suggested samples per partition to make the computation stable */
  static constexpr int m_stable_kmeans_sample_size = 50;

  /** Ideal number of data point in a partition */
  static constexpr int m_ideal_partition_size = 200;

  /** Incremental # of vectors requested for training purpose */
  static constexpr int m_dataset_chunk_size = 1000;

  /** Default number of query partitions */
  static constexpr int m_query_spilling_size = 100;

  /** Size of the memory (provided from outside) used for training */
  size_t m_training_mem_size {0};

  /** If we encountered an error during training */
  index_err_t m_training_err {SUCCESS};

  // KMeans related data

  /** Partitioner that select A(1) suitable partition for a new data point  */
  KMeansPartitioner m_mutate_partitioner;

  /** Partitioner that select suitable partitions for queries */
  KMeansPartitioner m_query_partitioner;

  /** Quantizer */
  VectorQuantizer m_quantizer;

  /** Native KMeans configuration object */
  KMeansConfig m_native_kmeans_config;

  /** Set to true once the native config has been generated */
  std::atomic<bool> m_config_generated{false};

  /** Dataset that the index works with */
  std::shared_ptr<Dataset> m_dataset;
};

} /* namespace ib_vector */

#endif /* _INCLUDE_KMEANS0INDEX_H_ */
