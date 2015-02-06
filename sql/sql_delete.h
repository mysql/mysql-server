/* Copyright (c) 2006, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SQL_DELETE_INCLUDED
#define SQL_DELETE_INCLUDED

#include "my_base.h"     // ha_rows
#include "sql_class.h"   // Query_result_interceptor
#include "sql_cmd_dml.h" // Sql_cmd_dml

class THD;
class Unique;
struct TABLE_LIST;

bool mysql_prepare_delete(THD *thd);
bool mysql_delete(THD *thd, ha_rows rows);
int mysql_multi_delete_prepare(THD *thd, uint *table_count);

class Query_result_delete :public Query_result_interceptor
{
  TABLE_LIST *delete_tables;
  /// Pointers to temporary files used for delayed deletion of rows
  Unique **tempfiles;
  /// Pointers to table objects matching tempfiles
  TABLE **tables;
  ha_rows deleted, found;
  uint num_of_tables;
  int error;
  /// Map of all tables to delete rows from
  table_map delete_table_map;
  /// Map of tables to delete from immediately
  table_map delete_immediate;
  // Map of transactional tables to be deleted from
  table_map transactional_table_map;
  /// Map of non-transactional tables to be deleted from
  table_map non_transactional_table_map;
  /// True if some delete operation has been performed (immediate or delayed)
  bool do_delete;
  /// True if some actual delete operation against non-transactional table done
  bool non_transactional_deleted;
  /*
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

public:
  Query_result_delete(TABLE_LIST *dt, uint num_of_tables);
  ~Query_result_delete();
  virtual bool need_explain_interceptor() const { return true; }
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int do_deletes();
  int do_table_deletes(TABLE *table);
  bool send_eof();
  inline ha_rows num_deleted()
  {
    return deleted;
  }
  virtual void abort_result_set();
};


class Sql_cmd_delete : public Sql_cmd_dml
{
public:
  virtual enum_sql_command sql_command_code() const { return SQLCOM_DELETE; }

  virtual bool execute(THD *thd);

  virtual bool prepared_statement_test(THD *thd);
  virtual bool prepare(THD *thd)
  {
    // TODO: move the mysql_prepare_delete() call there
    return false;
  }

private:
  bool mysql_prepare_delete(THD *thd);
  bool mysql_delete(THD *thd, ha_rows rows);
};


class Sql_cmd_delete_multi : public Sql_cmd_dml
{
public:
  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_DELETE_MULTI;
  }

  virtual bool execute(THD *thd);

  virtual bool prepared_statement_test(THD *thd);
  virtual bool prepare(THD *thd)
  {
    uint table_count;
    return mysql_multi_delete_prepare(thd, &table_count);
  }

private:
  int mysql_multi_delete_prepare(THD *thd, uint *table_count);
};


#endif /* SQL_DELETE_INCLUDED */
