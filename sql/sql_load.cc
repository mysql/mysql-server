/*
   Copyright (c) 2000, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Copy data from a text file to table */

#include "sql/sql_load.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
// Execute_load_query_log_event,
// LOG_EVENT_UPDATE_TABLE_MAP_VERSION_F
#include <string.h>
#include <sys/types.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <sstream>

#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_macros.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/binlog/event/load_data_events.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/my_loglevel.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/strings/m_ctype.h"
#include "mysql/thread_type.h"
#include "mysql_com.h"
#include "mysqld_error.h"

#include "nulls.h"
#include "scope_guard.h"

#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"
#include "sql/binlog.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_table.h"                 // dd::table_storage_engine
#include "sql/dd/types/abstract_table.h"
#include "sql/derror.h"
#include "sql/error_handler.h"  // Ignore_error_handler
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"
#include "sql/item_timefunc.h"  // Item_func_now_local
#include "sql/log.h"
#include "sql/log_event.h"  // Delete_file_log_event,
#include "sql/mysqld.h"     // mysql_real_data_home
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/query_result.h"
#include "sql/rpl_replica.h"
#include "sql/rpl_rli.h"   // Relay_log_info
#include "sql/sql_base.h"  // fill_record_n_invoke_before_triggers
#include "sql/sql_class.h"
#include "sql/sql_data_change.h"
#include "sql/sql_error.h"
#include "sql/sql_insert.h"  // check_that_all_fields_are_given_values,
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_show.h"
#include "sql/sql_table.h"
#include "sql/sql_view.h"  // check_key_in_view
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/thd_raii.h"                  // Disable_autocommit_guard
#include "sql/thr_malloc.h"
#include "sql/transaction.h"
#include "sql/transaction_info.h"
#include "sql/trigger_def.h"
#include "sql_string.h"
#include "string_with_len.h"
#include "strxnmov.h"
#include "thr_lock.h"

#include <mysql/components/services/bulk_load_service.h>

class READ_INFO;

using std::max;
using std::min;

class XML_TAG {
 public:
  int level;
  String field;
  String value;
  XML_TAG(int l, String f, String v);
};

XML_TAG::XML_TAG(int l, String f, String v) {
  level = l;
  field.append(f);
  value.append(v);
}

#define GET (stack_pos != stack ? *--stack_pos : my_b_get(&cache))
#define PUSH(A) *(stack_pos++) = (A)

class READ_INFO {
  File file;
  uchar *buffer;      /* Buffer for read text */
  uchar *end_of_buff; /* Data in buffer ends here */
  size_t buff_length; /* Length of buffer */
  const uchar *field_term_ptr, *line_term_ptr;
  const char *line_start_ptr, *line_start_end;
  size_t field_term_length, line_term_length, enclosed_length;
  int field_term_char, line_term_char, enclosed_char, escape_char;
  int *stack, *stack_pos;
  bool found_end_of_line, start_of_line, eof;
  bool need_end_io_cache;
  IO_CACHE cache;
  int level; /* for load xml */

  size_t max_size() { return std::numeric_limits<size_t>::max() - 1; }

  size_t check_length(size_t length, size_t grow) {
    // Adding new element to the end of the buffer in amortized constant time is
    // possible only if buffer capacity grows geometrically (capacity * 2) when
    // buffer is full.
    const size_t new_length = length + std::max(length, grow);
    return ((new_length < length || new_length > max_size()) ? max_size()
                                                             : new_length);
  }

 public:
  bool error, line_truncated, found_null, enclosed;
  uchar *row_start, /* Found row starts here */
      *row_end;     /* Found row ends here */
  const CHARSET_INFO *read_charset;

  READ_INFO(File file, size_t tot_length, const CHARSET_INFO *cs,
            const String &field_term, const String &line_start,
            const String &line_term, const String &enclosed, int escape,
            bool get_it_from_net, bool is_fifo);
  ~READ_INFO();
  bool read_field();
  bool read_fixed_length();
  bool next_line();
  char unescape(char chr);
  bool terminator(const uchar *ptr, size_t length);
  bool find_start_of_fields();
  /* load xml */
  List<XML_TAG> taglist;
  int read_value(int delim, String *val);
  int read_cdata(String *val, bool *have_cdata);
  bool read_xml();
  void clear_level(int level);

  /*
    We need to force cache close before destructor is invoked to log
    the last read block
  */
  void end_io_cache() {
    ::end_io_cache(&cache);
    need_end_io_cache = false;
  }

  /*
    Either this method, or we need to make cache public
    Arg must be set from Sql_cmd_load_table::execute_inner()
    since constructor does not see either the table or THD value
  */
  void set_io_cache_arg(void *arg) { cache.arg = arg; }

  /**
    skip all data till the eof.
  */
  void skip_data_till_eof() {
    while (GET != my_b_EOF)
      ;
  }
};

/**
  Truncate to create a new table for BULK LOAD. The transaction is not
  committed and rolls back if bulk load fails.

  @param thd Current thread
  @param table_ref table reference
  @param table_def dd table
  @returns true if error
*/
bool Sql_cmd_load_table::truncate_table_for_bulk_load(
    THD *thd, Table_ref *const table_ref, dd::Table *table_def) {
  DBUG_TRACE;

  HA_CREATE_INFO dummy_create_info;
  // Create a path to the table, but without a extension
  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, table_ref->db,
                       table_ref->table_name, "", 0);

  // Truncate table, attempt to reconstruct the table.
  if (ha_create_table(thd, path, table_ref->db, table_ref->table_name,
                      &dummy_create_info, true, false, table_def) != 0) {
    return true;
  }
  return false;
}

/**
  Check bulk load parameters for limits.
  @param thd Current thread
  @returns true if error
*/
bool Sql_cmd_load_table::check_bulk_load_parameters(THD *thd) {
  DBUG_TRACE;
  /* On error, this function needs to return true. */
  const bool error = true;

  /* First check if bulk loader component is available. */
  my_h_service svc = nullptr;

  if (srv_registry->acquire("bulk_load_driver", &svc)) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Bulk Load");
    return error;
  }
  srv_registry->release(svc);

  auto sec_ctx = thd->security_context();

  if (m_bulk_source == LOAD_SOURCE_S3) {
    if (!sec_ctx->has_global_grant(STRING_WITH_LEN("LOAD_FROM_S3")).first) {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "LOAD_FROM_S3");
      return true;
    }
  }

  if (m_bulk_source == LOAD_SOURCE_URL) {
    if (!sec_ctx->has_global_grant(STRING_WITH_LEN("LOAD_FROM_URL")).first) {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "LOAD_FROM_URL");
      return true;
    }
  }
  const String *escaped = m_exchange.field.escaped;
  const String *enclosed = m_exchange.field.enclosed;

  /* Check for single byte separator. */
  if (escaped->length() > 1 || enclosed->length() > 1) {
    my_error(ER_WRONG_FIELD_TERMINATORS, MYF(0));
    return true;
  }

  /* Report problems with non-ASCII separators */
  const String *field_term = m_exchange.field.field_term;
  const String *line_term = m_exchange.line.line_term;

  if (!escaped->is_ascii() || !enclosed->is_ascii() ||
      !field_term->is_ascii() || !line_term->is_ascii()) {
    push_warning(thd, Sql_condition::SL_WARNING,
                 WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED,
                 ER_THD(thd, WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED));
  }
  return false;
}

/**
  Validate table for bulk load operation.

  @param       thd         Thread handle.
  @param       table_ref   Table_ref instance of a table.
  @param       table_def   DD table instance of a table.
  @param[out]  hton        Handlerton instance of table's SE.

  @returns false if bulk load can be executed on table.
*/
bool Sql_cmd_load_table::validate_table_for_bulk_load(
    THD *thd, Table_ref *const table_ref, dd::Table *table_def,
    handlerton **hton) {
  // Table belongs to engine supporting bulk load.
  (*hton) = nullptr;
  if (dd::table_storage_engine(thd, table_def, hton)) {
    /* Error is already raised. */
    return true;
  }
  if ((*hton) == nullptr ||
      !ha_check_storage_engine_flag(*hton, HTON_SUPPORTS_BULK_LOAD)) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Bulk Load is not supported for SE");
    return true;
  }

  /*
    Engine supporting bulk should also supports atomic DDL and
    truncate by recreate.
  */
  assert((*hton)->flags & HTON_SUPPORTS_ATOMIC_DDL);
  assert((*hton)->flags & HTON_CAN_RECREATE);

  if (is_temporary_table(table_ref)) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0),
             "Temporary Table with LOAD ALGORITHM = BULK");
    return true;
  }

  if (table_ref->is_view() || !table_ref->is_insertable() ||
      !table_ref->is_updatable()) {
    my_error(ER_NON_INSERTABLE_TABLE, MYF(0), table_ref->alias, "BULK LOAD");
    return true;
  }

  if (table_ref->table->part_info != nullptr) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Partitioned Table");
    return true;
  }

  if (!table_ref->table->file->bulk_load_check(thd)) {
    /* SE already raises the error. */
    return true;
  }

  /*
    Skip Foreign Key and Secondary Engine check. They would eventually fail
    during bulk load execution and rollback.
  */

  return false;
}

