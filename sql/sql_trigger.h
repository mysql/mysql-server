#ifndef SQL_TRIGGER_INCLUDED
#define SQL_TRIGGER_INCLUDED

/*
   Copyright (c) 2004, 2014, Oracle and/or its affiliates. All rights reserved.

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
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

///////////////////////////////////////////////////////////////////////////

/**
  @file

  @brief
  This file contains declarations of global public functions which are used
  directly from parser/executioner to perform basic operations on triggers
  (CREATE TRIGGER, DROP TRIGGER, ALTER TABLE, DROP TABLE, ...)
*/

///////////////////////////////////////////////////////////////////////////

#include "m_string.h"

class THD;

struct TABLE_LIST;

///////////////////////////////////////////////////////////////////////////

bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create);

bool add_table_for_trigger(THD *thd,
                           const LEX_CSTRING &db_name,
                           const LEX_STRING &trigger_name,
                           bool continue_if_not_exist,
                           TABLE_LIST **table);

bool change_trigger_table_name(THD *thd,
                               const char *db_name,
                               const char *table_alias,
                               const char *table_name,
                               const char *new_db_name,
                               const char *new_table_name);

bool drop_all_triggers(THD *thd,
                       const char *db_name,
                       const char *table_name);

///////////////////////////////////////////////////////////////////////////

#endif /* SQL_TRIGGER_INCLUDED */
