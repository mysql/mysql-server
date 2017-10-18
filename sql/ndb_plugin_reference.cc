/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements
#include "ndb_plugin_reference.h"

// Using interface defined in
#include "mysql/plugin.h"
#include "mysql/mysql_lex_string.h"

// Using
#include "sql_plugin.h"


Ndb_plugin_reference::Ndb_plugin_reference() :
    plugin(nullptr)
{
}


bool Ndb_plugin_reference::lock()
{
  const LEX_CSTRING plugin_name = { C_STRING_WITH_LEN("ndbcluster") };

  // Resolve reference to "ndbcluster plugin"
  plugin = plugin_lock_by_name(NULL,
                               plugin_name,
                               MYSQL_STORAGE_ENGINE_PLUGIN);
  if (!plugin)
    return false;

  return true;
}


st_plugin_int*
Ndb_plugin_reference::handle() const {

  return plugin_ref_to_int(plugin);
}


Ndb_plugin_reference::~Ndb_plugin_reference()
{
  if (plugin)
  {
    // Unlock the "ndbcluster_plugin" reference
    plugin_unlock(NULL, plugin);
  }
}
