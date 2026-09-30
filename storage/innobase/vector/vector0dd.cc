// Copyright 2026 Google LLC

#include "vector0dd.h"

#include <stdbool.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "vector0index.h"
#include "vector0persist.h"
#include "vector0types.h"
#include "vector0vector.h"

#include "db0err.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "lob0lob.h"
#include "log0recv.h"
#include "my_dbug.h"
#include "kmeans0index.h"
#include "sql/dd/types/column.h"
#include "sql/sql_class.h"
#include "sync0rw.h"
#include "sync0sync.h"
#include "sync0types.h"
#include "rem0rec.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0dbg.h"
#include "ut0log.h"
#include "ut0new.h"
#include "ut0ut.h"
#include "vector0pk.h"

namespace ib_vector {
std::unique_ptr<IndexRegistry> index_registry{nullptr};

void set_vector_col_info(dict_table_t* table, const dd::Column* column) {
  if (column->type() != dd::enum_column_types::VECTOR) {
    return;
  }
  size_t dim = column->char_length() / 4;
  ut_a(valid_dimensions(dim));

  uint32_t col_pos = table->n_def;
  auto vector_col_info =
      std::make_shared<VectorColInfo>(table->name.m_name, col_pos, dim);

  table->vector_col_info = std::move(vector_col_info);
}

dberr_t VectorIndexInfo::load_and_lock_index(dict_table_t* table) {
  size_t retry_count = 0;

begin:
  rw_lock_s_lock(m_latch, UT_LOCATION_HERE);

  if (is_ready()) {
    // return with the latch S-locked, if the index is ready to use
    return DB_SUCCESS;
  } else if (m_index->state() != INDEX_PENDING_LOAD) {
    // some runtime error, e.g, some other loader just found out that this
    // index is corrupt
    rw_lock_s_unlock(m_latch);
    return DB_VEC_INDEX_LOAD_FAILED;
  }

  // at this point, the index state has to be INDEX_PENDING_LOAD
  if (retry_count++ > m_thrashing_retry_limit) {
    ib::warn() << "Failed to load and lock index, retry count exceeded";
    rw_lock_s_unlock(m_latch);
    return DB_VEC_INDEX_LOAD_TIMEDOUT;
  }

  // upgrade to X lock so that we gain the privilege to reload the index
  rw_lock_s_unlock(m_latch);
  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);

  // check again due to the lock gap during the upgrade
  if (is_ready()) {
    // return with the latch X-locked, if the index is ready to use
    // return DB_SUCCESS;
  } else if (m_index->state() != INDEX_PENDING_LOAD) {
    rw_lock_x_unlock(m_latch);
    return DB_VEC_INDEX_LOAD_FAILED;
  }

  // it is still PENDING_LOAD, then le's do it

  // request memory from the index registry
  if (!index_registry->request_memory(*this)) {
    rw_lock_x_unlock(m_latch);
    goto begin;
  }

  // no index registry lock is needed anymore, since
  //     1. the deflation of other indexes are already done
  //     2. this index's memory is already reserved
  //     3. the index lrd list is already updated
  //     4. this index is safe from pre-mature deflation because the index
  //        lock is still held in X-mode

  // reloading index can be a long process but all other users of this
  // index has to wait on the X latch
  auto err = m_index->reload(ib_vector::PersistType::PAGED, table);

  // add a 3 second sleep to simulate a long running reload
  DBUG_EXECUTE_IF("vector_mem_mgmt_test", sleep(3););

  if (err != DB_SUCCESS) {
    ib::error() << "Failed to reload vector index: " << ut_strerr(err);
    rw_lock_x_unlock(m_latch);
    return DB_VEC_INDEX_LOAD_FAILED;
  }

  // upon reaching this point, the index is ready to use

  // downgrading the latch to S-mode leaves another gap so have to start
  // over again
  rw_lock_x_unlock(m_latch);
  goto begin;
}

dict_table_t* VectorIndexInfo::open_sub_table() const {
  if (m_sub_table_id == 0) {
    return nullptr;
  }

  /** No MDL locking for sub table. */
  auto table =
      dd_table_open_on_id(m_sub_table_id, nullptr, nullptr, false, false);
  if (table == nullptr) {
    ib::error() << "Failed: open sub table " << m_sub_table_id;
  }
  return table;
}

void VectorIndexInfo::close_sub_table(dict_table_t* sub_table) const {
  ut_a(m_sub_table_id != 0);
  dd_table_close(sub_table, nullptr, nullptr, false);
}

