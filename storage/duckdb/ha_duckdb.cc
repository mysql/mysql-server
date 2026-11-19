/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "storage/duckdb/ha_duckdb.h"

#include <mysql/components/services/log_builtins.h>
#include <algorithm>  // std::transform
#include <cctype>     // std::tolower
#include "ddl_convertor.h"
#include "delta_appender.h"
#include "dml_convertor.h"
#include "my_dbug.h"
#include "mysql/plugin.h"
#include "row_mysql.h"
#include "sql/dd/cache/dictionary_client.h"
#include "sql/dd/dictionary.h"
#include "sql/dd/types/schema.h"
#include "sql/dd/types/table.h"
#include "sql/duckdb/duckdb_context.h"
#include "sql/duckdb/duckdb_query.h"

#include "duckdb_select.h"
#include "my_time.h"
#include "scope_guard.h"
#include "sql/create_field.h"
#include "sql/dd/dd_table.h"
#include "sql/dd/types/tablespace.h"
#include "sql/debug_sync.h"
#include "sql/duckdb/duckdb_config.h"
#include "sql/duckdb/duckdb_sql_quote.h"
#include "sql/duckdb/duckdb_table.h"
#include "sql/rpl_rli.h"
#include "sql/sql_class.h"
#include "sql/sql_lex.h"
#include "sql/sql_partition.h"
#include "sql/sql_plugin.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"
#include "sql/sql_time.h"
#include "sql/tztime.h"
#include "typelib.h"

bool copy_ddl_in_batch = true;
bool dml_in_batch = true;
bool update_modified_column_only = true;

struct duckdb_status_t {
  ulonglong duckdb_rows_insert;
  ulonglong duckdb_rows_update;
  ulonglong duckdb_rows_delete;

  ulonglong duckdb_rows_insert_in_batch;
  ulonglong duckdb_rows_update_in_batch;
  ulonglong duckdb_rows_delete_in_batch;

  ulonglong duckdb_commit;
  ulonglong duckdb_rollback;
};

duckdb_status_t srv_duckdb_status;

static handler *duckdb_create_handler(handlerton *hton, TABLE_SHARE *table,
                                      bool partitioned, MEM_ROOT *mem_root);

handlerton *duckdb_hton;

/* Interface to mysqld, to check system tables supported by SE */
static bool duckdb_is_supported_system_table(const char *db,
                                             const char *table_name,
                                             bool is_sql_layer_system_table);

/** Get the tablespace type given the name.
@param[in]  tablespace_name tablespace name
@param[out] space_type      type of space
@return Operation status.
*/
static bool duckdb_get_tablespace_type_by_name(const char *tablespace_name,
                                               Tablespace_type *space_type);

Duckdb_share::Duckdb_share() {
  thr_lock_init(&lock);
  m_auto_inc_val = 1;
}

