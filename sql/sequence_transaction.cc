/* Copyright (c) 2000, 2018, Alibaba and/or its affiliates. All rights reserved.

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

/**
  @file

  Implementation of Sequence autonomous transaction.
*/

#include "sql/sequence_transaction.h"
#include "sql/sequence_common.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/transaction.h"


/**
  @addtogroup Sequence Engine

  Sequence autonomous transaction

  @{
*/

/**
  Constructor of autonomous transaction.
*/
THD::Autonomous_trx_rw::Autonomous_trx_rw(THD *thd, Attachable_trx *prev_trx)
    : Attachable_trx(thd, prev_trx) {
  init_autonomous();
}

/**
  Cleanup the autonomous transaction.
 */
THD::Autonomous_trx_rw::~Autonomous_trx_rw() {}

/**
  The autonomous is readwrite, and will generate binlog, so inherit the
  option_bits.
*/
void THD::Autonomous_trx_rw::init_autonomous() {
  m_thd->tx_read_only = false;
  m_thd->variables.option_bits = m_trx_state.m_thd_option_bits;
  m_is_autonomous = true;
}

void THD::begin_autonomous_rw_transaction() {
  m_attachable_trx = new Autonomous_trx_rw(this, m_attachable_trx);
}

void THD::end_autonomous_rw_transaction() {
  Attachable_trx *prev_trx = m_attachable_trx->get_prev_attachable_trx();
  delete m_attachable_trx;
  m_attachable_trx = prev_trx;
}

bool THD::is_autonomous_transaction() const {
  return is_attachable_rw_transaction_active() &&
         m_attachable_trx->is_autonomous();
}

/**
  Update the base table and reflush the cache.

  @param[in]      super_table       The query opened table. Here will open
                                    other one to do updating.

  @retval         0                 Success
  @retval         ~0                Failure
*/
int Reload_sequence_cache_ctx::reload_sequence_cache(TABLE *super_table) {
  int error = 0;
  TABLE *table;
  DBUG_ENTER("Reload_sequence_cache_ctx::reload_sequence_cache");

  /**
    Report error and return the uniform error HA_ERR_SEQUENCE_ACCESS_FAILURE.
    since this function is called by ha_sequence.
  */
  if (!(m_thd->is_current_stmt_binlog_disabled()) &&
      !(m_thd->is_current_stmt_binlog_format_row())) {
    my_error(ER_SEQUENCE_BINLOG_FORMAT, MYF(0));
    goto err;
  }

  /* Open the sequence table */
  if ((error = m_trans.get_otx()->open_table())) goto err;

  table = m_trans.get_otx()->get_table();

  if ((error = table->file->ha_flush_cache(super_table))) {
    trans_rollback_stmt(m_thd);
    trans_rollback(m_thd);
    /* Here the error is handler error, so return it directly. */
    DBUG_RETURN(error);
  } else {
    if (trans_commit_stmt(m_thd) || trans_commit(m_thd)) {
      trans_rollback(m_thd);
      goto err;
    }
  }
  DBUG_RETURN(0);

err:
  DBUG_RETURN(HA_ERR_SEQUENCE_ACCESS_FAILURE);
}

/**
  Constructor of Sequence transaction.
  Open a new autonomous transaction by backup current transaction context
*/
Sequence_transaction::Sequence_transaction(THD *thd, TABLE_SHARE *share)
    : m_otx(thd, share), m_thd(thd) {
  m_thd->begin_autonomous_rw_transaction();
}
/**
  Destructor of Sequence transaction.
  End the autonomous transaction by restore transaction context
*/
Sequence_transaction::~Sequence_transaction() {
  m_thd->end_autonomous_rw_transaction();
}

Reload_sequence_cache_ctx::~Reload_sequence_cache_ctx() {
  m_thd->in_sub_stmt = m_saved_in_sub_stmt;
}

/// @} (end of group Sequence Engine)
