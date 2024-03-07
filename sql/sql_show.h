/* Copyright (c) 2005, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_SHOW_H
#define SQL_SHOW_H

#include <stddef.h>
#include <sys/types.h>

#include "field_types.h"
#include "lex_string.h"
#include "my_inttypes.h"
#include "mysql/status_var.h"
#include "sql/sql_select.h"
#include "typelib.h"

/* Forward declarations */
class Item;
class JOIN;
class QEP_TAB;
class Query_block;
class String;
class Table_ident;
class THD;
class sp_name;
struct CHARSET_INFO;
struct HA_CREATE_INFO;
struct LEX;
struct ST_SCHEMA_TABLE;
struct System_status_var;
struct TABLE;
class Table_ref;
typedef enum enum_mysql_show_type SHOW_TYPE;
enum enum_schema_tables : int;
enum enum_var_type : int;

/** Characters shown for the command in 'show processlist'. */
constexpr const size_t PROCESS_LIST_WIDTH{100};
/* Characters shown for the command in 'information_schema.processlist' */
constexpr const size_t PROCESS_LIST_INFO_WIDTH{65535};

bool store_create_info(THD *thd, Table_ref *table_list, String *packet,
                       HA_CREATE_INFO *create_info_arg, bool show_database,
                       bool for_show_create_stmt);

void append_identifier(const THD *thd, String *packet, const char *name,
                       size_t length, const CHARSET_INFO *from_cs,
                       const CHARSET_INFO *to_cs);

void append_identifier(const THD *thd, String *packet, const char *name,
                       size_t length);

void mysqld_list_fields(THD *thd, Table_ref *table, const char *wild);
bool mysqld_show_create(THD *thd, Table_ref *table_list);
bool mysqld_show_create_db(THD *thd, char *dbname, HA_CREATE_INFO *create);

void mysqld_list_processes(THD *thd, const char *user, bool verbose,
                           bool has_cursor);
bool mysqld_show_privileges(THD *thd);
void calc_sum_of_all_status(System_status_var *to);
void append_definer(const THD *thd, String *buffer,
                    const LEX_CSTRING &definer_user,
                    const LEX_CSTRING &definer_host);
bool add_status_vars(const SHOW_VAR *list);
void remove_status_vars(SHOW_VAR *list);
void init_status_vars();
void free_status_vars();
bool get_recursive_status_var(THD *thd, const char *name, char *const value,
                              enum_var_type var_type, size_t *length,
                              const CHARSET_INFO **charset);
void reset_status_vars();
ulonglong get_status_vars_version(void);
bool show_create_trigger(THD *thd, const sp_name *trg_name);
void view_store_options(const THD *thd, Table_ref *table, String *buff);

bool schema_table_store_record(THD *thd, TABLE *table);

/**
  Store record to I_S table, convert HEAP table to InnoDB table if necessary.

  @param[in]  thd            thread handler
  @param[in]  table          Information schema table to be updated
  @param[in]  make_ondisk    if true, convert heap table to on disk table.
                             default value is true.
  @return 0 on success
  @return error code on failure.
*/
int schema_table_store_record2(THD *thd, TABLE *table, bool make_ondisk);

/**
  Convert HEAP table to InnoDB table if necessary

  @param[in] thd     thread handler
  @param[in] table   Information schema table to be converted.
  @param[in] error   the error code returned previously.
  @return false on success, true on error.
*/
bool convert_heap_table_to_ondisk(THD *thd, TABLE *table, int error);
void initialize_information_schema_acl();
bool make_table_list(THD *thd, Query_block *sel, const LEX_CSTRING &db_name,
                     const LEX_CSTRING &table_name);

ST_SCHEMA_TABLE *find_schema_table(THD *thd, const char *table_name);
ST_SCHEMA_TABLE *get_schema_table(enum enum_schema_tables schema_table_idx);
bool make_schema_query_block(THD *thd, Query_block *sel,
                             enum enum_schema_tables schema_table_idx);
bool mysql_schema_table(THD *thd, LEX *lex, Table_ref *table_list);
enum enum_schema_tables get_schema_table_idx(ST_SCHEMA_TABLE *schema_table);

const char *get_one_variable(THD *thd, const SHOW_VAR *variable,
                             enum_var_type value_type, SHOW_TYPE show_type,
                             System_status_var *status_var,
                             const CHARSET_INFO **charset, char *buff,
                             size_t *length, bool *is_null = nullptr);

