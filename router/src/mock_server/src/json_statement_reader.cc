/*
  Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "json_statement_reader.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
// if we build within the server, it will set RAPIDJSON_NO_SIZETYPEDEFINE
// globally and require to include my_rapidjson_size_t.h
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/schema.h>
#include <rapidjson/stringbuffer.h>
#include <cassert>
#include <cerrno>
#include <chrono>
#include <memory>
#include <stdexcept>

#include "mysql_server_mock_schema.h"

#ifdef _WIN32
#include <regex>
#else
#include <regex.h>
#endif
#include "mysql_protocol_encoder.h"

namespace {

// default allocator for rapidJson (MemoryPoolAllocator) is broken for
// SparcSolaris
using JsonDocument =
    rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonValue =
    rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
using JsonSchemaDocument =
    rapidjson::GenericSchemaDocument<JsonValue, rapidjson::CrtAllocator>;
using JsonSchemaValidator =
    rapidjson::GenericSchemaValidator<JsonSchemaDocument>;

std::string get_json_value_as_string(const JsonValue &value,
                                     size_t repeat = 1) {
  if (value.IsString()) {
    const std::string val = value.GetString();
    std::string result;
    if (val.empty()) return val;
    result.reserve(val.length() * repeat);

    for (size_t i = 0; i < repeat; ++i) {
      result += val;
    }

    return result;
  }
  if (value.IsNull()) return "";
  if (value.IsInt()) return std::to_string(value.GetInt());
  if (value.IsUint()) return std::to_string(value.GetUint());
  if (value.IsDouble()) return std::to_string(value.GetDouble());
  // TODO: implement other types when needed

  throw(std::runtime_error("Unsupported json value type: " +
                           std::to_string(static_cast<int>(value.GetType()))));
}

std::string get_json_string_field(const JsonValue &parent,
                                  const std::string &field,
                                  const std::string &default_val = "",
                                  bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    harness_assert(!required);  // schema should have caught this
    return default_val;
  }

  harness_assert(
      parent[field.c_str()].IsString());  // schema should have caught this

  return parent[field.c_str()].GetString();
}

double get_json_double_field(const JsonValue &parent, const std::string &field,
                             const double default_val = 0.0,
                             bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    harness_assert(!required);  // schema should have caught this
    return default_val;
  }

  harness_assert(
      parent[field.c_str()].IsDouble());  // schema should have caught this
  return parent[field.c_str()].GetDouble();
}

template <class INT_TYPE>
INT_TYPE get_json_integer_field(const JsonValue &parent,
                                const std::string &field,
                                const INT_TYPE default_val = 0,
                                bool required = false) {
  const bool found = parent.HasMember(field.c_str());
  if (!found) {
    harness_assert(!required);  // schema should have caught this
    return default_val;
  }

  harness_assert(
      parent[field.c_str()].IsInt());  // schema should have caught this

  return static_cast<INT_TYPE>(parent[field.c_str()].GetInt());
}

}  // namespace

namespace server_mock {

struct QueriesJsonReader::Pimpl {
  mysql_protocol::Capabilities::Flags server_capabilities_;

  JsonDocument json_document_;
  size_t current_stmt_{0u};

  // load queries JSON; throws std::runtime_error on invalid JSON file
  Pimpl(const std::string &json_filename)
      : json_document_(load_json_from_file(json_filename)) {}

  static JsonDocument load_json_from_file(const std::string &filename);
  static void validate_json_against_schema(const JsonSchemaDocument &schema,
                                           const JsonDocument &json);

  std::unique_ptr<ResultsetResponse> read_result_info(const JsonValue &stmt);
  std::unique_ptr<Response> read_ok_info(const JsonValue &stmt);
  std::unique_ptr<Response> read_error_info(const JsonValue &stmt);
};

QueriesJsonReader::QueriesJsonReader(const std::string &json_filename)
    : pimpl_(new Pimpl(json_filename)) {
  // construct schema JSON; throws std::runtime_error on invalid JSON, but note
  // that invalid schema will slip by without throwing (but it will cause
  // validate_json_against_schema() to fail later on)
  JsonDocument schema_json;
  if (schema_json
          .Parse<rapidjson::kParseCommentsFlag>(SqlQueryJsonSchema::data(),
                                                SqlQueryJsonSchema::size())
          .HasParseError())
    throw std::runtime_error(
        "Parsing JSON schema failed at offset " +
        std::to_string(schema_json.GetErrorOffset()) + ": " +
        rapidjson::GetParseError_En(schema_json.GetParseError()));
  JsonSchemaDocument schema(schema_json);

  // validate JSON against schema; throws std::runtime if validation fails
  try {
    pimpl_->validate_json_against_schema(schema, pimpl_->json_document_);
  } catch (const std::runtime_error &e) {
    // TODO: we could also get here if schema itself is not valid. To diagnose
    // that,
    //       another validate_json_against_schema() could be ran here to
    //       validate our schema against schema spec
    //       (http://json-schema.org/draft-04/schema#)

    throw std::runtime_error("JSON file '" + json_filename +
                             "' failed validation against JSON schema:\n" +
                             e.what());
  }

  // schema should have caught these
  harness_assert(pimpl_->json_document_.HasMember("stmts"));
  harness_assert(pimpl_->json_document_["stmts"].IsArray());
}

// this is needed for pimpl, otherwise compiler complains
// about pimpl unknown size in std::unique_ptr
QueriesJsonReader::~QueriesJsonReader() = default;

// throws std::runtime_error on
// - file read error
// - JSON parse error
/*static*/ JsonDocument QueriesJsonReader::Pimpl::load_json_from_file(
    const std::string &filename) {
  // This DOES NOT have to be big enough to contain the entire JSON file.
  // In such case, FileReadStream will automatically read more from the file
  // once it reaches end of buffer.
  constexpr size_t kMaxFileSize = 64 * 1024;

// open JSON file
#ifndef _WIN32
  FILE *fp = fopen(filename.c_str(), "rb");  // after rapidjson doc
#else
  FILE *fp = fopen(filename.c_str(), "r");
#endif
  if (!fp) {
    throw std::runtime_error("Could not open JSON file '" + filename +
                             "' for reading: " + strerror(errno));
  }
  std::shared_ptr<void> exit_guard(nullptr, [&](void *) { fclose(fp); });

  // read JSON file
  char readBuffer[kMaxFileSize];
  rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

  // ensure file is a valid JSON file
  JsonDocument json;
  if (json.ParseStream<rapidjson::kParseCommentsFlag>(is).HasParseError()) {
    throw std::runtime_error("Parsing JSON file '" + filename +
                             "' failed at offset " +
                             std::to_string(json.GetErrorOffset()) + ": " +
                             rapidjson::GetParseError_En(json.GetParseError()));
  }

  return json;
}

