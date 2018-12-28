/* Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/*
  This files defines some MySQL C API functions that are server specific
*/

#include <stddef.h>
#include <sys/types.h>
#include <algorithm>
#include "sql_client.h"

#include "mysql_com.h"
#include "sql/mysqld.h"  // global_system_variables
#include "sql/system_variables.h"
#include "sql_class.h"
#include "item_func.h"
#include <iostream>

using std::max;
using std::min;

/*
  Function called by my_net_init() to set some check variables
*/

void my_net_local_init(NET *net) {
  net->max_packet = (uint)global_system_variables.net_buffer_length;

  my_net_set_read_timeout(net, (uint)global_system_variables.net_read_timeout);
  my_net_set_write_timeout(net,
                           (uint)global_system_variables.net_write_timeout);

  net->retry_count = (uint)global_system_variables.net_retry_count;
  net->max_packet_size =
      max<size_t>(global_system_variables.net_buffer_length,
                  global_system_variables.max_allowed_packet);
}

SQLRow::SQLRow(std::vector<std::string> meta, const char* data, unsigned long long data_len) 
{
  //printf("::%s\n", data);
  std::string tmp = "";
  bool in_escape = false;
  unsigned long long i;
  for(i = 0;i<data_len;++i)
  {
    if(!in_escape)
    {
      switch(data[i])
      {
        case '\\':
          in_escape = true;
          continue;
        case '|':
          row.push_back(tmp);
          tmp = "";
        break;

        case '"':
          continue;

        break;

        default:
          tmp += data[i];
      }
    } else {
        tmp += data[i];
      in_escape = false;
    }
  }
  row.push_back(tmp); 
  this->meta = meta;
}

const std::string SQLRow::at(const std::string name)
{
  std::vector<std::string>::iterator it;
  int i = 0;
  for(it=meta.begin(); it< meta.end(); it++)
  {
    if(name == *it)
    { return row[i]; }
    ++i;
  }

  return "";
}

const std::string SQLRow::at(const unsigned int num)
{ 
  return row[num];
}

const std::vector<std::string> SQLRow::get_row()
{ 
  return row;
}

void SQLRow::print() 
{
  for(unsigned int i=0;i<row.size();++i) 
  { printf("%d=%s\n",i,(row[i]).c_str()); }
}

SQLCursor::SQLCursor(const char* cols, long long col_len, const char* buf, unsigned long long buf_len ) {
  data = (char*)malloc(buf_len+1);
  memset(data,0,buf_len+1);
  memcpy(data, buf, buf_len);
  data_len = buf_len;
  offset = 0;

  /* populate the list of columns (what counts as metadata...) for the resultset */
  std::string tmp;
  for(int i=0;i<col_len;++i) 
  {
    if(cols[i] != ',')
    { tmp += cols[i];
    } else
    { meta.push_back(tmp);
      tmp = "";
    }
  }
  if(tmp != "")
  { meta.push_back(tmp); }

  cur_row = NULL;
}

void SQLCursor::reset() 
{ offset = 0; }

SQLRow* SQLCursor::next() 
{ 
  if(cur_row != NULL) delete cur_row;
  cur_row = NULL;

  std::string tmp = "";
  char c=0;
  while(offset < data_len) 
  { 
    if((c = data[offset++]) != '\n') 
    { 
      tmp += c;
    } else {
      break;
    } 
  }
  if(tmp != "") 
  {
    cur_row = new SQLRow(meta, tmp.c_str(), tmp.length());
  }
  return cur_row;
}

SQLCursor::~SQLCursor() 
{
  if(cur_row != NULL) delete cur_row;
  if(data != NULL) free(data);
}


std::string SQLClient::sqlcode()
{   
  return _sqlcode; 
}

std::string SQLClient::sqlerr()
{ 
  return _sqlerr; 
}

