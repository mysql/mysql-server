// Copyright 2026 Google LLC

#ifndef _INCLUDE_VECTOR0DD_H_
#define _INCLUDE_VECTOR0DD_H_

#include <stdbool.h>

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "vector0index.h"
#include "vector0persist.h"
#include "vector0vector.h"

#include "my_config.h"
#include "univ.i"

#include "db0err.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "sql/dd/types/column.h"
#include "sync0debug.h"
#include "sync0rw.h"
#include "sync0sync.h"
#include "ut0dbg.h"
#include "ut0log.h"
#include "ut0new.h"

namespace ib_vector {

static inline bool valid_dimensions(uint32_t dim) {
  return dim >= VECTOR_MIN_SUPPORTED_DIMENSIONS
      && dim <= VECTOR_MAX_SUPPORTED_DIMENSIONS;
}

/* This contains information about vector column in a table. Note that we allow
only one vector column per table. A shared pointer of this struct will hang off
from dict_table_t. As the information is immutable, we don't need to define this
as a class with private members. */
struct VectorColInfo {
  /* Constructor
  @param[in]    table_name    table name in db_name/table_name format
  @param[in]    col_pos       column position in dict_table_t def
  @param[in]    dim           dimensionality */
  VectorColInfo(const char* table_name, uint16_t col_pos, size_t dim)
      : table_name_(table_name), col_pos_(col_pos), dim_(dim) {}

  /* We need to update the table_name during RENAME table DDL. */
  void set_table_name(const char* table_name) { table_name_ = table_name; }

  /* We need a dict_index_t for vector_index. This will hang from the
  regular list of indexes in dict_table_t as well. It is kind of a special
  index. Much like FTS_DOC_ID_INDEX which is part of index list as well but also
  have a pointer directly in dict_table_t */
  dict_index_t* persistent_vec_index_{nullptr};

  /** Table name in db_name/table_name format */
  std::string table_name_;

  /* This is column position in the table definition. This is not the same as
  position in the clustered index. Clustered index will have PK columns first
  followed by two system columns followed by user defined columns. */
  const uint16_t col_pos_{UINT16_UNDEFINED};
  const size_t dim_{0};

  /** Dead code  */
  std::shared_ptr<VectorIndex> index() const { return nullptr; }
};

/** This class holds all the information needed to perform operations on a
vector index. The main operations are DML and ANN. We hold the sub_table_id to
be able to open it. We also hold the pointer to the actual vector index. */
class VectorIndexInfo : private ut::Non_copyable {
  // IndexRegistry needs to access the per-index latch, but we don't want to
  // expose this latch to the general public.
  friend class IndexRegistry;

 public:
  /** Default constructor */
  VectorIndexInfo() {
    m_latch = static_cast<rw_lock_t *>(ut::malloc(sizeof(rw_lock_t)));
    ut_a(m_latch);
    rw_lock_create(vector_index_key, m_latch, LATCH_ID_VECTOR_INDEX);
  }

  /** Destructor */
  virtual ~VectorIndexInfo() {
    rw_lock_free(m_latch);
    ut::free(m_latch);
  }

  table_id_t sub_table_id() const { return m_sub_table_id; }
  std::shared_ptr<VectorIndex> index() const { return m_index; }
  uint64_t tree_size() const { return m_tree_size; }

  /** Set the sub table id. When creating an index the sub_table id is known
  before the index is created.
  @param[in]    sub_table_id    sub table id */
  void set_sub_table_id(table_id_t sub_table_id) {
    ut_ad(m_sub_table_id == 0);
    ut_ad(m_index == nullptr);

    m_sub_table_id = sub_table_id;
  }

  /** Set the vector index pointer. This is set when the index is created. At
  this point the sub_table is not populated yet.
  @param[in]    index   vector index pointer */
  void set_index(std::shared_ptr<VectorIndex> index) {
    ut_ad(m_sub_table_id != 0);
    ut_ad(m_index == nullptr);
    m_index = index;
  }

