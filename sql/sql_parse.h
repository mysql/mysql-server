/* Copyright (c) 2006, 2024, Oracle and/or its affiliates.

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

#ifndef SQL_PARSE_INCLUDED
#define SQL_PARSE_INCLUDED

#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_command.h"
#include "my_sqlcommand.h"
#include "mysql/strings/m_ctype.h"
#include "mysql_com.h"             // enum_server_command
#include "sql/handler.h"           // enum_schema_tables
#include "sql/system_variables.h"  // System_variables
#include "storage/perfschema/terminology_use_previous_enum.h"

struct mysql_rwlock_t;
template <typename T>
class SQL_I_List;

/**
  @addtogroup GROUP_PARSER
  @{
*/

class Comp_creator;
class Item;
class Object_creation_ctx;
class Parser_state;
class THD;
class Table_ident;
struct LEX;
struct LEX_USER;
struct ORDER;
struct Parse_context;
class Table_ref;
union COM_DATA;

extern "C" int test_if_data_home_dir(const char *dir);

bool stmt_causes_implicit_commit(const THD *thd, uint mask);

#ifndef NDEBUG
extern void turn_parser_debug_on();
#endif

bool parse_sql(THD *thd, Parser_state *parser_state,
               Object_creation_ctx *creation_ctx);

void free_items(Item *item);
void cleanup_items(Item *item);
void bind_fields(Item *first);

Comp_creator *comp_eq_creator(bool invert);
Comp_creator *comp_equal_creator(bool invert);
Comp_creator *comp_ge_creator(bool invert);
Comp_creator *comp_gt_creator(bool invert);
Comp_creator *comp_le_creator(bool invert);
Comp_creator *comp_lt_creator(bool invert);
Comp_creator *comp_ne_creator(bool invert);

int prepare_schema_table(THD *thd, LEX *lex, Table_ident *table_ident,
                         enum enum_schema_tables schema_table_idx);
void get_default_definer(THD *thd, LEX_USER *definer);
LEX_USER *create_default_definer(THD *thd);
LEX_USER *get_current_user(THD *thd, LEX_USER *user);
bool check_string_char_length(const LEX_CSTRING &str, const char *err_msg,
                              size_t max_char_length, const CHARSET_INFO *cs,
                              bool no_error);
bool merge_charset_and_collation(const CHARSET_INFO *charset,
                                 const CHARSET_INFO *collation,
                                 const CHARSET_INFO **to);
bool merge_sp_var_charset_and_collation(const CHARSET_INFO *charset,
                                        const CHARSET_INFO *collation,
                                        const CHARSET_INFO **to);
bool check_host_name(const LEX_CSTRING &str);
bool mysql_test_parse_for_slave(THD *thd);
bool is_update_query(enum enum_sql_command command);
bool is_explainable_query(enum enum_sql_command command);
bool is_log_table_write_query(enum enum_sql_command command);
bool alloc_query(THD *thd, const char *packet, size_t packet_length);
void dispatch_sql_command(THD *thd, Parser_state *parser_state);
void mysql_reset_thd_for_next_command(THD *thd);
void create_table_set_open_action_and_adjust_tables(LEX *lex);
int mysql_execute_command(THD *thd, bool first_level = false);
bool do_command(THD *thd);
bool dispatch_command(THD *thd, const COM_DATA *com_data,
                      enum enum_server_command command);
bool prepare_index_and_data_dir_path(THD *thd, const char **data_file_name,
                                     const char **index_file_name,
                                     const char *table_name);
int append_file_to_dir(THD *thd, const char **filename_ptr,
                       const char *table_name);
void execute_init_command(THD *thd, LEX_STRING *init_command,
                          mysql_rwlock_t *var_lock);
void add_to_list(SQL_I_List<ORDER> &list, ORDER *order);
void add_join_on(Table_ref *b, Item *expr);
bool push_new_name_resolution_context(Parse_context *pc, Table_ref *left_op,
                                      Table_ref *right_op);
void init_sql_command_flags(void);
const CHARSET_INFO *get_bin_collation(const CHARSET_INFO *cs);
void killall_non_super_threads(THD *thd);
bool shutdown(THD *thd, enum mysql_enum_shutdown_level level);
bool show_precheck(THD *thd, LEX *lex, bool lock);
void statement_id_to_session(THD *thd);

/* Variables */

extern uint sql_command_flags[];

