/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_all_instr.cc
  Abstract tables for all instruments (implementation).
*/

#include "storage/perfschema/table_all_instr.h"

#include <stddef.h>

#include "my_thread.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"

ha_rows table_all_instr::get_row_count(void) {
  return global_mutex_container.get_row_count() +
         global_rwlock_container.get_row_count() +
         global_cond_container.get_row_count() +
         global_file_container.get_row_count() +
         global_socket_container.get_row_count();
}

table_all_instr::table_all_instr(const PFS_engine_table_share *share)
    : PFS_engine_table(share, &m_pos), m_pos(), m_next_pos() {}

void table_all_instr::reset_position(void) {
  m_pos.reset();
  m_next_pos.reset();
}

int table_all_instr::rnd_next(void) {
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_file *file;
  PFS_socket *socket;

  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    switch (m_pos.m_index_1) {
      case pos_all_instr::VIEW_MUTEX: {
        PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index_2);
        mutex = it.scan_next(&m_pos.m_index_2);
        if (mutex != NULL) {
          m_next_pos.set_after(&m_pos);
          return make_mutex_row(mutex);
        }
      } break;
      case pos_all_instr::VIEW_RWLOCK: {
        PFS_rwlock_iterator it =
            global_rwlock_container.iterate(m_pos.m_index_2);
        rwlock = it.scan_next(&m_pos.m_index_2);
        if (rwlock != NULL) {
          m_next_pos.set_after(&m_pos);
          return make_rwlock_row(rwlock);
        }
      } break;
      case pos_all_instr::VIEW_COND: {
        PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index_2);
        cond = it.scan_next(&m_pos.m_index_2);
        if (cond != NULL) {
          m_next_pos.set_after(&m_pos);
          return make_cond_row(cond);
        }
      } break;
      case pos_all_instr::VIEW_FILE: {
        PFS_file_iterator it = global_file_container.iterate(m_pos.m_index_2);
        file = it.scan_next(&m_pos.m_index_2);
        if (file != NULL) {
          m_next_pos.set_after(&m_pos);
          return make_file_row(file);
        }
      } break;
      case pos_all_instr::VIEW_SOCKET: {
        PFS_socket_iterator it =
            global_socket_container.iterate(m_pos.m_index_2);
        socket = it.scan_next(&m_pos.m_index_2);
        if (socket != NULL) {
          m_next_pos.set_after(&m_pos);
          return make_socket_row(socket);
        }
      } break;
    }
  }

  return HA_ERR_END_OF_FILE;
}

int table_all_instr::rnd_pos(const void *pos) {
  PFS_mutex *mutex;
  PFS_rwlock *rwlock;
  PFS_cond *cond;
  PFS_file *file;
  PFS_socket *socket;

  set_position(pos);

  switch (m_pos.m_index_1) {
    case pos_all_instr::VIEW_MUTEX:
      mutex = global_mutex_container.get(m_pos.m_index_2);
      if (mutex != NULL) {
        return make_mutex_row(mutex);
      }
      break;
    case pos_all_instr::VIEW_RWLOCK:
      rwlock = global_rwlock_container.get(m_pos.m_index_2);
      if (rwlock != NULL) {
        return make_rwlock_row(rwlock);
      }
      break;
    case pos_all_instr::VIEW_COND:
      cond = global_cond_container.get(m_pos.m_index_2);
      if (cond != NULL) {
        return make_cond_row(cond);
      }
      break;
    case pos_all_instr::VIEW_FILE:
      file = global_file_container.get(m_pos.m_index_2);
      if (file != NULL) {
        return make_file_row(file);
      }
      break;
    case pos_all_instr::VIEW_SOCKET:
      socket = global_socket_container.get(m_pos.m_index_2);
      if (socket != NULL) {
        return make_socket_row(socket);
      }
      break;
  }

  return HA_ERR_RECORD_DELETED;
}

int table_all_instr::index_next(void) {
  for (m_pos.set_at(&m_next_pos); m_pos.has_more_view(); m_pos.next_view()) {
    if (!m_opened_index->match_view(m_pos.m_index_1)) {
      continue;
    }

    switch (m_pos.m_index_1) {
      case pos_all_instr::VIEW_MUTEX: {
        PFS_mutex *mutex;
        PFS_mutex_iterator it = global_mutex_container.iterate(m_pos.m_index_2);
        do {
          mutex = it.scan_next(&m_pos.m_index_2);
          if (mutex != NULL) {
            if (m_opened_index->match(mutex)) {
              if (!make_mutex_row(mutex)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        } while (mutex != NULL);
      } break;
      case pos_all_instr::VIEW_RWLOCK: {
        PFS_rwlock *rwlock;
        PFS_rwlock_iterator it =
            global_rwlock_container.iterate(m_pos.m_index_2);
        do {
          rwlock = it.scan_next(&m_pos.m_index_2);
          if (rwlock != NULL) {
            if (m_opened_index->match(rwlock)) {
              if (!make_rwlock_row(rwlock)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        } while (rwlock != NULL);
      } break;
      case pos_all_instr::VIEW_COND: {
        PFS_cond *cond;
        PFS_cond_iterator it = global_cond_container.iterate(m_pos.m_index_2);
        do {
          cond = it.scan_next(&m_pos.m_index_2);
          if (cond != NULL) {
            if (m_opened_index->match(cond)) {
              if (!make_cond_row(cond)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        } while (cond != NULL);
      } break;
      case pos_all_instr::VIEW_FILE: {
        PFS_file *file;
        PFS_file_iterator it = global_file_container.iterate(m_pos.m_index_2);
        do {
          file = it.scan_next(&m_pos.m_index_2);
          if (file != NULL) {
            if (m_opened_index->match(file)) {
              if (!make_file_row(file)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        } while (file != NULL);
      } break;
      case pos_all_instr::VIEW_SOCKET: {
        PFS_socket *socket;
        PFS_socket_iterator it =
            global_socket_container.iterate(m_pos.m_index_2);
        do {
          socket = it.scan_next(&m_pos.m_index_2);
          if (socket != NULL) {
            if (m_opened_index->match(socket)) {
              if (!make_socket_row(socket)) {
                m_next_pos.set_after(&m_pos);
                return 0;
              }
            }
          }
        } while (socket != NULL);
      } break;
    }
  }

  return HA_ERR_END_OF_FILE;
}
