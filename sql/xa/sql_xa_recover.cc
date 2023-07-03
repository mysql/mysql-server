/* Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "sql/xa/sql_xa_recover.h"     // Sql_cmd_xa_recover
#include "mysqld_error.h"              // Error codes
#include "sql/debug_sync.h"            // DEBUG_SYNC
#include "sql/item.h"                  // Item_int, Item_empyt_string
#include "sql/protocol.h"              // Protocol
#include "sql/sql_class.h"             // THD
#include "sql/transaction_info.h"      // Transaction_ctx
#include "sql/xa/transaction_cache.h"  // xa::Transaction_cache

Sql_cmd_xa_recover::Sql_cmd_xa_recover(bool print_xid_as_hex)
    : m_print_xid_as_hex(print_xid_as_hex) {}

enum_sql_command Sql_cmd_xa_recover::sql_command_code() const {
  return SQLCOM_XA_RECOVER;
}

bool Sql_cmd_xa_recover::execute(THD *thd) {
  bool st = check_xa_recover_privilege(thd) || trans_xa_recover(thd);

  DBUG_EXECUTE_IF("crash_after_xa_recover", { DBUG_SUICIDE(); });

  return st;
}

bool Sql_cmd_xa_recover::trans_xa_recover(THD *thd) {
  Protocol *protocol = thd->get_protocol();

  DBUG_TRACE;

  mem_root_deque<Item *> field_list(thd->mem_root);
  field_list.push_back(
      new Item_int(NAME_STRING("formatID"), 0, MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int(NAME_STRING("gtrid_length"), 0,
                                    MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_int(NAME_STRING("bqual_length"), 0,
                                    MY_INT32_NUM_DECIMAL_DIGITS));
  field_list.push_back(new Item_empty_string("data", XIDDATASIZE * 2 + 2));

  if (thd->send_result_metadata(field_list,
                                Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    return true;

  auto list = xa::Transaction_cache::get_cached_transactions();
  for (const auto &transaction : list) {
    XID_STATE *xs = transaction->xid_state();
    if (xs->has_state(XID_STATE::XA_PREPARED)) {
      protocol->start_row();
      xs->store_xid_info(protocol, m_print_xid_as_hex);

      if (protocol->end_row()) {
        return true;
      }
    }
  }

  my_eof(thd);
  return false;
}

bool Sql_cmd_xa_recover::check_xa_recover_privilege(THD *thd) const {
  Security_context *sctx = thd->security_context();

  if (!sctx->has_global_grant(STRING_WITH_LEN("XA_RECOVER_ADMIN")).first) {
    /*
      Report an error ER_XAER_RMERR. A supplementary error
      ER_SPECIFIC_ACCESS_DENIED_ERROR is also reported when
      SHOW WARNINGS is issued. This provides more information
      about the reason for failure.
    */
    my_error(ER_XAER_RMERR, MYF(0));
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "XA_RECOVER_ADMIN");
    return true;
  }

  return false;
}
