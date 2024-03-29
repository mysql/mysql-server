/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "sql/xa/sql_xa_end.h"            // Sql_cmd_xa_end
#include "mysql/psi/mysql_transaction.h"  // MYSQL_SET_TRANSACTION_XA_STATE
#include "mysqld_error.h"                 // Error codes
#include "sql/debug_sync.h"               // DEBUG_SYNC
#include "sql/sql_class.h"                // THD
#include "sql/transaction_info.h"         // Transaction_ctx

Sql_cmd_xa_end::Sql_cmd_xa_end(xid_t *xid_arg, enum xa_option_words xa_option)
    : m_xid(xid_arg), m_xa_opt(xa_option) {}

enum_sql_command Sql_cmd_xa_end::sql_command_code() const {
  return SQLCOM_XA_END;
}

bool Sql_cmd_xa_end::execute(THD *thd) {
  bool st = trans_xa_end(thd);

  if (!st) my_ok(thd);

  return st;
}

bool Sql_cmd_xa_end::trans_xa_end(THD *thd) {
  XID_STATE *xid_state = thd->get_transaction()->xid_state();
  DBUG_TRACE;

  /* TODO: SUSPEND and FOR MIGRATE are not supported yet. */
  if (m_xa_opt != XA_NONE)
    my_error(ER_XAER_INVAL, MYF(0));
  else if (!xid_state->has_state(XID_STATE::XA_ACTIVE) &&
           !xid_state->has_state(XID_STATE::XA_ROLLBACK_ONLY))
    my_error(ER_XAER_RMFAIL, MYF(0), xid_state->state_name());
  else if (!xid_state->has_same_xid(m_xid))
    my_error(ER_XAER_NOTA, MYF(0));
  else if (!xid_state->xa_trans_rolled_back()) {
    xid_state->set_state(XID_STATE::XA_IDLE);
    MYSQL_SET_TRANSACTION_XA_STATE(thd->m_transaction_psi,
                                   (int)xid_state->get_state());
  } else {
    MYSQL_SET_TRANSACTION_XA_STATE(thd->m_transaction_psi,
                                   (int)xid_state->get_state());
  }

  return thd->is_error() || !xid_state->has_state(XID_STATE::XA_IDLE);
}
