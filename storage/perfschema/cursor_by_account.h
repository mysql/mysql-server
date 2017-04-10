/* Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
  */

#ifndef CURSOR_BY_ACCOUNT_H
#define CURSOR_BY_ACCOUNT_H

/**
  @file storage/perfschema/cursor_by_account.h
  Cursor CURSOR_BY_ACCOUNT (declarations).
*/

#include "pfs_engine_table.h"
#include "pfs_account.h"
#include "table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

class PFS_index_accounts : public PFS_engine_index
{
public:
  PFS_index_accounts(PFS_engine_key *key_1) : PFS_engine_index(key_1)
  {
  }

  PFS_index_accounts(PFS_engine_key *key_1, PFS_engine_key *key_2)
    : PFS_engine_index(key_1, key_2)
  {
  }

  virtual ~PFS_index_accounts()
  {
  }

  virtual bool match(PFS_account *pfs) = 0;
};

/** Cursor CURSOR_BY_ACCOUNT. */
class cursor_by_account : public PFS_engine_table
{
public:
  static ha_rows get_row_count();

  virtual void reset_position(void);

  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);

  virtual int index_next();

protected:
  cursor_by_account(const PFS_engine_table_share *share);

public:
  ~cursor_by_account()
  {
  }

protected:
  virtual int make_row(PFS_account *account) = 0;

private:
  /** Current position. */
  PFS_simple_index m_pos;
  /** Next position. */
  PFS_simple_index m_next_pos;

protected:
  PFS_index_accounts *m_opened_index;
};

/** @} */
#endif
