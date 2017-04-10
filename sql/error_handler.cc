/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#include "sql/error_handler.h"

#include <errno.h>

#include "key.h"
#include "my_inttypes.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_thread_local.h"
#include "mysys_err.h"           // EE_*
#include "sql_class.h"           // THD
#include "sql_lex.h"
#include "system_variables.h"
#include "table.h"               // TABLE_LIST
#include "transaction_info.h"


/**
  Implementation of Drop_table_error_handler::handle_condition().
  The reason in having this implementation is to silence technical low-level
  warnings during DROP TABLE operation. Currently we don't want to expose
  the following warnings during DROP TABLE:
    - Some of table files are missed or invalid (the table is going to be
      deleted anyway, so why bother that something was missed).

  @return true if the condition is handled.
*/
bool Drop_table_error_handler::handle_condition(THD*,
                                                uint sql_errno,
                                                const char*,
                                                Sql_condition::enum_severity_level*,
                                                const char*)
{
  return (sql_errno == EE_DELETE && my_errno() == ENOENT);
}


/**
  This handler is used for the statements which support IGNORE keyword.
  If IGNORE is specified in the statement, this error handler converts
  the given errors codes to warnings.
  These errors occur for each record. With IGNORE, statements are not
  aborted and next row is processed.

*/
bool Ignore_error_handler::handle_condition(THD *thd,
                                            uint sql_errno,
                                            const char*,
                                            Sql_condition::enum_severity_level *level,
                                            const char*)
{
  /*
    If a statement is executed with IGNORE keyword then this handler
    gets pushed for the statement. If there is trigger on the table
    which contains statements without IGNORE then this handler should
    not convert the errors within trigger to warnings.
  */
  if (!thd->lex->is_ignore())
    return false;
  /*
    Error codes ER_DUP_ENTRY_WITH_KEY_NAME is used while calling my_error
    to get the proper error messages depending on the use case.
    The error code used is ER_DUP_ENTRY to call error functions.

    Same case exists for ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT which uses
    error code of ER_NO_PARTITION_FOR_GIVEN_VALUE to call error function.

    There error codes are added here to force consistency if these error
    codes are used in any other case in future.
  */
  switch (sql_errno)
  {
  case ER_SUBQUERY_NO_1_ROW:
  case ER_ROW_IS_REFERENCED_2:
  case ER_NO_REFERENCED_ROW_2:
  case ER_BAD_NULL_ERROR:
  case ER_DUP_ENTRY:
  case ER_DUP_ENTRY_WITH_KEY_NAME:
  case ER_DUP_KEY:
  case ER_VIEW_CHECK_FAILED:
  case ER_NO_PARTITION_FOR_GIVEN_VALUE:
  case ER_NO_PARTITION_FOR_GIVEN_VALUE_SILENT:
  case ER_ROW_DOES_NOT_MATCH_GIVEN_PARTITION_SET:
    (*level)= Sql_condition::SL_WARNING;
    break;
  default:
    break;
  }
  return false;
}

bool View_error_handler::handle_condition(
                                THD *thd,
                                uint sql_errno,
                                const char *,
                                Sql_condition::enum_severity_level*,
                                const char*)
{
  /*
    Error will be handled by Show_create_error_handler for
    SHOW CREATE statements.
  */
  if (thd->lex->sql_command == SQLCOM_SHOW_CREATE)
    return false;

  switch (sql_errno)
  {
    case ER_BAD_FIELD_ERROR:
    case ER_SP_DOES_NOT_EXIST:
    // ER_FUNC_INEXISTENT_NAME_COLLISION cannot happen here.
    case ER_PROCACCESS_DENIED_ERROR:
    case ER_COLUMNACCESS_DENIED_ERROR:
    case ER_TABLEACCESS_DENIED_ERROR:
    // ER_TABLE_NOT_LOCKED cannot happen here.
    case ER_NO_SUCH_TABLE:
    {
      TABLE_LIST *top= m_top_view->top_table();
      my_error(ER_VIEW_INVALID, MYF(0),
               top->view_db.str, top->view_name.str);
      return true;
    }

    case ER_NO_DEFAULT_FOR_FIELD:
    {
      TABLE_LIST *top= m_top_view->top_table();
      // TODO: make correct error message
      my_error(ER_NO_DEFAULT_FOR_VIEW_FIELD, MYF(0),
               top->view_db.str, top->view_name.str);
      return true;
    }
  }
  return false;
}

