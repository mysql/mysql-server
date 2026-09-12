// Copyright 2026 Google LLC

/** @file include/vector0index.h
 Vector index object type in MySQL */

#ifndef _INCLUDE_VECTOR0INDEX_H_
#define _INCLUDE_VECTOR0INDEX_H_

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include <vector0cfg.h>
#include <vector0types.h>

#include <dict0mem.h>
#include <que0que.h>
#include <trx0rec.h>
#include "trx0trx.h"
#include <ut0dbg.h>
#include "vector0persist.h"

#include "absl/synchronization/mutex.h"

namespace ib_vector {

/** Abstract base class for vector index operations.
This abstract class provides a unified interface for interacting with vector
indices. It outlines the common methods and properties required for building,
manipulating, and searching across different vector index libraries. */
class VectorIndexInterface {
 public:
  /** Constructor
  @param[in]    idx_name        the index name
  @param[in]    base_table      base table name in 'db_name/table_name' format
  @param[in]    cfg             the index configuration */
  VectorIndexInterface(const std::string& name, const std::string& base_table):
      m_name(name),
      m_base_table(base_table) {}

  /* No copying but moving is OK */
  VectorIndexInterface(VectorIndexInterface&&) = default;
  VectorIndexInterface& operator=(VectorIndexInterface&&) = default;
  VectorIndexInterface(const VectorIndexInterface&) = delete;
  virtual VectorIndexInterface& operator=(const VectorIndexInterface&) = delete;

  virtual ~VectorIndexInterface() = default;

  // query interface
    virtual index_err_t get_neighbors(Vector&& query, Neighbors& neighbors,
                                    VectorSearchOptions search_options,
                                    dict_index_t* sub_table_index = nullptr,
                                    trx_t* trx = nullptr) = 0;

  // streaming interface

  /** Add a cached query buffer in the query cache
  @param[in]    stream_id   the stream id of the cached query to be created
  @param[in]    ptr         pointer to the cached query buffer */
  virtual void add_cached_query(StreamIdT stream_id,
                                std::shared_ptr<QueryData> ptr) = 0;

  /** Release the cached query for the given stream id
  @param[in]    stream_id   the stream id to release */
  virtual void release_cached_query(StreamIdT stream_id) = 0;

  /** Check if there is a cached query for the given stream id
  @param[in]    stream_id   the stream id to check
  @return true if there is one */
  virtual bool has_cached_query(StreamIdT stream_id) = 0;

  /** Get the cached query for the given stream id
  @param[in]    stream_id   the stream id to get the cached query for
  @return a pointer to the cached query or nullptr if not found */
  virtual std::shared_ptr<QueryData> cached_query(StreamIdT stream_id) = 0;

  /** Get the cached sub table handle for the given stream id
  @param[in]    stream_id   the stream id to get the cached sub table for
  @return the cached sub table handle or nullptr if not found */
  virtual void* cached_sub_table(StreamIdT stream_id) = 0;

  // index modifiers

  /* Persist vector index
  @param[in]    type            persistence type
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code, or DB_SUCCESS
  @remarks The index should only persisted if persistence is supported for the
           IndexType and the index is in READY_TO_USE state. */
  virtual dberr_t persist(PersistType type, ...) = 0;

  /* Reload vector index
  @param[in]    type            persistence type
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code, or DB_SUCCESS */
  virtual dberr_t reload(PersistType type, ...) = 0;

  /** Release index memory */
  virtual void release_memory() = 0;

  /* Return the size of the memory used in training */
  virtual size_t training_memory_size() = 0;

  /** Check if this index has releasable memory or not
  @return true if there are releasable memory
  @remarks We can achieve the same goal by given set_state) a return value
           but that will make the function harder to read */
  virtual bool releasable() const = 0;

  virtual std::pair<std::shared_ptr<VectorPersist>, dberr_t>
  sync_mutation(que_thr_t* thr) = 0;

  /* Reset vector index object. */
  virtual void reset() = 0;

 protected:
  // Index internal data

  /** Name of the index, to support multiple indices on a vector column */
  const std::string m_name;

  /** Base table name, in db/table format */
  const std::string m_base_table;
};

/** Base class for vector index in MySQL
This class serves as the foundation for all vector index types in MySQL. It
extends the `VectorIndexInterface` to provide essential operations and shared
functionality for all vector index types. Specific index implementations
(e.g., KMEANSINdex, HNSWIndex) inherit and extend this base class.*/
class VectorIndex : public VectorIndexInterface {
 public:
  using VectorIndexInterface::operator=;
  /** Constructor
  @param[in]    idx_name        the index name
  @param[in]    base_table      base table name in 'db_name/table_name' format
  @param[in]    cfg             the index configuration */
  VectorIndex(const std::string& name, const std::string& base_table,
              VectorCfg&& cfg) :
    VectorIndexInterface(name, base_table),
    m_config(std::move(cfg)) {}

