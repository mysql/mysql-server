/* Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef RUN_COMMAND_H
#define RUN_COMMAND_H

#include <cstddef>
#include <forward_list>
#include <new>
#include <string>
#include <string_view>

#include "lex_string.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "mysql_time.h"
#include "sql/current_thd.h"
#include "sql/sql_class.h"
#include "sql/sql_cursor.h"
#include "sql/sql_prepare.h"  // Prepared_statement
#include "sql/statement/protocol_local_v2.h"
#include "sql/statement/statement_runnable.h"
#include "utils.h"

/**
  There must be one function of this kind in order for the symbols in the
  server's dynamic library to be visible to components.
*/
int dummy_function_to_ensure_we_are_linked_into_the_server();

/// Max limit for Regular Statement Handles in use.
static constexpr unsigned short MAX_REGULAR_STATEMENT_HANDLES_LIMIT{1024};

/**
  Number of PSI_statement_info instruments for Statement handles.
*/
#define STMT_HANDLE_PSI_STATEMENT_INFO_COUNT 6

#ifdef HAVE_PSI_INTERFACE
/**
  Initializes Statement Handles PFS statement instrumentation information
  instances.
*/
void init_statement_handle_interface_psi_keys();
#endif

/**
 * @brief Statement_handle is similar to Ed_connection. Some of the
 * limitations in Ed_connection is that,
 * - It does not support reading result metadata
 * - It does not support prepared statement, parameters and cursors.
 *
 * Above limitation leads to implementation of this interface.
 *
 * Statement_handle supports both execution of regular and
 * prepared statement.
 *
 * Note that we can get rid of Ed_connection implementation
 * as we support more functionality with Statement_handle.
 */
class Statement_handle {
 public:
  Statement_handle(THD *thd, const char *query, size_t length);

  /**
   * @brief Check if error is reported.
   *
   * @return true of error is reported.
   */
  bool is_error() const { return m_diagnostics_area->is_error(); }

  /**
   * @brief Check if error is reported.
   *
   * @return true of error is reported.
   */
  LEX_CSTRING get_last_error() {
    assert(is_error());
    return convert_and_store(&m_warning_mem_root,
                             m_diagnostics_area->message_text(),
                             strlen(m_diagnostics_area->message_text()),
                             system_charset_info, m_expected_charset);
  }

  /**
   * @brief Get the last mysql errno.
   *
   * @return unsigned int
   */
  unsigned int get_last_errno() const {
    assert(is_error());
    return m_diagnostics_area->mysql_errno();
  }

  /**
   * @brief Get the mysql error state.
   *
   * @return const char*
   */
  LEX_CSTRING get_mysql_state() {
    assert(is_error());
    return convert_and_store(&m_warning_mem_root,
                             m_diagnostics_area->returned_sqlstate(),
                             strlen(m_diagnostics_area->returned_sqlstate()),
                             system_charset_info, m_expected_charset);
  }

  /**
   * @brief Get number of warnings generated.
   *
   * @return ulonglong
   */
  ulonglong warning_count() const { return m_warnings_count; }

  /**
   * @brief Get list of all warnings.
   *
   * @return Warning*
   */
  Warning *get_warnings();

  /**
   * @brief Get the next result sets generated while executing the statement.
   *
   * @return Result_set*
   */
  Result_set *get_result_sets() { return m_result_sets; }

  /**
   * @brief Get the current result set object
   *
   * @return Result_set*
   */
  Result_set *get_current_result_set() { return m_current_rset; }

  /**
   * @brief Make the next result set the current result set.
   * We do it once we have read the current result set.
   *
   */
  void next_result_set() {
    auto next_rset = m_current_rset->get_next();
    m_current_rset = next_rset;
  }

  /**
   * @brief Execute the SQL command.
   * This can be either Regular or Prepared statement.
   *
   * @return true upon failure.
   * @return false upon success.
   */
  virtual bool execute() = 0;

  /**
   * @brief Check if we are processing a prepared statement.
   *
   * @return true upon failure.
   * @return false upon success.
   */
  virtual bool is_prepared_statement() = 0;

