/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_CLASSIC_PROTOCOL_CLONE_H_
#define MYSQL_ROUTER_CLASSIC_PROTOCOL_CLONE_H_

#include <cstdint>  // uint32_t
#include <vector>

namespace classic_protocol::clone {

struct Locator {
  uint8_t storage_engine_type;
  std::vector<uint8_t> locator;
};

namespace client {
// negotiate clone protocol.
//
// response: server::Ok, server::Error
class Init {
 public:
  uint32_t protocol_version;
  uint32_t ddl_timeout;

  std::vector<Locator> locators;
};

class Attach : public Init {};
class Reinit : public Init {};

// no content
class Execute {};

class Ack {
 public:
  uint32_t error_number;

  Locator locator;

  std::vector<uint8_t> descriptor;
};
// no content
class Exit {};
}  // namespace client

namespace server {
class Locators {
 public:
  uint32_t protocol_version;

  std::vector<Locator> locators;
};
class DataDescriptor {
 public:
  uint8_t storage_engine_type;

  uint8_t locator_ndx;
};
class Data {};
class Complete {};
class Error {};
}  // namespace server
}  // namespace classic_protocol::clone

#endif