void VectorIndexInfo::write_to_dd(dd::Properties& p,
                                  const dict_index_t* index) {
  ut_ad(ready_for_write_to_dd());
  if (!ready_for_write_to_dd()) {
    ib::error() << "Failed to write vector index to DD: " << m_sub_table_id;
    return;
  }

  p.set(dd_index_key_strings[DD_VECTOR_INDEX_VERSION], 1);
  p.set(dd_index_key_strings[DD_VECTOR_SUB_TABLE_ID], m_sub_table_id);
  p.set(dd_index_key_strings[DD_VECTOR_TREE_SIZE], m_tree_size);
  dd::String_type config(m_index->config().to_json_string());
  p.set(dd_index_key_strings[DD_VECTOR_INDEX_CONFIG], config);
}

void VectorIndexInfo::read_from_dd(const dd::Properties& p,
                                   const dict_index_t* index) {
  ut_ad(m_sub_table_id == 0);
  ut_ad(m_index == nullptr);
  ut_ad(m_tree_size == 0);

  uint32_t version;
  table_id_t sub_table_id;
  uint64_t tree_size;
  dd::String_type config;

  THD* thd = current_thd;
  ut_ad(thd != nullptr);
  MDL_ticket* mdl_ticket{nullptr};
  dict_table_t* table{nullptr};

  auto table_name = index->table->name.m_name;
  auto index_name = index->name();
  /* Shared pointer to self. */
  auto vec_info = dict_table_get_vector_index_info(index->table);

  DBUG_EXECUTE_IF("simulate_dd_vector_read_error", return;);

  if (p.get(dd_index_key_strings[DD_VECTOR_INDEX_VERSION], &version) ||
      p.get(dd_index_key_strings[DD_VECTOR_SUB_TABLE_ID], &sub_table_id) ||
      p.get(dd_index_key_strings[DD_VECTOR_TREE_SIZE], &tree_size) ||
      p.get(dd_index_key_strings[DD_VECTOR_INDEX_CONFIG], &config)) {
    ib::error() << "Vector index: failed to read from DD: " << index->name();
    return;
  }

  if (version != 1) {
    ib::error() << "Vector index: Unsupported version: " << version;
    return;
  }

  if (sub_table_id == 0) {
    ib::error() << "Vector index: Invalid sub table id: " << sub_table_id;
    return;
  }

  if (tree_size == 0 || tree_size > lob::MAX_SIZE) {
    ib::error() << "Vector index: Invalid tree size: " << tree_size;
    return;
  }

  m_sub_table_id = sub_table_id;
  m_tree_size = tree_size;

  /* Load the table in memory and pin it in InnoDB cache. As we are loading
  it for the first time, we do need to get MDL lock or else global DD will
  complain. Once it is loaded and pinned, we'll never need to go to DD again as
  the table is pinned in InnoDB dictionary cache. Therefore, during normal
  operations we can continue wihtout MDL locking.*/
  table = dd_table_open_on_id(m_sub_table_id, thd, &mdl_ticket, false, false);
  if (table == nullptr) {
    ib::error() << "Vetor index: failed to open sub table " << m_sub_table_id;
    return;
  }

  dict_sys_mutex_enter();
  dict_table_prevent_eviction(table);
  dict_sys_mutex_exit();

  dd_table_close(table, thd, &mdl_ticket, false);

  VectorCfg cfg(config.c_str());
  m_index = std::shared_ptr<VectorIndex>(
      new KMeansIndex(index_name, table_name, std::move(cfg)));
  m_index->set_state(INDEX_PENDING_LOAD);
  m_index->set_pk_fixed_len(get_pk_fixed_len(index->table->first_index()));
  index_registry->register_persistent_index(index->id, vec_info);
}