/**
  Execute BULK LOAD DATA
  @param thd Current thread.
  @returns true if error
*/
bool Sql_cmd_load_table::execute_bulk(THD *thd) {
  DBUG_TRACE;

  if (check_bulk_load_parameters(thd)) {
    return true;
  }

  /* Disallow Bulk Load if the table is locked with LOCK TABLE. */
  if (thd->locked_tables_mode != LTM_NONE) {
    my_error(ER_LOCK_OR_ACTIVE_TRANSACTION, MYF(0));
    return true;
  }

  Table_ref *table_ref = thd->lex->query_tables;

  // Acquire MDL lock on table, BACKUP_LOCK and GLOBAL lock objects.
  if (lock_table_names(thd, table_ref, nullptr,
                       thd->variables.lock_wait_timeout, 0)) {
    return true;
  }

  /* Truncate requires disabling auto commit guard. */
  const Disable_autocommit_guard autocommit_guard(thd);
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

  THD_STAGE_INFO(thd, stage_executing);

  // Acquire DD instance for modification. Truncate operation modifies DD
  // table instance.
  dd::Table *table_def = nullptr;

  if (thd->dd_client()->acquire_for_modification(
          table_ref->db, table_ref->table_name, &table_def)) {
    return true;
  }

  if (table_def == nullptr) {
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_ref->db, table_ref->table_name);
    return true;
  }

  uint counter;
  if (open_tables(thd, &table_ref, &counter, MYSQL_OPEN_HAS_MDL_LOCK)) {
    return true;
  }

  handlerton *hton = nullptr;
  if (validate_table_for_bulk_load(thd, table_ref, table_def, &hton)) {
    return true;
  }

  /*
    We need to close the table and reset the table_ref so that it can be used
    to re-open the table after truncate.
  */
  close_thread_tables(thd);
  tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table_ref->db, table_ref->table_name,
                   false);
  table_ref->table = nullptr;

  bool success = false;
  // Actions needed to cleanup before leaving scope.
  auto cleanup_guard = create_scope_guard([&]() {
    THD_STAGE_INFO(thd, stage_end);
    close_thread_tables(thd);
    // End transaction
    if (success) {
      success = !(trans_commit_stmt(thd) || trans_commit_implicit(thd));
    }
    if (!success) {
      trans_rollback_stmt(thd);
      trans_rollback_implicit(thd);
      tdc_remove_table(thd, TDC_RT_REMOVE_ALL, table_ref->db,
                       table_ref->table_name, false);
    }
    // Post DDL action for truncate. */
    if (hton != nullptr && hton->post_ddl) {
      hton->post_ddl(thd);
    }
  });

  /*
    We start bulk ingestion by truncating the table. We then continue the same
    transaction loading data and finishing with a commit or rollback.
    1. At this point the table is validated to not have any user data. It is
       thus safe to truncate.
    2. Truncate command creates a new space for the data to be loaded and the
       atomicity guarantee is provided by the truncate DDL log.
       a. Commit (Success case): Old tablespace is removed and we have the new
          one loaded with user data. The new space id is committed in DD SE
          private data.
       b. Rollback (Unsuccessful case): New tablespace with partially loaded
          data is removed and old tablespace remains. Rollback brings back the
          old tablespace ID in DD SE private data. Cached objects are removed.
  */
  if (truncate_table_for_bulk_load(thd, table_ref, table_def)) {
    my_error(ER_INTERNAL_ERROR, MYF(0), "BULK LOAD: Truncate failed");
    return true;
  }

  /*
    Open the table after truncate. Here we open the destination table, on which
    we already have an exclusive metadata lock.
  */
  if (open_tables(thd, &table_ref, &counter, MYSQL_OPEN_HAS_MDL_LOCK)) {
    return true;
  }

  size_t affected_rows = 0;
  if (!bulk_driver_service(thd, table_ref->table, affected_rows)) {
    return true;
  }

  success = true;
  char ok_message[512];
  snprintf(
      ok_message, sizeof(ok_message), ER_THD(thd, ER_LOAD_INFO),
      static_cast<long>(affected_rows), 0L, 0L,
      static_cast<long>(thd->get_stmt_da()->current_statement_cond_count()));

  my_ok(thd, affected_rows, 0LL, ok_message);
  return false;
}

bool Sql_cmd_load_table::bulk_driver_service(THD *thd, const TABLE *table,
                                             size_t &affected_rows) {
  Bulk_source src = Bulk_source::LOCAL;

  switch (m_bulk_source) {
    case LOAD_SOURCE_URL:
      src = Bulk_source::OCI;
      break;

    case LOAD_SOURCE_S3:
      src = Bulk_source::S3;
      break;

    case LOAD_SOURCE_FILE:
      src = Bulk_source::LOCAL;
      break;
  }

  std::string lowercase(m_compression_algorithm_string.str,
                        m_compression_algorithm_string.length);

  std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(),
                 ::tolower);
  Bulk_compression_algorithm compression_algorithm =
      Bulk_compression_algorithm::NONE;
  if (m_compression_algorithm_string.length == 0) {
    compression_algorithm = Bulk_compression_algorithm::NONE;
  } else if (lowercase == "zstd") {
    compression_algorithm = Bulk_compression_algorithm::ZSTD;
  } else {
    std::stringstream ss;
    ss << "Invalid compression algorithm: "
       << std::string(m_compression_algorithm_string.str,
                      m_compression_algorithm_string.length);
    my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
             ss.str().c_str());
    return false;
  }

  std::string file_name_arg(m_exchange.file_name);

  Bulk_load_file_info info(src, file_name_arg, m_file_count);

  {
    std::string error;
    if (!info.parse(error)) {
      my_error(ER_BULK_PARSER_ERROR, MYF(0), error.c_str());
      return false;
    }
  }

  if (src == Bulk_source::LOCAL) {
    char name[FN_REFLEN];

    Table_ref *const table_list = thd->lex->query_tables;
    const char *db = table_list->db;
    const char *tdb = thd->db().str ? thd->db().str : db;

    if (!dirname_length(info.m_file_prefix.c_str())) {
      strxnmov(name, FN_REFLEN - 1, mysql_real_data_home, tdb, NullS);
      (void)fn_format(name, info.m_file_prefix.c_str(), name, "",
                      MY_RELATIVE_PATH | MY_UNPACK_FILENAME);
    } else {
      (void)fn_format(
          name, info.m_file_prefix.c_str(), mysql_real_data_home, "",
          MY_RELATIVE_PATH | MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH);
    }

    if (!is_secure_file_path(name)) {
      /* Read only allowed from within dir specified by secure_file_priv */
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
      return false;
    }
    /* Replace relative path. */
    if (!test_if_hard_path(info.m_file_prefix.c_str())) {
      info.m_file_prefix.assign(name);
    }
  }

  my_h_service svc = nullptr;

  if (srv_registry->acquire("bulk_load_driver", &svc)) {
    my_error(ER_NOT_SUPPORTED_YET, MYF(0), "Bulk Load");
    return false;
  }

  auto load_driver = reinterpret_cast<SERVICE_TYPE(bulk_load_driver) *>(svc);

  auto load_handle = load_driver->create_bulk_loader(
      thd, thd->thread_id(), table, src,
      m_exchange.cs ? m_exchange.cs : thd->variables.collation_database);

  /* Set schema, table, file name string options. */
  std::string schema_name(table->s->db.str, table->s->db.length);
  load_driver->set_string(load_handle, Bulk_string::SCHEMA_NAME, schema_name);

  std::string table_name(table->s->table_name.str, table->s->table_name.length);
  load_driver->set_string(load_handle, Bulk_string::TABLE_NAME, table_name);

  load_driver->set_string(load_handle, Bulk_string::FILE_PREFIX,
                          info.m_file_prefix);

  if (info.m_file_suffix.has_value()) {
    load_driver->set_string(load_handle, Bulk_string::FILE_SUFFIX,
                            info.m_file_suffix.value());
  }

  if (!info.m_appendtolastprefix.empty()) {
    load_driver->set_string(load_handle, Bulk_string::APPENDTOLASTPREFIX,
                            info.m_appendtolastprefix);
  }

  /* If not set by user, default is assigned from default_field_term. */
  const String *field_term = m_exchange.field.field_term;
  std::string col_term(field_term->ptr(), field_term->length());

  load_driver->set_string(load_handle, Bulk_string::COLUMN_TERM, col_term);

  /* If not set by user, default is assigned from default_line_term. */
  const String *line_term = m_exchange.line.line_term;
  std::string row_term(line_term->ptr(), line_term->length());
  load_driver->set_string(load_handle, Bulk_string::ROW_TERM, row_term);

  /* Set boolean options. */
  load_driver->set_condition(load_handle, Bulk_condition::ORDERED_DATA,
                             m_ordered_data);
  load_driver->set_condition(load_handle, Bulk_condition::OPTIONAL_ENCLOSE,
                             m_exchange.field.opt_enclosed);
  load_driver->set_condition(load_handle, Bulk_condition::DRYRUN,
                             info.m_is_dryrun);

  /* Set size options. */
  load_driver->set_size(load_handle, Bulk_size::CONCURRENCY, m_concurrency);
  load_driver->set_size(load_handle, Bulk_size::COUNT_FILES, m_file_count);
  load_driver->set_size(load_handle, Bulk_size::START_INDEX,
                        info.m_start_index);
  load_driver->set_size(load_handle, Bulk_size::COUNT_ROW_SKIP,
                        m_exchange.skip_lines);
  load_driver->set_size(load_handle, Bulk_size::COUNT_COLUMNS,
                        table->s->fields);
  load_driver->set_size(load_handle, Bulk_size::MEMORY, m_memory_size);

  /* Set escape and enclosing character options */
  const String *escaped = m_exchange.field.escaped;
  const String *enclosed = m_exchange.field.enclosed;
  if (m_exchange.escaped_given() && !escaped->is_empty()) {
    load_driver->set_char(load_handle, Bulk_char::ESCAPE_CHAR, (*escaped)[0]);
  }
  if (!enclosed->is_empty()) {
    load_driver->set_char(load_handle, Bulk_char::ENCLOSE_CHAR, (*enclosed)[0]);
  }

  load_driver->set_compression_algorithm(load_handle, compression_algorithm);

  bool success = load_driver->load(load_handle, affected_rows);

  load_driver->drop_bulk_loader(thd, load_handle);

  srv_registry->release(svc);

  return success;
}

