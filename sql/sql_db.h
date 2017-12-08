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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef SQL_DB_INCLUDED
#define SQL_DB_INCLUDED

#include "lex_string.h"

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

bool mysql_opt_change_db(THD *thd,
                         const LEX_CSTRING &new_db_name,
                         LEX_STRING *saved_db_name,
                         bool force_switch,
                         bool *cur_db_changed);
bool get_default_db_collation(const dd::Schema &schema,
                              const CHARSET_INFO **collation);
bool get_default_db_collation(THD *thd,
                              const char *db_name,
                              const CHARSET_INFO **collation);
#endif /* SQL_DB_INCLUDED */
