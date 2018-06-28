/* Copyright (c) 2000, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* Function with list databases, tables or fields */

#include "sql/sql_show.h"

#include "my_config.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <new>
#include <string>

#include "binary_log_types.h"
#include "keycache.h"  // dflt_key_cache
#include "m_ctype.h"
#include "m_string.h"
#include "mf_wcomp.h"    // wild_compare,wild_one,wild_many
#include "mutex_lock.h"  // MUTEX_LOCK
#include "my_alloc.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_command.h"
#include "my_dbug.h"
#include "my_dir.h"  // MY_DIR
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/mysql_lex_string.h"
#include "mysql/plugin.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "nullable.h"
#include "scope_guard.h"  // Scope_guard
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_grant_db
#include "sql/auth/sql_security_ctx.h"
#include "sql/dd/cache/dictionary_client.h"  // dd::cache::Dictionary_client
#include "sql/dd/dd_schema.h"                // dd::Schema_MDL_locker
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"  // dd::Column
#include "sql/dd/types/table.h"   // dd::Table
#include "sql/debug_sync.h"       // DEBUG_SYNC
#include "sql/derror.h"           // ER_THD
#include "sql/enum_query_type.h"
#include "sql/error_handler.h"  // Internal_error_handler
#include "sql/field.h"          // Field
#include "sql/filesort.h"       // filesort_free_buffers
#include "sql/item.h"           // Item_empty_string
#include "sql/item_cmpfunc.h"   // Item_cond
#include "sql/item_func.h"
#include "sql/key.h"
#include "sql/log.h"
#include "sql/mdl.h"
#include "sql/mem_root_array.h"
#include "sql/mysqld.h"              // lower_case_table_names
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/opt_trace.h"           // fill_optimizer_trace_info
#include "sql/partition_info.h"      // partition_info
#include "sql/protocol.h"            // Protocol
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/sp_head.h"   // sp_head
#include "sql/sql_base.h"  // close_thread_tables
#include "sql/sql_bitmap.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_const.h"
#include "sql/sql_db.h"  // get_default_db_collation
#include "sql/sql_error.h"
#include "sql/sql_executor.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_optimizer.h"  // JOIN
#include "sql/sql_parse.h"      // command_name
#include "sql/sql_partition.h"
#include "sql/sql_plugin.h"  // PLUGIN_IS_DELETED, LOCK_plugin
#include "sql/sql_plugin_ref.h"
#include "sql/sql_profile.h"
#include "sql/sql_table.h"      // filename_to_tablename
#include "sql/sql_tmp_table.h"  // create_tmp_table
#include "sql/sql_trigger.h"    // acquire_shared_mdl_for_trigger
#include "sql/system_variables.h"
#include "sql/table_trigger_dispatcher.h"  // Table_trigger_dispatcher
#include "sql/temp_table_param.h"
#include "sql/thr_malloc.h"
#include "sql/trigger.h"  // Trigger
#include "sql/tztime.h"   // Time_zone
#include "template_utils.h"
#include "thr_lock.h"

namespace dd {
class Abstract_table;
class Schema;
}  // namespace dd

/* @see dynamic_privileges_table.cc */
bool iterate_all_dynamic_privileges(THD *thd,
                                    std::function<bool(const char *)> action);
using std::max;
using std::min;

#define STR_OR_NIL(S) ((S) ? (S) : "<nil>")

/**
  @class CSET_STRING
  @brief Character set armed LEX_CSTRING
*/
class CSET_STRING {
 private:
  LEX_CSTRING string;
  const CHARSET_INFO *cs;

 public:
  CSET_STRING() : cs(&my_charset_bin) {
    string.str = NULL;
    string.length = 0;
  }
  CSET_STRING(const char *str_arg, size_t length_arg,
              const CHARSET_INFO *cs_arg)
      : cs(cs_arg) {
    DBUG_ASSERT(cs_arg != NULL);
    string.str = str_arg;
    string.length = length_arg;
  }

  inline const char *str() const { return string.str; }
  inline size_t length() const { return string.length; }
  const CHARSET_INFO *charset() const { return cs; }
};

static const char *grant_names[] = {
    "select",   "insert",  "update", "delete", "create",     "drop",  "reload",
    "shutdown", "process", "file",   "grant",  "references", "index", "alter"};

TYPELIB grant_types = {sizeof(grant_names) / sizeof(char **), "grant_types",
                       grant_names, NULL};

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info);

static void get_cs_converted_string_value(THD *thd, String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs, bool use_hex);

static void append_algorithm(TABLE_LIST *table, String *buff);

static Item *make_cond_for_info_schema(Item *cond, TABLE_LIST *table);

static int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff);

/***************************************************************************
** List all table types supported
***************************************************************************/

static size_t make_version_string(char *buf, size_t buf_length, uint version) {
  return snprintf(buf, buf_length, "%d.%d", version >> 8, version & 0xff);
}

static bool show_plugins(THD *thd, plugin_ref plugin, void *arg) {
  TABLE *table = (TABLE *)arg;
  struct st_mysql_plugin *plug = plugin_decl(plugin);
  struct st_plugin_dl *plugin_dl = plugin_dlib(plugin);
  CHARSET_INFO *cs = system_charset_info;
  char version_buf[20];

  restore_record(table, s->default_values);

  DBUG_EXECUTE_IF("set_uninstall_sync_point", {
    if (strcmp(plugin_name(plugin)->str, "EXAMPLE") == 0)
      DEBUG_SYNC(thd, "before_store_plugin_name");
  });

  mysql_mutex_lock(&LOCK_plugin);
  if (plugin == nullptr || plugin_state(plugin) == PLUGIN_IS_FREED) {
    mysql_mutex_unlock(&LOCK_plugin);
    return false;
  }

  table->field[0]->store(plugin_name(plugin)->str, plugin_name(plugin)->length,
                         cs);

  table->field[1]->store(
      version_buf,
      make_version_string(version_buf, sizeof(version_buf), plug->version), cs);

  switch (plugin_state(plugin)) {
    /* case PLUGIN_IS_FREED: does not happen */
    case PLUGIN_IS_DELETED:
      table->field[2]->store(STRING_WITH_LEN("DELETED"), cs);
      break;
    case PLUGIN_IS_UNINITIALIZED:
      table->field[2]->store(STRING_WITH_LEN("INACTIVE"), cs);
      break;
    case PLUGIN_IS_READY:
      table->field[2]->store(STRING_WITH_LEN("ACTIVE"), cs);
      break;
    case PLUGIN_IS_DYING:
      table->field[2]->store(STRING_WITH_LEN("DELETING"), cs);
      break;
    case PLUGIN_IS_DISABLED:
      table->field[2]->store(STRING_WITH_LEN("DISABLED"), cs);
      break;
    default:
      DBUG_ASSERT(0);
  }

  table->field[3]->store(plugin_type_names[plug->type].str,
                         plugin_type_names[plug->type].length, cs);
  table->field[4]->store(version_buf,
                         make_version_string(version_buf, sizeof(version_buf),
                                             *(uint *)plug->info),
                         cs);

  if (plugin_dl) {
    table->field[5]->store(plugin_dl->dl.str, plugin_dl->dl.length, cs);
    table->field[5]->set_notnull();
    table->field[6]->store(version_buf,
                           make_version_string(version_buf, sizeof(version_buf),
                                               plugin_dl->version),
                           cs);
    table->field[6]->set_notnull();
  } else {
    table->field[5]->set_null();
    table->field[6]->set_null();
  }

  if (plug->author) {
    table->field[7]->store(plug->author, strlen(plug->author), cs);
    table->field[7]->set_notnull();
  } else
    table->field[7]->set_null();

  if (plug->descr) {
    table->field[8]->store(plug->descr, strlen(plug->descr), cs);
    table->field[8]->set_notnull();
  } else
    table->field[8]->set_null();

  switch (plug->license) {
    case PLUGIN_LICENSE_GPL:
      table->field[9]->store(PLUGIN_LICENSE_GPL_STRING,
                             strlen(PLUGIN_LICENSE_GPL_STRING), cs);
      break;
    case PLUGIN_LICENSE_BSD:
      table->field[9]->store(PLUGIN_LICENSE_BSD_STRING,
                             strlen(PLUGIN_LICENSE_BSD_STRING), cs);
      break;
    default:
      table->field[9]->store(PLUGIN_LICENSE_PROPRIETARY_STRING,
                             strlen(PLUGIN_LICENSE_PROPRIETARY_STRING), cs);
      break;
  }
  table->field[9]->set_notnull();

  table->field[10]->store(
      global_plugin_typelib_names[plugin_load_option(plugin)],
      strlen(global_plugin_typelib_names[plugin_load_option(plugin)]), cs);
  mysql_mutex_unlock(&LOCK_plugin);

  return schema_table_store_record(thd, table);
}

static int fill_plugins(THD *thd, TABLE_LIST *tables, Item *) {
  DBUG_ENTER("fill_plugins");

  if (plugin_foreach_with_mask(thd, show_plugins, MYSQL_ANY_PLUGIN,
                               ~PLUGIN_IS_FREED, tables->table))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

/***************************************************************************
 List all privileges supported
***************************************************************************/
struct show_privileges_st {
  const char *privilege;
  const char *context;
  const char *comment;
};

static struct show_privileges_st sys_privileges[] = {
    {"Alter", "Tables", "To alter the table"},
    {"Alter routine", "Functions,Procedures",
     "To alter or drop stored functions/procedures"},
    {"Create", "Databases,Tables,Indexes",
     "To create new databases and tables"},
    {"Create routine", "Databases", "To use CREATE FUNCTION/PROCEDURE"},
    {"Create role", "Server Admin", "To create new roles"},
    {"Create temporary tables", "Databases", "To use CREATE TEMPORARY TABLE"},
    {"Create view", "Tables", "To create new views"},
    {"Create user", "Server Admin", "To create new users"},
    {"Delete", "Tables", "To delete existing rows"},
    {"Drop", "Databases,Tables", "To drop databases, tables, and views"},
    {"Drop role", "Server Admin", "To drop roles"},
    {"Event", "Server Admin", "To create, alter, drop and execute events"},
    {"Execute", "Functions,Procedures", "To execute stored routines"},
    {"File", "File access on server", "To read and write files on the server"},
    {"Grant option", "Databases,Tables,Functions,Procedures",
     "To give to other users those privileges you possess"},
    {"Index", "Tables", "To create or drop indexes"},
    {"Insert", "Tables", "To insert data into tables"},
    {"Lock tables", "Databases",
     "To use LOCK TABLES (together with SELECT privilege)"},
    {"Process", "Server Admin",
     "To view the plain text of currently executing queries"},
    {"Proxy", "Server Admin", "To make proxy user possible"},
    {"References", "Databases,Tables", "To have references on tables"},
    {"Reload", "Server Admin",
     "To reload or refresh tables, logs and privileges"},
    {"Replication client", "Server Admin",
     "To ask where the slave or master servers are"},
    {"Replication slave", "Server Admin",
     "To read binary log events from the master"},
    {"Select", "Tables", "To retrieve rows from table"},
    {"Show databases", "Server Admin",
     "To see all databases with SHOW DATABASES"},
    {"Show view", "Tables", "To see views with SHOW CREATE VIEW"},
    {"Shutdown", "Server Admin", "To shut down the server"},
    {"Super", "Server Admin",
     "To use KILL thread, SET GLOBAL, CHANGE MASTER, etc."},
    {"Trigger", "Tables", "To use triggers"},
    {"Create tablespace", "Server Admin", "To create/alter/drop tablespaces"},
    {"Update", "Tables", "To update existing rows"},
    {"Usage", "Server Admin", "No privileges - allow connect only"},
    {NullS, NullS, NullS}};

bool mysqld_show_privileges(THD *thd) {
  List<Item> field_list;
  Protocol *protocol = thd->get_protocol();
  DBUG_ENTER("mysqld_show_privileges");

  field_list.push_back(new Item_empty_string("Privilege", 10));
  field_list.push_back(new Item_empty_string("Context", 15));
  field_list.push_back(new Item_empty_string("Comment", NAME_CHAR_LEN));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(true);

  show_privileges_st *privilege = sys_privileges;
  for (privilege = sys_privileges; privilege->privilege; privilege++) {
    protocol->start_row();
    protocol->store(privilege->privilege, system_charset_info);
    protocol->store(privilege->context, system_charset_info);
    protocol->store(privilege->comment, system_charset_info);
    if (protocol->end_row()) DBUG_RETURN(true);
  }
  if (iterate_all_dynamic_privileges(
          thd,
          /*
            For each registered dynamic privilege send a strz to
            this lambda function.
          */
          [&](const char *c) -> bool {
            protocol->start_row();
            protocol->store(c, system_charset_info);
            protocol->store("Server Admin", system_charset_info);
            protocol->store("", system_charset_info);
            if (protocol->end_row()) return true;
            return false;
          })) {
    DBUG_RETURN(true);
  }
  my_eof(thd);
  DBUG_RETURN(false);
}

/*
  find_files() - find files in a given directory.

  SYNOPSIS
    find_files()
    thd                 thread handler
    files               put found files in this list
    db                  database name to set in TABLE_LIST structure
    path                path to database
    wild                filter for found files
    dir                 read databases in path if true, read .frm files in
                        database otherwise

  RETURN
    FIND_FILES_OK       success
    FIND_FILES_OOM      out of memory error
    FIND_FILES_DIR      no such directory, or directory can't be read
*/

find_files_result find_files(THD *thd, List<LEX_STRING> *files, const char *db,
                             const char *path, const char *wild, bool dir,
                             MEM_ROOT *tmp_mem_root) {
  uint i;
  MY_DIR *dirp;
  MEM_ROOT **root_ptr = NULL, *old_root = NULL;
  uint col_access = thd->col_access;
  size_t wild_length = 0;
  TABLE_LIST table_list;
  DBUG_ENTER("find_files");

  if (wild) {
    if (!wild[0])
      wild = 0;
    else
      wild_length = strlen(wild);
  }

  if (!(dirp = my_dir(path, MYF(dir ? MY_WANT_STAT : 0)))) {
    if (my_errno() == ENOENT)
      my_error(ER_BAD_DB_ERROR, MYF(0), db);
    else {
      char errbuf[MYSYS_STRERROR_SIZE];
      my_error(ER_CANT_READ_DIR, MYF(0), path, my_errno(),
               my_strerror(errbuf, sizeof(errbuf), my_errno()));
    }
    DBUG_RETURN(FIND_FILES_DIR);
  }

  if (tmp_mem_root) {
    root_ptr = THR_MALLOC;
    old_root = *root_ptr;
    *root_ptr = tmp_mem_root;
  }

  for (i = 0; i < dirp->number_off_files; i++) {
    char uname[NAME_LEN + 1]; /* Unencoded name */
    FILEINFO *file;
    LEX_STRING *file_name = 0;
    size_t file_name_len;
    char *ext;

    file = dirp->dir_entry + i;
    if (dir) { /* Return databases */
      /*
        Ignore all the directories having names that start with a  dot (.).
        This covers '.' and '..' and other cases like e.g. '.mysqlgui'.
        Note that since 5.1 database directory names can't start with a
        dot (.) thanks to table name encoding.
      */
      if (file->name[0] == '.') continue;
      if (!MY_S_ISDIR(file->mystat->st_mode)) continue;
    } else {
      // Return only .frm files which aren't temp files.
      if (my_strcasecmp(system_charset_info, ext = fn_rext(file->name),
                        reg_ext) ||
          is_prefix(file->name, tmp_file_prefix))
        continue;
      *ext = 0;
    }

    file_name_len = filename_to_tablename(file->name, uname, sizeof(uname));

    if (wild) {
      if (lower_case_table_names) {
        if (my_wildcmp(files_charset_info, uname, uname + file_name_len, wild,
                       wild + wild_length, wild_prefix, wild_one, wild_many))
          continue;
      } else if (wild_compare(uname, file_name_len, wild, wild_length, 0))
        continue;
    }

    /* Don't show tables where we don't have any privileges */
    if (db && !(col_access & TABLE_ACLS)) {
      table_list.db = (char *)db;
      table_list.db_length = strlen(db);
      table_list.table_name = uname;
      table_list.table_name_length = file_name_len;
      table_list.grant.privilege = col_access;
      if (check_grant(thd, TABLE_ACLS, &table_list, true, 1, true)) continue;
    }

    if (!(file_name = tmp_mem_root
                          ? make_lex_string_root(tmp_mem_root, file_name, uname,
                                                 file_name_len, true)
                          : thd->make_lex_string(file_name, uname,
                                                 file_name_len, true)) ||
        files->push_back(file_name)) {
      my_dirend(dirp);
      DBUG_RETURN(FIND_FILES_OOM);
    }
  }
  DBUG_PRINT("info", ("found: %d files", files->elements));
  my_dirend(dirp);

  (void)ha_find_files(thd, db, path, wild, dir, files);

  if (tmp_mem_root) *root_ptr = old_root;

  DBUG_RETURN(FIND_FILES_OK);
}

/**
   An Internal_error_handler that suppresses errors regarding views'
   underlying tables that occur during privilege checking within SHOW CREATE
   VIEW commands. This happens in the cases when

   - A view's underlying table (e.g. referenced in its SELECT list) does not
     exist or columns of underlying table are altered. There should not be an
     error as no attempt was made to access it per se.

   - Access is denied for some table, column, function or stored procedure
     such as mentioned above. This error gets raised automatically, since we
     can't untangle its access checking from that of the view itself.
 */
class Show_create_error_handler : public Internal_error_handler {
  TABLE_LIST *m_top_view;
  bool m_handling;
  Security_context *m_sctx;

  char m_view_access_denied_message[MYSQL_ERRMSG_SIZE];
  const char *m_view_access_denied_message_ptr;

 public:
  /**
     Creates a new Show_create_error_handler for the particular security
     context and view.

     @param thd Thread context, used for security context information if needed.
     @param top_view The view. We do not verify at this point that top_view is
     in fact a view since, alas, these things do not stay constant.
  */
  explicit Show_create_error_handler(THD *thd, TABLE_LIST *top_view)
      : m_top_view(top_view),
        m_handling(false),
        m_view_access_denied_message_ptr(NULL) {
    m_sctx = (m_top_view->security_ctx != nullptr) ? m_top_view->security_ctx
                                                   : thd->security_context();
  }

 private:
  /**
     Lazy instantiation of 'view access denied' message. The purpose of the
     Show_create_error_handler is to hide details of underlying tables for
     which we have no privileges behind ER_VIEW_INVALID messages. But this
     obviously does not apply if we lack privileges on the view itself.
     Unfortunately the information about for which table privilege checking
     failed is not available at this point. The only way for us to check is by
     reconstructing the actual error message and see if it's the same.
  */
  const char *get_view_access_denied_message(THD *thd) {
    if (!m_view_access_denied_message_ptr) {
      m_view_access_denied_message_ptr = m_view_access_denied_message;
      snprintf(m_view_access_denied_message, MYSQL_ERRMSG_SIZE,
               ER_THD(thd, ER_TABLEACCESS_DENIED_ERROR), "SHOW VIEW",
               m_sctx->priv_user().str, m_sctx->host_or_ip().str,
               m_top_view->get_table_name());
    }
    return m_view_access_denied_message_ptr;
  }

 public:
  virtual bool handle_condition(THD *thd, uint sql_errno, const char *,
                                Sql_condition::enum_severity_level *,
                                const char *msg) {
    /*
       The handler does not handle the errors raised by itself.
       At this point we know if top_view is really a view.
    */
    if (m_handling || !m_top_view->is_view()) return false;

    m_handling = true;

    bool is_handled;

    switch (sql_errno) {
      case ER_TABLEACCESS_DENIED_ERROR:
        if (!strcmp(get_view_access_denied_message(thd), msg)) {
          /* Access to top view is not granted, don't interfere. */
          is_handled = false;
          break;
        }
        // Fall through
      case ER_COLUMNACCESS_DENIED_ERROR:
      // ER_VIEW_NO_EXPLAIN cannot happen here.
      case ER_PROCACCESS_DENIED_ERROR:
        is_handled = true;
        break;

      case ER_BAD_FIELD_ERROR:
        /*
          Established behavior: warn if column of underlying table is altered.
        */
      case ER_NO_SUCH_TABLE:
        /* Established behavior: warn if underlying tables are missing. */
      case ER_SP_DOES_NOT_EXIST:
        /* Established behavior: warn if underlying functions are missing. */
        push_warning_printf(thd, Sql_condition::SL_WARNING, ER_VIEW_INVALID,
                            ER_THD(thd, ER_VIEW_INVALID),
                            m_top_view->get_db_name(),
                            m_top_view->get_table_name());
        is_handled = true;
        break;
      default:
        is_handled = false;
    }

    m_handling = false;
    return is_handled;
  }
};

bool mysqld_show_create(THD *thd, TABLE_LIST *table_list) {
  Protocol *protocol = thd->get_protocol();
  char buff[2048];
  String buffer(buff, sizeof(buff), system_charset_info);
  List<Item> field_list;
  bool error = true;
  DBUG_ENTER("mysqld_show_create");
  DBUG_PRINT("enter",
             ("db: %s  table: %s", table_list->db, table_list->table_name));

  /*
    Metadata locks taken during SHOW CREATE should be released when
    the statmement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  /* We want to preserve the tree for views. */
  thd->lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_VIEW;

  {
    /*
      If there is an error during processing of an underlying view, an
      error message is wanted, but it has to be converted to a warning,
      so that execution can continue.
      This is handled by the Show_create_error_handler class.

      Use open_tables() instead of open_tables_for_query(). If an error occurs,
      this will ensure that tables are not closed on error, but remain open
      for the rest of the processing of the SHOW statement.
    */
    Show_create_error_handler view_error_suppressor(thd, table_list);
    thd->push_internal_handler(&view_error_suppressor);

    uint counter;
    bool open_error = open_tables(thd, &table_list, &counter,
                                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL);
    if (!open_error && table_list->is_view_or_derived()) {
      /*
        Prepare result table for view so that we can read the column list.
        Notice that Show_create_error_handler remains active, so that any
        errors due to missing underlying objects are converted to warnings.
      */
      open_error = table_list->resolve_derived(thd, true);
    }
    thd->pop_internal_handler();
    if (open_error && (thd->killed || thd->is_error())) goto exit;
  }

  /* TODO: add environment variables show when it become possible */
  if (thd->lex->only_view && !table_list->is_view()) {
    my_error(ER_WRONG_OBJECT, MYF(0), table_list->db, table_list->table_name,
             "VIEW");
    goto exit;
  }

  buffer.length(0);

  if (table_list->is_view())
    buffer.set_charset(table_list->view_creation_ctx->get_client_cs());

  if ((table_list->is_view() ? view_store_create_info(thd, table_list, &buffer)
                             : store_create_info(thd, table_list, &buffer, NULL,
                                                 false /* show_database */)))
    goto exit;

  if (table_list->is_view()) {
    field_list.push_back(new Item_empty_string("View", NAME_CHAR_LEN));
    field_list.push_back(new Item_empty_string(
        "Create View", max<uint>(buffer.length(), 1024U)));
    field_list.push_back(
        new Item_empty_string("character_set_client", MY_CS_NAME_SIZE));
    field_list.push_back(
        new Item_empty_string("collation_connection", MY_CS_NAME_SIZE));
  } else {
    field_list.push_back(new Item_empty_string("Table", NAME_CHAR_LEN));
    // 1024 is for not to confuse old clients
    field_list.push_back(new Item_empty_string(
        "Create Table", max<size_t>(buffer.length(), 1024U)));
  }

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    goto exit;

  protocol->start_row();
  if (table_list->is_view())
    protocol->store(table_list->view_name.str, system_charset_info);
  else {
    if (table_list->schema_table)
      protocol->store(table_list->schema_table->table_name,
                      system_charset_info);
    else
      protocol->store(table_list->table->alias, system_charset_info);
  }

  if (table_list->is_view()) {
    protocol->store(buffer.ptr(), buffer.length(),
                    table_list->view_creation_ctx->get_client_cs());

    protocol->store(table_list->view_creation_ctx->get_client_cs()->csname,
                    system_charset_info);

    protocol->store(table_list->view_creation_ctx->get_connection_cl()->name,
                    system_charset_info);
  } else
    protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row()) goto exit;

  error = false;
  my_eof(thd);

exit:
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  DBUG_RETURN(error);
}

