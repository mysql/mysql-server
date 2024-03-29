/*
 * Copyright (c) 2017, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_RESULT_FETCHER_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_RESULT_FETCHER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "plugin/x/client/mysqlxclient/xquery_result.h"
#include "plugin/x/client/mysqlxclient/xrow.h"
#include "plugin/x/tests/driver/connector/warning.h"

class Result_fetcher {
 public:
  using XQuery_result_ptr = std::unique_ptr<xcl::XQuery_result>;

 public:
  explicit Result_fetcher(XQuery_result_ptr query)
      : m_query(std::move(query)) {}

  void set_metadata(const std::vector<xcl::Column_metadata> &metadata) {
    m_query->set_metadata(metadata);
  }

  std::vector<xcl::Column_metadata> column_metadata() {
    if (m_error) return {};

    return m_query->get_metadata(&m_error);
  }

  const xcl::XRow *next() {
    if (m_cached_row) {
      auto result = m_cached_row;

      m_cached_row = nullptr;

      return result;
    }

    if (m_error) return nullptr;

    return m_query->get_next_row(&m_error);
  }

  bool next_data_set() {
    /* Skip empty resultsets */
    if (m_query->next_resultset(&m_error)) {
      m_cached_row = m_query->get_next_row(&m_error);

      return true;
    }

    return false;
  }

  xcl::XError get_last_error() const { return m_error; }

  bool is_out_params() const { return m_query->is_out_parameter_resultset(); }

  int64_t last_insert_id() const {
    uint64_t result;
    if (!m_query->try_get_last_insert_id(&result)) return -1;

    return result;
  }

  int64_t affected_rows() const {
    uint64_t result;
    if (!m_query->try_get_affected_rows(&result)) return -1;

    return result;
  }

  std::string info_message() const {
    std::string result;

    m_query->try_get_info_message(&result);

    return result;
  }

  std::vector<std::string> generated_document_ids() const {
    std::vector<std::string> result;
    m_query->try_get_generated_document_ids(&result);
    return result;
  }

  const std::vector<Warning> get_warnings() const {
    std::vector<Warning> result;

    if (nullptr == m_query) return {};

    for (const auto &warning : m_query->get_warnings()) {
      result.emplace_back(
          warning.msg(), warning.code(),
          warning.level() == ::Mysqlx::Notice::Warning_Level_NOTE);
    }

    return result;
  }

 private:
  XQuery_result_ptr m_query;
  xcl::XError m_error;
  const xcl::XRow *m_cached_row{nullptr};
};

std::ostream &operator<<(std::ostream &os, const xcl::Column_metadata &meta);

std::ostream &operator<<(std::ostream &os,
                         const std::vector<xcl::Column_metadata> &meta);

std::ostream &operator<<(std::ostream &os, Result_fetcher *result);

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_RESULT_FETCHER_H_
