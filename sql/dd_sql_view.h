#ifndef DD_SQL_VIEW_INCLUDED
#define DD_SQL_VIEW_INCLUDED
/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

class THD;
struct TABLE_LIST;
class sp_name;

/**
  Method to update metadata of views referecing "table" being renamed
  and views referecing(if there any) new table name "new_db.new_table_name".

  @param      thd                     Thread handle.
  @param      table                   Update metadata of views referencing this

  @retval     false                   Success.
  @retval     true                    Failure.
*/

bool update_referencing_views_metadata(THD *thd, const TABLE_LIST *table);


/**
  Method to update metadata of views referecing "table" being renamed
  and views referecing(if there any) new table name "new_db.new_table_name".

  @param      thd                     Thread handle.
  @param      table                   Update metadata of views referencing this
                                      table.
  @param      new_db                  New db name set in the rename operation.
  @param      new_table_name          New table name set in the rename
                                      operation.

  @retval     false                   Success.
  @retval     true                    Failure.
*/

bool update_referencing_views_metadata(THD *thd, const TABLE_LIST *table,
                                       const char *new_db,
                                       const char *new_table_name);


/**
  Method to update metadata of views using stored function.

  @param      thd        Thread handle.
  @param      spname     Name of the stored function.

  @retval     false      Success.
  @retval     true       Failure.
*/

bool update_referencing_views_metadata(THD *thd, const sp_name *spname);


/**
  Push error or warnings in case a view is invalid.

  @param        thd            Thread handle.
  @param        db             Database name.
  @param        view_name      View name.
*/
void push_view_warning_or_error(THD *thd, const char *db,
                                const char *view_name);

#endif // DD_SQL_VIEW_INCLUDED
