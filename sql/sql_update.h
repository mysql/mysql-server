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

#ifndef SQL_UPDATE_INCLUDED
#define SQL_UPDATE_INCLUDED

#include "sql_data_change.h" // enum_duplicates
#include "sql_class.h"       // select_result_interceptor

class Item;
class multi_update;
struct TABLE_LIST;

typedef class st_select_lex SELECT_LEX;

bool mysql_prepare_update(THD *thd, const TABLE_LIST *update_table_ref);
bool mysql_update(THD *thd, List<Item> &fields,
                  List<Item> &values, ha_rows limit,
                  enum enum_duplicates handle_duplicates,
                  ha_rows *found_return, ha_rows *updated_return);
bool mysql_multi_update(THD *thd,
                        List<Item> *fields, List<Item> *values,
                        enum enum_duplicates handle_duplicates,
                        SELECT_LEX *select_lex,
                        multi_update **result);
bool records_are_comparable(const TABLE *table);
bool compare_records(const TABLE *table);
int mysql_multi_update_prepare(THD *thd);

class multi_update :public select_result_interceptor
{
  TABLE_LIST *all_tables; /* query/update command tables */
  TABLE_LIST *leaves;     /* list of leves of join table tree */
  TABLE_LIST *update_tables;
  TABLE **tmp_tables, *main_table, *table_to_update;
  Temp_table_param *tmp_table_param;
  ha_rows updated, found;
  List <Item> *fields, *values;
  List <Item> **fields_for_table, **values_for_table;
  uint table_count;
  /*
   List of tables referenced in the CHECK OPTION condition of
   the updated view excluding the updated table. 
  */
  List <TABLE> unupdated_check_opt_tables;
  Copy_field *copy_field;
  enum enum_duplicates handle_duplicates;
  bool do_update, trans_safe;
  /* True if the update operation has made a change in a transactional table */
  bool transactional_tables;
  /* 
     error handling (rollback and binlogging) can happen in send_eof()
     so that afterward send_error() needs to find out that.
  */
  bool error_handled;

  /**
     Array of update operations, arranged per _updated_ table. For each
     _updated_ table in the multiple table update statement, a COPY_INFO
     pointer is present at the table's position in this array.

     The array is allocated and populated during multi_update::prepare(). The
     position that each table is assigned is also given here and is stored in
     the member TABLE::pos_in_table_list::shared. However, this is a publicly
     available field, so nothing can be trusted about its integrity.

     This member is NULL when the multi_update is created.

     @see multi_update::prepare
  */
  COPY_INFO **update_operations;

public:
  multi_update(TABLE_LIST *ut, TABLE_LIST *leaves_list,
	       List<Item> *fields, List<Item> *values,
	       enum_duplicates handle_duplicates);
  ~multi_update();
  virtual bool need_explain_interceptor() const { return true; }
  int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
  bool send_data(List<Item> &items);
  bool initialize_tables (JOIN *join);
  void send_error(uint errcode,const char *err);
  int  do_updates();
  bool send_eof();
  inline ha_rows num_found()
  {
    return found;
  }
  inline ha_rows num_updated()
  {
    return updated;
  }
  virtual void abort_result_set();
};

#endif /* SQL_UPDATE_INCLUDED */
