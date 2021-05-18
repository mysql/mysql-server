/*
  Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef MYSQL_PROTOCOL_AUTHENTICATION_INCLUDED
#define MYSQL_PROTOCOL_AUTHENTICATION_INCLUDED

#include <vector>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/stdx/string_view.h"

#ifdef _WIN32
// workaround LNK2005 in std::vector<uint8_t> when linking against
// mysql_protocol.lib
//
//     mysql_protocol.lib(mysql_protocol.dll) : error LNK2005:
//       "public: __cdecl std::vector<unsigned char,class
//       std::allocator<unsigned char> >::~vector<unsigned char,class
//       std::allocator<unsigned char> >(void)"
//       (??1?$vector@EV?$allocator@E@std@@@std@@QEAA@XZ) already defined in
//       authentication.obj
//
// to be removed if mysql_protocol/base_packet.h doesn't inherit from
// std::vector<uint8_t> anymore.
//
// only happens with MSVC
#include "mysqlrouter/mysql_protocol.h"
#endif

class MySQLNativePassword {
 public:
  static constexpr char name[] = "mysql_native_password";

  // client-side scrambling of the password
  static stdx::expected<std::vector<uint8_t>, void> scramble(
      stdx::string_view nonce, stdx::string_view password);
};

class CachingSha2Password {
 public:
  static constexpr char name[] = "caching_sha2_password";

  // client-side scrambling of the password
  static stdx::expected<std::vector<uint8_t>, void> scramble(
      stdx::string_view nonce, stdx::string_view password);
};

class ClearTextPassword {
 public:
  static constexpr char name[] = "mysql_clear_password";

  // client-side scrambling of the password
  static stdx::expected<std::vector<uint8_t>, void> scramble(
      stdx::string_view nonce, stdx::string_view password);
};

#endif
