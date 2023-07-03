/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
  */

#ifndef CURSOR_BY_ERROR_LOG_H
#define CURSOR_BY_ERROR_LOG_H

/**
  @file storage/perfschema/cursor_by_error_log.h
  Cursor CURSOR_BY_ERROR_LOG (declarations);
  PFS_ringbuffer_index, PFS_index_error_log.
*/

#include "sql/server_component/log_sink_perfschema.h"
#include "storage/perfschema/pfs_engine_table.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/table_helper.h"

/**
  Index in the error-log ring-buffer.

  Has a numeric index, a pointer to an event in the buffer,
  and the timestamp for that event.
  This lets us easily check whether this index is still
  valid: its timestamp must be greater than or equal to
  that of the ring-buffer's oldest entry (the tail or
  "read-position"). If our timestamp is older than that
  value, it points to an event that has since expired.
*/
class PFS_ringbuffer_index {
 private:
  /**
    Numeric row index. valid range [0;num_events[. -1 == EOF
  */
  int m_index;
  /**
    Pointer to an event in the ring-buffer.
    Before dereferencing this pointer, use
    @see log_sink_pfs_event_valid()
    to make sure it has not become stale.
  */
  log_sink_pfs_event *m_event;
  /**
    Time-stamp we copied from the event we
    point to when setting this index.
    The ring-buffer keeps track of what the
    oldest item in it is. If our timestamp
    is older than that item's timestamp,
    the event we're pointing too has been
    purged from the ring-buffer, and our
    pointer is stale. 0 for undefined.
  */
  ulonglong m_timestamp;

 public:
  /**
    Reset index.
  */
  void reset() {
    m_index = 0;
    m_event = nullptr;
    m_timestamp = 0;
  }

  /**
    Constructor.
  */
  PFS_ringbuffer_index() { reset(); }

  /**
    Set this index to a given position.
    This copies ``other`` without validating it.

    @param other  set our index from the given one
  */
  void set_at(const PFS_ringbuffer_index *other) {
    m_index = other->m_index;
    m_event = other->m_event;
    m_timestamp = other->m_timestamp;
  }

  /**
    Set our index to the element after the given one
    (if that is valid; otherwise, we cannot determine a next element).

    Caller should hold read-lock on ring-buffer.

    @param other  set our index to the position after the given one
  */
  void set_after(const PFS_ringbuffer_index *other) {
    assert(other != nullptr);

    // special case: ``other`` was reset or is otherwise at index start
    if ((other->m_index == 0) &&
        ((m_event = log_sink_pfs_event_first()) != nullptr) &&
        ((m_event = log_sink_pfs_event_next(m_event)) != nullptr)) {
      m_timestamp = m_event->m_timestamp;  // save timestamp
      m_index = 1;                         // "calculate" new index
      return;
    }

    // if ``other`` is valid and has a successor, use that
    if ((other->m_index != -1) &&  // EOF?
        log_sink_pfs_event_valid(other->m_event, other->m_timestamp) &&
        ((m_event = log_sink_pfs_event_next(other->m_event)) != nullptr)) {
      m_timestamp = m_event->m_timestamp;  // save timestamp
      m_index = other->m_index + 1;        // "calculate" new index
      return;
    }

    // no valid successor found
    reset();       // reset this index
    m_index = -1;  // flag EOF
  }

  /**
    Get event if it's still valid.

    Caller should hold read-lock on ring-buffer.

    If ``m_index`` is 0, the index was reset, and we re-obtain the
    ring-buffer's read-pointer / tail.  This updates m_event and
    m_timestamp.

    If ``m_index`` is -1 (EOF), we return ``nullptr``.

    Otherwise, we try to obtain the event in the ring-buffer pointed
    to by m_event.  If that event is still valid, we return it;
    otherwise, we return nullptr (but leave the stale pointer on
    the object for debugging). It is therefore vital to to determine
    success/failure by checking the retval rather than calling this
    method and then checking m_event directly!

    @retval nullptr    no event (EOF, empty buffer, or stale index)
    @retval !=nullptr  the event this index is referring to
  */
  log_sink_pfs_event *get_event() {
    /*
      Special case: the index was reset.
      Refresh pointer from oldest entry in the ring-buffer.
    */
    if (m_index == 0) {
      m_timestamp =
          ((m_event = log_sink_pfs_event_first()) == nullptr)
              ? 0  // first() failed: buffer empty. zero the timestamp.
              : m_event->m_timestamp;

      /*
        We don't log_sink_pfs_event_valid() the event here.
        We only just got it, and we should be holding a read-lock
        on the ring-buffer, so the event can't have expired.
      */

      return m_event;  // { 0, nullptr, 0 } on empty buffer, otherwise a valid
                       // event
    }

    /*
      If the index is at EOF, or points to an element that has since been
      discarded from the ring-buffer, we have no event we could return.
    */
    if ((m_index == -1) || !log_sink_pfs_event_valid(m_event, m_timestamp))
      return nullptr;  // no event to get

    return m_event;  // return valid event
  }

  /**
    Return current record (if valid), then set index to the next record.

    Caller should hold read-lock on ring-buffer.
    Returned value is only valid as long as the lock is held.

    Note that three states are possible:
    a) an event-pointer is returned, and the index points to a valid
       succeeding event (i.e. is not EOF): there are more elements
    b) an event-pointer is returned, but the index now flags EOF:
       this was the last element
    c) NULL is returned, and the index flags EOF:
       no event could be obtained (the buffer is empty, or we're at EOF)

    Updates m_event, m_timestamp, and m_index.

    @retval   nullptr  no valid record could be obtained (end of buffer etc.)
    @retval !=nullptr  pointer to an entry in the ring-buffer
  */
  log_sink_pfs_event *scan_next(void) {
    log_sink_pfs_event *current_event;

    // Is there a valid current event that we can load into this object?
    if ((current_event = get_event()) !=
        nullptr) {  // save current event if any
      // try to advance index to next event
      if ((m_event = log_sink_pfs_event_next(current_event)) != nullptr) {
        // success. update this index object.
        m_timestamp = m_event->m_timestamp;
        m_index++;
        return current_event;
      }
      // If we get here, current_event is valid, but has no successor.
    }

    // Index now points to an invalid event. Flag EOF.
    m_event = nullptr;
    m_timestamp = 0;
    m_index = -1;

    // last event in buffer (if get_event() succeeded), or NULL otherwise.
    return current_event;
  }
};

typedef PFS_ringbuffer_index pos_t;

/**
  @addtogroup performance_schema_tables
  @{
*/

/** Generic index for error_log table. Used by cursor class for open index. */
class PFS_index_error_log : public PFS_engine_index {
 public:
  explicit PFS_index_error_log(PFS_engine_key *key) : PFS_engine_index(key) {}

  ~PFS_index_error_log() = default;

  virtual bool match(log_sink_pfs_event *row) = 0;
};

/** Cursor CURSOR_BY_ERROR_LOG for error_log table. */
class cursor_by_error_log : public PFS_engine_table {
 public:
  static ha_rows get_row_count();

  virtual void reset_position(void) override;

  virtual int rnd_next() override;
  virtual int rnd_pos(const void *pos) override;

  virtual int index_next() override;

 protected:
  explicit cursor_by_error_log(const PFS_engine_table_share *share);

 public:
  ~cursor_by_error_log() override = default;

 protected:
  virtual int make_row(log_sink_pfs_event *row) = 0;

 private:
  /** Current position. */
  pos_t m_pos;
  /** Next position. */
  pos_t m_next_pos;

 protected:
  PFS_index_error_log *m_opened_index;
};

/** @} */
#endif