  /** Set the tree size. This is set after the sub_table has been populated.
  @param[in]    tree_size   size of the tree */
  void set_tree_size(uint64_t tree_size) {
    ut_ad(m_index != nullptr);
    m_tree_size = tree_size;
  }

  /** Write the information to DD mysql.indexes.se_private_data
  @param[in]    p        DD properties object
  @param[in]    index    vector index handle */
  void write_to_dd(dd::Properties& p, const dict_index_t* index);

  /** Read the information from DD mysql.indexes.se_private_data. We'll perform
  following steps:
  1) Read the vector index information from DD.
  2) Open the sub table.
  3) Pin the sub table.
  4) Create the vector index object based on config read from DD.
  5) Set the index state to INDEX_PENDING_LOAD.
  6) After this point any DML or ANN will force a load of the index tree.
  @param[in]    p        DD properties object
  @param[in]    index    vector index handle
  @remark In case of error index is marked as corrupt. */
  void read_from_dd(const dd::Properties& p, const dict_index_t* index);

  /** There are multiple reasons we can potentially mark the VectorIndex object
  as unusable.
  1) We are unable to read the index metadata from DD.
  2) We are unable to load the index tree.
  3) CHECK TABLE marks the index corrupt.
  4) ...
  This function should be called when the intention is to synchronize
  dict_index_t for vector index and the VectorIndex object. This will ensure
  that if either dict_index_t is marked as corrupt or VectorIndex object is
  null, we come out it with these flags set to corrupt and INDEX_NOT_USABLE.
  @param[in]    index    vector index handle */
  void mark_unusable_if_needed(dict_index_t* index);

  /** Mark the index ready explicitly after the building process is done */
  void mark_ready();

  bool is_ready() const {
    if (m_index == nullptr) {
      return false;
    }

    return m_index->state() == INDEX_READY_TO_USE;
  }

  bool is_available() const {
    if (m_index == nullptr) {
      return false;
    }
    return m_index->state() == INDEX_READY_TO_USE ||
           m_index->state() == INDEX_PENDING_LOAD;
  }

  /** Load the sub_table (without any MDL lock)
  @return handle to the sub table or nullptr */
  dict_table_t* open_sub_table() const;

  /** Load the vector index and open sub_table.
  @param[in]    table   table handle of base table
  @return handle to the sub table or nullptr
  @remarks This function can (and usually will) be called without knowing the
           state of the index. The only requirement is that the index must
           exists. The function returns with 2 possible outcomes:
             1. The index is S-latched and ready to use. sub_table is opened
                and its handle is returned
             2. The index is found unusable, in which case the VectorIndex
                aspect of it is marked as `INDEX_NOT_USABLE` while the
                dict_index_t part is marked corrupt in DD. A nullptr is
                returned, no latch on the VecIndexInfo structure. */
  dict_table_t * load_index_and_open_sub_table(dict_table_t * table);

  /** Release the memory lock held by the index */
  void release_index_lock() { rw_lock_s_unlock(m_latch); }

  /** Close the sub table.
  @param[in]    sub_table   sub table handle */
  void close_sub_table(dict_table_t* sub_table) const;

  /** Fill i_s table innodb_vector_indexes stats */
  VectorIndexStats fill_i_s_info() const;
  void inc_queries() { m_queries.fetch_add(1, std::memory_order_relaxed); }
  void inc_mutations() { m_mutations.fetch_add(1, std::memory_order_relaxed); }

 protected:
  /** Load the index if necessary and lock it in S-mode
  @param[in]    table   base table of this index
  @return DB_SUCCESS or error code */
  dberr_t load_and_lock_index(dict_table_t* table);

 private:
  bool ready_for_write_to_dd() const {
    return m_sub_table_id != 0 && m_index != nullptr && m_tree_size != 0;
  }