static int duckdb_prepare(handlerton *, THD *const thd, bool commit_trx) {
  if (commit_trx ||
      (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {
    std::string errmsg;

    DBUG_EXECUTE_IF("duckdb_prepare_skip_flush", return 0;);
    auto ret = thd->get_duckdb_context()->flush_appenders(errmsg);
    if (ret) {
      my_error(ER_DUCKDB_PREPARE_ERROR, MYF(0), errmsg.c_str());
      return 1;
    }
  }
  return 0;
}

static int duckdb_set_prepared_in_tc(handlerton *, THD *) {
  DBUG_TRACE;
  return 0;
}

static int duckdb_commit(handlerton *, THD *const thd, bool commit_trx) {
  if (commit_trx ||
      (!thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) {
    DBUG_EXECUTE_IF("crash_before_duckdb_commit", DBUG_SUICIDE(););
    DBUG_EXECUTE_IF("duckdb_inject_commit_error", {
      my_error(ER_DUCKDB_COMMIT_ERROR, MYF(0),
               "injected commit error for testing");
      std::string err_msg;
      thd->get_duckdb_context()->duckdb_trans_rollback(err_msg);
      return 1;
    });

    srv_duckdb_status.duckdb_commit++;

    std::string error_msg;
    if (thd->get_duckdb_context()->duckdb_trans_commit(error_msg)) {
      if (thd->get_rds_context().is_copy_ddl_from_innodb_to_duckdb()) {
        DBUG_SUICIDE();
        my_abort();
      }

      my_error(ER_DUCKDB_COMMIT_ERROR, MYF(0), error_msg.c_str());
      thd->get_duckdb_context()->duckdb_trans_rollback(error_msg);
      return ER_DUCKDB_COMMIT_ERROR;
    }

    DBUG_EXECUTE_IF("crash_after_duckdb_commit", DBUG_SUICIDE(););
  }
  return 0;
}

static int duckdb_rollback(handlerton *, THD *thd, bool rollback_trx) {
  if (!rollback_trx &&
      thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    /*
      DuckDB has no statement undo. Ask the server to roll back the entire
      transaction after statement cleanup, including other engines and binlog
      caches, rather than committing a failed statement's partial writes.
    */
    thd_mark_transaction_to_rollback(thd, true);
    return 0;
  }

  if (rollback_trx ||
      !thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    srv_duckdb_status.duckdb_rollback++;

    /** ha_commit_low invoke reset_scope(trx_scope) no matter if commit succeed
    or failed. So we need to rollback transaction here. */
    std::string error_msg;
    if (thd->get_duckdb_context()->duckdb_trans_rollback(error_msg)) {
      my_error(ER_DUCKDB_ROLLBACK_ERROR, MYF(0), error_msg.c_str());
      return ER_DUCKDB_ROLLBACK_ERROR;
    }
  }
  return 0;
}

static int duckdb_close_connection(handlerton *, THD *) { return 0; }

static int duckdb_register_trx(THD *thd) {
  if (thd->get_transaction()->xid_state()->check_in_xa(true)) {
    return HA_DUCKDB_REGISTER_TRX_ERROR;
  }

  /* When log_bin=on but log_replica_updates=off, we need to set_no_2pc
     explicitly */
  bool need_set_no_2pc =
      thd->slave_thread && opt_bin_log && !opt_log_replica_updates;
  trans_register_ha(thd, false, duckdb_hton, nullptr);
  if (need_set_no_2pc) {
    thd->get_transaction()->set_no_2pc(Transaction_ctx::STMT, true);
  }

  if (thd_test_options(thd, OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN)) {
    trans_register_ha(thd, true, duckdb_hton, nullptr);
    if (need_set_no_2pc) {
      thd->get_transaction()->set_no_2pc(Transaction_ctx::SESSION, true);
    }
  }

  /*
    It should start an explicit transaction even MySQL is in AUTOCOMMIT mode.
    whether a duckdb transaction should commit or rollback is controlled by
    MySQL. DuckDB should not commit a transaction implicitly.
  */
  if (!thd->get_duckdb_context()->has_transaction())
    thd->get_duckdb_context()->duckdb_trans_begin();
  return 0;
}

static void duckdb_drop_database(handlerton *, const char *path) {
  THD *thd = current_thd;
  DBUG_TRACE;

  if (myduck::global_mode != myduck::enum_modes::DUCKDB_ON) {
    return;
  }

  DBUG_PRINT("enter", ("path: '%s'", path));
  const std::string db(path);

  /* Drop schema if exist */
  std::string query = "DROP SCHEMA IF EXISTS ";
  query.append(myduck::quote_duckdb_ident(db));

  duckdb_register_trx(thd);
  [[maybe_unused]] auto query_result = myduck::duckdb_query(thd, query, false);
  assert(!query_result->HasError());
}

static void duckdb_create_database(handlerton *, char *db) {
  THD *thd = current_thd;
  DBUG_TRACE;

  if (myduck::global_mode != myduck::enum_modes::DUCKDB_ON) {
    return;
  }

  DBUG_PRINT("enter", ("db: '%s'", db));
  std::string query = "CREATE SCHEMA IF NOT EXISTS ";
  query.append(myduck::quote_duckdb_ident(db));

  duckdb_register_trx(thd);
  [[maybe_unused]] auto query_result = myduck::duckdb_query(thd, query, false);
  assert(!query_result->HasError());
}

/** Return partitioning flags. */
static uint duckdb_partition_flags() {
  return (HA_CANNOT_PARTITION_FK | HA_TRUNCATE_PARTITION_PRECLOSE);
}

static int duckdb_init_func(void *p) {
  DBUG_TRACE;
  duckdb_hton = (handlerton *)p;
  duckdb_hton->db_type = DB_TYPE_DUCKDB;
  duckdb_hton->state = SHOW_OPTION_YES;
  duckdb_hton->create = duckdb_create_handler;
  /** Compared with innodb, duckdb doesn't support:
   * foreign keys,
   * recycle bin
   * partition table
   */
  duckdb_hton->flags = HTON_SUPPORTS_ATOMIC_DDL | HTON_TEMPORARY_NOT_SUPPORTED |
                       HTON_SUPPORTS_TABLE_ENCRYPTION;

  duckdb_hton->is_supported_system_table = duckdb_is_supported_system_table;
  /*
    when binlog is enabled, it feaks duckdb support 2pc.
    Since the binlog crashsafe mechanism for duckdb requires 2pc.
  */
  if (opt_bin_log) duckdb_hton->prepare = duckdb_prepare;
  if (opt_bin_log) duckdb_hton->set_prepared_in_tc = duckdb_set_prepared_in_tc;
  duckdb_hton->commit = duckdb_commit;
  duckdb_hton->rollback = duckdb_rollback;
  duckdb_hton->close_connection = duckdb_close_connection;
  duckdb_hton->drop_database = duckdb_drop_database;
  duckdb_hton->create_database = duckdb_create_database;
  /* DuckDB has no savepoints; let the server reject SAVEPOINT explicitly. */
  duckdb_hton->get_tablespace_type_by_name = duckdb_get_tablespace_type_by_name;
  duckdb_hton->partition_flags = duckdb_partition_flags;

  return 0;
}

static MYSQL_SYSVAR_BOOL(copy_ddl_in_batch, copy_ddl_in_batch,
                         PLUGIN_VAR_RQCMDARG,
                         "Use batch insert to speed up copy ddl", NULL, NULL,
                         true);

static MYSQL_SYSVAR_BOOL(dml_in_batch, dml_in_batch, PLUGIN_VAR_RQCMDARG,
                         "Use batch to speed up INSERT/UPDATE/DELETE", NULL,
                         NULL, true);

static MYSQL_SYSVAR_ULONGLONG(
    batch_max_row_count, batch_max_row_count, PLUGIN_VAR_RQCMDARG,
    "To avoid out-of-memory errors, DuckDB needs to split large batch, which "
    "limits the number of rows in a batch; 0 means no limit.",
    NULL, NULL, 0, 0, ULLONG_MAX, 0);

static MYSQL_SYSVAR_BOOL(
    update_modified_column_only, update_modified_column_only,
    PLUGIN_VAR_RQCMDARG,
    "Whether to only update modified columns when replay Binlog", NULL, NULL,
    true);

/**
  @brief
  Example of simple lock controls. The "share" it creates is a
  structure we will pass to each example handler. Do you have to have
  one of these? Well, you have pieces that are used for locking, and
  they are needed to function.
*/

Duckdb_share *ha_duckdb::get_share() {
  Duckdb_share *tmp_share;

  DBUG_TRACE;

  lock_shared_ha_data();
  if (!(tmp_share = static_cast<Duckdb_share *>(get_ha_share_ptr()))) {
    tmp_share = new Duckdb_share();
    if (!tmp_share) goto err;

    set_ha_share_ptr(static_cast<Handler_share *>(tmp_share));
    if (table->found_next_number_field != nullptr) {
      ulonglong max_val_in_duckdb = get_auto_inc_from_duckdb();
      /*
        In this time, ha_duckdb->share has not been set yet. So we
        use tmp_share directly, which we be set to share in ha_duckdb later.
      */
      update_auto_inc_val(max_val_in_duckdb, tmp_share);
    }
  }
err:
  unlock_shared_ha_data();
  return tmp_share;
}

static handler *duckdb_create_handler(handlerton *hton, TABLE_SHARE *table,
                                      bool partitioned, MEM_ROOT *mem_root) {
  return new (mem_root) ha_duckdb(hton, table, partitioned);
}

ha_duckdb::ha_duckdb(handlerton *hton, TABLE_SHARE *table_arg, bool partitioned)
    : handler(hton, table_arg), share(nullptr), m_partitioned(partitioned) {
  bitmap_init(&m_blob_map, nullptr, MAX_FIELDS);
}

ha_duckdb::~ha_duckdb() { bitmap_free(&m_blob_map); }

enum row_type ha_duckdb::get_real_row_type(
    const HA_CREATE_INFO *create_info) const {
  /*
    row_format is meaningless for the duckdb engine. All DuckDB tables are
    currently converted from innodb, so we use `get_real_row_type` of ha_innodb
    directly. This ensures the row_format consistency of InnoDB and DuckDB.
  */
  THD *thd = ha_thd();
  handlerton *hton = ha_resolve_by_legacy_type(thd, DB_TYPE_INNODB);
  handler *file =
      get_new_handler((TABLE_SHARE *)nullptr, false, thd->mem_root, hton);
  row_type type = file->get_real_row_type(create_info);
  ::destroy_at(file);
  return type;
}
/*
  List of all system tables specific to the SE.
  Array element would look like below,
     { "<database_name>", "<system table name>" },
  The last element MUST be,
     { (const char*)NULL, (const char*)NULL }

  This array is optional, so every SE need not implement it.
*/
static st_handler_tablename ha_duckdb_system_tables[] = {
    {(const char *)nullptr, (const char *)nullptr}};

/**
  @brief Check if the given db.tablename is a system table for this SE.

  @param db                         Database name to check.
  @param table_name                 table name to check.
  @param is_sql_layer_system_table  if the supplied db.table_name is a SQL
                                    layer system table.

  @retval true   Given db.table_name is supported system table.
  @retval false  Given db.table_name is not a supported system table.
*/
static bool duckdb_is_supported_system_table(const char *db,
                                             const char *table_name,
                                             bool is_sql_layer_system_table) {
  st_handler_tablename *systab;

  // Does this SE support "ALL" SQL layer system tables ?
  if (is_sql_layer_system_table) return false;

  // Check if this is SE layer system tables
  systab = ha_duckdb_system_tables;
  while (systab && systab->db) {
    if (systab->db == db && strcmp(systab->tablename, table_name) == 0)
      return true;
    systab++;
  }

  return false;
}

/**
  @brief
  Used for opening tables. The name will be the name of the file.

  @details
  A table is opened when it needs to be opened; e.g. when a request comes in
  for a SELECT on the table (tables are not open and closed for each request,
  they are cached).

  Called from handler.cc by handler::ha_open(). The server opens all tables by
  calling ha_open() which then calls the handler specific open().

  @see
  handler::ha_open() in handler.cc
*/

int ha_duckdb::open(const char *, int, uint, const dd::Table *) {
  DBUG_TRACE;

  if (!(share = get_share())) return 1;
  thr_lock_data_init(&share->lock, &lock, nullptr);

  lock_shared_ha_data();
  if (table->part_info != nullptr) {
    m_part_info = table->part_info;
    share->populate_partition_name_hash(m_part_info);
  }
  unlock_shared_ha_data();

  return 0;
}

/**
  @brief
  Closes a table.

  @details
  Called from sql_base.cc, sql_select.cc, and table.cc. In sql_select.cc it is
  only used to close up temporary tables or during the process where a
  temporary table is converted over to being a myisam table.

  For sql_base.cc look at close_data_tables().

  @see
  sql_base.cc, sql_select.cc and table.cc
*/

int ha_duckdb::close(void) {
  DBUG_TRACE;
  return 0;
}

static int execute_dml(THD *thd, DMLConvertor *convertor) {
  if (convertor->check()) {
    return HA_DUCKDB_DML_ERROR;
  }

  auto query = convertor->translate();

  DBUG_PRINT("duckdb_print_dml", ("%s", query.c_str()));

  auto query_result = myduck::duckdb_query(thd, query);

  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return HA_DUCKDB_DML_ERROR;
  }

  return 0;
}

// check whether field is modified
static bool calc_field_difference(const uchar *old_row, uchar *new_row,
                                  TABLE *table, Field *field) {
  ulong o_len;
  ulong n_len;
  const uchar *o_ptr;
  const uchar *n_ptr;

  o_ptr = (const uchar *)old_row + field->offset(table->record[0]);
  n_ptr = (const uchar *)new_row + field->offset(table->record[0]);

  o_len = n_len = field->pack_length();

  switch (field->type()) {
    case MYSQL_TYPE_VARCHAR:
      /* This is a >= 5.0.3 type true VARCHAR where the real payload data
         length is stored in 1 or 2 bytes */
      o_ptr = row_mysql_read_true_varchar(&o_len, o_ptr,
                                          (ulong)field->get_length_bytes());
      n_ptr = row_mysql_read_true_varchar(&n_len, n_ptr,
                                          (ulong)field->get_length_bytes());
      break;
    case MYSQL_TYPE_GEOMETRY:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    // TODO: deal json as varchar
    case MYSQL_TYPE_JSON:
      o_ptr = row_mysql_read_blob_ref(&o_len, o_ptr, o_len);
      n_ptr = row_mysql_read_blob_ref(&n_len, n_ptr, n_len);
      break;
    default:;
  }

  if (field->is_nullable()) {
    if (field->is_null_in_record(old_row)) {
      o_len = ~0U;
    }

    if (field->is_null_in_record(new_row)) {
      n_len = ~0U;
    }
  }

  return o_len != n_len ||
         (o_len != ~0U && o_len != 0 && 0 != memcmp(o_ptr, n_ptr, o_len));
}

// calculate row difference, set bit for modified columns in table->tmp_set
static bool calc_row_difference(const uchar *old_row, uchar *new_row,
                                TABLE *table) {
  bool res = false;

  bitmap_clear_all(&table->tmp_set);

  for (uint i = 0; i < table->s->fields; i++) {
    Field *field = table->field[i];

    if (calc_field_difference(old_row, new_row, table, field)) {
      bitmap_set_bit(&table->tmp_set, field->field_index());
      res = true;
    }
  }

  return res;
}

// check whether PK is modified
static bool calc_pk_difference(const uchar *old_row, uchar *new_row,
                               TABLE *table) {
  KEY *key_info = table->key_info;
  if (!key_info) return false;

  KEY_PART_INFO *key_part = table->key_info->key_part;
  for (uint j = 0; j < key_info->user_defined_key_parts; j++, key_part++) {
    if (calc_field_difference(old_row, new_row, table, key_part->field))
      return true;
  }
  return false;
}

static myduck::BatchState get_batch_state(THD *thd, bool idempotent_flag,
                                          bool insert_only) {
  myduck::DuckdbThdContext *duckdb_context = thd->get_duckdb_context();
  myduck::BatchState batch_state = duckdb_context->get_batch_state();

  if (batch_state == myduck::BatchState::UNDEFINED) {
    bool batch = false;
    if (duckdb_context->is_in_copy_ddl()) {
      /*
        If copy ddl is from DuckDB to DuckDB, we need to use batch mode to
        avoid interrupt the fetch query.
      */
      batch = copy_ddl_in_batch ||
              thd->get_rds_context().is_copy_ddl_from_duckdb_to_duckdb();
      insert_only = true;
    } else if (dml_in_batch) {
      batch = true;
      if (idempotent_flag) insert_only = false;
    }

    if (!batch) {
      batch_state = myduck::BatchState::NOT_IN_BATCH;
    } else if (insert_only) {
      batch_state = myduck::BatchState::IN_INSERT_ONLY_BATCH;
    } else {
      batch_state = myduck::BatchState::IN_MIX_BATCH;
    }

    duckdb_context->set_batch_state(batch_state);
  }

  return batch_state;
}

// Build duckdb type map of blob type.
static void build_duckdb_blob_map(Field **field_list, MY_BITMAP *map) {
  for (Field **f_ptr = field_list; *f_ptr != nullptr; f_ptr++) {
    Field *field = *f_ptr;
    enum_field_types type = field->real_type();

    if (type == MYSQL_TYPE_SET || type == MYSQL_TYPE_ENUM ||
        type == MYSQL_TYPE_BIT || type == MYSQL_TYPE_GEOMETRY ||
        type == MYSQL_TYPE_VARCHAR || type == MYSQL_TYPE_STRING ||
        type == MYSQL_TYPE_JSON || type == MYSQL_TYPE_TINY_BLOB ||
        type == MYSQL_TYPE_BLOB || type == MYSQL_TYPE_MEDIUM_BLOB ||
        type == MYSQL_TYPE_LONG_BLOB) {
      if (FieldConvertor::convert_type(field) == "BLOB" ||
          FieldConvertor::convert_type(field) == "BITSTRING") {
        bitmap_set_bit(map, field->field_index());
      }
    }
  }
}

static bool specify_partition(THD *thd) {
  if (!thd->lex || !thd->lex->query_block) return false;

  Table_ref *table_list = thd->lex->query_block->get_table_list();
  if (table_list && table_list->partition_names &&
      table_list->partition_names->size() > 0) {
    return true;
  }

  return false;
}

/**
  @brief
  write_row() inserts a row. No extra() hint is given currently if a bulk load
  is happening. buf() is a byte array of data. You can use the field
  information to extract the data from the native byte array type.
*/

int ha_duckdb::write_row(uchar *buf) {
  DBUG_TRACE;
  int ret = 0;
  THD *thd = ha_thd();

  if (specify_partition(thd)) {
    my_error(ER_DUCKDB_CLIENT, MYF(0),
             "Specifying partitions is not supported");
    return HA_DUCKDB_SPECIFY_PARTITION_ERROR;
  }

  Relay_log_info *rli = thd->rli_slave;

  assert(table_share != nullptr && table != nullptr);
  my_bitmap_map *org_bitmap = dbug_tmp_use_all_columns(table, table->read_set);
  auto guard = create_scope_guard(
      [&]() { dbug_tmp_restore_column_map(table->read_set, org_bitmap); });

  ret = duckdb_register_trx(thd);
  if (ret) return ret;

  if (table->next_number_field && buf == table->record[0]) {
    int error;
    if ((error = update_auto_increment())) return error;
  }

  bool idempotent_flag = rli ? rli->get_local_duckdb_idempotent_flag() : false;
  bool insert_only_flag = rli ? rli->get_duckdb_insert_only_flag() : true;

  if (rli == nullptr && thd->variables.duckdb_data_import_mode) {
    insert_only_flag = false;
  }

  myduck::BatchState batch_state =
      get_batch_state(thd, idempotent_flag, insert_only_flag);
  assert(batch_state != myduck::BatchState::UNDEFINED);

  if (batch_state == myduck::BatchState::NOT_IN_BATCH) {
    if (idempotent_flag) {
      DeleteConvertor delete_convertor(table);
      ret = execute_dml(thd, &delete_convertor);
      if (ret) return ret;
    }
    // insert by execute insert into sql
    InsertConvertor convertor(table, idempotent_flag);
    ret = execute_dml(thd, &convertor);
    if (ret == 0) srv_duckdb_status.duckdb_rows_insert++;
  } else {
    if (m_first_write) {
      build_duckdb_blob_map(table->field, &m_blob_map);
      m_first_write = false;
    }
    ret = thd->get_duckdb_context()->append_row_insert(table, &m_blob_map);
    if (ret == 0) srv_duckdb_status.duckdb_rows_insert_in_batch++;
  }

  if (table->next_number_field) {
    my_bitmap_map *const old_map =
        dbug_tmp_use_all_columns(table, table->read_set);
    ulonglong update_val = table->next_number_field->val_int();
    dbug_tmp_restore_column_map(table->read_set, old_map);
    update_auto_inc_val(update_val);
  }

  return ret;
}

// refers to ha_federated::update_row
int ha_duckdb::update_row(const uchar *old_row, uchar *new_row) {
  DBUG_TRACE;
  int ret = 0;
  THD *thd = ha_thd();

  if (specify_partition(thd)) {
    my_error(ER_DUCKDB_CLIENT, MYF(0),
             "Specifying partitions is not supported");
    return HA_DUCKDB_SPECIFY_PARTITION_ERROR;
  }

  Relay_log_info *rli = thd->rli_slave;
  assert(!rli || !rli->get_duckdb_insert_only_flag());

  ret = duckdb_register_trx(thd);
  if (ret) return ret;

  bool idempotent_flag = rli ? rli->get_local_duckdb_idempotent_flag() : false;
  myduck::BatchState batch_state = get_batch_state(thd, idempotent_flag, false);
  assert(batch_state == myduck::BatchState::NOT_IN_BATCH ||
         batch_state == myduck::BatchState::IN_MIX_BATCH);

  bool pk_updated = calc_pk_difference(old_row, new_row, table);
  if (pk_updated && table->next_number_field) {
    my_bitmap_map *const old_map =
        dbug_tmp_use_all_columns(table, table->read_set);
    ulonglong update_val = table->next_number_field->val_int();
    dbug_tmp_restore_column_map(table->read_set, old_map);
    update_auto_inc_val(update_val);
  }

  if (batch_state == myduck::BatchState::NOT_IN_BATCH) {
    if (idempotent_flag && pk_updated) {
      if (!bitmap_is_set_all(table->write_set)) {
        LogErr(WARNING_LEVEL, ER_DUCKDB,
               "'binlog_row_image' is not set to 'FULL', idempotent replay is "
               "not possible!");
        return HA_DUCKDB_DML_ERROR;
      }

      /* When dempotent replay, replace the "Update" that modifies the PK with
         "Delete + Insert" */
      DeleteConvertor delete_convertor_old(table, old_row);
      ret = execute_dml(thd, &delete_convertor_old);
      if (ret) return ret;

      DeleteConvertor delete_convertor_new(table);
      ret = execute_dml(thd, &delete_convertor_new);
      if (ret) return ret;

      InsertConvertor insert_convertor(table, true);
      ret = execute_dml(thd, &insert_convertor);
    } else {
      if (update_modified_column_only &&
          calc_row_difference(old_row, new_row, table)) {
        /* Copy the tmp_set calculated in 'calc_row_difference' to write_set */
        bitmap_copy(table->write_set, &table->tmp_set);
      }
      bitmap_clear_all(&table->tmp_set);

      UpdateConvertor update_convertor(table, old_row);
      ret = execute_dml(thd, &update_convertor);
    }

    if (ret == 0) srv_duckdb_status.duckdb_rows_update++;
  } else {
    ret = thd->get_duckdb_context()->append_row_update(table, old_row);
    if (ret == 0) srv_duckdb_status.duckdb_rows_update_in_batch++;
  }

  return ret;
}

/**
  @brief
  This will delete a row. buf will contain a copy of the row to be deleted.
  The server will call this right after the current row has been called (from
  either a previous rnd_nexT() or index call).

  @details
  If you keep a pointer to the last row or can access a primary key it will
  make doing the deletion quite a bit easier. Keep in mind that the server does
  not guarantee consecutive deletions. ORDER BY clauses can be used.

  Called in sql_acl.cc and sql_udf.cc to manage internal table
  information.  Called in sql_delete.cc, sql_insert.cc, and
  sql_select.cc. In sql_select it is used for removing duplicates
  while in insert it is used for REPLACE calls.

  @see
  sql_acl.cc, sql_udf.cc, sql_delete.cc, sql_insert.cc and sql_select.cc
*/

int ha_duckdb::delete_row(const uchar *) {
  DBUG_TRACE;
  int ret = 0;
  THD *thd = ha_thd();

  if (specify_partition(thd)) {
    my_error(ER_DUCKDB_CLIENT, MYF(0),
             "Specifying partitions is not supported");
    return HA_DUCKDB_SPECIFY_PARTITION_ERROR;
  }

  Relay_log_info *rli = thd->rli_slave;
  assert(!rli || !rli->get_duckdb_insert_only_flag());

  ret = duckdb_register_trx(thd);
  if (ret) return ret;

  bool idempotent_flag = rli ? rli->get_local_duckdb_idempotent_flag() : false;
  myduck::BatchState batch_state = get_batch_state(thd, idempotent_flag, false);
  assert(batch_state == myduck::BatchState::NOT_IN_BATCH ||
         batch_state == myduck::BatchState::IN_MIX_BATCH);

  if (batch_state == myduck::BatchState::NOT_IN_BATCH) {
    DeleteConvertor convertor(table);
    ret = execute_dml(thd, &convertor);
    if (ret == 0) srv_duckdb_status.duckdb_rows_delete++;
  } else {
    ret = thd->get_duckdb_context()->append_row_delete(table);
    if (ret == 0) srv_duckdb_status.duckdb_rows_delete_in_batch++;
  }

  return ret;
}

/**
  @brief
  Positions an index cursor to the index specified in the handle. Fetches the
  row if available. If the key value is null, begin at the first key of the
  index.
*/

int ha_duckdb::index_read_map(uchar *, const uchar *, key_part_map,
                              enum ha_rkey_function) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  Used to read forward through the index.
*/

int ha_duckdb::index_next(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  Used to read backwards through the index.
*/

int ha_duckdb::index_prev(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  index_first() asks for the first key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_duckdb::index_first(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  index_last() asks for the last key in the index.

  @details
  Called from opt_range.cc, opt_sum.cc, sql_handler.cc, and sql_select.cc.

  @see
  opt_range.cc, opt_sum.cc, sql_handler.cc and sql_select.cc
*/
int ha_duckdb::index_last(uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  rnd_init() is called when the system wants the storage engine to do a table
  scan. See the example in the introduction at the top of this file to see when
  rnd_init() is called.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_duckdb::rnd_init(bool) {
  THD *thd = ha_thd();
  std::string schema_name;
  std::string table_name;
  /* 1. get db name and table name */
  if (table && table->s) {
    schema_name.assign(table->s->db.str, table->s->db.length);
    table_name.assign(table->s->table_name.str, table->s->table_name.length);
  } else {
    return HA_ERR_INTERNAL_ERROR;
  }

  /* 2. construct the query string and execute the query */
  std::string query = "SELECT * FROM " +
                      myduck::quote_duckdb_ident(schema_name) + "." +
                      myduck::quote_duckdb_ident(table_name);
  query_result = myduck::duckdb_query(thd, query, true);
  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return HA_ERR_INTERNAL_ERROR;
  }

  return 0;
}

int ha_duckdb::rnd_end() {
  DBUG_TRACE;
  query_result.reset();
  current_chunk.reset();
  return 0;
}

/**
  @brief
  This is called for each row of the table scan. When you run out of records
  you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
  The Field structure for the table is the key to getting data into buf
  in a manner that will allow the server to understand it.

  @details
  Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc,
  sql_table.cc, and sql_update.cc.

  @see
  filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc and
  sql_update.cc
*/
int ha_duckdb::rnd_next(uchar *buf) {
  DBUG_TRACE;
  THD *thd = ha_thd();

  if (!query_result) {
    return HA_ERR_INTERNAL_ERROR;
  }

  /* clean the old record */
  memset(buf, 0, table->s->reclength);

  /* fetch new chunk when current chunk is empty */
  if (!current_chunk || current_row_index >= current_chunk->size()) {
    current_chunk.reset();
    current_chunk = query_result->Fetch();

    /* return when data is exhausted */
    if (!current_chunk) {
      return HA_ERR_END_OF_FILE;
    }
    current_row_index = 0;
  }
  /* store the fields of a tuple */
  for (size_t col_idx = 0; col_idx < current_chunk->ColumnCount(); ++col_idx) {
    duckdb::Value value = current_chunk->GetValue(col_idx, current_row_index);
    Field *field = table->field[col_idx];
    store_duckdb_field_in_mysql_format(field, value, thd);
  }

  /* update NULL field tag */
  if (table->s->null_bytes > 0) {
    if (table->null_flags) {
      memcpy(buf, table->null_flags, table->s->null_bytes);
    } else {
      memset(buf, 0, table->s->null_bytes);
    }
  }

  /* update row_index */
  current_row_index++;

  /* successfully get one row */
  return 0;
}

/**
  @brief
  position() is called after each call to rnd_next() if the data needs
  to be ordered. You can do something like the following to store
  the position:
  @code
  my_store_ptr(ref, ref_length, current_position);
  @endcode

  @details
  The server uses ref to store data. ref_length in the above case is
  the size needed to store current_position. ref is just a byte array
  that the server will maintain. If you are using offsets to mark rows, then
  current_position should be the offset. If it is a primary key like in
  BDB, then it needs to be a primary key.

  Called from filesort.cc, sql_select.cc, sql_delete.cc, and sql_update.cc.

  @see
  filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc
*/
void ha_duckdb::position(const uchar *) { DBUG_TRACE; }

/**
  @brief
  This is like rnd_next, but you are given a position to use
  to determine the row. The position will be of the type that you stored in
  ref. You can use ha_get_ptr(pos,ref_length) to retrieve whatever key
  or position you saved when position() was called.

  @details
  Called from filesort.cc, records.cc, sql_insert.cc, sql_select.cc, and
  sql_update.cc.

  @see
  filesort.cc, records.cc, sql_insert.cc, sql_select.cc and sql_update.cc
*/
int ha_duckdb::rnd_pos(uchar *, uchar *) {
  int rc;
  DBUG_TRACE;
  rc = HA_ERR_WRONG_COMMAND;
  return rc;
}

/**
  @brief
  ::info() is used to return information to the optimizer. See my_base.h for
  the complete description.

  @details
  Currently this table handler doesn't implement most of the fields really
  needed. SHOW also makes use of this data.

  You will probably want to have the following in your code:
  @code
  if (records < 2)
    records = 2;
  @endcode
  The reason is that the server will optimize for cases of only a single
  record. If, in a table scan, you don't know the number of records, it
  will probably be better to set records to two so you can return as many
  records as you need. Along with records, a few more variables you may wish
  to set are:
    records
    deleted
    data_file_length
    index_file_length
    delete_length
    check_time
  Take a look at the public variables in handler.h for more information.

  Called in filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc, and sql_update.cc.

  @see
  filesort.cc, ha_heap.cc, item_sum.cc, opt_sum.cc, sql_delete.cc,
  sql_delete.cc, sql_derived.cc, sql_select.cc, sql_select.cc, sql_select.cc,
  sql_select.cc, sql_select.cc, sql_show.cc, sql_show.cc, sql_show.cc,
  sql_show.cc, sql_table.cc, sql_union.cc and sql_update.cc
*/
int ha_duckdb::info(uint flag) {
  DBUG_TRACE;

  if (flag & HA_STATUS_AUTO) {
    ulonglong new_val = get_auto_inc_from_duckdb();
    update_auto_inc_val(new_val);
    stats.auto_increment_value = share->m_auto_inc_val;
  }
  return 0;
}

/**
  @brief
  extra() is called whenever the server wishes to send a hint to
  the storage engine. The myisam engine implements the most hints.
  ha_innodb.cc has the most exhaustive list of these hints.

    @see
  ha_innodb.cc
*/
int ha_duckdb::extra(enum ha_extra_function operation) {
  DBUG_TRACE;
  THD *thd = ha_thd();

  switch (operation) {
    case HA_EXTRA_BEGIN_ALTER_COPY:
      thd->get_duckdb_context()->set_in_copy_ddl(true);
      break;
    case HA_EXTRA_END_ALTER_COPY:
      thd->get_duckdb_context()->set_in_copy_ddl(false);
      break;
    default:; /* Do nothing */
  }
  return 0;
}

/**
  @brief
  Used to delete all rows in a table, including cases of truncate and cases
  where the optimizer realizes that all rows will be removed as a result of an
  SQL statement.

  @details
  Called from item_sum.cc by Item_func_group_concat::clear(),
  Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
  Called from sql_delete.cc by mysql_delete().
  Called from sql_select.cc by JOIN::reinit().
  Called from sql_union.cc by st_query_block_query_expression::exec().

  @see
  Item_func_group_concat::clear(), Item_sum_count_distinct::clear() and
  Item_func_group_concat::clear() in item_sum.cc;
  mysql_delete() in sql_delete.cc;
  JOIN::reinit() in sql_select.cc and
  st_query_block_query_expression::exec() in sql_union.cc.
*/
int ha_duckdb::delete_all_rows() {
  DBUG_TRACE;
  return HA_ERR_WRONG_COMMAND;
}

/**
  @brief
  This create a lock on the table. If you are implementing a storage engine
  that can handle transacations look at ha_berkely.cc to see how you will
  want to go about doing this. Otherwise you should consider calling flock()
  here. Hint: Read the section "locking functions for mysql" in lock.cc to
  understand this.

  @details
  Called from lock.cc by lock_external() and unlock_external(). Also called
  from sql_table.cc by copy_data_between_tables().

  @see
  lock.cc by lock_external() and unlock_external() in lock.cc;
  the section "locking functions for mysql" in lock.cc;
  copy_data_between_tables() in sql_table.cc.
*/
int ha_duckdb::external_lock(THD *thd, int lock_type) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("lock_type: %d", lock_type));
  if (lock_type != F_UNLCK) {
    int ret = duckdb_register_trx(thd);
    if (ret) return ret;
  }
  return 0;
}

/** Returns number of THR_LOCK locks used for one instance of DuckDB table.
 DuckDB no longer relies on THR_LOCK locks so 0 value is returned.
 Instead of THR_LOCK locks DuckDB relies on combination of metadata locks
 (e.g. for LOCK TABLES and DDL) and its own locking subsystem.
 Note that even though this method returns 0, SQL-layer still calls
 "::store_lock()", "::start_stmt()" and "::external_lock()" methods for DuckDB
 tables. */
uint ha_duckdb::lock_count(void) const { return 0; }

/** Supposed to convert a MySQL table lock stored in the 'lock' field of the
 handle to a proper type before storing pointer to the lock into an array
 of pointers.
 In practice, since DuckDB no longer relies on THR_LOCK locks and its
 lock_count() method returns 0 it just informs storage engine about type
 of THR_LOCK which SQL-layer would have acquired for this specific statement
 on this specific table.
 MySQL also calls this if it wants to reset some table locks to a not-locked
 state during the processing of an SQL query. An example is that during a
 SELECT the read lock is released early on the 'const' tables where we only
 fetch one row. MySQL does not call this when it releases all locks at the
 end of an SQL statement.
 @return pointer to the current element in the 'to' array. */
THR_LOCK_DATA **ha_duckdb::store_lock(THD *, THR_LOCK_DATA **to,
                                      enum thr_lock_type) {
  return to;
}

/**
  @brief
  Used to delete a table. By the time delete_table() has been called all
  opened references to this table will have been closed (and your globally
  shared references released). The variable name will just be the name of
  the table. You will need to remove any files you have created at this point.

  @details
  If you do not implement this, the default delete_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from handler.cc by delete_table and ha_create_table(). Only used
  during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
  the storage engine.

  @see
  delete_table and ha_create_table() in handler.cc
*/
int ha_duckdb::delete_table(const char *, const dd::Table *dd_table) {
  DBUG_TRACE;

  dd::Schema *dd_schema = nullptr;

  THD *thd = ha_thd();

  int ret = duckdb_register_trx(thd);
  if (ret) return ret;

  dd::cache::Dictionary_client *client = dd::get_dd_client(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(client);

  if (client->acquire_uncached(dd_table->schema_id(), &dd_schema)) {
    return true;
  }

  std::string schema_name(dd_schema->name());
  std::string table_name(dd_table->name());

  std::ostringstream query;
  query << "USE " << myduck::quote_duckdb_ident(schema_name) << ";";
  query << "DROP TABLE IF EXISTS " << myduck::quote_duckdb_ident(table_name)
        << ";";
  auto query_result = myduck::duckdb_query(ha_thd(), query.str());

  if (query_result == nullptr) {
    return HA_DUCKDB_DROP_TABLE_ERROR;
  }

  thd->get_duckdb_context()->delete_appender(schema_name, table_name);

  if (thd->get_rds_context().is_copy_ddl_from_innodb_to_duckdb() &&
      thd->variables.duckdb_copy_ddl_threads > 1 &&
      strncmp(dd_table->name().c_str(), "#sql-", strlen("#sql-")) == 0) {
    if (commit_and_begin()) {
      return HA_DUCKDB_DROP_TABLE_ERROR;
    }
  }

  return 0;
}

/**
  @brief
  Renames a table from one name to another via an alter table call.

  @details
  If you do not implement this, the default rename_table() is called from
  handler.cc and it will delete all files with the file extensions from
  handlerton::file_extensions.

  Called from sql_table.cc by mysql_rename_table().

  @see
  mysql_rename_table() in sql_table.cc
*/

int ha_duckdb::rename_table(const char *from, const char *to,
                            const dd::Table *from_table_def,
                            dd::Table *to_table_def) {
  DBUG_TRACE;
  int ret = 0;
  THD *thd = ha_thd();

  ret = duckdb_register_trx(thd);
  if (ret) return ret;

  DatabaseTableNames old_t(from);
  DatabaseTableNames new_t(to);

  auto convertor = std::make_unique<RenameTableConvertor>(
      old_t.db_name, old_t.table_name, new_t.db_name, new_t.table_name,
      from_table_def, to_table_def);
  if (convertor->check()) {
    return HA_DUCKDB_RENAME_ERROR;
  }

  std::string query = convertor->translate();
  auto duckdb_context = thd->get_duckdb_context();

  /* Progress of copy ddl(alter table engine = duckdb)
  1. create tmp table with duckdb engine
  2. insert into tmp table using batch method(appenders)
  3. rename tmp table to normal
  4. trans_comit_implicit, flush appenders
  When doing copy ddl, need to flush before rename(step 3) because
  the table name recorded in appender is the tmp table name.
  The flushing operation is safe. */
  std::string error_msg;
  ret = duckdb_context->flush_appenders(error_msg);
  if (ret) return ret;

  auto query_result = myduck::duckdb_query(thd, query);

  if (query_result->HasError()) {
    return HA_DUCKDB_RENAME_ERROR;
  }
  return ret;
}

/**
  @brief
  Given a starting key and an ending key, estimate the number of rows that
  will exist between the two keys.

  @details
  end_key may be empty, in which case determine if start_key matches any rows.

  Called from opt_range.cc by check_quick_keys().

  @see
  check_quick_keys() in opt_range.cc
*/
ha_rows ha_duckdb::records_in_range(uint, key_range *, key_range *) {
  DBUG_TRACE;
  return 10;  // low number to force index usage
}

/**
  @brief
  create() is called to create a database. The variable name will have the name
  of the table.

  @details
  When create() is called you do not need to worry about
  opening the table. Also, the .frm file will have already been
  created so adjusting create_info is not necessary. You can overwrite
  the .frm file at this point if you wish to change the table
  definition, but there are no methods currently provided for doing
  so.

  Called from handle.cc by ha_create_table().

  @see
  ha_create_table() in handle.cc
*/
int ha_duckdb::create(const char *, TABLE *form, HA_CREATE_INFO *create_info,
                      dd::Table *dd_table) {
  DBUG_TRACE;

  THD *thd = ha_thd();
  int ret = duckdb_register_trx(thd);
  if (ret) return ret;

  CreateTableConvertor convertor(thd, form, create_info, dd_table);

  // check if the table is acceptable to duckdb
  if (convertor.check()) {
    return HA_DUCKDB_CREATE_ERROR;
  }

  // get the create table sql
  std::string query = convertor.translate();

  // execute the create sql
  auto query_result = myduck::duckdb_query(thd, query);

  if (query_result->HasError()) {
    return HA_DUCKDB_CREATE_ERROR;
  }

  if (thd->get_rds_context().is_copy_ddl_from_innodb_to_duckdb() &&
      thd->variables.duckdb_copy_ddl_threads > 1 &&
      strncmp(dd_table->name().c_str(), "#sql-", strlen("#sql-")) == 0) {
    if (commit_and_begin()) {
      return HA_DUCKDB_CREATE_ERROR;
    }
  }

  if (create_info->auto_increment_value) {
    if (share == nullptr) {
      share = get_share();
    }
  }

  return 0;
}

/**
  Check the columns to be dropped depends on the primary key or there are
  some indexes depends on a column after them.

  NOTE: In fact, the above applies to any index, but currently duckdb tables
  have at most one primary key.

  @param[in]  alter_info  Alter options, fields and keys for the
                          new version of table.
  @param[in]  table       Old table.

  @return true if the columns to be dropped are before or in primary key.
          Otherwise false.
*/
static inline bool column_dropped_before_or_in_primary_key(
    Alter_info *alter_info, const TABLE *table) {
  if ((alter_info->flags & Alter_info::ALTER_DROP_COLUMN) == 0) {
    return false;
  }
  if (table->s->keys == 0) return false;

  assert(table->s->keys == 1);
  const KEY *key = table->key_info;

  uint16_t max_field_index_in_pk = 0;
  MY_BITMAP tmp_set;
  bitmap_init(&tmp_set, nullptr, table->s->fields);

  KEY_PART_INFO *key_part = key->key_part;
  KEY_PART_INFO *key_part_end = key_part + key->user_defined_key_parts;

  for (; key_part < key_part_end; key_part++) {
    uint16_t field_index = key_part->field->field_index();
    bitmap_set_bit(&tmp_set, field_index);
    max_field_index_in_pk = std::max(max_field_index_in_pk, field_index);
  }

  for (const Alter_drop *drop : alter_info->drop_list) {
    if (drop->type != Alter_drop::COLUMN) {
      continue;
    }

    Field *field = nullptr;
    Field **first_field = table->field;
    for (Field **ptr = first_field; (field = *ptr); ptr++) {
      if (my_strcasecmp(system_charset_info, field->field_name, drop->name) ==
          0) {
        break;
      }
    }
    assert(field != nullptr);
    uint16_t field_index = field->field_index();
    if (field_index < max_field_index_in_pk ||
        bitmap_is_set(&tmp_set, field_index)) {
      bitmap_free(&tmp_set);
      return true;
    }
  }

  bitmap_free(&tmp_set);
  return false;
}

/**
  Determine whether the primary key deletion operation is currently
  being performed.

  @param[in]  alter_info  Alter options, fields and keys for the
                          new version of table.

  @return true if we are dropping the primary key. Otherwise false.
*/
static inline bool primary_key_dropped(Alter_info *alter_info) {
  if ((alter_info->flags & Alter_info::ALTER_DROP_INDEX) == 0) {
    return false;
  }

  for (const Alter_drop *drop : alter_info->drop_list) {
    if (drop->type != Alter_drop::KEY) {
      continue;
    }
    if (my_strcasecmp(system_charset_info, "primary", drop->name) == 0) {
      return true;
    }
  }

  return false;
}

static inline bool column_changed_in_primary_key(Alter_info *alter_info) {
  if ((alter_info->flags & Alter_info::ALTER_CHANGE_COLUMN) == 0) {
    return false;
  }

  List_iterator<Create_field> new_field_it(alter_info->create_list);
  Create_field *new_field;
  Field *field;

  while ((new_field = new_field_it++)) {
    if (new_field->change == nullptr) {
      continue;
    }
    field = new_field->field;

    if (field->is_equal(new_field) == IS_EQUAL_YES) {
      continue;
    }

    if (field->part_of_key.bits_set() != 0) {
      return true;
    }
  }

  return false;
}

/**
  Determine whether the database has changed after DDL.

  @param[in]  old_schema  Old schema name.
  @param[in]  new_schema  New schema name.

  @return true if the database has changed. Otherwise false.
*/
static inline bool database_changed(const char *old_schema,
                                    const char *new_schema) {
  return my_strcasecmp(system_charset_info, old_schema, new_schema) != 0;
}

static inline bool gcol_added_or_modifed(Alter_info *alter_info) {
  /*
    Since it is somewhat complex to determine whether only generated columns
    have been modified, the current solution is brute-force: as long as a table
    contains generated columns, we fall back to COPY DDL. This is an area that
    still needs optimization.
  */
  List_iterator<Create_field> new_field_it(alter_info->create_list);
  Create_field *new_field;
  while ((new_field = new_field_it++)) {
    if (new_field->is_gcol()) {
      return true;
    }

    if (new_field->field != nullptr && new_field->field->is_gcol()) {
      return true;
    }
  }

  return false;
}

enum_alter_inplace_result ha_duckdb::check_if_supported_inplace_alter(
    TABLE *altered_table [[maybe_unused]],
    Alter_inplace_info *ha_alter_info [[maybe_unused]]) {
  DBUG_TRACE;
  /*
    There are currently no indexes in DuckDB, and we no longer need to check
    for dependencies between indexes. Include:
    1. drop column which is included in DuckDB primary key.
    2. drop column before DuckDB primary key.
    3. drop DuckDB primary key.
    4. change column which is included in DuckDB primary key.
    5. remove RDS IPK column/index when add candidate key.
  */

  if (database_changed(table->s->db.str, altered_table->s->db.str)) {
    return HA_ALTER_INPLACE_NOT_SUPPORTED;
  }

  if (ha_alter_info->alter_info->flags & Alter_info::ALTER_COLUMN_ORDER) {
    return HA_ALTER_INPLACE_NOT_SUPPORTED;
  }

  /*
    The server must inspect existing rows when error_if_not_empty is set.
    INSTANT is invalid in this case. COPY also lets the SQL layer validate
    sql_mode and fill implicit zero dates before converting rows to DuckDB.
  */
  if (ha_alter_info->error_if_not_empty) {
    return HA_ALTER_INPLACE_NOT_SUPPORTED;
  }

  /*
    Check of generated column. Adding/modifying generated columns after table
    creation is not supported yet.
  */
  if (gcol_added_or_modifed(ha_alter_info->alter_info)) {
    return HA_ALTER_INPLACE_NOT_SUPPORTED;
  }

  /*
    Because DuckDB does not alter indexes, so we do not need to fix
    the key parts.
  */

  return HA_ALTER_INPLACE_INSTANT;
}

bool ha_duckdb::commit_inplace_alter_table(TABLE *altered_table,
                                           Alter_inplace_info *ha_alter_info,
                                           bool commit [[maybe_unused]],
                                           const dd::Table *old_table_def
                                           [[maybe_unused]],
                                           dd::Table *new_table_def) {
  DBUG_TRACE;

  /* DuckDB supports transaction DDL, return directly here. */
  if (!commit) {
    return false;
  }

  THD *thd = ha_thd();

  int ret = duckdb_register_trx(thd);
  if (ret) return true;

  ulonglong flags = ha_alter_info->alter_info->flags;
  assert(!(flags & myduck::UNSUPPORT_ALTER_FLAGS));
  assert(!(flags & myduck::IGNORED_ALTER_FLAGS));

  using DDL_convertor = std::unique_ptr<AlterTableConvertor>;
  using DDL_convertors = std::vector<DDL_convertor>;
  DDL_convertor convertor;
  DDL_convertors convertors;

  std::string schema_name = to_string(table->s->db);
  std::string table_name = to_string(table->s->table_name);

  if (flags & Alter_info::ALTER_ADD_COLUMN) {
    convertor = std::make_unique<AddColumnConvertor>(
        schema_name, table_name, altered_table, ha_alter_info->alter_info,
        new_table_def);
    convertors.push_back(std::move(convertor));
  }

  if (flags & Alter_info::ALTER_DROP_COLUMN) {
    convertor =
        std::make_unique<DropColumnConvertor>(schema_name, table_name, table);
    convertors.push_back(std::move(convertor));
  }

  if (flags & Alter_info::ALTER_CHANGE_COLUMN) {
    convertor = std::make_unique<ChangeColumnConvertor>(
        schema_name, table_name, altered_table, new_table_def,
        ha_alter_info->alter_info);
    convertors.push_back(std::move(convertor));
  }

  if (flags & Alter_info::ALTER_CHANGE_COLUMN_DEFAULT) {
    convertor = std::make_unique<ChangeColumnDefaultConvertor>(
        schema_name, table_name, altered_table, new_table_def,
        ha_alter_info->alter_info);
    convertors.push_back(std::move(convertor));
  }

  /*
    When add primary key, although we do not add a primary key in duckdb,
    we need to set the not null flag of the corresponding columns.
  */
  if (flags & Alter_info::ALTER_ADD_INDEX) {
    convertor = std::make_unique<ChangeColumnForPrimaryKeyConvertor>(
        schema_name, table_name, altered_table);
    convertors.push_back(std::move(convertor));
  }

  /*
    Drop partition in DuckDB is translated to delete data in DuckDB.
    By now, we only support RANGE/LIST partition.
  */
  if (flags & Alter_info::ALTER_DROP_PARTITION) {
    m_part_info->set_read_partitions(
        &ha_alter_info->alter_info->partition_names);
    convertor = std::make_unique<DropPartitionConvertor>(
        schema_name, table_name, m_part_info);
    convertors.push_back(std::move(convertor));
  }

  /* Nothing to do with DuckDB. */
  if (convertors.empty()) {
    return false;
  }

  for (auto &convertor : convertors) {
    if (convertor.get() == nullptr) {
      my_error(ER_DA_OOM, MYF(0));
      return true;
    }
  }

  std::ostringstream query;

  for (auto &convertor : convertors) {
    if (convertor->check()) {
      return true;
    }
    query << convertor->translate();
  }

  auto query_result = myduck::duckdb_query(thd, query.str());

  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return true;
  }

  return false;
}

int ha_duckdb::truncate(dd::Table *dd_table) {
  int err = 0;
  DBUG_TRACE;
  THD *thd = ha_thd();

  err = duckdb_register_trx(thd);
  if (err) return err;

  std::string schema_name = to_string(table->s->db);
  std::string table_name = to_string(table->s->table_name);

  std::ostringstream query;
  query << "USE " << myduck::quote_duckdb_ident(schema_name) << ";";
  query << "TRUNCATE TABLE " << myduck::quote_duckdb_ident(table_name) << ";";

  auto query_result = myduck::duckdb_query(ha_thd(), query.str());

  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return HA_DUCKDB_TRUNCATE_TABLE_ERROR;
  }

  if (share != nullptr) {
    share->m_auto_inc_val = 1;
  }

  /* Maintains similar behavior to InnoDB: clear the auto_increment value. */
  auto &s = dd_table->se_private_data();
  if (s.exists(myduck::DUCKDB_AUTO_INC)) {
    s.remove(myduck::DUCKDB_AUTO_INC);
  }

  return err;
}

/*
  For parallel copy ddl from innodb to duckdb, we need commit create
  to make other thd can access this tmp table.
  If copy ddl failed, we need to delete this tmp table becasue it has
  been committed.

  @return fasle if successful, otherwise true.
*/
bool ha_duckdb::commit_and_begin() {
  THD *thd = ha_thd();
  auto query_result = myduck::duckdb_query(thd, "COMMIT");
  if (query_result->HasError()) {
    return true;
  }

  query_result = myduck::duckdb_query(thd, "BEGIN");
  if (query_result->HasError()) {
    return true;
  }

  LogErr(INFORMATION_LEVEL, ER_DUCKDB,
         "commit and begin for copy ddl from InnoDB");

  return false;
}

void ha_duckdb::get_auto_increment(ulonglong offset, ulonglong increment,
                                   ulonglong nb_desired_values [[maybe_unused]],
                                   ulonglong *const first_value,
                                   ulonglong *const nb_reserved_values) {
  DEBUG_SYNC(ha_thd(), "duckdb_get_auto_inc_val");

  /*
    When the value of auto_increment_offset is greater than that of
    auto_increment_increment, the value of auto_increment_offset is ignored.
    Refers:
    https://dev.mysql.com/doc/refman/8.0/en/replication-options-source.html
  */
  if (offset > increment) {
    offset = 1;
  }

  Field *field = table->key_info[table->s->next_number_index].key_part[0].field;
  ulonglong new_val = 0;
  ulonglong max_val = field->get_max_int_value();
  std::atomic<ulonglong> &auto_inc_val = share->m_auto_inc_val;

  if (increment == 1) {
    assert(offset == 1);

    new_val = auto_inc_val;
    while (new_val != std::numeric_limits<ulonglong>::max()) {
      if (auto_inc_val.compare_exchange_weak(new_val,
                                             std::min(new_val + 1, max_val))) {
        break;
      }
    }
  } else {
    ulonglong last_val = auto_inc_val;

    if (last_val > max_val) {
      new_val = std::numeric_limits<ulonglong>::max();
    } else {
      do {
        assert(last_val > 0);
        ulonglong n =
            (last_val - 1) / increment +
            ((last_val - 1) % increment + increment - offset) / increment;

        if (n > (std::numeric_limits<ulonglong>::max() - offset) / increment) {
          assert(max_val == std::numeric_limits<ulonglong>::max());
          new_val = std::numeric_limits<ulonglong>::max();
          auto_inc_val.store(new_val);
          break;
        }

        new_val = n * increment + offset;
      } while (!auto_inc_val.compare_exchange_weak(
          last_val, std::min(new_val + 1, max_val)));
    }
  }

  *first_value = new_val;
  *nb_reserved_values = 1;
}

void ha_duckdb::update_create_info(HA_CREATE_INFO *const create_info) {
  DBUG_TRACE;
  assert(create_info != nullptr);

  if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
    create_info->auto_increment_value = share->m_auto_inc_val;
  }
}

ulonglong ha_duckdb::get_auto_inc_from_duckdb() {
  Field *field = table->found_next_number_field;
  if (field == nullptr) return 0;

  std::string schema_name = to_string(table->s->db);
  std::string table_name = to_string(table->s->table_name);

  std::ostringstream query;
  query << "SELECT MAX(`";
  query << field->field_name;
  query << "`) FROM `" << schema_name << "`.`" << table_name << "`;";

  auto query_result = myduck::duckdb_query(ha_thd(), query.str());
  if (query_result == nullptr || query_result->HasError()) return 0;

  if (query_result->type == duckdb::QueryResultType::STREAM_RESULT) {
    auto &stream_result = query_result->Cast<duckdb::StreamQueryResult>();
    query_result = stream_result.Materialize();
  }

  auto materialized_res =
      dynamic_cast<duckdb::MaterializedQueryResult *>(query_result.get());
  auto value = materialized_res->GetValue(0, 0);
  if (value.IsNull()) return 0;

  ulonglong value_in_duckdb = value.GetValue<uint64_t>();
  ulonglong max_val = field->get_max_int_value();

  return std::min(value_in_duckdb, max_val);
}

void ha_duckdb::update_auto_inc_val(ulonglong update_val,
                                    Duckdb_share *new_share) {
  Field *field = table->found_next_number_field;
  if (field == nullptr) {
    return;
  }

  ulonglong new_val = update_val;
  ulonglong max_val = field->get_max_int_value();
  if (new_val != max_val) {
    new_val++;
  }

  Duckdb_share *update_share = nullptr;

  if (new_share == nullptr) {
    update_share = share;
  } else {
    /*
      We just create the duckdb_share, so the auto increment value saved in DD
      should be checked too.
    */
    assert(share == nullptr);
    update_share = new_share;
    ulonglong auto_inc_in_dd = get_auto_inc_from_dd();
    new_val = std::max(new_val, auto_inc_in_dd);
    new_val = std::min(new_val, max_val);
  }

  if (new_val <= max_val) {
    ulonglong cur_auto_inc_val = update_share->m_auto_inc_val;
    while (cur_auto_inc_val < new_val &&
           !update_share->m_auto_inc_val.compare_exchange_weak(cur_auto_inc_val,
                                                               new_val)) {
    }
  }
}

ulonglong ha_duckdb::get_auto_inc_from_dd() {
  THD *thd = ha_thd();
  ulonglong auto_inc = 0;
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_def = nullptr;
  if (thd->dd_client()->acquire(table->s->db.str, table->s->table_name.str,
                                &table_def)) {
    return auto_inc;
  }

  if (table_def == nullptr) {
    return auto_inc;
  }

  auto &s = table_def->se_private_data();
  if (s.exists(myduck::DUCKDB_AUTO_INC)) {
    s.get(myduck::DUCKDB_AUTO_INC, &auto_inc);
  }

  return auto_inc;
}

struct st_mysql_storage_engine duckdb_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static SYS_VAR *duckdb_system_variables[] = {
    MYSQL_SYSVAR(copy_ddl_in_batch), MYSQL_SYSVAR(dml_in_batch),
    MYSQL_SYSVAR(batch_max_row_count),
    MYSQL_SYSVAR(update_modified_column_only), nullptr};

/** Get the tablespace type given the name.

@param[in]  tablespace_name tablespace name
@param[out] space_type      type of space

@return Operation status.
@retval false on success and true for failure.
*/
static bool duckdb_get_tablespace_type_by_name(const char *tablespace_name
                                               [[maybe_unused]],
                                               Tablespace_type *space_type
                                               [[maybe_unused]]) {
  /* TODO */
  return false;
}

static SHOW_VAR show_status_duckdb[] = {
    {"rows_insert", (char *)&srv_duckdb_status.duckdb_rows_insert,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"rows_update", (char *)&srv_duckdb_status.duckdb_rows_update,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {"rows_delete", (char *)&srv_duckdb_status.duckdb_rows_delete,
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},

    {"rows_insert_in_batch",
     (char *)&srv_duckdb_status.duckdb_rows_insert_in_batch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"rows_update_in_batch",
     (char *)&srv_duckdb_status.duckdb_rows_update_in_batch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"rows_delete_in_batch",
     (char *)&srv_duckdb_status.duckdb_rows_delete_in_batch, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},

    {"commit", (char *)&srv_duckdb_status.duckdb_commit, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {"rollback", (char *)&srv_duckdb_status.duckdb_rollback, SHOW_LONGLONG,
     SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

static int show_func_duckdb(MYSQL_THD, SHOW_VAR *var, char *) {
  var->type = SHOW_ARRAY;
  var->value = (char *)&show_status_duckdb;
  var->scope = SHOW_SCOPE_GLOBAL;
  return 0;
}

static SHOW_VAR func_status[] = {
    {"Duckdb", (char *)show_func_duckdb, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {NullS, NullS, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

mysql_declare_plugin(duckdb){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &duckdb_storage_engine,
    "DUCKDB",
    PLUGIN_AUTHOR_ORACLE,
    "Duckdb storage engine",
    PLUGIN_LICENSE_GPL,
    duckdb_init_func, /* Plugin Init */
    nullptr,          /* Plugin check uninstall */
    nullptr,          /* Plugin Deinit */
    0x0001 /* 0.1 */,
    func_status,             /* status variables */
    duckdb_system_variables, /* system variables */
    nullptr,                 /* config options */
    0,                       /* flags */
} mysql_declare_plugin_end;

uint32_t ha_duckdb::calculate_key_hash_value(Field **field_array) {
  return Partition_helper::ph_calculate_key_hash_value(field_array);
}

int ha_duckdb::truncate_partition_low(dd::Table *dd_table) {
  if (bitmap_is_set_all(&m_part_info->read_partitions)) {
    return truncate(dd_table);
  }

  if (m_part_info->part_type == partition_type::HASH) {
    my_error(ER_DUCKDB_ALTER_OPERATION_NOT_SUPPORTED, MYF(0),
             "TRUNCATE HASH/KEY PARTITION");
    return HA_DUCKDB_TRUNCATE_TABLE_ERROR;
  }

  std::string query;
  if (myduck::generate_delete_from_partition(m_part_info, true, query)) {
    return HA_DUCKDB_TRUNCATE_TABLE_ERROR;
  }

  auto query_result = myduck::duckdb_query(ha_thd(), query);

  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return HA_DUCKDB_TRUNCATE_TABLE_ERROR;
  }

  return 0;
}
