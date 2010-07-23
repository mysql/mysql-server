/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TABLE_ALL_INSTR_H
#define TABLE_ALL_INSTR_H

/**
  @file storage/perfschema/table_all_instr.h
  Abstract tables for all instruments (declarations).
*/

#include "pfs_instr_class.h"
#include "pfs_instr.h"
#include "pfs_engine_table.h"

/**
  @addtogroup Performance_schema_tables
  @{
*/

/** Position of a cursor on table_all_instr_class. */
struct pos_all_instr_class : public PFS_double_index,
                             public PFS_instrument_view_constants
{
  pos_all_instr_class()
    : PFS_double_index(VIEW_MUTEX, 1)
  {}

  inline void reset(void)
  {
    m_index_1= VIEW_MUTEX;
    m_index_2= 1;
  }

  inline bool has_more_view(void)
  { return (m_index_1 <= VIEW_FILE); }

  inline void next_view(void)
  {
    m_index_1++;
    /* Instrument keys start at 1, not 0. */
    m_index_2= 1;
  }
};

/**
  Abstract table, a union of all instrumentations class metadata.
  This table is a union of:
  - a view on all mutex classes,
  - a view on all rwlock classes,
  - a view on all cond classes,
  - a view on all file classes
*/
class table_all_instr_class : public PFS_engine_table
{
public:
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  table_all_instr_class(const PFS_engine_table_share *share);

public:
  ~table_all_instr_class()
  {}

protected:
  /**
    Build a row.
    @param klass                      the instrument class
  */
  virtual void make_instr_row(PFS_instr_class *klass)= 0;

  /** Current position. */
  pos_all_instr_class m_pos;
  /** Next position. */
  pos_all_instr_class m_next_pos;
};

/** Position of a cursor on table_all_instr. */
struct pos_all_instr : public PFS_double_index,
                       public PFS_instrument_view_constants
{
  pos_all_instr()
    : PFS_double_index(VIEW_MUTEX, 0)
  {}

  inline void reset(void)
  {
    m_index_1= VIEW_MUTEX;
    m_index_2= 0;
  }

  inline bool has_more_view(void)
  { return (m_index_1 <= VIEW_FILE); }

  inline void next_view(void)
  {
    m_index_1++;
    m_index_2= 0;
  }
};

/**
  Abstract table, a union of all instrumentations instances.
  This table is a union of:
  - a view on all mutex instances,
  - a view on all rwlock instances,
  - a view on all cond instances,
  - a view on all file instances
*/
class table_all_instr : public PFS_engine_table
{
public:
  virtual int rnd_next();
  virtual int rnd_pos(const void *pos);
  virtual void reset_position(void);

protected:
  table_all_instr(const PFS_engine_table_share *share);

public:
  ~table_all_instr()
  {}

protected:
  /**
    Build a row in the mutex instance view.
    @param pfs                        the mutex instance
  */
  virtual void make_mutex_row(PFS_mutex *pfs)= 0;
  /**
    Build a row in the rwlock instance view.
    @param pfs                        the rwlock instance
  */
  virtual void make_rwlock_row(PFS_rwlock *pfs)= 0;
  /**
    Build a row in the condition instance view.
    @param pfs                        the condition instance
  */
  virtual void make_cond_row(PFS_cond *pfs)= 0;
  /**
    Build a row in the file instance view.
    @param pfs                        the file instance
  */
  virtual void make_file_row(PFS_file *pfs)= 0;

  /** Current position. */
  pos_all_instr m_pos;
  /** Next position. */
  pos_all_instr m_next_pos;
};

/** @} */
#endif
