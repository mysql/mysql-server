// Copyright 2026 Google LLC

#include <cstdio>
#include <memory>
#include <string>
#include <utility>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <vector>

#include "db0err.h"
#include "dict0dd.h"
#include "dict0dict.h"
#include "dict0mem.h"
#include "include/mysqld_error.h"
#include "mem0mem.h"
#include "my_dbug.h"
#include "row0mysql.h"
#include "sql/current_thd.h"
#include "sql/dd/properties.h"
#include "vector0subtable.h"
#include "univ.i"
#include "ut0core.h"
#include "ut0dbg.h"
#include "ut0log.h"

namespace ib_vector {

std::string vector_generate_sub_table_name(const char *table_name,
                                  table_id_t table_id, space_index_t index_id) {
  const char* db_name_end = std::strchr(table_name, '/');
  size_t db_name_len = db_name_end - table_name + 1 /* +1 for back slash */;
  ut_ad(db_name_end);
  // sub table name format is dbname/vector_index_<table_id>_<index_id>
  return std::format("{:.{}}{}{:016x}_{:016x}",
                                          table_name, db_name_len,
                                          "vector_index_", table_id, index_id);
}

std::string vector_generate_sub_table_name(dict_table_t *table,
                                  dict_index_t *index) {
  return vector_generate_sub_table_name(table->name.m_name,
                                        table->id, index->id);
}

std::string vector_generate_sub_table_name(dict_table_t *table) {
  dict_index_t *vector_index = nullptr;
  /* TODO : Replace with wrapper function available */
  for (vector_index = table->first_index(); vector_index;
    vector_index = vector_index->next()) {
    if (dict_index_is_vector(vector_index)) {
      break;
    }
  }
  ut_ad(vector_index != nullptr);
  return vector_generate_sub_table_name(table->name.m_name,
                                 table->id, vector_index->id);
}

dict_table_t* create_vector_index_sub_table(trx_t *trx, dict_index_t *index) {
  dict_table_t *parent_table;
  dict_table_t *sub_table = nullptr;
  dberr_t error;
  dict_index_t *sub_table_index = nullptr;
  trx_dict_op_t op;

  ut_ad(!dict_sys_mutex_own());
  /* row_mysql_lock_data_dictionary must have been called before this */
  ut_ad(rw_lock_own(dict_operation_lock, RW_LOCK_X));

  parent_table = dd_table_open_on_name_in_mem(index->table_name, false);
  ut_a(parent_table != nullptr);
  ut_d(dict_sys_mutex_enter());
  ut_ad(parent_table->get_ref_count() >= 1);
  ut_d(dict_sys_mutex_exit());
  ut_ad(dict_index_is_vector(index));

  std::string sub_table_name =
      vector_generate_sub_table_name(parent_table, index);

  ut_ad(parent_table->flags2 & DICT_TF2_USE_FILE_PER_TABLE);
  ut_ad(!(parent_table->flags2 & DICT_TF2_TEMPORARY));
  /* sub table must use file per table and encryption file per table flags */
  //uint32_t flags2 = (DICT_TF2_USE_FILE_PER_TABLE |
  //                   DICT_TF2_ENCRYPTION_FILE_PER_TABLE);
  uint32_t flags2 = ((parent_table->flags2 & DICT_TF2_USE_FILE_PER_TABLE) |
                     (parent_table->flags2 &
                      DICT_TF2_ENCRYPTION_FILE_PER_TABLE) |
                     (parent_table->flags2 & DICT_TF2_TEMPORARY));

  sub_table = dict_mem_table_create(sub_table_name.c_str(),
                                    parent_table->space,
                                    3 /*number of columns */, 0, 0,
                                    parent_table->flags, flags2);
  ut_ad(sub_table != nullptr);


  mem_heap_t *heap = mem_heap_create(1024, UT_LOCATION_HERE);
  dict_mem_table_add_col(sub_table, heap, "partition_id", DATA_INT,
                         DATA_NOT_NULL, 8, true);
  dict_mem_table_add_col(
      sub_table, heap, "base_pk", DATA_BINARY, DATA_NOT_NULL, 3072, true);

  dict_mem_table_add_col(sub_table, heap, "content", DATA_BLOB,
                         0, 0 /* zero means variable size */, true);

  error = row_create_table_for_mysql(sub_table, nullptr, nullptr, trx, nullptr);

  if (error != DB_SUCCESS) {
    trx->error_state = error;
    sub_table = nullptr;
    ib::warn(ER_IB_MSG_466)
        << "Failed to create vector index's subtable for " << index->table_name;
    goto exit;
  }
  sub_table_index =
      dict_mem_index_create(sub_table_name.c_str(), "subtable_pk",
                            sub_table->space, DICT_UNIQUE | DICT_CLUSTERED, 2);
  sub_table_index->add_field("partition_id", 0, true);
  sub_table_index->add_field("base_pk", 0, true);

  op = trx_get_dict_operation(trx);

  error = row_create_index_for_mysql(sub_table_index, trx, nullptr, nullptr);

  trx->dict_operation = op;
  if (error != DB_SUCCESS) {
    trx->error_state = error;
    sub_table = nullptr;
    ib::warn(ER_IB_MSG_466) << "Failed to create vector index's subtable index "
                            << " for " << index->table_name;
    goto exit;
  }

  sub_table_index = dict_mem_index_create(
      sub_table_name.c_str(), "base_pk", sub_table->space, DICT_UNIQUE, 1);
  sub_table_index->add_field("base_pk", 0, true);

  op = trx_get_dict_operation(trx);

  error = row_create_index_for_mysql(sub_table_index, trx, nullptr, nullptr);

  trx->dict_operation = op;

  if (error != DB_SUCCESS) {
    trx->error_state = error;
    ib::warn(ER_IB_MSG_466) << "Failed to create vector subtable's index "
                            << " for " << index->table_name;
    goto exit;
  }

exit:
  dd_table_close(parent_table, nullptr, nullptr, false);

  mem_heap_free(heap);
  if (error == DB_SUCCESS) {
    index->fill_dd = true;
  }
  /* return nullptr in case of failure */
  return error == DB_SUCCESS ? sub_table : nullptr;
}

dberr_t create_vector_index_dd_sub_table(dict_table_t *parent_table) {
  dberr_t error = DB_SUCCESS;
  MDL_ticket *mdl_ticket = nullptr;
  dict_index_t *index = parent_table->first_index();
  for (; index != nullptr; index = index->next()) {
    if (dict_index_is_vector(index) && index->fill_dd) {
      /* only one vector index is allowed per table */
      break;
    }
  }
  ut_ad(index != nullptr && dict_index_is_vector(index));
  dict_table_t *sub_table = nullptr;
  CHARSET_INFO *charset;
  dict_field_t *field;

  field = index->get_field(0);
  uint cs_num = (uint)dtype_get_charset_coll(field->col->prtype);
  charset = get_charset(cs_num, MYF(MY_WME));

  std::string sub_table_qualified_name =
      vector_generate_sub_table_name(parent_table, index);

  sub_table = dd_table_open_on_name_in_mem(
      sub_table_qualified_name.c_str(), false);
  ut_ad(sub_table != nullptr);

  ut_ad(charset != nullptr);

  std::string sub_table_db_name;
  std::string sub_table_name;
  dict_name::get_table(sub_table_qualified_name,
                       sub_table_db_name, sub_table_name);

  /* Create dd::Table object */
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  const dd::Schema *schema = nullptr;
  if (mdl_locker.ensure_locked(sub_table_db_name.c_str()) ||
      client->acquire<dd::Schema>(sub_table_db_name.c_str(), &schema)) {
    return DB_ERROR;
  }

  /* Check if schema is nullptr? */
  if (schema == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), sub_table_db_name.c_str());
    dd_table_close(sub_table, nullptr, nullptr, false);
    return DB_ERROR;
  }

  std::unique_ptr<dd::Table> dd_table_obj(schema->create_table(thd));
  dd::Table *dd_table = dd_table_obj.get();

  dd_table->set_name(sub_table_name.c_str());
  dd_table->set_schema_id(schema->id());
  dd_table->set_engine(innobase_hton_name);
  /* sub table should be hidden. For debugging reasons, adding
     a simulation point to make sub table visible for debug builds.
     Usage : 
     SET SESSION debug='+d,skip_sub_table_hidden';
     CREATE VECTOR INDEX ....
     SELECT * FROM <sub_table>;
  */
  if (DBUG_EVALUATE_IF("skip_sub_table_hidden", 0, 1)) {
    dd_table->set_hidden(dd::Abstract_table::HT_HIDDEN_SE);
  }
  ut_ad(dict_tf_get_rec_format(sub_table->flags) == REC_FORMAT_DYNAMIC);
  dd_table->set_row_format(dd::Table::RF_DYNAMIC);

  dd::Properties *table_options = &dd_table->options();
  table_options->set("pack_record", true);
  table_options->set("checksum", false);
  table_options->set("delay_key_write", false);
  table_options->set("avg_row_length", 0);
  table_options->set("stats_sample_pages", 0);
  table_options->set("stats_auto_recalc", HA_STATS_AUTO_RECALC_DEFAULT);
  /* no compression for vector sub table */
  ut_ad(DICT_TF_GET_ZIP_SSIZE(sub_table->flags) == 0);
  table_options->set("key_block_size", 0);

  dd::Properties *se_private_data = &dd_table->se_private_data();
  se_private_data->set(dd_table_key_strings[DD_TABLE_VECTOR_SUB_TABLE_PARENT_ID],
                       parent_table->id);
  sub_table->is_vector_sub_table = true;

  /* 1st column: partition_id */
  const char *col_name = nullptr;
  dd::Column *col = dd_table->add_column();
  col_name = "partition_id";
  col->set_name(col_name);
  col->set_type(dd::enum_column_types::LONGLONG);
  col->set_numeric_scale(0);
  col->set_nullable(false);
  col->set_char_length(8);
  col->set_collation_id(charset->number);


  dd::Column *key_col1 = col;

  /* 2nd column: base_pk */
  col = dd_table->add_column();
  col->set_name("base_pk");
  col->set_type(dd::enum_column_types::VARCHAR);
  col->set_char_length(3072);
  col->set_nullable(false);
  col->set_collation_id(charset->number);
  col->set_collation_id(my_charset_bin.number);

  dd::Column *key_col2 = col;

  /* 3rd column: content */
  col = dd_table->add_column();
  col->set_name("content");
  col->set_type(dd::enum_column_types::BLOB);
  col->set_char_length(8);
  col->set_nullable(true);
  col->set_collation_id(my_charset_bin.number);

  /* Fill index */
  dd::Index *sub_table_index1 = dd_table->add_index();
  sub_table_index1->set_name("subtable_pk");
  sub_table_index1->set_algorithm(dd::Index::IA_BTREE);
  sub_table_index1->set_algorithm_explicit(false);
  sub_table_index1->set_visible(true);
  sub_table_index1->set_type(dd::Index::IT_PRIMARY);
  sub_table_index1->set_ordinal_position(1);
  sub_table_index1->set_generated(false);
  sub_table_index1->set_engine(dd_table->engine());

  sub_table_index1->options().set("flags", 32); /*TODO: 32 ?*/

  dd::Index_element *index_elem1;
  index_elem1 = sub_table_index1->add_element(key_col1);
  index_elem1->set_length(8);

  index_elem1 = sub_table_index1->add_element(key_col2);
  index_elem1->set_length(3072);

  /* Fill index */
  dd::Index *sub_table_index2 = dd_table->add_index();
  sub_table_index2->set_name("base_pk");
  sub_table_index2->set_algorithm(dd::Index::IA_BTREE);
  sub_table_index2->set_algorithm_explicit(false);
  sub_table_index2->set_visible(true);
  sub_table_index2->set_type(dd::Index::IT_UNIQUE);
  sub_table_index2->set_ordinal_position(2);
  sub_table_index2->set_generated(false);
  sub_table_index2->set_engine(dd_table->engine());

  sub_table_index2->options().set("flags", 32);

  dd::Index_element *index_elem2;
  index_elem2 = sub_table_index2->add_element(key_col2);
  index_elem2->set_length(3072);

  [[maybe_unused]] dd::Object_id space_id = parent_table->dd_space_id;
  ut_ad(space_id != dd::INVALID_OBJECT_ID);

  dd::Object_id dd_space_id = dd::INVALID_OBJECT_ID;

  ut_ad(dict_table_is_file_per_table(sub_table));
  char *filename = fil_space_get_first_path(sub_table->space);

  bool ret = dd_create_implicit_tablespace(client, sub_table->space,
                                      sub_table->name.m_name, filename, false,
                                      dd_space_id);

  ut::free(filename);
  if (ret) {
    error = DB_ERROR;
    goto exit;
  }
  sub_table->dd_space_id = dd_space_id;

  dd_write_table(dd_space_id, dd_table, sub_table);

  if (dd::acquire_exclusive_table_mdl(thd, sub_table_db_name.c_str(),
                                      sub_table_name.c_str(), false,
                                      &mdl_ticket)) {
    error = DB_ERROR;
    goto exit;
  }

  /* Store table to dd */
  if (client->store(dd_table)) {
    error = DB_ERROR;
    goto exit;
  }