const char *get_one_variable_ext(THD *running_thd, THD *target_thd,
                                 const SHOW_VAR *variable,
                                 enum_var_type value_type, SHOW_TYPE show_type,
                                 System_status_var *status_var,
                                 const CHARSET_INFO **charset, char *buff,
                                 size_t *length, bool *is_null = nullptr);

/* These functions were under INNODB_COMPATIBILITY_HOOKS */
int get_quote_char_for_identifier(const THD *thd, const char *name,
                                  size_t length);

void show_sql_type(enum_field_types type, bool is_array, uint metadata,
                   String *str, const CHARSET_INFO *field_cs = nullptr);

bool do_fill_information_schema_table(THD *thd, Table_ref *table_list,
                                      Item *condition);

extern std::atomic_ulong deprecated_use_i_s_processlist_count;
extern std::atomic_ullong deprecated_use_i_s_processlist_last_timestamp;
extern TYPELIB grant_types;

/**
  Sql_cmd_show represents the SHOW statements that are implemented
  as SELECT statements internally.
  Normally, preparation and execution is the same as for regular SELECT
  statements.
*/
class Sql_cmd_show : public Sql_cmd_select {
 public:
  Sql_cmd_show(enum_sql_command sql_command)
      : Sql_cmd_select(nullptr), m_sql_command(sql_command) {}
  enum_sql_command sql_command_code() const override { return m_sql_command; }
  virtual bool check_parameters(THD *) { return false; }
  /// Generally, the SHOW commands do not distinguish precheck and regular check
  bool precheck(THD *thd) override { return check_privileges(thd); }
  bool check_privileges(THD *) override;
  bool execute(THD *thd) override;

 protected:
  enum_sql_command m_sql_command;
};

/**
  Common base class: Represents commands that are not represented by
  a plan that is equivalent to a SELECT statement.

  This class has a common execution framework with an execute() function
  that calls check_privileges() and execute_inner().
*/
class Sql_cmd_show_noplan : public Sql_cmd_show {
 protected:
  Sql_cmd_show_noplan(enum_sql_command sql_command)
      : Sql_cmd_show(sql_command) {}
  bool execute(THD *thd) override {
    lex = thd->lex;
    if (check_privileges(thd)) return true;
    if (execute_inner(thd)) return true;
    return false;
  }
};

/// Common base class: Represents commands that operate on a schema (database)

class Sql_cmd_show_schema_base : public Sql_cmd_show {
 public:
  Sql_cmd_show_schema_base(enum_sql_command command) : Sql_cmd_show(command) {}
  bool check_privileges(THD *thd) override;
  bool check_parameters(THD *thd) override;
  bool set_metadata_lock(THD *thd);
};

/// Common base class: Represents the SHOW COLUMNS and SHOW KEYS statements.

class Sql_cmd_show_table_base : public Sql_cmd_show {
 public:
  Sql_cmd_show_table_base(enum_sql_command command) : Sql_cmd_show(command) {}
  bool check_privileges(THD *thd) override;
  bool check_parameters(THD *thd) override;

  bool m_temporary;  ///< True if table to be analyzed is temporary
};

/// Represents SHOW FUNCTION CODE and SHOW PROCEDURE CODE statements.

class Sql_cmd_show_routine_code final : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_routine_code(enum_sql_command sql_command,
                            const sp_name *routine_name)
      : Sql_cmd_show_noplan(sql_command), m_routine_name(routine_name) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

 private:
  const sp_name *m_routine_name;
};

/// Following are all subclasses of class Sql_cmd_show, in alphabetical order

/// Represents SHOW BINLOG EVENTS statement.

class Sql_cmd_show_binlog_events : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_binlog_events()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_BINLOG_EVENTS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

 private:
  // binlog_in
  // binlog_from
  // opt_limit
};

/// Represents SHOW BINARY LOGS statement.

class Sql_cmd_show_binlogs : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_binlogs() : Sql_cmd_show_noplan(SQLCOM_SHOW_BINLOGS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CHARACTER SET statement.

class Sql_cmd_show_charsets : public Sql_cmd_show {
 public:
  Sql_cmd_show_charsets() : Sql_cmd_show(SQLCOM_SHOW_CHARSETS) {}
};

/// Represents SHOW COLLATION statement.

class Sql_cmd_show_collations : public Sql_cmd_show {
 public:
  Sql_cmd_show_collations() : Sql_cmd_show(SQLCOM_SHOW_COLLATIONS) {}
};

/// Represents SHOW COLUMNS statement.

class Sql_cmd_show_columns : public Sql_cmd_show_table_base {
 public:
  Sql_cmd_show_columns() : Sql_cmd_show_table_base(SQLCOM_SHOW_FIELDS) {}
};

/// Represents SHOW CREATE DATABASE statement.

class Sql_cmd_show_create_database : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_database() : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE_DB) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CREATE EVENT statement.

