/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/io/connection_type.h"

namespace xpl {

Connection_type Connection_type_helper::convert_type(const enum_vio_type type) {
  switch (type) {
    case VIO_TYPE_SOCKET:
      return Connection_unixsocket;

    case VIO_TYPE_SSL:
      return Connection_tls;

    case VIO_TYPE_TCPIP:
      return Connection_tcpip;

    case VIO_TYPE_NAMEDPIPE:
      return Connection_namedpipe;

    default:
      return Connection_notset;
  }
}

enum_vio_type Connection_type_helper::convert_type(const Connection_type type) {
  for (int e = FIRST_VIO_TYPE; e <= LAST_VIO_TYPE; ++e) {
    if (type == convert_type(static_cast<enum_vio_type>(e)))
      return static_cast<enum_vio_type>(e);
  }

  return NO_VIO_TYPE;
}

bool Connection_type_helper::is_secure_type(const Connection_type type) {
  switch (type) {
    case Connection_tls:
    case Connection_unixsocket:  // fallthrough
      return true;

    default:
      return false;
  }
}

}  // namespace xpl
