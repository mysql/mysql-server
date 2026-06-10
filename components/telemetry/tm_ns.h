/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TELEMETRY_NS_H_INCLUDED
#define TELEMETRY_NS_H_INCLUDED

#include <string>

// For HAVE_SETNS
#include "my_config.h"

namespace telemetry {

constexpr int NO_FD = -1;

#ifdef HAVE_SETNS

int open_network_namespace(const std::string &network_namespace, int *fd);

int set_network_namespace(int fd);

int close_network_namespace(int fd);

#endif /* HAVE_SETNS */

int setup_network_namespaces();
int cleanup_network_namespaces();

}  // namespace telemetry

#endif /* TELEMETRY_NS_H_INCLUDED */
