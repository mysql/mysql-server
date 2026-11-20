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

#include <mysql/components/services/log_builtins.h>
#include <memory>
#include <queue>
#include <thread>
#include "mysql/psi/mysql_stage.h"
#include "scope_guard.h"
#include "sql/create_field.h"
#include "sql/derror.h"
#ifdef HAVE_DUCKDB
#include "sql/duckdb/duckdb_query.h"
#endif
#include "sql/handler.h"
#include "sql/histograms/table_histograms.h"
#include "sql/item.h"
#include "sql/mysqld.h"
#include "sql/partition_info.h"
#include "sql/sql_alter.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/sql_table.h"
#include "sql/sql_thd_internal_api.h"
#include "sql/transaction.h"

bool duckdb_require_primary_key = true;
bool force_innodb_to_duckdb = false;
bool ignore_index_hint_error = false;

LEX_CSTRING FORCE_CONVERT_SYS_SCHEMA_NAME = {STRING_WITH_LEN("sys")};

/**
  Convert partition/subpartition engine types to the target engine.

  @param[in]  part_info       Partition info containing partition elements.
  @param[in]  target_hton     Target engine handlerton.
  @param[in]  only_from_innodb If true, only convert partitions whose engine
                               is InnoDB; otherwise convert all non-null
  engines.

  @return true if at least one partition engine was converted, false otherwise.
*/
static bool convert_partition_engine(partition_info *part_info,
                                     handlerton *target_hton,
                                     bool only_from_innodb) {
  if (!part_info) return false;
  bool converted = false;
  List_iterator<partition_element> part_it(part_info->partitions);
  partition_element *part_elem;
  while ((part_elem = part_it++)) {
    if (part_elem->engine_type &&
        (!only_from_innodb ||
         part_elem->engine_type->db_type == DB_TYPE_INNODB)) {
      part_elem->engine_type = target_hton;
      converted = true;
    }
    List_iterator<partition_element> sub_it(part_elem->subpartitions);
    partition_element *sub_elem;
    while ((sub_elem = sub_it++)) {
      if (sub_elem->engine_type &&
          (!only_from_innodb ||
           sub_elem->engine_type->db_type == DB_TYPE_INNODB)) {
        sub_elem->engine_type = target_hton;
        converted = true;
      }
    }
  }
  return converted;
}

/**
  @brief whether the type of the thd is a ignore type

  Check whether the type of the thd is SYSTEM_THREAD_SERVER_INITIALIZE,
  SYSTEM_THREAD_DD_INITIALIZE or SYSTEM_THREAD_DD_RESTART

  @param[in]     thd          Thread descriptor.

  @return
     @retval true      the type of thd is a ignore type
     @retval false     thd is null or the type is another type
*/

static bool is_ignore_thread_type(const THD *thd) {
  if (!thd) {
    return false;
  }

  if (thd->system_thread == SYSTEM_THREAD_SERVER_INITIALIZE ||
      thd->system_thread == SYSTEM_THREAD_DD_INITIALIZE ||
      thd->system_thread == SYSTEM_THREAD_DD_RESTART) {
    return true;
  }

  return false;
}

/**
  @brief whether the input schema is a system schema

  Check whether the input schema name is mysql, sys,
  information schema or performance schema

  @param[in]     schema      input schema name.

  @return
     @retval true      the schema is a system schema
     @retval false     schema is null or the schema is not system schema

*/

static bool is_system_schema(const char *schema) {
  if (!schema) {
    return false;
  }

  if (my_strcasecmp(system_charset_info, schema, MYSQL_SCHEMA_NAME.str) == 0 ||
      my_strcasecmp(system_charset_info, schema,
                    PERFORMANCE_SCHEMA_DB_NAME.str) == 0 ||
      my_strcasecmp(system_charset_info, schema, INFORMATION_SCHEMA_NAME.str) ==
          0) {
    return true;
  }

  if (my_strcasecmp(system_charset_info, schema,
                    FORCE_CONVERT_SYS_SCHEMA_NAME.str) == 0) {
    return true;
  }

  return false;
}

/**
  @brief whether the table is a partition table

  Check whether the table is a partition table or will become a
  partition table

  @param[in]     thd      Thread descriptor.
  @param[in]     table    Table descriptor.


  @return
     @retval true      the table is a partition table
     @retval false     otherwise

*/

static bool is_partition_table(const THD *thd, const TABLE *table) {
  if (thd && thd->work_part_info) {
    return true;
  }

  if (table && table->part_info) {
    return true;
  }
  return false;
}

