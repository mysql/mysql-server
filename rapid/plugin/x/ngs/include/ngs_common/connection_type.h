/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */


#ifndef _NGS_CONNECTION_TYPE_H_
#define _NGS_CONNECTION_TYPE_H_

#include "violite.h"

namespace ngs
{

enum Connection_type
{
  Connection_notset,
  Connection_tcpip,
  Connection_unixsocket,
  Connection_tls,
  Connection_namedpipe
};

class Connection_type_helper
{
public:
  static Connection_type convert_type(const enum_vio_type type);
  static enum_vio_type convert_type(const Connection_type type);
  static bool is_secure_type(const Connection_type type);
};

} // namespace ngs

#endif // _NGS_CONNECTION_TYPE_H_
