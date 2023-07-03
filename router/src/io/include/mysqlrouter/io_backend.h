/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_IO_BACKEND_INCLUDED
#define ROUTER_IO_BACKEND_INCLUDED

#include <memory>  // unique_ptr
#include <set>
#include <string>

#include "mysql/harness/net_ts/impl/io_service_base.h"
#include "mysqlrouter/io_component_export.h"

class IO_COMPONENT_EXPORT IoBackend {
 public:
  /**
   * preferred backend for this platform.
   */
  static std::string preferred();

  /**
   * supported backends for this platform.
   */
  static std::set<std::string> supported();

  /**
   * create a backend from name.
   *
   * @param name name of the backend.
   * @returns backend
   * @retval nullptr if name doesn't refer to supported backends.
   */
  static std::unique_ptr<net::IoServiceBase> backend(const std::string &name);
};

#endif
