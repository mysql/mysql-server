/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */
#include <mysql/plugin.h>
#include "ngs_common/protocol_protobuf.h"
#include "replication.h"
#include "xpl_log.h"
#include "xpl_replication_observer.h"

using namespace xpl;

int xpl_before_server_shutdown(Server_state_param *param)
{
 // google::protobuf::ShutdownProtobufLibrary();

  return 0;
}

Server_state_observer xpl_server_state_observer =
{
  sizeof(Server_state_observer),
  NULL,                       // before the client connect the node
  NULL,                       // before recovery
  NULL,                       // after engine recovery
  NULL,                       // after recovery
  xpl_before_server_shutdown, // before shutdown
  NULL                        // after shutdown
};

int xpl::xpl_register_server_observers(MYSQL_PLUGIN p)
{
  return register_server_state_observer(&xpl_server_state_observer, p);
}

int xpl::xpl_unregister_server_observers(MYSQL_PLUGIN p)
{
  return unregister_server_state_observer(&xpl_server_state_observer, p);
}
