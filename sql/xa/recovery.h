/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef XA_RECOVERY_H_INCLUDED
#define XA_RECOVERY_H_INCLUDED

#include "sql/handler.h"    // XA_recover_txn
#include "sql/sql_class.h"  // THD
#include "sql/xa.h"         // Xid_commit_list, Xa_state_list, XID, ...

struct xarecover_st {
  int len, found_foreign_xids, found_my_xids;
  XA_recover_txn *list;
  Xid_commit_list const *commit_list;
  Xa_state_list *xa_list;
  bool dry_run;
};

namespace xa {
namespace recovery {
/**
  Callback to be invoked by `ha_recover` over each storage engine plugin.

  1. It invokes `handler::recover_prepared_in_tc(XA_recover_txn[])`
     callback from the plugin interface, in order to retrieve the list of
     transactions that are, at the moment, in prepared in TC state within
     the storage engine.
  2. For all externally coordinated transactions that are in prepared in TC
     state, adds each to the list of recoverable XA transactions
     `xarecover_st::xa_list` in `PREPARED` state.

  @param thd The THD session object to be used in recovering the
             transactions.
  @param plugin The plugin interface for the storage engine to recover the
                transactions for.
  @param arg A pointer to `struct xarecover_st` that hold the information
             about the transaction coordinator state.

  @return false if the recovery ended successfully, false otherwise.
 */
bool recover_prepared_in_tc_one_ht(THD *thd, plugin_ref plugin, void *arg);
/**
  Callback to be invoked by `ha_recover` over each storage engine plugin.

  1. It invokes `handler::recover(XA_recover_txn[])` callback from the
     plugin interface, in order to retrieve the list of transactions that
     are, at the moment, in prepared state within the storage engine.
  2. Invokes `recover_one_internal_trx` for all the retrieved transactions
     that are internally coordinated transactions.
  3. Invokes `recover_one_external_trx` for all the retrieved transactions
     that are externally coordinated transactions.

  @param thd The THD session object to be used in recovering the
             transactions.
  @param plugin The plugin interface for the storage engine to recover the
                transactions for.
  @param arg A pointer to `struct xarecover_st` that hold the information
             about the transaction coordinator state.

  @return false if the recovery ended successfully, false otherwise.
 */
bool recover_one_ht(THD *thd, plugin_ref plugin, void *arg);
}  // namespace recovery
}  // namespace xa
#endif  // XA_RECOVERY_H_INCLUDED