void VectorIndexInfo::mark_unusable_if_needed(dict_index_t* index) {
  if (m_index == nullptr) {
    /** We haven't even created the m_index object yet. This can happen, for
    example, when we are trying to read from the DD and an error is encountered.
    We maintain the invariant that m_index is always non-null and any vector
    index that we try to read from DD is registered in index_registry. */
    auto table_name = index->table->name.m_name;
    auto index_name = index->name();
    /* Shared pointer to self. */
    auto vec_info = dict_table_get_vector_index_info(index->table);

    VectorCfg dummy_cfg;

    /* It is dummy but still show some respect. */
    const int dim = index->table->vector_col_info->dim_;
    dummy_cfg.set_cfg(Options::VECTOR_DIMENSION, (int)dim);
    m_index = std::shared_ptr<VectorIndex>(
        new KMeansIndex(index_name, table_name, std::move(dummy_cfg)));
    m_index->set_unusable();

    // this is a dummy index, it always has false releasable() return value

    index_registry->register_persistent_index(index->id, vec_info);
    if (!index->is_corrupted()) {
      ib::warn() << "Vector index marked corrupted. " << index->name()
                 << " index id: " << index->id;
      dict_set_corrupted(index);
    }
  } else if (index->is_corrupted()) {
    /* If we have an m_index object, we should have registered it in
    index_registry. We have found some reason to mark the index corrupt after
    successfully reading it from DD. This can happen, for example, if we fail
    loading index tree or CHECK TABLE marks the index corrupt or index was
    marked corrupt in dynamic metadata etc. */
    ut_a(index_registry->persistent_index_registered(index->id));

    if (m_index->state() == INDEX_READY_TO_USE ||
        m_index->state() == INDEX_PENDING_LOAD) {
      bool releasable = m_index->releasable();

      rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
      if (releasable) {
        index_registry->release_memory(*this);
      }
      m_index->set_unusable();
      rw_lock_x_unlock(m_latch);
    }
  } else {
    ut_a(m_index->state() == INDEX_PENDING_LOAD);
  }
}

dict_table_t* VectorIndexInfo::load_index_and_open_sub_table(
    dict_table_t* table) {
  dberr_t err;
  dict_table_t* sub_table;

  auto index = dict_table_get_vector_index(table);
  if (index->is_corrupted()) {
    return nullptr;
  }

  ut_a(index != nullptr);
  if (!is_available()) {
    ut_a(m_index->state() == INDEX_NOT_USABLE);
    goto err_exit;
  }

  err = load_and_lock_index(table);
  if (err != DB_SUCCESS) {
    // if any error, be a newly found corruption or cannot load, etc, the index
    // be out-of-sync with the base data, so might as well mark it NOT_USABLE
    goto err_exit;
  }

  ut_a(is_ready());

  sub_table = open_sub_table();
  if (!sub_table) {
    rw_lock_s_unlock(m_latch);
    goto err_exit;
  }

  // it is the caller's responsibility to (S-)unlock the index latch
  return sub_table;

err_exit:
  if (!index->is_corrupted()) {
    ib::warn() << "Vector index marked corrupted. " << index->name()
               << " index id: " << index->id;
    dict_set_corrupted(index);
  }

  mark_unusable_if_needed(index);

  return nullptr;
}

void VectorIndexInfo::mark_ready() {
  ut_ad(m_index != nullptr);
  ut_ad(m_index->state() == INDEX_PENDING_BUILD ||
        m_index->state() == INDEX_PENDING_LOAD);
  ut_ad(m_sub_table_id != 0);

  // Here is the only place where we break the memory management rule
  // when the partitioner memory has already been allocated through training
  // but we just mark it right now.
  // It is a lesser-evil trade-off. We register this index before it is
  // READY_TO_USE since we want to be able to see if in i_s
  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);

  index_registry->release_memory(m_index->training_memory_size());
  [[maybe_unused]] auto result = index_registry->request_memory(*this);

  // We hold the X-latch of the memory region, and released the training
  // memory first to make room. The index memory request cannot fail
  ut_ad(result);

  m_index->set_state(INDEX_READY_TO_USE);
  rw_lock_x_unlock(m_latch);
}

VectorIndexStats VectorIndexInfo::fill_i_s_info() const {
  VectorIndexStats info;
  if (m_index == nullptr) {
    return info;
  }
  m_index->fill_i_s_info(info);
  info.queries_ = m_queries;
  info.mutations_ = m_mutations;
  info.tree_size_ = m_tree_size;
  return info;
}

IndexRegistry::IndexRegistry(size_t total_mem)
    : m_index_memory(0), m_training_memory(0), m_total_memory(total_mem) {
  // a few bytes do not worth the effort of updating auto_event_names[]
  m_latch = static_cast<rw_lock_t *>(ut::malloc(sizeof(rw_lock_t)));
  ut_a(m_latch);
  rw_lock_create(vector_index_registry_key, m_latch,
                 LATCH_ID_VECTOR_INDEX_REGISTRY);
  if (!srv_innodb_cloudsql_vector_mem_regulation) {
    m_disabled = true;
  }

  ib::info() << "Vector index registry created with memory limit: "
             << m_total_memory;
}

