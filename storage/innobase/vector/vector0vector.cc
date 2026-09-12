// Copyright 2026 Google LLC

#include <memory>
#include <string>
#include <utility>

#include <cstdint>

#include "btr0btr.h"
#include "db0err.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "my_dbug.h"
#include "row0mysql.h"
#include "row0pread.h"
#include "kmeans0index.h"
#include "vector0pk.h"
#include "vector0vector.h"
#include "vector0cfg.h"
#include "vector0dd.h"
#include "vector0index.h"
#include "vector0types.h"
#include "ut0dbg.h"
#include "ut0log.h"

namespace ib_vector {

/** Creates an index object. This does not put the object in index registry
or perform any training or building steps.
@param[in]     table      table handle to InnoDB table struct
@param[in]     index_name name of the index
@param[in]     index_cfg  validated index configuration. We only add dimensions
@param[out]    err_msg    filled with output
@return pointer to index object or nullptr on failure */
static std::shared_ptr<VectorIndex> create_index_object(
    dict_table_t* table,
    std::string index_name,
    VectorCfg& index_cfg,
    std::string& err_msg) {
  dict_index_t* index = table->first_index();
  ut_a(index);

  const int dim = get_vector_dim(index);
  index_cfg.set_cfg(Options::VECTOR_DIMENSION, (int)dim);

  auto vector_col_info = get_vector_col_info(index);
  ut_a(vector_col_info != nullptr);
  auto base_table = vector_col_info->table_name_;
  std::shared_ptr<VectorIndex> vector_index = nullptr;
  IndexType idx_type = index_cfg[Options::INDEX_TYPE];
  switch (idx_type) {
    case IndexType::TREE_SQ:
      vector_index = std::shared_ptr<VectorIndex>(
          new KMeansIndex(index_name, base_table, std::move(index_cfg)));
      break;
    default:
      err_msg += "Failed: invalid index type";
      break;
  }

  if (!vector_index) {
    err_msg += "Failed: index create";
  }
  return vector_index;
}

bool CreateVectorIndexInfo::validate() const {
  ut_a(m_alter_info);
  ut_a(m_mysql_table);
  ut_a(m_table);
  ut_a(m_trx);
  ut_a(m_vector_index);
  ut_a(m_stage);
  ut_a(m_key_num > 0);

  ut_a(ib_vector::table_has_vector_col(m_table));
  ut_a(ib_vector::dict_table_has_vector_index(m_table));
  ut_a(ib_vector::dict_table_get_vector_index(m_table) == m_vector_index);
  ut_a(m_vector_index->type == DICT_VECTOR);

  return true;
}

static dberr_t get_vector_index_config(enum ha_key_alg alg,
                                       enum ha_key_sub_alg quantizer,
                                       enum distance_measure dist_measure,
                                       uint32_t num_partitions,
                                       VectorCfg& cfg) {
  /* Algorithm is always specified. And in version 1 only KMEANS is supported. */
  if (alg != HA_KEY_ALG_KMEANS) {
    ib::error() << "Unsupported Vector Index Algorithm: " << alg;
    return DB_UNSUPPORTED;
  }

  /* Only supported sub_alg is SQ8 in version 1. */
  switch (quantizer) {
    case HA_KEY_SUB_ALG_UNSPECIFIED:
      cfg.set_default(Options::INDEX_TYPE, IndexType::TREE_SQ);
      break;
    case HA_KEY_SUB_ALG_TREE_SQ:
      cfg.set_cfg(Options::INDEX_TYPE, IndexType::TREE_SQ);
      break;
    default:
      ib::error() << "Unsupported Vector Index Quantizer: " << quantizer;
      return DB_UNSUPPORTED;
  }

  switch (dist_measure) {
    case DISTANCE_MEASURE_DOT_PRODUCT:
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::DOT_PRODUCT);
      break;
    case DISTANCE_MEASURE_L2_SQUARED:
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::L2_SQUARED);
      break;
    case DISTANCE_MEASURE_COSINE:
      cfg.set_cfg(Options::DIST_MEASURE, DistMeasure::COSINE);
      break;
    case DISTANCE_MEASURE_UNSPECIFIED:
    default:
      ib::error() << "Unsupported Vector Index Distance Measure: "
                  << dist_measure;
      return DB_UNSUPPORTED;
  }

  /* Zero num_partitions imply that user has not specified the value. */
  if (num_partitions) {
    cfg.set_cfg(Options::NUM_PARTITIONS, (int)num_partitions);
  }

  return DB_SUCCESS;
}