bool mysqld_show_create_db(THD *thd, char *dbname,
                           HA_CREATE_INFO *create_info) {
  char buff[2048], orig_dbname[NAME_LEN];
  String buffer(buff, sizeof(buff), system_charset_info);
  Security_context *sctx = thd->security_context();
  uint db_access;
  HA_CREATE_INFO create;
  uint create_options = create_info ? create_info->options : 0;
  Protocol *protocol = thd->get_protocol();
  DBUG_ENTER("mysql_show_create_db");

  strcpy(orig_dbname, dbname);
  if (lower_case_table_names && dbname != any_db)
    my_casedn_str(files_charset_info, dbname);

  if (sctx->check_access(DB_ACLS))
    db_access = DB_ACLS;
  else {
    if (sctx->get_active_roles()->size() > 0 && dbname != 0) {
      db_access = sctx->db_acl({dbname, strlen(dbname)});
    } else {
      db_access = (acl_get(thd, sctx->host().str, sctx->ip().str,
                           sctx->priv_user().str, dbname, 0) |
                   sctx->master_access());
    }
  }
  if (!(db_access & DB_ACLS) && check_grant_db(thd, dbname)) {
    my_error(ER_DBACCESS_DENIED_ERROR, MYF(0), sctx->priv_user().str,
             sctx->host_or_ip().str, dbname);
    query_logger.general_log_print(
        thd, COM_INIT_DB, ER_DEFAULT(ER_DBACCESS_DENIED_ERROR),
        sctx->priv_user().str, sctx->host_or_ip().str, dbname);
    DBUG_RETURN(true);
  }

  if (is_infoschema_db(dbname)) {
    dbname = INFORMATION_SCHEMA_NAME.str;
    create.default_table_charset = system_charset_info;
  } else {
    dd::Schema_MDL_locker mdl_handler(thd);
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
    const dd::Schema *schema = nullptr;
    if (mdl_handler.ensure_locked(dbname) ||
        thd->dd_client()->acquire(dbname, &schema))
      DBUG_RETURN(true);

    if (schema == nullptr) {
      my_error(ER_BAD_DB_ERROR, MYF(0), dbname);
      DBUG_RETURN(true);
    }

    if (get_default_db_collation(*schema, &create.default_table_charset)) {
      DBUG_ASSERT(thd->is_error() || thd->killed);
      DBUG_RETURN(true);
    }

    if (create.default_table_charset == NULL)
      create.default_table_charset = thd->collation();
  }
  List<Item> field_list;
  field_list.push_back(new Item_empty_string("Database", NAME_CHAR_LEN));
  field_list.push_back(new Item_empty_string("Create Database", 1024));

  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(true);

  protocol->start_row();
  protocol->store(orig_dbname, strlen(orig_dbname), system_charset_info);
  buffer.length(0);
  buffer.append(STRING_WITH_LEN("CREATE DATABASE "));
  if (create_options & HA_LEX_CREATE_IF_NOT_EXISTS)
    buffer.append(STRING_WITH_LEN("/*!32312 IF NOT EXISTS*/ "));
  append_identifier(thd, &buffer, orig_dbname, strlen(orig_dbname));

  if (create.default_table_charset) {
    buffer.append(STRING_WITH_LEN(" /*!40100"));
    buffer.append(STRING_WITH_LEN(" DEFAULT CHARACTER SET "));
    buffer.append(create.default_table_charset->csname);
    if (!(create.default_table_charset->state & MY_CS_PRIMARY) ||
        create.default_table_charset == &my_charset_utf8mb4_0900_ai_ci) {
      buffer.append(STRING_WITH_LEN(" COLLATE "));
      buffer.append(create.default_table_charset->name);
    }
    buffer.append(STRING_WITH_LEN(" */"));
  }
  protocol->store(buffer.ptr(), buffer.length(), buffer.charset());

  if (protocol->end_row()) DBUG_RETURN(true);
  my_eof(thd);
  DBUG_RETURN(false);
}

/****************************************************************************
  Return only fields for API mysql_list_fields
  Use "show table wildcard" in mysql instead of this
****************************************************************************/

void mysqld_list_fields(THD *thd, TABLE_LIST *table_list, const char *wild) {
  DBUG_ENTER("mysqld_list_fields");
  DBUG_PRINT("enter", ("table: %s", table_list->table_name));

  if (open_tables_for_query(thd, table_list,
                            MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL))
    DBUG_VOID_RETURN;

  if (table_list->is_view_or_derived()) {
    // Setup materialized result table so that we can read the column list
    if (table_list->resolve_derived(thd, false))
      DBUG_VOID_RETURN; /* purecov: inspected */
    if (table_list->setup_materialized_derived(thd))
      DBUG_VOID_RETURN; /* purecov: inspected */
  }
  TABLE *table = table_list->table;

  List<Item> field_list;

  Field **ptr, *field;
  for (ptr = table->field; (field = *ptr); ptr++) {
    if (!wild || !wild[0] ||
        !wild_case_compare(system_charset_info, field->field_name, wild)) {
      Item *item;
      if (table_list->is_view()) {
        item = new Item_ident_for_show(field, table_list->view_db.str,
                                       table_list->view_name.str);
        (void)item->fix_fields(thd, NULL);
      } else {
        item = new Item_field(field);
      }
      field_list.push_back(item);
    }
  }
  restore_record(table, s->default_values);  // Get empty record
  table->use_all_columns();
  if (thd->send_result_metadata(&field_list, Protocol::SEND_DEFAULTS))
    DBUG_VOID_RETURN;
  my_eof(thd);
  DBUG_VOID_RETURN;
}

/*
  Go through all character combinations and ensure that sql_lex.cc can
  parse it as an identifier.

  SYNOPSIS
  require_quotes()
  name			attribute name
  name_length		length of name

  RETURN
    #	Pointer to conflicting character
    0	No conflicting character
*/

static const char *require_quotes(const char *name, size_t name_length) {
  bool pure_digit = true;
  const char *end = name + name_length;

  for (; name < end; name++) {
    uchar chr = (uchar)*name;
    uint length = my_mbcharlen(system_charset_info, chr);
    if (length == 0 || (length == 1 && !system_charset_info->ident_map[chr]))
      return name;
    if (length == 1 && (chr < '0' || chr > '9')) pure_digit = false;
  }
  if (pure_digit) return name;
  return 0;
}

/**
  Convert and quote the given identifier if needed and append it to the
  target string. If the given identifier is empty, it will be quoted.
  This function always use the backtick as escape char and thus rid itself
  of the THD dependency.

  @param packet                target string
  @param name                  the identifier to be appended
  @param length                length of the appending identifier
*/

