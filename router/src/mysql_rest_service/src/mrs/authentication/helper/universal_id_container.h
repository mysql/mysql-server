/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_UNIVERSAL_ID_CONTAINER_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_UNIVERSAL_ID_CONTAINER_H_

#include <cassert>

#include "mrs/interface/universal_id.h"

namespace mrs {
namespace authentication {

class UniversalIdContainer {
 public:
  auto begin() const { return std::begin(id_.raw); }
  auto end() const { return std::end(id_.raw); }
  void push_back(uint8_t value) {
    assert(push_index_ < id_.raw.size());
    id_.raw[push_index_++] = value;
  }
  auto get_user_id() const { return id_; }

 private:
  UniversalId id_;
  uint64_t push_index_{0};
};

}  // namespace authentication
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_AUTHENTICATION_HELPER_UNIVERSAL_ID_CONTAINER_H_ \
        */
