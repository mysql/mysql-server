/* Copyright (c) 2006, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include "hash.h"                               /* HASH */

class THD;
typedef struct charset_info_st CHARSET_INFO;
typedef struct st_ha_create_information HA_CREATE_INFO;
typedef struct st_mysql_lex_string LEX_STRING;
typedef struct st_mysql_const_lex_string LEX_CSTRING;

int mysql_create_db(THD *thd, const char *db, HA_CREATE_INFO *create,
                    bool silent);
bool mysql_alter_db(THD *thd, const char *db, HA_CREATE_INFO *create);
bool mysql_rm_db(THD *thd,const LEX_CSTRING &db,bool if_exists, bool silent);
bool mysql_upgrade_db(THD *thd, const LEX_CSTRING &old_db);
bool mysql_change_db(THD *thd, const LEX_CSTRING &new_db_name,
                     bool force_switch);

bool mysql_opt_change_db(THD *thd,
                         const LEX_CSTRING &new_db_name,
                         LEX_STRING *saved_db_name,
                         bool force_switch,
                         bool *cur_db_changed);
bool my_dboptions_cache_init(void);
void my_dboptions_cache_free(void);
bool check_db_dir_existence(const char *db_name);
bool load_db_opt(THD *thd, const char *path, HA_CREATE_INFO *create);
bool load_db_opt_by_name(THD *thd, const char *db_name,
                         HA_CREATE_INFO *db_create_info);
const CHARSET_INFO *get_default_db_collation(THD *thd, const char *db_name);
bool my_dbopt_init(void);
void my_dbopt_cleanup(void);

#define MY_DB_OPT_FILE "db.opt"

#endif /* SQL_DB_INCLUDED */
