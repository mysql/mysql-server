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
#include "ndb_server_hooks.h"

// Using interface defined in
#include "sql/replication.h"
// Using
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_plugin_reference.h"

bool Ndb_server_hooks::register_server_hooks(hook_t *before_connections_hook,
                                             hook_t *dd_upgrade_hook) {
  // Only allow one server_started hook to be installed
  assert(!m_server_state_observer);

  Ndb_plugin_reference ndbcluster_plugin;

  // Resolve pointer to the ndbcluster plugin, it may
  // not resolve in case plugin has failed to init()
  if (!ndbcluster_plugin.lock()) return false;

  m_server_state_observer = new Server_state_observer{
      sizeof(Server_state_observer),

      // before clients are allowed to connect
      (before_handle_connection_t)before_connections_hook,
      nullptr,                              // before recovery
      nullptr,                              // after engine recovery
      nullptr,                              // after recovery
      nullptr,                              // before shutdown
      nullptr,                              // after shutdown
      (after_dd_upgrade_t)dd_upgrade_hook,  // after DD upgrade
  };

  // Install server state observer to be called
  // before the server allows incoming connections
  if (register_server_state_observer(m_server_state_observer,
                                     ndbcluster_plugin.handle())) {
    ndb_log_error("Failed to register server state observer");
    delete m_server_state_observer;
    m_server_state_observer = nullptr;
    return false;
  }

  return true;
}

void Ndb_server_hooks::unregister_all(void) {
  if (m_server_state_observer)
    unregister_server_state_observer(m_server_state_observer, nullptr);
}

Ndb_server_hooks::~Ndb_server_hooks() { delete m_server_state_observer; }
