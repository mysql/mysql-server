// Copyright 2026 Google LLC

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <format>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>

#include <vector0types.h>
#include "vector0build.h"
#include "vector0dd.h"
#include "vector0pk.h"
#include "vector0vector.h"
#include "kmeans0types.h"
#include "kmeans0index.h"
#include "kmeans0table.h"

#include "btr0btr.h"
#include "btr0pcur.h"
#include "data0data.h"
#include "data0type.h"
#include "db0err.h"
#include "ddl0ddl.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "dict0types.h"
#include "handler/ha_innodb.h"
#include "lock0lock.h"
#include "mach0data.h"
#include "mem0mem.h"
#include "mtr0mtr.h"
#include "page0cur.h"
#include "page0types.h"
#include "pars0pars.h"
#include "que0que.h"
#include "rem0cmp.h"
#include "rem0rec.h"
#include "rem0types.h"
#include "rem0wrec.h"
#include "row0ins.h"
#include "row0mysql.h"
#include "row0row.h"
#include "row0upd.h"
#include "srv0srv.h"
#include "trx0trx.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0dbg.h"
#include "ut0log.h"
#include "ut0lst.h"
#include "ut0stage.h"

namespace ib_vector {

// TODO: review the ut_a()'s in the vector implementation, to make sure
//       we don't crash unless absolutely ncessary. The preferred way
//       is to mark the index corrupt so user can rebuild.

KMeansTable::KMeansTable(KMeansIndex* index, que_thr_t* thr,
                     CreateVectorIndexInfo* create_info)
    : m_index(index), m_que_thr(thr), m_create_info(create_info) {
  ut_a(m_index);
  ut_a(m_create_info != nullptr || m_que_thr != nullptr);
  // enough space for a 1536 vector w/ a fairly straightforward PK
  m_heap = mem_heap_create(2048, UT_LOCATION_HERE);
}

KMeansTable::KMeansTable(KMeansIndex* index, dict_table_t* base_table)
    : m_index(index), m_base_table(base_table) {
  // no point guessing how big the partitioner data is
  m_heap = mem_heap_create(1, UT_LOCATION_HERE);
}

KMeansTable::~KMeansTable() {
  if (m_ins_node && m_ins_node->entry_sys_heap) {
    mem_heap_free(m_ins_node->entry_sys_heap);
  }

  if (m_upd_node != nullptr) {
    if (m_upd_node->pcur != nullptr) {
      m_upd_node->pcur->close();
     btr_pcur_t::free_for_mysql(m_upd_node->pcur);
    }

    if (m_upd_node->update != nullptr) {
      m_upd_node->update->free_per_stmt_heap();
    }

    if (m_upd_node->heap != nullptr) {
      mem_heap_free(m_upd_node->heap);
    }

    ut_ad(m_upd_node->cascade_heap == nullptr);
  }

  if (m_heap != nullptr) {
    mem_heap_free(m_heap);
  }
}

dberr_t KMeansTable::persist() {
  dberr_t err = DB_SUCCESS;
  if (m_que_thr) {
    // this is when DML happens on the base table, the persist() call there
    // only has que_thr_t* info. TABLE* input is set to NULL
    MONITOR_INC(MONITOR_VECTOR_INDEXES_NUM_MUTATIONS);
    switch (que_node_get_type(m_que_thr->run_node)) {
      case QUE_NODE_INSERT: {
        err = insert_row();
        break;
      }
      case QUE_NODE_UPDATE: {
        // value in m_old_pk signifies the fact that sub_table update is
        // needed therefore the necessary data had been populated through
        // an earlier get_upd_data() call
        if (m_old_pk) {
          err = update_row();
        }
        break;
      }
      default:
        ut_a(0);
    }
  } else {
    // this is when index build happens.
    err = backfill();
  }

  if (err != DB_SUCCESS && m_que_thr) {
    MONITOR_INC(MONITOR_VECTOR_INDEXES_NUM_MUTATIONS_FAILED);
  }
  return err;
}

std::pair<dberr_t, void*> KMeansTable::load() {
  ut_a(!m_que_thr && !m_create_info && m_base_table);

  auto info = dict_table_get_vector_index_info(m_base_table);
  ut_a(info);

  MONITOR_INC(MONITOR_VECTOR_INDEXES_NUM_TREE_LOAD);

  // load the partitioner
  auto partitioner_len = info->tree_size();
  auto multipliers_len = m_index->dims() * sizeof(VectorDataT);
  auto sub_table = info->open_sub_table();
  ut_a(sub_table);
  byte* data = nullptr;
  auto err = get_partitioner(sub_table, &data, partitioner_len);
  if (err != DB_SUCCESS) {
    goto func_exit;
  }

  m_index->set_partitioner((const char*)data, partitioner_len);

  // load the multipliers
  err = get_multipliers(sub_table, &data, multipliers_len);
  if (err != DB_SUCCESS) {
    goto func_exit;
  }

  {
    multipliers_len /= sizeof(VectorDataT);
    auto mult = absl::MakeConstSpan((const VectorDataT*)data, multipliers_len);
    m_index->set_multipliers(mult);
  }

func_exit:
  // close the sub_table
  info->close_sub_table(sub_table);
  if (err != DB_SUCCESS) {
    MONITOR_INC(MONITOR_VECTOR_INDEXES_NUM_TREE_LOAD_FAILED);
  } else {
    m_index->set_state(INDEX_READY_TO_USE);
  }
  return std::make_pair(err, err == DB_SUCCESS ? m_index : nullptr);
}

dberr_t KMeansTable::sync_mutation(que_thr_t* thr) {
  ut_a(m_que_thr != nullptr && m_create_info == nullptr);

  switch (que_node_get_type(m_que_thr->run_node)) {
    case QUE_NODE_INSERT: {
      // TODO: we can consider a get_ins_data() here and use m_new_pk
      //       and m_new_embedding to store the data. There will be an
      //       unnecessary op (an almost empty dtuple shell) involved,
      //       but,
      //       symmetry is beauty!
      break;
    }
    case QUE_NODE_UPDATE: {
      auto base_upd_node = static_cast<const upd_node_t*>(m_que_thr->run_node);
      if (upd_needed(base_upd_node)) {
        get_upd_data(base_upd_node);
      }
      break;
    }
    default:
      ut_a(0);
  }

  // TODO: consider translating all ut_a(..)'s inside this call to DB_ERROR
  return DB_SUCCESS;
}

dberr_t KMeansTable::insert_row() {
  dberr_t err = DB_SUCCESS;

  auto base_ins_node = static_cast<const ins_node_t*>(m_que_thr->run_node);
  auto base_table = base_ins_node->table;
  ut_a(que_node_get_type(base_ins_node) == QUE_NODE_INSERT);

  auto vec_info = dict_table_get_vector_index_info(base_table);
  auto st_handle = vec_info->load_index_and_open_sub_table(base_table);
  if (st_handle == nullptr) {
    return DB_VEC_INDEX_LOAD_FAILED;
  }

  err = build_ins_node(base_ins_node, st_handle);
  ut_a(err == DB_SUCCESS && m_ins_node);

  vec_info->release_index_lock();

  err = run_node(m_ins_node);

  if (err == DB_SUCCESS) {
    // update stats for sub_table
    vec_info->inc_mutations();
    srv_stats.n_rows_inserted.inc();
    dict_table_n_rows_inc(st_handle);
    row_update_statistics_if_needed(st_handle);
  }

  vec_info->close_sub_table(st_handle);
  return err;
}

dberr_t KMeansTable::update_row() {
  dberr_t err = DB_SUCCESS;

  auto base_upd_node = static_cast<const upd_node_t*>(m_que_thr->run_node);
  ut_a(que_node_get_type(base_upd_node) == QUE_NODE_UPDATE);
  ut_a(base_upd_node->state == UPD_NODE_UPDATE_CLUSTERED ||
       base_upd_node->state == UPD_NODE_UPDATE_ALL_SEC);
  ut_a(base_upd_node->has_clust_rec_x_lock);

  auto base_table = base_upd_node->table;
  auto vec_info = dict_table_get_vector_index_info(base_table);
  auto st_handle = vec_info->load_index_and_open_sub_table(base_table);
  if (st_handle == nullptr) {
    return DB_VEC_INDEX_LOAD_FAILED;
  }

  err = build_upd_node(base_upd_node, st_handle);

  vec_info->release_index_lock();

  if (m_upd_node) {
    err = run_node(m_upd_node);
    if (err == DB_SUCCESS) {
      // update stats for sub_table
      vec_info->inc_mutations();
      if (m_upd_node->is_delete) {
        srv_stats.n_rows_deleted.inc();
      } else {
        srv_stats.n_rows_updated.inc();
      }
    }
  }

  vec_info->close_sub_table(st_handle);
  return err;
}

dberr_t KMeansTable::backfill() {
  ut_a(m_que_thr == nullptr && m_create_info != nullptr);
  ut_a(m_create_info->validate());

  /** We are going to do a bit of trickery here. When we land here, we are
  trying to fill the sub_table. While we'll use the ddl::Context to do the
  parallel build of the sub_table, the ddl::Context assumes things to be laid
  out a little differently. We are coming here in the path of CREATE VECTOR
  INDEX. The operation we are trying to do is to create a vector index as a
  secondary index on the base table. However, physically what we are going to do
  will be roughly:

  * Read each clustered index row from base table
    -- Create sub_table clustered index entry
    -- Create sub_table secondary index entry
  * Sort these entries on key order
  * Spill them to disk when we hit the memory limit
  * Merge all sorted entries to build the btree for sub_table clustered index
  * Merge all sorted entries to build the btree for sub_table secondary index
  * Flush all dirty pages of the sub_table

  Though the code thinks we are building secondary index on the base_table, here
  we'll instead pass it the handle of sub_table and the two indexes of the
  sub_table which we want built. As such, ddl::Context will not even be aware of
  the presence of a vector index.

  Whatever sorcery we do here must be handled by us and no trace of this should
  go back to the upper layers e.g.: if ddl::Context is expected to update the
  index it is building in some way then we should update the vector_index
  accordingly after successful return from ddl::Context::build().  */
  dberr_t err = DB_SUCCESS;

  auto vec_info = dict_table_get_vector_index_info(m_create_info->m_table);
  ut_a(vec_info != nullptr);
  auto st_handle = vec_info->open_sub_table();
  /** We have just created the sub_table and pinned it in memory. This open
  call should never fail.  */
  ut_a(st_handle != nullptr);

  /* We have two indexes to build in sub_table. */
  dict_index_t* indexes_to_build[2];
  indexes_to_build[0] = st_handle->first_index();
  indexes_to_build[1] = st_handle->first_index()->next();

  auto seq = ddl::Sequence(nullptr, 0, 0);

  /* For sub_table we have two keys. The PK and the unique index on base_pk. We
  add the key number of vector index in the base_table as third entry here.
  This is used internally by the ddl threads to report an error. We map any
  errors back to the vector index instead of one of the specific sub_table
  indexes. */
  ulint key_numbers[3] = {0, 1, m_create_info->m_key_num};

  auto trx = m_create_info->m_trx;

  // find a reasonable memory size to parallel building
  ulong memory_limit = std::max(m_create_info->m_mem_size,
                                thd_ddl_buffer_size(trx->mysql_thd));
  memory_limit = std::min(memory_limit,
                          (ulong) std::numeric_limits<uint32_t>::max());

  const auto num_threads = thd_ddl_threads(trx->mysql_thd);
  ib::info() << "building the index with: num_threads: " << num_threads
             << " memory_limit: " << memory_limit;

  ddl::Context sub_table_build(
      trx,                     // transaction which must have an id assigned
      m_create_info->m_table,  // base table with vector column
      st_handle,               // sub table handle
      false,                   // not an online operation
      indexes_to_build,        // indexes to build on sub_table
      key_numbers,             // dummy values for now
      2,                       // number of indexes to build
      m_create_info->m_mysql_table,  // base table mysql table
      nullptr,                       // no added columns
      nullptr,                       // no column map
      ULINT_UNDEFINED,               // no autoinc involved
      seq,                           // no sequence
      false,                         // don't skip PK merge sort
      m_create_info->m_stage,        // alter stage
      nullptr,                       // no virtual columns added
      nullptr,                       // no TABLE* needed for virtual columns
      memory_limit,                  // memory limit
      num_threads,                   // number of parallel threads
      true                           // it is a vector index build
  );

  err = sub_table_build.build();
  if (err != DB_SUCCESS) {
    ib::error() << "Failed building sub_table, " << ut_strerr(err);
    /** In case of any error from ddl::Context we pass this back to the calling
    code in handler0alter.cc. We flag it as a build error. */
    if (err == DB_OUT_OF_MEMORY) {
      /** In case of OOM, we'd rather have our own error message. */
      err = DB_VEC_INDEX_OUT_OF_MEMORY;
    } else {
      m_create_info->m_build_error = true;
    }
  } else {
    /** Persist the non-leaf partitioner to the sub_table. */
    err = persist_partitioner(st_handle);
    DBUG_EXECUTE_IF("vector_index_persist_err", err = DB_IO_ERROR;);
    DBUG_EXECUTE_IF("vector_index_persist_crash", DBUG_SUICIDE(););
    if (err != DB_SUCCESS) {
      ib::error() << "Failed to persist partitioner to sub_table: error: "
                  << ut_strerr(err);
      err = DB_VEC_INDEX_PERSIST_FAILED;
    }

    // restore the mutate partitioner to its default (database instead of tlp)
    ib::info() << "restoring the mutate partitioner to its default";
    m_index->restore_mutate_partitioner();
  }
  vec_info->close_sub_table(st_handle);
  return err;
}

bool KMeansTable::upd_needed(const upd_node_t* base_upd_node) {
  auto upd = base_upd_node->update;

  // sub_table update is required if PK or embedding is updated on base table
  auto base_index = base_upd_node->table->first_index();
  if (base_upd_node->is_delete) {
    return true;
  }

  auto vector_idx_pos = get_vector_col_pos(base_index);
  for (ulint i = 0; i < upd->n_fields; i++) {
    auto field = upd->fields + i;
    if (field->field_no == vector_idx_pos) {
      return true;
    }

    // for update vector field_no is position in clustered index, it is
    // possible that the updated field is only part of PK prefix
    for (int j = 0; j < dict_index_get_n_unique_in_tree(base_index); j++) {
      auto col = base_index->get_col(j);
      if (col->get_col_phy_pos() == field->field_no) {
        return true;
      }
    }
  }

  return false;
}

dberr_t KMeansTable::build_ins_node(const ins_node_t* base_ins_node,
                                  dict_table_t* sub_table) {
  const dict_table_t* base_table = base_ins_node->table;
  const dict_index_t* base_index = base_table->first_index();
  const dtuple_t* base_row = base_ins_node->row;

  // create the sub table insert node
  m_ins_node = ins_node_create(INS_DIRECT, sub_table, m_heap);
  que_node_set_parent(m_ins_node, const_cast<ins_node_t*>(base_ins_node));

  // retrieve PK from the base table row
  auto index_entry = UT_LIST_GET_FIRST(base_ins_node->entry_list);
  auto [pk_val, pk_len] = write_pk_from_tuple(index_entry, base_index, m_heap);
  ut_a(pk_len != 0);

  // get the value of the vector column
  const uint32_t vec_col_pos = get_vector_col_info(base_index)->col_pos_;
  const dfield_t* vec_field = dtuple_get_nth_field(base_row, vec_col_pos);
  ut_a(!vec_field->ext);
  auto [emb_data, emb_len] =
      get_embedding_from_field(base_index, vec_field, m_heap);

  // create the sub_table insert node
  dtuple_t* tuple = dtuple_create(m_heap, sub_table->get_n_cols());
  dict_table_copy_types(tuple, sub_table);
  ut_ad(dtuple_check_typed(tuple));

  ins_node_set_new_row(m_ins_node, tuple);

  // populate the sub_table insert node
  ut_ad(emb_len == UNIV_SQL_NULL || emb_len / 4 == (size_t)m_index->dims());

  for (int i = 0; i < Cols::NUM_COLS; i++) {
    dfield_t* field = dtuple_get_nth_field(tuple, i);
    switch (i) {
      case Cols::PARTITION_ID: {
        Partitions part;
        dfield_set_data(field, get_partition_id(emb_data, emb_len),
                        partition_id_len);
        break;
      }
      case Cols::BASE_PK: {
        dfield_set_data(field, pk_val, pk_len);
        break;
      }
      case Cols::CONTENT: {
        auto [q_vec, q_len] = get_quantized_vector(emb_data, emb_len);
        dfield_set_data(field, q_vec, q_len);
        break;
      }
    }
  }

  return DB_SUCCESS;
}

dberr_t KMeansTable::build_upd_node(const upd_node_t* base_upd_node,
                                  dict_table_t* sub_table) {
  // find the secondary index which is based on base table PK
  dict_index_t* sec_index = get_secondary_index(sub_table);
  ut_a(sec_index != nullptr);

  // get IX lock on the sub table. We know we are going to update a row
  dberr_t err = lock_table(0, sub_table, LOCK_IX, m_que_thr);

  // This should never fail. IX conflicts with X but we only attempt to get X
  // lock in case we are doing some DDL operation.
  ut_a(err == DB_SUCCESS);

  mtr_t mtr;
  mtr_start(&mtr);

  // get the row (to be updated) from sub_table
  auto base_table = base_upd_node->table;
  auto row_ref = get_row(sec_index, base_table, &mtr);

  // create the update node for the sub_table
  ut_a(m_upd_node == nullptr);
  m_upd_node = row_create_update_node_for_mysql(sub_table, m_heap);
  que_node_set_parent(m_upd_node, const_cast<upd_node_t*>(base_upd_node));
  m_upd_node->is_delete = base_upd_node->is_delete;

  dict_index_t* clust_index = sub_table->first_index();
  auto clust_pcur = m_upd_node->pcur;
  clust_pcur->open_no_init(clust_index, row_ref, PAGE_CUR_LE, BTR_SEARCH_LEAF,
                           0, &mtr, UT_LOCATION_HERE);
  auto clust_rec = clust_pcur->get_rec();
  auto clust_block = clust_pcur->get_block();

  if (!validate_rec(clust_rec, row_ref, clust_index)) {
    err = DB_RECORD_NOT_FOUND;
    ut_d(ut_error);
  } else {
    // get the lock on the clustered record, no GAP lock needed
    err = lock_clust_rec_read_check_and_lock_alt(
        clust_block, clust_rec, clust_index,
        LOCK_X, LOCK_REC_NOT_GAP, m_que_thr);

    // There is a 1:1 mapping between base_table and sub_table. We only come
    // here if we are updating a row in the base table, which implies that we
    // are holding a lock on the base table. Hence we should also be able to
    // obtain the lock on sub_table w/o any wait
    ut_a(err == DB_SUCCESS);

    if (!m_upd_node->is_delete) {
      auto base_index = base_table->first_index();
      update_upd_node_fields(clust_rec, base_index);
    }

    clust_pcur->store_position(&mtr);
    ut_a(clust_pcur->m_rel_pos == BTR_PCUR_ON);
  }

  if (err != DB_SUCCESS) {
    if (clust_pcur) {
      clust_pcur->close();
      btr_pcur_t::free_for_mysql(clust_pcur);
    }
  }

  mtr_commit(&mtr);

  return err;
}

bool KMeansTable::validate_rec(const rec_t* rec,
                             const dtuple_t* match_entry,
                             const dict_index_t* clust_index) {
  std::string err_text;
  bool valid = true;

  // rec has to a user record
  if (!page_rec_is_user_rec(rec)) {
    err_text = "record is not user record";
    valid = false;
  }

  if (valid) {
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    ulint* offsets = offsets_;
    rec_offs_init(offsets_);
    offsets = rec_get_offsets(rec, clust_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    // record has to exactly match the match entry
    if (cmp_dtuple_rec(match_entry, rec, clust_index, offsets)) {
      err_text = "record does not match the entry";
      valid = false;
    }

    // record cannot be a delete marked
    // TODO: this check seemed to be redundant, see get_row()
    if (valid && rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
      err_text = "record is delete marked";
      valid = false;
    }
  }

  if (!valid) {
    ib::error() << "KMeansTable sub_table record validation failed. "
                << "table: " << clust_index->table->name.m_name << ", "
                << "index: " << clust_index->name << ", "
                << "error: " << err_text;
    rec_print(stderr, rec, clust_index);
  }

  return valid;
}

void KMeansTable::update_upd_node_fields(const rec_t* clust_rec,
                                       const dict_index_t* base_index) {
  ut_a(m_upd_node);

  auto update = m_upd_node->update;
  auto index = update->table->first_index();

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint* offsets = offsets_;
  rec_offs_init(offsets_);
  offsets = rec_get_offsets(clust_rec, index, offsets, ULINT_UNDEFINED,
                            UT_LOCATION_HERE, &m_heap);

  ulint n_diff = 0;

  auto upd = [&](const Cols upd_col, const byte* upd_data, ulint upd_len) {
    auto col = update->table->get_col(upd_col);
    auto field = upd_get_nth_field(update, n_diff);
    if (upd_len != UNIV_SQL_NULL) {
      dfield_set_data(&field->new_val, upd_data, upd_len);
    } else {
      dfield_set_null(&field->new_val);
    }
    auto col_pos_in_index = dict_col_get_index_pos(col, index);
    upd_field_set_field_no(field, col_pos_in_index, index);
    n_diff++;
  };

  // update the partition id
  if (m_new_embedding) {
    // it is always possible that the partition changed
    auto part_id =
        get_partition_id((const byte*)(m_new_embedding->data),
                         m_new_embedding->len);
    upd(Cols::PARTITION_ID, part_id, partition_id_len);
  }

  // update PK
  if (m_new_pk != nullptr) {
    auto [pk_val, pk_len] =
        write_pk_from_tuple(m_new_pk, base_index, m_heap);
    upd(Cols::BASE_PK, pk_val, pk_len);
  }

  // update the content, i.e., (quantized) vector data
  if (m_new_embedding) {
    auto [q_vec, q_vec_len] = get_quantized_vector(
        reinterpret_cast<const byte*>(m_new_embedding->data),
        m_new_embedding->len);
    upd(Cols::CONTENT, q_vec, q_vec_len);
  }

  update->n_fields = n_diff;
  ut_d(update->validate());
}

dberr_t KMeansTable::run_node(que_node_t* node) {
  ut_a(node == static_cast<que_node_t*>(m_ins_node) ||
       node == static_cast<que_node_t*>(m_upd_node));
  ut_a(node);

  dberr_t err = DB_SUCCESS;
  auto trx = thr_get_trx(m_que_thr);
  ut_a(trx->error_state == DB_SUCCESS);

  for (;;) {
    m_que_thr->run_node = node;
    m_que_thr->prev_node = node;

    if (node == static_cast<que_node_t*>(m_upd_node)) {
      TABLE* temp = m_que_thr->prebuilt->m_mysql_table;
      m_que_thr->prebuilt->m_mysql_table = nullptr;

      row_upd_step(m_que_thr);

      m_que_thr->prebuilt->m_mysql_table = temp;
    } else {
      row_ins_step(m_que_thr);
    };

    err = trx->error_state;
    if (err == DB_LOCK_WAIT) {
      // handle lock wait & possible retry
      que_thr_stop_for_mysql(m_que_thr);

      lock_wait_suspend_thread(m_que_thr);

      // if lucky enough, i.e., not ended in a lock wait timeout, or got
      // picked up as a victim of the selective deadlock resolution
      if (trx->error_state != DB_SUCCESS) {
        err = trx->error_state;
        break;
      } else {
        continue;
      }
    } else {
      break;
    }
  }

  // sub_table insert only happens after a successful base table insert, so
  // there should never be a duplicate key error.
  ut_a(err != DB_DUPLICATE_KEY);

  return err;
}

dict_index_t* KMeansTable::get_secondary_index(dict_table_t* sub_table) {
  dict_index_t* sec_index = nullptr;
  auto base_pk_col = sub_table->get_col(Cols::BASE_PK);
  for (auto idx = sub_table->first_index(); idx != nullptr; idx = idx->next()) {
    if (idx->n_user_defined_cols == 1 &&
        dict_col_get_index_pos(base_pk_col, idx) == 0) {
      ut_a(dict_index_is_unique(idx));
      ut_a(idx->n_uniq == 1);
      sec_index = idx;
      break;
    }
  }
  return sec_index;
}

dtuple_t* KMeansTable::get_row(dict_index_t* sec_index,
                             dict_table_t* base_table,
                             mtr_t* mtr) {
  // build an index entry to search the secondary index
  dtuple_t* base_pk_entry = dtuple_create(m_heap, 1);
  dtuple_set_n_fields_cmp(base_pk_entry, 1);
  dict_index_copy_types(base_pk_entry, sec_index, 1);

  // store the old pk (prior to this update) as the search key
  auto [pk_val, pk_len] =
      write_pk_from_tuple(m_old_pk, base_table->first_index(), m_heap);
  dfield_set_data(dtuple_get_nth_field(base_pk_entry, 0), pk_val, pk_len);
  ut_ad(dtuple_check_typed(base_pk_entry));

  // open and position cursor on the secondary index record
  btr_pcur_t sec_pcur;
  sec_pcur.open(sec_index, 0, base_pk_entry, PAGE_CUR_GE, BTR_SEARCH_LEAF,
                mtr, UT_LOCATION_HERE);

  ulint offsets_[REC_OFFS_NORMAL_SIZE];
  ulint* offsets = offsets_;
  rec_offs_init(offsets_);

  do {
    const rec_t* rec = sec_pcur.get_rec();

    // skip system records
    if (!page_rec_is_user_rec(rec)) {
      continue;
    }

    offsets = rec_get_offsets(rec, sec_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    if (rec_get_deleted_flag(rec, rec_offs_comp(offsets))) {
      /* TODO: Figure out if we need to take record lock on secondary index
      entries. Intuitively, we should not need to. Multiple exact entries
      exist because it is possible that this transaction or some other
      transaction has deleted marked this entry as part of an update where
      partition_id changed. As secondary indexes contain PK and partition_id
      is part of PK, we'd have delete marked this entry and inserted a new
      entry with same base_pk value but different partition_id.
      As we are only interested in the non-delete mark entry, we are probably
      OK to skip locking this record but we need to verify this. */
#if 0
      auto ret = lock_sec_rec_read_check_and_lock(
          lock_duration_t::AT_LEAST_STATEMENT, block, rec, sec_index, offsets,
          SELECT_ORDINARY, LOCK_S, LOCK_REC_NOT_GAP, thr);
      if (ret != DB_SUCCESS && ret != DB_SUCCESS_LOCKED_REC) {
        break;
      }
#endif
      continue;
    }

    break;
  } while (sec_pcur.move_to_next(mtr));

  if (!validate_rec(sec_pcur.get_rec(), base_pk_entry, sec_index)) {
    ut_d(ut_error);
    sec_pcur.close();
    mtr_commit(mtr);
    return nullptr;
  }

  // build the row reference from the secondary index record
  auto ref =
      row_build_row_ref(ROW_COPY_DATA, sec_index, sec_pcur.get_rec(), m_heap);

  sec_pcur.close();

  return ref;
}

void KMeansTable::get_upd_data(const upd_node_t* base_upd_node) {
  auto base_index = base_upd_node->table->first_index();
  auto base_pcur = base_upd_node->pcur;
  auto upd = base_upd_node->update;

  ut_a(base_pcur->is_clustered());
  ut_a(base_pcur->is_positioned());
  ut_a(base_pcur->get_rel_pos() == BTR_PCUR_ON);
  ut_a(base_pcur->m_old_rec != nullptr);
  ut_a(base_pcur->m_old_n_fields ==
       dict_index_get_n_unique_in_tree(base_index));

  m_old_pk = dict_index_build_data_tuple(
                 const_cast<dict_index_t*>(base_index),
                 base_pcur->m_old_rec,
                 base_pcur->m_old_n_fields,
                 m_heap);

  /* During delete, all we need is the old pk. */
  if (base_upd_node->is_delete) {
    return;
  }

  for (ulint i = 0; i < upd->n_fields; i++) {
    auto field = upd->fields + i;
    // get the new value of embedding
    if (field->field_no == get_vector_col_pos(base_index)) {
      m_new_embedding =
          reinterpret_cast<dfield_t*>(mem_heap_alloc(m_heap, sizeof(dfield_t)));
      dfield_copy(m_new_embedding, &field->new_val);
      ut_a(m_new_embedding->len == UNIV_SQL_NULL ||
           (m_new_embedding->len == get_vector_dim(base_index) * 4));
    }

    // get the new value of PK if it is updated
    for (int j = 0; j < dict_index_get_n_unique_in_tree(base_index); j++) {
      auto col = base_index->get_col(j);
      if (col->get_col_phy_pos() == field->field_no) {
        auto col_pos = field->field_no;
        // it may be that the updated field is only part of PK prefix
        if (col->has_prefix_phy_pos()) {
          col_pos = col->get_prefix_phy_pos();
        }
        if (m_new_pk == nullptr) {
          m_new_pk = dtuple_copy(m_old_pk, m_heap);
        }

        ut_a(col_pos < m_new_pk->n_fields);
        auto df = dtuple_get_nth_field(m_new_pk, col_pos);
        auto uf = &field->new_val;
        dfield_copy(df, uf);
        if (col->has_prefix_phy_pos()) {
          // if it is a prefix, we need to adjust the length of the field
          auto new_len = dtype_get_at_most_n_mbchars(
              col->prtype, col->mbminmaxlen,
              base_index->get_field(col_pos)->prefix_len, dfield_get_len(df),
              static_cast<const char*>(dfield_get_data(df)));
          dfield_set_len(df, new_len);
        }
      }
    }
  }
  ut_a(m_old_pk);
  ut_a(m_new_pk || m_new_embedding);
}

byte* KMeansTable::get_partition_id(KMeansIndex* vec_index,
                                  const byte* data,
                                  size_t len,
                                  mem_heap_t* heap) {
  ut_a(len == UNIV_SQL_NULL || len == vec_index->dims() * sizeof(VectorDataT));

  static int64_t null_partition_id = m_null_partition_id_threshold;
  auto buf = static_cast<byte*>(mem_heap_alloc(heap, partition_id_len));
  if (len == UNIV_SQL_NULL) {
    mach_write_int_type(buf, (const byte*)&null_partition_id, partition_id_len,
                        false);
    return buf;
  }

  Vector vec(reinterpret_cast<VectorDataT*>(const_cast<byte*>(data)),
             reinterpret_cast<VectorDataT*>(const_cast<byte*>(data+len)));

  Partitions part;
  vec_index->tokenize(vec, &part, 1);
  ut_a(part.size() == 1);
  ut_a(part[0] >= 0);

  uint64_t id = part[0];
  mach_write_int_type(buf, (const byte*)&id, partition_id_len, false);
  return buf;
}

std::pair<byte*, size_t> KMeansTable::get_quantized_vector(KMeansIndex* vec_index,
                                                         const byte* data,
                                                         size_t len,
                                                         mem_heap_t* heap) {
  ut_a(len == UNIV_SQL_NULL || len == vec_index->dims() * sizeof(VectorDataT));

  if (len == UNIV_SQL_NULL) {
    return {nullptr, UNIV_SQL_NULL};
  }

  size_t dim = len / sizeof(VectorDataT);
  auto orig_vec = absl::MakeConstSpan((VectorDataT*)data, dim);
  auto d = kmeans_wrapper::CreateDatapointFromPtr(orig_vec.data(), dim);
  vec_index->normalize_data(d.get());

  auto vec = absl::MakeConstSpan(d->float_values(), d->dimensionality());

  /* Allocate one byte per dimension */
  size_t total_len = dim;
  /* For L2_SQUARED we append the l2_norm to the vector. */
  if (vec_index->dist_measure() == DistMeasure::L2_SQUARED) {
    total_len += 4;
  }

  auto q_buf = static_cast<byte*>(mem_heap_alloc(heap, total_len));

  /* Quantize the vector into the buffer */
  auto q_vec = absl::MakeSpan((QuantizedDataT*)q_buf, dim);
  vec_index->quantize(vec, q_vec);

  /* Calculate and write the l2_norm based on original vector. */
  if (vec_index->dist_measure() == DistMeasure::L2_SQUARED) {
    auto inv_multi = vec_index->inverse_multipliers();
    VectorDistT l2_norm = 0.0;
    for (size_t i = 0; i < dim; i++) {
      auto dequantized = inv_multi[i] * q_vec[i];
      l2_norm += dequantized * dequantized;
    }
    memcpy(q_buf + dim, &l2_norm, 4);
  }

  return {q_buf, total_len};
}

void KMeansTable::create_query_graph(dict_table_t* sub_table) {
  ut_a(m_ins_node == nullptr);
  ut_a(m_que_thr == nullptr);
  ut_a(m_create_info != nullptr);

  m_ins_node = ins_node_create(INS_DIRECT, sub_table, m_heap);
  m_ins_node->select = nullptr;
  m_ins_node->values_list = nullptr;

  dtuple_t* tuple = dtuple_create(m_heap, sub_table->get_n_cols());
  dict_table_copy_types(tuple, sub_table);
  ut_ad(dtuple_check_typed(tuple));
  ut_ad(!dict_table_have_virtual_index(sub_table));

  ins_node_set_new_row(m_ins_node, tuple);

  m_que_thr = pars_complete_graph_for_exec(m_ins_node, m_create_info->m_trx,
                                           m_heap, nullptr);

  auto ins_graph = static_cast<que_fork_t*>(que_node_get_parent(m_que_thr));
  ins_graph->state = QUE_FORK_ACTIVE;
}

dberr_t KMeansTable::get_partitioner(dict_table_t* sub_table,
                                   byte** data, ulint len) {
  ut_a(sub_table);
  ut_a(data && len);

  *data = nullptr;
  auto buf = static_cast<byte*>(mem_heap_alloc(m_heap, len));
  size_t actual_len = 0;
  auto err = read_non_leaf_row(sub_table,
                               m_partitioner_id,
                               m_partitioner_pk,
                               &buf,
                               &actual_len);
  if (err == DB_SUCCESS) {
    if (actual_len != len) {
      err = DB_INDEX_CORRUPT;
    } else {
      *data = buf;
    }
  }

  return err;
}

dberr_t KMeansTable::get_multipliers(dict_table_t* sub_table,
                                   byte** data, ulint len) {
  ut_a(sub_table);
  ut_a(data && len);

  *data = nullptr;
  auto buf = static_cast<byte*>(mem_heap_alloc(m_heap, len));
  size_t actual_len = 0;
  auto err = read_non_leaf_row(sub_table,
                               m_multipliers_id,
                               m_multipliers_pk,
                               &buf,
                               &actual_len);
  if (err == DB_SUCCESS) {
    if (actual_len != len) {
      err = DB_INDEX_CORRUPT;
    } else {
      *data = buf;
    }
  }

  return err;
}

dberr_t KMeansTable::insert_non_leaf_row(dict_table_t* sub_table,
                                       int64_t partition_id, byte pk) {
  ut_a(m_index);
  create_query_graph(sub_table);
  ut_ad(m_ins_node != nullptr);
  ut_ad(m_que_thr != nullptr);

  dtuple_t* tuple = dtuple_create(m_heap, sub_table->get_n_cols());
  dict_table_copy_types(tuple, sub_table);
  ut_ad(dtuple_check_typed(tuple));

  ins_node_set_new_row(m_ins_node, tuple);

  /* Buffer to hold partition id */
  auto buf = static_cast<byte*>(mem_heap_alloc(m_heap, partition_id_len));
  mach_write_int_type(buf, (const byte*)&partition_id, partition_id_len, false);

  for (int i = 0; i < Cols::NUM_COLS; i++) {
    dfield_t* field = dtuple_get_nth_field(tuple, i);
    switch (i) {
      case Cols::PARTITION_ID: {
        dfield_set_data(field, buf, partition_id_len);
        break;
      }
      case Cols::BASE_PK: {
        dfield_set_data(field, &pk, partitioner_pk_len);
        break;
      }
      case Cols::CONTENT: {
        if (partition_id == m_partitioner_id) {
          std::string part_buf;
          m_index->serialized_partitioner(part_buf);
          auto len = part_buf.size();
          auto serialized_partitioner =
              static_cast<byte*>(mem_heap_alloc(m_heap, len));
          memcpy(serialized_partitioner, part_buf.data(), len);
          dfield_set_data(field, serialized_partitioner, len);

          /** Update the tree_size in the vector index info */
          auto info = dict_table_get_vector_index_info(m_create_info->m_table);
          ut_a(info != nullptr);
          info->set_tree_size(len);
        } else {
          auto multipliers = m_index->multipliers();
          auto len = multipliers.size() * sizeof(VectorDataT);
          dfield_set_data(field, (byte*)multipliers.data(), len);
        }
        break;
      }
    }
  }

  auto err = run_node(m_ins_node);

  /* We don't expect any errors here. No lock waits, no duplicate keys etc. */
  if (err == DB_SUCCESS) {
    srv_stats.n_rows_inserted.inc();
    dict_table_n_rows_inc(sub_table);
  } else {
    ib::error() << "Failed to insert non-leaf row in: " << sub_table->name;
    ut_d(ut_error);
  }

  if (m_ins_node->entry_sys_heap != nullptr) {
    mem_heap_free(m_ins_node->entry_sys_heap);
    m_ins_node->entry_sys_heap = nullptr;
  }

  m_ins_node = nullptr;
  m_que_thr = nullptr;
  return err;
}

dberr_t KMeansTable::persist_partitioner(dict_table_t* sub_table) {
  ut_ad(m_ins_node == nullptr);
  ut_ad(m_que_thr == nullptr);
  ut_ad(m_create_info != nullptr);

  /* Insert partitioner */
  auto err = insert_non_leaf_row(sub_table, m_partitioner_id, m_partitioner_pk);
  if (err == DB_SUCCESS) {
    /* Insert multipliers */
    err = insert_non_leaf_row(sub_table, m_multipliers_id, m_multipliers_pk);
  }

  ut_ad(m_ins_node == nullptr);
  ut_ad(m_que_thr == nullptr);
  ut_ad(m_create_info != nullptr);

  return err;
}

dberr_t KMeansTable::read_non_leaf_row(dict_table_t* sub_table, int64_t id,
                                     byte pk, byte** data, ulint* len) {
  dberr_t err = DB_SUCCESS;
  /* Build an entry to search the clustered index */
  dict_index_t* clust_index = sub_table->first_index();
  dtuple_t* entry = dtuple_create(m_heap, 2);
  dtuple_set_n_fields_cmp(entry, 2);
  dict_index_copy_types(entry, clust_index, 2);
  ut_ad(dtuple_check_typed(entry));

  auto buf = static_cast<byte*>(mem_heap_alloc(m_heap, partition_id_len));
  mach_write_int_type(buf, (const byte*)&id, partition_id_len, false);

  auto field_data = buf;
  auto field_len = partition_id_len;
  auto field = dtuple_get_nth_field(entry, 0);
  dfield_set_data(field, field_data, field_len);

  field_data = &pk;
  field_len = partitioner_pk_len;
  field = dtuple_get_nth_field(entry, 1);
  dfield_set_data(field, field_data, field_len);

  btr_pcur_t pcur;
  mtr_t mtr;
  mtr_start(&mtr);
  pcur.open_on_user_rec(clust_index, entry, PAGE_CUR_GE, BTR_SEARCH_LEAF, &mtr,
                        UT_LOCATION_HERE);

  /* We must be able to find the partitioner row. Though we play safe here. */
  ut_ad(pcur.is_on_user_rec());
  if (!pcur.is_on_user_rec()) {
    err = DB_RECORD_NOT_FOUND;
    goto func_exit;
  }

  /* No reason for it to be delete marked. */
  if (!validate_rec(pcur.get_rec(), entry, clust_index)) {
    err = DB_RECORD_NOT_FOUND;
    goto func_exit;
  }

  {
    /* Read the record */
    const rec_t* rec = pcur.get_rec();
    ulint offsets_[REC_OFFS_NORMAL_SIZE];
    ulint* offsets = offsets_;
    rec_offs_init(offsets_);
    offsets = rec_get_offsets(rec, clust_index, offsets, ULINT_UNDEFINED,
                              UT_LOCATION_HERE, &m_heap);

    auto pos = ClustIndexFields::CLUST_CONTENT;
    if (rec_offs_nth_extern(clust_index, offsets, pos)) {
      *data = lob::btr_rec_copy_externally_stored_field(
          nullptr, clust_index, rec, offsets, dict_table_page_size(sub_table),
          pos, len, nullptr, dict_index_is_sdi(clust_index), m_heap);

      /* We shouldn't get NULL here. */
      ut_a(*len != UNIV_SQL_NULL);

      /* We shouldn't get 0 in length which happens only if other threads are
      trying to update the same blob. */
      if (*len == 0) {
        err = DB_RECORD_NOT_FOUND;
        goto func_exit;
      }
    } else {
      const byte* rec_data =
          rec_get_nth_field(clust_index, rec, offsets, pos, len);
      if (*len == UNIV_SQL_NULL) {
        err = DB_RECORD_NOT_FOUND;
        goto func_exit;
      }

      /* Copy the data to the buffer. */
      *data = static_cast<byte*>(mem_heap_alloc(m_heap, *len));
      memcpy(*data, rec_data, *len);
    }
  }

func_exit:
  if (err != DB_SUCCESS) {
    ib::error() << "Failed to read non-leaf row in: " << sub_table->name;
    ut_d(ut_error);
    *data = nullptr;
    *len = 0;
  }

  mtr_commit(&mtr);
  pcur.close();

  return err;
}

} /* namespace ib_vector */

