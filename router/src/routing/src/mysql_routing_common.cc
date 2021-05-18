/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <string.h>

#include "mysql_routing_common.h"

std::string get_routing_thread_name(const std::string &config_name,
                                    const std::string &prefix) {
  const char *p = config_name.c_str();

  // at the time of writing, config_name starts with:
  //   "routing:<config_from_conf_file>" (with key)
  // or with:
  //   "routing" (without key).
  // Verify this assumption
  constexpr char kRouting[] = "routing";
  size_t kRoutingLen = sizeof(kRouting) - 1;  // -1 to ignore string terminator
  if (memcmp(p, kRouting, kRoutingLen)) return prefix + ":parse err";

  // skip over "routing[:]"
  p += kRoutingLen;
  if (*p == ':') p++;

  // at the time of writing, bootstrap generates 4 routing configurations by
  // default, which will result in <config_from_conf_file> having one of below 4
  // values:
  //   "<cluster_name>_default_ro",   "<cluster_name>_default_rw",
  //   "<cluster_name>_default_x_ro", "<cluster_name>_default_x_rw"
  // since we're limited to 15 chars for thread name, we skip over
  // "<cluster_name>_default_" so that suffixes ("x_ro", etc) can fit
  std::string key = p;
  const char kPrefix[] = "_default_";
  size_t pos = key.find(kPrefix);
  if (pos != key.npos) {
    key = key.substr(pos + sizeof(kPrefix) - 1);  // -1 for string terminator
  }

  // now put everything together
  std::string thread_name = prefix + ":" + key;
  thread_name.resize(15);  // max for pthread_setname_np()

  return thread_name;
}
