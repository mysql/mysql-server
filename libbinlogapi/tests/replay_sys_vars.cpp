/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/

#include <cstdlib>
#include "binlog_api.h"
#include "utilities.h"

int main (int argc, char* argv[])
{
  mysql::Binary_log
    binlog(mysql::system::create_transport("mysql://root@127.0.0.1:13000"));

  if (binlog.connect())
  {
    std::cerr << "Can't connect!" << std::endl;
    exit(1);
  }
  std::cout << "Connected to server!!" << std::endl;

  if (binlog.set_position(4) != ERR_OK)
  {
    std::cerr << "Can't reposition the binary log reader."
              << std::endl;
    exit(1);
  }

  Binary_log_event *event;

  bool quit=false;
  while (!quit)
  {
    if (binlog.wait_for_next_event (&event))
    {
      quit= true;
      continue;
    }

    std::cout << "Pos = "
              << event->header()->next_position
              <<" Event_type = "
              << event->get_event_type()
              << std::endl;

    switch (event->header()->type_code)
    {
      case mysql::QUERY_EVENT:
        {
          const mysql::Query_event *qev=
          static_cast<const mysql::Query_event *>(event);
          std::cout << "Query = "
                    << qev->query
                    << " DB = "
                    << qev->db_name
                    <<  std::endl;
          std::map<std::string, mysql::Value> my_var_map;

          if (server_var_decoder(&my_var_map, qev->variables))
            return (EXIT_FAILURE);

          mysql::Converter converter;

          typedef std::map<std::string, mysql::Value>::value_type my_pair;
          std::map<std::string, mysql::Value>::iterator it= my_var_map.begin();

          for (; it != my_var_map.end(); it++)
          {
            my_pair ref= *it;
            std::string value;
            converter.to(value, ref.second);
            std::cout << ref.first
                      << " = "
                      << value
                      << std::endl;
          }
          std::cout << "----------" << std::endl << std::endl;

          if (qev->query.find("DROP DATABASE `sys_var`") != std::string::npos
              || qev->query.find("DROP DATABASE sys_var") != std::string::npos)
            quit= true;
        }
        break;
      default:
        break;
    }
    delete event;
  }
  return 0;
}