class Sql_cmd_show_create_event : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_event() : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE_EVENT) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CREATE FUNCTION statement.

class Sql_cmd_show_create_function : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_function()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE_FUNC) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CREATE PROCEDURE statement.

class Sql_cmd_show_create_procedure : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_procedure()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE_PROC) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CREATE TABLE/VIEW statement.

class Sql_cmd_show_create_table : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_table(bool is_view, Table_ident *table_ident)
      : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE),
        m_is_view(is_view),
        m_table_ident(table_ident) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

 private:
  const bool m_is_view;
  Table_ident *const m_table_ident;
};

/// Represents SHOW CREATE TRIGGER statement.

class Sql_cmd_show_create_trigger : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_create_trigger()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_CREATE_TRIGGER) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW CREATE USER statement.

class Sql_cmd_show_create_user : public Sql_cmd_show {
 public:
  Sql_cmd_show_create_user() : Sql_cmd_show(SQLCOM_SHOW_CREATE_USER) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW DATABASES statement.

class Sql_cmd_show_databases : public Sql_cmd_show {
 public:
  Sql_cmd_show_databases() : Sql_cmd_show(SQLCOM_SHOW_DATABASES) {}
  bool check_privileges(THD *thd) override;
};

/// Represents SHOW ENGINE LOGS statement.

class Sql_cmd_show_engine_logs : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_engine_logs() : Sql_cmd_show_noplan(SQLCOM_SHOW_ENGINE_LOGS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW ENGINE MUTEX statement.

class Sql_cmd_show_engine_mutex : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_engine_mutex() : Sql_cmd_show_noplan(SQLCOM_SHOW_ENGINE_MUTEX) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW ENGINE STATUS statement.

class Sql_cmd_show_engine_status : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_engine_status()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_ENGINE_STATUS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW STORAGE ENGINES statement.

class Sql_cmd_show_engines : public Sql_cmd_show {
 public:
  Sql_cmd_show_engines() : Sql_cmd_show(SQLCOM_SHOW_STORAGE_ENGINES) {}
};

/// Represents SHOW ERRORS statement.

class Sql_cmd_show_errors : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_errors() : Sql_cmd_show_noplan(SQLCOM_SHOW_ERRORS) {}
  bool execute_inner(THD *thd) override {
    return mysqld_show_warnings(thd, 1UL << (uint)Sql_condition::SL_ERROR);
  }
};

/// Represents SHOW EVENTS statement.

class Sql_cmd_show_events : public Sql_cmd_show_schema_base {
 public:
  Sql_cmd_show_events() : Sql_cmd_show_schema_base(SQLCOM_SHOW_EVENTS) {}
  bool check_privileges(THD *thd) override;
  // To enable error message for unknown database, delete the below function.
  bool check_parameters(THD *) override { return false; }
};

/// Represents SHOW GRANTS statement.

class Sql_cmd_show_grants : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_grants(const LEX_USER *for_user_arg,
                      const List<LEX_USER> *using_users_arg)
      : Sql_cmd_show_noplan(SQLCOM_SHOW_GRANTS),
        for_user(for_user_arg),
        using_users(using_users_arg) {}

  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

 private:
  const LEX_USER *for_user;
  const List<LEX_USER> *using_users;
};

/// Represents the SHOW INDEX statement.

class Sql_cmd_show_keys : public Sql_cmd_show_table_base {
 public:
  Sql_cmd_show_keys() : Sql_cmd_show_table_base(SQLCOM_SHOW_KEYS) {}
};

/// Represents SHOW BINARY LOG STATUS statement.

class Sql_cmd_show_binary_log_status : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_binary_log_status()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_BINLOG_STATUS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW OPEN TABLES statement.

class Sql_cmd_show_open_tables : public Sql_cmd_show {
 public:
  Sql_cmd_show_open_tables() : Sql_cmd_show(SQLCOM_SHOW_OPEN_TABLES) {}
};

/// Represents SHOW PLUGINS statement.

class Sql_cmd_show_plugins : public Sql_cmd_show {
 public:
  Sql_cmd_show_plugins() : Sql_cmd_show(SQLCOM_SHOW_PLUGINS) {}
};

/// Represents SHOW PRIVILEGES statement.

class Sql_cmd_show_privileges : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_privileges() : Sql_cmd_show_noplan(SQLCOM_SHOW_PRIVILEGES) {}
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW PROCESSLIST statement.

