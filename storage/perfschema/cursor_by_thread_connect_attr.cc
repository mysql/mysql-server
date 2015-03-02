/* Copyright (c) 2008, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "cursor_by_thread_connect_attr.h"
#include "pfs_buffer_container.h"

ha_rows
cursor_by_thread_connect_attr::get_row_count(void)
{
  /*
    The real number of attributes per thread does not matter,
    we only need to hint the optimizer there are many per thread,
    so abusing session_connect_attrs_size_per_thread
    (which is a number of bytes, not attributes)
  */
  return global_thread_container.get_row_count() *
    session_connect_attrs_size_per_thread;
}

cursor_by_thread_connect_attr::cursor_by_thread_connect_attr(
  const PFS_engine_table_share *share) :
  PFS_engine_table(share, &m_pos), m_row_exists(false)
{}

int cursor_by_thread_connect_attr::rnd_next(void)
{
  PFS_thread *thread;
  bool has_more_thread= true;

  for (m_pos.set_at(&m_next_pos);
       has_more_thread;
       m_pos.next_thread())
  {
    thread= global_thread_container.get(m_pos.m_index_1, & has_more_thread);
    if (thread != NULL)
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

  thread= global_thread_container.get(m_pos.m_index_1);
  if (thread != NULL)
  {
    make_row(thread, m_pos.m_index_2);
    if (m_row_exists)
      return 0;
  }

  return HA_ERR_RECORD_DELETED;
}


void cursor_by_thread_connect_attr::reset_position(void)
{
  m_pos.reset();
  m_next_pos.reset();
}