static void force_convert_engine(THD *thd, HA_CREATE_INFO *create_info,
                                 TABLE *table) {
  if (!force_innodb_to_duckdb) return;
  if (create_info->db_type &&
      ha_is_storage_engine_disabled(create_info->db_type)) {
    return;
  }

  bool from_innodb_to_duckdb = false;
  bool is_tmp_table =
      table && table->s && (table->s->tmp_table != NO_TMP_TABLE);
  if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) && !is_tmp_table &&
      ((create_info->db_type &&
        create_info->db_type->db_type == DB_TYPE_INNODB) ||
       (!create_info->db_type && table && table->s &&
        table->s->db_type()->db_type == DB_TYPE_INNODB))) {
    from_innodb_to_duckdb = true;
  } else if (!(create_info->options & HA_LEX_CREATE_TMP_TABLE) &&
             !is_tmp_table && thd->work_part_info &&
             ((create_info->db_type &&
               create_info->db_type->db_type == DB_TYPE_DUCKDB) ||
              (!create_info->db_type && table && table->s &&
               table->s->db_type()->db_type == DB_TYPE_DUCKDB))) {
    const LEX_CSTRING duckdb_engine_name = {STRING_WITH_LEN("DUCKDB")};
    plugin_ref plugin = ha_resolve_by_name(thd, &duckdb_engine_name, false);
    if (plugin) {
      convert_partition_engine(thd->work_part_info,
                               plugin_data<handlerton *>(plugin), true);
    }
    return;
  }

  if (!from_innodb_to_duckdb) return;

  const LEX_CSTRING duckdb_engine_name = {STRING_WITH_LEN("DUCKDB")};
  plugin_ref plugin = ha_resolve_by_name(thd, &duckdb_engine_name, false);
  if (plugin) {
    handlerton *duckdb_hton = plugin_data<handlerton *>(plugin);
    create_info->db_type = duckdb_hton;
    create_info->used_fields |= HA_CREATE_USED_ENGINE;

    if (thd->work_part_info) {
      thd->work_part_info->default_engine_type = duckdb_hton;
      convert_partition_engine(thd->work_part_info, duckdb_hton, false);
    }

    return;
  }
}

void force_convert_engine(THD *thd, const char *db,
                          HA_CREATE_INFO *create_info) {
  if (is_ignore_thread_type(thd) || is_system_schema(db) ||
      (is_partition_table(thd, NULL) && !force_innodb_to_duckdb)) {
    return;
  }

  force_convert_engine(thd, create_info, nullptr);
}
void force_convert_engine(THD *thd, TABLE *table, const char *db1,
                          const char *db2, HA_CREATE_INFO *create_info) {
  if (is_ignore_thread_type(thd) || is_system_schema(db1) ||
      is_system_schema(db2) ||
      (is_partition_table(thd, table) && !force_innodb_to_duckdb)) {
    return;
  }

  force_convert_engine(thd, create_info, table);
}

/** Record buffer struct. */
struct rec_buf_t {
  /* record buffer. */
  void *buf;

  /* The number of rows in current buffer. */
  uint n_rows;

  /* Extra information, point to blob heap memory usually. */
  std::shared_ptr<void> extra;

  rec_buf_t(void *data, uint rows) : buf(data), n_rows(rows) {}
  rec_buf_t(void *data, uint rows, std::shared_ptr<void> &ext)
      : buf(data), n_rows(rows), extra(ext) {}
};

/** Thread safe queue of record buffer.  */
class Record_buffers {
 public:
  Record_buffers(size_t capacity, size_t buffer_size, size_t rec_size)
      : m_capacity(capacity),
        m_buffer_size(buffer_size),
        m_record_size(rec_size),
        m_finished(false) {
    mysql_mutex_init(PSI_NOT_INSTRUMENTED, &m_lock, MY_MUTEX_INIT_FAST);
    mysql_cond_init(PSI_NOT_INSTRUMENTED, &m_not_full_cv);
    mysql_cond_init(PSI_NOT_INSTRUMENTED, &m_not_empty_cv);
  }

  ~Record_buffers() {
    mysql_mutex_destroy(&m_lock);
    mysql_cond_destroy(&m_not_full_cv);
    mysql_cond_destroy(&m_not_empty_cv);
  }

  /** Push record buffer into queue. */
  bool push(void *buf, uint n_rows, std::shared_ptr<void> &extra) {
    Auto_locker lock(&m_lock);
#ifndef NDEBUG
    std::cout << "id: " << std::this_thread::get_id() << ", n_rows: " << n_rows
              << std::endl;
#endif
    while (m_buffers.size() >= m_capacity && !m_finished) {
      mysql_cond_wait(&m_not_full_cv, &m_lock);
    }
    if (m_finished) return false;

    /* Copy buf to buffers. */
    void *m_buf = acquire();
    memcpy(m_buf, buf, n_rows * m_record_size);
    rec_buf_t rec_buf(m_buf, n_rows, extra);
    m_buffers.push(rec_buf);
    mysql_cond_signal(&m_not_empty_cv);
    return true;
  }