IndexRegistry::~IndexRegistry() {
  if (!m_persistent_index_map.empty()) {
    ib::error() << "Persistent vector index map is not empty: size: "
                << m_persistent_index_map.size();
  }
  m_persistent_index_map.clear();

  rw_lock_free(m_latch);
  ut::free(m_latch);
}

void IndexRegistry::i_s_info(std::vector<VectorIndexStats>& vector_info) {
  vector_info.clear();
  rw_lock_s_lock(m_latch, UT_LOCATION_HERE);
  for (const auto& [index_id, index_info] : m_persistent_index_map) {
    vector_info.push_back(index_info->fill_i_s_info());
  }
  rw_lock_s_unlock(m_latch);
}

void IndexRegistry::i_s_memory_info(VectorIndexMemoryInfo& info) {
  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
  info.disabled_ = m_disabled;
  info.total_memory_ = m_total_memory;
  info.index_memory_ = m_index_memory.load();
  info.training_memory_ = m_training_memory.load();
  for (const auto& [index_id, index_info] : m_persistent_index_map) {
    if (index_info->is_ready()) {
      info.num_loaded_indexes_++;
    }
  }
  rw_lock_x_unlock(m_latch);
}

bool IndexRegistry::request_memory(const VectorIndexInfo& requester) {
  if (m_disabled) {
    return true;
  }

  // this call is entered when the requester is X-latched by the caller
  ut_ad(rw_lock_own(requester.m_latch, RW_LOCK_X));

  ut_ad(requester.is_available() ||
        // this exception is only for the newly built index, when the it
        // has already loaded the partitioner before requesting memory
        requester.index()->state() == INDEX_PENDING_BUILD);

  bool reserved = false;

  // this latch is held very shortly, worst case waiting for all indexes to
  // finish their ongoing queries
  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);

  // index can use as much memory as needed, as long as the overall quota
  // is not reached, and no ongoing trainings affected
  auto requested = requester.tree_size();
  DBUG_EXECUTE_IF("vector_mem_mgmt_test", requested = m_fake_index_size;);

  auto avail_memory =
      m_total_memory - m_training_memory.load() - m_index_memory.load();
  if (requested <= avail_memory) {
    m_index_memory.fetch_add(requested);
    reserved = true;
  } else {
    bool squeezable =
        (m_total_memory - m_training_memory.load()) >= requested;
    if (squeezable) {
      auto to_squeeze = m_index_memory.load()
                      + m_training_memory.load()
                      + requested
                      - m_total_memory;
      if (free_up_memory(to_squeeze)) {
        m_index_memory.fetch_add(requested);
        reserved = true;
      }
    }
  }

  if (!reserved) {
    ib::warn() << "Failed to allocate memory for index: " << requested << ", "
               << *this;
  }

  rw_lock_x_unlock(m_latch);
  return reserved;
}

bool IndexRegistry::request_memory(size_t size) {
  if (m_disabled) {
    return true;
  }

  bool allocated = false;

  DBUG_EXECUTE_IF("vector_mem_mgmt_test", size = m_fake_training_size;);

  // similar to requesting index memory, the worst case is to wait for all
  // indexes to finish their ongoing queries
  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);

  // training memory is capped at 80% (m_training_memory_ratio) of the
  // overall size, regardless how many trainings are active together
  auto total_training_memory = m_total_memory * m_training_memory_ratio;
  auto avail_training_memory = total_training_memory - m_training_memory.load();

  if ((size <= avail_training_memory) &&
      (size + m_index_memory.load() + m_training_memory.load()
           <= m_total_memory)) {
    m_training_memory.fetch_add(size);
    allocated = true;
  } else {
    bool squeezable =
        // remaining training memory plus index memory is enough to cover
        ((m_index_memory.load() + avail_training_memory) >= size) &&
        // all training memory is below the cap
        ((m_training_memory.load() + size) <= total_training_memory);
    if (squeezable) {
      size_t to_squeeze = m_index_memory.load()
                          + m_training_memory.load()
                          + size
                          - m_total_memory;
      if (free_up_memory(to_squeeze)) {
        m_training_memory.fetch_add(size);
        allocated = true;
      }
    }
  }

  if (!allocated) {
    ib::warn() << "Failed to allocate memory for training: " << size << ", "
               << *this;
  }

  rw_lock_x_unlock(m_latch);
  return allocated;
}