/**
  Map from enumeration values of type enum_server_command to
  descriptions of type std::string.

  In this context, a "command" is a type code for a remote procedure
  call in the client-server protocol; for instance, a "connect" or a
  "ping" or a "query".

  The getter functions use @@terminology_use_previous to
  decide which version of the name to use, for names that depend on
  it.
*/
class Command_names {
 private:
  /**
    Array indexed by enum_server_command, where each element is a
    description string.
  */
  static const std::string m_names[];
  /**
    Command whose name depends on @@terminology_use_previous.

    Currently, there is only one such command, so we use a single
    member variable.  In case we ever change any other command name
    and control the use of the old or new name using
    @@terminology_use_previous, we need to change the
    following three members into some collection type, e.g.,
    std::unordered_set.
  */
  static constexpr enum_server_command m_replace_com{COM_REGISTER_SLAVE};
  /**
    Name to use when compatibility is enabled.
  */
  static const std::string m_replace_str;
  /**
    The version when the name was changed.
  */
  static constexpr terminology_use_previous::enum_compatibility_version
      m_replace_version{terminology_use_previous::BEFORE_8_0_26};
  /**
    Given a system_variable object, returns the string to use for
    m_replace_com, according to the setting of
    terminology_use_previous stored in the object.

    @param sysvars The System_variables object holding the
    configuration that should be considered when doing the translation.

    @return The instrumentation name that was in use in the configured
    version, for m_replace_com.
  */
  static const std::string &translate(const System_variables &sysvars);
  /**
    Cast an integer to enum_server_command, and assert it is in range.

    @param cmd The integer value
    @return The enum_server_command
  */
  static enum_server_command int_to_cmd(int cmd) {
    assert(cmd >= 0);
    assert(cmd <= COM_END);
    return static_cast<enum_server_command>(cmd);
  }

 public:
  /**
    Return a description string for a given enum_server_command.

    This bypasses @@terminology_use_previous and acts as if
    it was set to NONE.

    @param cmd The enum_server_command
    @retval The description string
  */
  static const std::string &str_notranslate(enum_server_command cmd) {
    return m_names[cmd];
  }
  /**
    Return a description string for an integer that is the numeric
    value of an enum_server_command.

    This bypasses @@terminology_use_previous and acts as if
    it was set to NONE.

    @param cmd The integer value
    @retval The description string
  */
  static const std::string &str_notranslate(int cmd) {
    return str_notranslate(int_to_cmd(cmd));
  }
  /**
    Return a description string for a given enum_server_command.

    This takes @@session.terminology_use_previous into
    account, and returns an old name if one has been defined and the
    option is enabled.

    @param cmd The enum_server_command
    @retval The description string
  */
  static const std::string &str_session(enum_server_command cmd);
  /**
    Return a description string for a given enum_server_command.

    This takes @@global.terminology_use_previous into
    account, and returns an old name if one has been defined and the
    option is enabled.

    @param cmd The enum_server_command
    @retval The description string
  */
  static const std::string &str_global(enum_server_command cmd);
  /**
    Return a description string for an integer that is the numeric
    value of an enum_server_command.

    This takes @@session.terminology_use_previous into
    account, and returns an old name if one has been defined and the
    option is enabled.

    @param cmd The integer value
    @retval The description string
  */
  static const std::string &str_session(int cmd) {
    return str_session(int_to_cmd(cmd));
  }
};

bool sqlcom_can_generate_row_events(enum enum_sql_command command);

/**
  @brief This function checks if the sql_command is one that identifies the
  boundaries (begin, end or savepoint) of a transaction.

  @note this is used for replication purposes.

  @param command The parsed SQL_COMM to check.
  @return true if this is either a BEGIN, COMMIT, SAVEPOINT, ROLLBACK,
  ROLLBACK_TO_SAVEPOINT.
  @return false any other SQL command.
 */
bool is_normal_transaction_boundary_stmt(enum enum_sql_command command);

/**
  @brief This function checks if the sql_command is one that identifies the
  boundaries (begin, end or savepoint) of an XA transaction. It does not
  consider PREPARE statements.

  @note this is used for replication purposes.

  @param command The parsed SQL_COMM to check.
  @return true if this is either a XA_START, XA_END, XA_COMMIT, XA_ROLLBACK.
  @return false any other SQL command.
 */
bool is_xa_transaction_boundary_stmt(enum enum_sql_command command);

bool all_tables_not_ok(THD *thd, Table_ref *tables);
bool some_non_temp_table_to_be_updated(THD *thd, Table_ref *tables);

// TODO: remove after refactoring of ALTER DATABASE:
bool set_default_charset(HA_CREATE_INFO *create_info,
                         const CHARSET_INFO *value);
// TODO: remove after refactoring of ALTER DATABASE:
bool set_default_collation(HA_CREATE_INFO *create_info,
                           const CHARSET_INFO *value);

/* Bits in sql_command_flags */

