/* Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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
  @file storage/perfschema/cursor_by_user.cc
  Cursor CURSOR_BY_USER (implementation).
*/

#include "my_global.h"
#include "cursor_by_user.h"
#include "pfs_buffer_container.h"

ha_rows
cursor_by_user::get_row_count(void)
{
  return global_user_container.get_row_count();
}

cursor_by_user::cursor_by_user(const PFS_engine_table_share *share)
  : PFS_engine_table(share, &m_pos),
    m_pos(0), m_next_pos(0)
{}

void cursor_by_user::reset_position(void)
{
  m_pos.m_index= 0;
  m_next_pos.m_index= 0;
}

int cursor_by_user::rnd_next(void)
{
  PFS_user *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_user_iterator it= global_user_container.iterate(m_pos.m_index);
  pfs= it.scan_next(& m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    m_next_pos.set_after(&m_pos);
    return 0;
  }

  return HA_ERR_END_OF_FILE;
}

int
cursor_by_user::rnd_pos(const void *pos)
{
  PFS_user *pfs;

  set_position(pos);

  pfs= global_user_container.get(m_pos.m_index);
  if (pfs != NULL)
  {
    make_row(pfs);
    return 0;
  }

  return HA_ERR_RECORD_DELETED;
}

