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

#include "utilities.h"

using namespace mysql;

namespace mysql {

int server_var_decoder (std::map<std::string, mysql::Value> *my_var_map,
                        std::vector<uint8_t> variables)
{
  uint8_t length, i;
  std::string name;
  enum_field_types field_type;
  /* To handle special case of 'terminating null byte'. */
  bool is_null_byte= 0;


  std::vector<uint8_t>::iterator it= variables.begin();

  while (it != variables.end())
  {
    switch (*it++)
    {
      case Q_FLAGS2_CODE:
        {
          length= 4;
          name= "flag2";
          field_type= MYSQL_TYPE_LONG;
          break;
        }
      case Q_SQL_MODE_CODE:
        {
          length= 8;
          name= "sql_mode";
          field_type= MYSQL_TYPE_LONGLONG;
          break;
        }
      case Q_CATALOG_CODE:
        length= *it++;
        name= "catalog_name_old";
        field_type= MYSQL_TYPE_VAR_STRING;
        is_null_byte= 1;
        break;
      case Q_AUTO_INCREMENT:
        length= 2;
        my_var_map->insert(std::make_pair
                           ("auto_increment_increment",
                            mysql::Value(MYSQL_TYPE_SHORT,
                                         length, (char*) &(*it))));
        for (i= 0; i < length; i++)
          it++;

        name= "auto_increment_offset";
        field_type= MYSQL_TYPE_SHORT;
        break;
      case Q_CHARSET_CODE:
        length= 2;
        my_var_map->insert(std::make_pair
                           ("character_set_client",
                            mysql::Value(MYSQL_TYPE_SHORT,
                                         length, (char*) &(*it))));
        for (i= 0; i < length; i++)
          it++;

        my_var_map->insert(std::make_pair
                           ("collation_connection",
                            mysql::Value(MYSQL_TYPE_SHORT,
                                         length, (char*) &(*it))));
        for (i= 0; i < length; i++)
          it++;

        name= "collation_server";
        field_type= MYSQL_TYPE_SHORT;
        break;
      case Q_TIME_ZONE_CODE:
        length= *it++;
        name= "time_zone";
        field_type= MYSQL_TYPE_VAR_STRING;
        break;
      case Q_CATALOG_NZ_CODE:
        length= *it++;
        name= "catalog_name";
        field_type= MYSQL_TYPE_VAR_STRING;
        break;
      case Q_LC_TIME_NAMES_CODE:
        length= 2;
        name= "lc_time_names";
        field_type= MYSQL_TYPE_SHORT;
        break;
      case Q_CHARSET_DATABASE_CODE:
        length= 2;
        name= "collation_database";
        field_type= MYSQL_TYPE_SHORT;
        break;
      case Q_TABLE_MAP_FOR_UPDATE_CODE:
        length= 8;
        name= "table_map_for_update";
        field_type= MYSQL_TYPE_LONGLONG;
        break;
      case Q_MASTER_DATA_WRITTEN_CODE:
        length= 4;
        name= "master_data_written";
        field_type= MYSQL_TYPE_LONG;
        break;
      case Q_INVOKER:
        length= *it++;
        my_var_map->insert(std::make_pair
                           ("user",
                            mysql::Value(MYSQL_TYPE_VAR_STRING,
                                         length, (char*) &(*it))));
        for (i= 0; i < length; i++)
          it++;

        length= *it++;
        name= "host";
        field_type= MYSQL_TYPE_VARCHAR;
        break;
      default:
        /* Unknown status variables. Error!! */
        return 1;
    }                                           /* switch */
    my_var_map->insert(std::make_pair
                       (name, mysql::Value(field_type, length,
                                           (char*) &(*it))));
    while (length --)
      ++it;

    /* Handle null termination byte. */
    if (is_null_byte)
    {
      ++it;
      is_null_byte= 0;
    }
  }
  return 0;
}                                               /* server_var_decoder() */

}                                               /* mysql namespace */

