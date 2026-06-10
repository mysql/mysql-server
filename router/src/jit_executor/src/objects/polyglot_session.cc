/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "objects/polyglot_session.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "mysqlrouter/jit_executor_db_interface.h"

#include "objects/polyglot_result.h"
#include "router/src/router/include/mysqlrouter/utils_sqlstring.h"
#include "utils/utils_string.h"

namespace shcore {
namespace polyglot {

namespace {
std::string sub_query_placeholders(const std::string &query,
                                   const shcore::Array_t &args) {
  if (!args) return query;

  mysqlrouter::sqlstring squery(query.c_str(), 0);
  int i = 0;
  for (const shcore::Value &value : *args) {
    try {
      switch (value.get_type()) {
        case shcore::Integer:
          squery << value.as_int();
          break;
        case shcore::Bool:
          squery << value.as_bool();
          break;
        case shcore::Float:
          squery << value.as_double();
          break;
        case shcore::Binary:
          squery << value.get_string();
          break;
        case shcore::String:
          squery << value.get_string();
          break;
        case shcore::Null:
          squery << nullptr;
          break;
        default:
          throw std::invalid_argument(shcore::str_format(
              "Invalid type for placeholder value at index #%i", i));
      }
    } catch (const std::exception &e) {
      throw std::invalid_argument(shcore::str_format(
          "%s while substituting placeholder value at index #%i", e.what(), i));
    }
    ++i;
  }

  try {
    return squery.str();
  } catch (const std::exception &) {
    throw std::invalid_argument(
        "Insufficient number of values for placeholders in query");
  }

  return query;
}

const constexpr char *k_run_sql = "runSql";
}  // namespace

std::vector<std::string> Session::m_methods = {k_run_sql};

Session::Session(const std::shared_ptr<jit_executor::db::ISession> &session)
    : m_session{session} {}

//! Calls the named method with the given args
Value Session::call(const std::string &name, const Argument_list &args) {
  if (name == k_run_sql) {
    return shcore::Value(run_sql(args));
  }

  return {};
}

void Session::reset() { m_session->reset(); }

std::shared_ptr<PolyResult> Session::run_sql(const Argument_list &args) {
  auto query = args[0].as_string();
  if (args.size() > 1) {
    query = sub_query_placeholders(query, args[1].as_array());
  }

  return std::make_shared<PolyResult>(m_session->run_sql(query));
}

}  // namespace polyglot
}  // namespace shcore