void IndexRegistry::release_memory(const VectorIndexInfo& donor) {
  if (m_disabled) {
    return;
  }

  // make sure the caller had acquired X-latch on its own index
  ut_ad(rw_lock_own(donor.m_latch, RW_LOCK_X));

  auto size = donor.tree_size();
  DBUG_EXECUTE_IF("vector_mem_mgmt_test", size = m_fake_index_size;);

  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
  ut_a(m_index_memory.load() >= size);
  m_index_memory.fetch_sub(size);
  rw_lock_x_unlock(m_latch);
}

void IndexRegistry::release_memory(size_t size) {
  if (m_disabled) {
    return;
  }

  DBUG_EXECUTE_IF("vector_mem_mgmt_test", size = m_fake_training_size;);

  // add a 10 second sleep to simulate long running build
  DBUG_EXECUTE_IF("vector_mem_mgmt_test", sleep(10););

  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
  ut_a(m_training_memory.load() >= size);
  m_training_memory.fetch_sub(size);
  rw_lock_x_unlock(m_latch);
}

bool IndexRegistry::free_up_memory(size_t requested) {
  if (m_disabled) {
    return true;
  }

  // make sure the caller had acquired X-latch on the memory region
  ut_ad(rw_lock_own(m_latch, RW_LOCK_X));

  size_t released = 0;
  for (auto itr = m_index_lrd.begin();
       released < requested && itr != m_index_lrd.end(); ) {
    auto index_id = *itr;
    auto to_deflate = m_persistent_index_map[index_id];
    // only deflate those in READY_TO_USE state
    if (to_deflate->is_ready()) {
      auto size = to_deflate->tree_size();
      ut_a(size > 0);
      DBUG_EXECUTE_IF("vector_mem_mgmt_test", size = m_fake_index_size;);
      released += size;
      m_index_memory.fetch_sub(size);
      itr = m_index_lrd.erase(itr);
      m_index_lrd.push_back(index_id);

      rw_lock_x_lock(to_deflate->m_latch, UT_LOCATION_HERE);
      // this set_state() will deflate the to_deflate
      to_deflate->index()->set_state(INDEX_PENDING_LOAD);
      rw_lock_x_unlock(to_deflate->m_latch);
    } else {
      ++itr;
    }
  }

  // worst case it releases bunch of indexes but still ends w/ insufficient
  // amount, too bad
  return released >= requested;
}

void IndexRegistry::update_max_mem_size() {
  if (m_disabled) {
    return;
  }

  rw_lock_x_lock(m_latch, UT_LOCATION_HERE);
  auto new_limit = opt_cloudsql_vector_max_mem_size;
  if (m_total_memory <= new_limit) {
    /* We are increasing the memory limit. */
    m_total_memory = new_limit;
    rw_lock_x_unlock(m_latch);
    return;
  }

  auto in_use = m_training_memory.load() + m_index_memory.load();
  if (in_use <= new_limit) {
    /* We are decreasing the memory limit and we have enough memory to do so. */
    m_total_memory = new_limit;
    rw_lock_x_unlock(m_latch);
    return;
  }

  /* We need to deflate some indexes to make room for the new limit. */
  auto needed = in_use - new_limit;
  if (free_up_memory(needed)) {
    m_total_memory = new_limit;
    rw_lock_x_unlock(m_latch);
    return;
  }

  /* We failed to deflate enough indexes to make room for the new limit. Reset
  the limit to the old value. */
  opt_cloudsql_vector_max_mem_size = m_total_memory;
  rw_lock_x_unlock(m_latch);

  auto thd = current_thd;
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                      "Unable to update cloudsql_vector_max_mem_size to %lu",
                      new_limit);
  push_warning_printf(thd, Sql_condition::SL_WARNING, ER_WRONG_ARGUMENTS,
                      "cloudsql_vector_max_mem_size set to %lu",
                      m_total_memory);
}

void update_vector_max_mem_size() {
  if (!opt_cloudsql_vector) {
    return;
  }
  ut_a(index_registry != nullptr);
  index_registry->update_max_mem_size();
}

void index_vector_cleanup(dict_index_t* index) {
  if (!dict_index_is_vector(index)) {
    return;
  }

  auto vec_info = dict_table_get_vector_index_info(index->table);
  index->table->vector_col_info->persistent_vec_index_ = nullptr;

  /** We should always have a vector index info for a vector index. */
  if (vec_info == nullptr) {
    ib::error() << "Vector index info is null for index " << index->name();
    return;
  }

  /** We may or may not have a vector index object. For example, if DDL failed
  after we created the dict_index_t but before we created the VectorIndex
  object. */
  auto vec_index = vec_info->index();
  if (vec_index) {
    /* Any vector index object that we create should be always registered */
    if (index_registry->persistent_index_registered(index->id)) {
      index_registry->unregister_persistent_index(index->id);
    } else {
      ib::error() << "Vector index " << index->name()
                  << " is not registered in index registry.";
    }
    vec_index.reset();
  }
}