  /** id of the sub table that we'll use to open the sub table. */
  table_id_t m_sub_table_id{0};

  /** This is the size of the non-leaf tree which is set the first time
  we create the index and persist the tree (during CREATE VECTOR INDEX).
  We need this information to confirm that we have enough memory to load the
  tree. Note that this is the size of the BLOB field we stored when writing
  the tree to disk. We should add some slack for other memory overhead. */
  uint64_t m_tree_size{0};

  /** Latch to protect the access of m_index */
  rw_lock_t* m_latch {nullptr};

  /** Pointer to the vector index object. */
  std::shared_ptr<VectorIndex> m_index{nullptr};

  /** Stats */
  std::atomic<uint64_t> m_queries{0};
  std::atomic<uint64_t> m_mutations{0};

  /** Thrashing retry threshold */
  static constexpr size_t m_thrashing_retry_limit = 1000;
};

/* Dictionary functions to access persistent vector index. We must always
use these functions instead of the earlier one we defined for in-memory vector
indexes. This can be a little confusing. We have prefixed these functions with
dict_ to make it clear that these are for persistent vector indexes.
There is a five level deep structure here.

1) dict_tabel_t
  2) shared_ptr<VectorColInfo> vector_col_info  // iff vector col is defined
    3) dict_index_t* persistent_vec_index_      // iff vector index is created
      4) shared_ptr<VectorIndexInfo> vec_index_info // created during CREATE
VECTOR INDEX
        5) shared_ptr<VectorIndex> m_index      // actual vector index obj

Note that barring CREATE/DROP INDEX operations when things are in flux, we'll
hold the following invariants. These invariants are when things work. For
example, if we encounter an error during some KMeans operation the index state can
different then INDEX_READY_TO_USE. In such cases we'll return an error to the
upper layers. The idea here is that following invariants must hold for vector
indexes to work properly.

1) If base_table is loaded in memory and it has an index of type DICT_VECTOR
  a) There must exist a sub_table and it will be loaded in the memory as well
2) sub_table, once loaded in the memory, will be pinned i.e.: it will stay in
the memory.
3) persistent_vector_index_ != nullptr implies:
  a) vec_index_info must be not null
  b) m_index must be not null */

/* Return true we have called CREATE VECTOR INDEX on the table. */
inline bool dict_table_has_vector_index(const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return false;
  }
  bool ret = table->vector_col_info != nullptr &&
             table->vector_col_info->persistent_vec_index_ != nullptr;
  /* If we have a dict_index_t we must also have VectorIndexInfo. */
  ut_a(!ret || table->vector_col_info->persistent_vec_index_->vec_index_info !=
                   nullptr);
  return ret;
}

/* Return the dict_index_t for vector index. */
inline dict_index_t* dict_table_get_vector_index(const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return nullptr;
  }
  if (dict_table_has_vector_index(table)) {
    return table->vector_col_info->persistent_vec_index_;
  }
  return nullptr;
}

/* Return the VectorIndexInfo for vector index. */
inline std::shared_ptr<VectorIndexInfo> dict_table_get_vector_index_info(
    const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return nullptr;
  }
  if (dict_table_has_vector_index(table)) {
    return table->vector_col_info->persistent_vec_index_->vec_index_info;
  }
  return nullptr;
}

/* Return the VectorIndex of type T. */
template <typename T>
inline std::shared_ptr<T> dict_table_get_vector_index_ptr(
    const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return nullptr;
  }
  if (dict_table_has_vector_index(table)) {
    return dynamic_pointer_cast<T>(
        table->vector_col_info->persistent_vec_index_->vec_index_info->index());
  }
  return nullptr;
}

/* Return the VectorIndex object. */
inline std::shared_ptr<VectorIndex> dict_table_get_vector_index_ptr(
    const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return nullptr;
  }
  if (dict_table_has_vector_index(table)) {
    return table->vector_col_info->persistent_vec_index_->vec_index_info
        ->index();
  }
  return nullptr;
}

