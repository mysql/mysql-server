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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef PLUGIN_X_SRC_VARIABLES_TIMEOUT_CONFIG_H_
#define PLUGIN_X_SRC_VARIABLES_TIMEOUT_CONFIG_H_

#include <cstdint>

#include "plugin/x/src/variables/system_variables_defaults.h"

namespace xpl {

struct Timeouts_config {
  uint32_t m_interactive_timeout = defaults::timeout::k_interactive_timeout;
  uint32_t m_wait_timeout = defaults::timeout::k_wait_timeout;
  uint32_t m_read_timeout = defaults::timeout::k_read_timeout;
  uint32_t m_write_timeout = defaults::timeout::k_write_timeout;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_VARIABLES_TIMEOUT_CONFIG_H_