#define CF_CHANGES_DATA (1U << 0)
/* The 2nd bit is unused -- it used to be CF_HAS_ROW_COUNT. */
#define CF_STATUS_COMMAND (1U << 2)
#define CF_SHOW_TABLE_COMMAND (1U << 3)
#define CF_WRITE_LOGS_COMMAND (1U << 4)
/**
  Must be set for SQL statements that may contain
  Item expressions and/or use joins and tables.
  Indicates that the parse tree of such statement may
  contain rule-based optimizations that depend on metadata
  (i.e. number of columns in a table), and consequently
  that the statement must be re-prepared whenever
  referenced metadata changes. Must not be set for
  statements that themselves change metadata, e.g. RENAME,
  ALTER and other DDL, since otherwise will trigger constant
  reprepare. Consequently, complex item expressions and
  joins are currently prohibited in these statements.
*/
#define CF_REEXECUTION_FRAGILE (1U << 5)
/**
  Implicitly commit before the SQL statement is executed.

  Statements marked with this flag will cause any active
  transaction to end (commit) before proceeding with the
  command execution.

  This flag should be set for statements that probably can't
  be rolled back or that do not expect any previously metadata
  locked tables.
*/
#define CF_IMPLICIT_COMMIT_BEGIN (1U << 6)
/**
  Implicitly commit after the SQL statement.

  Statements marked with this flag are automatically committed
  at the end of the statement.

  This flag should be set for statements that will implicitly
  open and take metadata locks on system tables that should not
  be carried for the whole duration of a active transaction.
*/
#define CF_IMPLICIT_COMMIT_END (1U << 7)
/**
  CF_IMPLICIT_COMMIT_BEGIN and CF_IMPLICIT_COMMIT_END are used
  to ensure that the active transaction is implicitly committed
  before and after every DDL statement and any statement that
  modifies our currently non-transactional system tables.
*/
#define CF_AUTO_COMMIT_TRANS (CF_IMPLICIT_COMMIT_BEGIN | CF_IMPLICIT_COMMIT_END)

/**
  Diagnostic statement.
  Diagnostic statements:
  - SHOW WARNING
  - SHOW ERROR
  - GET DIAGNOSTICS (WL#2111)
  do not modify the Diagnostics Area during execution.
*/
#define CF_DIAGNOSTIC_STMT (1U << 8)

/**
  Identifies statements that may generate row events
  and that may end up in the binary log.
*/
#define CF_CAN_GENERATE_ROW_EVENTS (1U << 9)

/**
  Identifies statements which may deal with temporary tables and for which
  temporary tables should be pre-opened to simplify privilege checks.
*/
#define CF_PREOPEN_TMP_TABLES (1U << 10)

/**
  Identifies statements for which open handlers should be closed in the
  beginning of the statement.
*/
#define CF_HA_CLOSE (1U << 11)

/**
  Identifies statements that can be explained with EXPLAIN.
*/
#define CF_CAN_BE_EXPLAINED (1U << 12)

/** Identifies statements which may generate an optimizer trace */
#define CF_OPTIMIZER_TRACE (1U << 14)

/**
   Identifies statements that should always be disallowed in
   read only transactions.
*/
#define CF_DISALLOW_IN_RO_TRANS (1U << 15)

/**
  Identifies statements and commands that can be used with Protocol Plugin
*/
#define CF_ALLOW_PROTOCOL_PLUGIN (1U << 16)

/**
  Identifies statements (typically DDL) which needs auto-commit mode
  temporarily turned off.

  @note This is necessary to prevent InnoDB from automatically committing
        InnoDB transaction each time data-dictionary tables are closed
        after being updated.

  @note This is also necessary for ACL DDL, so the code which
        saves GTID state or slave state in the system tables at the
        commit time works correctly. This code does statement commit
        on low-level (see System_table_access:: close_table()) and
        thus can pre-maturely commit DDL if @@autocommit=1.
*/
#define CF_NEEDS_AUTOCOMMIT_OFF (1U << 17)

/**
  Identifies statements which can return rows of data columns (SELECT, SHOW ...)
*/
#define CF_HAS_RESULT_SET (1U << 18)

/**
  Identifies DDL statements which can be atomic.
  Having the bit ON does not yet define an atomic.
  The property is used both on the master and slave.
  On the master atomicity infers the binlog and gtid_executed system table.
  On the slave it more involves the slave info table.

  @note At the momemnt of declaration the covered DDL subset coincides
        with the of CF_NEEDS_AUTOCOMMIT_OFF.
*/
#define CF_POTENTIAL_ATOMIC_DDL (1U << 19)

/**
  Statement is depending on the ACL cache, which can be disabled by the
  --skip-grant-tables server option.
*/
#define CF_REQUIRE_ACL_CACHE (1U << 20)

/**
  Identifies statements as SHOW commands using INFORMATION_SCHEMA system views.
*/
#define CF_SHOW_USES_SYSTEM_VIEW (1U << 21)

/* Bits in server_command_flags */

/**
  Skip the increase of the global query id counter. Commonly set for
  commands that are stateless (won't cause any change on the server
  internal states). This is made obsolete as query id is incremented
  for ping and statistics commands as well because of race condition
  (Bug#58785).
*/
#define CF_SKIP_QUERY_ID (1U << 0)

/**
  Skip the increase of the number of statements that clients have
  sent to the server. Commonly used for commands that will cause
  a statement to be executed but the statement might have not been
  sent by the user (ie: stored procedure).
*/
#define CF_SKIP_QUESTIONS (1U << 1)

/**
  1U << 16 is reserved for Protocol Plugin statements and commands
*/

/**
  @} (end of group GROUP_PARSER)
*/

#endif /* SQL_PARSE_INCLUDED */
