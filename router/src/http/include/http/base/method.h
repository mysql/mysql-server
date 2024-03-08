/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_METHOD_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_METHOD_H_

#include <bitset>
#include <string_view>

#include "mysqlrouter/http_common_export.h"

namespace http {
namespace base {
namespace method {

using key_type = int;
using pos_type = unsigned;

namespace Pos {

constexpr pos_type Get = 0;
constexpr pos_type Post = 1;
constexpr pos_type Head = 2;
constexpr pos_type Put = 3;
constexpr pos_type Delete = 4;
constexpr pos_type Options = 5;
constexpr pos_type Trace = 6;
constexpr pos_type Connect = 7;
constexpr pos_type Patch = 8;

constexpr pos_type _LAST = Patch;

}  // namespace Pos

using Bitset = std::bitset<Pos::_LAST + 1>;

constexpr key_type Unknown{-1};
constexpr key_type Get{1 << Pos::Get};
constexpr key_type Post{1 << Pos::Post};
constexpr key_type Head{1 << Pos::Head};
constexpr key_type Put{1 << Pos::Put};
constexpr key_type Delete{1 << Pos::Delete};
constexpr key_type Options{1 << Pos::Options};
constexpr key_type Trace{1 << Pos::Trace};
constexpr key_type Connect{1 << Pos::Connect};
constexpr key_type Patch{1 << Pos::Patch};

HTTP_COMMON_EXPORT key_type from_string(const std::string_view &method_name);
HTTP_COMMON_EXPORT pos_type from_string_to_post(const std::string_view &method);

}  // namespace method
}  // namespace base
}  // namespace http

namespace HttpMethod {
using namespace http::base::method;
}  // namespace HttpMethod

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_BASE_METHOD_H_