// throws std::runtime_error on failed validation
/*static*/
void QueriesJsonReader::Pimpl::validate_json_against_schema(
    const JsonSchemaDocument &schema, const JsonDocument &json) {
  // verify JSON against the schema
  JsonSchemaValidator validator(schema);
  if (!json.Accept(validator)) {
    // validation failed - throw an error with info of where the problem is
    rapidjson::StringBuffer sb_schema;
    validator.GetInvalidSchemaPointer().StringifyUriFragment(sb_schema);
    rapidjson::StringBuffer sb_json;
    validator.GetInvalidDocumentPointer().StringifyUriFragment(sb_json);
    throw std::runtime_error(
        std::string("Failed schema directive: ") + sb_schema.GetString() +
        "\nFailed schema keyword:   " + validator.GetInvalidSchemaKeyword() +
        "\nFailure location in validated document: " + sb_json.GetString() +
        "\n");
  }
}

namespace {

bool pattern_matching(const std::string &s, const std::string &pattern) {
#ifndef _WIN32
  regex_t regex;
  auto r = regcomp(&regex, pattern.c_str(), REG_EXTENDED);
  if (r) {
    throw std::runtime_error("Error compiling regex pattern: " + pattern);
  }
  r = regexec(&regex, s.c_str(), 0, NULL, 0);
  regfree(&regex);
  return (r == 0);
#else
  std::regex regex(pattern);
  return std::regex_match(s, regex);
#endif
}

}  // unnamed namespace

constexpr char kAuthCachingSha2Password[] = "caching_sha2_password";
constexpr char kAuthNativePassword[] = "mysql_native_password";

HandshakeResponse QueriesJsonReader::handle_handshake_init(
    const std::vector<uint8_t> &, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  // defaults
  std::string server_version = "8.0.5-mock";
  uint32_t connection_id = 0;
  mysql_protocol::Capabilities::Flags server_capabilities =
      mysql_protocol::Capabilities::PROTOCOL_41 |
      mysql_protocol::Capabilities::PLUGIN_AUTH |
      mysql_protocol::Capabilities::SECURE_CONNECTION;
  uint16_t status_flags = 0;
  uint8_t character_set = 0;
  std::string auth_method = kAuthNativePassword;
  std::string auth_data = "01234567890123456789";

  if (pimpl_->json_document_.HasMember("handshake")) {
    const JsonValue &handshake_json = pimpl_->json_document_["handshake"];

    if (handshake_json.HasMember("greeting")) {
      const JsonValue &greeting_json = handshake_json["greeting"];
      double exec_time = get_json_double_field(greeting_json, "exec_time", 0.0);
      response.exec_time =
          std::chrono::microseconds(static_cast<long>(exec_time * 1000));
    }
  }

  // remember the server side capabilties
  pimpl_->server_capabilities_ = server_capabilities;

  response.response_type = HandshakeResponse::ResponseType::GREETING;
  response.response = std::unique_ptr<Greeting>{
      new Greeting(server_version, connection_id, server_capabilities,
                   status_flags, character_set, auth_method, auth_data)};

  next_state = HandshakeState::GREETED;

  return response;
}

