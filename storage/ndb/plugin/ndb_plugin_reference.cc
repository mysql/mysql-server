/*
   Copyright (c) 2017, 2024, Oracle and/or its affiliates.

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

// Implements
#include "ndb_plugin_reference.h"

#include "mysql/mysql_lex_string.h"
// Using interface defined in
#include "mysql/plugin.h"
// Using
#include "sql/sql_plugin.h"
#include "string_with_len.h"

Ndb_plugin_reference::Ndb_plugin_reference() : plugin(nullptr) {}

bool Ndb_plugin_reference::lock() {
  const LEX_CSTRING plugin_name = {STRING_WITH_LEN("ndbcluster")};

  // Resolve reference to "ndbcluster plugin"
  plugin =
      plugin_lock_by_name(nullptr, plugin_name, MYSQL_STORAGE_ENGINE_PLUGIN);
  if (!plugin) return false;

  return true;
}

st_plugin_int *Ndb_plugin_reference::handle() const {
  return plugin_ref_to_int(plugin);
}

Ndb_plugin_reference::~Ndb_plugin_reference() {
  if (plugin) {
    // Unlock the "ndbcluster_plugin" reference
    plugin_unlock(nullptr, plugin);
  }
}