/* Return true if the vector index is ready to use. */
inline bool dict_table_vector_index_is_ready(const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return false;
  }
  auto vec_info = dict_table_get_vector_index_info(table);
  return (vec_info && vec_info->is_ready());
}

/* Return true if the vector index is available to use. This means it is either
in INDEX_READY_TO_USE or INDEX_PENDING_LOAD state. In later case, it is the job
of the caller to load the index. */
inline bool dict_table_vector_index_is_available(const dict_table_t* table) {
  if (!opt_cloudsql_vector) {
    return false;
  }
  auto vec_info = dict_table_get_vector_index_info(table);
  return (vec_info && vec_info->is_available());
}

/* If the column comment tells us that it is a vector column then add
VectorColInfo to the table
@param[in,out]    table     InnoDB table handle
@param[in]        column    MySQL column handle */
void set_vector_col_info(dict_table_t* table, const dd::Column* column);

inline bool table_has_vector_col(const dict_table_t* table) {
  return table->vector_col_info != nullptr;
}

inline bool index_has_vector_col(const dict_index_t* index) {
  return index->is_clustered() && table_has_vector_col(index->table);
}

inline std::shared_ptr<VectorColInfo> get_vector_col_info(
    const dict_index_t* index) {
  return index->table->vector_col_info;
}

/* Dead code */
inline std::shared_ptr<VectorIndex> get_vector_index(const dict_index_t*) {
  return nullptr;
}

inline std::shared_ptr<VectorIndex> get_vector_index(const dict_table_t*) {
  return nullptr;
}

inline uint32_t get_vector_dim(const dict_index_t* index) {
  if (index->is_clustered()) {
    auto col_info = get_vector_col_info(index);
    if (col_info != nullptr) {
      return col_info->dim_;
    }
  }
  return UINT32_UNDEFINED;
}

/* Returns the position of vector column in the clustered index. */
inline uint32_t get_vector_col_pos(const dict_index_t* index) {
  if (index->is_clustered()) {
    auto col_info = get_vector_col_info(index);
    if (col_info != nullptr) {
      const dict_col_t* col = index->table->get_col(col_info->col_pos_);
      ut_a(col);
      return index->get_logical_pos(col->get_col_phy_pos());
    }
  }
  return UINT32_UNDEFINED;
}

/* A repository of vector indexes loaded in memory. Also tracks memory usage. */
class IndexRegistry {
 public:
  /* Constructor
  @param[in]    total_mem     toal size of dedicated vector index memroy */
  IndexRegistry(size_t total_mem);
  ~IndexRegistry();

  /* No copying or move */
  IndexRegistry() = delete;
  IndexRegistry(IndexRegistry&&) = delete;
  IndexRegistry& operator=(IndexRegistry&&) = delete;
  IndexRegistry(const IndexRegistry&) = delete;
  IndexRegistry& operator=(const IndexRegistry&) = delete;

  void i_s_info(std::vector<VectorIndexStats>& vector_info);
  void i_s_memory_info(VectorIndexMemoryInfo& info);

  /** Register a persistent index
  @param[in]    index_id     dict_index_t::id of vector index
  @param[in]    info         vector index info
  @return true on success */
  bool register_persistent_index(space_index_t index_id,
                                 std::shared_ptr<VectorIndexInfo> info) {
    rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
    auto ret = m_persistent_index_map.insert({index_id, info});
    m_index_lrd.push_back(index_id);
    if (info->is_ready()) {
      auto size = info->tree_size();
      DBUG_EXECUTE_IF("vector_mem_mgmt_test", size = m_fake_index_size;);
      m_index_memory.fetch_add(size);
    }
    rw_lock_x_unlock(m_latch);
    return ret.second;
  }

