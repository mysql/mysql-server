/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

#include "sql/duckdb/duckdb_log.h"
#include "mysql/components/services/log_builtins.h"
#include "mysqld_error.h"

namespace myduck {

ulonglong duckdb_log_options = 0;

const char *duckdb_log_types[] = {
    "DUCKDB_MULTI_TRX_BATCH_COMMIT", "DUCKDB_MULTI_TRX_BATCH_DETAIL",
    "DUCKDB_QUERY", "DUCKDB_QUERY_RESULT", nullptr};

bool log_duckdb_multi_trx_batch_commit(const char *reason) {
  const char *format = "commit duckdb batch due to %s";
  char type_specific_message[LOG_BUFF_MAX];
  int specific_message_length =
      snprintf(type_specific_message, LOG_BUFF_MAX, format, reason);

  if (specific_message_length < 0) return true;

  LogErr(INFORMATION_LEVEL, ER_DUCKDB, type_specific_message);

  return false;
}

bool log_duckdb_apply_event_type(const char *type) {
  const char *format = "apply event, type = %s";
  char type_specific_message[LOG_BUFF_MAX];
  int specific_message_length =
      snprintf(type_specific_message, LOG_BUFF_MAX, format, type);

  if (specific_message_length < 0) return true;

  LogErr(INFORMATION_LEVEL, ER_DUCKDB, type_specific_message);

  return false;
}

bool log_duckdb_gtid(const char *prefix, int type, int sidno, int64_t gno) {
  const char *format = "%s, type = %d, sidno = %d, gno = %lld";
  char type_specific_message[LOG_BUFF_MAX];
  int specific_message_length = snprintf(type_specific_message, LOG_BUFF_MAX,
                                         format, prefix, type, sidno, gno);

  if (specific_message_length < 0) return true;

  LogErr(INFORMATION_LEVEL, ER_DUCKDB, type_specific_message);

  return false;
}
}  // namespace myduck
