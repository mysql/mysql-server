/* Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/*
  This files defines some MySQL C API functions that are server specific
*/

#include "sql_class.h"                          // system_variables
#include <algorithm>
#include "sql_client.h"

using std::min;
using std::max;

/*
  Function called by my_net_init() to set some check variables
*/

extern "C" {
void my_net_local_init(NET *net)
{
#ifndef EMBEDDED_LIBRARY
  net->max_packet=   (uint) global_system_variables.net_buffer_length;

  my_net_set_read_timeout(net, (uint)global_system_variables.net_read_timeout);
  my_net_set_write_timeout(net,
                           (uint)global_system_variables.net_write_timeout);

  net->retry_count=  (uint) global_system_variables.net_retry_count;
  net->max_packet_size= max<size_t>(global_system_variables.net_buffer_length,
                                    global_system_variables.max_allowed_packet);
#endif
}
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
      if(data[i] == 'n') {
        tmp += "\n";
      } else {
        tmp += data[i];
      }
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
my_bool SQLClient::query(std::string columns, std::string query, SQLCursor** cursor) 
{ 
  Protocol_classic *protocol= conn->get_protocol_classic();
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
  save_client_capabilities= protocol->get_client_capabilities();
  protocol->add_client_capability(CLIENT_MULTI_QUERIES);
  save_vio= protocol->get_vio();
  protocol->set_vio(NULL);

  protocol->create_command(&com_data, COM_QUERY, (uchar*)marshal_sql.c_str(), marshal_sql.length());
  dispatch_command(conn, &com_data, COM_QUERY);
  protocol->set_client_capabilities(save_client_capabilities);
  protocol->set_vio(save_vio);

  user_var_entry *entry;
  entry= (user_var_entry *) my_hash_search(&conn->user_vars, (uchar*)"sql_result", 10);
  int is_resultset=0;
  if(!entry) 
  { 
    printf("Internal SQL client failed to produce result set through call:\n%s\n", marshal_sql.c_str());
    _sqlcode = "99999";
    _sqlerr = "sys.sql_client is missing or failed to produce result set";
    return false; 
  }

  if( strncmp(entry->ptr(),"OK",2) != 0 && ((is_resultset = strncmp(entry->ptr(),"RS",2)) != 0) ) 
  { unsigned int i; 
    for(i=0;i<entry->length();++i) {
      if(entry->ptr()[i] == ' ') 
      { 
        i++;
        break; 
      }
    }
    for(;i<entry->length();++i) {
      if(entry->ptr()[i] != ':') 
      { 
        _sqlcode += entry->ptr()[i];
      } else {
        break;
      }
    }
    _sqlerr.assign(entry->ptr()+i+1,entry->length()-i-1); 

    return false; 
  }

  if(is_resultset == 0) 
  {
    entry= (user_var_entry *) my_hash_search(&conn->user_vars, (uchar*)"sql_resultset", 13);

    if(entry && entry->length() > 0) 
    { 
      *cursor = new SQLCursor(columns.c_str(), columns.length(), entry->ptr(), entry->length()); 

      /* free up the resulset in the user variable using the SET command */
      marshal_sql = std::string("SET @sql_resultset=NULL");
      protocol->add_client_capability(CLIENT_MULTI_QUERIES);
      protocol->set_vio(NULL);

      protocol->create_command(&com_data, COM_QUERY, (uchar*)marshal_sql.c_str(), marshal_sql.length());
      dispatch_command(conn, &com_data, COM_QUERY);
      protocol->set_client_capabilities(save_client_capabilities);
      protocol->set_vio(save_vio);
    }
  }

  return true;

}
