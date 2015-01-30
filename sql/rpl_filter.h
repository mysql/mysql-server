/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef RPL_FILTER_H
#define RPL_FILTER_H

#include "my_global.h"
#include "hash.h"                               // HASH
#include "mysqld.h"                             // options_mysqld
#include "prealloced_array.h"                   // Prealloced_arrray
#include "sql_cmd.h"                            // Sql_cmd
#include "sql_list.h"                           // I_List

class Item;
class String;
struct TABLE_LIST;


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

  bool is_rewrite_empty();

  /* Setters - add filtering rules */
  int build_do_table_hash();
  int build_ignore_table_hash();

  int add_string_list(I_List<i_string> *list, const char* spec);
  int add_string_pair_list(I_List<i_string_pair> *list, char* key, char *val);
  int add_do_table_array(const char* table_spec);
  int add_ignore_table_array(const char* table_spec);

  int add_wild_do_table(const char* table_spec);
  int add_wild_ignore_table(const char* table_spec);

  int set_do_db(List<Item> *list);
  int set_ignore_db(List<Item> *list);
  int set_do_table(List<Item> *list);
  int set_ignore_table(List<Item> *list);
  int set_wild_do_table(List<Item> *list);
  int set_wild_ignore_table(List<Item> *list);
  int set_db_rewrite(List<Item> *list);
  typedef int (Rpl_filter::*Add_filter)(char const*);
  int parse_filter_list(List<Item> *item_list, Add_filter func);
  int add_do_db(const char* db_spec);
  int add_ignore_db(const char* db_spec);

  int add_db_rewrite(const char* from_db, const char* to_db);

  /* Getters - to get information about current rules */

  void get_do_table(String* str);
  void get_ignore_table(String* str);

  void get_wild_do_table(String* str);
  void get_wild_ignore_table(String* str);

  const char* get_rewrite_db(const char* db, size_t *new_len);
  void get_rewrite_db(String *str);

  I_List<i_string>* get_do_db();
  I_List<i_string>* get_ignore_db();
  void free_string_list(I_List<i_string> *l);
  void free_string_pair_list(I_List<i_string_pair> *l);


private:
  bool table_rules_on;

  typedef Prealloced_array<TABLE_RULE_ENT*, 16, true> Table_rule_array;

  void init_table_rule_hash(HASH* h, bool* h_inited);
  void init_table_rule_array(Table_rule_array*, bool* a_inited);

  int add_table_rule_to_array(Table_rule_array* a, const char* table_spec);
  int add_table_rule_to_hash(HASH* h, const char* table_spec, uint len);

  void free_string_array(Table_rule_array *a);

  void table_rule_ent_hash_to_str(String* s, HASH* h, bool inited);
  void table_rule_ent_dynamic_array_to_str(String* s, Table_rule_array* a,
                                           bool inited);
  TABLE_RULE_ENT* find_wild(Table_rule_array *a, const char* key, size_t len);

  int build_table_hash_from_array(Table_rule_array *table_array,
                                  HASH *table_hash,
                                  bool array_inited, bool *hash_inited);

  /*
    Those 6 structures below are uninitialized memory unless the
    corresponding *_inited variables are "true".
  */
  /* For quick search */
  HASH do_table_hash;
  HASH ignore_table_hash;

  Table_rule_array do_table_array;
  Table_rule_array ignore_table_array;

  Table_rule_array wild_do_table;
  Table_rule_array wild_ignore_table;

  bool do_table_hash_inited;
  bool ignore_table_hash_inited;
  bool do_table_array_inited;
  bool ignore_table_array_inited;
  bool wild_do_table_inited;
  bool wild_ignore_table_inited;

  I_List<i_string> do_db;
  I_List<i_string> ignore_db;

  I_List<i_string_pair> rewrite_db;
};


/** Sql_cmd_change_repl_filter represents the command CHANGE REPLICATION
 * FILTER.
 */
class Sql_cmd_change_repl_filter : public Sql_cmd
{
public:
  /** Constructor.  */
  Sql_cmd_change_repl_filter():
    do_db_list(NULL), ignore_db_list(NULL),
    do_table_list(NULL), ignore_table_list(NULL),
    wild_do_table_list(NULL), wild_ignore_table_list(NULL),
    rewrite_db_pair_list(NULL)
  {}

  ~Sql_cmd_change_repl_filter()
  {}

  virtual enum_sql_command sql_command_code() const
  {
    return SQLCOM_CHANGE_REPLICATION_FILTER;
  }
  bool execute(THD *thd);

  void set_filter_value(List<Item>* item_list, options_mysqld filter_type);
  bool change_rpl_filter(THD* thd);

private:

  List<Item> *do_db_list;
  List<Item> *ignore_db_list;
  List<Item> *do_table_list;
  List<Item> *ignore_table_list;
  List<Item> *wild_do_table_list;
  List<Item> *wild_ignore_table_list;
  List<Item> *rewrite_db_pair_list;

};

extern Rpl_filter *rpl_filter;
extern Rpl_filter *binlog_filter;

#endif // RPL_FILTER_H