  /** Pop record bffer into queue. */
  bool pop(rec_buf_t &buf) {
    Auto_locker lock(&m_lock);
    while (m_buffers.empty() && !m_finished) {
      mysql_cond_wait(&m_not_empty_cv, &m_lock);
    }
    if (m_buffers.empty()) return false;

    buf = m_buffers.front();
    m_buffers.pop();
    mysql_cond_signal(&m_not_full_cv);
    return true;
  }

  /** Acquire a usable record buffer. */
  void *acquire() {
    mysql_mutex_assert_owner(&m_lock);
    void *free = nullptr;
    if (m_free.empty()) {
      free = m_mem_root.Alloc(m_buffer_size);
    } else {
      free = m_free.back();
      m_free.pop_back();
    }
    return free;
  }

  /** Release a record buffer to free list. */
  void release(rec_buf_t &free) {
    Auto_locker lock(&m_lock);
    m_free.push_back(free.buf);
    free.n_rows = 0;
    free.extra.reset();
  }

  /** Finish, record buffer will not be pushed if finished. */
  void finish() {
    Auto_locker lock(&m_lock);
    m_finished = true;
    mysql_cond_broadcast(&m_not_full_cv);
    mysql_cond_broadcast(&m_not_empty_cv);
  }

 private:
  MEM_ROOT m_mem_root;
  std::queue<rec_buf_t> m_buffers;
  std::vector<void *> m_free;
  size_t m_capacity;
  size_t m_buffer_size;
  size_t m_record_size;
  mysql_mutex_t m_lock;
  mysql_cond_t m_not_full_cv;
  mysql_cond_t m_not_empty_cv;
  bool m_finished;

  /* RAII lock. */
  class Auto_locker {
   public:
    Auto_locker(mysql_mutex_t *lock) : m_lock(lock) {
      mysql_mutex_lock(m_lock);
    }
    ~Auto_locker() { mysql_mutex_unlock(m_lock); }

   private:
    mysql_mutex_t *m_lock;
  };
};

/**
  Find field by field name.
  @param[in]  table     TABLE object
  @param[in]  fiel_name field name.

  @return target field.
*/

static Field *find_field_in_table(TABLE *table, const char *field_name) {
  Field **first_field = table->field;
  Field **ptr, *cur_field;
  for (ptr = first_field; (cur_field = *ptr); ptr++) {
    if (my_strcasecmp(system_charset_info, cur_field->field_name, field_name) ==
        0)
      break;
  }
  return cur_field;
}

/*
  Free resource of fake TABLE for parallel COPY DDL.
  @param[in]  table   TABLE object to be freed.
*/
static void free_fake_table(TABLE *table) {
  if (table == nullptr) {
    return;
  }

  if (table->histograms != nullptr) {
    mysql_mutex_lock(&LOCK_open);
    table->s->m_histograms->release(table->histograms);
    mysql_mutex_unlock(&LOCK_open);
    table->histograms = nullptr;
  }
  free_io_cache(table);
  if (table->file) {
    ::destroy_at(table->file);
    table->file = nullptr;
  }
  closefrm(table, false);
  my_free(const_cast<char *>(table->alias));
  ::destroy_at(table);
  my_free(table);
}

