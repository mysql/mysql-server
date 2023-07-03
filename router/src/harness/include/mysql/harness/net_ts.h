/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_NET_TS_H_
#define MYSQL_HARNESS_NET_TS_H_

// implementation of the networking-ts draft: http://wg21.link/N4771
//
// see also:
// - http://wg21.link/p1269
//
// Executors:
// - http://wg21.link/P0761
// - http://wg21.link/P0443

// 11.1 [convenience.hdr.synop]

#include "net_ts/buffer.h"
#include "net_ts/executor.h"
#include "net_ts/internet.h"
#include "net_ts/io_context.h"
#include "net_ts/local.h"
#include "net_ts/socket.h"
#include "net_ts/timer.h"

#endif
