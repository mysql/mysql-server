/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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