dberr_t create_persistent_vector_index(CreateVectorIndexInfo* cr_info) {
  dberr_t err = DB_SUCCESS;

  std::shared_ptr<KMeansIndex> kmeans_index{nullptr};
  std::shared_ptr<VectorIndexInfo> vec_info{nullptr};
  index_err_t index_err;
  VectorCfg cfg;
  std::string err_msg;

  auto index = cr_info->m_vector_index;
  auto table = cr_info->m_table;
  auto trx = cr_info->m_trx;

  /* Step 1: Get a vector config from user provided options */
  const KEY* key = cr_info->m_alter_info->key_info_buffer + cr_info->m_key_num;

  ib::info() << "step 1: get_vector_index_config";
  err = get_vector_index_config(key->algorithm, key->quantizer,
                                key->distance_measure, key->m_num_partitions,
                                cfg);
  DBUG_EXECUTE_IF("vector_index_cfg_err", err = DB_UNSUPPORTED;);
  DBUG_EXECUTE_IF("vector_index_cfg_crash", DBUG_SUICIDE(););

  if (err != DB_SUCCESS) {
    err = DB_VEC_INDEX_CONFIG_ERROR;
    goto err_exit;
  }
  ib::info() << "Step 1: Complete: vector_index_config: " << cfg.to_string();

  if (trx_is_interrupted(trx)) {
    err = DB_INTERRUPTED;
    goto err_exit;
  }

  /* Step 2: Create a vector index object */
  ib::info() << "Step 2: Generating vector index object";
  kmeans_index = std::dynamic_pointer_cast<KMeansIndex>(
      create_index_object(table, index->name(), cfg, err_msg));
  ut_a(kmeans_index);
  kmeans_index->set_pk_fixed_len(get_pk_fixed_len(table->first_index()));

  /* Initialize vector index info and register the index. */
  vec_info = dict_table_get_vector_index_info(table);
  ut_a(vec_info != nullptr);
  ut_a(vec_info->sub_table_id() != 0);
  ut_a(vec_info->index() == nullptr);
  vec_info->set_index(kmeans_index);
  index_registry->register_persistent_index(index->id, vec_info);
  ib::info() << "Step 2: Complete: Generating vector index object";

  if (trx_is_interrupted(trx)) {
    err = DB_INTERRUPTED;
    goto err_exit;
  }

  /* Step 3: Train the index */
  ib::info() << "Step 3: Training vector index";
  kmeans_index->set_status("Building");
  err = kmeans_index->collect_training_data(table->first_index(), nullptr);
  DBUG_EXECUTE_IF("vector_index_scan_err", err = DB_IO_ERROR;);
  DBUG_EXECUTE_IF("vector_index_scan_crash", DBUG_SUICIDE(););
  if (err != DB_SUCCESS) {
    ib::error() << "Failed to scan table and build index: " << err_msg;
    if (err != DB_INTERRUPTED && err != DB_VEC_INDEX_OUT_OF_MEMORY) {
      err = DB_VEC_INDEX_BUILD_FAILED;
    }
    goto err_exit;
  }

  if (trx_is_interrupted(trx)) {
    err = DB_INTERRUPTED;
    goto err_exit;
  }

  kmeans_index->set_status("Training");
  index_err = kmeans_index->train();
  DBUG_EXECUTE_IF("vector_index_train_err",
                  index_err = NOT_ENOUGH_DATA_POINTS;);
  DBUG_EXECUTE_IF("vector_index_train_crash", DBUG_SUICIDE(););
  if (index_err != SUCCESS) {
    ib::error() << "Failed to train vector index: "
                << IndexErrToString(index_err);
    if (index_err == NOT_ENOUGH_DATA_POINTS) {
      err = DB_VEC_INDEX_NOT_ENOUGH_DATA;
    } else {
      err = DB_VEC_INDEX_TRAIN_FAILED;
    }
    goto err_exit;
  }
  ut_a(kmeans_index->state() == INDEX_PENDING_BUILD);
  ib::info() << "Step 3: Complete: Training vector index";

  if (trx_is_interrupted(trx)) {
    err = DB_INTERRUPTED;
    goto err_exit;
  }

  /* Step 4: Persist the index to the sub_table. */
  ib::info() << "Step 4: Persisting vector index";
  cr_info->m_mem_size = kmeans_index->training_memory_size();
  err = kmeans_index->persist(ib_vector::PersistType::PAGED, nullptr, cr_info);
  if (err != DB_SUCCESS) {
    ib::error() << "Failed to persist vector index: " << index->name();
    goto err_exit;
  }
  ib::info() << "Step 4: Complete: Persisting vector index";

  /* Step 5: Mark the index as ready to use. */
  ib::info() << "Step 5: Marking vector index as ready to use";
  vec_info->mark_ready();
  ut_a(dict_table_vector_index_is_ready(table));
  ib::info() << "Step 5: Complete: Marking vector index as ready to use";

  return DB_SUCCESS;

err_exit:
  ut_ad(err != DB_SUCCESS);
  /** We don't have to worry about memory occupied by the index object or it's
  potential entry in the registry. When we fail here the DDL is going to get
  rolled back and the index object will be destroyed. Also, the entry in the
  registry will be removed when the index object is destroyed. */
  return err;
}

