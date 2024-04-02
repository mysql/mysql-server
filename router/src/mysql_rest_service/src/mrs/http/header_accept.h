/*
 Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_HTTP_HEADER_ACCEPT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_HTTP_HEADER_ACCEPT_H_

#include <optional>
#include <string>
#include <vector>

#include "helper/media_type.h"

class HttpRequest;

namespace mrs {
namespace http {

using MimeClass = std::optional<std::string>;

class Accepts {
 public:
  explicit Accepts(const std::string &mime_type);

  bool is_acceptable(const std::string &mime_type);

 private:
  MimeClass mime_class_;
  MimeClass mime_subclass_;
};

class HeaderAccept {
 public:
  HeaderAccept();
  explicit HeaderAccept(const char *header_accept);

  std::optional<helper::MediaType> is_acceptable(
      const std::vector<helper::MediaType> &mime_types);
  bool is_acceptable(const helper::MediaType &mime_type);
  bool is_acceptable(const std::string &mime_type);

 private:
  std::vector<Accepts> accepts_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_HTTP_HEADER_ACCEPT_H_