  /**
   * @brief Check if the statement has been executed or prepared
   *
   * @return true upon executed or prepared
   * @return false otherwise
   */
  virtual bool is_executed_or_prepared() = 0;

  /**
   * @brief Feel all the result collected so far, from query execution.
   *
   */
  void free_old_result();

  /**
   * @brief Get the query string being executed.
   *
   * @return std::string_view
   */
  std::string_view get_query() { return m_query; }

  /**
   * @brief Set the capacity in bytes allowed for caching results.
   *
   * @param capacity of the result set
   */
  void set_capacity(size_t capacity) {
    m_protocol.set_result_set_capacity(capacity);
  }

  /**
   * @brief Get the capacity in bytes allowed for caching results.
   *
   * @return size_t
   */
  size_t get_capacity() { return m_protocol.get_result_set_capacity(); }

  /**
   * @brief Set thd protocol to enable result pass through.
   *
   * @param use_thd_protocol - parameter that decides if THD protocol should be
   * used
   */
  void set_use_thd_protocol(bool use_thd_protocol) {
    m_use_thd_protocol = use_thd_protocol;
  }

  /**
   * @brief Check if thd protocol is used.
   *
   * @return true if enabled.
   * @return false if not.
   */
  bool is_using_thd_protocol() const { return m_use_thd_protocol; }

  /**
   * @brief Set either Protocol_local_v2 when m_use_thd_protocol is not set or
   *        or classical protocol when m_use_thd_protocol is set to THD.
   */
  void set_thd_protocol();

  /**
   * @brief Reset THD protocol.
   */
  void reset_thd_protocol();

  /**
   * @brief Set the expected charset
   *
   * @param charset_name Name of the charset
   */
  void set_expected_charset(const char *charset_name) {
    m_expected_charset =
        get_charset_by_csname(charset_name, MY_CS_PRIMARY, MYF(0));
  }

  /**
   * @brief Get the expected charset
   *
   * @return const char* the expected charset name
   */
  const char *get_expected_charset() {
    if (m_expected_charset == nullptr) return nullptr;
    return m_expected_charset->csname;
  }

  /**
   * @brief Get the num rows per fetch.
   *
   * @return size_t
   */
  size_t get_num_rows_per_fetch() { return m_num_rows_per_fetch; }

  /**
   * @brief Set the num of rows to be retrieved per fetch.
   *
   * @param num_rows_per_fetch number of rows per fetch
   */
  void set_num_rows_per_fetch(size_t num_rows_per_fetch) {
    m_num_rows_per_fetch = num_rows_per_fetch;
  }

  /**
   * @brief Destroy the Statement_handle object
   *
   */
  virtual ~Statement_handle() { free_old_result(); }

 protected:
  /**
   * @brief Send statement execution status after execute().
   */
  void send_statement_status();

 protected:
  // The query being executed.
  std::string m_query;

  // Details about error or warning occured.
  MEM_ROOT m_warning_mem_root;
  Warning *m_warnings;
  ulonglong m_warnings_count = 0;
  Diagnostics_area *m_diagnostics_area;

  // The thread executing the statement.
  THD *m_thd;

  // List of all results generated by the query.
  Result_set *m_result_sets;

  // The last result generated by the guery.
  Result_set *m_current_rset;

  // Do not intercept the results in the protocol, but pass it to
  // THD* protocol. E.g., if this is set to 'false', the results are
  // captured into Protocol_result_v2, else it goes to THD->get_protocol()
  bool m_use_thd_protocol = false;

  // Max rows to read per fetch() call.
  size_t m_num_rows_per_fetch = 1;

  CHARSET_INFO *m_expected_charset;

  // Collect result set from single statement.
  void add_result_set(Result_set *result_set);

  // Set result set from prepared statement with cursor.
  // This is called at the end of fetch().
  void set_result_set(Result_set *result_set);

  /**
   * @brief Copy the warnings generated for the query from the
   * diagnostics area
   */
  void copy_warnings();

  // Protocol that intercepts all the rows from query executed.
  Protocol_local_v2 m_protocol;
  friend class Protocol_local_v2;

  // Protocol used by THD before switching.
  Protocol *m_saved_protocol{nullptr};