exit:
  dd_table_close(sub_table, nullptr, nullptr, false);
  return error;
}

dberr_t drop_vector_index_sub_table(dict_table_t *parent_table,
                                    dict_index_t *index, trx_t *trx) {
  dberr_t error = DB_SUCCESS;

  std::string sub_table_name =
      vector_generate_sub_table_name(parent_table, index);

  THD *thd = current_thd;
  MDL_ticket *mdl = nullptr;

  /* Check that the table exists in our data dictionary.
  Similar to regular drop table case, we will open table with
  DICT_ERR_IGNORE_INDEX_ROOT and DICT_ERR_IGNORE_CORRUPT option */
  dict_table_t *sub_table = dd_table_open_on_name(
      thd, &mdl, sub_table_name.c_str(), true,
      static_cast<dict_err_ignore_t>(DICT_ERR_IGNORE_INDEX_ROOT |
                                     DICT_ERR_IGNORE_CORRUPT));

  if (sub_table == nullptr) {
    /* If the sub table is not found (e.g. due to an orphaned state or desync
    from prior operations), log a warning and return DB_TABLE_NOT_FOUND instead
    of asserting, allowing parent table/index drop to complete gracefully. */
    ib::warn(ER_IB_MSG_464)
        << "Vector index sub table " << sub_table_name
        << " not found for table " << parent_table->name.m_name;
    return DB_TABLE_NOT_FOUND;
  }

  dd_table_close(sub_table, thd, &mdl, true);

  error = row_drop_table_for_mysql(sub_table_name.c_str(), trx, false, nullptr);

  if (error != DB_SUCCESS) {
    ib::error(ER_IB_MSG_464)
        << "Unable to drop vector index sub table for table "
        << parent_table->name.m_name
        << ": " << ut_strerr(error);
    return (error);
  }
  return drop_vector_index_dd_sub_table(parent_table, index, trx);
}