/**
  Execute LOAD DATA query

  @param thd                 Current thread.
  @param handle_duplicates   Indicates whenever we should emit error or
                             replace row if we will meet duplicates.

  @returns true if error
*/
bool Sql_cmd_load_table::execute_inner(THD *thd,
                                       enum enum_duplicates handle_duplicates) {
  char name[FN_REFLEN];
  File file;
  bool error = false;
  const String *field_term = m_exchange.field.field_term;
  const String *escaped = m_exchange.field.escaped;
  const String *enclosed = m_exchange.field.enclosed;
  bool is_fifo = false;
  Query_block *select = thd->lex->query_block;
  LOAD_FILE_INFO lf_info;
  THD::killed_state killed_status = THD::NOT_KILLED;
  bool is_concurrent;
  bool transactional_table;
  Table_ref *const table_list = thd->lex->query_tables;
  const char *db = table_list->db;  // This is never null
  /*
    If path for file is not defined, we will use the current database.
    If this is not set, we will use the directory where the table to be
    loaded is located
  */
  const char *tdb = thd->db().str ? thd->db().str : db;  // Result is never null
  ulong skip_lines = m_exchange.skip_lines;
  DBUG_TRACE;

  /*
    Bug #34283
    mysqlbinlog leaves tmpfile after termination if binlog contains
    load data infile, so in mixed mode we go to row-based for
    avoiding the problem.
  */
  thd->set_current_stmt_binlog_format_row_if_mixed();

  if (escaped->length() > 1 || enclosed->length() > 1) {
    my_error(ER_WRONG_FIELD_TERMINATORS, MYF(0));
    return true;
  }

  /* Report problems with non-ascii separators */
  if (!escaped->is_ascii() || !enclosed->is_ascii() ||
      !field_term->is_ascii() || !m_exchange.line.line_term->is_ascii() ||
      !m_exchange.line.line_start->is_ascii()) {
    push_warning(thd, Sql_condition::SL_WARNING,
                 WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED,
                 ER_THD(thd, WARN_NON_ASCII_SEPARATOR_NOT_IMPLEMENTED));
  }

  if (open_and_lock_tables(thd, table_list, 0)) return true;

  THD_STAGE_INFO(thd, stage_executing);
  if (select->setup_tables(thd, table_list, false)) return true;

  if (run_before_dml_hook(thd)) return true;

  if (table_list->is_view() && select->resolve_placeholder_tables(thd, false))
    return true; /* purecov: inspected */

  Table_ref *const insert_table_ref =
      table_list->is_updatable() &&  // View must be updatable
              !table_list
                   ->is_multiple_tables() &&  // Multi-table view not allowed
              !table_list->is_derived()
          ?  // derived tables not allowed
          table_list->updatable_base_table()
          : nullptr;

  if (insert_table_ref == nullptr ||
      check_key_in_view(thd, table_list, insert_table_ref)) {
    my_error(ER_NON_UPDATABLE_TABLE, MYF(0), table_list->alias, "LOAD");
    return true;
  }
  if (select->derived_table_count &&
      select->check_view_privileges(thd, INSERT_ACL, SELECT_ACL))
    return true; /* purecov: inspected */

  if (table_list->is_merged()) {
    if (table_list->prepare_check_option(thd)) return true;

    if (handle_duplicates == DUP_REPLACE &&
        table_list->prepare_replace_filter(thd))
      return true;
  }

  // Pass the check option down to the underlying table:
  insert_table_ref->check_option = table_list->check_option;
  /*
    Let us emit an error if we are loading data to table which is used
    in subselect in SET clause like we do it for INSERT.

    The main thing to fix to remove this restriction is to ensure that the
    table is marked to be 'used for insert' in which case we should never
    mark this table as 'const table' (ie, one that has only one row).
  */
  if (unique_table(insert_table_ref, table_list->next_global, false)) {
    my_error(ER_UPDATE_TABLE_USED, MYF(0), table_list->table_name);
    return true;
  }

  TABLE *const table = insert_table_ref->table;

  for (Field **cur_field = table->field; *cur_field; ++cur_field)
    (*cur_field)->reset_warnings();

  transactional_table = table->file->has_transactions();
  is_concurrent =
      (table_list->lock_descriptor().type == TL_WRITE_CONCURRENT_INSERT);

  if (m_opt_fields_or_vars.empty()) {
    Field_iterator_table_ref field_iterator;
    field_iterator.set(table_list);
    for (; !field_iterator.end_of_fields(); field_iterator.next()) {
      // Do not include user hidden fields.
      if (field_iterator.field() != nullptr &&
          field_iterator.field()->is_hidden())
        continue;

      Item *item;
      if (!(item = field_iterator.create_item(thd))) return true;

      if (item->field_for_view_update() == nullptr) {
        my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), item->item_name.ptr());
        return true;
      }
      m_opt_fields_or_vars.push_back(item->real_item());
    }
    bitmap_set_all(table->write_set);
    /*
      Let us also prepare SET clause, although it is probably empty
      in this case.
    */
    if (setup_fields(thd, /*want_privilege=*/INSERT_ACL,
                     /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                     /*column_update=*/true, /*typed_items=*/nullptr,
                     &m_opt_set_fields, Ref_item_array()) ||
        setup_fields(thd, /*want_privilege=*/SELECT_ACL,
                     /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                     /*column_update=*/false, /*typed_items=*/nullptr,
                     &m_opt_set_exprs, Ref_item_array()))
      return true;
  } else {  // Part field list
    /*
      Because m_opt_fields_or_vars may contain user variables,
      pass false for column_update in first call below.
    */
    if (setup_fields(thd, /*want_privilege=*/INSERT_ACL,
                     /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                     /*column_update=*/false, /*typed_items=*/nullptr,
                     &m_opt_fields_or_vars, Ref_item_array()) ||
        setup_fields(thd, /*want_privilege=*/INSERT_ACL,
                     /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                     /*column_update=*/true, /*typed_items=*/nullptr,
                     &m_opt_set_fields, Ref_item_array()))
      return true;

    /*
      Special updatability test is needed because m_opt_fields_or_vars may
      contain a mix of column references and user variables.
    */
    for (Item *item : m_opt_fields_or_vars) {
      if ((item->type() == Item::FIELD_ITEM ||
           item->type() == Item::REF_ITEM) &&
          item->field_for_view_update() == nullptr) {
        my_error(ER_NONUPDATEABLE_COLUMN, MYF(0), item->item_name.ptr());
        return true;
      }
      if (item->type() == Item::STRING_ITEM) {
        /*
          This item represents a user variable. Create a new item with the
          same name that can be added to LEX::set_var_list. This ensures
          that corresponding Item_func_get_user_var items are resolved as
          non-const items.
        */
        Item_func_set_user_var *user_var =
            new (thd->mem_root) Item_func_set_user_var(item->item_name, item);
        if (user_var == nullptr) return true;
        thd->lex->set_var_list.push_back(user_var);
      }
    }

    // Consider the following table:
    //
    //   CREATE TABLE t1 (x DOUBLE, y DOUBLE, g POINT SRID 4326 NOT NULL);
    //
    // If the user wants to load a file which only contains two values (x and y
    // coordinates), it is possible to do it by executing the following
    // statement:
    //
    //  LOAD DATA INFILE 'data' (@x, @y)
    //    SET x = @x, y = @y, g = ST_SRID(POINT(@x, @y));
    //
    // However, the columns that are specified in the SET clause are only marked
    // in the write set, and not in fields_set_during_insert. The latter is the
    // bitmap used during check_that_all_fields_are_given_values(), so we need
    // to copy the bits from the write set over to said bitmap. If not, the
    // server will return an error saying that column 'g' doesn't have a default
    // value.
    bitmap_union(table->fields_set_during_insert, table->write_set);

    if (check_that_all_fields_are_given_values(thd, table, table_list))
      return true;
    /* Fix the expressions in SET clause */
    if (setup_fields(thd, /*want_privilege=*/SELECT_ACL,
                     /*allow_sum_func=*/false, /*split_sum_funcs=*/false,
                     /*column_update=*/false, /*typed_items=*/nullptr,
                     &m_opt_set_exprs, Ref_item_array()))
      return true;
  }

  const int escape_char =
      (escaped->length() &&
       (m_exchange.escaped_given() ||
        !(thd->variables.sql_mode & MODE_NO_BACKSLASH_ESCAPES)))
          ? (*escaped)[0]
          : INT_MAX;

  /*
    * LOAD DATA INFILE fff INTO TABLE xxx SET columns2
    sets all columns, except if file's row lacks some: in that case,
    defaults are set by read_fixed_length() and read_sep_field(),
    not by COPY_INFO.
    * LOAD DATA INFILE fff INTO TABLE xxx (columns1) SET columns2=
    may need a default for columns other than columns1 and columns2.
  */
  const bool manage_defaults = !m_opt_fields_or_vars.empty();
  COPY_INFO info(COPY_INFO::INSERT_OPERATION, &m_opt_fields_or_vars,
                 &m_opt_set_fields, manage_defaults, handle_duplicates,
                 escape_char);

  if (info.add_function_default_columns(table, table->write_set)) return true;

  if (table->triggers) {
    if (table->triggers->mark_fields(TRG_EVENT_INSERT)) return true;
  }

  prepare_triggers_for_insert_stmt(thd, table);

  size_t tot_length = 0;
  bool use_blobs = false, use_vars = false;

  for (Item *item : m_opt_fields_or_vars) {
    const Item *real_item = item->real_item();

    if (real_item->type() == Item::FIELD_ITEM) {
      const Field *field = down_cast<const Item_field *>(real_item)->field;
      if (field->is_flag_set(BLOB_FLAG)) {
        use_blobs = true;
        tot_length += 4096;  // Will be extended if needed
      } else {
        tot_length += field->field_length;
      }
    } else if (item->type() == Item::STRING_ITEM) {
      use_vars = true;
    }
  }
  if (use_blobs && m_exchange.line.line_term->is_empty() &&
      field_term->is_empty()) {
    my_error(ER_BLOBS_AND_NO_TERMINATED, MYF(0));
    return true;
  }
  if (use_vars && !field_term->length() && !enclosed->length()) {
    my_error(ER_LOAD_FROM_FIXED_SIZE_ROWS_TO_VAR, MYF(0));
    return true;
  }

  // Statement is fully prepared:
  thd->lex->unit->set_prepared();

  // Start execution:
  thd->lex->set_exec_started();

  if (m_is_local_file) {
    (void)net_request_file(thd->get_protocol_classic()->get_net(),
                           m_exchange.file_name);
    file = -1;
  } else {
    if (!dirname_length(m_exchange.file_name)) {
      strxnmov(name, FN_REFLEN - 1, mysql_real_data_home, tdb, NullS);
      (void)fn_format(name, m_exchange.file_name, name, "",
                      MY_RELATIVE_PATH | MY_UNPACK_FILENAME);
    } else {
      (void)fn_format(
          name, m_exchange.file_name, mysql_real_data_home, "",
          MY_RELATIVE_PATH | MY_UNPACK_FILENAME | MY_RETURN_REAL_PATH);
    }

    if ((thd->system_thread &
         (SYSTEM_THREAD_SLAVE_SQL | SYSTEM_THREAD_SLAVE_WORKER)) != 0) {
      Relay_log_info *rli = thd->rli_slave->get_c_rli();

      if (strncmp(rli->slave_patternload_file, name,
                  rli->slave_patternload_file_size)) {
        /*
          LOAD DATA INFILE in the slave SQL Thread can only read from
          --replica-load-tmpdir". This should never happen. Please, report a
          bug.
        */
        LogErr(ERROR_LEVEL, ER_LOAD_DATA_INFILE_FAILED_IN_UNEXPECTED_WAY);
        my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--replica-load-tmpdir");
        return true;
      }
    } else if (!is_secure_file_path(name)) {
      /* Read only allowed from within dir specified by secure_file_priv */
      my_error(ER_OPTION_PREVENTS_STATEMENT, MYF(0), "--secure-file-priv");
      return true;
    }

#if !defined(_WIN32)
    MY_STAT stat_info;
    if (!my_stat(name, &stat_info, MYF(MY_WME))) return true;

    // if we are not in slave thread, the file must be:
    if (!thd->slave_thread &&
        !((stat_info.st_mode & S_IFLNK) != S_IFLNK &&   // symlink
          ((stat_info.st_mode & S_IFREG) == S_IFREG ||  // regular file
           (stat_info.st_mode & S_IFIFO) == S_IFIFO)))  // named pipe
    {
      my_error(ER_TEXTFILE_NOT_READABLE, MYF(0), name);
      return true;
    }
    if ((stat_info.st_mode & S_IFIFO) == S_IFIFO) is_fifo = true;
#endif
    if ((file = mysql_file_open(key_file_load, name, O_RDONLY, MYF(MY_WME))) <
        0)

      return true;
  }

  READ_INFO read_info(
      file, tot_length,
      m_exchange.cs ? m_exchange.cs : thd->variables.collation_database,
      *field_term, *m_exchange.line.line_start, *m_exchange.line.line_term,
      *enclosed, info.escape_char, m_is_local_file, is_fifo);
  if (read_info.error) {
    if (file >= 0) mysql_file_close(file, MYF(0));  // no files in net reading
    return true;                                    // Can't allocate buffers
  }

  if (mysql_bin_log.is_open()) {
    lf_info.thd = thd;
    lf_info.logged_data_file = false;
    lf_info.last_pos_in_file = HA_POS_ERROR;
    lf_info.log_delayed = transactional_table;
    read_info.set_io_cache_arg((void *)&lf_info);
  }

  thd->check_for_truncated_fields = CHECK_FIELD_WARN;
  thd->num_truncated_fields = 0L;
  /* Skip lines if there is a line terminator */
  if (m_exchange.line.line_term->length() &&
      m_exchange.filetype != FILETYPE_XML) {
    /* m_exchange.skip_lines needs to be preserved for logging */
    while (skip_lines > 0) {
      skip_lines--;
      if (read_info.next_line()) break;
    }
  }

  if (!(error = read_info.error)) {
    table->next_number_field = table->found_next_number_field;
    if (thd->lex->is_ignore() || handle_duplicates == DUP_REPLACE)
      table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
    if (handle_duplicates == DUP_REPLACE &&
        (!table->triggers || !table->triggers->has_delete_triggers()))
      table->file->ha_extra(HA_EXTRA_WRITE_CAN_REPLACE);
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES)
      table->file->ha_start_bulk_insert((ha_rows)0);
    table->copy_blobs = true;

    if (m_exchange.filetype == FILETYPE_XML) /* load xml */
      error =
          read_xml_field(thd, info, insert_table_ref, read_info, skip_lines);
    else if (!field_term->length() && !enclosed->length())
      error =
          read_fixed_length(thd, info, insert_table_ref, read_info, skip_lines);
    else
      error = read_sep_field(thd, info, insert_table_ref, read_info, *enclosed,
                             skip_lines);
    if (thd->locked_tables_mode <= LTM_LOCK_TABLES &&
        table->file->ha_end_bulk_insert() && !error) {
      table->file->print_error(my_errno(), MYF(0));
      error = true;
    }
    table->next_number_field = nullptr;
  }
  if (file >= 0) mysql_file_close(file, MYF(0));
  free_blobs(table); /* if pack_blob was used */
  table->copy_blobs = false;
  thd->check_for_truncated_fields = CHECK_FIELD_IGNORE;
  /*
     simulated killing in the middle of per-row loop
     must be effective for binlogging
  */
  DBUG_EXECUTE_IF("simulate_kill_bug27571", {
    error = true;
    thd->killed = THD::KILL_QUERY;
  };);

  killed_status = error ? thd->killed.load() : THD::NOT_KILLED;

  if (error) {
    if (m_is_local_file) read_info.skip_data_till_eof();

    if (mysql_bin_log.is_open()) {
      {
        /*
          Make sure last block (the one which caused the error) gets
          logged.  This is needed because otherwise after write of (to
          the binlog, not to read_info (which is a cache))
          Delete_file_log_event the bad block will remain in read_info
          (because pre_read is not called at the end of the last
          block; remember pre_read is called whenever a new block is
          read from disk).  At the end of Sql_cmd_load_table::execute_inner(),
          the destructor of read_info will call end_io_cache() which will flush
          read_info, so we will finally have this in the binlog:

          Append_block # The last successful block
          Delete_file
          Append_block # The failing block
          which is nonsense.
          Or could also be (for a small file)
          Create_file  # The failing block
          which is nonsense (Delete_file is not written in this case, because:
          Create_file has not been written, so Delete_file is not written, then
          when read_info is destroyed end_io_cache() is called which writes
          Create_file.
        */
        read_info.end_io_cache();
        /* If the file was not empty, wrote_create_file is true */
        if (lf_info.logged_data_file) {
          const int errcode =
              query_error_code(thd, killed_status == THD::NOT_KILLED);

          /* since there is already an error, the possible error of
             writing binary log will be ignored */
          if (thd->get_transaction()->cannot_safely_rollback(
                  Transaction_ctx::STMT))
            (void)write_execute_load_query_log_event(
                thd, table_list->db, table_list->table_name, is_concurrent,
                handle_duplicates, transactional_table, errcode);
          else {
            Delete_file_log_event d(thd, db, transactional_table);
            (void)mysql_bin_log.write_event(&d);
          }
        }
      }
    }
    error = true;  // Error on read
    goto err;
  }

  snprintf(name, sizeof(name), ER_THD(thd, ER_LOAD_INFO),
           (long)info.stats.records, (long)info.stats.deleted,
           (long)(info.stats.records - info.stats.copied),
           (long)thd->get_stmt_da()->current_statement_cond_count());

  if (mysql_bin_log.is_open()) {
    /*
      We need to do the job that is normally done inside
      binlog_query() here, which is to ensure that the pending event
      is written before tables are unlocked and before any other
      events are written.  We also need to update the table map
      version for the binary log to mark that table maps are invalid
      after this point.
     */
    if (thd->is_current_stmt_binlog_format_row())
      error = thd->binlog_flush_pending_rows_event(true, transactional_table);
    else {
      /*
        As already explained above, we need to call end_io_cache() or the last
        block will be logged only after Execute_load_query_log_event (which is
        wrong), when read_info is destroyed.
      */
      read_info.end_io_cache();
      if (lf_info.logged_data_file) {
        const int errcode =
            query_error_code(thd, killed_status == THD::NOT_KILLED);
        error = write_execute_load_query_log_event(
            thd, table_list->db, table_list->table_name, is_concurrent,
            handle_duplicates, transactional_table, errcode);
      }
    }
    if (error) goto err;
  }

  /* ok to client sent only after binlog write and engine commit */
  my_ok(thd, info.stats.copied + info.stats.deleted, 0L, name);
