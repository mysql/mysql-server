/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_PROTOCOL_CONFIG_H_
#define _NGS_PROTOCOL_CONFIG_H_

#include <stdint.h>
#include <list>

#include "ngs_common/chrono.h"


namespace ngs
{

class Protocol_config
{
public:
  uint32_t default_max_frame_size;
  uint32_t max_message_size;

  chrono::seconds connect_timeout;
  chrono::milliseconds connect_timeout_hysteresis;

  Protocol_config()
  : default_max_frame_size(16*1024*1024),
    max_message_size(16*1024*1024),
    connect_timeout(0),
    connect_timeout_hysteresis(100)
  {
  }
};

} // namespace ngs

#endif // _NGS_PROTOCOL_CONFIG_H_