/* Only SELECT allocate cursors.  If a select returns true and does
 * not allocate a cursor, then the resultset is EMPTY.  The cursor
 * pointer is set to NULL if a cursor is not allocated.
*/
bool SQLClient::query(std::string columns, std::string query, SQLCursor** cursor) 
{ 
  Vio* save_vio;
  ulong save_client_capabilities;
  COM_DATA com_data;
  std::string new_columns;
  std::string tmp;
  *cursor = (SQLCursor*)NULL;

  if(columns == "" || query == "") 
  { 
    *cursor=(SQLCursor*)NULL;
    return false;
  }

  tmp = "";
  for(unsigned int i=0;i<query.length();++i) {
    if(query[i] == '\'') {
      tmp += "\\'";
    } else {
      tmp += query[i];
    }
  }
  query = tmp;
  tmp = "";

  for(unsigned int i=0;i<columns.length();++i) 
  {
      if(columns[i] != ',' && i != (columns.length()-1))
      { 
        tmp += columns[i];
      } else
      {
        /* slurp up last character */
        if(i==(columns.length()-1)) 
        { tmp += columns[i];  }

        /* the stored proc expects the columns to be * separated */
        if(new_columns != "") 
        { new_columns += '*'; }

        int len = 0;
        /* reserve some extra space for the overhead of multiple expressions */
        char *c = (char*)malloc((len=(tmp.length() + 128)*4));
        char *d;
        if(c == NULL) 
        { 
          printf("Internal SQL client out of memory\n");
          _sqlcode = "99998";
          _sqlerr = "Internal SQL client out of memory\n";
          return false; 
        }

        memset(c,0,len);
        snprintf(c,len-1,"REPLACE(`%s`,''\"'',''\\\\\\\\\"'')", tmp.c_str());
        snprintf(c,len-1,"REPLACE(%s,\"\\\\n\",\"\\\\\\\\n\")", (d=strdup(c)));
        free(d);
        int real_len = snprintf(c,len-1,"IFNULL(%s,\"\\\\N\")", (d=strdup(c)));
        free(d);
        new_columns += std::string(c,real_len);
        free(c);
        tmp = "";
      }  
  }
  _sqlcode = "";
  _sqlerr = "";

  std::string marshal_sql = std::string("CALL sys.sql_client('") + new_columns + "','" + query + "');";
  save_client_capabilities= conn->get_protocol_classic()->get_client_capabilities();
  conn->get_protocol_classic()->add_client_capability(CLIENT_MULTI_QUERIES);
  save_vio= conn->get_protocol_classic()->get_vio();
  conn->get_protocol_classic()->set_vio(NULL);

  conn->get_protocol_classic()->create_command(&com_data, COM_QUERY, (uchar*)marshal_sql.c_str(), marshal_sql.length());
  dispatch_command(conn, &com_data, COM_QUERY);
  conn->get_protocol_classic()->set_client_capabilities(save_client_capabilities);
  conn->get_protocol_classic()->set_vio(save_vio);

  std::string resultset;
  std::string sql_result;
  /* Protects conn->user_vars. */
  mysql_mutex_lock(&conn->LOCK_thd_data);

  const auto it = conn->user_vars.find("sql_resultset");
  if (it->second->length() > 0) {
	  resultset.assign((char*)it->second->ptr(), it->second->length());
  }

  const auto it2 = conn->user_vars.find("sql_result");
  if (it2->second->length() > 0) {
	  sql_result.assign((char*)it2->second->ptr(), it2->second->length());
  }

  mysql_mutex_unlock(&conn->LOCK_thd_data);
  if(!resultset.length()) 
  { 
    printf("Internal SQL client failed to produce result set through call:\n%s\n", marshal_sql.c_str());
    _sqlcode = "99999";
    _sqlerr = "sys.sql_client is missing or failed to produce result set";
    return false; 
  }

  if((sql_result != "OK") && (sql_result != "RS")) 
  { unsigned int i; 
    for(i=0;i<sql_result.length();++i) {
      if(sql_result.c_str()[i] == ' ') 
      { 
        i++;
        break; 
      }
    }
    for(;i<resultset.length();++i) {
      if(sql_result.c_str()[i] != ':') 
      { 
        _sqlcode += sql_result.c_str()[i];
      } else {
        break;
      }
    }
    _sqlerr.copy((char*)sql_result.c_str()+i+1,resultset.length()-i-1,0); 

    return false; 
  }
  if(sql_result == "RS") 
  { if(resultset.length() > 0) 
    { *cursor = new SQLCursor(columns.c_str(), columns.length(), resultset.c_str(), resultset.length()); 

      /* free up the resultset in the user variable using the SET command */
      marshal_sql = std::string("SET @sql_resultset=NULL");
      conn->get_protocol_classic()->add_client_capability(CLIENT_MULTI_QUERIES);
      conn->get_protocol_classic()->set_vio(NULL);

      conn->get_protocol_classic()->create_command(&com_data, COM_QUERY, (uchar*)marshal_sql.c_str(), marshal_sql.length());
      dispatch_command(conn, &com_data, COM_QUERY);
      conn->get_protocol_classic()->set_client_capabilities(save_client_capabilities);
      conn->get_protocol_classic()->set_vio(save_vio);
    }
  }

  return true;

}
