/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#pragma once

#include <string>
#include <vector>

namespace hm {

typedef std::string Field;
typedef std::vector<std::string> Row;
typedef std::vector<Row> Result_Set;

struct Query {
  Query() = default;
  explicit Query(const char *query_text) : query{query_text} {}
  explicit Query(std::string query_text) : query{std::move(query_text)} {}
  Query(std::string query_title, std::string query_text)
      : title{std::move(query_title)}, query{std::move(query_text)} {}

  const std::string title;
  const std::string query;
  int run(Result_Set &r) const;
};

/**
  Query processor
*/
class Query_Manager {
 public:
  Query_Manager() = default;
  ~Query_Manager() = default;
  Query_Manager(const Query_Manager &) = delete;
  Query_Manager &operator=(const Query_Manager &) = delete;
  Query_Manager(Query_Manager &&) = delete;
  Query_Manager &operator=(Query_Manager &&) = delete;

  static int run_query(const Query &query_info, Result_Set &result_set);
  static int run_query(const Query &&query_info, Result_Set &result_set);
  static int run_query(const Query &query_info);
  static int run_query(const Query &&query_info);
};

}  // namespace hm
