/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

/* Execute CALL statement */

#include "sql/sql_call.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <algorithm>
#include <atomic>

#include "lex_string.h"
#include "my_base.h"

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/plugin_audit.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_routine_access, check_table_access
#include "sql/item.h"              // class Item
#include "sql/protocol.h"
#include "sql/sp.h"  // sp_find_routine
#include "sql/sp_head.h"
#include "sql/sp_pcontext.h"  // class sp_variable
#include "sql/sql_audit.h"    // AUDIT_EVENT
#include "sql/sql_class.h"    // class THD
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/system_variables.h"
#include "template_utils.h"

using std::max;

bool Sql_cmd_call::precheck(THD *thd) {
  // Check execute privilege on stored procedure
  if (check_routine_access(thd, EXECUTE_ACL, proc_name->m_db.str,
                           proc_name->m_name.str, true, false))
    return true;

  // Check SELECT privileges for any subqueries
  if (check_table_access(thd, SELECT_ACL, lex->query_tables, false, UINT_MAX,
                         false))
    return true;
  return false;
}

bool Sql_cmd_call::check_privileges(THD *thd) {
  if (check_routine_access(thd, EXECUTE_ACL, proc_name->m_db.str,
                           proc_name->m_name.str, true, false))
    return true;

  if (check_all_table_privileges(thd)) {
    return true;
  }

  sp_head *sp = sp_find_routine(thd, enum_sp_type::PROCEDURE, proc_name,
                                &thd->sp_proc_cache, true);
  if (sp == nullptr) {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PROCEDURE", proc_name->m_qname.str);
    return true;
  }

  sp_pcontext *root_parsing_context = sp->get_root_parsing_context();

  if (proc_args != nullptr) {
    int arg_no = 0;
    for (Item *arg : *proc_args) {
      sp_variable *spvar = root_parsing_context->find_variable(arg_no);
      if (arg->type() == Item::TRIGGER_FIELD_ITEM) {
        Item_trigger_field *itf = down_cast<Item_trigger_field *>(arg);
        itf->set_required_privilege(
            spvar->mode == sp_variable::MODE_IN    ? SELECT_ACL
            : spvar->mode == sp_variable::MODE_OUT ? UPDATE_ACL
                                                   : SELECT_ACL | UPDATE_ACL);
      }
      if (arg->walk(&Item::check_column_privileges, enum_walk::PREFIX,
                    pointer_cast<uchar *>(thd)))
        return true;
      arg_no++;
    }
  }
  thd->want_privilege = SELECT_ACL;
  if (lex->query_block->check_privileges_for_subqueries(thd)) {
    return true;
  }

  return false;
}

bool Sql_cmd_call::prepare_inner(THD *thd) {
  // All required SPs should be in cache so no need to look into DB.

  Query_block *const select = lex->query_block;

  sp_head *sp = sp_find_routine(thd, enum_sp_type::PROCEDURE, proc_name,
                                &thd->sp_proc_cache, true);
  if (sp == nullptr) {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PROCEDURE", proc_name->m_qname.str);
    return true;
  }

  sp_pcontext *root_parsing_context = sp->get_root_parsing_context();

  uint arg_count = proc_args != nullptr ? proc_args->size() : 0;

  if (root_parsing_context->context_var_count() != arg_count) {
    my_error(ER_SP_WRONG_NO_OF_ARGS, MYF(0), "PROCEDURE",
             proc_name->m_qname.str, root_parsing_context->context_var_count(),
             arg_count);
    return true;
  }

  if (proc_args == nullptr) {
    lex->unit->set_prepared();
    return false;
  }
  int arg_no = 0;
  for (Item *&arg : *proc_args) {
    sp_variable *spvar = root_parsing_context->find_variable(arg_no);
    if (arg->type() == Item::TRIGGER_FIELD_ITEM) {
      Item_trigger_field *itf = down_cast<Item_trigger_field *>(arg);
      itf->set_required_privilege(
          spvar->mode == sp_variable::MODE_IN    ? SELECT_ACL
          : spvar->mode == sp_variable::MODE_OUT ? UPDATE_ACL
                                                 : SELECT_ACL | UPDATE_ACL);
    }
    if ((!arg->fixed && arg->fix_fields(thd, &arg)) || arg->check_cols(1))
      return true; /* purecov: inspected */
    if (arg->data_type() == MYSQL_TYPE_INVALID) {
      switch (Item::type_to_result(spvar->type)) {
        case INT_RESULT:
        case REAL_RESULT:
        case DECIMAL_RESULT:
          if (arg->propagate_type(
                  thd,
                  Type_properties(spvar->type, spvar->field_def.is_unsigned)))
            return true;
          break;
        case STRING_RESULT:
          if (arg->propagate_type(
                  thd, Type_properties(spvar->type, spvar->field_def.charset)))
            return true;
          break;
        default:
          assert(false);
      }
    }
    arg_no++;
  }

  if (select->apply_local_transforms(thd, true)) return true;

  lex->unit->set_prepared();

  return false;
}

bool Sql_cmd_call::execute_inner(THD *thd) {
  // All required SPs should be in cache so no need to look into DB.

  sp_head *sp = sp_setup_routine(thd, enum_sp_type::PROCEDURE, proc_name,
                                 &thd->sp_proc_cache);
  if (sp == nullptr) {
    my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "PROCEDURE", proc_name->m_qname.str);
    return true;
  }

  /*
    Check that the stored procedure doesn't contain Dynamic SQL and doesn't
    return result sets: such stored procedures can't be called from
    a function or trigger.
  */
  if (thd->in_sub_stmt) {
    const char *where =
        (thd->in_sub_stmt & SUB_STMT_TRIGGER ? "trigger" : "function");
    if (sp->is_not_allowed_in_function(where)) return true;
  }

  if (mysql_event_tracking_stored_program_notify(
          thd, AUDIT_EVENT(EVENT_TRACKING_STORED_PROGRAM_EXECUTE),
          proc_name->m_db.str, proc_name->m_name.str, nullptr))
    return true;

  /*
    If sp_head::MULTI_RESULTS is set, then set SERVER_MORE_RESULTS_EXISTS if
    not set already and remember that it should be cleared.
  */
  uint bits_to_be_cleared = (~thd->server_status & SERVER_MORE_RESULTS_EXISTS);
  if (sp->m_flags & sp_head::MULTI_RESULTS) {
    if (!thd->get_protocol()->has_client_capability(CLIENT_MULTI_RESULTS)) {
      // Client does not support multiple result sets
      my_error(ER_SP_BADSELECT, MYF(0), sp->m_qname.str);
      return true;
    }
    thd->server_status |= SERVER_MORE_RESULTS_EXISTS;
  }

  const ha_rows select_limit = thd->variables.select_limit;
  thd->variables.select_limit = HA_POS_ERROR;

  /*
    Never write CALL statements into binlog:
    - If the mode is non-prelocked, each statement will be logged separately.
    - If the mode is prelocked, the invoking statement will care about writing
      into binlog.
    So just execute the statement.
  */
  const bool result = sp->execute_procedure(thd, proc_args);

  thd->variables.select_limit = select_limit;

  thd->server_status &= ~bits_to_be_cleared;

  if (result) {
    assert(thd->is_error() || thd->killed);
    return true;  // Substatement should already have sent error
  }

  my_ok(thd, max(thd->get_row_count_func(), 0LL));

  return false;
}