  // No copies.
  Statement_handle(const Statement_handle &) = delete;
  // No move.
  Statement_handle(Statement_handle &&) = delete;
  // No self assignment.
  Statement_handle &operator=(const Statement_handle &) = delete;
  // No move.
  Statement_handle &operator=(Statement_handle &&) = delete;
};

/**
 * @brief Regular_statement_handle enables execution of all SQL statements
 * except for prepared statements.
 */
class Regular_statement_handle : public Statement_handle {
 public:
  Regular_statement_handle(THD *thd, const char *query, uint length)
      : Statement_handle(thd, query, length) {}

  ~Regular_statement_handle() override {
    if (m_is_executed) m_thd->m_regular_statement_handle_count--;
  }

  /**
   * @brief Execute a regular statement.
   *
   * @return true if statement fails.
   * @return false if statement succeeds.
   */
  bool execute() override;

  /**
   * @brief Convey that this is regular statement.
   *
   * @return Always returns 'false'
   */
  bool is_prepared_statement() override { return false; }

  /**
   * @brief Check if the statement has been executed
   *
   * @return true if executed
   * @return false otherwise
   */
  bool is_executed_or_prepared() override { return m_is_executed; }

 public:
#ifdef HAVE_PSI_INTERFACE
  // PSI_statement_info instances for regular statement handle.
  static PSI_statement_info stmt_psi_info;
#endif

 private:
  /// Flag to indicate if statement has been executed. Set to true in execute().
  bool m_is_executed = false;

  // Used by execute() above.
  bool execute(Server_runnable *server_runnable);
};

/**
 * @brief Prepared_statement_handle enables support for prepared
 * statement execution. Supports parameters and cursors.
 */
class Prepared_statement_handle : public Statement_handle {
 public:
  Prepared_statement_handle(THD *thd, const char *query, uint length)
      : Statement_handle(thd, query, length),
        m_parameter_mem_root(key_memory_prepared_statement_main_mem_root,
                             thd->variables.query_alloc_block_size) {}

  /**
   * @brief Prepares the statement using m_query.
   *
   * If the statement is already in executing mode, close the cursor
   * deallocate the previous statement and start preparing new statement
   * using m_query.
   *
   * @return true if fails.
   * @return false if succeeds.
   */
  bool prepare();

  /**
   * @brief Set the parameter value in a prepared statement.
   *
   * @param idx            Index of '?' in prepared statement.
   * @param is_null        Set parameter to NULL value.
   * @param type           Set the parameters field type.
   * @param is_unsigned    Mark parameter as unsigned.
   * @param data           Pointer to buffer containing the value.
   * @param data_length    Length of buffer 'data'
   * @param name           Name of parameter (mostly unused)
   * @param name_length    Length of 'name'
   *
   * @return true if fails.
   * @return false if succeeds.
   */
  bool set_parameter(uint idx, bool is_null, enum_field_types type,
                     bool is_unsigned, const void *data,
                     unsigned long data_length, const char *name,
                     unsigned long name_length);

  /**
   * @brief Get the parameter object
   *
   * @param index of the parameter
   * @return Item_param*
   */
  Item_param *get_parameter(size_t index);

  /**
   * @brief Execute the statement that is prepared.
   *
   * If a statement is not yet prepared, we fail.
   *
   * If a statement was already in EXECUTED state, we close the cursor
   * and execute the statement again.
   *
   * @return true
   * @return false
   */
  bool execute() override;

  /**
   * @brief Fetch rows from statement using cursor.
   *
   * Not all statement uses cursor. Check is_cursor_open() and
   * then invoke this call.
   *
   * Attempt to call fetch rows without preparing or executing the
   * statement will cause failure.
   *
   * Attempt to call fetch() without cursor in use, will cause failure.
   *
   * @return true
   * @return false
   */
  bool fetch();

  /**
   * @brief Check if the statement uses cursor and it is open.
   *
   * If the API is called without preparing the statement will
   * result in 'false'
   *
   * @return true if cursor is in use
   * @return false if cursor not in use.
   */
  bool is_cursor_open() {
    if (!m_stmt) return false;
    return m_stmt->m_cursor && m_stmt->m_cursor->is_open();
  }

