/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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
