/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_IO_CONNECTION_TYPE_H_
#define PLUGIN_X_SRC_IO_CONNECTION_TYPE_H_

#include "violite.h"

namespace xpl {

enum Connection_type {
  Connection_notset,
  Connection_tcpip,
  Connection_unixsocket,
  Connection_tls,
  Connection_namedpipe
};

class Connection_type_helper {
 public:
  static Connection_type convert_type(const enum_vio_type type);
  static enum_vio_type convert_type(const Connection_type type);
  static bool is_secure_type(const Connection_type type);
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_IO_CONNECTION_TYPE_H_