  /**
   * @brief Check if the statement uses cursor.
   *
   * If the API is called without executing the statement
   * will result in 'false'
   *
   * @return true if statement uses cursor.
   * @return false if statement does not uses cursor.
   */
  bool uses_cursor() {
    if (!m_stmt || m_stmt->m_arena.get_state() != Query_arena::STMT_EXECUTED)
      return false;
    return m_stmt->m_cursor != nullptr;
  }

  /**
   * @brief Reset the statement parameters and cursors.
   *
   * Invoking this API will close the cursor in use.
   * This is invoked generally before executing
   * the statement for the second time, after prepare.
   *
   * @return true
   * @return false
   */
  bool reset();

  /**
   * @brief Close the statement that is prepared.
   *
   * @return true
   * @return false
   */
  bool close();

  /**
   * @brief Get number of parameters used in the statement.
   *
   * This should be called after preparing a statement.
   *
   * @return uint
   */
  uint get_param_count() { return m_stmt ? m_stmt->m_param_count : 0; }

  /**
   * @brief Convey that this is prepared statement.
   *
   * @return Always returns true
   */
  bool is_prepared_statement() override { return true; }

  /**
   * @brief Check if the statement has been executed or prepared
   *
   * @return true if executed or prepared
   * @return false otherwise
   */
  bool is_executed_or_prepared() override {
    return m_stmt != nullptr &&
           m_stmt->m_arena.get_state() > Query_arena::STMT_INITIALIZED;
  }

  /**
   * @brief Virtual destroy for Prepared_statement_handle object
   *
   */
  virtual ~Prepared_statement_handle() override { internal_close(); }

#ifdef HAVE_PSI_INTERFACE
  // PSI_statement_info instances for prepared statement handle.
  static PSI_statement_info prepare_psi_info;
  static PSI_statement_info execute_psi_info;
  static PSI_statement_info fetch_psi_info;
  static PSI_statement_info reset_psi_info;
  static PSI_statement_info close_psi_info;
#endif

 private:
  /**
   * @brief This is a wrapper function used to execute
   * and operation on Prepared_statement. This takes case of using
   * relevant protocol, diagnostic area, backup the current query arena
   * before executing the prepared statement operation.
   *
   * @tparam Function type of function to run
   * @param exec_func function to run
   * @param psi_info  PSI_statement_info instance.
   *
   * @return true
   * @return false
   */
  template <typename Function>
  bool run(Function exec_func, PSI_statement_info *psi_info);

  // See prepare()
  bool internal_prepare();

  // See execute()
  bool internal_execute();

  // See fetch()
  bool internal_fetch();

  // See reset()
  bool internal_reset();

  /**
   * @brief Reset the statement parameters and cursors.
   *
   * @param invalidate_params   When set to true parameters are invalidated.
   *
   * @return true on failure.
   * @return false on execute.
   */

  bool internal_reset(bool invalidate_params);

  // See close()
  bool internal_close();

  /**
   * @brief Method to enable cursor.
   *
   * @return true if cursor can be used for a statement.
   * @return false if cursor can not be used for a statement.
   */
  bool enable_cursor();

  /**
   * @brief Create a parameter buffers
   *
   * @return true on failure
   * @return false on success
   */
  bool create_parameter_buffers();

  // The prepared statement implementation.
  Prepared_statement *m_stmt{nullptr};

  // Parameters values.
  PS_PARAM *m_parameters{nullptr};

  /*
    Store the parameter and parameter values in a separate mem_root.
    These can be even stored in m_stmt's mem_root. But reprepare() would
    free up the memory used by the prepared statement. Hence, using separate
    mem_root.
  */
  MEM_ROOT m_parameter_mem_root;

  /*
    Size of each parameter buffer allocated in mem_root.
    We re-use the same buffer, if the parameter value size fit
    within this max.
  */
  ulong *m_parameter_buffer_max{nullptr};

  /*
    Denotes if new parameters were set.
    This is set 'true' for the first time. It is set to 'false' upon
    re-execute without rebinding any parameters.
  */
  bool m_bound_new_parameter_types{true};
};

#endif
