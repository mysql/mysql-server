/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_factory.h"
#include "mrs/rest/handler_file.h"
#include "mrs/rest/handler_function.h"
#include "mrs/rest/handler_object_metadata.h"
#include "mrs/rest/handler_schema_metadata.h"
#include "mrs/rest/handler_sp.h"
#include "mrs/rest/handler_table.h"

namespace mrs {
namespace rest {

using HandlerPtr = std::unique_ptr<HandlerFactory::Handler>;

HandlerPtr HandlerFactory::create_file_handler(
    Route *r, AuthManager *auth_manager,
    mrs::interface::QueryFactory *query_factor) {
  return HandlerPtr{new HandlerFile(r, auth_manager, query_factor)};
}

HandlerPtr HandlerFactory::create_function_handler(Route *r,
                                                   AuthManager *auth_manager) {
  return HandlerPtr{new HandlerFunction(r, auth_manager)};
}

HandlerPtr HandlerFactory::create_sp_handler(Route *r,
                                             AuthManager *auth_manager) {
  return HandlerPtr{new HandlerSP(r, auth_manager)};
}

HandlerPtr HandlerFactory::create_object_handler(
    Route *r, AuthManager *auth_manager, mrs::GtidManager *gtid_manager) {
  return HandlerPtr{new HandlerTable(r, auth_manager, gtid_manager)};
}

HandlerPtr HandlerFactory::create_object_metadata_handler(
    Route *r, AuthManager *auth_manager) {
  return HandlerPtr{new HandlerMetadata(r, auth_manager)};
}

HandlerPtr HandlerFactory::create_schema_metadata_handler(
    RouteSchema *r, AuthManager *auth_manager) {
  return HandlerPtr{new HandlerSchemaMetadata(r, auth_manager)};
}

}  // namespace rest
}  // namespace mrs