dberr_t drop_vector_index_dd_sub_table(dict_table_t *parent_table,
                                       dict_index_t *index,
                                       trx_t *trx) {
  dberr_t error = DB_SUCCESS;
  dd::Object_id dd_space_id;
  const dd::Table *dd_table = nullptr;
  std::string sub_table_name =
      vector_generate_sub_table_name(parent_table, index);
  std::string db_name;
  std::string table_name;
  dict_name::get_table(sub_table_name, db_name, table_name);

  dict_sys_mutex_exit();
  THD *thd = current_thd;
  dd::Schema_MDL_locker mdl_locker(thd);
  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_name.c_str(), table_name.c_str(),
                                      false, &mdl_ticket)) {
    error = DB_ERROR;
    goto err_exit;
  }

  if (client->acquire<dd::Table>(db_name.c_str(), table_name.c_str(),
                                 &dd_table)) {
    error = DB_ERROR;
    goto err_exit;
  }

  if (dd_table == nullptr) {
    error = DB_ERROR;
    goto err_exit;
  }
  dd_space_id = (*dd_table->indexes().begin())->tablespace_id();
  if (dd_drop_tablespace(client, dd_space_id)) {
    error = DB_ERROR;
    goto err_exit;
  }
  if (client->drop(dd_table)) {
    error = DB_ERROR;
    goto err_exit;
  }
