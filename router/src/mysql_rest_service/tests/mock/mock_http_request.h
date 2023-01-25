/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_REQUEST_H_
#define ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_REQUEST_H_

#include "mysqlrouter/http_request.h"

class MockHttpHeaders : public HttpHeaders {
 public:
  MockHttpHeaders() : HttpHeaders() {}

  MOCK_METHOD(int, add, (const char *key, const char *value), (override));
  MOCK_METHOD(const char *, get, (const char *key), (const, override));
  MOCK_METHOD(Iterator, begin, (), (override));
  MOCK_METHOD(Iterator, end, (), (override));
};

class MockHttpBuffer : public HttpBuffer {
 public:
  MockHttpBuffer() : HttpBuffer() {}

  MOCK_METHOD(void, add, (const char *data, size_t data_size), (override));
  MOCK_METHOD(void, add_file, (int file_fd, off_t offset, off_t size),
              (override));
  MOCK_METHOD(size_t, length, (), (const, override));
  MOCK_METHOD(std::vector<uint8_t>, pop_front, (size_t length), (override));
};

class MockHttpUri : public HttpUri {
 public:
  MockHttpUri() : HttpUri() {}

  MOCK_METHOD(std::string, join, (), (const, override));
  MOCK_METHOD(std::string, get_scheme, (), (const, override));
  MOCK_METHOD(void, set_scheme, (const std::string &scheme), (override));
  MOCK_METHOD(std::string, get_userinfo, (), (const, override));
  MOCK_METHOD(void, set_userinfo, (const std::string &userinfo), (override));
  MOCK_METHOD(std::string, get_host, (), (const, override));
  MOCK_METHOD(void, set_host, (const std::string &host), (override));
  MOCK_METHOD(uint16_t, get_port, (), (const, override));
  MOCK_METHOD(void, set_port, (uint16_t port), (const, override));
  MOCK_METHOD(std::string, get_path, (), (const, override));
  MOCK_METHOD(void, set_path, (const std::string &path), (override));
  MOCK_METHOD(std::string, get_fragment, (), (const, override));
  MOCK_METHOD(void, set_fragment, (const std::string &fragment), (override));
  MOCK_METHOD(std::string, get_query, (), (const, override));
  MOCK_METHOD(bool, set_query, (const std::string &query), (override));
  MOCK_METHOD(bool, is_valid, (), (const));

  operator bool() const override { return is_valid(); }
};

class MockHttpRequest : public HttpRequest {
 public:
  MOCK_METHOD(HttpHeaders &, get_output_headers, (), (override));
  MOCK_METHOD(HttpHeaders &, get_input_headers, (), (const, override));
  MOCK_METHOD(HttpBuffer &, get_output_buffer, (), (override));
  MOCK_METHOD(HttpBuffer &, get_input_buffer, (), (const, override));
  MOCK_METHOD(unsigned, get_response_code, (), (const, override));
  MOCK_METHOD(std::string, get_response_code_line, (), (const, override));
  MOCK_METHOD(HttpMethod::type, get_method, (), (const, override));
  MOCK_METHOD(HttpUri &, get_uri, (), (const, override));
  MOCK_METHOD(void, send_reply, (int status_code), (override));
  MOCK_METHOD(void, send_reply, (int status_code, std::string status_text),
              (override));
  MOCK_METHOD(void, send_reply,
              (int status_code, std::string status_text, HttpBuffer &buffer),
              (override));
  MOCK_METHOD(void, send_error, (int status_code), (override));
  MOCK_METHOD(void, send_error, (int status_code, std::string status_text),
              (override));
  MOCK_METHOD(int, error_code, (), (override));
  MOCK_METHOD(void, error_code, (int), (override));
  MOCK_METHOD(std::string, error_msg, (), (override));
  MOCK_METHOD(std::error_code, socket_error_code, (), (const, override));
  MOCK_METHOD(void, socket_error_code, (std::error_code ec), (override));
  MOCK_METHOD(bool, is_modified_since, (time_t last_modified), (override));
  MOCK_METHOD(bool, add_last_modified, (time_t last_modified), (override));

  MOCK_METHOD(bool, is_valid, (), (const));
  operator bool() const override { return is_valid(); }
};

#endif  // ROUTER_SRC_REST_MRS_TESTS_MOCK_MOCK_HTTP_REQUEST_H_
