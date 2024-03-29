/*
   Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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
#include <iostream>
#include <cstdlib>

#include <NdbApi.hpp>

namespace
{
  inline void test_connection(const Ndb_cluster_connection &connection)
  {
    std::cout << "Connected to: " << connection.get_system_name()
              << ",\n\ton port: " << connection.get_connected_port()
              << ",\n\tactive NDBDs: " << connection.get_active_ndb_objects()
              << std::endl;
  }
}

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::cout << "Usage: ndb_ndbapi_basic_connect <connectstring>"
              << std::endl;
    return EXIT_FAILURE;
  }

  const char *connectstring = argv[1];

  ndb_init();
  {
    Ndb_cluster_connection connection(connectstring);
    if (connection.connect() != 0)
    {
      std::cout << "Cannot connect to cluster management server" << std::endl;
      return EXIT_FAILURE;
    }

    if (connection.wait_until_ready(30, 0) != 0)
    {
      std::cout << "Cluster was not ready within 30 secs" << std::endl;
      return EXIT_FAILURE;
    }

    // Let's verify connection
    test_connection(connection);
  }
  ndb_end(0);

  return EXIT_SUCCESS;
}

