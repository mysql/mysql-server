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

#ifndef LOG_SINK_PERFSCHEMA_IMP_H
#define LOG_SINK_PERFSCHEMA_IMP_H

#include <mysql/components/service_implementation.h>

/**
  Primitives for logging services to add to performance_schema.error_log.
*/
class log_sink_perfschema_imp {
 public: /* Service Implementations */
  static DEFINE_METHOD(log_service_error, event_add,
                       (ulonglong timestamp, ulonglong thread_id, ulong prio,
                        const char *error_code, uint error_code_length,
                        const char *subsys, uint subsys_length,
                        const char *message, uint message_length));
};

#endif /* LOG_SINK_PERFSCHEMA_IMP_H */