void append_identifier(String *packet, const char *name, size_t length) {
  const char *name_end;
  char quote_char = '`';

  const CHARSET_INFO *cs_info = system_charset_info;
  const char *to_name = name;
  size_t to_length = length;

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  (void)packet->reserve(to_length * 2 + 2);
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end = to_name + to_length; to_name < name_end;
       to_name += to_length) {
    uchar chr = static_cast<uchar>(*to_name);
    to_length = my_mbcharlen(cs_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!to_length) to_length = 1;
    if (to_length == 1 && chr == static_cast<uchar>(quote_char))
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(to_name, to_length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}

/**
  Convert and quote the given identifier if needed and append it to the
  target string. If the given identifier is empty, it will be quoted.

  @param thd                   thread handler
  @param packet                target string
  @param name                  the identifier to be appended
  @param length                length of the appending identifier
  @param from_cs               Charset information about the input string
  @param to_cs                 Charset information about the target string
*/

void append_identifier(THD *thd, String *packet, const char *name,
                       size_t length, const CHARSET_INFO *from_cs,
                       const CHARSET_INFO *to_cs) {
  const char *name_end;
  char quote_char;
  int q;

  const CHARSET_INFO *cs_info = system_charset_info;
  const char *to_name = name;
  size_t to_length = length;
  String to_string(name, length, from_cs);

  if (from_cs != NULL && to_cs != NULL && from_cs != to_cs)
    thd->convert_string(&to_string, from_cs, to_cs);

  if (to_cs != NULL) {
    to_name = to_string.c_ptr();
    to_length = to_string.length();
    cs_info = to_cs;
  }

  q = thd != NULL ? get_quote_char_for_identifier(thd, to_name, to_length)
                  : '`';

  if (q == EOF) {
    packet->append(to_name, to_length, packet->charset());
    return;
  }

  /*
    The identifier must be quoted as it includes a quote character or
   it's a keyword
  */

  (void)packet->reserve(to_length * 2 + 2);
  quote_char = (char)q;
  packet->append(&quote_char, 1, system_charset_info);

  for (name_end = to_name + to_length; to_name < name_end;
       to_name += to_length) {
    uchar chr = static_cast<uchar>(*to_name);
    to_length = my_mbcharlen(cs_info, chr);
    /*
      my_mbcharlen can return 0 on a wrong multibyte
      sequence. It is possible when upgrading from 4.0,
      and identifier contains some accented characters.
      The manual says it does not work. So we'll just
      change length to 1 not to hang in the endless loop.
    */
    if (!to_length) to_length = 1;
    if (to_length == 1 && chr == static_cast<uchar>(quote_char))
      packet->append(&quote_char, 1, system_charset_info);
    packet->append(to_name, to_length, system_charset_info);
  }
  packet->append(&quote_char, 1, system_charset_info);
}

/*
  Get the quote character for displaying an identifier.

  SYNOPSIS
    get_quote_char_for_identifier()
    thd		Thread handler
    name	name to quote
    length	length of name

  IMPLEMENTATION
    Force quoting in the following cases:
      - name is empty (for one, it is possible when we use this function for
        quoting user and host names for DEFINER clause);
      - name is a keyword;
      - name includes a special character;
    Otherwise identifier is quoted only if the option OPTION_QUOTE_SHOW_CREATE
    is set.

  RETURN
    EOF	  No quote character is needed
    #	  Quote character
*/

int get_quote_char_for_identifier(THD *thd, const char *name, size_t length) {
  if (length && !is_keyword(name, length) && !require_quotes(name, length) &&
      !(thd->variables.option_bits & OPTION_QUOTE_SHOW_CREATE))
    return EOF;
  if (thd->variables.sql_mode & MODE_ANSI_QUOTES) return '"';
  return '`';
}

void append_identifier(THD *thd, String *packet, const char *name,
                       size_t length) {
  if (thd == 0)
    append_identifier(packet, name, length);
  else
    append_identifier(thd, packet, name, length, NULL, NULL);
}

/* Append directory name (if exists) to CREATE INFO */

static void append_directory(THD *thd, String *packet, const char *dir_type,
                             const char *filename) {
  if (filename && !(thd->variables.sql_mode & MODE_NO_DIR_IN_CREATE)) {
    size_t length = dirname_length(filename);
    packet->append(' ');
    packet->append(dir_type);
    packet->append(STRING_WITH_LEN(" DIRECTORY='"));
#ifdef _WIN32
    /* Convert \ to / to be able to create table on unix */
    char *winfilename = (char *)thd->memdup(filename, length);
    char *pos, *end;
    for (pos = winfilename, end = pos + length; pos < end; pos++) {
      if (*pos == '\\') *pos = '/';
    }
    filename = winfilename;
#endif
    packet->append(filename, length);
    packet->append('\'');
  }
}

#define LIST_PROCESS_HOST_LEN 64

/**
  Print "ON UPDATE" clause of a field into a string.

  @param field             The field to generate ON UPDATE clause for.
  @param val
  @param lcase             Whether to print in lower case.
  @return                  false on success, true on error.
*/
static bool print_on_update_clause(Field *field, String *val, bool lcase) {
  DBUG_ASSERT(val->charset()->mbminlen == 1);
  val->length(0);
  if (field->has_update_default_function()) {
    if (lcase)
      val->copy(STRING_WITH_LEN("on update "), val->charset());
    else
      val->copy(STRING_WITH_LEN("ON UPDATE "), val->charset());
    val->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
    if (field->decimals() > 0) val->append_parenthesized(field->decimals());
    return true;
  }
  return false;
}

static bool print_default_clause(Field *field, String *def_value, bool quoted) {
  enum enum_field_types field_type = field->type();

  const bool has_now_default = field->has_insert_default_function();
  const bool has_default = (field_type != FIELD_TYPE_BLOB &&
                            !(field->flags & NO_DEFAULT_VALUE_FLAG) &&
                            !(field->auto_flags & Field::NEXT_NUMBER));

  if (field->gcol_info) return false;

  def_value->length(0);
  if (has_default) {
    if (has_now_default)
    /*
      We are using CURRENT_TIMESTAMP instead of NOW because it is the SQL
      standard.
    */
    {
      def_value->append(STRING_WITH_LEN("CURRENT_TIMESTAMP"));
      if (field->decimals() > 0)
        def_value->append_parenthesized(field->decimals());
    } else if (!field->is_null()) {  // Not null by default
      char tmp[MAX_FIELD_WIDTH];
      String type(tmp, sizeof(tmp), field->charset());
      if (field_type == MYSQL_TYPE_BIT) {
        longlong dec = field->val_int();
        char *ptr = longlong2str(dec, tmp + 2, 2);
        uint32 length = (uint32)(ptr - tmp);
        tmp[0] = 'b';
        tmp[1] = '\'';
        tmp[length] = '\'';
        type.length(length + 1);
        quoted = 0;
      } else
        field->val_str(&type);
      if (type.length()) {
        String def_val;
        uint dummy_errors;
        /* convert to system_charset_info == utf8 */
        def_val.copy(type.ptr(), type.length(), field->charset(),
                     system_charset_info, &dummy_errors);
        if (quoted)
          append_unescaped(def_value, def_val.ptr(), def_val.length());
        else
          def_value->append(def_val.ptr(), def_val.length());
      } else if (quoted)
        def_value->append(STRING_WITH_LEN("''"));
    } else if (field->maybe_null() && quoted)
      def_value->append(STRING_WITH_LEN("NULL"));  // Null as default
    else
      return 0;
  }
  return has_default;
}

/**
  Build a CREATE TABLE statement for a table.

  @param thd              The thread
  @param table_list       A list containing one table to write statement for.
  @param packet           Pointer to a string where statement will be written.
  @param create_info_arg  Pointer to create information that can be used to
                          tailor the format of the statement.  Can be NULL,
                          in which case only SQL_MODE is considered when
                          building the statement.
  @param show_database    If true, then print the database before the table
                          name. The database name is only printed in the event
                          that it is different from the current database.
                          If false, then do not print the database before
                          the table name.

  @returns Currently always return 0, but might return error code in the future.
*/

int store_create_info(THD *thd, TABLE_LIST *table_list, String *packet,
                      HA_CREATE_INFO *create_info_arg, bool show_database) {
  List<Item> field_list;
  char tmp[MAX_FIELD_WIDTH], *for_str, buff[128],
      def_value_buf[MAX_FIELD_WIDTH];
  const char *alias;
  String type(tmp, sizeof(tmp), system_charset_info);
  String def_value(def_value_buf, sizeof(def_value_buf), system_charset_info);
  Field **ptr, *field;
  uint primary_key;
  KEY *key_info;
  TABLE *table = table_list->table;
  handler *file = table->file;
  TABLE_SHARE *share = table->s;
  HA_CREATE_INFO create_info;
  bool show_table_options = false;
  bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;
  my_bitmap_map *old_map;
  int error = 0;
  DBUG_ENTER("store_create_info");
  DBUG_PRINT("enter", ("table: %s", table->s->table_name.str));

  restore_record(table, s->default_values);  // Get empty record

  if (share->tmp_table)
    packet->append(STRING_WITH_LEN("CREATE TEMPORARY TABLE "));
  else
    packet->append(STRING_WITH_LEN("CREATE TABLE "));
  if (create_info_arg &&
      (create_info_arg->options & HA_LEX_CREATE_IF_NOT_EXISTS))
    packet->append(STRING_WITH_LEN("IF NOT EXISTS "));
  if (table_list->schema_table)
    alias = table_list->schema_table->table_name;
  else {
    if (lower_case_table_names == 2)
      alias = table->alias;
    else {
      alias = share->table_name.str;
    }
  }

  /*
    Print the database before the table name if told to do that. The
    database name is only printed in the event that it is different
    from the current database.  The main reason for doing this is to
    avoid having to update gazillions of tests and result files, but
    it also saves a few bytes of the binary log.
   */
  if (show_database) {
    const LEX_STRING *const db =
        table_list->schema_table ? &INFORMATION_SCHEMA_NAME : &table->s->db;
    if (!thd->db().str || strcmp(db->str, thd->db().str)) {
      append_identifier(thd, packet, db->str, db->length);
      packet->append(STRING_WITH_LEN("."));
    }
  }

  append_identifier(thd, packet, alias, strlen(alias));
  packet->append(STRING_WITH_LEN(" (\n"));
  /*
    We need this to get default values from the table
    We have to restore the read_set if we are called from insert in case
    of row based replication.
  */
  old_map = tmp_use_all_columns(table, table->read_set);
  auto grd = create_scope_guard(
      [&]() { tmp_restore_column_map(table->read_set, old_map); });
  dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
  const dd::Table *table_obj = nullptr;
  if (share->tmp_table)
    table_obj = table->s->tmp_table_def;
  else {
    if (thd->dd_client()->acquire(dd::String_type(share->db.str),
                                  dd::String_type(share->table_name.str),
                                  &table_obj))
      DBUG_RETURN(true);
    DBUG_EXECUTE_IF("sim_acq_fail_in_store_ci", {
      my_error(ER_UNKNOWN_ERROR_NUMBER, MYF(0), 42);
      DBUG_RETURN(true);
    });
  }

  for (ptr = table->field; (field = *ptr); ptr++) {
    uint flags = field->flags;
    enum_field_types field_type = field->real_type();

    if (ptr != table->field) packet->append(STRING_WITH_LEN(",\n"));

    packet->append(STRING_WITH_LEN("  "));
    append_identifier(thd, packet, field->field_name,
                      strlen(field->field_name));
    packet->append(' ');
    // check for surprises from the previous call to Field::sql_type()
    if (type.ptr() != tmp)
      type.set(tmp, sizeof(tmp), system_charset_info);
    else
      type.set_charset(system_charset_info);

    field->sql_type(type);
    /*
      If the session variable 'show_old_temporals' is enabled and the field
      is a temporal type of old format, add a comment to indicate the same.
    */
    if (thd->variables.show_old_temporals &&
        (field_type == MYSQL_TYPE_TIME || field_type == MYSQL_TYPE_DATETIME ||
         field_type == MYSQL_TYPE_TIMESTAMP))
      type.append(" /* 5.5 binary format */");
    packet->append(type.ptr(), type.length(), system_charset_info);

    bool column_has_explicit_collation = false;
    /* We may not have a table_obj for schema_tables. */
    if (table_obj)
      column_has_explicit_collation =
          table_obj->get_column(field->field_name)->is_explicit_collation();

    if (field->has_charset()) {
      /*
        For string types dump charset name only if field charset is same as
        table charset or was explicitly assigned.
      */
      if (field->charset() != share->table_charset ||
          column_has_explicit_collation) {
        packet->append(STRING_WITH_LEN(" CHARACTER SET "));
        packet->append(field->charset()->csname);
      }
      /*
        For string types dump collation name only if
        collation is not primary for the given charset
        or was explicitly assigned.
      */
      if (!(field->charset()->state & MY_CS_PRIMARY) ||
          column_has_explicit_collation ||
          (field->charset() == &my_charset_utf8mb4_0900_ai_ci &&
           share->table_charset != &my_charset_utf8mb4_0900_ai_ci)) {
        packet->append(STRING_WITH_LEN(" COLLATE "));
        packet->append(field->charset()->name);
      }
    }

    if (field->gcol_info) {
      packet->append(STRING_WITH_LEN(" GENERATED ALWAYS"));
      packet->append(STRING_WITH_LEN(" AS ("));
      char buffer[128];
      String s(buffer, sizeof(buffer), system_charset_info);
      field->gcol_info->print_expr(thd, &s);
      packet->append(s);
      packet->append(STRING_WITH_LEN(")"));
      if (field->stored_in_db)
        packet->append(STRING_WITH_LEN(" STORED"));
      else
        packet->append(STRING_WITH_LEN(" VIRTUAL"));
    }

    if (flags & NOT_NULL_FLAG)
      packet->append(STRING_WITH_LEN(" NOT NULL"));
    else if (field->type() == MYSQL_TYPE_TIMESTAMP) {
      /*
        TIMESTAMP field require explicit NULL flag, because unlike
        all other fields they are treated as NOT NULL by default.
      */
      packet->append(STRING_WITH_LEN(" NULL"));
    }

    if (field->type() == MYSQL_TYPE_GEOMETRY) {
      const Field_geom *field_geom = down_cast<const Field_geom *>(field);
      if (field_geom->get_srid().has_value()) {
        packet->append(STRING_WITH_LEN(" /*!80003 SRID "));
        packet->append_ulonglong(field_geom->get_srid().value());
        packet->append(STRING_WITH_LEN(" */"));
      }
    }
    switch (field->field_storage_type()) {
      case HA_SM_DEFAULT:
        break;
      case HA_SM_DISK:
        packet->append(STRING_WITH_LEN(" /*!50606 STORAGE DISK */"));
        break;
      case HA_SM_MEMORY:
        packet->append(STRING_WITH_LEN(" /*!50606 STORAGE MEMORY */"));
        break;
      default:
        DBUG_ASSERT(0);
        break;
    }

    switch (field->column_format()) {
      case COLUMN_FORMAT_TYPE_DEFAULT:
        break;
      case COLUMN_FORMAT_TYPE_FIXED:
        packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT FIXED */"));
        break;
      case COLUMN_FORMAT_TYPE_DYNAMIC:
        packet->append(STRING_WITH_LEN(" /*!50606 COLUMN_FORMAT DYNAMIC */"));
        break;
      default:
        DBUG_ASSERT(0);
        break;
    }

    if (print_default_clause(field, &def_value, true)) {
      packet->append(STRING_WITH_LEN(" DEFAULT "));
      packet->append(def_value.ptr(), def_value.length(), system_charset_info);
    }

    if (print_on_update_clause(field, &def_value, false)) {
      packet->append(STRING_WITH_LEN(" "));
      packet->append(def_value);
    }

    if (field->auto_flags & Field::NEXT_NUMBER)
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT"));

    if (field->comment.length) {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, field->comment.str, field->comment.length);
    }
  }

  key_info = table->key_info;
  /* Allow update_create_info to update row type */
  create_info.row_type = share->row_type;
  file->update_create_info(&create_info);
  primary_key = share->primary_key;

  for (uint i = 0; i < share->keys; i++, key_info++) {
    KEY_PART_INFO *key_part = key_info->key_part;
    bool found_primary = 0;
    packet->append(STRING_WITH_LEN(",\n  "));

    if (i == primary_key && !strcmp(key_info->name, primary_key_name)) {
      found_primary = 1;
      /*
        No space at end, because a space will be added after where the
        identifier would go, but that is not added for primary key.
      */
      packet->append(STRING_WITH_LEN("PRIMARY KEY"));
    } else if (key_info->flags & HA_NOSAME)
      packet->append(STRING_WITH_LEN("UNIQUE KEY "));
    else if (key_info->flags & HA_FULLTEXT)
      packet->append(STRING_WITH_LEN("FULLTEXT KEY "));
    else if (key_info->flags & HA_SPATIAL)
      packet->append(STRING_WITH_LEN("SPATIAL KEY "));
    else
      packet->append(STRING_WITH_LEN("KEY "));

    if (!found_primary)
      append_identifier(thd, packet, key_info->name, strlen(key_info->name));

    packet->append(STRING_WITH_LEN(" ("));

    for (uint j = 0; j < key_info->user_defined_key_parts; j++, key_part++) {
      if (j) packet->append(',');

      if (key_part->field)
        append_identifier(thd, packet, key_part->field->field_name,
                          strlen(key_part->field->field_name));
      if (key_part->field &&
          (key_part->length !=
               table->field[key_part->fieldnr - 1]->key_length() &&
           !(key_info->flags & (HA_FULLTEXT | HA_SPATIAL)))) {
        packet->append_parenthesized((long)key_part->length /
                                     key_part->field->charset()->mbmaxlen);
      }
      if (key_part->key_part_flag & HA_REVERSE_SORT)
        packet->append(STRING_WITH_LEN(" DESC"));
    }
    packet->append(')');
    store_key_options(thd, packet, table, key_info);
    if (key_info->parser) {
      LEX_STRING *parser_name = plugin_name(key_info->parser);
      packet->append(STRING_WITH_LEN(" /*!50100 WITH PARSER "));
      append_identifier(thd, packet, parser_name->str, parser_name->length);
      packet->append(STRING_WITH_LEN(" */ "));
    }
  }

  /*
    Get possible foreign key definitions stored in InnoDB and append them
    to the CREATE TABLE statement
  */

  if ((for_str = file->get_foreign_key_create_info())) {
    packet->append(for_str, strlen(for_str));
    file->free_foreign_key_create_info(for_str);
  }

  packet->append(STRING_WITH_LEN("\n)"));
  if (!foreign_db_mode) {
    show_table_options = true;

    // Show tablespace name only if it is explicitly provided by user.
    bool show_tablespace = false;
    if (share->tmp_table) {
      // Innodb allows temporary tables in be in system temporary tablespace.
      show_tablespace = share->tablespace;
    } else if (share->tablespace) {
      dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
      const dd::Table *table_obj = nullptr;
      if (thd->dd_client()->acquire(dd::String_type(share->db.str),
                                    dd::String_type(share->table_name.str),
                                    &table_obj)) {
        DBUG_RETURN(true);
      }
      DBUG_ASSERT(table_obj != nullptr);
      show_tablespace = table_obj->is_explicit_tablespace();
    }

    /* TABLESPACE and STORAGE */
    if (show_tablespace || share->default_storage_media != HA_SM_DEFAULT) {
      packet->append(STRING_WITH_LEN(" /*!50100"));
      if (show_tablespace) {
        packet->append(STRING_WITH_LEN(" TABLESPACE "));
        append_identifier(thd, packet, share->tablespace,
                          strlen(share->tablespace));
      }

      if (share->default_storage_media == HA_SM_DISK)
        packet->append(STRING_WITH_LEN(" STORAGE DISK"));
      if (share->default_storage_media == HA_SM_MEMORY)
        packet->append(STRING_WITH_LEN(" STORAGE MEMORY"));

      packet->append(STRING_WITH_LEN(" */"));
    }

    /*
      IF   check_create_info
      THEN add ENGINE only if it was used when creating the table
    */
    if (!create_info_arg ||
        (create_info_arg->used_fields & HA_CREATE_USED_ENGINE)) {
      packet->append(STRING_WITH_LEN(" ENGINE="));
      /*
        TODO: Replace this if with the else branch. Not done yet since
        NDB handlerton says "ndbcluster" and ha_ndbcluster says "NDBCLUSTER".
      */
      if (table->part_info) {
        packet->append(ha_resolve_storage_engine_name(
            table->part_info->default_engine_type));
      } else {
        packet->append(file->table_type());
      }
    }

    /*
      Add AUTO_INCREMENT=... if there is an AUTO_INCREMENT column,
      and NEXT_ID > 1 (the default).  We must not print the clause
      for engines that do not support this as it would break the
      import of dumps, but as of this writing, the test for whether
      AUTO_INCREMENT columns are allowed and wether AUTO_INCREMENT=...
      is supported is identical, !(file->table_flags() & HA_NO_AUTO_INCREMENT))
      Because of that, we do not explicitly test for the feature,
      but may extrapolate its existence from that of an AUTO_INCREMENT column.
    */

    if (create_info.auto_increment_value > 1) {
      char *end;
      packet->append(STRING_WITH_LEN(" AUTO_INCREMENT="));
      end = longlong10_to_str(create_info.auto_increment_value, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->table_charset) {
      /*
        IF   check_create_info
        THEN add DEFAULT CHARSET only if it was used when creating the table
      */
      if (!create_info_arg ||
          (create_info_arg->used_fields & HA_CREATE_USED_DEFAULT_CHARSET)) {
        packet->append(STRING_WITH_LEN(" DEFAULT CHARSET="));
        packet->append(share->table_charset->csname);
        if (!(share->table_charset->state & MY_CS_PRIMARY) ||
            share->table_charset == &my_charset_utf8mb4_0900_ai_ci) {
          packet->append(STRING_WITH_LEN(" COLLATE="));
          packet->append(table->s->table_charset->name);
        }
      }
    }

    if (share->min_rows) {
      char *end;
      packet->append(STRING_WITH_LEN(" MIN_ROWS="));
      end = longlong10_to_str(share->min_rows, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->max_rows && !table_list->schema_table) {
      char *end;
      packet->append(STRING_WITH_LEN(" MAX_ROWS="));
      end = longlong10_to_str(share->max_rows, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->avg_row_length) {
      char *end;
      packet->append(STRING_WITH_LEN(" AVG_ROW_LENGTH="));
      end = longlong10_to_str(share->avg_row_length, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }

    if (share->db_create_options & HA_OPTION_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=1"));
    if (share->db_create_options & HA_OPTION_NO_PACK_KEYS)
      packet->append(STRING_WITH_LEN(" PACK_KEYS=0"));
    if (share->db_create_options & HA_OPTION_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=1"));
    if (share->db_create_options & HA_OPTION_NO_STATS_PERSISTENT)
      packet->append(STRING_WITH_LEN(" STATS_PERSISTENT=0"));
    if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_ON)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=1"));
    else if (share->stats_auto_recalc == HA_STATS_AUTO_RECALC_OFF)
      packet->append(STRING_WITH_LEN(" STATS_AUTO_RECALC=0"));
    if (share->stats_sample_pages != 0) {
      char *end;
      packet->append(STRING_WITH_LEN(" STATS_SAMPLE_PAGES="));
      end = longlong10_to_str(share->stats_sample_pages, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    /* We use CHECKSUM, instead of TABLE_CHECKSUM, for backward compability */
    if (share->db_create_options & HA_OPTION_CHECKSUM)
      packet->append(STRING_WITH_LEN(" CHECKSUM=1"));
    if (share->db_create_options & HA_OPTION_DELAY_KEY_WRITE)
      packet->append(STRING_WITH_LEN(" DELAY_KEY_WRITE=1"));

    /*
      If 'show_create_table_verbosity' is enabled, the row format would
      be displayed in the output of SHOW CREATE TABLE even if default
      row format is used. Otherwise only the explicitly mentioned
      row format would be displayed.
    */
    if (thd->variables.show_create_table_verbosity) {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint)share->real_row_type]);
    } else if (create_info.row_type != ROW_TYPE_DEFAULT) {
      packet->append(STRING_WITH_LEN(" ROW_FORMAT="));
      packet->append(ha_row_type[(uint)create_info.row_type]);
    }
    if (table->s->key_block_size) {
      char *end;
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end = longlong10_to_str(table->s->key_block_size, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    if (table->s->compress.length) {
      packet->append(STRING_WITH_LEN(" COMPRESSION="));
      append_unescaped(packet, share->compress.str, share->compress.length);
    }
    if (table->s->encrypt_type.length) {
      packet->append(STRING_WITH_LEN(" ENCRYPTION="));
      append_unescaped(packet, share->encrypt_type.str,
                       share->encrypt_type.length);
    }
    table->file->append_create_info(packet);
    if (share->comment.length) {
      packet->append(STRING_WITH_LEN(" COMMENT="));
      append_unescaped(packet, share->comment.str, share->comment.length);
    }
    if (share->connect_string.length) {
      packet->append(STRING_WITH_LEN(" CONNECTION="));
      append_unescaped(packet, share->connect_string.str,
                       share->connect_string.length);
    }
    if (share->secondary_engine.str != nullptr) {
      packet->append(" SECONDARY_ENGINE=");
      packet->append(share->secondary_engine.str,
                     share->secondary_engine.length);
    }
    append_directory(thd, packet, "DATA", create_info.data_file_name);
    append_directory(thd, packet, "INDEX", create_info.index_file_name);
  }
  {
    if (table->part_info &&
        !(table->s->db_type()->partition_flags &&
          (table->s->db_type()->partition_flags() & HA_USE_AUTO_PARTITION) &&
          table->part_info->is_auto_partitioned)) {
      /*
        Partition syntax for CREATE TABLE is at the end of the syntax.
      */
      uint part_syntax_len;
      char *part_syntax;
      String comment_start;
      table->part_info->set_show_version_string(&comment_start);
      if ((part_syntax = generate_partition_syntax(
               table->part_info, &part_syntax_len, false, show_table_options,
               true,  // For proper quoting.
               comment_start.c_ptr()))) {
        packet->append(comment_start);
        if (packet->append(part_syntax, part_syntax_len) ||
            packet->append(STRING_WITH_LEN(" */")))
          error = 1;
        my_free(part_syntax);
      }
    }
  }
  DBUG_RETURN(error);
}

static void store_key_options(THD *thd, String *packet, TABLE *table,
                              KEY *key_info) {
  bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;
  char *end, buff[32];

  if (!foreign_db_mode) {
    /*
      Send USING clause only if key algorithm was explicitly specified
      at the table creation time.
    */
    if (key_info->is_algorithm_explicit) {
      if (key_info->algorithm == HA_KEY_ALG_BTREE)
        packet->append(STRING_WITH_LEN(" USING BTREE"));

      if (key_info->algorithm == HA_KEY_ALG_HASH)
        packet->append(STRING_WITH_LEN(" USING HASH"));

      if (key_info->algorithm == HA_KEY_ALG_RTREE) {
        /* We should send USING only in non-default case: non-spatial rtree. */
        DBUG_ASSERT(!(key_info->flags & HA_SPATIAL));
        packet->append(STRING_WITH_LEN(" USING RTREE"));
      }
    }

    if ((key_info->flags & HA_USES_BLOCK_SIZE) &&
        table->s->key_block_size != key_info->block_size) {
      packet->append(STRING_WITH_LEN(" KEY_BLOCK_SIZE="));
      end = longlong10_to_str(key_info->block_size, buff, 10);
      packet->append(buff, (uint)(end - buff));
    }
    DBUG_ASSERT(MY_TEST(key_info->flags & HA_USES_COMMENT) ==
                (key_info->comment.length > 0));
    if (key_info->flags & HA_USES_COMMENT) {
      packet->append(STRING_WITH_LEN(" COMMENT "));
      append_unescaped(packet, key_info->comment.str, key_info->comment.length);
    }

    if (!key_info->is_visible)
      packet->append(STRING_WITH_LEN(" /*!80000 INVISIBLE */"));
  }
}

void view_store_options(THD *thd, TABLE_LIST *table, String *buff) {
  append_algorithm(table, buff);
  append_definer(thd, buff, table->definer.user, table->definer.host);
  if (table->view_suid)
    buff->append(STRING_WITH_LEN("SQL SECURITY DEFINER "));
  else
    buff->append(STRING_WITH_LEN("SQL SECURITY INVOKER "));
}

/**
  Append ALGORITHM clause to the given buffer.

  @param table              VIEW definition
  @param [in,out] buff      buffer to hold ALGORITHM clause
*/

static void append_algorithm(TABLE_LIST *table, String *buff) {
  buff->append(STRING_WITH_LEN("ALGORITHM="));
  switch ((int8)table->algorithm) {
    case VIEW_ALGORITHM_UNDEFINED:
      buff->append(STRING_WITH_LEN("UNDEFINED "));
      break;
    case VIEW_ALGORITHM_TEMPTABLE:
      buff->append(STRING_WITH_LEN("TEMPTABLE "));
      break;
    case VIEW_ALGORITHM_MERGE:
      buff->append(STRING_WITH_LEN("MERGE "));
      break;
    default:
      DBUG_ASSERT(0);  // never should happen
  }
}

/**
  Append DEFINER clause to the given buffer.

  @param thd           thread handle
  @param [in,out] buffer        buffer to hold DEFINER clause
  @param definer_user  user name part of definer
  @param definer_host  host name part of definer
*/

void append_definer(THD *thd, String *buffer, const LEX_CSTRING &definer_user,
                    const LEX_CSTRING &definer_host) {
  buffer->append(STRING_WITH_LEN("DEFINER="));
  append_identifier(thd, buffer, definer_user.str, definer_user.length);
  buffer->append('@');
  append_identifier(thd, buffer, definer_host.str, definer_host.length);
  buffer->append(' ');
}

static int view_store_create_info(THD *thd, TABLE_LIST *table, String *buff) {
  bool foreign_db_mode = (thd->variables.sql_mode & MODE_ANSI) != 0;

  // Print compact view name if the view belongs to the current database
  bool compact_view_name =
      thd->db().str != NULL && (!strcmp(thd->db().str, table->view_db.str));

  buff->append(STRING_WITH_LEN("CREATE "));
  if (!foreign_db_mode) {
    view_store_options(thd, table, buff);
  }
  buff->append(STRING_WITH_LEN("VIEW "));
  if (!compact_view_name) {
    append_identifier(thd, buff, table->view_db.str, table->view_db.length);
    buff->append('.');
  }
  append_identifier(thd, buff, table->view_name.str, table->view_name.length);
  print_derived_column_names(thd, buff, table->derived_column_names());

  buff->append(STRING_WITH_LEN(" AS "));

  /*
    We can't just use table->query, because our SQL_MODE may trigger
    a different syntax, like when ANSI_QUOTES is defined.

    Append the db name only if it is not the same as connection's
    database.
  */
  table->view_query()->unit->print(
      buff, enum_query_type(QT_TO_ARGUMENT_CHARSET | QT_NO_DEFAULT_DB));

  if (table->with_check != VIEW_CHECK_NONE) {
    if (table->with_check == VIEW_CHECK_LOCAL)
      buff->append(STRING_WITH_LEN(" WITH LOCAL CHECK OPTION"));
    else
      buff->append(STRING_WITH_LEN(" WITH CASCADED CHECK OPTION"));
  }
  return 0;
}

/****************************************************************************
  Return info about all processes
  returns for each thread: thread id, user, host, db, command, info
****************************************************************************/
class thread_info {
 public:
  thread_info()
      : thread_id(0),
        start_time_in_secs(0),
        command(0),
        user(NULL),
        host(NULL),
        db(NULL),
        proc_info(NULL),
        state_info(NULL) {}

  my_thread_id thread_id;
  time_t start_time_in_secs;
  uint command;
  const char *user, *host, *db, *proc_info, *state_info;
  CSET_STRING query_string;
};

// For sorting by thread_id.
class thread_info_compare
    : public std::binary_function<const thread_info *, const thread_info *,
                                  bool> {
 public:
  bool operator()(const thread_info *p1, const thread_info *p2) {
    return p1->thread_id < p2->thread_id;
  }
};

static const char *thread_state_info(THD *tmp) {
  if (tmp->get_protocol()->get_rw_status()) {
    if (tmp->get_protocol()->get_rw_status() == 2)
      return "Sending to client";
    else if (tmp->get_command() == COM_SLEEP)
      return "";
    else
      return "Receiving from client";
  } else {
    MUTEX_LOCK(lock, &tmp->LOCK_current_cond);
    if (tmp->proc_info)
      return tmp->proc_info;
    else if (tmp->current_cond.load())
      return "Waiting on cond";
    else
      return NULL;
  }
}

/**
  This class implements callback function used by mysqld_list_processes() to
  list all the client process information.
*/
typedef Mem_root_array<thread_info *> Thread_info_array;
class List_process_list : public Do_THD_Impl {
 private:
  /* Username of connected client. */
  const char *m_user;
  Thread_info_array *m_thread_infos;
  /* THD of connected client. */
  THD *m_client_thd;
  size_t m_max_query_length;

 public:
  List_process_list(const char *user_value, Thread_info_array *thread_infos,
                    THD *thd_value, size_t max_query_length)
      : m_user(user_value),
        m_thread_infos(thread_infos),
        m_client_thd(thd_value),
        m_max_query_length(max_query_length) {}

  virtual void operator()(THD *inspect_thd) {
    Security_context *inspect_sctx = inspect_thd->security_context();
    LEX_CSTRING inspect_sctx_user = inspect_sctx->user();
    LEX_CSTRING inspect_sctx_host = inspect_sctx->host();
    LEX_CSTRING inspect_sctx_host_or_ip = inspect_sctx->host_or_ip();

    mysql_mutex_lock(&inspect_thd->LOCK_thd_protocol);
    if ((!(inspect_thd->get_protocol() &&
           inspect_thd->get_protocol()->connection_alive()) &&
         !inspect_thd->system_thread) ||
        (m_user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                    strcmp(inspect_sctx_user.str, m_user)))) {
      mysql_mutex_unlock(&inspect_thd->LOCK_thd_protocol);
      return;
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_protocol);

    thread_info *thd_info = new (*THR_MALLOC) thread_info;

    /* ID */
    thd_info->thread_id = inspect_thd->thread_id();

    /* USER */
    if (inspect_sctx_user.str)
      thd_info->user = m_client_thd->mem_strdup(inspect_sctx_user.str);
    else if (inspect_thd->system_thread)
      thd_info->user = "system user";
    else
      thd_info->user = "unauthenticated user";

    /* HOST */
    if (inspect_thd->peer_port &&
        (inspect_sctx_host.length || inspect_sctx->ip().length) &&
        m_client_thd->security_context()->host_or_ip().str[0]) {
      if ((thd_info->host =
               (char *)m_client_thd->alloc(LIST_PROCESS_HOST_LEN + 1)))
        snprintf((char *)thd_info->host, LIST_PROCESS_HOST_LEN, "%s:%u",
                 inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
    } else
      thd_info->host = m_client_thd->mem_strdup(
          inspect_sctx_host_or_ip.str[0]
              ? inspect_sctx_host_or_ip.str
              : inspect_sctx_host.length ? inspect_sctx_host.str : "");

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data", {
      if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
          inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
        DEBUG_SYNC(m_client_thd,
                   "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
    });
    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db = inspect_thd->db().str;
    if (db) thd_info->db = m_client_thd->mem_strdup(db);

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION)
      thd_info->proc_info = "Killed";
    thd_info->command = (int)inspect_thd->get_command();  // Used for !killed.

    /* STATE */
    thd_info->state_info = thread_state_info(inspect_thd);

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str = inspect_thd->query().str;
      size_t query_length = inspect_thd->query().length;
      String buf;
      if (inspect_thd->is_a_srv_session()) {
        buf.append(query_length ? "PLUGIN: " : "PLUGIN");

        if (query_length) buf.append(query_str, query_length);

        query_str = buf.c_ptr();
        query_length = buf.length();
      }
      /* No else. We need fall-through */
      if (query_str) {
        const size_t width = min<size_t>(m_max_query_length, query_length);
        const char *q = m_client_thd->strmake(query_str, width);
        /* Safety: in case strmake failed, we set length to 0. */
        thd_info->query_string =
            CSET_STRING(q, q ? width : 0, inspect_thd->charset());
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    thd_info->start_time_in_secs = inspect_thd->query_start_in_secs();

    m_thread_infos->push_back(thd_info);
  }
};

void mysqld_list_processes(THD *thd, const char *user, bool verbose) {
  Item *field;
  List<Item> field_list;
  Thread_info_array thread_infos(thd->mem_root);
  size_t max_query_length =
      (verbose ? thd->variables.max_allowed_packet : PROCESS_LIST_WIDTH);
  Protocol *protocol = thd->get_protocol();
  DBUG_ENTER("mysqld_list_processes");

  field_list.push_back(
      new Item_int(NAME_STRING("Id"), 0, MY_INT64_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("User", USERNAME_CHAR_LENGTH));
  field_list.push_back(new Item_empty_string("Host", LIST_PROCESS_HOST_LEN));
  field_list.push_back(field = new Item_empty_string("db", NAME_CHAR_LEN));
  field->maybe_null = 1;
  field_list.push_back(new Item_empty_string("Command", 16));
  field_list.push_back(field = new Item_return_int("Time", 7, MYSQL_TYPE_LONG));
  field->unsigned_flag = 0;
  field_list.push_back(field = new Item_empty_string("State", 30));
  field->maybe_null = 1;
  field_list.push_back(field = new Item_empty_string("Info", max_query_length));
  field->maybe_null = 1;
  if (thd->send_result_metadata(&field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_VOID_RETURN;

  if (!thd->killed) {
    thread_infos.reserve(Global_THD_manager::get_instance()->get_thd_count());
    List_process_list list_process_list(user, &thread_infos, thd,
                                        max_query_length);
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&list_process_list);
  }

  // Return list sorted by thread_id.
  std::sort(thread_infos.begin(), thread_infos.end(), thread_info_compare());

  time_t now = my_time(0);
  for (size_t ix = 0; ix < thread_infos.size(); ++ix) {
    thread_info *thd_info = thread_infos.at(ix);
    protocol->start_row();
    protocol->store((ulonglong)thd_info->thread_id);
    protocol->store(thd_info->user, system_charset_info);
    protocol->store(thd_info->host, system_charset_info);
    protocol->store(thd_info->db, system_charset_info);
    if (thd_info->proc_info)
      protocol->store(thd_info->proc_info, system_charset_info);
    else
      protocol->store(command_name[thd_info->command].str, system_charset_info);
    if (thd_info->start_time_in_secs)
      protocol->store_long((longlong)(now - thd_info->start_time_in_secs));
    else
      protocol->store_null();
    protocol->store(thd_info->state_info, system_charset_info);
    protocol->store(thd_info->query_string.str(),
                    thd_info->query_string.charset());
    if (protocol->end_row()) break; /* purecov: inspected */
  }
  my_eof(thd);
  DBUG_VOID_RETURN;
}

/**
  This class implements callback function used by fill_schema_processlist()
  to populate all the client process information into I_S table.
*/
class Fill_process_list : public Do_THD_Impl {
 private:
  /* THD of connected client. */
  THD *m_client_thd;
  /* Information of each process is added as records into this table. */
  TABLE_LIST *m_tables;

 public:
  Fill_process_list(THD *thd_value, TABLE_LIST *tables_value)
      : m_client_thd(thd_value), m_tables(tables_value) {}

  virtual void operator()(THD *inspect_thd) {
    Security_context *inspect_sctx = inspect_thd->security_context();
    LEX_CSTRING inspect_sctx_user = inspect_sctx->user();
    LEX_CSTRING inspect_sctx_host = inspect_sctx->host();
    LEX_CSTRING inspect_sctx_host_or_ip = inspect_sctx->host_or_ip();
    const char *client_priv_user =
        m_client_thd->security_context()->priv_user().str;
    const char *user =
        m_client_thd->security_context()->check_access(PROCESS_ACL)
            ? NullS
            : client_priv_user;

    if ((!inspect_thd->get_protocol()->connection_alive() &&
         !inspect_thd->system_thread) ||
        (user && (inspect_thd->system_thread || !inspect_sctx_user.str ||
                  strcmp(inspect_sctx_user.str, user))))
      return;

    TABLE *table = m_tables->table;
    restore_record(table, s->default_values);

    /* ID */
    table->field[0]->store((ulonglong)inspect_thd->thread_id(), true);

    /* USER */
    const char *val = NULL;
    if (inspect_sctx_user.str)
      val = inspect_sctx_user.str;
    else if (inspect_thd->system_thread)
      val = "system user";
    else
      val = "unauthenticated user";
    table->field[1]->store(val, strlen(val), system_charset_info);

    /* HOST */
    if (inspect_thd->peer_port &&
        (inspect_sctx_host.length || inspect_sctx->ip().length) &&
        m_client_thd->security_context()->host_or_ip().str[0]) {
      char host[LIST_PROCESS_HOST_LEN + 1];
      snprintf(host, LIST_PROCESS_HOST_LEN, "%s:%u",
               inspect_sctx_host_or_ip.str, inspect_thd->peer_port);
      table->field[2]->store(host, strlen(host), system_charset_info);
    } else
      table->field[2]->store(inspect_sctx_host_or_ip.str,
                             inspect_sctx_host_or_ip.length,
                             system_charset_info);

    DBUG_EXECUTE_IF("processlist_acquiring_dump_threads_LOCK_thd_data", {
      if (inspect_thd->get_command() == COM_BINLOG_DUMP ||
          inspect_thd->get_command() == COM_BINLOG_DUMP_GTID)
        DEBUG_SYNC(m_client_thd,
                   "processlist_after_LOCK_thd_list_before_LOCK_thd_data");
    });
    /* DB */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_data);
    const char *db = inspect_thd->db().str;
    if (db) {
      table->field[3]->store(db, strlen(db), system_charset_info);
      table->field[3]->set_notnull();
    }

    /* COMMAND */
    if (inspect_thd->killed == THD::KILL_CONNECTION) {
      val = "Killed";
      table->field[4]->store(val, strlen(val), system_charset_info);
    } else
      table->field[4]->store(command_name[inspect_thd->get_command()].str,
                             command_name[inspect_thd->get_command()].length,
                             system_charset_info);

    /* STATE */
    val = thread_state_info(inspect_thd);
    if (val) {
      table->field[6]->store(val, strlen(val), system_charset_info);
      table->field[6]->set_notnull();
    }

    mysql_mutex_unlock(&inspect_thd->LOCK_thd_data);

    /* INFO */
    mysql_mutex_lock(&inspect_thd->LOCK_thd_query);
    {
      const char *query_str = inspect_thd->query().str;
      size_t query_length = inspect_thd->query().length;
      String buf;
      if (inspect_thd->is_a_srv_session()) {
        buf.append(query_length ? "PLUGIN: " : "PLUGIN");

        if (query_length) buf.append(query_str, query_length);

        query_str = buf.c_ptr();
        query_length = buf.length();
      }
      /* No else. We need fall-through */
      if (query_str) {
        const size_t width = min<size_t>(PROCESS_LIST_INFO_WIDTH, query_length);
        table->field[7]->store(query_str, width, inspect_thd->charset());
        table->field[7]->set_notnull();
      }
    }
    mysql_mutex_unlock(&inspect_thd->LOCK_thd_query);

    /* MYSQL_TIME */
    if (inspect_thd->query_start_in_secs())
      table->field[5]->store((longlong)(my_micro_time() / 1000000 -
                                        inspect_thd->query_start_in_secs()),
                             false);
    else
      table->field[5]->store(0, false);

    schema_table_store_record(m_client_thd, table);
  }
};

static int fill_schema_processlist(THD *thd, TABLE_LIST *tables, Item *) {
  DBUG_ENTER("fill_schema_processlist");

  Fill_process_list fill_process_list(thd, tables);
  if (!thd->killed) {
    Global_THD_manager::get_instance()->do_for_all_thd_copy(&fill_process_list);
  }
  DBUG_RETURN(0);
}

/*****************************************************************************
  Status functions
*****************************************************************************/
Status_var_array all_status_vars(0);
bool status_vars_inited = 0;
/* Version counter, protected by LOCK_STATUS. */
ulonglong status_var_array_version = 0;

static inline int show_var_cmp(const SHOW_VAR *var1, const SHOW_VAR *var2) {
  return strcmp(var1->name, var2->name);
}

class Show_var_cmp
    : public std::binary_function<const SHOW_VAR &, const SHOW_VAR &, bool> {
 public:
  bool operator()(const SHOW_VAR &var1, const SHOW_VAR &var2) {
    return show_var_cmp(&var1, &var2) < 0;
  }
};

static inline bool is_show_undef(const SHOW_VAR &var) {
  return var.type == SHOW_UNDEF;
}

/*
  Deletes all the SHOW_UNDEF elements from the array.
  Shrinks array capacity to zero if it is completely empty.
*/
static void shrink_var_array(Status_var_array *array) {
  /* remove_if maintains order for the elements that are *not* removed */
  array->erase(std::remove_if(array->begin(), array->end(), is_show_undef),
               array->end());
  if (array->empty()) Status_var_array().swap(*array);
}

/*
  Adds an array of SHOW_VAR entries to the output of SHOW STATUS

  SYNOPSIS
    add_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to add to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    The handling of all_status_vars[] is completely internal, it's allocated
    automatically when something is added to it, and deleted completely when
    the last entry is removed.

    As a special optimization, if add_status_vars() is called before
    init_status_vars(), it assumes "startup mode" - neither concurrent access
    to the array nor SHOW STATUS are possible (thus it skips locks and sort)

    The last entry of the all_status_vars[] should always be {0,0,SHOW_UNDEF}
*/
int add_status_vars(const SHOW_VAR *list) {
  MUTEX_LOCK(lock, status_vars_inited ? &LOCK_status : NULL);

  try {
    while (list->name) all_status_vars.push_back(*list++);
  } catch (const std::bad_alloc &) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR),
             static_cast<int>(sizeof(Status_var_array::value_type)));
    return 1;
  }

  if (status_vars_inited)
    std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());

  status_var_array_version++;
  return 0;
}

/*
  Make all_status_vars[] usable for SHOW STATUS

  NOTE
    See add_status_vars(). Before init_status_vars() call, add_status_vars()
    works in a special fast "startup" mode. Thus init_status_vars()
    should be called as late as possible but before enabling multi-threading.
*/
void init_status_vars() {
  status_vars_inited = 1;
  std::sort(all_status_vars.begin(), all_status_vars.end(), Show_var_cmp());
  status_var_array_version++;
}

void reset_status_vars() {
  Status_var_array::iterator ptr = all_status_vars.begin();
  Status_var_array::iterator last = all_status_vars.end();
  for (; ptr < last; ptr++) {
    /* Note that SHOW_LONG_NOFLUSH variables are not reset */
    if (ptr->type == SHOW_LONG || ptr->type == SHOW_SIGNED_LONG)
      *(ulong *)ptr->value = 0;
  }
}

/*
  Current version of the all_status_vars.
*/
ulonglong get_status_vars_version(void) { return (status_var_array_version); }

/*
  catch-all cleanup function, cleans up everything no matter what

  DESCRIPTION
    This function is not strictly required if all add_to_status/
    remove_status_vars are properly paired, but it's a safety measure that
    deletes everything from the all_status_vars[] even if some
    remove_status_vars were forgotten
*/
void free_status_vars() {
  Status_var_array().swap(all_status_vars);
  status_var_array_version++;
}

/**
  @brief           Get the value of given status variable

  @param[in]       thd        thread handler
  @param[in]       list       list of SHOW_VAR objects in which function should
                              search
  @param[in]       name       name of the status variable
  @param[in]       var_type   Variable type
  @param[in,out]   value      buffer in which value of the status variable
                              needs to be filled in
  @param[in,out]   length     filled with buffer length

  @return          status
    @retval        false      if variable is not found in the list
    @retval        true       if variable is found in the list
*/

bool get_status_var(THD *thd, SHOW_VAR *list, const char *name,
                    char *const value, enum_var_type var_type, size_t *length) {
  for (; list->name; list++) {
    int res = strcmp(list->name, name);
    if (res == 0) {
      /*
        if var->type is SHOW_FUNC, call the function.
        Repeat as necessary, if new var is again SHOW_FUNC
       */
      SHOW_VAR tmp;
      for (; list->type == SHOW_FUNC; list = &tmp)
        ((mysql_show_var_func)(list->value))(thd, &tmp, value);

      get_one_variable(thd, list, var_type, list->type, NULL, NULL, value,
                       length);
      return true;
    }
  }
  return false;
}

/*
  Removes an array of SHOW_VAR entries from the output of SHOW STATUS

  SYNOPSIS
    remove_status_vars(SHOW_VAR *list)
    list - an array of SHOW_VAR entries to remove to all_status_vars
           the last entry must be {0,0,SHOW_UNDEF}

  NOTE
    there's lots of room for optimizing this, especially in non-sorted mode,
    but nobody cares - it may be called only in case of failed plugin
    initialization in the mysqld startup.
*/

void remove_status_vars(SHOW_VAR *list) {
  if (status_vars_inited) {
    mysql_mutex_lock(&LOCK_status);
    size_t a = 0, b = all_status_vars.size(), c = (a + b) / 2;

    for (; list->name; list++) {
      int res = 0;
      for (a = 0, b = all_status_vars.size(); b - a > 1; c = (a + b) / 2) {
        res = show_var_cmp(list, &all_status_vars[c]);
        if (res < 0)
          b = c;
        else if (res > 0)
          a = c;
        else
          break;
      }
      if (res == 0) all_status_vars[c].type = SHOW_UNDEF;
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
    mysql_mutex_unlock(&LOCK_status);
  } else {
    uint i;
    for (; list->name; list++) {
      for (i = 0; i < all_status_vars.size(); i++) {
        if (show_var_cmp(list, &all_status_vars[i])) continue;
        all_status_vars[i].type = SHOW_UNDEF;
        break;
      }
    }
    shrink_var_array(&all_status_vars);
    status_var_array_version++;
  }
}

/**
  Returns the value of a system or a status variable.

  @param thd            The handle of the current THD.
  @param variable       Details of the variable.
  @param value_type     Variable type.
  @param show_type      Variable show type.
  @param status_var     Status values or NULL if for system variable.
  @param [out] charset  Character set of the value.
  @param [in,out] buff  Buffer to store the value.
  @param [out] length   Length of the value.

  @returns              Pointer to the value buffer.
*/

const char *get_one_variable(THD *thd, const SHOW_VAR *variable,
                             enum_var_type value_type, SHOW_TYPE show_type,
                             System_status_var *status_var,
                             const CHARSET_INFO **charset, char *buff,
                             size_t *length) {
  return get_one_variable_ext(thd, thd, variable, value_type, show_type,
                              status_var, charset, buff, length);
}

/**
  @brief Returns the value of a system or a status variable.

  @param running_thd     The handle of the current THD.
  @param target_thd      The handle of the remote THD.
  @param variable        Details of the variable.
  @param value_type      Variable type.
  @param show_type       Variable show type.
  @param status_var      Status values or NULL if for system variable.
  @param [out] charset   Character set of the value.
  @param [in,out] buff   Buffer to store the value.
  @param [out] length    Length of the value.

  @returns               Pointer to the value buffer.
*/

const char *get_one_variable_ext(THD *running_thd, THD *target_thd,
                                 const SHOW_VAR *variable,
                                 enum_var_type value_type, SHOW_TYPE show_type,
                                 System_status_var *status_var,
                                 const CHARSET_INFO **charset, char *buff,
                                 size_t *length) {
  const char *value;
  const CHARSET_INFO *value_charset;

  if (show_type == SHOW_SYS) {
    LEX_STRING null_lex_str;
    null_lex_str.str = 0;  // For sys_var->value_ptr()
    null_lex_str.length = 0;
    sys_var *var = ((sys_var *)variable->value);
    show_type = var->show_type();
    value = (char *)var->value_ptr(running_thd, target_thd, value_type,
                                   &null_lex_str);
    value_charset = var->charset(target_thd);
  } else {
    value = variable->value;
    value_charset = system_charset_info;
  }

  const char *pos = buff;
  const char *end = buff;

  /*
    Note that value may == buff. All SHOW_xxx code below should still work.
  */
  switch (show_type) {
    case SHOW_DOUBLE_STATUS:
      value = ((char *)status_var + (ulong)value);
      /* 6 is the default precision for '%f' in sprintf() */
      end = buff + my_fcvt(*(double *)value, 6, buff, NULL);
      value_charset = system_charset_info;
      break;

    case SHOW_DOUBLE:
      /* 6 is the default precision for '%f' in sprintf() */
      end = buff + my_fcvt(*(double *)value, 6, buff, NULL);
      value_charset = system_charset_info;
      break;

    case SHOW_LONG_STATUS:
      value = ((char *)status_var + (ulong)value);
      end = int10_to_str(*(long *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONG:
      /* the difference lies in refresh_status() */
    case SHOW_LONG_NOFLUSH:
      end = int10_to_str(*(long *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_SIGNED_LONG:
      end = int10_to_str(*(long *)value, buff, -10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONGLONG_STATUS:
      value = ((char *)status_var + (ulong)value);
      end = longlong10_to_str(*(longlong *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_LONGLONG:
      end = longlong10_to_str(*(longlong *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_HA_ROWS:
      end = longlong10_to_str((longlong) * (ha_rows *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_BOOL:
      end = my_stpcpy(buff, *(bool *)value ? "ON" : "OFF");
      value_charset = system_charset_info;
      break;

    case SHOW_MY_BOOL:
      end = my_stpcpy(buff, *(bool *)value ? "ON" : "OFF");
      value_charset = system_charset_info;
      break;

    case SHOW_INT:
      end = int10_to_str((long)*(uint32 *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_HAVE: {
      SHOW_COMP_OPTION tmp = *(SHOW_COMP_OPTION *)value;
      pos = show_comp_option_name[(int)tmp];
      end = strend(pos);
      value_charset = system_charset_info;
      break;
    }

    case SHOW_CHAR: {
      if (!(pos = value)) pos = "";
      end = strend(pos);
      break;
    }

    case SHOW_CHAR_PTR: {
      if (!(pos = *(char **)value)) pos = "";

      end = strend(pos);
      break;
    }

    case SHOW_LEX_STRING: {
      LEX_STRING *ls = (LEX_STRING *)value;
      if (!(pos = ls->str))
        end = pos = "";
      else
        end = pos + ls->length;
      break;
    }

    case SHOW_KEY_CACHE_LONG:
      value = (char *)dflt_key_cache + (ulong)value;
      end = int10_to_str(*(long *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_KEY_CACHE_LONGLONG:
      value = (char *)dflt_key_cache + (ulong)value;
      end = longlong10_to_str(*(longlong *)value, buff, 10);
      value_charset = system_charset_info;
      break;

    case SHOW_UNDEF:
      break; /* Return empty string */

    case SHOW_SYS: /* Cannot happen */

    default:
      DBUG_ASSERT(0);
      break;
  }

  *length = (size_t)(end - pos);
  /* Some callers do not use the result. */
  if (charset != NULL) {
    DBUG_ASSERT(value_charset != NULL);
    *charset = value_charset;
  }
  return pos;
}

/**
  Collect status for all running threads.
*/
class Add_status : public Do_THD_Impl {
 public:
  Add_status(System_status_var *value) : m_stat_var(value) {}
  virtual void operator()(THD *thd) {
    if (!thd->status_var_aggregated)
      add_to_status(m_stat_var, &thd->status_var, false);
  }

 private:
  /* Status of all threads are summed into this. */
  System_status_var *m_stat_var;
};

void calc_sum_of_all_status(System_status_var *to) {
  DBUG_ENTER("calc_sum_of_all_status");
  mysql_mutex_assert_owner(&LOCK_status);
  /* Get global values as base. */
  *to = global_status_var;
  Add_status add_status(to);
  Global_THD_manager::get_instance()->do_for_all_thd_copy(&add_status);
  DBUG_VOID_RETURN;
}

/* This is only used internally, but we need it here as a forward reference */
extern ST_SCHEMA_TABLE schema_tables[];

/**
  Condition pushdown used for INFORMATION_SCHEMA / SHOW queries.
  This structure is to implement an optimization when
  accessing data dictionary data in the INFORMATION_SCHEMA
  or SHOW commands.
  When the query contain a TABLE_SCHEMA or TABLE_NAME clause,
  narrow the search for data based on the constraints given.
*/
struct LOOKUP_FIELD_VALUES {
  /**
    Value of a TABLE_SCHEMA clause.
    Note that this value length may exceed @c NAME_LEN.
    @sa wild_db_value
  */
  LEX_STRING db_value;
  /**
    Value of a TABLE_NAME clause.
    Note that this value length may exceed @c NAME_LEN.
    @sa wild_table_value
  */
  LEX_STRING table_value;
  /**
    True when @c db_value is a LIKE clause,
    false when @c db_value is an '=' clause.
  */
  bool wild_db_value;
  /**
    True when @c table_value is a LIKE clause,
    false when @c table_value is an '=' clause.
  */
  bool wild_table_value;
};

/*
  Store record to I_S table, convert HEAP table
  to MyISAM if necessary

  SYNOPSIS
    schema_table_store_record()
    thd                   thread handler
    table                 Information schema table to be updated

  RETURN
    0	                  success
    1	                  error
*/

bool schema_table_store_record(THD *thd, TABLE *table) {
  int error;
  if ((error = table->file->ha_write_row(table->record[0]))) {
    Temp_table_param *param = table->pos_in_table_list->schema_table_param;

    if (create_ondisk_from_heap(thd, table, param->start_recinfo,
                                &param->recinfo, error, false, NULL))
      return 1;
  }
  return 0;
}

/**
  Store record to I_S table, convert HEAP table to InnoDB table if necessary.

  @param[in]  thd            thread handler
  @param[in]  table          Information schema table to be updated
  @param[in]  make_ondisk    if true, convert heap table to on disk table.
                             default value is true.
  @return 0 on success
  @return error code on failure.
*/
int schema_table_store_record2(THD *thd, TABLE *table, bool make_ondisk) {
  int error;
  if ((error = table->file->ha_write_row(table->record[0]))) {
    if (!make_ondisk) return error;

    if (convert_heap_table_to_ondisk(thd, table, error)) return 1;
  }
  return 0;
}

/**
  Convert HEAP table to InnoDB table if necessary

  @param[in] thd     thread handler
  @param[in] table   Information schema table to be converted.
  @param[in] error   the error code returned previously.
  @return false on success, true on error.
*/
bool convert_heap_table_to_ondisk(THD *thd, TABLE *table, int error) {
  Temp_table_param *param = table->pos_in_table_list->schema_table_param;

  return (create_ondisk_from_heap(thd, table, param->start_recinfo,
                                  &param->recinfo, error, false, NULL));
}

/**
  Prepare a Table_ident and add a table_list into SELECT_LEX

  @param thd         Thread
  @param sel         Instance of SELECT_LEX.
  @param db_name     Database name.
  @param table_name  Table name.

  @returns true on failure.
           false on success.
*/
int make_table_list(THD *thd, SELECT_LEX *sel, const LEX_CSTRING &db_name,
                    const LEX_CSTRING &table_name) {
  Table_ident *table_ident;
  table_ident = new (*THR_MALLOC)
      Table_ident(thd->get_protocol(), db_name, table_name, 1);
  if (!sel->add_table_to_list(thd, table_ident, 0, 0, TL_READ, MDL_SHARED_READ))
    return 1;
  return 0;
}

/**
  @brief    Get lookup value from the part of 'WHERE' condition

  @details This function gets lookup value from
           the part of 'WHERE' condition if it's possible and
           fill appropriate lookup_field_vals struct field
           with this value.

  @param[in]      thd                   thread handler
  @param[in]      item_func             part of WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

static bool get_lookup_value(THD *thd, Item_func *item_func, TABLE_LIST *table,
                             LOOKUP_FIELD_VALUES *lookup_field_vals) {
  ST_SCHEMA_TABLE *schema_table = table->schema_table;
  ST_FIELD_INFO *field_info = schema_table->fields_info;
  const char *field_name1 =
      schema_table->idx_field1 >= 0
          ? field_info[schema_table->idx_field1].field_name
          : "";
  const char *field_name2 =
      schema_table->idx_field2 >= 0
          ? field_info[schema_table->idx_field2].field_name
          : "";

  if (item_func->functype() == Item_func::EQ_FUNC ||
      item_func->functype() == Item_func::EQUAL_FUNC) {
    int idx_field, idx_val;
    char tmp[MAX_FIELD_WIDTH];
    String *tmp_str, str_buff(tmp, sizeof(tmp), system_charset_info);
    Item_field *item_field;
    CHARSET_INFO *cs = system_charset_info;

    if (item_func->arguments()[0]->type() == Item::FIELD_ITEM &&
        item_func->arguments()[1]->const_item()) {
      idx_field = 0;
      idx_val = 1;
    } else if (item_func->arguments()[1]->type() == Item::FIELD_ITEM &&
               item_func->arguments()[0]->const_item()) {
      idx_field = 1;
      idx_val = 0;
    } else
      return 0;

    item_field = (Item_field *)item_func->arguments()[idx_field];
    if (table->table != item_field->field->table) return 0;
    tmp_str = item_func->arguments()[idx_val]->val_str(&str_buff);

    /* impossible value */
    if (!tmp_str) return 1;

    /* Lookup value is database name */
    if (!cs->coll->strnncollsp(cs, (uchar *)field_name1, strlen(field_name1),
                               (uchar *)item_field->field_name,
                               strlen(item_field->field_name))) {
      thd->make_lex_string(&lookup_field_vals->db_value, tmp_str->ptr(),
                           tmp_str->length(), false);
    }
    /* Lookup value is table name */
    else if (!cs->coll->strnncollsp(cs, (uchar *)field_name2,
                                    strlen(field_name2),
                                    (uchar *)item_field->field_name,
                                    strlen(item_field->field_name))) {
      thd->make_lex_string(&lookup_field_vals->table_value, tmp_str->ptr(),
                           tmp_str->length(), false);
    }
  }
  return 0;
}

/**
  @brief    Calculates lookup values from 'WHERE' condition

  @details This function calculates lookup value(database name, table name)
           from 'WHERE' condition if it's possible and
           fill lookup_field_vals struct fields with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      table                 I_S table
  @param[in, out] lookup_field_vals     Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

static bool calc_lookup_values_from_cond(
    THD *thd, Item *cond, TABLE_LIST *table,
    LOOKUP_FIELD_VALUES *lookup_field_vals) {
  if (!cond) return 0;

  if (cond->type() == Item::COND_ITEM) {
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        if (item->type() == Item::FUNC_ITEM) {
          if (get_lookup_value(thd, (Item_func *)item, table,
                               lookup_field_vals))
            return 1;
        } else {
          if (calc_lookup_values_from_cond(thd, item, table, lookup_field_vals))
            return 1;
        }
      }
    }
    return 0;
  } else if (cond->type() == Item::FUNC_ITEM &&
             get_lookup_value(thd, (Item_func *)cond, table, lookup_field_vals))
    return 1;
  return 0;
}

static bool uses_only_table_name_fields(Item *item, TABLE_LIST *table) {
  if (item->type() == Item::FUNC_ITEM) {
    Item_func *item_func = (Item_func *)item;
    for (uint i = 0; i < item_func->argument_count(); i++) {
      if (!uses_only_table_name_fields(item_func->arguments()[i], table))
        return 0;
    }
  } else if (item->type() == Item::FIELD_ITEM) {
    Item_field *item_field = (Item_field *)item;
    CHARSET_INFO *cs = system_charset_info;
    ST_SCHEMA_TABLE *schema_table = table->schema_table;
    ST_FIELD_INFO *field_info = schema_table->fields_info;
    const char *field_name1 =
        schema_table->idx_field1 >= 0
            ? field_info[schema_table->idx_field1].field_name
            : "";
    const char *field_name2 =
        schema_table->idx_field2 >= 0
            ? field_info[schema_table->idx_field2].field_name
            : "";
    if (table->table != item_field->field->table ||
        (cs->coll->strnncollsp(cs, (uchar *)field_name1, strlen(field_name1),
                               (uchar *)item_field->field_name,
                               strlen(item_field->field_name)) &&
         cs->coll->strnncollsp(cs, (uchar *)field_name2, strlen(field_name2),
                               (uchar *)item_field->field_name,
                               strlen(item_field->field_name))))
      return 0;
  } else if (item->type() == Item::REF_ITEM)
    return uses_only_table_name_fields(item->real_item(), table);

  if (item->type() == Item::SUBSELECT_ITEM && !item->const_item()) return 0;

  return 1;
}

static Item *make_cond_for_info_schema(Item *cond, TABLE_LIST *table) {
  if (!cond) return (Item *)0;
  if (cond->type() == Item::COND_ITEM) {
    if (((Item_cond *)cond)->functype() == Item_func::COND_AND_FUNC) {
      /* Create new top level AND item */
      Item_cond_and *new_cond = new Item_cond_and;
      if (!new_cond) return (Item *)0;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_for_info_schema(item, table);
        if (fix) new_cond->argument_list()->push_back(fix);
      }
      switch (new_cond->argument_list()->elements) {
        case 0:
          return (Item *)0;
        case 1:
          return new_cond->argument_list()->head();
        default:
          new_cond->quick_fix_field();
          return new_cond;
      }
    } else {  // Or list
      Item_cond_or *new_cond = new Item_cond_or;
      if (!new_cond) return (Item *)0;
      List_iterator<Item> li(*((Item_cond *)cond)->argument_list());
      Item *item;
      while ((item = li++)) {
        Item *fix = make_cond_for_info_schema(item, table);
        if (!fix) return (Item *)0;
        new_cond->argument_list()->push_back(fix);
      }
      new_cond->quick_fix_field();
      new_cond->top_level_item();
      return new_cond;
    }
  }

  if (!uses_only_table_name_fields(cond, table)) return (Item *)0;
  return cond;
}

/**
  @brief   Calculate lookup values(database name, table name)

  @details This function calculates lookup values(database name, table name)
           from 'WHERE' condition or wild values (for 'SHOW' commands only)
           from LEX struct and fill lookup_field_vals struct field
           with these values.

  @param[in]      thd                   thread handler
  @param[in]      cond                  WHERE condition
  @param[in]      tables                I_S table
  @param[in, out] lookup_field_values   Struct which holds lookup values

  @return
    0             success
    1             error, there can be no matching records for the condition
*/

static bool get_lookup_field_values(THD *thd, Item *cond, TABLE_LIST *tables,
                                    LOOKUP_FIELD_VALUES *lookup_field_values) {
  LEX *lex = thd->lex;
  const char *wild = lex->wild ? lex->wild->ptr() : NullS;
  bool rc = 0;

  memset(lookup_field_values, 0, sizeof(LOOKUP_FIELD_VALUES));
  switch (lex->sql_command) {
    case SQLCOM_SHOW_DATABASES:
      if (wild) {
        thd->make_lex_string(&lookup_field_values->db_value, wild, strlen(wild),
                             0);
        lookup_field_values->wild_db_value = 1;
      }
      break;
    case SQLCOM_SHOW_TABLES:
    case SQLCOM_SHOW_TABLE_STATUS:
    case SQLCOM_SHOW_TRIGGERS:
    case SQLCOM_SHOW_EVENTS:
      thd->make_lex_string(&lookup_field_values->db_value, lex->select_lex->db,
                           strlen(lex->select_lex->db), 0);
      if (wild) {
        thd->make_lex_string(&lookup_field_values->table_value, wild,
                             strlen(wild), 0);
        lookup_field_values->wild_table_value = 1;
      }
      break;
    default:
      /*
        The "default" is for queries over I_S.
        All previous cases handle SHOW commands.
      */
      rc = calc_lookup_values_from_cond(thd, cond, tables, lookup_field_values);
      break;
  }

  if (lower_case_table_names && !rc) {
    /*
      We can safely do in-place upgrades here since all of the above cases
      are allocating a new memory buffer for these strings.
    */
    if (lookup_field_values->db_value.str &&
        lookup_field_values->db_value.str[0])
      my_casedn_str(system_charset_info, lookup_field_values->db_value.str);
    if (lookup_field_values->table_value.str &&
        lookup_field_values->table_value.str[0])
      my_casedn_str(system_charset_info, lookup_field_values->table_value.str);
  }

  return rc;
}

enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table) {
  return (enum enum_schema_tables)(schema_table - &schema_tables[0]);
}

/*
  Create db names list. Information schema name always is first in list

  SYNOPSIS
    make_db_list()
    thd                   thread handler
    files                 list of db names
    wild                  wild string
    idx_field_vals        idx_field_vals->db_name contains db name or
                          wild string
    with_i_schema         returns 1 if we added 'IS' name to list
                          otherwise returns 0

  RETURN
    zero                  success
    non-zero              error
*/

static int make_db_list(THD *thd, List<LEX_STRING> *files,
                        LOOKUP_FIELD_VALUES *lookup_field_vals,
                        bool *with_i_schema, MEM_ROOT *tmp_mem_root) {
  LEX_STRING *i_s_name_copy = 0;
  i_s_name_copy =
      thd->make_lex_string(i_s_name_copy, INFORMATION_SCHEMA_NAME.str,
                           INFORMATION_SCHEMA_NAME.length, true);
  *with_i_schema = 0;
  if (lookup_field_vals->wild_db_value) {
    /*
      This part of code is only for SHOW DATABASES command.
      idx_field_vals->db_value can be 0 when we don't use
      LIKE clause (see also get_index_field_values() function)
    */
    if (!lookup_field_vals->db_value.str ||
        !wild_case_compare(system_charset_info, INFORMATION_SCHEMA_NAME.str,
                           lookup_field_vals->db_value.str)) {
      *with_i_schema = 1;
      if (files->push_back(i_s_name_copy)) return 1;
    }
    return (find_files(thd, files, NullS, mysql_data_home,
                       lookup_field_vals->db_value.str, 1,
                       tmp_mem_root) != FIND_FILES_OK);
  }

  /*
    If we have db lookup value we just add it to list and
    exit from the function.
    We don't do this for database names longer than the maximum
    name length.
  */
  if (lookup_field_vals->db_value.str) {
    if (lookup_field_vals->db_value.length > NAME_LEN) {
      /*
        Impossible value for a database name,
        found in a WHERE DATABASE_NAME = 'xxx' clause.
      */
      return 0;
    }

    if (is_infoschema_db(lookup_field_vals->db_value.str,
                         lookup_field_vals->db_value.length)) {
      *with_i_schema = 1;
      if (files->push_back(i_s_name_copy)) return 1;
      return 0;
    }
    if (files->push_back(&lookup_field_vals->db_value)) return 1;
    return 0;
  }

  /*
    Create list of existing databases. It is used in case
    of select from information schema table
  */
  if (files->push_back(i_s_name_copy)) return 1;
  *with_i_schema = 1;
  return (find_files(thd, files, NullS, mysql_data_home, NullS, 1,
                     tmp_mem_root) != FIND_FILES_OK);
}

struct st_add_schema_table {
  List<LEX_STRING> *files;
  const char *wild;
};

static bool add_schema_table(THD *thd, plugin_ref plugin, void *p_data) {
  LEX_STRING *file_name = 0;
  st_add_schema_table *data = (st_add_schema_table *)p_data;
  List<LEX_STRING> *file_list = data->files;
  const char *wild = data->wild;
  ST_SCHEMA_TABLE *schema_table = plugin_data<ST_SCHEMA_TABLE *>(plugin);
  DBUG_ENTER("add_schema_table");

  if (schema_table->hidden) DBUG_RETURN(0);
  if (wild) {
    if (lower_case_table_names) {
      if (wild_case_compare(files_charset_info, schema_table->table_name, wild))
        DBUG_RETURN(0);
    } else if (wild_compare(schema_table->table_name,
                            strlen(schema_table->table_name), wild,
                            strlen(wild), 0))
      DBUG_RETURN(0);
  }

  if ((file_name =
           thd->make_lex_string(file_name, schema_table->table_name,
                                strlen(schema_table->table_name), true)) &&
      !file_list->push_back(file_name))
    DBUG_RETURN(0);
  DBUG_RETURN(1);
}

static int schema_tables_add(THD *thd, List<LEX_STRING> *files,
                             const char *wild) {
  LEX_STRING *file_name = 0;
  ST_SCHEMA_TABLE *tmp_schema_table = schema_tables;
  st_add_schema_table add_data;
  DBUG_ENTER("schema_tables_add");

  for (; tmp_schema_table->table_name; tmp_schema_table++) {
    if (tmp_schema_table->hidden) continue;
    if (wild) {
      if (lower_case_table_names) {
        if (wild_case_compare(files_charset_info, tmp_schema_table->table_name,
                              wild))
          continue;
      } else if (wild_compare(tmp_schema_table->table_name,
                              strlen(tmp_schema_table->table_name), wild,
                              strlen(wild), 0))
        continue;
    }
    if ((file_name = thd->make_lex_string(
             file_name, tmp_schema_table->table_name,
             strlen(tmp_schema_table->table_name), true)) &&
        !files->push_back(file_name))
      continue;
    DBUG_RETURN(1);
  }

  add_data.files = files;
  add_data.wild = wild;
  if (plugin_foreach(thd, add_schema_table, MYSQL_INFORMATION_SCHEMA_PLUGIN,
                     &add_data))
    DBUG_RETURN(1);

  DBUG_RETURN(0);
}

/**
  @brief          Create table names list

  @details        The function creates the list of table names in
                  database.

  @note           This is an intermediate solution which will be replaced
                  by the implementation in WL#6599.

  @param[in]      thd                   thread handler
  @param[in]      table_names           List of table names in database
  @param[in]      lex                   pointer to LEX struct
  @param[in]      lookup_field_vals     pointer to LOOKUP_FIELD_VALUE struct
  @param[in]      with_i_schema         true means that we add I_S tables to
  list
  @param[in]      db_name               database name

  @return         Operation status
    @retval       0           ok
    @retval       1           fatal error
    @retval       2           Not fatal error; Safe to ignore this file list
*/

static int make_table_name_list(THD *thd, List<LEX_STRING> *table_names,
                                LEX *lex,
                                LOOKUP_FIELD_VALUES *lookup_field_vals,
                                bool with_i_schema, LEX_STRING *db_name) {
  char path[FN_REFLEN + 1];
  build_table_filename(path, sizeof(path) - 1, db_name->str, "", "", 0);
  if (!lookup_field_vals->wild_table_value &&
      lookup_field_vals->table_value.str) {
    if (lookup_field_vals->table_value.length > NAME_LEN) {
      /*
        Impossible value for a table name,
        found in a WHERE TABLE_NAME = 'xxx' clause.
      */
      return 0;
    }

    if (with_i_schema) {
      LEX_STRING *name = NULL;
      ST_SCHEMA_TABLE *schema_table =
          find_schema_table(thd, lookup_field_vals->table_value.str);
      if (schema_table && !schema_table->hidden) {
        if (!(name = thd->make_lex_string(name, schema_table->table_name,
                                          strlen(schema_table->table_name),
                                          true)) ||
            table_names->push_back(name))
          return 1;
      }
    } else {
      if (table_names->push_back(&lookup_field_vals->table_value)) return 1;
      /*
        Check that table is relevant in current transaction.
        (used for ndb engine, see ndbcluster_find_files(), ha_ndbcluster.cc)
      */
      (void)ha_find_files(thd, db_name->str, path,
                          lookup_field_vals->table_value.str, 0, table_names);
    }
    return 0;
  }

  /*
    This call will add all matching the wildcards (if specified) IS tables
    to the list
  */
  if (with_i_schema)
    return (schema_tables_add(thd, table_names,
                              lookup_field_vals->table_value.str));

  {
    const dd::Schema *sch_obj = NULL;
    if (thd->dd_client()->acquire(db_name->str, &sch_obj)) return 1;

    if (!sch_obj) {
      /*
        Report missing database only if it is a 'SHOW' command.
        Another thread may have dropped the database after we
        got its name from the DD.
      */
      if (sql_command_flags[lex->sql_command] & CF_STATUS_COMMAND) {
        my_error(ER_BAD_DB_ERROR, MYF(0), db_name->str);
        return 1;
      }
      return 2;
    }

    std::vector<dd::String_type> component_names;
    if (thd->dd_client()->fetch_schema_component_names<dd::Abstract_table>(
            sch_obj, &component_names))
      return 1;

    for (std::vector<dd::String_type>::const_iterator name =
             component_names.begin();
         name != component_names.end(); ++name) {
      if (lookup_field_vals->table_value.str) {
        if (lower_case_table_names) {
          if (my_wildcmp(files_charset_info, name->c_str(),
                         name->c_str() + name->length(),
                         lookup_field_vals->table_value.str,
                         lookup_field_vals->table_value.str +
                             lookup_field_vals->table_value.length,
                         wild_prefix, wild_one, wild_many))
            continue;
        } else if (wild_compare(name->c_str(), name->length(),
                                lookup_field_vals->table_value.str,
                                lookup_field_vals->table_value.length, 0))
          continue;
      }

      /* Don't show tables where we don't have any privileges */
      if (!(thd->col_access & TABLE_ACLS)) {
        TABLE_LIST table_list;
        table_list.db = db_name->str;
        table_list.db_length = db_name->length;
        table_list.table_name = const_cast<char *>(name->c_str());
        table_list.table_name_length = name->length();
        table_list.grant.privilege = thd->col_access;
        if (check_grant(thd, TABLE_ACLS, &table_list, true, 1, true)) continue;
      }

      LEX_STRING *table_name = NULL;
      table_name =
          thd->make_lex_string(table_name, name->c_str(), name->length(), true);
      table_names->push_back(table_name);
    }
  }

  return 0;
}

/**
  Fill I_S table with data obtained by performing full-blown table open.

  @param  thd                       Thread handler.
  @param  mem_root                  Designated mem_root for query.
  @param  is_show_fields_or_keys    Indicates whether it is a legacy SHOW
                                    COLUMNS or SHOW KEYS statement.
  @param  table                     TABLE object for I_S table to be filled.
  @param  schema_table              I_S table description structure.
  @param  orig_db_name              Database name.
  @param  orig_table_name           Table name.
  @param  open_tables_state_backup  Open_tables_state object which is used
                                    to save/restore original status of
                                    variables related to open tables state.
  @param  can_deadlock              Indicates that deadlocks are possible
                                    due to metadata locks, so to avoid
                                    them we should not wait in case if
                                    conflicting lock is present.
  @return Operation status
    @retval  false on success
    @retval  true  fatal error
*/
static bool fill_schema_table_by_open(
    THD *thd, MEM_ROOT *mem_root, bool is_show_fields_or_keys, TABLE *table,
    ST_SCHEMA_TABLE *schema_table, LEX_STRING *orig_db_name,
    LEX_STRING *orig_table_name, Open_tables_backup *open_tables_state_backup,
    bool can_deadlock) {
  Query_arena i_s_arena(mem_root, Query_arena::STMT_CONVENTIONAL_EXECUTION),
      backup_arena, *old_arena;
  LEX *old_lex = thd->lex, temp_lex, *lex;
  LEX_CSTRING db_name_lex_cstr, table_name_lex_cstr;
  TABLE_LIST *table_list;
  bool result = true;

  DBUG_ENTER("fill_schema_table_by_open");
  /*
    When a view is opened its structures are allocated on a permanent
    statement arena and linked into the LEX tree for the current statement
    (this happens even in cases when view is handled through TEMPTABLE
    algorithm).

    To prevent this process from unnecessary hogging of memory in the permanent
    arena of our I_S query and to avoid damaging its LEX we use temporary
    arena and LEX for table/view opening.

    Use temporary arena instead of statement permanent arena. Also make
    it active arena and save original one for successive restoring.
  */
  old_arena = thd->stmt_arena;
  thd->stmt_arena = &i_s_arena;
  thd->set_n_backup_active_arena(&i_s_arena, &backup_arena);

  /* Prepare temporary LEX. */
  thd->lex = lex = &temp_lex;
  lex_start(thd);

  /* Disable constant subquery evaluation as we won't be locking tables. */
  lex->context_analysis_only = CONTEXT_ANALYSIS_ONLY_VIEW;

  /*
    Some of process_table() functions rely on wildcard being passed from
    old LEX (or at least being initialized).
  */
  lex->wild = old_lex->wild;

  /*
    Since make_table_list() might change database and table name passed
    to it we create copies of orig_db_name and orig_table_name here.
    These copies are used for make_table_list() while unaltered values
    are passed to process_table() functions.
  */
  if (!thd->make_lex_string(&db_name_lex_cstr, orig_db_name->str,
                            orig_db_name->length, false) ||
      !thd->make_lex_string(&table_name_lex_cstr, orig_table_name->str,
                            orig_table_name->length, false))
    goto end;

  /*
    Create table list element for table to be open. Link it with the
    temporary LEX. The latter is required to correctly open views and
    produce table describing their structure.
  */
  if (make_table_list(thd, lex->select_lex, db_name_lex_cstr,
                      table_name_lex_cstr))
    goto end;

  table_list = lex->select_lex->table_list.first;

  if (is_show_fields_or_keys) {
    /*
      Restore thd->temporary_tables to be able to process
      temporary tables (only for 'show index' & 'show columns').
      This should be changed when processing of temporary tables for
      I_S tables will be done.
    */
    thd->temporary_tables = open_tables_state_backup->temporary_tables;
  } else {
    /*
      Apply optimization flags for table opening which are relevant for
      this I_S table. We can't do this for SHOW COLUMNS/KEYS because of
      backward compatibility.
    */
    table_list->i_s_requested_object = schema_table->i_s_requested_object;
  }

  result = open_temporary_tables(thd, table_list);

  if (!result)
    result = open_tables_for_query(
        thd, table_list,
        MYSQL_OPEN_IGNORE_FLUSH | MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL |
            (can_deadlock ? MYSQL_OPEN_FAIL_ON_MDL_CONFLICT : 0));
  if (!result && table_list->is_view_or_derived()) {
    result = table_list->resolve_derived(thd, false);
    if (!result) result = table_list->setup_materialized_derived(thd);
  }
  /*
    Restore old value of sql_command back as it is being looked at in
    process_table() function.
  */
  lex->sql_command = old_lex->sql_command;

  DEBUG_SYNC(thd, "after_open_table_ignore_flush");

  /*
    XXX:  show_table_list has a flag i_is_requested,
    and when it's set, open_tables_for_query()
    can return an error without setting an error message
    in THD, which is a hack. This is why we have to
    check for res, then for thd->is_error() and only then
    for thd->main_da.sql_errno().

    Again we don't do this for SHOW COLUMNS/KEYS because
    of backward compatibility.
  */
  if (!is_show_fields_or_keys && result && thd->is_error() &&
      thd->get_stmt_da()->mysql_errno() == ER_NO_SUCH_TABLE) {
    /*
      Hide error for a non-existing table.
      For example, this error can occur when we use a where condition
      with a db name and table, but the table does not exist.
    */
    result = false;
    thd->clear_error();
  } else {
    result = schema_table->process_table(thd, table_list, table, result,
                                         orig_db_name, orig_table_name);
  }

end:
  lex->unit->cleanup(true);

  /* Restore original LEX value, statement's arena and THD arena values. */
  lex_end(thd->lex);

  // Free items, before restoring backup_arena below.
  DBUG_ASSERT(i_s_arena.free_list == NULL);
  thd->free_items();

  /*
    For safety reset list of open temporary tables before closing
    all tables open within this Open_tables_state.
  */
  thd->temporary_tables = NULL;
  close_thread_tables(thd);
  /*
    Release metadata lock we might have acquired.

    Without this step metadata locks acquired for each table processed
    will be accumulated. In situation when a lot of tables are processed
    by I_S query this will result in transaction with too many metadata
    locks. As result performance of acquisition of new lock will suffer.

    Of course, the fact that we don't hold metadata lock on tables which
    were processed till the end of I_S query makes execution less isolated
    from concurrent DDL. Consequently one might get 'dirty' results from
    such a query. But we have never promised serializability of I_S queries
    anyway.

    We don't have any tables open since we took backup, so rolling back to
    savepoint is safe.
  */
  thd->mdl_context.rollback_to_savepoint(
      open_tables_state_backup->mdl_system_tables_svp);

  thd->lex = old_lex;

  thd->stmt_arena = old_arena;
  thd->restore_active_arena(&i_s_arena, &backup_arena);

  DBUG_RETURN(result);
}

/**
  @brief          Get open table method

  @details        The function calculates the method which will be used
                  for table opening:
                  SKIP_OPEN_TABLE - do not open table
                  OPEN_FRM_ONLY   - open FRM file only
                  OPEN_FULL_TABLE - open FRM, data, index files
  @param[in]      tables               I_S table table_list
  @param[in]      schema_table         I_S table struct

  @return         return a set of flags
    @retval       SKIP_OPEN_TABLE | OPEN_FRM_ONLY | OPEN_FULL_TABLE
*/

static uint get_table_open_method(TABLE_LIST *tables,
                                  ST_SCHEMA_TABLE *schema_table) {
  /*
    determine which method will be used for table opening
  */
  if (schema_table->i_s_requested_object & OPTIMIZE_I_S_TABLE) {
    Field **ptr, *field;
    int table_open_method = 0, field_indx = 0;
    uint star_table_open_method = OPEN_FULL_TABLE;
    bool used_star = true;  // true if '*' is used in select
    for (ptr = tables->table->field; (field = *ptr); ptr++) {
      star_table_open_method =
          min(star_table_open_method,
              schema_table->fields_info[field_indx].open_method);
      if (bitmap_is_set(tables->table->read_set, field->field_index)) {
        used_star = false;
        table_open_method |= schema_table->fields_info[field_indx].open_method;
      }
      field_indx++;
    }
    if (used_star) return star_table_open_method;
    return table_open_method;
  }
  /* I_S tables which use get_all_tables but can not be optimized */
  return (uint)OPEN_FULL_TABLE;
}

/**
   Try acquire high priority share metadata lock on a table (with
   optional wait for conflicting locks to go away).

   @param thd            Thread context.
   @param table          Table list element for the table
   @param can_deadlock   Indicates that deadlocks are possible due to
                         metadata locks, so to avoid them we should not
                         wait in case if conflicting lock is present.

   @note This is an auxiliary function to be used in cases when we want to
         access table's description by looking up info in TABLE_SHARE without
         going through full-blown table open.
   @note This function assumes that there are no other metadata lock requests
         in the current metadata locking context.

   @retval false  No error, if lock was obtained TABLE_LIST::mdl_request::ticket
                  is set to non-NULL value.
   @retval true   Some error occured (probably thread was killed).
*/

bool try_acquire_high_prio_shared_mdl_lock(THD *thd, TABLE_LIST *table,
                                           bool can_deadlock) {
  bool error;
  MDL_REQUEST_INIT(&table->mdl_request, MDL_key::TABLE, table->db,
                   table->table_name, MDL_SHARED_HIGH_PRIO, MDL_TRANSACTION);

  if (can_deadlock) {
    /*
      When .FRM is being open in order to get data for an I_S table,
      we might have some tables not only open but also locked.
      E.g. this happens when a SHOW or I_S statement is run
      under LOCK TABLES or inside a stored function.
      By waiting for the conflicting metadata lock to go away we
      might create a deadlock which won't entirely belong to the
      MDL subsystem and thus won't be detectable by this subsystem's
      deadlock detector. To avoid such situation, when there are
      other locked tables, we prefer not to wait on a conflicting
      lock.
    */
    error = thd->mdl_context.try_acquire_lock(&table->mdl_request);
  } else
    error = thd->mdl_context.acquire_lock(&table->mdl_request,
                                          thd->variables.lock_wait_timeout);

  return error;
}

/**
  @brief          Fill I_S tables whose data are retrieved
                  from frm files and storage engine

  @details        The information schema tables are internally represented as
                  temporary tables that are filled at query execution time.
                  Those I_S tables whose data are retrieved
                  from frm files and storage engine are filled by the function
                  get_all_tables().

  @param[in]      thd                      thread handler
  @param[in]      tables                   I_S table
  @param[in]      cond                     'WHERE' condition

  @return         Operation status
    @retval       0                        success
    @retval       1                        error
*/

static int get_all_tables(THD *thd, TABLE_LIST *tables, Item *cond) {
  LEX *lex = thd->lex;
  TABLE *table = tables->table;
  SELECT_LEX *lsel = tables->schema_select_lex;
  ST_SCHEMA_TABLE *schema_table = tables->schema_table;
  LOOKUP_FIELD_VALUES lookup_field_vals;
  LEX_STRING *db_name, *table_name;
  bool with_i_schema;
  List<LEX_STRING> db_names;
  List_iterator_fast<LEX_STRING> it(db_names);
  Item *partial_cond = 0;
  int error = 1;
  Open_tables_backup open_tables_state_backup;
  Security_context *sctx = thd->security_context();
  uint table_open_method;
  bool can_deadlock;

  DBUG_ENTER("get_all_tables");

  MEM_ROOT tmp_mem_root;
  init_sql_alloc(key_memory_get_all_tables, &tmp_mem_root,
                 TABLE_ALLOC_BLOCK_SIZE, 0);

  /*
    In cases when SELECT from I_S table being filled by this call is
    part of statement which also uses other tables or is being executed
    under LOCK TABLES or is part of transaction which also uses other
    tables waiting for metadata locks which happens below might result
    in deadlocks.
    To avoid them we don't wait if conflicting metadata lock is
    encountered and skip table with emitting an appropriate warning.
  */
  can_deadlock = thd->mdl_context.has_locks();

  /*
    We should not introduce deadlocks even if we already have some
    tables open and locked, since we won't lock tables which we will
    open and will ignore pending exclusive metadata locks for these
    tables by using high-priority requests for shared metadata locks.
  */
  thd->reset_n_backup_open_tables_state(&open_tables_state_backup, 0);

  tables->table_open_method = table_open_method =
      get_table_open_method(tables, schema_table);
  DBUG_PRINT("open_method", ("%d", tables->table_open_method));
  /*
    this branch processes SHOW FIELDS, SHOW INDEXES commands.
    see sql_parse.cc, prepare_schema_table() function where
    this values are initialized
  */
  if (lsel && lsel->table_list.first) {
    LEX_STRING db_name, table_name;

    db_name.str = const_cast<char *>(lsel->table_list.first->db);
    db_name.length = lsel->table_list.first->db_length;

    table_name.str = const_cast<char *>(lsel->table_list.first->table_name);
    table_name.length = lsel->table_list.first->table_name_length;

    error = fill_schema_table_by_open(thd, &tmp_mem_root, true, table,
                                      schema_table, &db_name, &table_name,
                                      &open_tables_state_backup, can_deadlock);
    goto err;
  }

  if (get_lookup_field_values(thd, cond, tables, &lookup_field_vals)) {
    error = 0;
    goto err;
  }

  DBUG_PRINT("INDEX VALUES", ("db_name='%s', table_name='%s'",
                              STR_OR_NIL(lookup_field_vals.db_value.str),
                              STR_OR_NIL(lookup_field_vals.table_value.str)));

  if (!lookup_field_vals.wild_db_value && !lookup_field_vals.wild_table_value) {
    /*
      if lookup value is empty string then
      it's impossible table name or db name
    */
    if ((lookup_field_vals.db_value.str &&
         !lookup_field_vals.db_value.str[0]) ||
        (lookup_field_vals.table_value.str &&
         !lookup_field_vals.table_value.str[0])) {
      error = 0;
      goto err;
    }
  }

  if (lookup_field_vals.db_value.length && !lookup_field_vals.wild_db_value)
    tables->has_db_lookup_value = true;
  if (lookup_field_vals.table_value.length &&
      !lookup_field_vals.wild_table_value)
    tables->has_table_lookup_value = true;

  if (tables->has_db_lookup_value && tables->has_table_lookup_value)
    partial_cond = 0;
  else
    partial_cond = make_cond_for_info_schema(cond, tables);

  if (lex->is_explain()) {
    /* EXPLAIN SELECT */
    error = 0;
    goto err;
  }

  if (make_db_list(thd, &db_names, &lookup_field_vals, &with_i_schema,
                   &tmp_mem_root))
    goto err;
  it.rewind(); /* To get access to new elements in basis list */
  while ((db_name = it++)) {
    DBUG_ASSERT(db_name->length <= NAME_LEN);

    bool have_db_privileges = false;
    if (sctx->get_active_roles()->size() > 0) {
      LEX_CSTRING const_db_name = {db_name->str, db_name->length};
      have_db_privileges = sctx->db_acl(const_db_name) > 0 ? true : false;
    }
    if (!(check_access(thd, SELECT_ACL, db_name->str, &thd->col_access, NULL, 0,
                       1) ||
          (!thd->col_access && check_grant_db(thd, db_name->str))) ||
        sctx->check_access(DB_ACLS | SHOW_DB_ACL, true) || have_db_privileges ||
        acl_get(thd, sctx->host().str, sctx->ip().str, sctx->priv_user().str,
                db_name->str, 0))

    {
      // We must make sure the schema is released and unlocked in the right
      // order. Fail if we are unable to get a meta data lock on the schema
      // name.
      dd::Schema_MDL_locker mdl_handler(thd);
      if (mdl_handler.ensure_locked(db_name->str)) goto err;

      dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());
      List<LEX_STRING> table_names;
      int res = make_table_name_list(thd, &table_names, lex, &lookup_field_vals,
                                     with_i_schema, db_name);
      if (res == 2) /* Not fatal error, continue */
        continue;
      if (res) goto err;

      List_iterator_fast<LEX_STRING> it_files(table_names);
      while ((table_name = it_files++)) {
        DBUG_ASSERT(table_name->length <= NAME_LEN);
        restore_record(table, s->default_values);
        table->field[schema_table->idx_field1]->store(
            db_name->str, db_name->length, system_charset_info);
        table->field[schema_table->idx_field2]->store(
            table_name->str, table_name->length, system_charset_info);

        if (!partial_cond || partial_cond->val_int()) {
          /*
            OPEN_FRM_ONLY is only set for columns in tables which
            does not have OPTIMIZE_I_S_TABLE set.
            get_table_open_method() always returns OPEN_FULL_TABLE
            if OPTIMIZE_I_S_TABLE is not set, so OPEN_FRM_ONLY will
            never be the table_open_method.
          */
          DBUG_ASSERT(table_open_method != OPEN_FRM_ONLY);

          DEBUG_SYNC(thd, "before_open_in_get_all_tables");

          if (fill_schema_table_by_open(
                  thd, &tmp_mem_root, false, table, schema_table, db_name,
                  table_name, &open_tables_state_backup, can_deadlock))
            goto err;
        }
      }
      /*
        If we have information schema its always the first table and only
        the first table. Reset for other tables.
      */
      with_i_schema = 0;
    }
  }
  error = 0;
err:
  /*
    HACK
    If view security context were used we need to drop the reference those now
    or we will attempt to logout uninitialized security contexts during
    query clean up.
    TABLE_LIST::view_ctx allocation happens in parse_view_definition()
    during table open. The allocation happens on thd->stmt_area

    TODO Why is the view security context allocated if it's not used?
  */
  thd->m_view_ctx_list.empty();
  free_root(&tmp_mem_root, MYF(0));
  thd->restore_backup_open_tables_state(&open_tables_state_backup);

  DBUG_RETURN(error);
}

static int get_schema_tmp_table_columns_record(THD *thd, TABLE_LIST *tables,
                                               TABLE *table, bool res,
                                               LEX_STRING *db_name,
                                               LEX_STRING *table_name) {
  DBUG_ENTER("get_schema_tmp_table_columns_record");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_FIELDS);

  if (res) DBUG_RETURN(res);

  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : nullptr;
  CHARSET_INFO *cs = system_charset_info;
  TABLE *show_table = tables->table;
  Field **ptr = show_table->field;
  Field *field;
  show_table->use_all_columns();  // Required for default
  restore_record(show_table, s->default_values);

  for (; (field = *ptr); ptr++) {
    uchar *pos;
    char tmp[MAX_FIELD_WIDTH];
    String type(tmp, sizeof(tmp), system_charset_info);

    if (wild && wild[0] &&
        wild_case_compare(system_charset_info, field->field_name, wild))
      continue;

    // Get default row, with all NULL fields set to NULL
    restore_record(table, s->default_values);

    // NAME
    table->field[TMP_TABLE_COLUMNS_COLUMN_NAME]->store(
        field->field_name, strlen(field->field_name), cs);

    // COLUMN_TYPE
    field->sql_type(type);
    table->field[TMP_TABLE_COLUMNS_COLUMN_TYPE]->store(type.ptr(),
                                                       type.length(), cs);

    // COLLATION_NAME
    if (field->has_charset()) {
      table->field[TMP_TABLE_COLUMNS_COLLATION_NAME]->store(
          field->charset()->name, strlen(field->charset()->name), cs);
      table->field[TMP_TABLE_COLUMNS_COLLATION_NAME]->set_notnull();
    }

    // IS_NULLABLE
    pos = (uchar *)((field->flags & NOT_NULL_FLAG) ? "NO" : "YES");
    table->field[TMP_TABLE_COLUMNS_IS_NULLABLE]->store(
        (const char *)pos, strlen((const char *)pos), cs);

    // COLUMN_KEY
    pos =
        (uchar *)((field->flags & PRI_KEY_FLAG)
                      ? "PRI"
                      : (field->flags & UNIQUE_KEY_FLAG)
                            ? "UNI"
                            : (field->flags & MULTIPLE_KEY_FLAG) ? "MUL" : "");
    table->field[TMP_TABLE_COLUMNS_COLUMN_KEY]->store(
        (const char *)pos, strlen((const char *)pos), cs);

    // COLUMN_DEFAULT
    if (print_default_clause(field, &type, false)) {
      table->field[TMP_TABLE_COLUMNS_COLUMN_DEFAULT]->store(type.ptr(),
                                                            type.length(), cs);
      table->field[TMP_TABLE_COLUMNS_COLUMN_DEFAULT]->set_notnull();
    }

    // EXTRA
    /*
      For non-temporary tables, EXTRA column value in I_S.columns table
      is stored as below,

      IF (col.is_auto_increment=true,
      CONCAT(IFNULL(CONCAT("on update ", col.update_option, " "),''),
      "auto_increment"),
      CONCAT("on update ", col.update_option)) AS EXTRA,

      Following the same logic for columns of temporary tables also.
      */
    if (field->auto_flags & Field::NEXT_NUMBER) {
      if (print_on_update_clause(field, &type, true))
        table->field[TMP_TABLE_COLUMNS_EXTRA]->store(type.ptr(), type.length(),
                                                     cs);
      table->field[TMP_TABLE_COLUMNS_EXTRA]->store(
          STRING_WITH_LEN("auto_increment"), cs);
    } else {
      if (print_on_update_clause(field, &type, true))
        table->field[TMP_TABLE_COLUMNS_EXTRA]->store(type.ptr(), type.length(),
                                                     cs);
      else
        table->field[TMP_TABLE_COLUMNS_EXTRA]->store(STRING_WITH_LEN("NULL"),
                                                     cs);
    }

    // PRIVILEGES
    uint col_access;
    check_access(thd, SELECT_ACL, db_name->str, &tables->grant.privilege, 0, 0,
                 tables->schema_table != nullptr);
    col_access = get_column_grant(thd, &tables->grant, db_name->str,
                                  table_name->str, field->field_name) &
                 COL_ACLS;
    if (!tables->schema_table && !col_access) continue;
    char *end = tmp;
    for (uint bitnr = 0; col_access; col_access >>= 1, bitnr++) {
      if (col_access & 1) {
        *end++ = ',';
        end = my_stpcpy(end, grant_types.type_names[bitnr]);
      }
    }
    table->field[TMP_TABLE_COLUMNS_PRIVILEGES]->store(
        tmp + 1, end == tmp ? 0 : (uint)(end - tmp - 1), cs);

    // COLUMN_COMMENT
    table->field[TMP_TABLE_COLUMNS_COLUMN_COMMENT]->store(
        field->comment.str, field->comment.length, cs);

    // COLUMN_GENERATION_EXPRESSION
    if (field->gcol_info) {
      if (field->stored_in_db)
        table->field[TMP_TABLE_COLUMNS_EXTRA]->store(
            STRING_WITH_LEN("STORED GENERATED"), cs);
      else
        table->field[TMP_TABLE_COLUMNS_EXTRA]->store(
            STRING_WITH_LEN("VIRTUAL GENERATED"), cs);

      char buffer[128];
      String s(buffer, sizeof(buffer), system_charset_info);
      field->gcol_info->print_expr(thd, &s);
      table->field[TMP_TABLE_COLUMNS_GENERATION_EXPRESSION]->store(
          s.ptr(), s.length(), cs);
    } else
      table->field[TMP_TABLE_COLUMNS_GENERATION_EXPRESSION]->set_null();

    if (schema_table_store_record(thd, table)) DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

static bool iter_schema_engines(THD *thd, plugin_ref plugin, void *ptable) {
  TABLE *table = (TABLE *)ptable;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  CHARSET_INFO *scs = system_charset_info;
  handlerton *default_type = ha_default_handlerton(thd);
  DBUG_ENTER("iter_schema_engines");

  /* Disabled plugins */
  if (plugin_state(plugin) != PLUGIN_IS_READY) {
    struct st_mysql_plugin *plug = plugin_decl(plugin);

    if (!(wild && wild[0] && wild_case_compare(scs, plug->name, wild))) {
      restore_record(table, s->default_values);
      table->field[0]->store(plug->name, strlen(plug->name), scs);
      table->field[1]->store(C_STRING_WITH_LEN("NO"), scs);
      table->field[2]->store(plug->descr, strlen(plug->descr), scs);
      if (schema_table_store_record(thd, table)) DBUG_RETURN(1);
    }
    DBUG_RETURN(0);
  }

  if (!(hton->flags & HTON_HIDDEN)) {
    LEX_STRING *name = plugin_name(plugin);
    if (!(wild && wild[0] && wild_case_compare(scs, name->str, wild))) {
      LEX_STRING yesno[2] = {{C_STRING_WITH_LEN("NO")},
                             {C_STRING_WITH_LEN("YES")}};
      LEX_STRING *tmp;
      const char *option_name = show_comp_option_name[(int)hton->state];
      restore_record(table, s->default_values);

      table->field[0]->store(name->str, name->length, scs);
      if (hton->state == SHOW_OPTION_YES && default_type == hton)
        option_name = "DEFAULT";
      table->field[1]->store(option_name, strlen(option_name), scs);
      table->field[2]->store(plugin_decl(plugin)->descr,
                             strlen(plugin_decl(plugin)->descr), scs);
      tmp = &yesno[MY_TEST(hton->commit)];
      table->field[3]->store(tmp->str, tmp->length, scs);
      table->field[3]->set_notnull();
      tmp = &yesno[MY_TEST(hton->prepare)];
      table->field[4]->store(tmp->str, tmp->length, scs);
      table->field[4]->set_notnull();
      tmp = &yesno[MY_TEST(hton->savepoint_set)];
      table->field[5]->store(tmp->str, tmp->length, scs);
      table->field[5]->set_notnull();

      if (schema_table_store_record(thd, table)) DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(0);
}

static int fill_schema_engines(THD *thd, TABLE_LIST *tables, Item *) {
  DBUG_ENTER("fill_schema_engines");
  if (plugin_foreach_with_mask(thd, iter_schema_engines,
                               MYSQL_STORAGE_ENGINE_PLUGIN, ~PLUGIN_IS_FREED,
                               tables->table))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}

static int get_schema_tmp_table_keys_record(THD *thd, TABLE_LIST *tables,
                                            TABLE *table, bool res,
                                            LEX_STRING *,
                                            LEX_STRING *table_name) {
  DBUG_ENTER("get_schema_tmp_table_keys_record");

  DBUG_ASSERT(thd->lex->sql_command == SQLCOM_SHOW_KEYS);

  if (res) DBUG_RETURN(res);

  CHARSET_INFO *cs = system_charset_info;
  TABLE *show_table = tables->table;
  KEY *key_info = show_table->s->key_info;
  if (show_table->file)
    show_table->file->info(HA_STATUS_VARIABLE | HA_STATUS_NO_LOCK |
                           HA_STATUS_TIME);

  for (uint i = 0; i < show_table->s->keys; i++, key_info++) {
    KEY_PART_INFO *key_part = key_info->key_part;
    const char *str;
    for (uint j = 0; j < key_info->user_defined_key_parts; j++, key_part++) {
      restore_record(table, s->default_values);

      // TABLE_NAME
      table->field[TMP_TABLE_KEYS_TABLE_NAME]->store(table_name->str,
                                                     table_name->length, cs);

      // NON_UNIQUE
      table->field[TMP_TABLE_KEYS_IS_NON_UNIQUE]->store(
          (longlong)((key_info->flags & HA_NOSAME) ? 0 : 1), true);

      // INDEX_NAME
      table->field[TMP_TABLE_KEYS_INDEX_NAME]->store(
          key_info->name, strlen(key_info->name), cs);

      // SEQ_IN_INDEX
      table->field[TMP_TABLE_KEYS_SEQ_IN_INDEX]->store((longlong)(j + 1), true);

      // COLUMN_NAME
      str = (key_part->field ? key_part->field->field_name : "?unknown field?");
      table->field[TMP_TABLE_KEYS_COLUMN_NAME]->store(str, strlen(str), cs);

      if (show_table->file) {
        // COLLATION
        if (show_table->file->index_flags(i, j, 0) & HA_READ_ORDER) {
          table->field[TMP_TABLE_KEYS_COLLATION]->store(
              ((key_part->key_part_flag & HA_REVERSE_SORT) ? "D" : "A"), 1, cs);
          table->field[TMP_TABLE_KEYS_COLLATION]->set_notnull();
        }

        // CARDINALITY
        KEY *key = show_table->key_info + i;
        if (key->has_records_per_key(j)) {
          double records =
              (show_table->file->stats.records / key->records_per_key(j));
          table->field[TMP_TABLE_KEYS_CARDINALITY]->store(
              static_cast<longlong>(round(records)), true);
          table->field[TMP_TABLE_KEYS_CARDINALITY]->set_notnull();
        }
      }

      // INDEX_TYPE
      if (key_info->flags & HA_SPATIAL)
        str = "SPATIAL";
      else {
        ha_key_alg key_alg = key_info->algorithm;
        /* If index algorithm is implicit get SE default. */
        switch (key_alg) {
          case HA_KEY_ALG_SE_SPECIFIC:
            str = "";
            break;
          case HA_KEY_ALG_BTREE:
            str = "BTREE";
            break;
          case HA_KEY_ALG_RTREE:
            str = "RTREE";
            break;
          case HA_KEY_ALG_HASH:
            str = "HASH";
            break;
          case HA_KEY_ALG_FULLTEXT:
            str = "FULLTEXT";
            break;
          default:
            DBUG_ASSERT(0);
            str = "";
        }
      }
      table->field[TMP_TABLE_KEYS_INDEX_TYPE]->store(str, strlen(str), cs);

      // SUB_PART
      if (!(key_info->flags & HA_FULLTEXT) &&
          (key_part->field &&
           key_part->length !=
               show_table->s->field[key_part->fieldnr - 1]->key_length())) {
        table->field[TMP_TABLE_KEYS_SUB_PART]->store(
            key_part->length / key_part->field->charset()->mbmaxlen, true);
        table->field[TMP_TABLE_KEYS_SUB_PART]->set_notnull();
      }

      // NULLABLE
      uint flags = key_part->field ? key_part->field->flags : 0;
      const char *pos = (char *)((flags & NOT_NULL_FLAG) ? "" : "YES");
      table->field[TMP_TABLE_KEYS_IS_NULLABLE]->store(pos, strlen(pos), cs);

      // COMMENT
      if (!show_table->s->keys_in_use.is_set(i) && key_info->is_visible)
        table->field[TMP_TABLE_KEYS_COMMENT]->store(STRING_WITH_LEN("disabled"),
                                                    cs);
      else
        table->field[TMP_TABLE_KEYS_COMMENT]->store("", 0, cs);
      table->field[TMP_TABLE_KEYS_COMMENT]->set_notnull();

      // INDEX_COMMENT
      DBUG_ASSERT(MY_TEST(key_info->flags & HA_USES_COMMENT) ==
                  (key_info->comment.length > 0));
      if (key_info->flags & HA_USES_COMMENT)
        table->field[TMP_TABLE_KEYS_INDEX_COMMENT]->store(
            key_info->comment.str, key_info->comment.length, cs);

      // is_visible column
      const char *is_visible = key_info->is_visible ? "YES" : "NO";
      table->field[TMP_TABLE_KEYS_IS_VISIBLE]->store(is_visible,
                                                     strlen(is_visible), cs);
      table->field[TMP_TABLE_KEYS_IS_VISIBLE]->set_notnull();

      if (schema_table_store_record(thd, table)) DBUG_RETURN(1);
    }
  }
  DBUG_RETURN(res);
}

/*
  Convert a string in a given character set to a string which can be
  used for FRM file storage in which case use_hex is true and we store
  the character constants as hex strings in the character set encoding
  their field have. In the case of SHOW CREATE TABLE and the
  PARTITIONS information schema table we instead provide utf8 strings
  to the user and convert to the utf8 character set.

  SYNOPSIS
    get_cs_converted_part_value_from_string()
    item                           Item from which constant comes
    input_str                      String as provided by val_str after
                                   conversion to character set
    output_str                     Out value: The string created
    cs                             Character set string is encoded in
                                   NULL for INT_RESULT's here
    use_hex                        true => hex string created
                                   false => utf8 constant string created

  RETURN VALUES
    true                           Error
    false                          Ok
*/

int get_cs_converted_part_value_from_string(THD *thd, Item *item,
                                            String *input_str,
                                            String *output_str,
                                            const CHARSET_INFO *cs,
                                            bool use_hex) {
  if (item->result_type() == INT_RESULT) {
    longlong value = item->val_int();
    output_str->set(value, system_charset_info);
    return false;
  }
  if (!input_str) {
    my_error(ER_PARTITION_FUNCTION_IS_NOT_ALLOWED, MYF(0));
    return true;
  }
  get_cs_converted_string_value(thd, input_str, output_str, cs, use_hex);
  return false;
}

static int fill_open_tables(THD *thd, TABLE_LIST *tables, Item *) {
  DBUG_ENTER("fill_open_tables");
  const char *wild = thd->lex->wild ? thd->lex->wild->ptr() : NullS;
  TABLE *table = tables->table;
  CHARSET_INFO *cs = system_charset_info;
  OPEN_TABLE_LIST *open_list;
  if (!(open_list = list_open_tables(thd, thd->lex->select_lex->db, wild)) &&
      thd->is_fatal_error)
    DBUG_RETURN(1);

  for (; open_list; open_list = open_list->next) {
    restore_record(table, s->default_values);
    table->field[0]->store(open_list->db, strlen(open_list->db), cs);
    table->field[1]->store(open_list->table, strlen(open_list->table), cs);
    table->field[2]->store((longlong)open_list->in_use, true);
    table->field[3]->store((longlong)open_list->locked, true);
    if (schema_table_store_record(thd, table)) DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

struct schema_table_ref {
  const char *table_name;
  ST_SCHEMA_TABLE *schema_table;
};

/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table_in_plugin()
    plugin              plugin
    table_name          table name

  RETURN
    0	table not found
    1   found the schema table
*/
static bool find_schema_table_in_plugin(THD *, plugin_ref plugin,
                                        void *p_table) {
  schema_table_ref *p_schema_table = (schema_table_ref *)p_table;
  const char *table_name = p_schema_table->table_name;
  ST_SCHEMA_TABLE *schema_table = plugin_data<ST_SCHEMA_TABLE *>(plugin);
  DBUG_ENTER("find_schema_table_in_plugin");

  if (!my_strcasecmp(system_charset_info, schema_table->table_name,
                     table_name)) {
    p_schema_table->schema_table = schema_table;
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

/*
  Find schema_tables elment by name

  SYNOPSIS
    find_schema_table()
    thd                 thread handler
    table_name          table name

  RETURN
    0	table not found
    #   pointer to 'schema_tables' element
*/

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char *table_name) {
  schema_table_ref schema_table_a;
  ST_SCHEMA_TABLE *schema_table = schema_tables;
  DBUG_ENTER("find_schema_table");

  for (; schema_table->table_name; schema_table++) {
    if (!my_strcasecmp(system_charset_info, schema_table->table_name,
                       table_name))
      DBUG_RETURN(schema_table);
  }

  schema_table_a.table_name = table_name;
  if (plugin_foreach(thd, find_schema_table_in_plugin,
                     MYSQL_INFORMATION_SCHEMA_PLUGIN, &schema_table_a))
    DBUG_RETURN(schema_table_a.schema_table);

  DBUG_RETURN(NULL);
}

ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx) {
  return &schema_tables[schema_table_idx];
}

/**
  Create information_schema table using schema_table data.

  @note
    For MYSQL_TYPE_DECIMAL fields only, the field_length member has encoded
    into it two numbers, based on modulus of base-10 numbers.  In the ones
    position is the number of decimals.  Tens position is unused.  In the
    hundreds and thousands position is a two-digit decimal number representing
    length.  Encode this value with  (decimals*100)+length  , where
    0<decimals<10 and 0<=length<100 .

  @param thd	       	          thread handler

  @param table_list Used to pass I_S table information(fields info, tables
  parameters etc) and table name.

  @returns  Pointer to created table
  @retval  NULL           Can't create table
*/

static TABLE *create_schema_table(THD *thd, TABLE_LIST *table_list) {
  int field_count = 0;
  Item *item;
  TABLE *table;
  List<Item> field_list;
  ST_SCHEMA_TABLE *schema_table = table_list->schema_table;
  ST_FIELD_INFO *fields_info = schema_table->fields_info;
  CHARSET_INFO *cs = system_charset_info;
  DBUG_ENTER("create_schema_table");

  for (; fields_info->field_name; fields_info++) {
    switch (fields_info->field_type) {
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_INT24:
        if (!(item = new Item_return_int(
                  fields_info->field_name, fields_info->field_length,
                  fields_info->field_type, fields_info->value))) {
          DBUG_RETURN(0);
        }
        item->unsigned_flag = (fields_info->field_flags & MY_I_S_UNSIGNED);
        break;
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_TIMESTAMP:
      case MYSQL_TYPE_DATETIME: {
        const Name_string field_name(fields_info->field_name,
                                     strlen(fields_info->field_name));
        if (!(item =
                  new Item_temporal(fields_info->field_type, field_name, 0, 0)))
          DBUG_RETURN(0);

        if (fields_info->field_type == MYSQL_TYPE_TIMESTAMP ||
            fields_info->field_type == MYSQL_TYPE_DATETIME)
          item->decimals = fields_info->field_length;

        break;
      }
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE: {
        const Name_string field_name(fields_info->field_name,
                                     strlen(fields_info->field_name));
        if ((item = new Item_float(field_name, 0.0, NOT_FIXED_DEC,
                                   fields_info->field_length)) == NULL)
          DBUG_RETURN(NULL);
        break;
      }
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        if (!(item = new Item_decimal((longlong)fields_info->value, false))) {
          DBUG_RETURN(0);
        }
        item->unsigned_flag = (fields_info->field_flags & MY_I_S_UNSIGNED);
        item->decimals = fields_info->field_length % 10;
        item->max_length = (fields_info->field_length / 100) % 100;
        if (item->unsigned_flag == 0) item->max_length += 1;
        if (item->decimals > 0) item->max_length += 1;
        item->item_name.copy(fields_info->field_name);
        break;
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
        if (!(item = new Item_blob(fields_info->field_name,
                                   fields_info->field_length))) {
          DBUG_RETURN(0);
        }
        break;
      default:
        /* Don't let unimplemented types pass through. Could be a grave error.
         */
        DBUG_ASSERT(fields_info->field_type == MYSQL_TYPE_STRING);

        if (!(item =
                  new Item_empty_string("", fields_info->field_length, cs))) {
          DBUG_RETURN(0);
        }
        item->item_name.copy(fields_info->field_name);
        break;
    }
    field_list.push_back(item);
    item->maybe_null = (fields_info->field_flags & MY_I_S_MAYBE_NULL);
    field_count++;
  }
  Temp_table_param *tmp_table_param = new (thd->mem_root) Temp_table_param;
  if (!tmp_table_param) DBUG_RETURN(0);

  tmp_table_param->table_charset = cs;
  tmp_table_param->field_count = field_count;
  tmp_table_param->schema_table = 1;
  SELECT_LEX *select_lex = thd->lex->current_select();
  if (!(table = create_tmp_table(
            thd, tmp_table_param, field_list, (ORDER *)0, 0, 0,
            select_lex->active_options() | TMP_TABLE_ALL_COLUMNS, HA_POS_ERROR,
            table_list->alias)))
    DBUG_RETURN(0);
  my_bitmap_map *bitmaps =
      (my_bitmap_map *)thd->alloc(bitmap_buffer_size(field_count));
  bitmap_init(&table->def_read_set, bitmaps, field_count, false);
  table->read_set = &table->def_read_set;
  bitmap_clear_all(table->read_set);
  table_list->schema_table_param = tmp_table_param;
  DBUG_RETURN(table);
}

/*
  For old SHOW compatibility. It is used when
  old SHOW doesn't have generated column names
  Make list of fields for SHOW

  SYNOPSIS
    make_old_format()
    thd			thread handler
    schema_table        pointer to 'schema_tables' element

  RETURN
   1	error
   0	success
*/

static int make_old_format(THD *thd, ST_SCHEMA_TABLE *schema_table) {
  ST_FIELD_INFO *field_info = schema_table->fields_info;
  Name_resolution_context *context = &thd->lex->select_lex->context;
  for (; field_info->field_name; field_info++) {
    if (field_info->old_name) {
      Item_field *field =
          new Item_field(context, NullS, NullS, field_info->field_name);
      if (field) {
        field->item_name.copy(field_info->old_name);
        if (add_item_to_list(thd, field)) return 1;
      }
    }
  }
  return 0;
}

static int make_tmp_table_columns_format(THD *thd,
                                         ST_SCHEMA_TABLE *schema_table) {
  int fields_arr[] = {
      TMP_TABLE_COLUMNS_COLUMN_NAME,    TMP_TABLE_COLUMNS_COLUMN_TYPE,
      TMP_TABLE_COLUMNS_COLLATION_NAME, TMP_TABLE_COLUMNS_IS_NULLABLE,
      TMP_TABLE_COLUMNS_COLUMN_KEY,     TMP_TABLE_COLUMNS_COLUMN_DEFAULT,
      TMP_TABLE_COLUMNS_EXTRA,          TMP_TABLE_COLUMNS_PRIVILEGES,
      TMP_TABLE_COLUMNS_COLUMN_COMMENT, -1};
  int *field_num = fields_arr;
  ST_FIELD_INFO *field_info;
  Name_resolution_context *context = &thd->lex->select_lex->context;

  for (; *field_num >= 0; field_num++) {
    field_info = &schema_table->fields_info[*field_num];
    if (!thd->lex->verbose && (*field_num == TMP_TABLE_COLUMNS_COLLATION_NAME ||
                               *field_num == TMP_TABLE_COLUMNS_PRIVILEGES ||
                               *field_num == TMP_TABLE_COLUMNS_COLUMN_COMMENT))
      continue;
    Item_field *field =
        new Item_field(context, NullS, NullS, field_info->field_name);
    if (field) {
      field->item_name.copy(field_info->old_name);
      if (add_item_to_list(thd, field)) return 1;
    }
  }
  return 0;
}

/*
  Create information_schema table

  SYNOPSIS
  mysql_schema_table()
    thd                thread handler
    lex                pointer to LEX
    table_list         pointer to table_list

  RETURN
    0	success
    1   error
*/

int mysql_schema_table(THD *thd, LEX *lex, TABLE_LIST *table_list) {
  TABLE *table;
  DBUG_ENTER("mysql_schema_table");
  if (!(table = table_list->schema_table->create_table(thd, table_list)))
    DBUG_RETURN(1);
  table->s->tmp_table = SYSTEM_TMP_TABLE;
  table_list->grant.privilege = SELECT_ACL;
  /*
    This test is necessary to make
    case insensitive file systems +
    upper case table names(information schema tables) +
    views
    working correctly
  */
  if (table_list->schema_table_name)
    table->alias_name_used = my_strcasecmp(
        table_alias_charset, table_list->schema_table_name, table_list->alias);
  table_list->table_name = table->s->table_name.str;
  table_list->table_name_length = table->s->table_name.length;
  table_list->table = table;
  table->pos_in_table_list = table_list;
  if (table_list->select_lex->first_execution)
    table_list->select_lex->add_base_options(OPTION_SCHEMA_TABLE);
  lex->safe_to_cache_query = 0;

  if (table_list->schema_table_reformed)  // show command
  {
    SELECT_LEX *sel = lex->current_select();
    Item *item;
    Field_translator *transl, *org_transl;

    ulonglong want_privilege_saved = thd->want_privilege;
    thd->want_privilege = SELECT_ACL;
    enum enum_mark_columns save_mark_used_columns = thd->mark_used_columns;
    thd->mark_used_columns = MARK_COLUMNS_READ;

    if (table_list->field_translation) {
      Field_translator *end = table_list->field_translation_end;
      for (transl = table_list->field_translation; transl < end; transl++) {
        if (!transl->item->fixed &&
            transl->item->fix_fields(thd, &transl->item))
          DBUG_RETURN(1);
      }

      thd->want_privilege = want_privilege_saved;
      thd->mark_used_columns = save_mark_used_columns;

      DBUG_RETURN(0);
    }
    List_iterator_fast<Item> it(sel->item_list);
    if (!(transl = (Field_translator *)(thd->stmt_arena->alloc(
              sel->item_list.elements * sizeof(Field_translator))))) {
      DBUG_RETURN(1);
    }
    for (org_transl = transl; (item = it++); transl++) {
      transl->item = item;
      transl->name = item->item_name.ptr();
      if (!item->fixed && item->fix_fields(thd, &transl->item)) {
        DBUG_RETURN(1);
      }
    }

    thd->want_privilege = want_privilege_saved;
    thd->mark_used_columns = save_mark_used_columns;
    table_list->field_translation = org_transl;
    table_list->field_translation_end = transl;
  }

  DBUG_RETURN(0);
}

/*
  Generate select from information_schema table

  SYNOPSIS
    make_schema_select()
    thd                  thread handler
    sel                  pointer to SELECT_LEX
    schema_table_idx     index of 'schema_tables' element

  RETURN
    0	success
    1   error
*/

int make_schema_select(THD *thd, SELECT_LEX *sel,
                       enum enum_schema_tables schema_table_idx) {
  ST_SCHEMA_TABLE *schema_table = get_schema_table(schema_table_idx);
  LEX_STRING db, table;
  DBUG_ENTER("make_schema_select");
  DBUG_PRINT("enter", ("mysql_schema_select: %s", schema_table->table_name));
  /*
     We have to make non const db_name & table_name
     because of lower_case_table_names
  */
  thd->make_lex_string(&db, INFORMATION_SCHEMA_NAME.str,
                       INFORMATION_SCHEMA_NAME.length, 0);
  thd->make_lex_string(&table, schema_table->table_name,
                       strlen(schema_table->table_name), 0);

  if (schema_table->old_format(thd, schema_table) || /* Handle old syntax */
      !sel->add_table_to_list(
          thd,
          new (*THR_MALLOC) Table_ident(thd->get_protocol(), to_lex_cstring(db),
                                        to_lex_cstring(table), 0),
          0, 0, TL_READ, MDL_SHARED_READ)) {
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}

/**
  Fill INFORMATION_SCHEMA-table, leave correct Diagnostics_area
  state after itself.

  This function is a wrapper around ST_SCHEMA_TABLE::fill_table(), which
  may "partially silence" some errors. The thing is that during
  fill_table() many errors might be emitted. These errors stem from the
  nature of fill_table().

  For example, SELECT ... FROM INFORMATION_SCHEMA.xxx WHERE TABLE_NAME = 'xxx'
  results in a number of 'Table @<db name@>.xxx does not exist' errors,
  because fill_table() tries to open the 'xxx' table in every possible
  database.

  Those errors are cleared (the error status is cleared from
  Diagnostics_area) inside fill_table(), but they remain in the
  Diagnostics_area condition list (the list is not cleared because
  it may contain useful warnings).

  This function is responsible for making sure that Diagnostics_area
  does not contain warnings corresponding to the cleared errors.

  @note: THD::no_warnings_for_error used to be set before calling
  fill_table(), thus those errors didn't go to Diagnostics_area. This is not
  the case now (THD::no_warnings_for_error was eliminated as a hack), so we
  need to take care of those warnings here.

  @param thd            Thread context.
  @param table_list     I_S table.
  @param qep_tab     JOIN/SELECT table.

  @return Error status.
  @retval true Error.
  @retval false Success.
*/
static bool do_fill_table(THD *thd, TABLE_LIST *table_list, QEP_TAB *qep_tab) {
  /*
    Return if there is already an error reported.

    This situation occurs because there are few functions
    that return success, even after reporting error as
    mentioned in Bug#25642468. The following check would
    be removed by fix for Bug#25642468.
  */
  if (thd->is_error()) return true;

  // NOTE: fill_table() may generate many "useless" warnings, which will be
  // ignored afterwards. On the other hand, there might be "useful"
  // warnings, which should be presented to the user. Diagnostics_area usually
  // stores no more than THD::variables.max_error_count warnings.
  // The problem is that "useless warnings" may occupy all the slots in the
  // Diagnostics_area, so "useful warnings" get rejected. In order to avoid
  // that problem we create a Diagnostics_area instance, which is capable of
  // storing "unlimited" number of warnings.
  Diagnostics_area *da = thd->get_stmt_da();
  Diagnostics_area tmp_da(true);

  // Don't copy existing conditions from the old DA so we don't get them twice
  // when we call copy_non_errors_from_da below.
  thd->push_diagnostics_area(&tmp_da, false);

  /*
    We pass a condition, which can be used to do less file manipulations (for
    example, WHERE TABLE_SCHEMA='test' allows to open only directory 'test',
    not other database directories). Filling schema tables is done before
    QEP_TAB::sort_table() (=filesort, for ORDER BY), so we can trust
    that condition() is complete, has not been zeroed by filesort:
  */
  DBUG_ASSERT(qep_tab->condition() == qep_tab->condition_optim());

  bool res = table_list->schema_table->fill_table(thd, table_list,
                                                  qep_tab->condition());

  thd->pop_diagnostics_area();

  // Pass an error if any.
  if (tmp_da.is_error()) {
    da->set_error_status(tmp_da.mysql_errno(), tmp_da.message_text(),
                         tmp_da.returned_sqlstate());
    da->push_warning(thd, tmp_da.mysql_errno(), tmp_da.returned_sqlstate(),
                     Sql_condition::SL_ERROR, tmp_da.message_text());
  }

  // Pass warnings (if any).
  //
  // Filter out warnings with SL_ERROR level, because they
  // correspond to the errors which were filtered out in fill_table().
  da->copy_non_errors_from_da(thd, &tmp_da);

  return res;
}

/*
  Fill temporary schema tables before SELECT

  SYNOPSIS
    get_schema_tables_result()
    join  join which use schema tables
    executed_place place where I_S table processed

  RETURN
    false success
    true  error
*/

bool get_schema_tables_result(JOIN *join,
                              enum enum_schema_table_state executed_place) {
  THD *thd = join->thd;
  bool result = 0;
  DBUG_ENTER("get_schema_tables_result");

  /* Check if the schema table is optimized away */
  if (!join->qep_tab) DBUG_RETURN(result);

  for (uint i = 0; i < join->tables; i++) {
    QEP_TAB *const tab = join->qep_tab + i;
    if (!tab->table() || !tab->table_ref) continue;

    TABLE_LIST *const table_list = tab->table_ref;
    if (table_list->schema_table && thd->fill_information_schema_tables()) {
      bool is_subselect = join->select_lex->master_unit() &&
                          join->select_lex->master_unit()->item;

      /* A value of 0 indicates a dummy implementation */
      if (table_list->schema_table->fill_table == 0) continue;

      /* skip I_S optimizations specific to get_all_tables */
      if (thd->lex->is_explain() &&
          (table_list->schema_table->fill_table != get_all_tables))
        continue;

      /*
        If schema table is already processed and
        the statement is not a subselect then
        we don't need to fill this table again.
        If schema table is already processed and
        schema_table_state != executed_place then
        table is already processed and
        we should skip second data processing.
      */
      if (table_list->schema_table_state &&
          (!is_subselect || table_list->schema_table_state != executed_place))
        continue;

      /*
        if table is used in a subselect and
        table has been processed earlier with the same
        'executed_place' value then we should refresh the table.
      */
      if (table_list->schema_table_state && is_subselect) {
        table_list->table->file->extra(HA_EXTRA_RESET_STATE);
        table_list->table->file->ha_delete_all_rows();
        free_io_cache(table_list->table);
        filesort_free_buffers(table_list->table, 1);
        table_list->table->set_not_started();
      } else
        table_list->table->file->stats.records = 0;

      /* To be removed after 5.7 */
      if (is_infoschema_db(table_list->db, table_list->db_length)) {
        static LEX_STRING INNODB_LOCKS = {C_STRING_WITH_LEN("INNODB_LOCKS")};
        static LEX_STRING INNODB_LOCK_WAITS = {
            C_STRING_WITH_LEN("INNODB_LOCK_WAITS")};

        if (my_strcasecmp(system_charset_info, table_list->schema_table_name,
                          INNODB_LOCKS.str) == 0) {
          /* Deprecated in 5.7 */
          push_warning_printf(
              thd, Sql_condition::SL_WARNING,
              ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
              ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
              "INFORMATION_SCHEMA.INNODB_LOCKS");
        } else if (my_strcasecmp(system_charset_info,
                                 table_list->schema_table_name,
                                 INNODB_LOCK_WAITS.str) == 0) {
          /* Deprecated in 5.7 */
          push_warning_printf(
              thd, Sql_condition::SL_WARNING,
              ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT,
              ER_THD(thd, ER_WARN_DEPRECATED_SYNTAX_NO_REPLACEMENT),
              "INFORMATION_SCHEMA.INNODB_LOCK_WAITS");
        }
      }

      if (do_fill_table(thd, table_list, tab)) {
        result = 1;
        join->error = 1;
        table_list->schema_table_state = executed_place;
        break;
      }
      table_list->schema_table_state = executed_place;
    }
  }
  DBUG_RETURN(result);
}

struct run_hton_fill_schema_table_args {
  TABLE_LIST *tables;
  Item *cond;
};

static bool run_hton_fill_schema_table(THD *thd, plugin_ref plugin, void *arg) {
  struct run_hton_fill_schema_table_args *args =
      (run_hton_fill_schema_table_args *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  if (hton->fill_is_table && hton->state == SHOW_OPTION_YES)
    hton->fill_is_table(hton, thd, args->tables, args->cond,
                        get_schema_table_idx(args->tables->schema_table));
  return false;
}

static int hton_fill_schema_table(THD *thd, TABLE_LIST *tables, Item *cond) {
  DBUG_ENTER("hton_fill_schema_table");

  struct run_hton_fill_schema_table_args args;
  args.tables = tables;
  args.cond = cond;

  plugin_foreach(thd, run_hton_fill_schema_table, MYSQL_STORAGE_ENGINE_PLUGIN,
                 &args);

  DBUG_RETURN(0);
}

ST_FIELD_INFO engines_fields_info[] = {
    {"ENGINE", 64, MYSQL_TYPE_STRING, 0, 0, "Engine", SKIP_OPEN_TABLE},
    {"SUPPORT", 8, MYSQL_TYPE_STRING, 0, 0, "Support", SKIP_OPEN_TABLE},
    {"COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, "Comment", SKIP_OPEN_TABLE},
    {"TRANSACTIONS", 3, MYSQL_TYPE_STRING, 0, 1, "Transactions",
     SKIP_OPEN_TABLE},
    {"XA", 3, MYSQL_TYPE_STRING, 0, 1, "XA", SKIP_OPEN_TABLE},
    {"SAVEPOINTS", 3, MYSQL_TYPE_STRING, 0, 1, "Savepoints", SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO tmp_table_keys_fields_info[] = {
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table",
     OPEN_FRM_ONLY},
    {"NON_UNIQUE", 1, MYSQL_TYPE_LONGLONG, 0, 0, "Non_unique", OPEN_FRM_ONLY},
    {"INDEX_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FRM_ONLY},
    {"INDEX_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Key_name",
     OPEN_FRM_ONLY},
    {"SEQ_IN_INDEX", 2, MYSQL_TYPE_LONGLONG, 0, 0, "Seq_in_index",
     OPEN_FRM_ONLY},
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Column_name",
     OPEN_FRM_ONLY},
    {"COLLATION", 1, MYSQL_TYPE_STRING, 0, 1, "Collation", OPEN_FRM_ONLY},
    {"CARDINALITY", MY_INT64_NUM_DECIMAL_DIGITS, MYSQL_TYPE_LONGLONG, 0, 1,
     "Cardinality", OPEN_FULL_TABLE},
    {"SUB_PART", 3, MYSQL_TYPE_LONGLONG, 0, 1, "Sub_part", OPEN_FRM_ONLY},
    {"PACKED", 10, MYSQL_TYPE_STRING, 0, 1, "Packed", OPEN_FRM_ONLY},
    {"NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
    {"INDEX_TYPE", 16, MYSQL_TYPE_STRING, 0, 0, "Index_type", OPEN_FULL_TABLE},
    {"COMMENT", 16, MYSQL_TYPE_STRING, 0, 1, "Comment", OPEN_FRM_ONLY},
    {"INDEX_COMMENT", INDEX_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0,
     "Index_comment", OPEN_FRM_ONLY},
    {"IS_VISIBLE", 4, MYSQL_TYPE_STRING, 0, 1, "Visible", OPEN_FULL_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO user_privileges_fields_info[] = {
    {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO schema_privileges_fields_info[] = {
    {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO table_privileges_fields_info[] = {
    {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO column_privileges_fields_info[] = {
    {"GRANTEE", 81, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"PRIVILEGE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"IS_GRANTABLE", 3, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO open_tables_fields_info[] = {
    {"Database", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Database",
     SKIP_OPEN_TABLE},
    {"Table", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Table", SKIP_OPEN_TABLE},
    {"In_use", 1, MYSQL_TYPE_LONGLONG, 0, 0, "In_use", SKIP_OPEN_TABLE},
    {"Name_locked", 4, MYSQL_TYPE_LONGLONG, 0, 0, "Name_locked",
     SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO partitions_fields_info[] = {
    {"TABLE_CATALOG", FN_REFLEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
    {"TABLE_SCHEMA", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     OPEN_FULL_TABLE},
    {"TABLE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
    {"PARTITION_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {"SUBPARTITION_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {"PARTITION_ORDINAL_POSITION", 21, MYSQL_TYPE_LONGLONG, 0,
     (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
    {"SUBPARTITION_ORDINAL_POSITION", 21, MYSQL_TYPE_LONGLONG, 0,
     (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
    {"PARTITION_METHOD", 18, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
    {"SUBPARTITION_METHOD", 12, MYSQL_TYPE_STRING, 0, 1, 0, OPEN_FULL_TABLE},
    {"PARTITION_EXPRESSION", 65535, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {"SUBPARTITION_EXPRESSION", 65535, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {"PARTITION_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {"TABLE_ROWS", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
     OPEN_FULL_TABLE},
    {"AVG_ROW_LENGTH", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
     OPEN_FULL_TABLE},
    {"DATA_LENGTH", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
     OPEN_FULL_TABLE},
    {"MAX_DATA_LENGTH", 21, MYSQL_TYPE_LONGLONG, 0,
     (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
    {"INDEX_LENGTH", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
     OPEN_FULL_TABLE},
    {"DATA_FREE", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, 0,
     OPEN_FULL_TABLE},
    {"CREATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
    {"UPDATE_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
    {"CHECK_TIME", 0, MYSQL_TYPE_DATETIME, 0, 1, 0, OPEN_FULL_TABLE},
    {"CHECKSUM", 21, MYSQL_TYPE_LONGLONG, 0,
     (MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED), 0, OPEN_FULL_TABLE},
    {"PARTITION_COMMENT", 80, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
    {"NODEGROUP", 12, MYSQL_TYPE_STRING, 0, 0, 0, OPEN_FULL_TABLE},
    {"TABLESPACE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
     OPEN_FULL_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO processlist_fields_info[] = {
    {"ID", 21, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, "Id", SKIP_OPEN_TABLE},
    {"USER", USERNAME_CHAR_LENGTH, MYSQL_TYPE_STRING, 0, 0, "User",
     SKIP_OPEN_TABLE},
    {"HOST", LIST_PROCESS_HOST_LEN, MYSQL_TYPE_STRING, 0, 0, "Host",
     SKIP_OPEN_TABLE},
    {"DB", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Db", SKIP_OPEN_TABLE},
    {"COMMAND", 16, MYSQL_TYPE_STRING, 0, 0, "Command", SKIP_OPEN_TABLE},
    {"TIME", 7, MYSQL_TYPE_LONG, 0, 0, "Time", SKIP_OPEN_TABLE},
    {"STATE", 64, MYSQL_TYPE_STRING, 0, 1, "State", SKIP_OPEN_TABLE},
    {"INFO", PROCESS_LIST_INFO_WIDTH, MYSQL_TYPE_STRING, 0, 1, "Info",
     SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO plugin_fields_info[] = {
    {"PLUGIN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Name",
     SKIP_OPEN_TABLE},
    {"PLUGIN_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"PLUGIN_STATUS", 10, MYSQL_TYPE_STRING, 0, 0, "Status", SKIP_OPEN_TABLE},
    {"PLUGIN_TYPE", 80, MYSQL_TYPE_STRING, 0, 0, "Type", SKIP_OPEN_TABLE},
    {"PLUGIN_TYPE_VERSION", 20, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"PLUGIN_LIBRARY", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, "Library",
     SKIP_OPEN_TABLE},
    {"PLUGIN_LIBRARY_VERSION", 20, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
    {"PLUGIN_AUTHOR", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 1, 0,
     SKIP_OPEN_TABLE},
    {"PLUGIN_DESCRIPTION", 65535, MYSQL_TYPE_STRING, 0, 1, 0, SKIP_OPEN_TABLE},
    {"PLUGIN_LICENSE", 80, MYSQL_TYPE_STRING, 0, 1, "License", SKIP_OPEN_TABLE},
    {"LOAD_OPTION", 64, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO tablespaces_fields_info[] = {
    {"TABLESPACE_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0,
     SKIP_OPEN_TABLE},
    {"ENGINE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE},
    {"TABLESPACE_TYPE", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL,
     0, SKIP_OPEN_TABLE},
    {"LOGFILE_GROUP_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0,
     MY_I_S_MAYBE_NULL, 0, SKIP_OPEN_TABLE},
    {"EXTENT_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
    {"AUTOEXTEND_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
    {"MAXIMUM_SIZE", 21, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
    {"NODEGROUP_ID", 21, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_MAYBE_NULL | MY_I_S_UNSIGNED, 0, SKIP_OPEN_TABLE},
    {"TABLESPACE_COMMENT", 2048, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL, 0,
     SKIP_OPEN_TABLE},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

ST_FIELD_INFO tmp_table_columns_fields_info[] = {
    {"COLUMN_NAME", NAME_CHAR_LEN, MYSQL_TYPE_STRING, 0, 0, "Field",
     OPEN_FRM_ONLY},
    {"COLUMN_TYPE", 65535, MYSQL_TYPE_STRING, 0, 0, "Type", OPEN_FRM_ONLY},
    {"COLLATION_NAME", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING, 0, 1, "Collation",
     OPEN_FRM_ONLY},
    {"IS_NULLABLE", 3, MYSQL_TYPE_STRING, 0, 0, "Null", OPEN_FRM_ONLY},
    {"COLUMN_KEY", 3, MYSQL_TYPE_STRING, 0, 0, "Key", OPEN_FRM_ONLY},
    {"COLUMN_DEFAULT", MAX_FIELD_VARCHARLENGTH, MYSQL_TYPE_STRING, 0, 1,
     "Default", OPEN_FRM_ONLY},
    {"EXTRA", 30, MYSQL_TYPE_STRING, 0, 0, "Extra", OPEN_FRM_ONLY},
    {"PRIVILEGES", 80, MYSQL_TYPE_STRING, 0, 0, "Privileges", OPEN_FRM_ONLY},
    {"COLUMN_COMMENT", COLUMN_COMMENT_MAXLEN, MYSQL_TYPE_STRING, 0, 0,
     "Comment", OPEN_FRM_ONLY},
    {"GENERATION_EXPRESSION", GENERATED_COLUMN_EXPRESSION_MAXLEN,
     MYSQL_TYPE_STRING, 0, 0, "Generation expression", OPEN_FRM_ONLY},
    {0, 0, MYSQL_TYPE_STRING, 0, 0, 0, SKIP_OPEN_TABLE}};

/** For creating fields of information_schema.OPTIMIZER_TRACE */
extern ST_FIELD_INFO optimizer_trace_info[];

/*
  Description of ST_FIELD_INFO in table.h

  Make sure that the order of schema_tables and enum_schema_tables are the same.

*/

ST_SCHEMA_TABLE schema_tables[] = {
    {"COLUMN_PRIVILEGES", column_privileges_fields_info, create_schema_table,
     fill_schema_column_privileges, 0, 0, -1, -1, 0, 0},
    {"ENGINES", engines_fields_info, create_schema_table, fill_schema_engines,
     make_old_format, 0, -1, -1, 0, 0},
    {"OPEN_TABLES", open_tables_fields_info, create_schema_table,
     fill_open_tables, make_old_format, 0, -1, -1, 1, 0},
    {"OPTIMIZER_TRACE", optimizer_trace_info, create_schema_table,
     fill_optimizer_trace_info, NULL, NULL, -1, -1, false, 0},
    {"PLUGINS", plugin_fields_info, create_schema_table, fill_plugins,
     make_old_format, 0, -1, -1, 0, 0},
    {"PROCESSLIST", processlist_fields_info, create_schema_table,
     fill_schema_processlist, make_old_format, 0, -1, -1, 0, 0},
    {"PROFILING", query_profile_statistics_info, create_schema_table,
     fill_query_profile_statistics_info, make_profile_table_for_show, NULL, -1,
     -1, false, 0},
    {"SCHEMA_PRIVILEGES", schema_privileges_fields_info, create_schema_table,
     fill_schema_schema_privileges, 0, 0, -1, -1, 0, 0},
    {"TABLESPACES", tablespaces_fields_info, create_schema_table,
     hton_fill_schema_table, 0, 0, -1, -1, 0, 0},
    {"TABLE_PRIVILEGES", table_privileges_fields_info, create_schema_table,
     fill_schema_table_privileges, 0, 0, -1, -1, 0, 0},
    {"USER_PRIVILEGES", user_privileges_fields_info, create_schema_table,
     fill_schema_user_privileges, 0, 0, -1, -1, 0, 0},
    {"TMP_TABLE_COLUMNS", tmp_table_columns_fields_info, create_schema_table,
     get_all_tables, make_tmp_table_columns_format,
     get_schema_tmp_table_columns_record, -1, -1, 1, 0},
    {"TMP_TABLE_KEYS", tmp_table_keys_fields_info, create_schema_table,
     get_all_tables, make_old_format, get_schema_tmp_table_keys_record, -1, -1,
     1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

int initialize_schema_table(st_plugin_int *plugin) {
  ST_SCHEMA_TABLE *schema_table;
  DBUG_ENTER("initialize_schema_table");

  if (!(schema_table = (ST_SCHEMA_TABLE *)my_malloc(key_memory_ST_SCHEMA_TABLE,
                                                    sizeof(ST_SCHEMA_TABLE),
                                                    MYF(MY_WME | MY_ZEROFILL))))
    DBUG_RETURN(1);
  /* Historical Requirement */
  plugin->data = schema_table;  // shortcut for the future
  if (plugin->plugin->init) {
    schema_table->create_table = create_schema_table;
    schema_table->old_format = make_old_format;
    schema_table->idx_field1 = -1, schema_table->idx_field2 = -1;

    /* Make the name available to the init() function. */
    schema_table->table_name = plugin->name.str;

    if (plugin->plugin->init(schema_table)) {
      LogErr(ERROR_LEVEL, ER_PLUGIN_INIT_FAILED, plugin->name.str);
      plugin->data = NULL;
      my_free(schema_table);
      DBUG_RETURN(1);
    }

    /* Make sure the plugin name is not set inside the init() function. */
    schema_table->table_name = plugin->name.str;
  }
  DBUG_RETURN(0);
}

int finalize_schema_table(st_plugin_int *plugin) {
  ST_SCHEMA_TABLE *schema_table = (ST_SCHEMA_TABLE *)plugin->data;
  DBUG_ENTER("finalize_schema_table");

  if (schema_table) {
    if (plugin->plugin->deinit) {
      DBUG_PRINT("info", ("Deinitializing plugin: '%s'", plugin->name.str));
      if (plugin->plugin->deinit(NULL)) {
        DBUG_PRINT("warning", ("Plugin '%s' deinit function returned error.",
                               plugin->name.str));
      }
    }
    my_free(schema_table);
  }
  DBUG_RETURN(0);
}

/**
  Output trigger information (SHOW CREATE TRIGGER) to the client.

  @param thd          Thread context.
  @param trigger      table trigger to dump.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

static bool show_create_trigger_impl(THD *thd, Trigger *trigger) {
  Protocol *p = thd->get_protocol();
  List<Item> fields;

  // Construct sql_mode string.

  LEX_STRING sql_mode_str;

  if (sql_mode_string_representation(thd, trigger->get_sql_mode(),
                                     &sql_mode_str))
    return true;

  char create_trg_str_buf[10 * STRING_BUFFER_USUAL_SIZE];
  String create_trg_str(create_trg_str_buf, sizeof(create_trg_str_buf),
                        system_charset_info);
  create_trg_str.length(0);

  /*
    NOTE: SQL statement field must be not less than 1024 in order not to
    confuse old clients.
  */
  Item_empty_string *stmt_fld = new Item_empty_string(
      "SQL Original Statement", max<size_t>(create_trg_str.length(), 1024));

  if (stmt_fld == nullptr) return true;

  if (trigger->create_full_trigger_definition(thd, &create_trg_str))
    return true;

  stmt_fld->maybe_null = true;

  // Send header.

  if (fields.push_back(new Item_empty_string("Trigger", NAME_LEN)) ||
      fields.push_back(
          new Item_empty_string("sql_mode", sql_mode_str.length)) ||
      fields.push_back(stmt_fld) ||
      fields.push_back(
          new Item_empty_string("character_set_client", MY_CS_NAME_SIZE)) ||
      fields.push_back(
          new Item_empty_string("collation_connection", MY_CS_NAME_SIZE)) ||
      fields.push_back(
          new Item_empty_string("Database Collation", MY_CS_NAME_SIZE)) ||
      fields.push_back(new Item_temporal(
          MYSQL_TYPE_TIMESTAMP, Name_string("Created", sizeof("created") - 1),
          0, 0)) ||
      thd->send_result_metadata(&fields,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  // Resolve trigger client character set.

  const CHARSET_INFO *client_cs;

  if (resolve_charset(trigger->get_client_cs_name().str, NULL, &client_cs))
    return true;

  // Send data.

  p->start_row();

  if (p->store(trigger->get_trigger_name().str,
               trigger->get_trigger_name().length, system_charset_info) ||
      p->store(sql_mode_str, system_charset_info) ||
      p->store(create_trg_str.c_ptr(), create_trg_str.length(), client_cs) ||
      p->store(trigger->get_client_cs_name().str,
               trigger->get_client_cs_name().length, system_charset_info) ||
      p->store(trigger->get_connection_cl_name().str,
               trigger->get_connection_cl_name().length, system_charset_info) ||
      p->store(trigger->get_db_cl_name().str, trigger->get_db_cl_name().length,
               system_charset_info))
    return true;

  bool rc;
  if (!trigger->is_created_timestamp_null()) {
    MYSQL_TIME timestamp;
    my_tz_SYSTEM->gmt_sec_to_TIME(&timestamp, trigger->get_created_timestamp());
    rc = p->store(&timestamp, 2);
  } else
    rc = p->store_null();

  if (rc || p->end_row()) return true;

  my_eof(thd);

  return false;
}

/**
  Read the Data Dictionary to obtain base table name for the specified
  trigger name and construct TABE_LIST object for the base table.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return TABLE_LIST object corresponding to the base table.

  TODO: This function is a copy&paste from add_table_to_list() and
  sp_add_to_query_tables(). The problem is that in order to be compatible
  with Stored Programs (Prepared Statements), we should not touch thd->lex.
  The "source" functions also add created TABLE_LIST object to the
  thd->lex->query_tables.

  The plan to eliminate this copy&paste is to:

    - get rid of sp_add_to_query_tables() and use Lex::add_table_to_list().
      Only add_table_to_list() must be used to add tables from the parser
      into Lex::query_tables list.

    - do not update Lex::query_tables in add_table_to_list().
*/

static TABLE_LIST *get_trigger_table(THD *thd, const sp_name *trg_name) {
  LEX_CSTRING db;
  LEX_STRING tbl_name;

  dd::Schema_MDL_locker mdl_locker(thd);

  dd::cache::Dictionary_client *dd_client = thd->dd_client();
  dd::cache::Dictionary_client::Auto_releaser releaser(dd_client);

  const dd::Schema *sch_obj = nullptr;
  if (mdl_locker.ensure_locked(trg_name->m_db.str) ||
      dd_client->acquire(trg_name->m_db.str, &sch_obj))
    return nullptr;

  if (sch_obj == nullptr) {
    my_error(ER_BAD_DB_ERROR, MYF(0), trg_name->m_db.str);
    return nullptr;
  }

  dd::String_type table_name;
  if (dd_client->get_table_name_by_trigger_name(*sch_obj, trg_name->m_name.str,
                                                &table_name))
    return nullptr;

  if (table_name == "") {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    return nullptr;
  }

  db = trg_name->m_db;
  db.str = thd->strmake(db.str, db.length);

  tbl_name.str = thd->strmake(table_name.c_str(), table_name.length());
  tbl_name.length = table_name.length();

  if (db.str == nullptr || tbl_name.str == nullptr) return nullptr;

  /* We need to reset statement table list to be PS/SP friendly. */
  return new (thd->mem_root)
      TABLE_LIST(db.str, db.length, tbl_name.str, tbl_name.length, tbl_name.str,
                 TL_IGNORE);
}

/**
  Acquire shared MDL lock for a specified database name/table name.

  @param thd         Thread context.
  @param db_name     Database name.
  @param table_name  Table name.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

static bool acquire_mdl_for_table(THD *thd, const char *db_name,
                                  const char *table_name) {
  MDL_request table_request;
  MDL_REQUEST_INIT(&table_request, MDL_key::TABLE, db_name, table_name,
                   MDL_SHARED, MDL_TRANSACTION);

  if (thd->mdl_context.acquire_lock(&table_request,
                                    thd->variables.lock_wait_timeout))
    return true;

  return false;
}

/**
  SHOW CREATE TRIGGER high-level implementation.

  @param thd      Thread context.
  @param trg_name Trigger name.

  @return Operation status
    @retval true Error.
    @retval false Success.
*/

bool show_create_trigger(THD *thd, const sp_name *trg_name) {
  uint num_tables; /* NOTE: unused, only to pass to open_tables(). */
  bool error = true;
  Trigger *trigger;

  /*
    Metadata locks taken during SHOW CREATE TRIGGER should be released when
    the statement completes as it is an information statement.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  if (acquire_shared_mdl_for_trigger(thd, trg_name->m_db.str,
                                     trg_name->m_name.str))
    return true;

  TABLE_LIST *lst = get_trigger_table(thd, trg_name);

  if (!lst) return true;

  if (check_table_access(thd, TRIGGER_ACL, lst, false, 1, true)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "TRIGGER");
    return true;
  }

  DEBUG_SYNC(thd, "show_create_trigger_before_table_lock");

  if (acquire_mdl_for_table(thd, trg_name->m_db.str, lst->table_name))
    return true;

  /*
    Open the table by name in order to load Table_trigger_dispatcher object.
  */
  if (open_tables(thd, &lst, &num_tables,
                  MYSQL_OPEN_FORCE_SHARED_HIGH_PRIO_MDL)) {
    my_error(ER_TRG_CANT_OPEN_TABLE, MYF(0), trg_name->m_db.str,
             lst->table_name);

    goto exit;

    /* Perform closing actions and return error status. */
  }

  if (!lst->table->triggers) {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    goto exit;
  }

  trigger = lst->table->triggers->find_trigger(trg_name->m_name);

  if (!trigger) {
    my_error(ER_TRG_CORRUPTED_FILE, MYF(0), trg_name->m_db.str,
             lst->table_name);

    goto exit;
  }

  error = show_create_trigger_impl(thd, trigger);

  /*
    NOTE: if show_create_trigger_impl() failed, that means we could not
    send data to the client. In this case we simply raise the error
    status and client connection will be closed.
  */

exit:
  close_thread_tables(thd);
  /* Release any metadata locks taken during SHOW CREATE TRIGGER. */
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);
  return error;
}

static IS_internal_schema_access is_internal_schema_access;

void initialize_information_schema_acl() {
  ACL_internal_schema_registry::register_schema(INFORMATION_SCHEMA_NAME,
                                                &is_internal_schema_access);
}

/*
  Convert a string in character set in column character set format
  to utf8 character set if possible, the utf8 character set string
  will later possibly be converted to character set used by client.
  Thus we attempt conversion from column character set to both
  utf8 and to character set client.

  Examples of strings that should fail conversion to utf8 are unassigned
  characters as e.g. 0x81 in cp1250 (Windows character set for for countries
  like Czech and Poland). Example of string that should fail conversion to
  character set on client (e.g. if this is latin1) is 0x2020 (daggger) in
  ucs2.

  If the conversion fails we will as a fall back convert the string to
  hex encoded format. The caller of the function can also ask for hex
  encoded format of output string unconditionally.

  SYNOPSIS
    get_cs_converted_string_value()
    thd                             Thread object
    input_str                       Input string in cs character set
    output_str                      Output string to be produced in utf8
    cs                              Character set of input string
    use_hex                         Use hex string unconditionally


  RETURN VALUES
    No return value
*/

static void get_cs_converted_string_value(THD *thd, String *input_str,
                                          String *output_str,
                                          const CHARSET_INFO *cs,
                                          bool use_hex) {
  output_str->length(0);
  if (input_str->length() == 0) {
    output_str->append("''");
    return;
  }
  if (!use_hex) {
    String try_val;
    uint try_conv_error = 0;

    try_val.copy(input_str->ptr(), input_str->length(), cs,
                 thd->variables.character_set_client, &try_conv_error);
    if (!try_conv_error) {
      String val;
      uint conv_error = 0;

      val.copy(input_str->ptr(), input_str->length(), cs, system_charset_info,
               &conv_error);
      if (!conv_error) {
        append_unescaped(output_str, val.ptr(), val.length());
        return;
      }
    }
    /* We had a conversion error, use hex encoded string for safety */
  }
  {
    const uchar *ptr;
    size_t i, len;
    char buf[3];

    output_str->append("_");
    output_str->append(cs->csname);
    output_str->append(" ");
    output_str->append("0x");
    len = input_str->length();
    ptr = (uchar *)input_str->ptr();
    for (i = 0; i < len; i++) {
      uint high, low;

      high = (*ptr) >> 4;
      low = (*ptr) & 0x0F;
      buf[0] = _dig_vec_upper[high];
      buf[1] = _dig_vec_upper[low];
      buf[2] = 0;
      output_str->append((const char *)buf);
      ptr++;
    }
  }
  return;
}

/**
  A field's SQL type printout

  @param type     the type to print
  @param metadata field's metadata, depending on the type
                  could be nothing, length, or length + decimals
  @param str      String to print to
  @param field_cs field's charset. When given [var]char length is printed in
                  characters, otherwise - in bytes

*/

void show_sql_type(enum_field_types type, uint16 metadata, String *str,
                   const CHARSET_INFO *field_cs) {
  DBUG_ENTER("show_sql_type");
  DBUG_PRINT("enter", ("type: %d, metadata: 0x%x", type, metadata));

  switch (type) {
    case MYSQL_TYPE_TINY:
      str->set_ascii(STRING_WITH_LEN("tinyint"));
      break;

    case MYSQL_TYPE_SHORT:
      str->set_ascii(STRING_WITH_LEN("smallint"));
      break;

    case MYSQL_TYPE_LONG:
      str->set_ascii(STRING_WITH_LEN("int"));
      break;

    case MYSQL_TYPE_FLOAT:
      str->set_ascii(STRING_WITH_LEN("float"));
      break;

    case MYSQL_TYPE_DOUBLE:
      str->set_ascii(STRING_WITH_LEN("double"));
      break;

    case MYSQL_TYPE_NULL:
      str->set_ascii(STRING_WITH_LEN("null"));
      break;

    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_TIMESTAMP2:
      str->set_ascii(STRING_WITH_LEN("timestamp"));
      break;

    case MYSQL_TYPE_LONGLONG:
      str->set_ascii(STRING_WITH_LEN("bigint"));
      break;

    case MYSQL_TYPE_INT24:
      str->set_ascii(STRING_WITH_LEN("mediumint"));
      break;

    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
      str->set_ascii(STRING_WITH_LEN("date"));
      break;

    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIME2:
      str->set_ascii(STRING_WITH_LEN("time"));
      break;

    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_DATETIME2:
      str->set_ascii(STRING_WITH_LEN("datetime"));
      break;

    case MYSQL_TYPE_YEAR:
      str->set_ascii(STRING_WITH_LEN("year"));
      break;

    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_VARCHAR: {
      const CHARSET_INFO *cs = str->charset();
      size_t length;
      if (field_cs)
        length =
            cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                               "varchar(%u)", metadata / field_cs->mbmaxlen);
      else
        length =
            cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                               "varchar(%u(bytes))", metadata);
      str->length(length);
    } break;

    case MYSQL_TYPE_BIT: {
      const CHARSET_INFO *cs = str->charset();
      int bit_length = 8 * (metadata >> 8) + (metadata & 0xFF);
      size_t length = cs->cset->snprintf(
          cs, (char *)str->ptr(), str->alloced_length(), "bit(%d)", bit_length);
      str->length(length);
    } break;

    case MYSQL_TYPE_DECIMAL: {
      const CHARSET_INFO *cs = str->charset();
      size_t length =
          cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                             "decimal(%d,?)", metadata);
      str->length(length);
    } break;

    case MYSQL_TYPE_NEWDECIMAL: {
      const CHARSET_INFO *cs = str->charset();
      /*
        Field_new_decimal encodes metadata this way. Bit shifts can't be used
        due to different endianness on different platforms.
      */
      uchar *metadata_ptr = (uchar *)&metadata;
      uint len = *metadata_ptr;
      uint dec = *(metadata_ptr + 1);
      size_t length =
          cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                             "decimal(%d,%d)", len, dec);
      str->length(length);
    } break;

    case MYSQL_TYPE_ENUM:
      str->set_ascii(STRING_WITH_LEN("enum"));
      break;

    case MYSQL_TYPE_SET:
      str->set_ascii(STRING_WITH_LEN("set"));
      break;

    case MYSQL_TYPE_TINY_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("tinyblob"));
      else
        str->set_ascii(STRING_WITH_LEN("tinytext"));
      break;

    case MYSQL_TYPE_MEDIUM_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("mediumblob"));
      else
        str->set_ascii(STRING_WITH_LEN("mediumtext"));
      break;

    case MYSQL_TYPE_LONG_BLOB:
      if (!field_cs || field_cs == &my_charset_bin)
        str->set_ascii(STRING_WITH_LEN("longblob"));
      else
        str->set_ascii(STRING_WITH_LEN("longtext"));
      break;

    case MYSQL_TYPE_BLOB:
      /*
        Field::real_type() lies regarding the actual type of a BLOB, so
        it is necessary to check the pack length to figure out what kind
        of blob it really is.
        Non-'BLOB' is handled above.
       */
      switch (metadata) {
        case 1:
          str->set_ascii(STRING_WITH_LEN("tinyblob"));
          break;

        case 3:
          str->set_ascii(STRING_WITH_LEN("mediumblob"));
          break;

        case 4:
          str->set_ascii(STRING_WITH_LEN("longblob"));
          break;

        default:
        case 2:
          if (!field_cs || field_cs == &my_charset_bin)
            str->set_ascii(STRING_WITH_LEN("blob"));
          else
            str->set_ascii(STRING_WITH_LEN("text"));
          break;
      }
      break;

    case MYSQL_TYPE_STRING: {
      /*
        This is taken from Field_string::unpack.
      */
      const CHARSET_INFO *cs = str->charset();
      uint bytes = (((metadata >> 4) & 0x300) ^ 0x300) + (metadata & 0x00ff);
      size_t length;
      if (field_cs)
        length =
            cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                               "char(%d)", bytes / field_cs->mbmaxlen);
      else
        length =
            cs->cset->snprintf(cs, (char *)str->ptr(), str->alloced_length(),
                               "char(%d(bytes))", bytes);
      str->length(length);
    } break;

    case MYSQL_TYPE_GEOMETRY:
      str->set_ascii(STRING_WITH_LEN("geometry"));
      break;

    case MYSQL_TYPE_JSON:
      str->set_ascii(STRING_WITH_LEN("json"));
      break;

    default:
      str->set_ascii(STRING_WITH_LEN("<unknown type>"));
  }
  DBUG_VOID_RETURN;
}
