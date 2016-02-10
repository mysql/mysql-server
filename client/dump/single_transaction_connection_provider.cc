/*
  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "single_transaction_connection_provider.h"

using namespace Mysql::Tools::Dump;

Mysql::Tools::Base::Mysql_query_runner*
  Single_transaction_connection_provider::create_new_runner(
    Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
      message_handler)
{
  Mysql::Tools::Base::Mysql_query_runner* runner= NULL;
  my_boost::mutex::scoped_lock lock(m_pool_mutex);
  if (m_runner_pool.size() > 0)
  {
    runner=m_runner_pool.back();
    m_runner_pool.pop_back();
  }
  return runner;
}

Single_transaction_connection_provider::Single_transaction_connection_provider(
  Mysql::Tools::Base::I_connection_factory* connection_factory,
  unsigned int connections,
  Mysql::I_callable<bool, const Mysql::Tools::Base::Message_data&>*
  message_handler)
  : Thread_specific_connection_provider(connection_factory),
    m_connections(connections)
{
  /* create a pool of connections */
  for (unsigned int conn_count= 1; conn_count <= m_connections; conn_count++)
  {
    Mysql::Tools::Base::Mysql_query_runner* runner=
      Abstract_connection_provider::create_new_runner(message_handler);
    /*
     To get a consistent backup we lock the server and flush all the tables.
     This is done with FLUSH TABLES WITH READ LOCK (FTWRL).
     FTWRL does following:
       1. Acquire a global read lock so that other clients can still query the
          database.
       2. Close all open tables.
       3. No further commits is allowed.
     This will ensure that any further connections will view the same state
     of all the databases which is ideal state to take backup.
     However flush tables is needed only if the database we backup has non
     innodb tables.
    */
    if (conn_count == 1)
      runner->run_query("FLUSH TABLES WITH READ LOCK");
    runner->run_query(
      "SET SESSION TRANSACTION ISOLATION LEVEL REPEATABLE READ");
    runner->run_query(
      "START TRANSACTION WITH CONSISTENT SNAPSHOT");
    if (conn_count == m_connections)
      m_runner_pool[0]->run_query("UNLOCK TABLES");
    m_runner_pool.push_back(runner);
  }
}
