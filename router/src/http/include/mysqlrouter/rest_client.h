/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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
#ifndef MYSQL_ROUTER_REST_CLIENT_H_INCLUDED
#define MYSQL_ROUTER_REST_CLIENT_H_INCLUDED

#include "mysqlrouter/http_client.h"

constexpr const char kRestAPIVersion[] = "20190715";

class HTTP_CLIENT_EXPORT RestClient {
 public:
  RestClient(IOContext &io_ctx, const std::string &address, uint16_t port)
      : http_client_{std::unique_ptr<HttpClient>{
            new HttpClient(io_ctx, address, port)}} {}

  RestClient(IOContext &io_ctx, const std::string &address, uint16_t port,
             const std::string &username, const std::string &password)
      : username_{username},
        password_{password},
        http_client_{std::unique_ptr<HttpClient>{
            new HttpClient(io_ctx, address, port)}} {}

  RestClient(IOContext &io_ctx, const HttpUri &u, const std::string &username,
             const std::string &password)
      : username_{username},
        password_{password},
        http_client_{std::unique_ptr<HttpClient>{
            new HttpClient(io_ctx, u.get_host(), u.get_port())}} {}

  // build a RestClient around an existing HttpClient object that's consumed
  RestClient(std::unique_ptr<HttpClient> &&http_client)
      : http_client_{std::move(http_client)} {}

  HttpRequest request_sync(
      HttpMethod::type method, const std::string &uri,
      const std::string &request_body = {},
      const std::string &content_type = "application/json");

  operator bool() const { return http_client_->operator bool(); }

  std::string error_msg() const { return http_client_->error_msg(); }

 private:
  std::string username_;
  std::string password_;
  std::unique_ptr<HttpClient> http_client_;
};

#endif
