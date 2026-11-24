// Copyright 2026 Google LLC

#ifndef _INCLUDE_VECTOR0VECTOR_H_
#define _INCLUDE_VECTOR0VECTOR_H_

#include <error.h>
#include <sys/stat.h>

#include <cstdint>
#include <string_view>

#include "dict0mem.h"

class VectorSearchResults;

extern bool opt_cloudsql_vector;
extern ulong opt_cloudsql_vector_max_mem_size;
extern bool opt_initialize;
extern bool opt_cloudsql_vector_test_mode;

namespace ib_vector {

/* Forward declaration */
class VectorIndex;

class VectorCfg;

/** Struct to carray information CREATE VECTOR INDEX */
struct CreateVectorIndexInfo {
  CreateVectorIndexInfo(const Alter_inplace_info* alter_info, trx_t* trx,
                        TABLE* mysql_table, ulint key_num, dict_table_t* table,
                        dict_index_t* index, Alter_stage* stage)
      : m_alter_info(alter_info),
        m_trx(trx),
        m_mysql_table(mysql_table),
        m_key_num(key_num),
        m_table(table),
        m_vector_index(index),
        m_stage(stage) {
    ut_a(validate());
  }

  bool validate() const;

  /* Information about the vector index options. */
  const Alter_inplace_info* m_alter_info{nullptr};

  /* Transaction handle */
  trx_t* m_trx{nullptr};

  /* SQL layer TABLE structure to the base table */
  TABLE* m_mysql_table{nullptr};

  /* Key number of vector index in SQL layer */
  ulint m_key_num{0};

  /* InnoDB table handle */
  dict_table_t* m_table{nullptr};

  /* InnoDB index handle for vector index */
  dict_index_t* m_vector_index{nullptr};

  /* Alter stage */
  Alter_stage* m_stage{nullptr};

  /* Availalbe memory size */
  size_t m_mem_size{0};

  /* Set to true if we encounter an error during sub_talbe build */
  bool m_build_error{false};
};

/** This is called during CREATE VECTOR INDEX path. This function is responsible
for creating the vector index object, training the index and persisting the
index into the sub_table. Specifically, this function does the following:
1. Generate vector index config
2. Create vector index object
3. Train the index
4. Persist the index to the sub_table
5. Mark the index as ready to use.
@param[in]    create_info    struct containing information about the CREATE
                             VECTOR INDEX operation
@return DB_SUCCESS on success or error code on failure */
dberr_t create_persistent_vector_index(CreateVectorIndexInfo* cr_info);

/** This is called during CHECK TABLE path. This function is responsible
for validating consistency of the btree of the indexes in the sub_table
corresponding to the vector index.
@param[in]    table    the base table
@param[in]    trx      the transaction handle
@return DB_SUCCESS on success or error code on failure
@return true if ok **/
bool validate_sub_table_index_btree(dict_table_t* table, trx_t* trx);

/** This is called during CHECK TABLE path. This function is responsible
for validating consistency of the  indexes in the sub_table corresponding to the
vector index.

@param[in]     table               the base table
@param[in]     trx                 the transaction handle
@param[in]     max_threads         Number of threads to use for the scan
@param[in]     check_keys          True if called form check table.
@param[out]    n_rows              Number of entries seen in consistent read.
@return DB_SUCCESS or other error */
dberr_t scan_sub_table_recs(dict_table_t* table, trx_t* trx, size_t max_threads,
                            bool check_keys,
                            ulint* n_rows);

/** This is called during CHECK TABLE path. This function mark a vector index as
unusable if needed..
@param[in]     table               the base table
@param[in]     index               vector index
*/
void mark_unusable_if_needed(dict_table_t* table, dict_index_t* index);

/** Update the memory limit for vector indexes when cloudsql_vector_max_mem_size
is changed dynamically. */
void update_vector_max_mem_size();

} /* namespace ib_vector */

#endif /* _INCLUDE_VECTOR0VECTOR_H_ */
