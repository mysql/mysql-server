/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_SCHEMA_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_SCHEMA_H_

#include <cstdint>
#include <string>
#include <vector>

#include "mrs/interface/rest_handler.h"
#include "mrs/interface/state.h"
#include "mrs/interface/universal_id.h"

namespace mrs {
namespace interface {

class Object;

class ObjectSchema {
 public:
  using VectorOfRoutes = std::vector<Object *>;
  using Handler = mrs::interface::RestHandler;

  virtual ~ObjectSchema() = default;

 public:
  virtual void turn(const State state) = 0;
  virtual void route_unregister(Object *r) = 0;
  virtual void route_register(Object *r) = 0;

  virtual const std::string &get_name() const = 0;
  virtual const std::string &get_url() const = 0;
  virtual const std::string &get_path() const = 0;
  virtual const std::string &get_options() const = 0;
  virtual const std::string get_full_path() const = 0;
  virtual const VectorOfRoutes &get_routes() const = 0;
  virtual bool requires_authentication() const = 0;
  virtual UniversalId get_service_id() const = 0;
  virtual UniversalId get_id() const = 0;
};

}  // namespace interface
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_SCHEMA_H_
