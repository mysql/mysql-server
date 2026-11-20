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

#pragma once
#include <cstdint>
#include <memory>
#include "duckdb_manager.h"
#include "field_types.h"

namespace duckdb {
class ClientContext;
class QueryResult;
class Connection;
}  // namespace duckdb

class THD;
struct CHARSET_INFO;

//===--------------------------------------------------------------------===//
// Functions to execute query.
//===--------------------------------------------------------------------===//
namespace myduck {
struct result_template {
  enum_field_types type;
  bool is_unsigned;
  uint8_t decimals;
  const CHARSET_INFO *cs;
};

std::unique_ptr<duckdb::QueryResult> duckdb_query(THD *thd,
                                                  const std::string &query,
                                                  bool need_config = true,
                                                  bool keep_schema = true);

std::unique_ptr<duckdb::QueryResult> duckdb_query(
    duckdb::ClientContext &context, const std::string &query,
    bool keep_schema = true);

std::unique_ptr<duckdb::QueryResult> duckdb_query(
    duckdb::Connection &connection, const std::string &query,
    bool keep_schema = true);

std::unique_ptr<duckdb::QueryResult> duckdb_query(const std::string &query,
                                                  bool keep_schema = true);

bool duckdb_query_and_send(THD *thd, const std::string &query, bool send_result,
                           bool push_error);

void duckdb_send_result(THD *thd, duckdb::QueryResult &result);

std::string BytesToHumanReadableString(uint64_t bytes,
                                       uint64_t multiplier = 1024);
}  // namespace myduck
