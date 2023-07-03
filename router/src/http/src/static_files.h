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

#ifndef MYSQLROUTER_HTTP_STATIC_FILES_INCLUDED
#define MYSQLROUTER_HTTP_STATIC_FILES_INCLUDED

#include <string>

#include "mysqlrouter/http_server_component.h"

class HttpStaticFolderHandler : public BaseRequestHandler {
 public:
  explicit HttpStaticFolderHandler(std::string static_basedir,
                                   std::string require_realm)
      : static_basedir_(std::move(static_basedir)),
        require_realm_{std::move(require_realm)} {}

  void handle_request(HttpRequest &req) override;

 private:
  std::string static_basedir_;
  std::string require_realm_;
};

#endif
