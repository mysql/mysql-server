/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_

#include <set>
#include <string>

namespace mrs {

enum Authentication { kAuthenticationNone, kAuthenticationBasic2Server };

class Node {
 public:
  Node() {}
  Node(const std::string &host, const uint16_t port)
      : host_{host}, port_{port} {}

  std::string host_;
  uint16_t port_;
};

class SslConfiguration {
 public:
  mysql_ssl_mode ssl_mode_;
  std::string ssl_ca_file_;
  std::string ssl_ca_path_;
  std::string ssl_crl_file_;
  std::string ssl_crl_path_;
  std::string ssl_curves_;
  std::string ssl_ciphers_;
};

class Configuration {
 public:  // Option fetched from configuration file
  std::string mysql_user_;
  std::string mysql_user_password_;
  std::string mysql_user_data_access_;
  std::string mysql_user_data_access_password_;

  std::set<std::string> routing_names_;

 public:  // Options fetched from other plugins
  bool is_https_;
  SslConfiguration ssl_;
  // TODO(lkotula): Later on it should be diviced on read-only and writable
  // nodes (Shouldn't be in review)
  std::vector<Node> nodes_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_CONFIG_H_