err:
  assert(table->file->has_transactions() ||
         !(info.stats.copied || info.stats.deleted) ||
         thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT));
  table->file->ha_release_auto_increment();
  return error;
}

/**
  @note Not a very useful function; just to avoid duplication of code

  @returns true if error
*/
bool Sql_cmd_load_table::write_execute_load_query_log_event(
    THD *thd, const char *db_arg, const char *table_name_arg,
    bool is_concurrent, enum enum_duplicates duplicates,
    bool transactional_table, int errcode) {
  const char *tbl = table_name_arg;
  const char *tdb = (thd->db().str != nullptr ? thd->db().str : db_arg);
  const String *query = nullptr;
  String string_buf;
  size_t fname_start = 0;
  size_t fname_end = 0;

  if (thd->db().str == nullptr || strcmp(db_arg, thd->db().str)) {
    /*
      If used database differs from table's database,
      prefix table name with database name so that it
      becomes a FQ name.
     */
    string_buf.set_charset(system_charset_info);
    append_identifier(thd, &string_buf, db_arg, strlen(db_arg));
    string_buf.append(".");
  }
  append_identifier(thd, &string_buf, table_name_arg, strlen(table_name_arg));
  tbl = string_buf.c_ptr_safe();
  Load_query_generator gen(thd, &m_exchange, tdb, tbl, is_concurrent,
                           duplicates == DUP_REPLACE, thd->lex->is_ignore());
  query = gen.generate(&fname_start, &fname_end);

  Execute_load_query_log_event e(
      thd, query->ptr(), query->length(), fname_start, fname_end,
      (duplicates == DUP_REPLACE)
          ? mysql::binlog::event::LOAD_DUP_REPLACE
          : (thd->lex->is_ignore() ? mysql::binlog::event::LOAD_DUP_IGNORE
                                   : mysql::binlog::event::LOAD_DUP_ERROR),
      transactional_table, false, false, errcode);

  return mysql_bin_log.write_event(&e);
}

namespace {
/**
  Checks if an item is a hidden generated column.

  @param table       Pointer to TABLE object
  @param item        Item to check

  @returns true if checked item is a hidden generated column.
*/
inline bool is_hidden_generated_column(TABLE *table, Item *item) {
  Item *real_item = item->real_item();
  if (table->has_gcol() && real_item->type() == Item::FIELD_ITEM) {
    const Field *field = down_cast<Item_field *>(real_item)->field;
    if (bitmap_is_set(&table->fields_for_functional_indexes,
                      field->field_index()))
      return true;
  }
  return false;
}
}  // namespace

