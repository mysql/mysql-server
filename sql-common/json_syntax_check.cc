/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "sql-common/json_syntax_check.h"

#include <assert.h>
#include <string>
#include <utility>

#include "my_rapidjson_size_t.h"  // IWYU pragma: keep

#include <rapidjson/error/en.h>
#include <rapidjson/error/error.h>
#include <rapidjson/memorystream.h>
#include <rapidjson/reader.h>

bool Syntax_check_handler::StartObject() {
  m_too_deep_error_raised = check_json_depth(++m_depth, m_depth_handler);
  return !m_too_deep_error_raised;
}

bool Syntax_check_handler::EndObject(rapidjson::SizeType) {
  --m_depth;
  return true;
}

bool Syntax_check_handler::StartArray() {
  m_too_deep_error_raised = check_json_depth(++m_depth, m_depth_handler);
  return !m_too_deep_error_raised;
}

bool Syntax_check_handler::EndArray(rapidjson::SizeType) {
  --m_depth;
  return true;
}
Syntax_check_handler::Syntax_check_handler(JsonErrorHandler m_depth_handler)
    : m_depth_handler(std::move(m_depth_handler)) {}

bool is_valid_json_syntax(const char *text, size_t length, size_t *error_offset,
                          std::string *error_message,
                          const JsonErrorHandler &depth_handler) {
  Syntax_check_handler handler(depth_handler);
  rapidjson::Reader reader;
  rapidjson::MemoryStream ms(text, length);
  const bool valid = reader.Parse<rapidjson::kParseDefaultFlags>(ms, handler);

  if (!valid && (error_offset != nullptr || error_message != nullptr)) {
    const std::pair<std::string, size_t> error = get_error_from_reader(reader);

    if (error_offset != nullptr) {
      *error_offset = error.second;
    }
    if (error_message != nullptr) {
      error_message->assign(error.first);
    }
  }

  return valid;
}

/// The maximum number of nesting levels allowed in a JSON document.
static constexpr int JSON_DOCUMENT_MAX_DEPTH = 100;

bool check_json_depth(size_t depth, const JsonErrorHandler &handler) {
  if (depth > JSON_DOCUMENT_MAX_DEPTH) {
    handler();
    return true;
  }
  return false;
}

bool check_json_depth(size_t depth,
                      const JsonSerializationErrorHandler &handler) {
  return check_json_depth(depth, [&handler]() { handler.TooDeep(); });
}

std::pair<std::string, size_t> get_error_from_reader(
    const rapidjson::Reader &reader) {
  assert(reader.GetParseErrorCode() != rapidjson::kParseErrorNone);
  return std::make_pair(
      std::string(rapidjson::GetParseError_En(reader.GetParseErrorCode())),
      reader.GetErrorOffset());
}