/**
  COPY DDL worker, which is similar like `copy_data_between_tables`.
*/
static void copy_data_thread(THD *ori_thd [[maybe_unused]],
                             PSI_stage_progress *psi [[maybe_unused]],
                             TABLE *ori_from, TABLE *ori_to,
                             List<Create_field> *ori_create, ha_rows *copied,
                             ha_rows *deleted, Alter_table_ctx *alter_ctx,
                             Record_buffers *record_buffers, ulong *current_row,
                             int &error) {
  DBUG_TRACE;

  my_thread_init();
  assert(current_thd == nullptr);
  THD *thd = create_thd(false, false, false, PSI_NOT_INSTRUMENTED, 0);
  thd->security_context()->skip_grants();
  assert(current_thd == thd);
  TABLE *from = nullptr;
  TABLE *to = nullptr;

  auto restore_thd = create_scope_guard([&]() {
    free_fake_table(from);
    free_fake_table(to);
    thd->free_items();
    destroy_thd(thd);
    assert(current_thd == nullptr);
    my_thread_end();
  });

  {
    /*
      For partition table, query_block may be accessed during open_table,
      so we bind original lex to the temporary thd.
    */
    auto saved_lex = thd->lex;
    thd->lex = ori_thd->lex;
    auto reset_current_thd =
        create_scope_guard([&]() { thd->lex = saved_lex; });

    from = (TABLE *)my_malloc(key_memory_TABLE, sizeof(TABLE), MYF(MY_WME));
    if (from == nullptr ||
        open_table_from_share(thd, ori_from->s, "", 0, (uint)READ_ALL, 0, from,
                              false, nullptr)) {
      error = -1;
      return;
    }
    bitmap_set_all(from->read_set);

    to = (TABLE *)my_malloc(key_memory_TABLE, sizeof(TABLE), MYF(MY_WME));
    if (to == nullptr ||
        open_table_from_share(thd, ori_to->s, "", 0, (uint)READ_ALL, 0, to,
                              false, nullptr)) {
      error = -1;
      return;
    }
    bitmap_set_all(to->write_set);
  }

  /* Copy create_fields from original create_field list. */
  List<Create_field> create;
  List_iterator<Create_field> it_ori(*ori_create);
  Create_field *el;
  while ((el = it_ori++)) {
    create.push_back(el->clone(thd->mem_root));
  }

  /* Map table->field to create_field. */
  List_iterator<Create_field> it(create);
  while ((el = it++)) {
    if (el->field) {
      el->field = find_field_in_table(from, el->field_name);
      assert(el->field);
    }
  }

  Copy_field *copy, *copy_end;
  Field **ptr;
  /*
    Fields which values need to be generated for each row, i.e. either
    generated fields or newly added fields with generated default values.
  */
  Field **gen_fields, **gen_fields_end;
  bool auto_increment_field_copied = false;
  sql_mode_t save_sql_mode;

  /* We do not use unit and select here. */

  /*
    If target storage engine supports atomic DDL we should not commit
    and disable transaction to let SE do proper cleanup on error/crash.
    Such engines should be smart enough to disable undo/redo logging
    for target table automatically.
    Temporary tables path doesn't employ atomic DDL support so disabling
    transaction is OK. Moreover doing so allows to not interfere with
    concurrent FLUSH TABLES WITH READ LOCK.
  */
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_prepare_alter_copy_data(thd)) {
    error = -1;
    return;
  }

  if (!(copy = new (thd->mem_root) Copy_field[to->s->fields])) {
    error = -1; /* purecov: inspected */
    return;
  }

  if (!(gen_fields = thd->mem_root->ArrayAlloc<Field *>(
            to->s->gen_def_field_count + to->s->vfields))) {
    std::destroy_n(copy, to->s->fields);
    error = -1;
    return;
  }

  if (to->file->ha_external_lock(thd, F_WRLCK)) {
    std::destroy_n(copy, to->s->fields);
    error = -1;
    return;
  }

  /* DuckDB do not need to handle manage keys. */

  /* Only consider not preapred */

  /*
    We want warnings/errors about data truncation emitted when we
    copy data to new version of table.
  */
  thd->check_for_truncated_fields = CHECK_FIELD_WARN;
  thd->num_truncated_fields = 0L;

  /* Skip from file info. */

  /* DuckDB ha_start_bulk_insert do nothing, skip. */

  mysql_stage_set_work_estimated(psi, from->file->stats.records);

  save_sql_mode = thd->variables.sql_mode;

  it.rewind();
  const Create_field *def;
  copy_end = copy;
  gen_fields_end = gen_fields;
  for (ptr = to->field; *ptr; ptr++) {
    def = it++;
    if ((*ptr)->is_gcol()) {
      /*
        Values in generated columns need to be (re)generated even for
        pre-existing columns, as they might depend on other columns,
        values in which might have changed as result of this ALTER.
        Because of this there is no sense in copying old values for
        these columns.
        TODO: Figure out if we can avoid even reading these old values
              from SE.
      */
      *(gen_fields_end++) = *ptr;
      continue;
    }
    // Array fields will be properly generated during GC update loop below
    assert(!def->is_array);
    if (def->field) {
      if (*ptr == to->next_number_field) {
        auto_increment_field_copied = true;
        /*
          If we are going to copy contents of one auto_increment column to
          another auto_increment column it is sensible to preserve zeroes.
          This condition also covers case when we are don't actually alter
          auto_increment column.
        */
        if (def->field == from->found_next_number_field)
          thd->variables.sql_mode |= MODE_NO_AUTO_VALUE_ON_ZERO;
      }
      (copy_end++)->set(*ptr, def->field);
    } else {
      /*
        New column. Add it to the array of columns requiring value
        generation if it has generated default.
      */
      if ((*ptr)->has_insert_default_general_value_expression()) {
        assert(!((*ptr)->is_gcol()));
        *(gen_fields_end++) = *ptr;
      }
    }
  }

  ulong found_count = 0;
  ulong delete_count = 0;

  /* DuckDB do case order. */

  /* DuckDB do not use RowIterator */

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();

  /* Do not handle prepared. */

  /* Skip create iterator. */

  thd->get_stmt_da()->reset_current_row_for_condition();

  set_column_static_defaults(to, create);

  to->file->ha_extra(HA_EXTRA_BEGIN_ALTER_COPY);

  rec_buf_t rec_buf(nullptr, 0);
  while (record_buffers->pop(rec_buf)) {
    DBUG_EXECUTE_IF("duckdb_convert_sleep_during_parallel_alter", {
      fprintf(stderr, "duckdb_convert_sleep_during_parallel_alter\n");
      my_sleep(6000000);
    });

    for (size_t i = 0; i < rec_buf.n_rows; i++) {
      memcpy(from->record[0], (uchar *)(rec_buf.buf) + i * from->s->reclength,
             from->s->reclength);
      if (thd->killed) {
        thd->send_kill_message();
        error = 1;
        break;
      }
      /*
        Return error if source table isn't empty.

        For a DATE/DATETIME field, return error only if strict mode
        and No ZERO DATE mode is enabled.
      */
      if ((alter_ctx->error_if_not_empty &
           Alter_table_ctx::GEOMETRY_WITHOUT_DEFAULT) ||
          ((alter_ctx->error_if_not_empty &
            Alter_table_ctx::DATETIME_WITHOUT_DEFAULT) &&
           (thd->variables.sql_mode & MODE_NO_ZERO_DATE) &&
           thd->is_strict_mode())) {
        error = 1;
        break;
      }
      if (to->next_number_field) {
        if (auto_increment_field_copied)
          to->autoinc_field_has_explicit_non_null_value = true;
        else
          to->next_number_field->reset();
      }

      for (Copy_field *copy_ptr = copy; copy_ptr != copy_end; copy_ptr++) {
        copy_ptr->invoke_do_copy();
      }
      if (thd->is_error()) {
        error = 1;
        break;
      }

      /*
        Iterate through all generated columns and all new columns which have
        generated defaults and evaluate their values. This needs to happen
        after copying values for old columns and storing default values for
        new columns without generated defaults, as generated values might
        depend on these values.
        OTOH generated columns/generated defaults need to be processed in
        the order in which their columns are present in table as generated
        values are allowed to depend on each other as long as there are no
        forward references (i.e. references to other columns with generated
        values which come later in the table).
      */
      for (ptr = gen_fields; ptr != gen_fields_end; ptr++) {
        Item *expr_item;
        if ((*ptr)->is_gcol()) {
          expr_item = (*ptr)->gcol_info->expr_item;
        } else {
          assert((*ptr)->has_insert_default_general_value_expression());
          expr_item = (*ptr)->m_default_val_expr->expr_item;
        }
        expr_item->save_in_field(*ptr, false);
        if (thd->is_error()) {
          error = 1;
          break;
        }
      }
      if (error) break;

      error = invoke_table_check_constraints(thd, to);
      if (error) break;

      error = to->file->ha_write_row(to->record[0]);
      to->autoinc_field_has_explicit_non_null_value = false;
      if (error) {
        if (!to->file->is_ignorable_error(error)) {
          /* Not a duplicate key error. */
          to->file->print_error(error, MYF(0));
          break;
        } else {
          /* Report duplicate key error. */
          uint key_nr = to->file->get_dup_key(error);
          if ((int)key_nr >= 0) {
            const char *err_msg = ER_THD(thd, ER_DUP_ENTRY_WITH_KEY_NAME);
            if (key_nr == 0 && (to->key_info[0].key_part[0].field->is_flag_set(
                                   AUTO_INCREMENT_FLAG)))
              err_msg = ER_THD(thd, ER_DUP_ENTRY_AUTOINCREMENT_CASE);
            print_keydup_error(
                to, key_nr == MAX_KEY ? nullptr : &to->key_info[key_nr],
                err_msg, MYF(0), from->s->table_name.str);
          } else
            to->file->print_error(error, MYF(0));
          break;
        }
      } else {
        found_count++;
        mysql_stage_set_work_completed(psi, found_count);
      }
      thd->get_stmt_da()->inc_current_row_for_condition();
    }
    record_buffers->release(rec_buf);
  }
  /* Skip iterrator reset. */
  free_io_cache(from);

  /* DuckDB do nothing in ha_end_bulk_insert. Skip. */

  to->file->ha_extra(HA_EXTRA_END_ALTER_COPY);

  DBUG_EXECUTE_IF("crash_copy_before_commit", DBUG_SUICIDE(););
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_commit_alter_copy_data(thd))
    error = 1;

  std::destroy_n(copy, to->s->fields);
  thd->variables.sql_mode = save_sql_mode;
  free_io_cache(from);
  *copied = found_count;
  *deleted = delete_count;
  *current_row = thd->get_stmt_da()->current_row_for_condition();
  to->file->ha_release_auto_increment();
  if (to->file->ha_external_lock(thd, F_UNLCK)) error = 1;
  if (error < 0 && to->file->ha_extra(HA_EXTRA_PREPARE_FOR_RENAME)) error = 1;
  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;
  error = error > 0 ? -1 : 0;

  if (error == -1) {
    trans_rollback_stmt(thd);
    trans_rollback(thd);
  } else {
    if (trans_commit_stmt(thd) || trans_commit(thd) ||
        DBUG_EVALUATE_IF("duckdb_parallel_copy_ddl_failed", true, false)) {
      error = -1;
    }
  }
}