HandshakeResponse QueriesJsonReader::handle_handshake_greeted(
    const std::vector<uint8_t> &payload, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  // decode the payload

  // prepend length of packet again as HandshakeResponsePacket parser
  // expects a full frame, not the payload
  std::vector<uint8_t> frame{0, 0, 0, 1};
  frame.insert(frame.end(), payload.begin(), payload.end());
  for (unsigned int i = 0, sz = payload.size(); i < 3; i++, sz >>= 8) {
    frame[i] = sz % 0xff;
  }

  mysql_protocol::HandshakeResponsePacket pkt(frame);

  pkt.parse_payload(pimpl_->server_capabilities_);

  // default: OK the auth or switch to sha256

  if (pkt.get_auth_plugin() == kAuthCachingSha2Password) {
    response.response_type = HandshakeResponse::ResponseType::AUTH_SWITCH;
    response.response = std::unique_ptr<AuthSwitch>{
        new AuthSwitch(kAuthCachingSha2Password, "123456789|ABCDEFGHI|")};

    next_state = HandshakeState::AUTH_SWITCHED;
  } else if (pkt.get_auth_plugin() == kAuthNativePassword) {
    response.response_type = HandshakeResponse::ResponseType::OK;
    response.response = std::unique_ptr<OkResponse>{new OkResponse()};

    next_state = HandshakeState::DONE;
  } else {
    response.response_type = HandshakeResponse::ResponseType::ERROR;
    response.response = std::unique_ptr<ErrorResponse>{
        new ErrorResponse(0, "unknown auth-method")};

    next_state = HandshakeState::DONE;
  }

  return response;
}

HandshakeResponse QueriesJsonReader::handle_handshake_auth_switched(
    const std::vector<uint8_t> &, HandshakeState &next_state) {
  HandshakeResponse response;

  response.exec_time = get_default_exec_time();

  // switched to sha256
  //
  // for now, ignore the payload and send the fast-auth ticket

  response.response_type = HandshakeResponse::ResponseType::AUTH_FAST;
  response.response = std::unique_ptr<AuthFast>{new AuthFast()};

  next_state = HandshakeState::DONE;

  return response;
}

HandshakeResponse QueriesJsonReader::handle_handshake(
    const std::vector<uint8_t> &payload) {
  switch (handshake_state_) {
    case HandshakeState::INIT:
      return handle_handshake_init(payload, handshake_state_);
    case HandshakeState::GREETED:
      return handle_handshake_greeted(payload, handshake_state_);
    case HandshakeState::AUTH_SWITCHED:
      return handle_handshake_auth_switched(payload, handshake_state_);
    default: {
      HandshakeResponse response;

      response.response_type = HandshakeResponse::ResponseType::ERROR;
      response.response = std::unique_ptr<ErrorResponse>{
          new ErrorResponse(0, "wrong handshake state")};

      handshake_state_ = HandshakeState::DONE;
      return response;
    }
  }
}

StatementResponse QueriesJsonReader::handle_statement(
    const std::string &statement_received) {
  StatementResponse response;

  const JsonValue &stmts = pimpl_->json_document_["stmts"];
  if (pimpl_->current_stmt_ >= stmts.Size()) {
    response.response_type = StatementResponse::ResponseType::ERROR;
    response.response.reset(new ErrorResponse(
        MYSQL_PARSE_ERROR, std::string("Unexpected stmt, got: \"") +
                               statement_received + "\"; expected nothing"));

    return response;
  }

  auto &stmt = stmts[pimpl_->current_stmt_++];
  harness_assert(
      stmt.HasMember("stmt") ||
      stmt.HasMember("stmt.regex"));  // schema should have caught this

  if (stmt.HasMember("exec_time")) {
    double exec_time = get_json_double_field(stmt, "exec_time", 0.0);
    response.exec_time =
        std::chrono::microseconds(static_cast<long>(exec_time * 1000));
  } else {
    response.exec_time = get_default_exec_time();
  }

  bool statement_is_regex = false;
  std::string name{"stmt"};
  if (stmt.HasMember("stmt.regex")) {
    name = "stmt.regex";
    statement_is_regex = true;
  }

  harness_assert(
      stmt[name.c_str()].IsString());  // schema should have caught this

  std::string statement = stmt[name.c_str()].GetString();

  bool statement_matching{false};
  if (!statement_is_regex) {  // not regex
    statement_matching = (statement_received == statement);
  } else {  // regex
    statement_matching = pattern_matching(statement_received, statement);
  }

  if (!statement_matching) {
    response.response_type = StatementResponse::ResponseType::ERROR;
    response.response.reset(new ErrorResponse(
        MYSQL_PARSE_ERROR, std::string("Unexpected stmt, got: \"") +
                               statement_received + "\"; expected: \"" +
                               statement + "\""));
  } else if (stmt.HasMember("ok")) {
    response.response_type = StatementResponse::ResponseType::OK;
    response.response = pimpl_->read_ok_info(stmt);
  } else if (stmt.HasMember("error")) {
    response.response_type = StatementResponse::ResponseType::ERROR;
    response.response = pimpl_->read_error_info(stmt);
  } else if (stmt.HasMember("result")) {
    response.response_type = StatementResponse::ResponseType::RESULT;
    response.response = pimpl_->read_result_info(stmt);
  } else {
    harness_assert_this_should_not_execute();  // schema should have caught
                                               // this
  }

  return response;
}