class Sql_cmd_show_processlist : public Sql_cmd_show {
 public:
  Sql_cmd_show_processlist() : Sql_cmd_show(SQLCOM_SHOW_PROCESSLIST) {}
  explicit Sql_cmd_show_processlist(bool verbose)
      : Sql_cmd_show(SQLCOM_SHOW_PROCESSLIST), m_verbose(verbose) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

  void set_use_pfs(bool use_pfs) { m_use_pfs = use_pfs; }
  bool verbose() const { return m_verbose; }

 private:
  bool use_pfs() { return m_use_pfs; }

  const bool m_verbose{false};
  bool m_use_pfs{false};
};

/// Represents SHOW PARSE_TREE statement.

class Sql_cmd_show_parse_tree : public Sql_cmd_show {
 public:
  Sql_cmd_show_parse_tree() : Sql_cmd_show(SQLCOM_SHOW_PARSE_TREE) {}
};

/// Represents SHOW PROFILE statement.

class Sql_cmd_show_profile : public Sql_cmd_show {
 public:
  Sql_cmd_show_profile() : Sql_cmd_show(SQLCOM_SHOW_PROFILE) {}
};

/// Represents SHOW PROFILES statement.

class Sql_cmd_show_profiles : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_profiles() : Sql_cmd_show_noplan(SQLCOM_SHOW_PROFILES) {}
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW RELAYLOG EVENTS statement.

class Sql_cmd_show_relaylog_events : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_relaylog_events()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_RELAYLOG_EVENTS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;

 private:
  // binlog_in
  // binlog_from
  // opt_limit
};

/// Represents SHOW REPLICAS statement.

class Sql_cmd_show_replicas : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_replicas() : Sql_cmd_show_noplan(SQLCOM_SHOW_REPLICAS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW REPLICA STATUS statement.

class Sql_cmd_show_replica_status : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_replica_status()
      : Sql_cmd_show_noplan(SQLCOM_SHOW_REPLICA_STATUS) {}
  bool check_privileges(THD *thd) override;
  bool execute_inner(THD *thd) override;
};

/// Represents SHOW STATUS statement.

class Sql_cmd_show_status : public Sql_cmd_show {
 public:
  Sql_cmd_show_status() : Sql_cmd_show(SQLCOM_SHOW_STATUS) {}
  bool execute(THD *thd) override;
};

/// Represents SHOW STATUS FUNCTION statement.

class Sql_cmd_show_status_func : public Sql_cmd_show {
 public:
  Sql_cmd_show_status_func() : Sql_cmd_show(SQLCOM_SHOW_STATUS_FUNC) {}
};

/// Represents SHOW STATUS PROCEDURE statement.

class Sql_cmd_show_status_proc : public Sql_cmd_show {
 public:
  Sql_cmd_show_status_proc() : Sql_cmd_show(SQLCOM_SHOW_STATUS_PROC) {}
};

/// Represents SHOW TABLE STATUS statement.

class Sql_cmd_show_table_status : public Sql_cmd_show_schema_base {
 public:
  Sql_cmd_show_table_status()
      : Sql_cmd_show_schema_base(SQLCOM_SHOW_TABLE_STATUS) {}
};

/// Represents SHOW TABLES statement.

class Sql_cmd_show_tables : public Sql_cmd_show_schema_base {
 public:
  Sql_cmd_show_tables() : Sql_cmd_show_schema_base(SQLCOM_SHOW_TABLES) {}
};

/// Represents SHOW TRIGGERS statement.

class Sql_cmd_show_triggers : public Sql_cmd_show_schema_base {
 public:
  Sql_cmd_show_triggers() : Sql_cmd_show_schema_base(SQLCOM_SHOW_TRIGGERS) {}
};

/// Represents SHOW VARIABLES statement.

class Sql_cmd_show_variables : public Sql_cmd_show {
 public:
  Sql_cmd_show_variables() : Sql_cmd_show(SQLCOM_SHOW_VARIABLES) {}
};

/// Represents SHOW WARNINGS statement.

class Sql_cmd_show_warnings : public Sql_cmd_show_noplan {
 public:
  Sql_cmd_show_warnings() : Sql_cmd_show_noplan(SQLCOM_SHOW_WARNS) {}
  bool execute_inner(THD *thd) override {
    return mysqld_show_warnings(thd,
                                (1UL << (uint)Sql_condition::SL_NOTE) |
                                    (1UL << (uint)Sql_condition::SL_WARNING) |
                                    (1UL << (uint)Sql_condition::SL_ERROR));
  }
};

#endif /* SQL_SHOW_H */