/**
  Read of rows of fixed size + optional garbage + optional newline

  @param thd         Pointer to THD object
  @param info        Pointer to COPY_INFO object
  @param table_list  Pointer to Table_ref object
  @param read_info   Pointer to READ_INFO object
  @param skip_lines  Number of ignored lines
                     at the start of the file.

  @returns true if error
*/
bool Sql_cmd_load_table::read_fixed_length(THD *thd, COPY_INFO &info,
                                           Table_ref *table_list,
                                           READ_INFO &read_info,
                                           ulong skip_lines) {
  TABLE *table = table_list->table;
  bool err;
  DBUG_TRACE;

  while (!read_info.read_fixed_length()) {
    if (thd->killed) {
      thd->send_kill_message();
      return true;
    }
    if (skip_lines) {
      /*
        We could implement this with a simple seek if:
        - We are not using DATA INFILE LOCAL
        - escape character is  ""
        - line starting prefix is ""
      */
      skip_lines--;
      continue;
    }
    uchar *pos = read_info.row_start;

    restore_record(table, s->default_values);
    /*
      Check whether default values of the fields not specified in column list
      are correct or not.
    */
    if (validate_default_values_of_unset_fields(thd, table)) {
      read_info.error = true;
      break;
    }

    const Autoinc_field_has_explicit_non_null_value_reset_guard after_each_row(
        table);

    for (Item *item : m_opt_fields_or_vars) {
      // Skip hidden generated columns.
      if (is_hidden_generated_column(table, item)) continue;
      /*
        There is no variables in fields_vars list in this format so
        this conversion is safe (no need to check for STRING_ITEM).
      */
      assert(item->real_item()->type() == Item::FIELD_ITEM);
      Item_field *sql_field = static_cast<Item_field *>(item->real_item());
      Field *field = sql_field->field;
      if (field == table->next_number_field)
        table->autoinc_field_has_explicit_non_null_value = true;
      /*
        No fields specified in fields_vars list can be null in this format.
        Mark field as not null, we should do this for each row because of
        restore_record...
      */
      field->set_notnull();

      if (pos == read_info.row_end) {
        thd->num_truncated_fields++; /* Not enough fields */
        push_warning_printf(thd, Sql_condition::SL_WARNING,
                            ER_WARN_TOO_FEW_RECORDS,
                            ER_THD(thd, ER_WARN_TOO_FEW_RECORDS),
                            thd->get_stmt_da()->current_row_for_condition());
        if (thd->is_error()) return true;
        if (field->type() == FIELD_TYPE_TIMESTAMP && !field->is_nullable()) {
          // Specific of TIMESTAMP NOT NULL: set to CURRENT_TIMESTAMP.
          Item_func_now_local::store_in(field);
        }
      } else {
        uint length;
        uchar save_chr;
        if ((length = (uint)(read_info.row_end - pos)) > field->field_length)
          length = field->field_length;
        save_chr = pos[length];
        pos[length] = '\0';  // Safeguard aganst malloc
        field->store((char *)pos, length, read_info.read_charset);
        pos[length] = save_chr;
        if ((pos += length) > read_info.row_end)
          pos = read_info.row_end; /* Fills rest with space */
      }
    }
    if (pos != read_info.row_end) {
      thd->num_truncated_fields++; /* Too long row */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER_THD(thd, ER_WARN_TOO_MANY_RECORDS),
                          thd->get_stmt_da()->current_row_for_condition());
    }

    if (thd->killed || fill_record_n_invoke_before_triggers(
                           thd, &info, m_opt_set_fields, m_opt_set_exprs, table,
                           TRG_EVENT_INSERT, table->s->fields, true, nullptr))
      return true;

    switch (table_list->view_check_option(thd)) {
      case VIEW_CHECK_SKIP:
        read_info.next_line();
        goto continue_loop;
      case VIEW_CHECK_ERROR:
        return true;
    }

    if (invoke_table_check_constraints(thd, table)) {
      if (thd->is_error()) return true;
      // continue when IGNORE clause is used.
      read_info.next_line();
      goto continue_loop;
    }

    err = write_record(thd, table, &info, nullptr);
    if (err) return true;

    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())  // Skip to next line
      break;
    if (read_info.line_truncated) {
      thd->num_truncated_fields++; /* Too long row */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER_THD(thd, ER_WARN_TOO_MANY_RECORDS),
                          thd->get_stmt_da()->current_row_for_condition());
    }
    thd->get_stmt_da()->inc_current_row_for_condition();
  continue_loop:;
  }
  return read_info.error;
}

class Field_tmp_nullability_guard {
 public:
  explicit Field_tmp_nullability_guard(Item *item) : m_field(nullptr) {
    if (item->type() == Item::FIELD_ITEM) {
      m_field = ((Item_field *)item)->field;
      /*
        Enable temporary nullability for items that corresponds
        to table fields.
      */
      m_field->set_tmp_nullable();
    }
  }

  ~Field_tmp_nullability_guard() {
    if (m_field) m_field->reset_tmp_nullable();
  }

 private:
  Field *m_field;
};

/**
  Read rows in delimiter-separated formats.

  @param thd         Pointer to THD object
  @param info        Pointer to COPY_INFO object
  @param table_list  Pointer to Table_ref object
  @param read_info   Pointer to READ_INFO object
  @param enclosed    ENCLOSED BY character
  @param skip_lines  Number of ignored lines
                     at the start of the file.

  @returns true if error
*/
bool Sql_cmd_load_table::read_sep_field(THD *thd, COPY_INFO &info,
                                        Table_ref *table_list,
                                        READ_INFO &read_info,
                                        const String &enclosed,
                                        ulong skip_lines) {
  TABLE *table = table_list->table;
  size_t enclosed_length;
  bool err;
  DBUG_TRACE;

  enclosed_length = enclosed.length();

  for (;;) {
    if (thd->killed) {
      thd->send_kill_message();
      return true;
    }

    restore_record(table, s->default_values);
    /*
      Check whether default values of the fields not specified in column list
      are correct or not.
    */
    if (validate_default_values_of_unset_fields(thd, table)) {
      read_info.error = true;
      break;
    }

    const Autoinc_field_has_explicit_non_null_value_reset_guard after_each_row(
        table);

    auto it = m_opt_fields_or_vars.begin();
    for (; it != m_opt_fields_or_vars.end(); ++it) {
      Item *item = *it;
      uint length;
      uchar *pos;
      Item *real_item;

      // Skip hidden generated columns.
      if (is_hidden_generated_column(table, item)) continue;

      if (read_info.read_field()) break;

      /* If this line is to be skipped we don't want to fill field or var */
      if (skip_lines) continue;

      pos = read_info.row_start;
      length = (uint)(read_info.row_end - pos);

      real_item = item->real_item();

      Field_tmp_nullability_guard fld_tmp_nullability_guard(real_item);

      if ((!read_info.enclosed && (enclosed_length && length == 4 &&
                                   !memcmp(pos, STRING_WITH_LEN("NULL")))) ||
          (length == 1 && read_info.found_null)) {
        if (real_item->type() == Item::FIELD_ITEM) {
          Field *field = ((Item_field *)real_item)->field;
          if (field->reset())  // Set to 0
          {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0), field->field_name,
                     thd->get_stmt_da()->current_row_for_condition());
            return true;
          }
          if (!field->is_nullable() && field->type() == FIELD_TYPE_TIMESTAMP) {
            // Specific of TIMESTAMP NOT NULL: set to CURRENT_TIMESTAMP.
            Item_func_now_local::store_in(field);
          } else {
            /*
              Set field to NULL. Later we will clear temporary nullability flag
              and check NOT NULL constraint.
            */
            field->set_null();
          }
        } else if (item->type() == Item::STRING_ITEM) {
          assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
          ((Item_user_var_as_out_param *)item)
              ->set_null_value(read_info.read_charset);
        }

        continue;
      }

      if (real_item->type() == Item::FIELD_ITEM) {
        Field *field = ((Item_field *)real_item)->field;
        field->set_notnull();
        read_info.row_end[0] = 0;  // Safe to change end marker
        if (field == table->next_number_field)
          table->autoinc_field_has_explicit_non_null_value = true;
        field->store((char *)pos, length, read_info.read_charset);
      } else if (item->type() == Item::STRING_ITEM) {
        assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
        ((Item_user_var_as_out_param *)item)
            ->set_value((char *)pos, length, read_info.read_charset);
      }
    }

    if (thd->is_error()) read_info.error = true;

    if (read_info.error) break;
    if (skip_lines) {
      skip_lines--;
      continue;
    }
    if (it != m_opt_fields_or_vars.end()) {
      /* Have not read any field, thus input file is simply ended */
      if (it == m_opt_fields_or_vars.begin()) break;

      for (; it != m_opt_fields_or_vars.end(); ++it) {
        Item *item = *it;
        Item *real_item = item->real_item();
        if (real_item->type() == Item::FIELD_ITEM) {
          Field *field = ((Item_field *)real_item)->field;
          /*
            We set to 0. But if the field is DEFAULT NULL, the "null bit"
            turned on by restore_record() above remains so field will be NULL.
          */
          if (field->reset()) {
            my_error(ER_WARN_NULL_TO_NOTNULL, MYF(0), field->field_name,
                     thd->get_stmt_da()->current_row_for_condition());
            return true;
          }
          if (field->type() == FIELD_TYPE_TIMESTAMP && !field->is_nullable())
            // Specific of TIMESTAMP NOT NULL: set to CURRENT_TIMESTAMP.
            Item_func_now_local::store_in(field);
          /*
            QQ: We probably should not throw warning for each field.
            But how about intention to always have the same number
            of warnings in THD::num_truncated_fields (and get rid of
            num_truncated_fields in the end?)
          */
          thd->num_truncated_fields++;
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_TOO_FEW_RECORDS,
                              ER_THD(thd, ER_WARN_TOO_FEW_RECORDS),
                              thd->get_stmt_da()->current_row_for_condition());
          if (thd->is_error()) return true;
        } else if (item->type() == Item::STRING_ITEM) {
          assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
          ((Item_user_var_as_out_param *)item)
              ->set_null_value(read_info.read_charset);
        }
      }
    }

    if (thd->killed || fill_record_n_invoke_before_triggers(
                           thd, &info, m_opt_set_fields, m_opt_set_exprs, table,
                           TRG_EVENT_INSERT, table->s->fields, true, nullptr))
      return true;

    if (!table->triggers) {
      /*
        If there is no trigger for the table then check the NOT NULL constraint
        for every table field.

        For the table that has BEFORE-INSERT trigger installed checking for
        NOT NULL constraint is done inside function
        fill_record_n_invoke_before_triggers() after all trigger instructions
        has been executed.
      */
      for (Item *item : m_opt_fields_or_vars) {
        Item *real_item = item->real_item();
        if (real_item->type() == Item::FIELD_ITEM)
          ((Item_field *)real_item)
              ->field->check_constraints(ER_WARN_NULL_TO_NOTNULL);
      }
    }

    if (thd->is_error()) return true;

    switch (table_list->view_check_option(thd)) {
      case VIEW_CHECK_SKIP:
        read_info.next_line();
        goto continue_loop;
      case VIEW_CHECK_ERROR:
        return true;
    }

    if (invoke_table_check_constraints(thd, table)) {
      if (thd->is_error()) return true;
      // continue when IGNORE clause is used.
      read_info.next_line();
      goto continue_loop;
    }

    err = write_record(thd, table, &info, nullptr);
    if (err) return true;
    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    if (read_info.next_line())  // Skip to next line
      break;
    if (read_info.line_truncated) {
      thd->num_truncated_fields++; /* Too long row */
      push_warning_printf(thd, Sql_condition::SL_WARNING,
                          ER_WARN_TOO_MANY_RECORDS,
                          ER_THD(thd, ER_WARN_TOO_MANY_RECORDS),
                          thd->get_stmt_da()->current_row_for_condition());
      if (thd->killed) return true;
    }
    thd->get_stmt_da()->inc_current_row_for_condition();
  continue_loop:;
  }
  return read_info.error;
}

