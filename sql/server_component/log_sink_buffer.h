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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file log_sink_buffer.h

  This file contains

  a) the API for the log-sink that buffers errors logged during start-up
  so they can be flushed once all configured log-components have become
  available;

  b) the API for querying and setting the phase the server is in
  with regard to logging (buffering, normal operation, and so forth);

  c) the API for flushing the buffered information (to force writing
  out this information in cases of early shutdowns and so on).
*/

#ifndef LOG_SINK_BUFFER_H
#define LOG_SINK_BUFFER_H

#include "log_builtins_internal.h"
#include "my_sys.h"

extern mysql_mutex_t THR_LOCK_log_buffered;  // let log_builtins init/destroy

enum log_sink_buffer_flush_mode {
  LOG_BUFFER_DISCARD_ONLY,        ///< discard all buffered log-events
  LOG_BUFFER_PROCESS_AND_DISCARD  ///< process+discard buffered log-events
};

enum log_error_stage {
  LOG_ERROR_STAGE_BUFFERING,           ///< no log-destination yet
  LOG_ERROR_STAGE_COMPONENTS,          ///< external services available
  LOG_ERROR_STAGE_COMPONENTS_AND_PFS,  ///< full logging incl. to pfs
  LOG_ERROR_STAGE_SHUTTING_DOWN        ///< no external components
};

struct log_line_buffer {
  log_line ll;            ///< log-event we're buffering
  log_line_buffer *next;  ///< chronologically next log-event
};

/// Set error-logging stage hint (e.g. are loadable services available yet?).
void log_error_stage_set(enum log_error_stage les);

/// What mode is error-logging in (e.g. are loadable services available yet)?
enum log_error_stage log_error_stage_get(void);

/// Write a log-event to the buffer sink.
int log_sink_buffer(void *instance [[maybe_unused]], log_line *ll);

/**
  Release all buffered log-events (discard_error_log_messages()),
  optionally after running them through the error log stack first
  (flush_error_log_messages()). Safe to call repeatedly (though
  subsequent calls will only output anything if further events
  occurred after the previous flush).

  @param  mode  LOG_BUFFER_DISCARD_ONLY (to just
                throw away the buffered events), or
                LOG_BUFFER_PROCESS_AND_DISCARD to
                filter/print them first
*/
void log_sink_buffer_flush(enum log_sink_buffer_flush_mode mode);

/**
  Prepend a list of log-events to the already buffered events.

  @param[in]  head  Head of the list to prepend to the main list
  @param[out] tail  Pointer to the `next` pointer in last element to prepend
*/
void log_sink_buffer_prepend_list(log_line_buffer *head,
                                  log_line_buffer **tail);

#endif /* LOG_SINK_BUFFER_H */
