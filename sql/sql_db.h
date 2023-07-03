/* Copyright (c) 2006, 2023, Oracle and/or its affiliates.

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

#ifndef SQL_DB_INCLUDED
#define SQL_DB_INCLUDED

#include "lex_string.h"

struct TABLE_SHARE;
class THD;
namespace dd {
class Schema;
}

struct CHARSET_INFO;
struct HA_CREATE_INFO;

bool mysql_create_db(THD *thd, const char *db, HA_CREATE_INFO *create);
bool mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create);
bool mysql_rm_db(THD *thd, const LEX_CSTRING &db, bool if_exists);
bool mysql_change_db(THD *thd, const LEX_CSTRING &new_db_name,
                     bool force_switch);

bool mysql_opt_change_db(THD *thd, const LEX_CSTRING &new_db_name,
                         LEX_STRING *saved_db_name, bool force_switch,
                         bool *cur_db_changed);
bool get_default_db_collation(const dd::Schema &schema,
                              const CHARSET_INFO **collation);
bool get_default_db_collation(THD *thd, const char *db_name,
                              const CHARSET_INFO **collation);
bool check_schema_readonly(THD *thd, const char *schema_name,
                           TABLE_SHARE *share = nullptr);
#endif /* SQL_DB_INCLUDED */
