/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQLD_MOCK_STATEMENT_READER_INCLUDED
#define MYSQLD_MOCK_STATEMENT_READER_INCLUDED

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "mysql_protocol_common.h"

namespace server_mock {

struct Response {
  virtual ~Response() = default;
};

/** @brief Keeps result data for single SQL statement that returns
 *         resultset.
 **/
struct ResultsetResponse : public Response {
  std::vector<column_info_type> columns;
  std::vector<RowValueType> rows;
};

struct OkResponse : public Response {
  OkResponse(unsigned int last_insert_id_ = 0, unsigned int warning_count_ = 0)
      : last_insert_id(last_insert_id_), warning_count(warning_count_) {}

  unsigned int last_insert_id;
  unsigned int warning_count;
};

struct ErrorResponse : public Response {
  ErrorResponse(unsigned int code_, std::string msg_,
                std::string sql_state_ = "HY000")
      : code(code_), msg(msg_), sql_state(sql_state_) {}

  unsigned int code;
  std::string msg;
  std::string sql_state;
};

class Greeting : public Response {
 public:
  Greeting(const std::string &server_version, uint32_t connection_id,
           mysql_protocol::Capabilities::Flags capabilities,
           uint16_t status_flags, uint8_t character_set,
           const std::string &auth_method, const std::string &auth_data)
      : server_version_{server_version},
        connection_id_{connection_id},
        capabilities_{capabilities},
        status_flags_{status_flags},
        character_set_{character_set},
        auth_method_{auth_method},
        auth_data_{auth_data} {}

  mysql_protocol::Capabilities::Flags capabilities() const {
    return capabilities_;
  }

  std::string server_version() const { return server_version_; }

  uint32_t connection_id() const { return connection_id_; }
  uint8_t character_set() const { return character_set_; }
  uint16_t status_flags() const { return status_flags_; }

  std::string auth_method() const { return auth_method_; }
  std::string auth_data() const { return auth_data_; }

 private:
  std::string server_version_;
  uint32_t connection_id_;
  mysql_protocol::Capabilities::Flags capabilities_;
  uint16_t status_flags_;
  uint8_t character_set_;
  std::string auth_method_;
  std::string auth_data_;
};

class AuthSwitch : public Response {
 public:
  AuthSwitch(const std::string &method, const std::string &data)
      : method_{method}, data_{data} {}

  std::string method() const { return method_; }
  std::string data() const { return data_; }

 private:
  std::string method_;
  std::string data_;
};

class AuthFast : public Response {};

struct HandshakeResponse {
  enum class ResponseType {
    UNKNOWN,
    OK,
    ERROR,
    GREETING,
    AUTH_SWITCH,
    AUTH_FAST
  };

  // expected response type
  ResponseType response_type{ResponseType::UNKNOWN};

  std::unique_ptr<Response> response;

  // execution time in microseconds
  std::chrono::microseconds exec_time{0};
};

/** @class StatementResponse
 *
 * @brief Keeps single SQL statement data.
 **/
struct StatementResponse {
  // Must initialize exec_time here rather than using a member initializer
  // due to a bug in Developer Studio.
  StatementResponse() : exec_time(0) {}

  /** @enum ResponseType
   *
   * Response expected for given SQL statement.
   **/
  enum class ResponseType { UNKNOWN, OK, ERROR, RESULT };

  // expected response type for the statement
  ResponseType response_type{ResponseType::UNKNOWN};

  std::unique_ptr<Response> response;

  // execution time in microseconds
  std::chrono::microseconds exec_time;
};

class StatementReaderBase {
 public:
  /** @brief Returns the data about the next statement from the
   *         json file. If there is no more statements it returns
   *         empty statement.
   **/
  virtual StatementResponse handle_statement(const std::string &statement) = 0;

  /**
   * process the handshake packet and return a client response
   */
  virtual HandshakeResponse handle_handshake(
      const std::vector<uint8_t> &payload) = 0;

  /** @brief Returns the default execution time in microseconds. If
   *         no default execution time is provided in json file, then
   *         0 microseconds is returned.
   **/
  virtual std::chrono::microseconds get_default_exec_time() = 0;

  virtual ~StatementReaderBase() = default;
};

}  // namespace server_mock

#endif
