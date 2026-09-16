/* Copyright (c) 2026, Oracle and/or its affiliates.

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
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef MYSQL_COMMAND_SESSION_STATE_H
#define MYSQL_COMMAND_SESSION_STATE_H

#include <mysql/components/service.h>
#include <mysql/components/services/bits/mysql_command_session_state_bits.h>
#include <mysql/components/services/mysql_command_services.h>
#include <stddef.h>

/**
  @ingroup group_components_services_inventory

  Provide iterator access to session-tracker data cached in the MYSQL handle
  used by command services.

  init() creates an iterator snapshot for the requested tracker type from the
  cached session-tracker data of the most recent command executed through this
  MYSQL_H. The iterator owns its traversal state and must be released with
  deinit().

  get_next() copies data into caller-provided storage and returns the actual
  item length through @p length. If @p buffer is NULL or @p buffer_length is too
  small, MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL is returned and the
  iterator is not advanced.

  The cache is populated only when the active text consumer advertises
  CLIENT_SESSION_TRACK.

  For MYSQL_COMMAND_SESSION_TRACK_SYSTEM_VARIABLES, iteration returns
  alternating variable name and variable value entries, matching
  mysql_session_track_get_first() and mysql_session_track_get_next().
*/
DEFINE_SERVICE_HANDLE(MYSQL_COMMAND_SESSION_STATE_ITERATOR_H);

BEGIN_SERVICE_DEFINITION(mysql_command_session_state)

/**
  Initializes an iterator for a session-tracker type.

  @param      mysql_h  Command-service handle.
  @param      type     Session-tracker type to read.
  @param[out] iterator Iterator handle.

  @return
    @retval MYSQL_COMMAND_SESSION_STATE_OK Success. @p iterator must be released
            with deinit().
    @retval MYSQL_COMMAND_SESSION_STATE_END No item available for this tracker
            type.
    @retval MYSQL_COMMAND_SESSION_STATE_INVALID Invalid input.
    @retval MYSQL_COMMAND_SESSION_STATE_ERROR Internal error.
*/
DECLARE_METHOD(mysql_command_session_state_status, init,
               (MYSQL_H mysql_h, mysql_command_session_state_type type,
                MYSQL_COMMAND_SESSION_STATE_ITERATOR_H *iterator));

/**
  Copies the next iterator item into a caller-provided buffer.

  @param      iterator      Iterator handle returned by init().
  @param[out] buffer        Output buffer, or NULL to query actual item length.
  @param      buffer_length Output buffer length in bytes.
  @param[out] length        Actual item length in bytes.

  @return
    @retval MYSQL_COMMAND_SESSION_STATE_OK Success. The iterator advances.
    @retval MYSQL_COMMAND_SESSION_STATE_END No further item is available.
    @retval MYSQL_COMMAND_SESSION_STATE_INVALID Invalid input.
    @retval MYSQL_COMMAND_SESSION_STATE_BUFFER_TOO_SMALL The buffer is NULL or
            too small. @p length contains the actual item length. The iterator
            is not advanced.
    @retval MYSQL_COMMAND_SESSION_STATE_ERROR Internal error.
*/
DECLARE_METHOD(mysql_command_session_state_status, get_next,
               (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator, char *buffer,
                size_t buffer_length, size_t *length));

/**
  Releases an iterator returned by init().

  @param iterator Iterator handle.
*/
DECLARE_METHOD(void, deinit, (MYSQL_COMMAND_SESSION_STATE_ITERATOR_H iterator));

END_SERVICE_DEFINITION(mysql_command_session_state)

#endif /* MYSQL_COMMAND_SESSION_STATE_H */