  /** Unregister a persistent index
  @param[in]    index_id     dict_index_t::id of vector index
  @return true on success */
  bool unregister_persistent_index(space_index_t index_id) {
    rw_lock_x_lock(m_latch, UT_LOCATION_HERE);

    size_t freed = 0;

    {
      auto to_remove = m_persistent_index_map.find(index_id);

      if (to_remove == m_persistent_index_map.end()) {
        rw_lock_x_unlock(m_latch);
        ib::warn() << "Vector index already unregistered";
        return true;
      }

      // must obtain an X-latch to delete an index, or else we may race with
      // another thread that is using this index.
      rw_lock_x_lock(to_remove->second->m_latch, UT_LOCATION_HERE);
      rw_lock_x_unlock(to_remove->second->m_latch);

      // since index_registry is still under X-latch, nobody can visit this
      // to_remove index, even it is unlocked from index level

      if (to_remove->second->is_ready() &&
          to_remove->second->index()->releasable()) {
        freed = to_remove->second->tree_size();
        DBUG_EXECUTE_IF("vector_mem_mgmt_test", freed = m_fake_index_size;);
      }
    }

    // by now the to_remove is already out-of-scope

    auto ret = m_persistent_index_map.erase(index_id);
    auto itr = std::find(m_index_lrd.begin(), m_index_lrd.end(), index_id);
    ut_a(itr != m_index_lrd.end());
    m_index_lrd.erase(itr);
    m_index_memory.fetch_sub(freed);

    // by now, there are no traces for that index left, even phantom ones

    rw_lock_x_unlock(m_latch);

    return ret == 1;
  }

  /** Check if a persistent index is registered
  @param[in]    index_id     dict_index_t::id of vector index
  @return true if index is registered */
  bool persistent_index_registered(space_index_t index_id) {
    rw_lock_s_lock(m_latch, UT_LOCATION_HERE);
    auto ret = m_persistent_index_map.contains(index_id);
    rw_lock_s_unlock(m_latch);
    return ret;
  }

  /** Request some index memory to inflate a vector index
  @param[in]    requester    the index that is requesting memory
  @return true if memory is made available
  @remarks This call does not have prerequesite for index regristry latch, but
           the requester must have been X-latched.*/
  bool request_memory(const VectorIndexInfo& requester);

  /** Request index training memory
  @param[in]    size        size of memory to request
  @return true if memory is made available
  @remarks This call does not have prerequesite for index regristry latch. */
  bool request_memory(size_t size);

  /** Release the index memory
  @param[in]    size        size of memory to release
  @remarks This call does not have prerequesite for index regristry latch. */
  void release_memory(const VectorIndexInfo& donor);

  /** Release the index training memory
  @param[in]    size        size of memory to release
  @remarks This call does not have prerequesite for index regristry latch. */
  void release_memory(size_t size);

  std::ostream& print_mem_info(std::ostream& os) const {
    os << "IndexRegistry::MemoryInfo: total_memory: " << m_total_memory
       << ", max_training_memory: "
       << (size_t)(m_total_memory * m_training_memory_ratio)
       << ", reserved_index_memory: " << m_index_memory.load()
       << ", reserved_training_memory: " << m_training_memory.load();
    return os;
  }

  /** Update the max memory size
  @remarks This call does not have prerequesite for index regristry latch. */
  void update_max_mem_size();

  /** Disable the memory regulator */
  void disable_memory_regulator() {
    rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
    m_disabled = true;
    rw_lock_x_unlock(m_latch);
  }

 protected:
  /** Try free up the vector index memory
  @return true if the requested memory is reserved
  @remarks The caller must have acquired the X-latch on the index registry.
           The caller should try to do some calculation first to  ensure
           (best-effort) that there is enough memory left to satisfy the
           request. This function involves locking individual indexes so
           efficiency is vital. */
  bool free_up_memory(size_t requested);

 private:
  /* Latch protecting all fields. */
  rw_lock_t* m_latch {nullptr};

