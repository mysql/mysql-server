/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.
 *
   SQLClient,SQLRow,SQLCursor portions are 
   Copyright (c) 2016 Justin Swanhart
   all rights reserved

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_CLIENT_INCLUDED
#define SQL_CLIENT_INCLUDED

#include "sql_parse.h"

class SQLRow
{
  private:
  std::vector<std::string> row;
  std::vector<std::string> meta;

  public:
  SQLRow(std::vector<std::string> meta, const char* data, unsigned long long data_len); 
  const std::string at(const std::string name);
  const std::string at(const unsigned int num);
  const std::vector<std::string> get_row();
  void print();

};

class SQLCursor
{ 
  private:
  char *data;
  unsigned long long data_len;
  unsigned long long offset;
  SQLRow* cur_row;
  std::vector<std::string> meta;

  public:
  SQLCursor(const char* cols, long long col_len, const char* buf, unsigned long long buf_len );
  void reset(); 
  SQLRow* next(); 
  ~SQLCursor() ;
};


class SQLClient 
{
  private:
  THD* conn;
  std::string _sqlcode;
  std::string _sqlerr; 

  public:
  SQLClient(THD *thd) : conn(thd) {}
  std::string sqlcode();
  std::string sqlerr();
  my_bool query(std::string columns, std::string query, SQLCursor** cursor);
};
#endif
