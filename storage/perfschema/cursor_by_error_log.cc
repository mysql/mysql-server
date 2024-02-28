/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
  @file storage/perfschema/cursor_by_error_log.cc
  Cursor CURSOR_BY_ERROR_LOG (implementation).
*/

#include "storage/perfschema/cursor_by_error_log.h"

#include <stddef.h>

#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_instr.h"

// const
cursor_by_error_log::cursor_by_error_log(const PFS_engine_table_share *share)
    : PFS_engine_table(share, &m_pos), m_opened_index(nullptr) {}

/// Get row-count (by getting the number of events in the ring-buffer)
ha_rows cursor_by_error_log::get_row_count() {
  return (ha_rows)log_sink_pfs_event_count();
}

/**
  Reset cursor position.

  We come through here when using a condition
  (rather than just ORDER BY [ASC/DESC]) in SELECT,
  e.g. in the form of ha_perfschema::index_read
  with HA_READ_KEY_EXACT when using =.

  We don't need a lock here since the reset
  breaks the association with the ring-buffer;
  it will be re-established on the first read-
  style primitive (reading a "row" of payload
  from the ring-buffer, or trying to advance
  to the next entry, either of which will imply
  log_sink_pfs_event_first() at some point).
*/
void cursor_by_error_log::reset_position() {
  m_pos.reset();
  m_next_pos.reset();
}

/**
  Read next row (from ring-buffer into table).

  We come through here when not using an index.

  @retval HA_ERR_END_OF_FILE  failed to locate data, or to create a row from it
  @retval 0                   success
*/
int cursor_by_error_log::rnd_next() {
  log_sink_pfs_event *row;
  int ret = HA_ERR_END_OF_FILE;

  log_sink_pfs_read_start();  // obtain read-lock on ring-buffer

  m_pos.set_at(&m_next_pos);  // set current position to next

  row = m_pos.scan_next();  // get current row and advance index

  if (row != nullptr) {  // if we managed to obtain an event (a "row") ...
    // Set next_position to the successor of the row we just obtained.
    m_next_pos.set_at(&m_pos);

    // copy event from ring-buffer to table_error_log instance
    ret = make_row(row);
  }

  log_sink_pfs_read_end();  // release read-lock

  return ret;
}

/**
  Random pos.  Unused.

  @retval HA_ERR_RECORD_DELETED  failed to locate data/to create a row from it
  @retval 0                      success
*/
int cursor_by_error_log::rnd_pos(const void *pos) {
  log_sink_pfs_event *row; /* purecov: begin inspected */
  int ret = HA_ERR_RECORD_DELETED;

  log_sink_pfs_read_start();  // acquire read-lock on ring-buffer.

  set_position(pos);

  row = m_pos.get_event();  // obtain event from ring-buffer

  if (row != nullptr) ret = make_row(row);  // fill in SQL row

  log_sink_pfs_read_end();  // release read-lock.

  return ret; /* purecov: end */
}

/**
  Go to next entry in index and retrieve the matching error log event.

  We come through here e.g. for = in the SELECT's condition,
  i.e. HA_READ_KEY_EXACT.

  @retval HA_ERR_END_OF_FILE  failed to locate data, or to create a row from it
  @retval 0                   success
*/
int cursor_by_error_log::index_next() {
  log_sink_pfs_event *row;
  int ret = HA_ERR_END_OF_FILE;

  log_sink_pfs_read_start();  // obtain read-lock on ring-buffer

  m_pos.set_at(&m_next_pos);  // set current position to next

  while ((row = m_pos.scan_next()) != nullptr) {  // Get row from ring-buffer
    if (m_opened_index->match(row)) {  // If it matches the index-result ...
      if (!make_row(row)) {            // - make a copy of the event
        m_next_pos.set_at(&m_pos);     // - advance read position
        ret = 0;                       // - flag success
        break;                         // We're done.
      }
    }
  }

  log_sink_pfs_read_end();

  return ret;
}
