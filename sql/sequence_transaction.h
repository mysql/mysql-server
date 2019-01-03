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

#ifndef SEQUENCE_TRANSACTION_INCLUDED
#define SEQUENCE_TRANSACTION_INCLUDED

#include "sql/sql_sequence.h"  // Open_sequence_table_ctx

class THD;
struct TABLE_SHARE;

/**
  Sequence updating base table transaction is autonomous:
  Firstly backup current transaction context.
  Secondly commit inner transaction directly.
  At last restore the backed transaction context.
*/
class Sequence_transaction {
 public:
  explicit Sequence_transaction(THD *thd, TABLE_SHARE *share);

  virtual ~Sequence_transaction();

  /**
    Get opened table context.

    @retval     m_otx       Opened table context
  */
  Open_sequence_table_ctx *get_otx() { return &m_otx; }

 private:
  Open_sequence_table_ctx m_otx;
  THD *m_thd;
};

/**
  Updating the base sequence table context.
  It will update the base table, and reflush the sequence share cache.
*/
class Reload_sequence_cache_ctx {
 public:
  explicit Reload_sequence_cache_ctx(THD *thd, TABLE_SHARE *share)
      : m_trans(thd, share), m_thd(thd), m_saved_in_sub_stmt(thd->in_sub_stmt) {
    thd->in_sub_stmt = false;
  }

  virtual ~Reload_sequence_cache_ctx();
  /**
    Update the base table and reflush the cache.

    @param[in]      super_table       The query opened table.Here will open
                                      other one to do updating.

    @retval         0                 Success
    @retval         ~0                Failure
  */
  int reload_sequence_cache(TABLE *super_table);

 private:
  Sequence_transaction m_trans;
  THD *m_thd;
  bool m_saved_in_sub_stmt;
};

#endif