/**
  Read rows in xml format

  @param thd         Pointer to THD object
  @param info        Pointer to COPY_INFO object
  @param table_list  Pointer to Table_ref object
  @param read_info   Pointer to READ_INFO object
  @param skip_lines  Number of ignored lines
                     at the start of the file.

  @returns true if error
*/
bool Sql_cmd_load_table::read_xml_field(THD *thd, COPY_INFO &info,
                                        Table_ref *table_list,
                                        READ_INFO &read_info,
                                        ulong skip_lines) {
  TABLE *table = table_list->table;
  const CHARSET_INFO *cs = read_info.read_charset;
  DBUG_TRACE;

  for (;;) {
    if (thd->killed) {
      thd->send_kill_message();
      return true;
    }

    // read row tag and save values into tag list
    if (read_info.read_xml()) break;

    List_iterator_fast<XML_TAG> xmlit(read_info.taglist);
    xmlit.rewind();
    XML_TAG *tag = nullptr;

#ifndef NDEBUG
    DBUG_PRINT("read_xml_field", ("skip_lines=%d", (int)skip_lines));
    while ((tag = xmlit++)) {
      DBUG_PRINT("read_xml_field", ("got tag:%i '%s' '%s'", tag->level,
                                    tag->field.c_ptr(), tag->value.c_ptr()));
    }
#endif

    restore_record(table, s->default_values);
    /*
      Check whether default values of the fields not specified in column list
      are correct or not.
    */
    if (validate_default_values_of_unset_fields(thd, table)) {
      read_info.error = true;
      break;
    }

    const Autoinc_field_has_explicit_non_null_value_reset_guard after_each_row(
        table);

    auto it = m_opt_fields_or_vars.begin();
    Item *item = nullptr;
    for (; it != m_opt_fields_or_vars.end(); ++it) {
      item = *it;
      /* If this line is to be skipped we don't want to fill field or var */
      if (skip_lines) continue;

      // Skip hidden generated columns.
      if (is_hidden_generated_column(table, item)) continue;

      /* find field in tag list */
      xmlit.rewind();
      tag = xmlit++;

      while (tag && strcmp(tag->field.c_ptr(), item->item_name.ptr()) != 0)
        tag = xmlit++;

      item = item->real_item();

      if (!tag)  // found null
      {
        if (item->type() == Item::FIELD_ITEM) {
          Field *field = (static_cast<Item_field *>(item))->field;
          field->reset();
          field->set_null();
          if (field == table->next_number_field)
            table->autoinc_field_has_explicit_non_null_value = true;
          if (!field->is_nullable()) {
            if (field->type() == FIELD_TYPE_TIMESTAMP)
              // Specific of TIMESTAMP NOT NULL: set to CURRENT_TIMESTAMP.
              Item_func_now_local::store_in(field);
            else if (field != table->next_number_field)
              field->set_warning(Sql_condition::SL_WARNING,
                                 ER_WARN_NULL_TO_NOTNULL, 1);
          }
        } else {
          assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
          ((Item_user_var_as_out_param *)item)->set_null_value(cs);
        }
        continue;
      }

      if (item->type() == Item::FIELD_ITEM) {
        Field *field = ((Item_field *)item)->field;
        field->set_notnull();
        if (field == table->next_number_field)
          table->autoinc_field_has_explicit_non_null_value = true;
        field->store(tag->value.ptr(), tag->value.length(), cs);
      } else {
        assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
        ((Item_user_var_as_out_param *)item)
            ->set_value(tag->value.ptr(), tag->value.length(), cs);
      }
    }

    if (read_info.error) break;

    if (skip_lines) {
      skip_lines--;
      continue;
    }

    if (item != nullptr) {
      /* Have not read any field, thus input file is simply ended */
      if (it == m_opt_fields_or_vars.begin()) break;

      for (; it != m_opt_fields_or_vars.end(); item = *it++) {
        if (item->type() == Item::FIELD_ITEM) {
          /*
            QQ: We probably should not throw warning for each field.
            But how about intention to always have the same number
            of warnings in THD::num_truncated_fields (and get rid of
            num_truncated_fields in the end?)
          */
          thd->num_truncated_fields++;
          push_warning_printf(thd, Sql_condition::SL_WARNING,
                              ER_WARN_TOO_FEW_RECORDS,
                              ER_THD(thd, ER_WARN_TOO_FEW_RECORDS),
                              thd->get_stmt_da()->current_row_for_condition());
          if (thd->is_error()) return true;
        } else {
          assert(nullptr != dynamic_cast<Item_user_var_as_out_param *>(item));
          ((Item_user_var_as_out_param *)item)->set_null_value(cs);
        }
      }
    }

    if (thd->killed || fill_record_n_invoke_before_triggers(
                           thd, &info, m_opt_set_fields, m_opt_set_exprs, table,
                           TRG_EVENT_INSERT, table->s->fields, true, nullptr))
      return true;

    switch (table_list->view_check_option(thd)) {
      case VIEW_CHECK_SKIP:
        goto continue_loop;
      case VIEW_CHECK_ERROR:
        return true;
    }

    if (invoke_table_check_constraints(thd, table)) {
      if (thd->is_error()) return true;
      // continue when IGNORE clause is used.
      goto continue_loop;
    }

    if (write_record(thd, table, &info, nullptr)) return true;

    /*
      We don't need to reset auto-increment field since we are restoring
      its default value at the beginning of each loop iteration.
    */
    thd->get_stmt_da()->inc_current_row_for_condition();
  continue_loop:;
  }
  return read_info.error || thd->is_error();
} /* load xml end */

/* Unescape all escape characters, mark \N as null */

char READ_INFO::unescape(char chr) {
  /* keep this switch synchornous with the ESCAPE_CHARS macro */
  switch (chr) {
    case 'n':
      return '\n';
    case 't':
      return '\t';
    case 'r':
      return '\r';
    case 'b':
      return '\b';
    case '0':
      return 0;  // Ascii null
    case 'Z':
      return '\032';  // Win32 end of file
    case 'N':
      found_null = true;

      [[fallthrough]];
    default:
      return chr;
  }
}

/*
  Read a line using buffering
  If last line is empty (in line mode) then it isn't outputted
*/

READ_INFO::READ_INFO(File file_par, size_t tot_length, const CHARSET_INFO *cs,
                     const String &field_term, const String &line_start,
                     const String &line_term, const String &enclosed_par,
                     int escape, bool get_it_from_net, bool is_fifo)
    : file(file_par),
      buff_length(tot_length),
      escape_char(escape),
      found_end_of_line(false),
      eof(false),
      need_end_io_cache(false),
      error(false),
      line_truncated(false),
      found_null(false),
      read_charset(cs) {
  /*
    Field and line terminators must be interpreted as sequence of unsigned char.
    Otherwise, non-ascii terminators will be negative on some platforms,
    and positive on others (depending on the implementation of char).
  */
  field_term_ptr =
      static_cast<const uchar *>(static_cast<const void *>(field_term.ptr()));
  field_term_length = field_term.length();
  line_term_ptr =
      static_cast<const uchar *>(static_cast<const void *>(line_term.ptr()));
  line_term_length = line_term.length();

  level = 0; /* for load xml */
  if (line_start.length() == 0) {
    line_start_ptr = nullptr;
    start_of_line = false;
  } else {
    line_start_ptr = line_start.ptr();
    line_start_end = line_start_ptr + line_start.length();
    start_of_line = true;
  }
  /* If field_terminator == line_terminator, don't use line_terminator */
  if (field_term_length == line_term_length &&
      !memcmp(field_term_ptr, line_term_ptr, field_term_length)) {
    line_term_length = 0;
    line_term_ptr = nullptr;
  }
  enclosed_char = (enclosed_length = enclosed_par.length())
                      ? (uchar)enclosed_par[0]
                      : INT_MAX;
  field_term_char = field_term_length ? field_term_ptr[0] : INT_MAX;
  line_term_char = line_term_length ? line_term_ptr[0] : INT_MAX;

  /* Set of a stack for unget if long terminators */
  size_t length =
      max<size_t>(cs->mbmaxlen, max(field_term_length, line_term_length)) + 1;
  length = std::max(length, line_start.length());
  stack = stack_pos = (int *)(*THR_MALLOC)->Alloc(sizeof(int) * length);

  if (buff_length > max_size() ||
      !(buffer = (uchar *)my_malloc(key_memory_READ_INFO, buff_length + 1,
                                    MYF(MY_WME)))) {
    error = true; /* purecov: inspected */
  } else {
    end_of_buff = buffer + buff_length;
    if (init_io_cache(
            &cache, (get_it_from_net) ? -1 : file, 0,
            (get_it_from_net) ? READ_NET : (is_fifo ? READ_FIFO : READ_CACHE),
            0L, true, MYF(MY_WME))) {
      my_free(buffer); /* purecov: inspected */
      buffer = nullptr;
      error = true;
    } else {
      /*
        init_io_cache() will not initialize read_function member
        if the cache is READ_NET. So we work around the problem with a
        manual assignment
      */
      need_end_io_cache = true;

      if (get_it_from_net) cache.read_function = _my_b_net_read;

      if (mysql_bin_log.is_open())
        cache.pre_read = cache.pre_close = (IO_CACHE_CALLBACK)log_loaded_block;
    }
  }
}

READ_INFO::~READ_INFO() {
  if (need_end_io_cache) ::end_io_cache(&cache);

  if (buffer != nullptr) my_free(buffer);
  List_iterator<XML_TAG> xmlit(taglist);
  XML_TAG *t;
  while ((t = xmlit++)) delete (t);
}

/**
  The logic here is similar with my_mbcharlen, except for GET and PUSH

  @param[in]  cs  charset info
  @param[in]  chr the first char of sequence
  @param[out] len the length of multi-byte char
*/
#define GET_MBCHARLEN(cs, chr, len)                     \
  do {                                                  \
    len = my_mbcharlen((cs), (chr));                    \
    if (len == 0 && my_mbmaxlenlen((cs)) == 2) {        \
      const int chr1 = GET;                             \
      if (chr1 != my_b_EOF) {                           \
        len = my_mbcharlen_2((cs), (chr), chr1);        \
        /* Character is gb18030 or invalid (len = 0) */ \
        assert(len == 0 || len == 2 || len == 4);       \
      }                                                 \
      if (len != 0) PUSH(chr1);                         \
    }                                                   \
  } while (0)

/**
  Skip the terminator string (if any) in the input stream.

  @param ptr    Terminator string.
  @param length Terminator string length.

  @returns false if terminator was found and skipped,
           true if terminator was not found
*/
inline bool READ_INFO::terminator(const uchar *ptr, size_t length) {
  int chr = 0;
  size_t i;
  for (i = 1; i < length; i++) {
    chr = GET;
    if (chr != *++ptr) {
      break;
    }
  }
  if (i == length) return true;
  PUSH(chr);
  while (i-- > 1) PUSH(*--ptr);
  return false;
}

