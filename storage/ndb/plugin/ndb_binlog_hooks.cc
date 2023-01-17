/*
   Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

// Implements
#include "ndb_binlog_hooks.h"

// Using interface defined in
#include "sql/replication.h"
// Using
#include "storage/ndb/plugin/ndb_log.h"
#include "storage/ndb/plugin/ndb_plugin_reference.h"

bool Ndb_binlog_hooks::register_hooks(
    after_reset_master_hook_t *after_reset_master) {
  // Only allow hooks to be installed once
  assert(!m_binlog_transmit_observer);

  // Resolve pointer to the ndbcluster plugin
  Ndb_plugin_reference ndbcluster_plugin;
  if (!ndbcluster_plugin.lock()) {
    return false;
  }

  m_binlog_transmit_observer = new Binlog_transmit_observer{
      sizeof(Binlog_transmit_observer),

      nullptr,                                   // transmit_start
      nullptr,                                   // transmit_stop
      nullptr,                                   // reserve_header
      nullptr,                                   // before_send_event
      nullptr,                                   // after_send_event
      (after_reset_master_t)after_reset_master,  // after_reset_master
  };

  // Install replication observer to be called when applier thread start
  if (register_binlog_transmit_observer(m_binlog_transmit_observer,
                                        ndbcluster_plugin.handle())) {
    ndb_log_error("Failed to register binlog transmit observer");
    return false;
  }

  return true;
}

void Ndb_binlog_hooks::unregister_all(void) {
  if (m_binlog_transmit_observer) {
    unregister_binlog_transmit_observer(m_binlog_transmit_observer, nullptr);
  }
}

Ndb_binlog_hooks::~Ndb_binlog_hooks() { delete m_binlog_transmit_observer; }
