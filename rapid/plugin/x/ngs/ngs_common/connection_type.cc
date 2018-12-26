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

#include "ngs_common/connection_type.h"


using namespace ngs;

Connection_type Connection_type_helper::convert_type(const enum_vio_type type)
{
  switch (type)
  {
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

enum_vio_type Connection_type_helper::convert_type(const Connection_type type)
{
  for(int e = FIRST_VIO_TYPE; e <= LAST_VIO_TYPE; ++e)
  {
    if (type == convert_type(static_cast<enum_vio_type>(e)))
      return static_cast<enum_vio_type>(e);
  }

  return NO_VIO_TYPE;
}

bool Connection_type_helper::is_secure_type(const Connection_type type)
{
  switch(type)
  {
  case ngs::Connection_tls:
  case ngs::Connection_unixsocket: // fallthrough
    return true;

  default:
    return false;
  }
}