bool process_dd_indexes_rec(mem_heap_t* heap, const rec_t* rec,
                            const dict_index_t** index, MDL_ticket** mdl,
                            dict_table_t* dd_indexes, mtr_t* mtr) {
  ulint len;
  const byte* field;
  uint32_t index_id;
  uint32_t space_id;
  uint64_t table_id;

  *index = nullptr;

  ut_ad(!rec_get_deleted_flag(rec, dict_table_is_comp(dd_indexes)));

  ulint* offsets = rec_get_offsets(rec, dd_indexes->first_index(), nullptr,
                                   ULINT_UNDEFINED, UT_LOCATION_HERE, &heap);

  const dd::Object_table& dd_object_table = dd::get_dd_table<dd::Index>();

  field =
      rec_get_nth_field(nullptr, rec, offsets,
                        dd_object_table.field_number("FIELD_ENGINE") + 2, &len);

  /* If "engine" field is not "innodb", return. */
  if (strncmp((const char*)field, "InnoDB", 6) != 0) {
    mtr_commit(mtr);
    return false;
  }

  /* Get the se_private_data field. */
  field = (const byte*)rec_get_nth_field(
      nullptr, rec, offsets,
      dd_object_table.field_number("FIELD_SE_PRIVATE_DATA") + 2, &len);

  if (len == 0 || len == UNIV_SQL_NULL) {
    mtr_commit(mtr);
    return false;
  }

  /* Only process vector indexes. */
  dd::String_type prop((char*)field);
  dd::Properties* p = dd::Properties::parse_properties(prop);
  if (!p || !p->exists(dd_index_key_strings[DD_VECTOR_INDEX_VERSION])) {
    if (p) {
      delete p;
    }
    mtr_commit(mtr);
    return false;
  }

  if (p->get(dd_index_key_strings[DD_INDEX_ID], &index_id)) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Get the tablespace id. */
  if (p->get(dd_index_key_strings[DD_INDEX_SPACE_ID], &space_id)) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Load the table and get the index. */
  if (!p->exists(dd_index_key_strings[DD_TABLE_ID])) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  if (p->get(dd_index_key_strings[DD_TABLE_ID], &table_id)) {
    delete p;
    mtr_commit(mtr);
    return false;
  }

  /* Load the table and get the index. */
  THD* thd = current_thd;
  dict_table_t* table;

  mtr_commit(mtr);
  table = dd_table_open_on_id(table_id, thd, mdl, true, true);
  if (!table) {
    delete p;
    return false;
  }

  for (const dict_index_t* t_index = table->first_index(); t_index != nullptr;
       t_index = t_index->next()) {
    if (t_index->space == space_id && t_index->id == index_id) {
      *index = t_index;
    }
  }

  if (*index == nullptr) {
    dd_table_close(table, thd, mdl, true);
    delete p;
    return false;
  }

  delete p;
  ut_ad(dict_index_is_vector(*index));
  return true;
}

dberr_t init_vector_index_registry(size_t mem_limit) {
  ut_a(index_registry == nullptr);

  if (!opt_cloudsql_vector) {
    ib::info() << "Disabling vector index feature. 'cloudsql_vector' is off.";
    return DB_SUCCESS;
  }

  DBUG_EXECUTE_IF("vector_mem_mgmt_test",
                  mem_limit = IndexRegistry::m_fake_total_memory;);

  index_registry = std::make_unique<IndexRegistry>(mem_limit);
  ut_a(index_registry);
  return DB_SUCCESS;
}

void disable_vector_index_memory_regulator() {
  if (!opt_cloudsql_vector) {
    return;
  }
  ut_a(index_registry != nullptr);
  index_registry->disable_memory_regulator();
}

void shutdown_index_registry() {
  if (!opt_cloudsql_vector || opt_initialize) {
    ut_ad(index_registry == nullptr);
    return;
  }

  index_registry.reset();
  ut_a(index_registry == nullptr);
  ib::info() << "Vector index registry shutdown complete.";
}

} /* namespace ib_vector */