  /** Map of persistent vector indexes */
  std::unordered_map<space_index_t, std::shared_ptr<VectorIndexInfo>>
      m_persistent_index_map;

  /** List of all vector indexes roughly in the recently deflated order */
  std::list<space_index_t> m_index_lrd;

  /** A backdoor flag to allow disabling memory regulator. This can only be set
  to true once. Once it is set, it cannot be reset. This imples that we can read
  it safely without locking. */
  bool m_disabled{false};

  /** Size of memory currently used by vector indexes */
  std::atomic<size_t> m_index_memory;

  /** Size of memory currently used by training vector indexes */
  std::atomic<size_t> m_training_memory;

  /** Size of the memory allocated for vector indexes */
  size_t m_total_memory;

  /** Portion of the vector index memory dedicated for training purpose */
  static constexpr float m_training_memory_ratio {0.8};

  /** Test Setup for Memory Management
  We fake the sizes used in index memory manament system, and its function
  calls, in test mode (+d,vector_mem_mgmt_test).
    total : 100
    build : 80
    index : 20
  We will also hard-code the requested & released value, right before the
  call reaches request_memory(), release_memory() and free_up_memory(),
  regardless of the real tree_size() value.

  This way, the memory management operates based on a mocked index size but the
  actual indexes are intact.

  With that mocked memory size, we also mock the index traiing & tree sizes
    tree_size : 15
    training  : 30
  Note we do not have time to engineer a sophisticated test harnness just for
  this, so please use a small dataset for training. Big dataset will likely
  call `requrest_memory()` multiple times, therefore screw up the calculation.

  Together with the total memory size, we are able to easily test below
  scenarios:
    - 6 indexes can co-exist but the 7th will have to deflate others
    - 2 builds can co-exist but the 3rd cannot, build will fail
    - when there are 5 indexes, a build will need to deflate
    - when there are 4 indexes, the 2nd build  will deflate indexes
    - when there are 2 build sessions, 3rd index has to inflate others
    - combinations of above,
    - etc, etc,
    - sky is the limit */
 public:
  /** Fake vector index memory region size, for test only */
  static constexpr size_t m_fake_total_memory = 100;

  /** Fake vector index size, for test only */
  static constexpr size_t m_fake_index_size = 15;

  /** Fake training dataset size, for test only */
  static constexpr size_t m_fake_training_size = 30;
};

inline std::ostream& operator<<(std::ostream& os,
                                const IndexRegistry& registry) {
  return registry.print_mem_info(os);
}

extern std::unique_ptr<IndexRegistry> index_registry;
dberr_t init_vector_index_registry(size_t mem_limit);
void disable_vector_index_memory_regulator();
void shutdown_index_registry();

/** Cleanup vector index when the index is removed from the dictionary cache.
@param[in]    index        dict_index_t of the vector index */
void index_vector_cleanup(dict_index_t* index);

/** Process one mysql.indexes record and get the dict_index_t. This only
processes vector indexes.
@param[in]      heap            Temp memory heap
@param[in,out]  rec             mysql.indexes record
@param[in,out]  index           dict_index_t to fill
@param[in]      mdl             MDL on index->table
@param[in]      dd_indexes      dict_table_t obj of mysql.indexes
@param[in]      mtr             Mini-transaction
@retval true if index is filled */
bool process_dd_indexes_rec(mem_heap_t* heap, const rec_t* rec,
                            const dict_index_t** index, MDL_ticket** mdl,
                            dict_table_t* dd_indexes, mtr_t* mtr);

inline void i_s_fill_vector_info(std::vector<VectorIndexStats>& vector_info) {
  if (!opt_cloudsql_vector) {
    return;
  }
  index_registry->i_s_info(vector_info);
}

inline void i_s_fill_vector_memoy_info(VectorIndexMemoryInfo& info) {
  if (!opt_cloudsql_vector) {
    return;
  }
  index_registry->i_s_memory_info(info);
}
} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0DD_H_ */
