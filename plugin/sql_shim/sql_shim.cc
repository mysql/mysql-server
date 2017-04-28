/*  Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
    02110-1301  USA */
#define MYSQL_SERVER
#include <mysql/plugin.h>
#include <sql_class.h>
#include <protocol_classic.h>
#include <locale>
#include <sql_client.h>

static my_bool query_injection_point(
  THD* thd, COM_DATA *com_data, enum enum_server_command command,
  COM_DATA* new_com_data, enum enum_server_command* new_command );

#ifdef HAVE_PSI_INTERFACE
static PSI_memory_key key_memory_sql_shim;

static PSI_memory_info all_rewrite_memory[]=
{
  { &key_memory_sql_shim, "sql_shim", 0 }
};
#else
#define key_memory_sql_shim PSI_NOT_INSTRUMENTED
#endif

static int plugin_init(MYSQL_PLUGIN)
{
  #ifdef HAVE_PSI_INTERFACE
  const char* category= "sql";
  int count;

  count= array_elements(all_rewrite_memory);
  mysql_memory_register(category, all_rewrite_memory, count);
  #endif

  return 0; /* success */
}

static int plugin_deinit(MYSQL_PLUGIN)
{ 
  //delete warp_engine;
  return 0;
}

/* SQL shim plugin descriptor */
static struct st_mysql_sqlshim sql_shim_desc =
{
  MYSQL_SQLSHIM_INTERFACE_VERSION,                  /* interface version */
  query_injection_point                             
};

/* Plugin descriptor */
mysql_declare_plugin(audit_log)
{
  MYSQL_SQLSHIM_PLUGIN,           /* plugin type                   */
  &sql_shim_desc,                 /* type specific descriptor      */
  "sql_shim",              	      /* plugin name                   */
  "Justin Swanhart",               /* author                        */
  "Example SQL shim plugin",
  PLUGIN_LICENSE_GPL,             /* license                       */
  plugin_init,                    /* plugin initializer            */
  plugin_deinit,                  /* plugin deinitializer          */
  0x0001,                         /* version                       */
  NULL,                           /* status variables              */
  NULL,                           /* system variables              */
  NULL,                           /* reserverd                     */
  0                               /* flags                         */
}
mysql_declare_plugin_end;

static my_bool query_injection_point(THD* thd, COM_DATA *com_data, enum enum_server_command command, 
                           COM_DATA* new_com_data, enum enum_server_command* new_command) 
{
  /* example rewrite rule for SHOW PASSWORD*/
  if(command == COM_QUERY) 
  { 
    std::locale loc;
    std::string old_query(com_data->com_query.query,com_data->com_query.length);
    for(unsigned int i=0;i<com_data->com_query.length;++i) {
      old_query[i] = std::toupper(old_query[i], loc);
    }

    if(old_query == "SHOW PASSWORD") 
    { 

      std::string new_query;
      SQLClient conn(thd);
      SQLCursor* stmt;
      SQLRow* row;

      if(conn.query("pw,user","select authentication_string as pw,user from mysql.user where concat(user,'@',host) = USER() or user = USER() LIMIT 1", &stmt)) 
      {
        if(stmt != NULL) 
        {
          if((row = stmt->next()))  
          { 
            new_query = "SELECT '" + row->at(0) + "'";       
          }
        } else 
        {
          delete stmt;
          return false;
        }
      } else {
        return false;
      }

      /* replace the command sent to the server */
      if(new_query != "") 
      {
        Protocol_classic *protocol= thd->get_protocol_classic();
        protocol->create_command(new_com_data, COM_QUERY, (uchar *) strdup(new_query.c_str()), new_query.length());
        *new_command = COM_QUERY;
      } else {
        if(stmt) delete stmt;
        return false;
      }
      if(stmt) delete stmt;
      return true; 
    }
  }

  /* don't replace command */
  return false;
}
