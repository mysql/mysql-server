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

/**
  @file storage/perfschema/table_all_instr.cc
  Abstract tables for all instruments (implementation).
*/

#include "my_global.h"
#include "my_pthread.h"
#include "table_all_instr.h"
#include "pfs_global.h"

table_all_instr::table_all_instr(const PFS_engine_table_share *share)
  : PFS_engine_table(share, &m_pos),
    m_pos(), m_next_pos()
{}

void table_all_instr::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_all_instr::rnd_next(void)
{
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_file *file;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1) {
    case pos_all_instr::VIEW_MUTEX:
      for ( ; m_pos.m_index_2 < mutex_max; m_pos.m_index_2++)
      {
        mutex= &mutex_array[m_pos.m_index_2];
        if (mutex->m_lock.is_populated())
        {
          make_mutex_row(mutex);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    case pos_all_instr::VIEW_RWLOCK:
      for ( ; m_pos.m_index_2 < rwlock_max; m_pos.m_index_2++)
      {
        rwlock= &rwlock_array[m_pos.m_index_2];
        if (rwlock->m_lock.is_populated())
        {
          make_rwlock_row(rwlock);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    case pos_all_instr::VIEW_COND:
      for ( ; m_pos.m_index_2 < cond_max; m_pos.m_index_2++)
      {
        cond= &cond_array[m_pos.m_index_2];
        if (cond->m_lock.is_populated())
        {
          make_cond_row(cond);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    case pos_all_instr::VIEW_FILE:
      for ( ; m_pos.m_index_2 < file_max; m_pos.m_index_2++)
      {
        file= &file_array[m_pos.m_index_2];
        if (file->m_lock.is_populated())
        {
          make_file_row(file);
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_all_instr::rnd_pos(const void *pos)
{
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_file *file;

  set_position(pos);

  switch (m_pos.m_index_1) {
  case pos_all_instr::VIEW_MUTEX:
    DBUG_ASSERT(m_pos.m_index_2 < mutex_max);
    mutex= &mutex_array[m_pos.m_index_2];
    if (mutex->m_lock.is_populated())
    {
      make_mutex_row(mutex);
      return 0;
    }
    break;
  case pos_all_instr::VIEW_RWLOCK:
    DBUG_ASSERT(m_pos.m_index_2 < rwlock_max);
    rwlock= &rwlock_array[m_pos.m_index_2];
    if (rwlock->m_lock.is_populated())
    {
      make_rwlock_row(rwlock);
      return 0;
    }
    break;
  case pos_all_instr::VIEW_COND:
    DBUG_ASSERT(m_pos.m_index_2 < cond_max);
    cond= &cond_array[m_pos.m_index_2];
    if (cond->m_lock.is_populated())
    {
      make_cond_row(cond);
      return 0;
    }
    break;
  case pos_all_instr::VIEW_FILE:
    DBUG_ASSERT(m_pos.m_index_2 < file_max);
    file= &file_array[m_pos.m_index_2];
    if (file->m_lock.is_populated())
    {
      make_file_row(file);
      return 0;
    }
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

table_all_instr_class::table_all_instr_class
(const PFS_engine_table_share *share)
  : PFS_engine_table(share, &m_pos),
    m_pos(), m_next_pos()
{}

void table_all_instr_class::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}

int table_all_instr_class::rnd_next(void)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_view();
       m_pos.next_view())
  {
    switch (m_pos.m_index_1) {
    case pos_all_instr_class::VIEW_MUTEX:
      mutex_class= find_mutex_class(m_pos.m_index_2);
      if (mutex_class)
      {
        make_instr_row(mutex_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_all_instr_class::VIEW_RWLOCK:
      rwlock_class= find_rwlock_class(m_pos.m_index_2);
      if (rwlock_class)
      {
        make_instr_row(rwlock_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_all_instr_class::VIEW_COND:
      cond_class= find_cond_class(m_pos.m_index_2);
      if (cond_class)
      {
        make_instr_row(cond_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    case pos_all_instr_class::VIEW_FILE:
      file_class= find_file_class(m_pos.m_index_2);
      if (file_class)
      {
        make_instr_row(file_class);
        m_next_pos.set_after(&m_pos);
        return 0;
      }
      break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_all_instr_class::rnd_pos(const void *pos)
{
  PFS_mutex_class *mutex_class;
  PFS_rwlock_class *rwlock_class;
  PFS_cond_class *cond_class;
  PFS_file_class *file_class;

  set_position(pos);
  switch (m_pos.m_index_1) {
  case pos_all_instr_class::VIEW_MUTEX:
    mutex_class= find_mutex_class(m_pos.m_index_2);
    if (mutex_class)
    {
      make_instr_row(mutex_class);
      return 0;
    }
    break;
  case pos_all_instr_class::VIEW_RWLOCK:
    rwlock_class= find_rwlock_class(m_pos.m_index_2);
    if (rwlock_class)
    {
      make_instr_row(rwlock_class);
      return 0;
    }
    break;
  case pos_all_instr_class::VIEW_COND:
    cond_class= find_cond_class(m_pos.m_index_2);
    if (cond_class)
    {
      make_instr_row(cond_class);
      return 0;
    }
    break;
  case pos_all_instr_class::VIEW_FILE:
    file_class= find_file_class(m_pos.m_index_2);
    if (file_class)
    {
      make_instr_row(file_class);
      return 0;
    }
    break;
  }

  return HA_ERR_RECORD_DELETED;
}