/**
  Implementation of STRICT mode.
  Upgrades a set of given conditions from warning to error.
*/
bool Strict_error_handler::handle_condition(THD *thd,
                                            uint sql_errno,
                                            const char*,
                                            Sql_condition::enum_severity_level *level,
                                            const char*)
{
  /*
    STRICT error handler should not be effective if we have changed the
    variable to turn off STRICT mode. This is the case when a SF/SP/Trigger
    calls another SP/SF. A statement in SP/SF which is affected by STRICT mode
    with push this handler for the statement. If the same statement calls
    another SP/SF/Trigger, we already have the STRICT handler pushed for the
    statement. We dont want the strict handler to be effective for the
    next SP/SF/Trigger call if it was not created in STRICT mode.
  */
  if (!thd->is_strict_mode())
    return false;
  /* STRICT MODE should affect only the below statements */
  switch (thd->lex->sql_command)
  {
  case SQLCOM_SET_OPTION:
  case SQLCOM_SELECT:
    if (m_set_select_behavior == DISABLE_SET_SELECT_STRICT_ERROR_HANDLER)
      return false;
  case SQLCOM_CREATE_TABLE:
  case SQLCOM_CREATE_INDEX:
  case SQLCOM_DROP_INDEX:
  case SQLCOM_INSERT:
  case SQLCOM_REPLACE:
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  case SQLCOM_UPDATE:
  case SQLCOM_UPDATE_MULTI:
  case SQLCOM_DELETE:
  case SQLCOM_DELETE_MULTI:
  case SQLCOM_ALTER_TABLE:
  case SQLCOM_LOAD:
  case SQLCOM_CALL:
  case SQLCOM_END:
    break;
  default:
    return false;
  }

  switch (sql_errno)
  {
  case ER_TRUNCATED_WRONG_VALUE:
  case ER_WRONG_VALUE_FOR_TYPE:
  case ER_WARN_DATA_OUT_OF_RANGE:
  case ER_DIVISION_BY_ZERO:
  case ER_TRUNCATED_WRONG_VALUE_FOR_FIELD:
  case WARN_DATA_TRUNCATED:
  case ER_DATA_TOO_LONG:
  case ER_BAD_NULL_ERROR:
  case ER_NO_DEFAULT_FOR_FIELD:
  case ER_TOO_LONG_KEY:
  case ER_NO_DEFAULT_FOR_VIEW_FIELD:
  case ER_WARN_NULL_TO_NOTNULL:
  case ER_CUT_VALUE_GROUP_CONCAT:
  case ER_DATETIME_FUNCTION_OVERFLOW:
  case ER_WARN_TOO_FEW_RECORDS:
  case ER_WARN_TOO_MANY_RECORDS:
  case ER_INVALID_ARGUMENT_FOR_LOGARITHM:
  case ER_NUMERIC_JSON_VALUE_OUT_OF_RANGE:
  case ER_INVALID_JSON_VALUE_FOR_CAST:
  case ER_WARN_ALLOWED_PACKET_OVERFLOWED:
    if ((*level == Sql_condition::SL_WARNING) &&
        (!thd->get_transaction()->cannot_safely_rollback(Transaction_ctx::STMT)
         || (thd->variables.sql_mode & MODE_STRICT_ALL_TABLES)))
    {
      (*level)= Sql_condition::SL_ERROR;
    }
    break;
  default:
    break;
  }
  return false;
}


/**
  This internal handler is used to trap ER_NO_SUCH_TABLE and
  ER_WRONG_MRG_TABLE errors during CHECK/REPAIR TABLE for MERGE
  tables.
*/

class Repair_mrg_table_error_handler : public Internal_error_handler
{
public:
  Repair_mrg_table_error_handler()
    : m_handled_errors(false), m_unhandled_errors(false)
  {}

  virtual bool handle_condition(THD*,
                                uint sql_errno,
                                const char*,
                                Sql_condition::enum_severity_level*,
                                const char*)
  {
    if (sql_errno == ER_NO_SUCH_TABLE || sql_errno == ER_WRONG_MRG_TABLE)
    {
      m_handled_errors= true;
      return true;
    }

    m_unhandled_errors= true;
    return false;
  }

  /**
    Returns true if there were ER_NO_SUCH_/WRONG_MRG_TABLE and there
    were no unhandled errors. false otherwise.
  */
  bool safely_trapped_errors()
  {
    /*
      Check for m_handled_errors is here for extra safety.
      It can be useful in situation when call to open_table()
      fails because some error which was suppressed by another
      error handler (e.g. in case of MDL deadlock which we
      decided to solve by back-off and retry).
    */
    return (m_handled_errors && (! m_unhandled_errors));
  }

private:
  bool m_handled_errors;
  bool m_unhandled_errors;
};
