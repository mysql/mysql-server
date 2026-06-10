/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "tm_log.h"

/*
  In include/mysql/components/services/log_builtins.h,
  the helper macros require these two globals.
*/
SERVICE_TYPE(log_builtins) * log_bi{nullptr};
SERVICE_TYPE(log_builtins_string) * log_bs{nullptr};

namespace telemetry {

void Log::init(SERVICE_TYPE(log_builtins) * log_bi_srv,
               SERVICE_TYPE(log_builtins_string) * log_bs_srv) {
  log_bi = log_bi_srv;
  log_bs = log_bs_srv;
}

void Log::log_message(const char *src_file, int src_line, long long level,
                      long long code, const char *msg, ...) {
  va_list args;
  va_start(args, msg);
  log_message_va(src_file, src_line, level, code, msg, args);
  va_end(args);
}

}  // namespace telemetry
