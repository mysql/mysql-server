/* Copyright (c) 2011, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

/**
  @file storage/perfschema/cursor_by_user.cc
  Cursor CURSOR_BY_USER (implementation).
*/

#include "storage/perfschema/cursor_by_user.h"

#include <stddef.h>

#include "storage/perfschema/pfs_buffer_container.h"

ha_rows cursor_by_user::get_row_count() {
  return global_user_container.get_row_count();
}

cursor_by_user::cursor_by_user(const PFS_engine_table_share *share)
    : PFS_engine_table(share, &m_pos),
      m_pos(0),
      m_next_pos(0),
      m_opened_index(nullptr) {}

void cursor_by_user::reset_position() {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

int cursor_by_user::rnd_next() {
  PFS_user *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_user_iterator it = global_user_container.iterate(m_pos.m_index);
  pfs = it.scan_next(&m_pos.m_index);
  if (pfs != nullptr) {
    m_next_pos.set_after(&m_pos);
    return make_row(pfs);
  }

  return HA_ERR_END_OF_FILE;
}

int cursor_by_user::rnd_pos(const void *pos) {
  PFS_user *pfs;

  set_position(pos);

  pfs = global_user_container.get(m_pos.m_index);
  if (pfs != nullptr) {
    return make_row(pfs);
  }

  return HA_ERR_RECORD_DELETED;
}

int cursor_by_user::index_next() {
  PFS_user *pfs;

  m_pos.set_at(&m_next_pos);
  PFS_user_iterator it = global_user_container.iterate(m_pos.m_index);

  do {
    pfs = it.scan_next(&m_pos.m_index);
    if (pfs != nullptr) {
      if (m_opened_index->match(pfs)) {
        if (!make_row(pfs)) {
          m_next_pos.set_after(&m_pos);
          return 0;
        }
      }
    }
  } while (pfs != nullptr);

  return HA_ERR_END_OF_FILE;
}