err_exit:
  dict_sys_mutex_enter();
  return error;
}

void pin_vector_index_sub_table(dict_table_t *table) {
  std::string table_name = vector_generate_sub_table_name(table);
  dict_table_t *index_table;
  ut_ad(dict_sys_mutex_own());
  index_table = dd_table_open_on_name_in_mem(table_name.c_str(), true);
  if (index_table != nullptr && index_table->can_be_evicted) {
    dict_table_prevent_eviction(index_table);
  }
  if (index_table != nullptr) {
    dd_table_close(index_table, nullptr, nullptr, true);
  }
}

void unpin_vector_index_sub_table(dict_table_t *table, bool dict_locked) {
  if (!dict_locked) {
    dict_sys_mutex_enter();
  }
  std::string table_name = vector_generate_sub_table_name(table);

  dict_table_t *index_table;
  index_table = dd_table_open_on_name_in_mem(table_name.c_str(), true);
  if (index_table != nullptr && !index_table->can_be_evicted) {
    dict_table_allow_eviction(index_table);
  }
  if (index_table != nullptr) {
    dd_table_close(index_table, nullptr, nullptr, true);
  }
  if (!dict_locked) {
    dict_sys_mutex_exit();
  }
}
dberr_t lock_vector_index_sub_table(THD *thd, dict_table_t *table) {
  ut_ad(table->is_vector_sub_table);
  std::string table_name = table->name.m_name;
  std::string db_n;
  std::string table_n;
  dict_name::get_table(table_name, db_n, table_n);
  MDL_ticket *mdl_ticket = nullptr;
  if (dd::acquire_exclusive_table_mdl(thd, db_n.c_str(), table_n.c_str(),
                                      false, &mdl_ticket)) {
    return (DB_ERROR);
  }
  return (DB_SUCCESS);
}
} /* namespace ib_vector */
