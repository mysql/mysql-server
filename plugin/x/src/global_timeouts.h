/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef GLOBAL_TIMEOUTS_H_
#define GLOBAL_TIMEOUTS_H_

#include <mysql/plugin.h>

struct Global_timeouts {
  enum Default {
    k_interactive_timeout = 28800,
    k_wait_timeout = 28800,
    k_read_timeout = 30,
    k_write_timeout = 60
  };

  uint32_t interactive_timeout;
  uint32_t wait_timeout;
  uint32_t read_timeout;
  uint32_t write_timeout;
};

Global_timeouts get_global_timeouts();
void set_session_wait_timeout(THD *thd, const uint32_t wait_timeout);

#endif  // GLOBAL_TIMEOUTS_H_