/**
  @returns true if error. If READ_INFO::error is true, then error is fatal (OOM
           or charset error). Otherwise see READ_INFO::found_end_of_line for
           unexpected EOL error or READ_INFO::eof for EOF error respectively.
*/
bool READ_INFO::read_field() {
  int chr, found_enclosed_char;
  uchar *to, *new_buffer;

  found_null = false;
  if (found_end_of_line) return true;  // One have to call next_line

  /* Skip until we find 'line_start' */

  if (start_of_line) {  // Skip until line_start
    start_of_line = false;
    if (find_start_of_fields()) return true;
  }
  if ((chr = GET) == my_b_EOF) {
    found_end_of_line = eof = true;
    return true;
  }
  to = buffer;
  if (chr == enclosed_char) {
    found_enclosed_char = enclosed_char;
    *to++ = (uchar)chr;  // If error
  } else {
    found_enclosed_char = INT_MAX;
    PUSH(chr);
  }

  for (;;) {
    bool escaped_mb = false;
    while (to < end_of_buff) {
      chr = GET;
      if (chr == my_b_EOF) goto found_eof;
      if (chr == escape_char) {
        if ((chr = GET) == my_b_EOF) {
          *to++ = (uchar)escape_char;
          goto found_eof;
        }
        /*
          When escape_char == enclosed_char, we treat it like we do for
          handling quotes in SQL parsing -- you can double-up the
          escape_char to include it literally, but it doesn't do escapes
          like \n. This allows: LOAD DATA ... ENCLOSED BY '"' ESCAPED BY '"'
          with data like: "fie""ld1", "field2"
         */
        if (escape_char != enclosed_char || chr == escape_char) {
          uint ml;
          GET_MBCHARLEN(read_charset, chr, ml);
          /*
            For escaped multibyte character, push back the first byte,
            and will handle it below.
            Because multibyte character's second byte is possible to be
            0x5C, per Query_result_export::send_data, both head byte and
            tail byte are escaped for such characters. So mark it if the
            head byte is escaped and will handle it below.
          */
          if (ml == 1)
            *to++ = (uchar)unescape((char)chr);
          else {
            escaped_mb = true;
            PUSH(chr);
          }
          continue;
        }
        PUSH(chr);
        chr = escape_char;
      }
      if (chr == line_term_char && found_enclosed_char == INT_MAX) {
        if (terminator(line_term_ptr,
                       line_term_length)) {  // Maybe unexpected linefeed
          enclosed = false;
          found_end_of_line = true;
          row_start = buffer;
          row_end = to;
          return false;
        }
      }
      if (chr == found_enclosed_char) {
        if ((chr = GET) == found_enclosed_char) {  // Remove duplicated
          *to++ = (uchar)chr;
          continue;
        }
        // End of enclosed field if followed by field_term or line_term
        if (chr == my_b_EOF ||
            (chr == line_term_char &&
             terminator(line_term_ptr,
                        line_term_length))) {  // Maybe unexpected linefeed
          enclosed = true;
          found_end_of_line = true;
          row_start = buffer + 1;
          row_end = to;
          return false;
        }
        if (chr == field_term_char &&
            terminator(field_term_ptr, field_term_length)) {
          enclosed = true;
          row_start = buffer + 1;
          row_end = to;
          return false;
        }
        /*
          The string didn't terminate yet.
          Store back next character for the loop
        */
        PUSH(chr);
        /* copy the found term character to 'to' */
        chr = found_enclosed_char;
      } else if (chr == field_term_char && found_enclosed_char == INT_MAX) {
        if (terminator(field_term_ptr, field_term_length)) {
          enclosed = false;
          row_start = buffer;
          row_end = to;
          return false;
        }
      }

      uint ml;
      GET_MBCHARLEN(read_charset, chr, ml);
      if (ml == 0) {
        *to = '\0';
        my_error(ER_INVALID_CHARACTER_STRING, MYF(0), read_charset->csname,
                 buffer);
        error = true;
        return true;
      }

      if (ml > 1 && to + ml <= end_of_buff) {
        uchar *p = to;
        *to++ = chr;

        for (uint i = 1; i < ml; i++) {
          chr = GET;
          if (chr == my_b_EOF) {
            /*
             Need to back up the bytes already ready from illformed
             multi-byte char
            */
            to -= i;
            goto found_eof;
          } else if (chr == escape_char && escaped_mb) {
            // Unescape the second byte if it is escaped.
            chr = GET;
            chr = (uchar)unescape((char)chr);
          }
          *to++ = chr;
        }
        if (escaped_mb) escaped_mb = false;
        if (my_ismbchar(read_charset, (const char *)p, (const char *)to))
          continue;
        for (uint i = 0; i < ml; i++) PUSH(*--to);
        chr = GET;
      } else if (ml > 1) {
        // Buffer is too small, exit while loop, and reallocate.
        PUSH(chr);
        break;
      }
      *to++ = (uchar)chr;
    }
    // We come here if buffer is too small. Enlarge it and continue. Fail if we
    // cannot extend buffer anymore.
    const size_t new_buffer_length = check_length(buff_length, IO_SIZE);
    if ((new_buffer_length == buff_length) ||
        !(new_buffer =
              (uchar *)my_realloc(key_memory_READ_INFO, (char *)buffer,
                                  new_buffer_length + 1, MYF(MY_WME)))) {
      error = true;
      return true;
    }
    to = new_buffer + (to - buffer);
    buffer = new_buffer;
    buff_length = new_buffer_length;
    end_of_buff = buffer + buff_length;
  }

found_eof:
  enclosed = false;
  found_end_of_line = eof = true;
  row_start = buffer;
  row_end = to;
  return false;
}

/**
  Read a row with fixed length.

  @note
    The row may not be fixed size on disk if there are escape
    characters in the file.

  @note
    One can't use fixed length with multi-byte charset **

  @returns true if error (unexpected end of file/line)
*/
bool READ_INFO::read_fixed_length() {
  int chr;
  uchar *to;
  if (found_end_of_line) return true;  // One have to call next_line

  if (start_of_line) {  // Skip until line_start
    start_of_line = false;
    if (find_start_of_fields()) return true;
  }

  to = row_start = buffer;
  while (to < end_of_buff) {
    if ((chr = GET) == my_b_EOF) goto found_eof;
    if (chr == escape_char) {
      if ((chr = GET) == my_b_EOF) {
        *to++ = (uchar)escape_char;
        goto found_eof;
      }
      *to++ = (uchar)unescape((char)chr);
      continue;
    }
    if (chr == line_term_char) {
      if (terminator(line_term_ptr,
                     line_term_length)) {  // Maybe unexpected linefeed
        found_end_of_line = true;
        row_end = to;
        return false;
      }
    }
    *to++ = (uchar)chr;
  }
  row_end = to;  // Found full line
  return false;

found_eof:
  found_end_of_line = eof = true;
  row_start = buffer;
  row_end = to;
  return to == buffer;
}

/**
  @returns true if error (unexpected end of file/line)
*/
bool READ_INFO::next_line() {
  line_truncated = false;
  start_of_line = line_start_ptr != nullptr;
  if (found_end_of_line || eof) {
    found_end_of_line = false;
    return eof;
  }
  found_end_of_line = false;
  if (!line_term_length) return false;  // No lines
  for (;;) {
    int chr = GET;
    uint ml;
    if (chr == my_b_EOF) {
      eof = true;
      return true;
    }
    GET_MBCHARLEN(read_charset, chr, ml);
    if (ml > 1) {
      for (uint i = 1; chr != my_b_EOF && i < ml; i++) chr = GET;
      if (chr == escape_char) continue;
    }
    if (chr == my_b_EOF) {
      eof = true;
      return true;
    }
    if (chr == escape_char) {
      line_truncated = true;
      if (GET == my_b_EOF) return true;
      continue;
    }
    if (chr == line_term_char && terminator(line_term_ptr, line_term_length))
      return false;
    line_truncated = true;
  }
}

/**
  @returns true if error (unexpected end of file/line)
*/
bool READ_INFO::find_start_of_fields() {
  int chr;
try_again:
  do {
    if ((chr = GET) == my_b_EOF) {
      found_end_of_line = eof = true;
      return true;
    }
  } while ((char)chr != line_start_ptr[0]);
  for (const char *ptr = line_start_ptr + 1; ptr != line_start_end; ptr++) {
    chr = GET;                // Eof will be checked later
    if ((char)chr != *ptr) {  // Can't be line_start
      PUSH(chr);
      while (--ptr != line_start_ptr) {  // Restart with next char
        PUSH(*ptr);
      }
      goto try_again;
    }
  }
  return false;
}

/*
  Clear taglist from tags with a specified level
*/
void READ_INFO::clear_level(int level_arg) {
  DBUG_TRACE;
  List_iterator<XML_TAG> xmlit(taglist);
  xmlit.rewind();
  XML_TAG *tag;

  while ((tag = xmlit++)) {
    if (tag->level >= level_arg) {
      xmlit.remove();
      delete tag;
    }
  }
}

/*
  Convert an XML entity to Unicode value.
  Return -1 on error;
*/
static int my_xml_entity_to_char(const char *name, size_t length) {
  if (length == 2) {
    if (!memcmp(name, "gt", length)) return '>';
    if (!memcmp(name, "lt", length)) return '<';
  } else if (length == 3) {
    if (!memcmp(name, "amp", length)) return '&';
  } else if (length == 4) {
    if (!memcmp(name, "quot", length)) return '"';
    if (!memcmp(name, "apos", length)) return '\'';
  }
  return -1;
}

/**
  @brief Convert newline, linefeed, tab to space

  @param chr    character

  @details According to the "XML 1.0" standard,
           only space (@#x20) characters, carriage returns,
           line feeds or tabs are considered as spaces.
           Convert all of them to space (@#x20) for parsing simplicity.
*/
static int my_tospace(int chr) {
  return (chr == '\t' || chr == '\r' || chr == '\n') ? ' ' : chr;
}

/*
  Read an xml value: handle multibyte and xml escape

  @param      delim  Delimiter character.
  @param[out] val    Resulting value string.

  @returns next character after delim
           or
           my_b_EOF in case of charset error/unexpected EOF.
*/
int READ_INFO::read_value(int delim, String *val) {
  int chr;
  String tmp;

  for (chr = GET; my_tospace(chr) != delim && chr != my_b_EOF;) {
    uint ml;
    GET_MBCHARLEN(read_charset, chr, ml);
    if (ml == 0) {
      chr = my_b_EOF;
      val->length(0);
      return chr;
    }

    if (ml > 1) {
      DBUG_PRINT("read_xml", ("multi byte"));

      for (uint i = 1; i < ml; i++) {
        val->append(chr);
        /*
          Don't use my_tospace() in the middle of a multi-byte character
          TODO: check that the multi-byte sequence is valid.
        */
        chr = GET;
        if (chr == my_b_EOF) return chr;
      }
    }
    if (chr == '&') {
      tmp.length(0);
      for (chr = my_tospace(GET); chr != ';'; chr = my_tospace(GET)) {
        if (chr == my_b_EOF) return chr;
        tmp.append(chr);
      }
      if ((chr = my_xml_entity_to_char(tmp.ptr(), tmp.length())) >= 0)
        val->append(chr);
      else {
        val->append('&');
        val->append(tmp);
        val->append(';');
      }
    } else
      val->append(chr);
    chr = GET;
  }
  return my_tospace(chr);
}