  // getters & setters (for index metadata)

  /** Get the vector dimension
  @return dimension that is configured by the customer */
  int dims() const { return m_config[Options::VECTOR_DIMENSION]; }

  /** Get the number of partitions
  @return partitions that is configured by the customer */
  int num_partitions() const { return m_config[Options::NUM_PARTITIONS]; }


  /** Get the index type
  @return the type enum */
  IndexType type() const { return m_config[Options::INDEX_TYPE]; }

  /** Get the distance measure
  @return the distance measure algorithm enum */
  DistMeasure dist_measure() const { return m_config[Options::DIST_MEASURE]; }

  // getters (for object internal data)

  /** Get the configuration
  @return a reference to the internal VectorCfg object */
  VectorCfg& config() { return m_config; }

  /** Initial size of index when it was built
  @return initial size */
  uint32_t initial_size() const { return m_base_table_size_at_build.load(); }

  /** Set initial index size during reload */
  void set_initial_size(uint32_t size) {
    ut_ad(m_state.load() == INDEX_PENDING_TRAIN);
    ut_ad(m_base_table_size_at_build.load() == 0);
    m_base_table_size_at_build.store(size);
  }

  /** Get the current count of vectors in the index
  @return the count */
  int size() const { return m_current_size.load(); }


  // getters (for object internal data)

  /** Get the index name
  @return a copy of the name */
  std::string name() const { return m_name; }
  void set_name(const std::string& name) {
    const_cast<std::string&>(m_name) = name;
  }

  /** Get the base table name
  @return a copy of the base table name */
  std::string base_table() const { return m_base_table; }
  void set_base_table_name(const std::string& name) {
    const_cast<std::string&>(m_base_table) = name;
  }

  /** Get PK fixed length
  @return the fixed length of the primary key or zero if the key has variable
  length fields. */
  uint16_t pk_fixed_len() const { return m_pk_fixed_len; }
  void set_pk_fixed_len(uint16_t len) {
    const_cast<uint16_t&>(m_pk_fixed_len) = len;
  }

  /** Increment base table size counter atomically */
  void inc_base_table_size() {
    ut_ad(m_state.load() == INDEX_PENDING_BUILD ||
          m_state.load() == INDEX_PENDING_TRAIN);
    m_base_table_size_at_build.fetch_add(1, std::memory_order_relaxed);
  }

  /** Reset base table size to zero. Needed if we are doing two passes. */
  void reset_base_table_size() {
    ut_ad(m_state.load() == INDEX_PENDING_BUILD ||
          m_state.load() == INDEX_PENDING_TRAIN);
    m_base_table_size_at_build.store(0);
  }

  /** Get the state of index
  @return the state (INDEX_READY_TO_USE, INDEX_PENDING_BUILD, etc) of the
  underlying vector index object */
  IndexState state() const { return m_state.load(); }

  /** Set the state of index. Know what you are doing. Externally, the state
  should be set only to INDEX_READY_TO_USE or INDEX_NOT_USABLE. Though for now
  we allow setting it to INDEX_PENDING_LOAD.
  @param[in]    state   the new state */
  void set_state(IndexState state) {
    // can't move to, or away from INDEX_NOT_USABLE state
    ut_a(m_state != INDEX_NOT_USABLE || state != INDEX_NOT_USABLE);

    switch (state) {
      case INDEX_READY_TO_USE:
        m_status = "Ready";
        break;
      case INDEX_PENDING_TRAIN:
      case INDEX_PENDING_BUILD:
        /* Not allowed */
        ut_error;
        break;
      case INDEX_PENDING_LOAD:
        release_memory();
        m_status = "Pending Load";
        break;
      default:
        // just to please the compiler, actual check was the ut_a() above
        break;
    }
    m_state.store(state);
  }

  /** Mark the index as unusable. Once marked there is no turning back */
  void set_unusable() {
    if (releasable()) {
      release_memory();
    }
    m_state.store(INDEX_NOT_USABLE);
    m_status = "Not usable";
  }

  void set_status(const std::string& status) { m_status = status; }

  // query interface
  virtual index_err_t get_neighbors(Vector&& query, Neighbors& neighbors,
                                    VectorSearchOptions search_options,
                                    dict_index_t* sub_table_index = nullptr,
                                    trx_t* trx = nullptr) override {
    return NOT_IMPLEMENTED;
  }

  // streaming interface

  virtual void add_cached_query(StreamIdT stream_id,
                                std::shared_ptr<QueryData> ptr) override {
    absl::WriterMutexLock lock(m_query_cache.mutex);
    m_query_cache.cache[stream_id] = ptr;
  }