int parallel_copy_data_between_tables(THD *thd,
                                      PSI_stage_progress *psi [[maybe_unused]],
                                      TABLE *from, TABLE *to,
                                      List<Create_field> &create,
                                      ha_rows *copied, ha_rows *deleted,
                                      Alter_table_ctx *alter_ctx) {
  DBUG_TRACE;

  LogErr(INFORMATION_LEVEL, ER_DUCKDB_PARALLEL_COPY, "begin",
         to->s->table_name.str);
  thd->get_rds_context().set_duckdb_parallel_copy_ddl(true);

  int error = 0;
  void *scan_trx = nullptr;
  size_t n_threads = 0;
  error = from->file->parallel_scan_init(
      scan_trx, &n_threads, false, thd->variables.duckdb_copy_ddl_threads);
  if (error) {
    thd->get_rds_context().set_duckdb_parallel_copy_ddl(false);
    return error;
  }
  auto thread_contexts = std::make_unique<void *[]>(n_threads);
  size_t size_buffer_size = 2 * 1024 * 1024;
  Record_buffers record_buffers(n_threads * 2, size_buffer_size,
                                from->s->reclength);

  std::vector<ha_rows> copied_array(n_threads, 0);
  std::vector<ha_rows> deleted_array(n_threads, 0);
  std::vector<ulong> current_rows_array(n_threads, 0);
  std::vector<int> errs(n_threads, 0);

  /* Parallel write to DuckDB. */
  std::vector<std::thread> threads;
  for (size_t i = 0; i < n_threads; i++) {
    threads.emplace_back(copy_data_thread, thd, psi, from, to, &create,
                         &copied_array[i], &deleted_array[i], alter_ctx,
                         &record_buffers, &current_rows_array[i],
                         std::ref(errs[i]));
  }

  /* Paralle read from InnoDB. */
  auto init_fn = [&](void *, ulong, ulong, const ulong *, const ulong *,
                     const ulong *) -> bool {
    /* no op */
    return false;
  };

  auto load_fn = [&](void *, uint nrows, void *rowdata, uint64_t,
                     void *other) -> bool {
    auto ext = (std::shared_ptr<void> *)(other);
    record_buffers.push(rowdata, nrows, *ext);
    return false;
  };

  auto end_fn = [&](void *) {
    /* no op */
    return;
  };

  if (from->file->parallel_scan(scan_trx, thread_contexts.get(), init_fn,
                                load_fn, end_fn) != 0) {
    error = 1;
  }
  from->file->parallel_scan_end(scan_trx);
  record_buffers.finish();
  /* Wait util all buffers are writen. */
  for (auto &thread : threads) {
    thread.join();
  }

  ulong current_rows = 0;
  for (size_t i = 0; i < n_threads; i++) {
    *copied += copied_array[i];
    *deleted += deleted_array[i];
    current_rows += current_rows_array[i];
    if (errs[i] != 0) {
      error = errs[i];
      if (!thd->get_stmt_da()->is_set()) {
        my_error(ER_DUCKDB_CLIENT, MYF(0), "parallel copy ddl failed");
      }
    }
  }
  thd->get_stmt_da()->set_current_row_for_condition(current_rows);

  thd->get_rds_context().set_duckdb_parallel_copy_ddl(false);

  DBUG_EXECUTE_IF("simulate_parallel_copy_ddl_crash", { DBUG_SUICIDE(); });

  DBUG_EXECUTE_IF("simulate_parallel_copy_ddl_failed", {
    my_error(ER_UNKNOWN_ERROR, MYF(0));
    error = 1;
  });

  if (error != 0) {
    LogErr(ERROR_LEVEL, ER_DUCKDB_PARALLEL_COPY, "failed",
           to->s->table_name.str);
  } else {
    LogErr(INFORMATION_LEVEL, ER_DUCKDB_PARALLEL_COPY, "finished",
           to->s->table_name.str);
  }

  return error;
}

