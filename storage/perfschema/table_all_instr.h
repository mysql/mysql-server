/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TABLE_ALL_INSTR_H
#define TABLE_ALL_INSTR_H

/**
  @file storage/perfschema/table_all_instr.h
  Abstract tables for all instruments (declarations).
*/

#include <sys/types.h>

#include "my_compiler.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Position of a cursor on table_all_instr. */
struct pos_all_instr : public PFS_double_index,
                       public PFS_instrument_view_constants
{
  pos_all_instr() : PFS_double_index(FIRST_VIEW, 0)
  {
  }

  inline void
  reset(void)
  {
    m_index_1 = FIRST_VIEW;
    m_index_2 = 0;
  }

  inline bool
  has_more_view(void)
  {
    return (m_index_1 <= LAST_VIEW);
  }

  inline void
  next_view(void)
  {
    m_index_1++;
    m_index_2 = 0;
  }
};

class PFS_index_all_instr : public PFS_engine_index
{
public:
  PFS_index_all_instr(PFS_engine_key *key_1) : PFS_engine_index(key_1)
  {
  }

  ~PFS_index_all_instr()
  {
  }

  virtual bool
  match(PFS_mutex *)
  {
    return false;
  }
  virtual bool
  match(PFS_rwlock *)
  {
    return false;
  }
  virtual bool
  match(PFS_cond *)
  {
    return false;
  }
  virtual bool
  match(PFS_file *)
  {
    return false;
  }
  virtual bool
  match(PFS_socket *)
  {
    return false;
  }
  /* All views match by default. */
  virtual bool
  match_view(uint view MY_ATTRIBUTE((unused)))
  {
    return true;
  }
};

/**
  Abstract table, a union of all instrumentations instances.
  This table is a union of:
  - a view on all mutex instances,
  - a view on all rwlock instances,
  - a view on all cond instances,
  - a view on all file instances,
  - a view on all socket instances
*/
class table_all_instr : public PFS_engine_table
{
public:
  static ha_rows get_row_count();

  virtual int
  index_init(uint, bool)
  {
    return 0;
  }
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);
  virtual int index_next(void);

protected:
  table_all_instr(const PFS_engine_table_share *share);

public:
  ~table_all_instr()
  {
  }

protected:
  /**
    Build a row in the mutex instance view.
    @param pfs                        the mutex instance
  */
  virtual int make_mutex_row(PFS_mutex *pfs) = 0;
  /**
    Build a row in the rwlock instance view.
    @param pfs                        the rwlock instance
  */
  virtual int make_rwlock_row(PFS_rwlock *pfs) = 0;
  /**
    Build a row in the condition instance view.
    @param pfs                        the condition instance
  */
  virtual int make_cond_row(PFS_cond *pfs) = 0;
  /**
    Build a row in the file instance view.
    @param pfs                        the file instance
  */
  virtual int make_file_row(PFS_file *pfs) = 0;
  /**
    Build a row in the socket instance view.
    @param pfs                        the socket instance
  */
  virtual int make_socket_row(PFS_socket *pfs) = 0;

  /** Current position. */
  pos_all_instr m_pos;
  /** Next position. */
  pos_all_instr m_next_pos;

  PFS_index_all_instr *m_opened_index;
};

/** @} */
#endif