  virtual void release_cached_query(StreamIdT stream_id) override {
    absl::WriterMutexLock lock(m_query_cache.mutex);
    auto itr = m_query_cache.cache.find(stream_id);
    ut_ad(m_query_cache.cache.end() != itr);
    m_query_cache.cache.erase(itr);
  }

  virtual bool has_cached_query(StreamIdT stream_id) override {
    absl::ReaderMutexLock lock(m_query_cache.mutex);
    return m_query_cache.cache.end() != m_query_cache.cache.find(stream_id);
  }

  virtual std::shared_ptr<QueryData>
  cached_query(StreamIdT stream_id) override {
    absl::ReaderMutexLock lock(m_query_cache.mutex);
    const auto& itr = m_query_cache.cache.find(stream_id);
    return m_query_cache.cache.end() != itr ? itr->second : nullptr;
  }

  virtual void* cached_sub_table(StreamIdT stream_id) override {
    absl::ReaderMutexLock lock(m_query_cache.mutex);
    auto itr = m_query_cache.cache.find(stream_id);
    // it's possible to call this from cloudsql_vector_ann_cleanup() after a
    // previously failed cloudsql_vector_ann_search(), there is nothing
    // to clean up in that case
    return (m_query_cache.cache.end() != itr)
            ? itr->second->saved_sub_table_handle
            : nullptr;
  }

  /* Persist vector index to a file. The index is only persisted if persistence
  is supported for the IndexType and the index is in READY_TO_USE state.
  @param[in]    type            persistence type
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code, or DB_SUCCESS */
  virtual dberr_t persist(PersistType type, ...) override { return DB_UNSUPPORTED; }

  /* Reload vector index
  @param[in]    type            persistence type
  @param[in]    ...             variadic arguments for the detailed impl
  @return error code, or DB_SUCCESS */
  virtual dberr_t reload(PersistType type, ...) override {
    return DB_UNSUPPORTED;
  }

  /** Rlease the memory */
  virtual void release_memory() override { ut_error; }

  /* Return the size of the memroy used in training */
  virtual size_t training_memory_size() override { ut_error; }

  /** Check if this index has releasable memory or not
  @return true if there are releasable memory */
  virtual bool releasable() const override { ut_error; }

  virtual std::pair<std::shared_ptr<VectorPersist>, dberr_t>
  sync_mutation(que_thr_t* thr) override {
    return std::pair<std::shared_ptr<VectorPersist>, dberr_t>
               (nullptr, DB_ERROR);
  }

  // Set partition information
  virtual void set_partition_info(VectorIndexStats& info) const {}

  /** Fill in the static information for innodb_vector_indexes
  @param[in]    info    the structure to fill in the information */
  void fill_i_s_info(VectorIndexStats& info) const {
    IndexType type = m_config[Options::INDEX_TYPE];
    info.name_ = m_name;
    info.table_name_ = m_base_table;
    std::replace(info.table_name_.begin(), info.table_name_.end(), '/', '.');
    info.type_ = IndexTypeToString(type);
    info.dimension_ = dims();
    info.dist_measure_ =
        DistMeasureToUserString(m_config[Options::DIST_MEASURE]);
    info.index_state_ = IndexStateToString(m_state.load());
    set_partition_info(info);
    info.status_ = m_status;
  }

  /* Reset vector index object. */
  virtual void reset() override {}

 protected:
  /** Cache for streaming queries */
  struct VectorQueryCache {
    std::map<StreamIdT, std::shared_ptr<QueryData>> cache;
    // this is not to provide exclusive access to each individual cache slot,
    // but to make sure the underlying tree structure is sane when concurrent
    // insert/delete happens on the cache
    mutable absl::Mutex mutex;
  };
  VectorQueryCache m_query_cache;

  /** This contains the size on non-NULL rows in the base table.  It is updated
  during the index build time. */
  std::atomic<uint32_t> m_base_table_size_at_build{0};

  /** Current size of the index */
  std::atomic<uint32_t> m_current_size{0};

  /** Index state, barrier fended for concurrent access */
  std::atomic<IndexState> m_state;

  /** Primary key length in bytes. This is set iff PK consists of only fixed
  size fields. */
  const uint16_t m_pk_fixed_len{0};

  /** Mutex for index write access */
  mutable absl::Mutex m_mutex;

  /** Vector configurations with edit tracking & prioritizing */
  VectorCfg m_config;

  /** An informative string about the current status of the index. This is
  more for informational purpose and should not be confused with m_state which
  represents internal state of the index. */
  std::string m_status{"UNKNOWN"};

  /* Stats */
  std::atomic<uint64_t> m_queries{0};
  std::atomic<uint64_t> m_mutations{0};
  std::atomic<uint64_t> m_appends{0};
};

} /* namespace ib_vector */

#endif /* INCLUDE_VECTOR0INDEX_H_ */