/**
  Check if key is a unique key without nullable part.
*/
bool is_not_nullable_uk(Key_spec *key, Alter_info *alter_info) {
  if (key->type != KEYTYPE_UNIQUE) {
    return false;
  }

  for (uint i = 0; i < key->columns.size(); i++) {
    const Key_part_spec *col = key->columns[i];
    if (col->get_field_name() == nullptr) {
      return false;
    }

    List_iterator<Create_field> it(alter_info->create_list);
    Create_field *sql_field = nullptr;
    while ((sql_field = it++) &&
           my_strcasecmp(system_charset_info, col->get_field_name(),
                         sql_field->field_name))
      ;
    if (sql_field != nullptr && sql_field->is_nullable) {
      return false;
    }
  }

  return true;
}

/**
  Copy data between DuckDB tables using "INSERT ... SELECT ..."

  @param[in]  thd          Thread handle.
  @param[in]  psi          Progress indicator.
  @param[in]  from         Table to copy from.
  @param[in]  to           Table to copy to.
  @param[in]  copy         Field-pairs to copy.
  @param[in]  copy_end     End of field-pairs to copy.
  @param[out] found_count  Number of rows copied.

  @return 0 if success, otherwise error code.
*/
int copy_data_use_select_insert(THD *thd, PSI_stage_progress *psi, TABLE *from,
                                TABLE *to, Copy_field *copy,
                                Copy_field *copy_end, ulong &found_count) {
#ifdef HAVE_DUCKDB
  std::string query;
  std::string from_table =
      "`" + std::string(from->s->db.str, from->s->db.length) + "`.`" +
      std::string(from->s->table_name.str, from->s->table_name.length) + "`";
  std::string to_table =
      "`" + std::string(to->s->db.str, to->s->db.length) + "`.`" +
      std::string(to->s->table_name.str, to->s->table_name.length) + "`";

  std::string from_fields_str;
  std::string to_fields_str;

  Field *from_field = nullptr;
  Field *to_field = nullptr;

  /**
    TODO: gen_field is not support now. And some default
    value/expression may be lost.
  */
  for (Copy_field *copy_ptr = copy; copy_ptr != copy_end; copy_ptr++) {
    from_field = copy_ptr->from_field();
    to_field = copy_ptr->to_field();

    from_fields_str += "`";
    from_fields_str += from_field->field_name;
    from_fields_str += "`";

    to_fields_str += "`";
    to_fields_str += to_field->field_name;
    to_fields_str += "`";

    if (copy_ptr + 1 != copy_end) {
      from_fields_str += ", ";
      to_fields_str += ", ";
    }
  }

  query += "INSERT INTO " + to_table + " (" + to_fields_str + ") ";
  query += "SELECT " + from_fields_str + " FROM " + from_table;

  auto query_result = myduck::duckdb_query(thd, query, true);

  if (query_result->HasError()) {
    my_error(ER_DUCKDB_QUERY_ERROR, MYF(0), query_result->GetError().c_str());
    return 1;
  }

  if (query_result->type == duckdb::QueryResultType::STREAM_RESULT) {
    auto &stream_result = query_result->Cast<duckdb::StreamQueryResult>();
    query_result = stream_result.Materialize();
  }
  assert(query_result->type == duckdb::QueryResultType::MATERIALIZED_RESULT);
  auto &materialize_query_result =
      query_result->Cast<duckdb::MaterializedQueryResult>();
  auto &coll = materialize_query_result.Collection();
  for (auto &row : coll.Rows()) {
    assert(coll.ColumnCount() == 1);
    auto value = row.GetValue(0);
    found_count += value.GetValue<uint64_t>();
  }
  mysql_stage_set_work_completed(psi, found_count);
  thd->get_stmt_da()->set_current_row_for_condition(found_count);

  return 0;
#else
  (void)thd;
  (void)psi;
  (void)from;
  (void)to;
  (void)copy;
  (void)copy_end;
  (void)found_count;
  my_error(ER_UNKNOWN_STORAGE_ENGINE, MYF(0), "DuckDB");
  return 1;
#endif
}