bool validate_sub_table_index_btree(dict_table_t* table, trx_t* trx) {
  ut_a(table != nullptr);

  bool ret = true;
  dict_index_t* index;
  auto vec_info = dict_table_get_vector_index_info(table);
  ut_a(vec_info);
  auto sub_table = vec_info->open_sub_table();
  if (sub_table == nullptr) {
    ib::info() << "Check table: sub table is null";
    return false;
  }

  for (index = sub_table->first_index(); index != nullptr;
       index = index->next()) {
    ut_a(!dict_index_is_vector(index));

    if (index->is_corrupted()) {
      ib::info() << "Check table: corrupted index: " << index->name();
      ret = false;
      continue;
    }

    if (!btr_validate_index(index, trx, false)) {
      ib::info() << "Check table: validate index failed for index: "
                 << index->name();
      ret = false;
    }

    DBUG_EXECUTE_IF(
        "dict_set_sub_table_clust_index_corrupted",
        if (index->is_clustered()) { ret = false; });

    DBUG_EXECUTE_IF(
        "dict_set_sub_table_index_corrupted",
        if (!index->is_clustered()) { ret = false; });
  }
  vec_info->close_sub_table(sub_table);

  return ret;
}

dberr_t scan_sub_table_recs(dict_table_t* table, trx_t* trx, size_t max_threads,
                            bool check_keys,
                            ulint* n_rows) {
  ut_a(table != nullptr);

  dberr_t ret = DB_SUCCESS;
  auto vec_info = dict_table_get_vector_index_info(table);
  ut_a(vec_info);
  auto sub_table = vec_info->open_sub_table();
  if (sub_table == nullptr) {
    ib::info() << "Check table: sub table is null";
    return DB_INDEX_CORRUPT;
  }

  // TODO: To include non-clustered indexes as well.
  dict_index_t* index = sub_table->first_index();
  ut_a(index != nullptr);
  ut_a(index->is_clustered());

  DBUG_EXECUTE_IF("dict_set_sub_table_corrupted_clust_index", ret = DB_ERROR;
                  goto close_sub_table_and_exit;);

  if (index->is_corrupted()) {
    ib::info() << "Check table: corrupted index: " << index->name();
    ret = DB_ERROR;
    goto close_sub_table_and_exit;
  }

  max_threads = Parallel_reader::available_threads(max_threads, false);
  // if no threads are available, use 1 thread for synchronous scan.
  max_threads = max_threads <= 1 ? 2 : max_threads;

  DBUG_EXECUTE_IF(
      "dict_set_sub_table_clust_index_row_scan_corrupted",
      if (index->is_clustered()) {
        ret = DB_ERROR;
        goto close_sub_table_and_exit;
      });

  ret = parallel_check_table(trx, index, max_threads, n_rows);

  DBUG_EXECUTE_IF("dict_set_sub_table_mismatch_rows_count", *n_rows = 103;);

  if (ret != DB_SUCCESS) {
    ib::info() << "Check table: Parallel check table failed for index: "
               << index->name();
    goto close_sub_table_and_exit;
  }
close_sub_table_and_exit:
  // close sub_table
  vec_info->close_sub_table(sub_table);
  return ret;
}

/* Mark a vector index as unusable if needed. */
void mark_unusable_if_needed(dict_table_t* table, dict_index_t* index) {
  ut_a(table != nullptr);
  ut_a(dict_index_is_vector(index));
  ut_a(index->is_corrupted());
  auto vec_info = dict_table_get_vector_index_info(table);
  ut_a(vec_info);
  vec_info->mark_unusable_if_needed(index);
}
} /* namespace ib_vector */