/*
  Read CDATA value if any.
  Ignore multibyte and XML escape.
  Note: the last character read must be '<' before calling this function.

  @param[out] val           Resulting CDATA string.
  @param[out] have_cdata    Set if really read CDATA.

  @returns    Last character read or
              my_b_EOF in case of unexpected EOF.
*/
int READ_INFO::read_cdata(String *val, bool *have_cdata) {
  const char cdata_head[] = "![CDATA[";
  const char *head_ptr = cdata_head;

  /* Check for CDATA head "![CDATA[" */
  for (size_t i = 0; i < strlen(cdata_head); i++) {
    int chr = GET;

    if (chr != *head_ptr++) {
      /*
        Didn't find "![CDATA[" head,
        push back the last (unmatched) character
      */
      PUSH(chr);
      /* and all matched from the head. */
      while (i--) PUSH(*--head_ptr);

      *have_cdata = false;
      return '<';
    }
  }

  int tail[3]{0};
  for (tail[2] = GET; tail[2] != my_b_EOF; tail[2] = GET) {
    /* Check for CDATA tail "]]>" */
    if (tail[0] == ']' && tail[1] == ']' && tail[2] == '>') {
      /* Cut last two characters ("]]") which were appended to val. */
      assert(val->length() >= 2);
      val->length(val->length() - 2);

      *have_cdata = true;
      return '>';
    }
    /* Shift the tail */
    tail[0] = tail[1];
    tail[1] = tail[2];

    val->append(tail[2]);
  }

  /* Didn't find CDATA tail "]]>", the last character read must be my_b_EOF. */
  assert(tail[2] == my_b_EOF);
  *have_cdata = false;
  return my_b_EOF;
}

/*
  Read a record in xml format
  tags and attributes are stored in taglist
  when tag set in ROWS IDENTIFIED BY is closed, we are ready and return

  @returns true if error (unexpected end of file)
*/
bool READ_INFO::read_xml() {
  DBUG_TRACE;
  int chr, chr2, chr3;
  int delim = 0;
  String tag, attribute, value;
  bool in_tag = false;

  tag.length(0);
  attribute.length(0);
  value.length(0);

  for (chr = my_tospace(GET); chr != my_b_EOF;) {
    switch (chr) {
      case '<': /* read tag */
        /* TODO: check if this is a comment <!-- comment -->  */
        chr = my_tospace(GET);
        if (chr == '!') {
          chr2 = GET;
          chr3 = GET;

          if (chr2 == '-' && chr3 == '-') {
            chr2 = 0;
            chr3 = 0;
            chr = my_tospace(GET);

            while (chr != '>' || chr2 != '-' || chr3 != '-') {
              if (chr == '-') {
                chr3 = chr2;
                chr2 = chr;
              } else if (chr2 == '-') {
                chr2 = 0;
                chr3 = 0;
              }
              chr = my_tospace(GET);
              if (chr == my_b_EOF) goto found_eof;
            }
            break;
          }
        }

        tag.length(0);
        while (chr != '>' && chr != ' ' && chr != '/' && chr != my_b_EOF) {
          if (chr != delim) /* fix for the '<field name =' format */
            tag.append(chr);
          chr = my_tospace(GET);
        }

        // row tag should be in ROWS IDENTIFIED BY '<row>' - stored in line_term
        if ((tag.length() == line_term_length - 2) &&
            (memcmp(tag.ptr(), line_term_ptr + 1, tag.length()) == 0)) {
          DBUG_PRINT("read_xml", ("start-of-row: %i %s %s", level,
                                  tag.c_ptr_safe(), line_term_ptr));
        }

        if (chr == ' ' || chr == '>') {
          level++;
          clear_level(level + 1);
        }

        if (chr == ' ')
          in_tag = true;
        else
          in_tag = false;
        break;

      case ' ':            /* read attribute */
        while (chr == ' ') /* skip blanks */
          chr = my_tospace(GET);

        if (!in_tag) break;

        while (chr != '=' && chr != '/' && chr != '>' && chr != my_b_EOF) {
          attribute.append(chr);
          chr = my_tospace(GET);
        }
        break;

      case '>': /* end tag - read tag value */
        in_tag = false;
        /* Skip all whitespaces */
        while (' ' == (chr = my_tospace(GET))) {
        }
        /*
          Push the first non-whitespace char back to Stack. This char would be
          read in the upcoming call to read_value()
         */
        PUSH(chr);

        /* Read <![CDATA[ ... ]]> and tag's value. */
        bool have_cdata;
        do {
          chr = read_value('<', &value);
          if (chr == my_b_EOF) goto found_eof;

          chr = read_cdata(&value, &have_cdata);
          if (chr == my_b_EOF) goto found_eof;
        } while (have_cdata);

        /* save value to list */
        if (tag.length() > 0 && value.length() > 0) {
          DBUG_PRINT("read_xml", ("lev:%i tag:%s val:%s", level,
                                  tag.c_ptr_safe(), value.c_ptr_safe()));
          taglist.push_front(new XML_TAG(level, tag, value));
        }
        tag.length(0);
        value.length(0);
        attribute.length(0);
        break;

      case '/': /* close tag */
        chr = my_tospace(GET);
        /* Decrease the 'level' only when (i) It's not an */
        /* (without space) empty tag i.e. <tag/> or, (ii) */
        /* It is of format <row col="val" .../>           */
        if (chr != '>' || in_tag) {
          level--;
          in_tag = false;
        }
        if (chr != '>')  /* if this is an empty tag <tag   /> */
          tag.length(0); /* we should keep tag value          */
        while (chr != '>' && chr != my_b_EOF) {
          tag.append(chr);
          chr = my_tospace(GET);
        }

        if ((tag.length() == line_term_length - 2) &&
            (memcmp(tag.ptr(), line_term_ptr + 1, tag.length()) == 0)) {
          DBUG_PRINT("read_xml",
                     ("found end-of-row %i %s", level, tag.c_ptr_safe()));
          return false;  // normal return
        }
        chr = my_tospace(GET);
        break;

      case '=': /* attribute name end - read the value */
        // check for tag field and attribute name
        if (!memcmp(tag.c_ptr_safe(), STRING_WITH_LEN("field")) &&
            !memcmp(attribute.c_ptr_safe(), STRING_WITH_LEN("name"))) {
          /*
            this is format <field name="xx">xx</field>
            where actual fieldname is in attribute
          */
          delim = my_tospace(GET);
          tag.length(0);
          attribute.length(0);
          chr = '<'; /* we pretend that it is a tag */
          level--;
          break;
        }

        // check for " or '
        chr = GET;
        if (chr == my_b_EOF) goto found_eof;
        if (chr == '"' || chr == '\'') {
          delim = chr;
        } else {
          delim = ' '; /* no delimiter, use space */
          PUSH(chr);
        }

        chr = read_value(delim, &value);
        if (attribute.length() > 0 && value.length() > 0) {
          DBUG_PRINT("read_xml", ("lev:%i att:%s val:%s\n", level + 1,
                                  attribute.c_ptr_safe(), value.c_ptr_safe()));
          taglist.push_front(new XML_TAG(level + 1, attribute, value));
        }
        attribute.length(0);
        value.length(0);
        if (chr != ' ') chr = my_tospace(GET);
        break;

      default:
        chr = my_tospace(GET);
    } /* end switch */
  }   /* end while */

found_eof:
  DBUG_PRINT("read_xml", ("Found eof"));
  eof = true;
  return true;
}

bool Sql_cmd_load_table::execute(THD *thd) {
  LEX *const lex = thd->lex;
  bool need_file_acl = false;

  if (m_is_bulk_operation) {
    if (m_exchange.filetype == FILETYPE_XML) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD XML", "BULK Algorithm");
      return true;
    }
    if (!m_opt_fields_or_vars.empty()) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "column list specification");
      return true;
    }
    if (!m_opt_set_fields.empty()) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "assignment to columns or variables");
      return true;
    }
    if (m_is_local_file) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "LOCAL client file");
      return true;
    }
    if (!m_exchange.line.line_start->is_empty()) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "LINES STARTING BY");
      return true;
    }
    if (m_exchange.field.field_term->length() > 1) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "multi-byte column separator");
      return true;
    }

    if (thd->lex->is_ignore()) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA with BULK Algorithm",
               "IGNORE clause");
      return true;
    }
    /* We need FILE_ACL only if the data source files are in server. */
    need_file_acl = (m_bulk_source == LOAD_SOURCE_FILE);

  } else {
    if (m_file_count > 0) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA without BULK Algorithm",
               "multiple files");
      return true;
    }
    if (m_bulk_source == LOAD_SOURCE_URL) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA without BULK Algorithm",
               "URL source");
      return true;
    }
    if (m_compression_algorithm_string.length != 0) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA without BULK Algorithm",
               "COMPRESSION specified!");
      return true;
    }

    if (is_json_object(m_exchange.file_name)) {
      my_error(ER_WRONG_USAGE, MYF(0), "LOAD DATA without BULK Algorithm",
               "JSON object specified as filename!");
      return true;
    }

    /* We need FILE_ACL if data is not streamed by client from client local
     * file. */
    need_file_acl = !m_is_local_file;
  }

  const uint privilege =
      (lex->duplicates == DUP_REPLACE ? INSERT_ACL | DELETE_ACL : INSERT_ACL) |
      (need_file_acl ? FILE_ACL : 0);

  if (m_is_local_file) {
    if (!thd->get_protocol()->has_client_capability(CLIENT_LOCAL_FILES) ||
        !opt_local_infile) {
      my_error(ER_CLIENT_LOCAL_FILES_DISABLED, MYF(0));
      return true;
    }
  }

  if (check_one_table_access(thd, privilege, lex->query_tables)) return true;

  /* Push strict / ignore error handler */
  Ignore_error_handler ignore_handler;
  Strict_error_handler strict_handler;
  if (thd->lex->is_ignore())
    thd->push_internal_handler(&ignore_handler);
  else if (thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  lex->set_using_hypergraph_optimizer(
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER));

  bool res = true;

  if (m_is_bulk_operation) {
    res = execute_bulk(thd);
  } else {
    res = execute_inner(thd, lex->duplicates);
  }

  /* Pop ignore / strict error handler */
  if (thd->lex->is_ignore() || thd->is_strict_mode())
    thd->pop_internal_handler();

  return res;
}