int copy_data_between_duckdb_tables(THD *thd, PSI_stage_progress *psi,
                                    TABLE *from, TABLE *to,
                                    List<Create_field> &create,
                                    ulonglong *copied, ulonglong *deleted,
                                    Alter_table_ctx *alter_ctx
                                    [[maybe_unused]]) {
  int error;
  Copy_field *copy, *copy_end;
  Field **ptr;
  /*
    Fields which values need to be generated for each row, i.e. either
    generated fields or newly added fields with generated default values.
  */
  Field **gen_fields, **gen_fields_end;
  /* auto_increment_field_copied is not used. */
  sql_mode_t save_sql_mode;

  /* We do not use unit and select here. */

  /*
    If target storage engine supports atomic DDL we should not commit
    and disable transaction to let SE do proper cleanup on error/crash.
    Such engines should be smart enough to disable undo/redo logging
    for target table automatically.
    Temporary tables path doesn't employ atomic DDL support so disabling
    transaction is OK. Moreover doing so allows to not interfere with
    concurrent FLUSH TABLES WITH READ LOCK.
  */
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_prepare_alter_copy_data(thd))
    return -1;

  if (!(copy = new (thd->mem_root) Copy_field[to->s->fields]))
    return -1; /* purecov: inspected */

  if (!(gen_fields = thd->mem_root->ArrayAlloc<Field *>(
            to->s->gen_def_field_count + to->s->vfields))) {
    std::destroy_n(copy, to->s->fields);
    return -1;
  }

  if (to->file->ha_external_lock(thd, F_WRLCK)) {
    std::destroy_n(copy, to->s->fields);
    return -1;
  }

  /* DuckDB do not need to handle manage keys. */

  /* Only consider not preapred */

  /*
    We want warnings/errors about data truncation emitted when we
    copy data to new version of table.
  */
  thd->check_for_truncated_fields = CHECK_FIELD_WARN;
  thd->num_truncated_fields = 0L;

  /* Skip from file info. */

  /* DuckDB ha_start_bulk_insert do nothing, skip. */

  mysql_stage_set_work_estimated(psi, from->file->stats.records);

  save_sql_mode = thd->variables.sql_mode;

  List_iterator<Create_field> it(create);
  const Create_field *def;
  copy_end = copy;
  gen_fields_end = gen_fields;
  for (ptr = to->field; *ptr; ptr++) {
    def = it++;
    if ((*ptr)->is_gcol()) {
      /*
        Values in generated columns need to be (re)generated even for
        pre-existing columns, as they might depend on other columns,
        values in which might have changed as result of this ALTER.
        Because of this there is no sense in copying old values for
        these columns.
        TODO: Figure out if we can avoid even reading these old values
              from SE.
      */
      *(gen_fields_end++) = *ptr;
      continue;
    }
    // Array fields will be properly generated during GC update loop below
    assert(!def->is_array);
    if (def->field) {
      if (*ptr == to->next_number_field) {
        /* auto_increment_field_copied is not used. */
        /*
          If we are going to copy contents of one auto_increment column to
          another auto_increment column it is sensible to preserve zeroes.
          This condition also covers case when we are don't actually alter
          auto_increment column.
        */
        if (def->field == from->found_next_number_field)
          thd->variables.sql_mode |= MODE_NO_AUTO_VALUE_ON_ZERO;
      }
      (copy_end++)->set(*ptr, def->field);
    } else {
      /*
        New column. Add it to the array of columns requiring value
        generation if it has generated default.
      */
      if ((*ptr)->has_insert_default_general_value_expression()) {
        assert(!((*ptr)->is_gcol()));
        *(gen_fields_end++) = *ptr;
      }
    }
  }

  ulong found_count = 0;
  ulong delete_count = 0;

  /* DuckDB do case order. */

  /* DuckDB do not use RowIterator */

  /* Tell handler that we have values for all columns in the to table */
  to->use_all_columns();

  /* Do not handle prepared. */

  /* Skip create iterator. */

  thd->get_stmt_da()->reset_current_row_for_condition();

  set_column_static_defaults(to, create);

  to->file->ha_extra(HA_EXTRA_BEGIN_ALTER_COPY);

  error = copy_data_use_select_insert(thd, psi, from, to, copy, copy_end,
                                      found_count);

  /* Skip iterator reset. */
  free_io_cache(from);

  /* DuckDB do nothing in ha_end_bulk_insert. Skip. */

  to->file->ha_extra(HA_EXTRA_END_ALTER_COPY);

  DBUG_EXECUTE_IF("crash_copy_before_commit", DBUG_SUICIDE(););
  if ((!(to->file->ht->flags & HTON_SUPPORTS_ATOMIC_DDL) ||
       from->s->tmp_table) &&
      mysql_trans_commit_alter_copy_data(thd))
    error = 1;

  std::destroy_n(copy, to->s->fields);
  thd->variables.sql_mode = save_sql_mode;
  free_io_cache(from);
  *copied = found_count;
  *deleted = delete_count;
  to->file->ha_release_auto_increment();
  if (to->file->ha_external_lock(thd, F_UNLCK)) error = 1;
  if (error < 0 && to->file->ha_extra(HA_EXTRA_PREPARE_FOR_RENAME)) error = 1;
  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;
  return error > 0 ? -1 : 0;
}
