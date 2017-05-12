/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SQL_DO_INCLUDED
#define SQL_DO_INCLUDED

#include <sys/types.h>

#include "my_sqlcommand.h"
#include "query_result.h"
#include "sql_select.h"

class Item;
class THD;
template <class T> class List;

class Sql_cmd_do final : public Sql_cmd_select
{
public:
  explicit Sql_cmd_do(Query_result *result_arg) : Sql_cmd_select(result_arg)
  {}

  enum_sql_command sql_command_code() const override { return SQLCOM_DO; }
};

class Query_result_do final : public Query_result
{
public:
  Query_result_do(THD *thd): Query_result(thd) {}
  bool send_result_set_metadata(List<Item>&, uint) override
  { return false; }
  bool send_data(List<Item> &items) override;
  bool send_eof() override;
  bool check_simple_select() const override { return false; }
  void abort_result_set() override {}
  void cleanup() override {}
};

#endif /* SQL_DO_INCLUDED */
