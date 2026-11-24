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

#ifndef SQL_DUCKDB_LOG_INCLUDED
#define SQL_DUCKDB_LOG_INCLUDED

#include <my_inttypes.h>
#include <cstdint>

namespace myduck {
extern ulonglong duckdb_log_options;

enum enum_duckdb_log_types {
  DUCKDB_MULTI_TRX_BATCH_COMMIT,
  DUCKDB_MULTI_TRX_BATCH_DETAIL,
  DUCKDB_QUERY,
  DUCKDB_QUERY_RESULT
};

extern const char *duckdb_log_types[];

#define LOG_DUCKDB_MULTI_TRX_BATCH_COMMIT \
  (1ULL << myduck::enum_duckdb_log_types::DUCKDB_MULTI_TRX_BATCH_COMMIT)
#define LOG_DUCKDB_MULTI_TRX_BATCH_DETAIL \
  (1ULL << myduck::enum_duckdb_log_types::DUCKDB_MULTI_TRX_BATCH_DETAIL)
#define LOG_DUCKDB_QUERY (1ULL << myduck::enum_duckdb_log_types::DUCKDB_QUERY)
#define LOG_DUCKDB_QUERY_RESULT \
  (1ULL << myduck::enum_duckdb_log_types::DUCKDB_QUERY_RESULT)

bool log_duckdb_multi_trx_batch_commit(const char *reason);

bool log_duckdb_apply_event_type(const char *type);

bool log_duckdb_gtid(const char *prefix, int type, int sidno, int64_t gno);
}  // namespace myduck

#endif  // SQL_DUCKDB_LOG_INCLUDED
