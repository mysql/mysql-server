/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**
  @file storage/perfschema/cursor_by_host.cc
  Cursor CURSOR_BY_HOST (implementation).
*/

#include "my_global.h"
#include "cursor_by_host.h"
#include "pfs_host.h"

cursor_by_host::cursor_by_host(const PFS_engine_table_share *share)
  : PFS_engine_table(share, &m_pos),
    m_pos(0), m_next_pos(0)
{}

void cursor_by_host::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int cursor_by_host::rnd_next(void)
{
  PFS_host *host;

  for (m_pos.set_at(&m_next_pos);
       m_pos.m_index < host_max;
       m_pos.next())
  {
    host= & host_array[m_pos.m_index];
    if (host->m_lock.is_populated())
    {
      make_row(host);
      m_next_pos.set_after(&m_pos);
      return 0;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int
cursor_by_host::rnd_pos(const void *pos)
{
  PFS_host *pfs;

  set_position(pos);
  DBUG_ASSERT(m_pos.m_index < host_max);
  pfs= &host_array[m_pos.m_index];
  if (pfs->m_lock.is_populated())
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

