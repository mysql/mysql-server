/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "ndb_server_hooks.h"

// Using interface defined in
#include "sql/replication.h"

// Using
#include "sql/ndb_log.h"
#include "sql/ndb_plugin_reference.h"


bool Ndb_server_hooks::register_server_started(hook_t* hook_func)
{
  // Only allow one server_started hook to be installed
  DBUG_ASSERT(!m_server_state_observer);

  Ndb_plugin_reference ndbcluster_plugin;

  // Resolve pointer to the ndbcluster plugin, it may
  // not resolve in case plugin has failed to init()
  if (!ndbcluster_plugin.lock())
    return false;

  m_server_state_observer =
      new Server_state_observer {
        sizeof(Server_state_observer),

                                   // before clients are allowed to connect
        (before_handle_connection_t)hook_func,
        NULL,                      // before recovery
        NULL,                      // after engine recovery
        NULL,                      // after recovery
        NULL,                      // before shutdown
        NULL,                      // after shutdown
      };

  // Install server state observer to be called
  // before the server allows incoming connections
  if (register_server_state_observer(m_server_state_observer,
                                     ndbcluster_plugin.handle()))
  {
    ndb_log_error("Failed to register server state observer");
    return false;
  }

  return true;
}


bool Ndb_server_hooks::register_applier_start(hook_t* hook_func)
{
  // Only allow one applier_start hook to be installed
  DBUG_ASSERT(!m_binlog_relay_io_observer);

  Ndb_plugin_reference ndbcluster_plugin;

  // Resolve pointer to the ndbcluster plugin
  if (!ndbcluster_plugin.lock())
    return false;

  m_binlog_relay_io_observer=
      new Binlog_relay_IO_observer {
        sizeof(Binlog_relay_IO_observer),

      NULL,                        // thread_start
      NULL,                        // thread_stop
      (applier_start_t)hook_func,  // applier_start
      NULL,                        // applier_stop
      NULL,                        // before_request_transmit
      NULL,                        // after_read_event
      NULL,                        // after_queue_event
      NULL,                        // after_reset
      NULL                         // applier_log_event
      };


  // Install replication observer to be called when applier thread start
  if (register_binlog_relay_io_observer(m_binlog_relay_io_observer,
                                        ndbcluster_plugin.handle()))
  {
    ndb_log_error("Failed to register binlog relay io observer");
    return false;
  }

  return true;
}


void Ndb_server_hooks::unregister_all(void)
{
  if (m_server_state_observer)
    unregister_server_state_observer(m_server_state_observer,
                                     nullptr);
  if (m_binlog_relay_io_observer)
    unregister_binlog_relay_io_observer(m_binlog_relay_io_observer,
                                        nullptr);
}


Ndb_server_hooks::~Ndb_server_hooks()
{
  delete m_server_state_observer;
  delete m_binlog_relay_io_observer;
}
