/* Copyright (c) 2023, 2024 Oracle and/or its affiliates.

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

#ifndef SERVER_TELEMETRY_LOGS_BITS_H
#define SERVER_TELEMETRY_LOGS_BITS_H

#include <time.h>   // time_t
#include <cstddef>  // size_t
#include <cstdint>  // uint64_t

#include "server_telemetry_logs_client_bits.h"

typedef void (*log_delivery_callback_t)(const char *logger_name,
                                        OTELLogLevel severity,
                                        const char *message, time_t timestamp,
                                        const log_attribute_t *attr_array,
                                        size_t attr_count);

/**
  Register telemetry logger callback.

  @param logger pointer to callback function to be registered.
  @retval FALSE: success
  @retval TRUE: failure
*/
typedef bool (*register_telemetry_logger_v1_t)(log_delivery_callback_t logger);

/**
  Unregister telemetry logger callback.

  @param logger pointer to callback function to be unregistered.
  @retval FALSE: success
  @retval TRUE: failure
*/
typedef bool (*unregister_telemetry_logger_v1_t)(
    log_delivery_callback_t logger);

/**
  Wrapper method to notify telemetry logger callback (if registered) of new log
  event.
*/
typedef void (*notify_telemetry_logger_v1_t)(
    PSI_logger *logger, OTELLogLevel level, const char *message,
    time_t timestamp, const log_attribute_t *attr_array, size_t attr_count);

#endif /* SERVER_TELEMETRY_LOGS_BITS_H */
