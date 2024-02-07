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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  This defines functions to let logging services add error log events
  to performance_schema.error_log.

  For documentation of the individual functions, see log_sink_perfschema.cc
*/

#ifndef LOG_SINK_PERFSCHEMA_H
#define LOG_SINK_PERFSCHEMA_H

#include <mysql/components/service_implementation.h>

/**
  Primitives for logging services to add to performance_schema.error_log.
*/
BEGIN_SERVICE_DEFINITION(log_sink_perfschema)

/**
  Add a log-event to the ring buffer.

  We require the various pieces of information to be passed individually
  rather than accepting a log_sink_pfs_event so we can sanity check each
  part individually and don't have to worry about different components
  using different versions/sizes of the struct.

  We copy the data as needed, so caller may free their copy once this
  call returns.

  @param timestamp          Timestamp (in microseconds), or
                            0 to have one generated
  @param thread_id          thread_id of the thread that detected
                            the issue
  @param prio               (INFORMATION|WARNING|ERROR|SYSTEM)_LEVEL
  @param error_code         MY-123456
  @param error_code_length  length in bytes of error_code
  @param subsys             Subsystem ("InnoDB", "Server", "Repl")
  @param subsys_length      length in bytes of subsys
  @param message            data field (error message/JSON record/...)
  @param message_length     length of data field

  @retval LOG_SERVICE_SUCCESS                success
  @retval LOG_SERVICE_ARGUMENT_TOO_LONG      argument too long
  @retval LOG_SERVICE_INVALID_ARGUMENT       invalid argument
*/
DECLARE_METHOD(log_service_error, event_add,
               (ulonglong timestamp, ulonglong thread_id, ulong prio,
                const char *error_code, uint error_code_length,
                const char *subsys, uint subsys_length, const char *message,
                uint message_length));

END_SERVICE_DEFINITION(log_sink_perfschema)

#endif
