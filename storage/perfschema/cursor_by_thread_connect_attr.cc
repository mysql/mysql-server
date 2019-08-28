/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "my_global.h"
#include "cursor_by_thread_connect_attr.h"

cursor_by_thread_connect_attr::cursor_by_thread_connect_attr(
  const PFS_engine_table_share *share) :
  PFS_engine_table(share, &m_pos), m_row_exists(false)
{}

int cursor_by_thread_connect_attr::rnd_next(void)
{
  PFS_thread *thread;

  for (m_pos.set_at(&m_next_pos);
       m_pos.has_more_thread();
       m_pos.next_thread())
  {
    thread= &thread_array[m_pos.m_index_1];

    if (thread->m_lock.is_populated())
    {
      make_row(thread, m_pos.m_index_2);
      if (m_row_exists)
      {
        m_next_pos.set_after(&m_pos);
        return 0;
      }
    }
  }
  return HA_ERR_END_OF_FILE;
}


int cursor_by_thread_connect_attr::rnd_pos(const void *pos)
{
  PFS_thread *thread;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index_1 < thread_max);

  thread= &thread_array[m_pos.m_index_1];
  if (!thread->m_lock.is_populated())
    return HA_ERR_RECORD_DELETED;

  make_row(thread, m_pos.m_index_2);
  if (m_row_exists)
    return 0;

  return HA_ERR_RECORD_DELETED;
}


void cursor_by_thread_connect_attr::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}