std::chrono::microseconds QueriesJsonReader::get_default_exec_time() {
  if (pimpl_->json_document_.HasMember("defaults")) {
    auto &defaults = pimpl_->json_document_["defaults"];
    if (defaults.HasMember("exec_time")) {
      double exec_time = get_json_double_field(defaults, "exec_time", 0.0);
      return std::chrono::microseconds(static_cast<long>(exec_time * 1000));
    }
  }
  return std::chrono::microseconds(0);
}

std::unique_ptr<ResultsetResponse> QueriesJsonReader::Pimpl::read_result_info(
    const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("result"));

  const auto &result = stmt["result"];

  std::unique_ptr<ResultsetResponse> response(new ResultsetResponse);

  // read columns
  if (result.HasMember("columns")) {
    const auto &columns = result["columns"];
    harness_assert(columns.IsArray());  // schema should have caught this

    for (size_t i = 0; i < columns.Size(); ++i) {
      auto &column = columns[i];
      column_info_type column_info{
          get_json_string_field(column, "name", "", true),
          column_type_from_string(
              get_json_string_field(column, "type", "", true)),
          get_json_string_field(column, "orig_name"),
          get_json_string_field(column, "table"),
          get_json_string_field(column, "orig_table"),
          get_json_string_field(column, "schema"),
          get_json_string_field(column, "catalog", "def"),
          get_json_integer_field<uint16_t>(column, "flags"),
          get_json_integer_field<uint8_t>(column, "decimals"),
          get_json_integer_field<uint32_t>(column, "length"),
          get_json_integer_field<uint16_t>(column, "character_set", 63),
          get_json_integer_field<unsigned>(column, "repeat", 1)};

      response->columns.push_back(column_info);
    }
  }

  // read rows
  if (result.HasMember("rows")) {
    const auto &rows = result["rows"];
    harness_assert(rows.IsArray());  // schema should have caught this

    auto columns_size = response->columns.size();

    for (size_t i = 0; i < rows.Size(); ++i) {
      auto &row = rows[i];
      harness_assert(row.IsArray());  // schema should have caught this

      // this check cannot be performed by validating against schema, thus we
      // need it in code
      if (row.Size() != columns_size) {
        throw std::runtime_error(
            std::string("Wrong statements document structure: ") +
            "number of row fields different than number of columns " +
            std::to_string(row.Size()) + " != " + std::to_string(columns_size));
      }

      RowValueType row_values;
      for (size_t j = 0; j < row.Size(); ++j) {
        auto &column_info = response->columns[j];
        const size_t repeat = static_cast<size_t>(column_info.repeat);
        if (row[j].IsNull()) {
          row_values.push_back(std::make_pair(false, ""));
        } else {
          row_values.push_back(
              std::make_pair(true, get_json_value_as_string(row[j], repeat)));
        }
      }

      response->rows.push_back(row_values);
    }
  }

  return response;
}

std::unique_ptr<Response> QueriesJsonReader::Pimpl::read_ok_info(
    const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("ok"));

  const auto &f_ok = stmt["ok"];

  return std::unique_ptr<Response>(new OkResponse(
      get_json_integer_field<unsigned int>(f_ok, "last_insert_id", 0),
      get_json_integer_field<unsigned int>(f_ok, "warnings", 0)));
}

std::unique_ptr<Response> QueriesJsonReader::Pimpl::read_error_info(
    const JsonValue &stmt) {
  // only asserting as this should have been checked before if we got here
  assert(stmt.HasMember("error"));

  const auto &f_error = stmt["error"];

  return std::unique_ptr<Response>(new ErrorResponse(
      get_json_integer_field<unsigned int>(f_error, "code", 0, true),
      get_json_string_field(f_error, "message", "unknown error-msg"),
      get_json_string_field(f_error, "sql_state", "HY000")));
}

}  // namespace server_mock
