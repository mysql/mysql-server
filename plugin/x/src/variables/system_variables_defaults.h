/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_DEFAULTS_H_
#define PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_DEFAULTS_H_

#include <cstdint>

#include "plugin/x/generated/mysqlx_version.h"

#define BYTE(X) (X)
#define KBYTE(X) ((X)*1024)
#define MBYTE(X) ((X)*1024 * 1024)
#define GBYTE(X) ((X)*1024 * 1024 * 1024)

namespace defaults {
namespace timeout {

const uint32_t k_interactive_timeout = 28800;
const uint32_t k_wait_timeout = 28800;
const uint32_t k_read_timeout = 30;
const uint32_t k_write_timeout = 60;
const uint32_t k_connect_timeout = 30;
const uint32_t k_port_open_timeout = 0;

}  // namespace timeout

namespace connectivity {

const char *const k_bind_address = "*";
const uint32_t k_port = MYSQLX_TCP_PORT;
const char *const k_socket = MYSQLX_UNIX_ADDR;
const uint32_t k_max_connections = 100;
const uint32_t k_max_allowed_packet = MBYTE(64);
const bool k_enable_hello_notice = true;

}  // namespace connectivity

namespace threads {

const uint32_t k_min_worker_threads = 2;
const uint32_t k_idle_worker_thread_timeout = 60;

}  // namespace threads

namespace docstore {

const uint32_t k_document_id_unique_prefix = 0;

}  // namespace docstore

}  // namespace defaults

#endif  // PLUGIN_X_SRC_VARIABLES_SYSTEM_VARIABLES_DEFAULTS_H_
