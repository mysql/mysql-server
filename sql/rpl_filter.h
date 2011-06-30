/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
   
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

#ifndef RPL_FILTER_H
#define RPL_FILTER_H

#include "mysql.h"
#include "sql_list.h"                           /* I_List */
#include "hash.h"                               /* HASH */

class String;
struct TABLE_LIST;
typedef struct st_dynamic_array DYNAMIC_ARRAY;

typedef struct st_table_rule_ent
{
  char* db;
  char* tbl_name;
  uint key_len;
} TABLE_RULE_ENT;

/*
  Rpl_filter

  Inclusion and exclusion rules of tables and databases.
  Also handles rewrites of db.
  Used for replication and binlogging.
 */
class Rpl_filter 
{
public:
  Rpl_filter();
  ~Rpl_filter();
  Rpl_filter(Rpl_filter const&);
  Rpl_filter& operator=(Rpl_filter const&);
 
  /* Checks - returns true if ok to replicate/log */

  bool tables_ok(const char* db, TABLE_LIST* tables);
  bool db_ok(const char* db);
  bool db_ok_with_wild_table(const char *db);

  bool is_on();

  /* Setters - add filtering rules */

  int add_do_table(const char* table_spec);
  int add_ignore_table(const char* table_spec);

  int add_wild_do_table(const char* table_spec);
  int add_wild_ignore_table(const char* table_spec);

  void add_do_db(const char* db_spec);
  void add_ignore_db(const char* db_spec);

  void add_db_rewrite(const char* from_db, const char* to_db);

  /* Getters - to get information about current rules */

  void get_do_table(String* str);
  void get_ignore_table(String* str);

  void get_wild_do_table(String* str);
  void get_wild_ignore_table(String* str);

  const char* get_rewrite_db(const char* db, size_t *new_len);

  I_List<i_string>* get_do_db();
  I_List<i_string>* get_ignore_db();

private:
  bool table_rules_on;

  void init_table_rule_hash(HASH* h, bool* h_inited);
  void init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited);

  int add_table_rule(HASH* h, const char* table_spec);
  int add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec);

  void free_string_array(DYNAMIC_ARRAY *a);

  void table_rule_ent_hash_to_str(String* s, HASH* h, bool inited);
  void table_rule_ent_dynamic_array_to_str(String* s, DYNAMIC_ARRAY* a,
                                           bool inited);
  TABLE_RULE_ENT* find_wild(DYNAMIC_ARRAY *a, const char* key, int len);

  /*
    Those 4 structures below are uninitialized memory unless the
    corresponding *_inited variables are "true".
  */
  HASH do_table;
  HASH ignore_table;
  DYNAMIC_ARRAY wild_do_table;
  DYNAMIC_ARRAY wild_ignore_table;

  bool do_table_inited;
  bool ignore_table_inited;
  bool wild_do_table_inited;
  bool wild_ignore_table_inited;

  I_List<i_string> do_db;
  I_List<i_string> ignore_db;

  I_List<i_string_pair> rewrite_db;
};

extern Rpl_filter *rpl_filter;
extern Rpl_filter *binlog_filter;

#endif // RPL_FILTER_H
