/* Copyright (c) 2000, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "rpl_filter.h"

#include "auth_common.h"                // SUPER_ACL
#include "item.h"                       // Item
#include "rpl_mi.h"                     // Master_info
#include "rpl_msr.h"                    // channel_map
#include "rpl_rli.h"                    // Relay_log_info
#include "rpl_slave.h"                  // SLAVE_SQL
#include "table.h"                      // TABLE_LIST
#include "template_utils.h"             // my_free_container_pointers


#define TABLE_RULE_HASH_SIZE   16
extern PSI_memory_key key_memory_array_buffer;

Rpl_filter::Rpl_filter() :
  table_rules_on(false),
  do_table_array(key_memory_TABLE_RULE_ENT),
  ignore_table_array(key_memory_TABLE_RULE_ENT),
  wild_do_table(key_memory_TABLE_RULE_ENT),
  wild_ignore_table(key_memory_TABLE_RULE_ENT),
  do_table_hash_inited(0), ignore_table_hash_inited(0),
  do_table_array_inited(0), ignore_table_array_inited(0),
  wild_do_table_inited(0), wild_ignore_table_inited(0)
{
  do_db.empty();
  ignore_db.empty();
  rewrite_db.empty();
}


Rpl_filter::~Rpl_filter() 
{
  if (do_table_hash_inited)
    my_hash_free(&do_table_hash);
  if (ignore_table_hash_inited)
    my_hash_free(&ignore_table_hash);

  free_string_array(&do_table_array);
  free_string_array(&ignore_table_array);
  free_string_array(&wild_do_table);
  free_string_array(&wild_ignore_table);

  free_string_list(&do_db);
  free_string_list(&ignore_db);
  free_string_pair_list(&rewrite_db);
}


/*
  Returns true if table should be logged/replicated 

  SYNOPSIS
    tables_ok()
    db              db to use if db in TABLE_LIST is undefined for a table
    tables          list of tables to check

  NOTES
    Changing table order in the list can lead to different results. 
    
    Note also order of precedence of do/ignore rules (see code).  For
    that reason, users should not set conflicting rules because they
    may get unpredicted results (precedence order is explained in the
    manual).

    If no table in the list is marked "updating", then we always
    return 0, because there is no reason to execute this statement on
    slave if it updates nothing.  (Currently, this can only happen if
    statement is a multi-delete (SQLCOM_DELETE_MULTI) and "tables" are
    the tables in the FROM):

    In the case of SQLCOM_DELETE_MULTI, there will be a second call to
    tables_ok(), with tables having "updating==TRUE" (those after the
    DELETE), so this second call will make the decision (because
    all_tables_not_ok() = !tables_ok(1st_list) &&
    !tables_ok(2nd_list)).

  TODO
    "Include all tables like "abc.%" except "%.EFG"". (Can't be done now.)
    If we supported Perl regexps, we could do it with pattern: /^abc\.(?!EFG)/
    (I could not find an equivalent in the regex library MySQL uses).

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

bool 
Rpl_filter::tables_ok(const char* db, TABLE_LIST* tables)
{
  bool some_tables_updating= 0;
  DBUG_ENTER("Rpl_filter::tables_ok");

  for (; tables; tables= tables->next_global)
  {
    char hash_key[2*NAME_LEN+2];
    char *end;
    uint len;

    if (!tables->updating) 
      continue;
    some_tables_updating= 1;
    end= my_stpcpy(hash_key, tables->db ? tables->db : db);
    *end++= '.';
    len= (uint) (my_stpcpy(end, tables->table_name) - hash_key);
    if (do_table_hash_inited) // if there are any do's
    {
      if (my_hash_search(&do_table_hash, (uchar*) hash_key, len))
	DBUG_RETURN(1);
    }
    if (ignore_table_hash_inited) // if there are any ignores
    {
      if (my_hash_search(&ignore_table_hash, (uchar*) hash_key, len))
	DBUG_RETURN(0); 
    }
    if (wild_do_table_inited && 
	find_wild(&wild_do_table, hash_key, len))
      DBUG_RETURN(1);
    if (wild_ignore_table_inited && 
	find_wild(&wild_ignore_table, hash_key, len))
      DBUG_RETURN(0);
  }

  /*
    If no table was to be updated, ignore statement (no reason we play it on
    slave, slave is supposed to replicate _changes_ only).
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  DBUG_RETURN(some_tables_updating &&
              !do_table_hash_inited && !wild_do_table_inited);
}


/*
  Checks whether a db matches some do_db and ignore_db rules

  SYNOPSIS
    db_ok()
    db              name of the db to check

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated                  
*/

bool
Rpl_filter::db_ok(const char* db)
{
  DBUG_ENTER("Rpl_filter::db_ok");

  if (do_db.is_empty() && ignore_db.is_empty())
    DBUG_RETURN(1); // Ok to replicate if the user puts no constraints

  /*
    Previous behaviour "if the user has specified restrictions on which
    databases to replicate and db was not selected, do not replicate" has
    been replaced with "do replicate".
    Since the filtering criteria is not equal to "NULL" the statement should
    be logged into binlog.
  */
  if (!db)
    DBUG_RETURN(1);

  if (!do_db.is_empty()) // if the do's are not empty
  {
    I_List_iterator<i_string> it(do_db);
    i_string* tmp;

    while ((tmp=it++))
    {
      /*
        Filters will follow the setting of lower_case_table_name
        to be case sensitive when setting lower_case_table_name=0.
        Otherwise they will be case insensitive but accent sensitive.
      */
      if (!my_strcasecmp(table_alias_charset, tmp->ptr, db))
        DBUG_RETURN(1); // match
    }
    DBUG_RETURN(0);
  }
  else // there are some elements in the don't, otherwise we cannot get here
  {
    I_List_iterator<i_string> it(ignore_db);
    i_string* tmp;

    while ((tmp=it++))
    {
      /*
        Filters will follow the setting of lower_case_table_name
        to be case sensitive when setting lower_case_table_name=0.
        Otherwise they will be case insensitive but accent sensitive.
      */
      if (!my_strcasecmp(table_alias_charset, tmp->ptr, db))
        DBUG_RETURN(0); // match
    }
    DBUG_RETURN(1);
  }
}


/*
  Checks whether a db matches wild_do_table and wild_ignore_table
  rules (for replication)

  SYNOPSIS
    db_ok_with_wild_table()
    db		name of the db to check.
		Is tested with check_db_name() before calling this function.

  NOTES
    Here is the reason for this function.
    We advise users who want to exclude a database 'db1' safely to do it
    with replicate_wild_ignore_table='db1.%' instead of binlog_ignore_db or
    replicate_ignore_db because the two lasts only check for the selected db,
    which won't work in that case:
    USE db2;
    UPDATE db1.t SET ... #this will be replicated and should not
    whereas replicate_wild_ignore_table will work in all cases.
    With replicate_wild_ignore_table, we only check tables. When
    one does 'DROP DATABASE db1', tables are not involved and the
    statement will be replicated, while users could expect it would not (as it
    rougly means 'DROP db1.first_table, DROP db1.second_table...').
    In other words, we want to interpret 'db1.%' as "everything touching db1".
    That is why we want to match 'db1' against 'db1.%' wild table rules.

  RETURN VALUES
    0           should not be logged/replicated
    1           should be logged/replicated
*/

bool
Rpl_filter::db_ok_with_wild_table(const char *db)
{
  DBUG_ENTER("Rpl_filter::db_ok_with_wild_table");

  char hash_key[NAME_LEN+2];
  char *end;
  size_t len;
  end= my_stpcpy(hash_key, db);
  *end++= '.';
  len= end - hash_key ;
  if (wild_do_table_inited && find_wild(&wild_do_table, hash_key, len))
  {
    DBUG_PRINT("return",("1"));
    DBUG_RETURN(1);
  }
  if (wild_ignore_table_inited && find_wild(&wild_ignore_table, hash_key, len))
  {
    DBUG_PRINT("return",("0"));
    DBUG_RETURN(0);
  }  

  /*
    If no explicit rule found and there was a do list, do not replicate.
    If there was no do list, go ahead
  */
  DBUG_PRINT("return",("db=%s,retval=%d", db, !wild_do_table_inited));
  DBUG_RETURN(!wild_do_table_inited);
}


bool
Rpl_filter::is_on()
{
  return table_rules_on;
}


bool
Rpl_filter::is_rewrite_empty()
{
  return rewrite_db.is_empty();
}


int
Rpl_filter::add_do_table_array(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_do_table");
  if (!do_table_array_inited)
    init_table_rule_array(&do_table_array, &do_table_array_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_table_rule_to_array(&do_table_array, table_spec));
}


int
Rpl_filter::add_ignore_table_array(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_ignore_table");
  if (!ignore_table_array_inited)
    init_table_rule_array(&ignore_table_array, &ignore_table_array_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_table_rule_to_array(&ignore_table_array, table_spec));
}


int 
Rpl_filter::add_wild_do_table(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_wild_do_table");
  if (!wild_do_table_inited)
    init_table_rule_array(&wild_do_table, &wild_do_table_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_table_rule_to_array(&wild_do_table, table_spec));
}
  

int 
Rpl_filter::add_wild_ignore_table(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_wild_ignore_table");
  if (!wild_ignore_table_inited)
    init_table_rule_array(&wild_ignore_table, &wild_ignore_table_inited);
  table_rules_on= 1;
  int ret= add_table_rule_to_array(&wild_ignore_table, table_spec);
  DBUG_RETURN(ret);
}

int
Rpl_filter::add_db_rewrite(const char* from_db, const char* to_db)
{
  DBUG_ENTER("Rpl_filter::add_db_rewrite");
  int ret= add_string_pair_list(&rewrite_db, (char*)from_db, (char*)to_db);
  DBUG_RETURN(ret);
}

/*
  Build do_table rules to HASH from dynamic array
  for faster filter checking.

  @return
             0           ok
             1           error
*/
int
Rpl_filter::build_do_table_hash()
{
  DBUG_ENTER("Rpl_filter::build_do_table_hash");

  if (build_table_hash_from_array(&do_table_array, &do_table_hash,
                       do_table_array_inited, &do_table_hash_inited))
    DBUG_RETURN(1);

  /* Free do table ARRAY as it is a copy in do table HASH */
  if (do_table_array_inited)
  {
    free_string_array(&do_table_array);
    do_table_array_inited= FALSE;
  }

  DBUG_RETURN(0);
}

/*
  Build ignore_table rules to HASH from dynamic array
  for faster filter checking.

  @return
             0           ok
             1           error
*/
int
Rpl_filter::build_ignore_table_hash()
{
  DBUG_ENTER("Rpl_filter::build_ignore_table_hash");

  if (build_table_hash_from_array(&ignore_table_array, &ignore_table_hash,
                       ignore_table_array_inited, &ignore_table_hash_inited))
    DBUG_RETURN(1);

  /* Free ignore table ARRAY as it is a copy in ignore table HASH */
  if (ignore_table_array_inited)
  {
    free_string_array(&ignore_table_array);
    ignore_table_array_inited= FALSE;
  }

  DBUG_RETURN(0);
}


/**
  Table rules are initially added to DYNAMIC_LIST, and then,
  when the charset to use for tables has been established,
  inserted into a HASH for faster filter checking.

  @param[in] table_array         dynamic array stored table rules
  @param[in] table_hash          HASH for storing table rules
  @param[in] array_inited        Table rules are added to dynamic array
  @param[in] hash_inited         Table rules are added to HASH

  @return
             0           ok
             1           error
*/
int
Rpl_filter::build_table_hash_from_array(Table_rule_array *table_array,
                                        HASH *table_hash,
                                        bool array_inited, bool *hash_inited)
{
  DBUG_ENTER("Rpl_filter::build_table_hash");

  if (array_inited)
  {
    init_table_rule_hash(table_hash, hash_inited);
    for (size_t i= 0; i < table_array->size(); i++)
    {
      TABLE_RULE_ENT* e= table_array->at(i);
      if (add_table_rule_to_hash(table_hash, e->db, e->key_len))
        DBUG_RETURN(1);
    }
  }

  DBUG_RETURN(0);
}


/**
  Added one table rule to HASH.

  @param[in] h                   HASH for storing table rules
  @param[in] table_spec          Table name with db
  @param[in] len                 The length of table_spec

  @return
             0           ok
             1           error
*/
int
Rpl_filter::add_table_rule_to_hash(HASH* h, const char* table_spec, uint len)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(key_memory_TABLE_RULE_ENT,
                                                 sizeof(TABLE_RULE_ENT)
                                                 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db= (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name= e->db + (dot - table_spec) + 1;
  e->key_len= len;
  memcpy(e->db, table_spec, len);

  if (my_hash_insert(h, (uchar*)e))
  {
    my_free(e);
    return 1;
  }
  return 0;
}


/*
  Add table expression to dynamic array
*/

int
Rpl_filter::add_table_rule_to_array(Table_rule_array* a, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  size_t len = strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(key_memory_TABLE_RULE_ENT,
                                                 sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db= (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name= e->db + (dot - table_spec) + 1;
  e->key_len= len;
  memcpy(e->db, table_spec, len);

  if (a->push_back(e))
  {
    my_free(e);
    return 1;
  }
  return 0;
}

int
Rpl_filter::parse_filter_list(List<Item> *item_list, Add_filter add)
{
  DBUG_ENTER("Rpl_filter::parse_filter_rule");
  int status= 0;
  if (item_list->is_empty()) /* to support '()' syntax */
    DBUG_RETURN(status);
  List_iterator_fast<Item> it(*item_list);
  Item * item;
  while ((item= it++))
  {
    String buf;
    status = (this->*add)(item->val_str(&buf)->c_ptr());
    if (status)
      break;
  }
  DBUG_RETURN(status);
}


int
Rpl_filter::set_do_db(List<Item> *do_db_list)
{
  DBUG_ENTER("Rpl_filter::set_do_db");
  if (!do_db_list)
    DBUG_RETURN(0);
  free_string_list(&do_db);
  int ret= parse_filter_list(do_db_list, &Rpl_filter::add_do_db);
  DBUG_RETURN(ret);
}

int
Rpl_filter::set_ignore_db(List<Item> *ignore_db_list)
{
  DBUG_ENTER("Rpl_filter::set_ignore_db");
  if (!ignore_db_list)
    DBUG_RETURN(0);
  free_string_list(&ignore_db);
  int ret= parse_filter_list(ignore_db_list, &Rpl_filter::add_ignore_db);
  DBUG_RETURN(ret);
}

int
Rpl_filter::set_do_table(List<Item> *do_table_list)
{
  DBUG_ENTER("Rpl_filter::set_do_table");
  if (!do_table_list)
    DBUG_RETURN(0);
  int status;
  if (do_table_hash_inited)
    my_hash_free(&do_table_hash);
  if (do_table_array_inited)
    free_string_array(&do_table_array); /* purecov: inspected */
  status= parse_filter_list(do_table_list, &Rpl_filter::add_do_table_array);
  if (!status)
  {
    status = build_do_table_hash();
    if (do_table_hash_inited && !do_table_hash.records)
    {
      my_hash_free(&do_table_hash);
      do_table_hash_inited= 0;
    }
  }
  DBUG_RETURN(status);
}

int
Rpl_filter::set_ignore_table(List<Item>* ignore_table_list)
{
  DBUG_ENTER("Rpl_filter::set_ignore_table");
  if (!ignore_table_list)
    DBUG_RETURN(0);
  int status;
  if (ignore_table_hash_inited)
    my_hash_free(&ignore_table_hash);
  if (ignore_table_array_inited)
    free_string_array(&ignore_table_array); /* purecov: inspected */
  status= parse_filter_list(ignore_table_list, &Rpl_filter::add_ignore_table_array);
  if (!status)
  {
    status = build_ignore_table_hash();
    if (ignore_table_hash_inited && !ignore_table_hash.records)
    {
      my_hash_free(&ignore_table_hash);
      ignore_table_hash_inited= 0;
    }
  }
  DBUG_RETURN(status);
}

int
Rpl_filter::set_wild_do_table(List<Item> *wild_do_table_list)
{
  DBUG_ENTER("Rpl_filter::set_wild_do_table");
  if (!wild_do_table_list)
    DBUG_RETURN(0);
  int status;
  if (wild_do_table_inited)
    free_string_array(&wild_do_table);

  status= parse_filter_list(wild_do_table_list, &Rpl_filter::add_wild_do_table);

  if (wild_do_table.empty())
  {
    wild_do_table.shrink_to_fit();
    wild_do_table_inited= 0;
  }
  DBUG_RETURN(status);
}

int
Rpl_filter::set_wild_ignore_table(List<Item> *wild_ignore_table_list)
{
  DBUG_ENTER("Rpl_filter::set_wild_ignore_table");
  if (!wild_ignore_table_list)
    DBUG_RETURN(0);
  int status;
  if (wild_ignore_table_inited)
    free_string_array(&wild_ignore_table);

  status= parse_filter_list(wild_ignore_table_list, &Rpl_filter::add_wild_ignore_table);

  if (wild_ignore_table.empty())
  {
    wild_ignore_table.shrink_to_fit();
    wild_ignore_table_inited= 0;
  }
  DBUG_RETURN(status);
}

int
Rpl_filter::set_db_rewrite(List<Item> *rewrite_db_pair_list)
{
  DBUG_ENTER("Rpl_filter::set_db_rewrite");
  if (!rewrite_db_pair_list)
    DBUG_RETURN(0);
  int status= 0;
  free_string_pair_list(&rewrite_db);
  if (rewrite_db_pair_list->is_empty()) /* to support '()' syntax */
    DBUG_RETURN(status);
  List_iterator_fast<Item> it(*rewrite_db_pair_list);
  Item * db_key, *db_val;

  /* Please note that grammer itself allows only even number of db values. So
   * it is ok to do it++ twice without checking anything. */
  db_key= it++;
  db_val= it++;
  while (db_key && db_val)
  {
    String buf1, buf2;
    status = add_db_rewrite(db_key->val_str(&buf1)->c_ptr(), db_val->val_str(&buf2)->c_ptr());
    if (status)
      break;
    db_key= it++;
    db_val= it++;
  }
  DBUG_RETURN(status);
}

int
Rpl_filter::add_string_list(I_List<i_string> *list, const char* spec)
{
  char *str;
  i_string *node;

  if (! (str= my_strdup(key_memory_rpl_filter, spec, MYF(MY_WME))))
    return true; /* purecov: inspected */

  if (! (node= new i_string(str)))
  {
    /* purecov: begin inspected */
    my_free(str);
    return true;
    /* purecov: end */
  }

  list->push_back(node);

  return false;
}

int
Rpl_filter::add_string_pair_list(I_List<i_string_pair> *list, char* key, char *val)
{
  char *dup_key, *dup_val;
  i_string_pair *node;

  if (! (dup_key= my_strdup(key_memory_rpl_filter, key, MYF(MY_WME))))
    return true; /* purecov: inspected */
  if (! (dup_val= my_strdup(key_memory_rpl_filter, val, MYF(MY_WME))))
  {
    /* purecov: begin inspected */
    my_free(dup_key);
    return true;
    /* purecov: end */
  }

  if (! (node= new i_string_pair(dup_key, dup_val)))
  {
    /* purecov: begin inspected */
    my_free(dup_key);
    my_free(dup_val);
    return true;
    /* purecov: end */
  }

  list->push_back(node);

  return false;
}

int
Rpl_filter::add_do_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_do_db");
  int ret= add_string_list(&do_db, table_spec);
  DBUG_RETURN(ret);
}

int
Rpl_filter::add_ignore_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_ignore_db");
  int ret= add_string_list(&ignore_db, table_spec);
  DBUG_RETURN(ret);
}

extern "C" uchar *get_table_key(const uchar *, size_t *, my_bool);
extern "C" void free_table_ent(void* a);

uchar *get_table_key(const uchar* a, size_t *len,
                     my_bool MY_ATTRIBUTE((unused)))
{
  TABLE_RULE_ENT *e= (TABLE_RULE_ENT *) a;

  *len= e->key_len;
  return (uchar*)e->db;
}


void free_table_ent(void* a)
{
  TABLE_RULE_ENT *e= (TABLE_RULE_ENT *) a;
  
  my_free(e);
}


void
Rpl_filter::init_table_rule_hash(HASH* h, bool* h_inited)
{
  my_hash_init(h, table_alias_charset, TABLE_RULE_HASH_SIZE,0,0,
               get_table_key, free_table_ent, 0,
               key_memory_TABLE_RULE_ENT);
  *h_inited = 1;
}


void 
Rpl_filter::init_table_rule_array(Table_rule_array* a, bool* a_inited)
{
  a->clear();
  *a_inited = 1;
}


TABLE_RULE_ENT* 
Rpl_filter::find_wild(Table_rule_array *a, const char* key, size_t len)
{
  const char* key_end= key + len;

  for (size_t i= 0; i < a->size(); i++)
  {
    TABLE_RULE_ENT* e= a->at(i);
    /*
      Filters will follow the setting of lower_case_table_name
      to be case sensitive when setting lower_case_table_name=0.
      Otherwise they will be case insensitive but accent sensitive.
    */
    if (!my_wildcmp(table_alias_charset, key, key_end,
		    (const char*)e->db,
		    (const char*)(e->db + e->key_len),
		    '\\',wild_one,wild_many))
      return e;
  }
  
  return 0;
}


void 
Rpl_filter::free_string_array(Table_rule_array *a)
{
  my_free_container_pointers(*a);
  a->shrink_to_fit();
}

void
Rpl_filter::free_string_list(I_List<i_string> *l)
{
  void *ptr;
  i_string *tmp;

  while ((tmp= l->get()))
  {
    ptr= (void *) tmp->ptr;
    my_free(ptr);
    delete tmp;
  }

  l->empty();
}

void
Rpl_filter::free_string_pair_list(I_List<i_string_pair> *pl)
{
  i_string_pair *tmp;
  while ((tmp= pl->get()))
  {
    my_free((void*)tmp->key);
    my_free((void*)tmp->val);
    delete tmp;
  }

  pl->empty();
}

/*
  Builds a String from a HASH of TABLE_RULE_ENT. Cannot be used for any other 
  hash, as it assumes that the hash entries are TABLE_RULE_ENT.

  SYNOPSIS
    table_rule_ent_hash_to_str()
    s               pointer to the String to fill
    h               pointer to the HASH to read

  RETURN VALUES
    none
*/

void 
Rpl_filter::table_rule_ent_hash_to_str(String* s, HASH* h, bool inited)
{
  s->length(0);
  if (inited)
  {
    for (uint i= 0; i < h->records; i++)
    {
      TABLE_RULE_ENT* e= (TABLE_RULE_ENT*) my_hash_element(h, i);
      if (s->length())
        s->append(',');
      s->append(e->db,e->key_len);
    }
  }
}


void 
Rpl_filter::table_rule_ent_dynamic_array_to_str(String* s, Table_rule_array* a,
                                                bool inited)
{
  s->length(0);
  if (inited)
  {
    for (size_t i= 0; i < a->size(); i++)
    {
      TABLE_RULE_ENT* e= a->at(i);
      if (s->length())
        s->append(',');
      s->append(e->db,e->key_len);
    }
  }
}


void
Rpl_filter::get_do_table(String* str)
{
  table_rule_ent_hash_to_str(str, &do_table_hash, do_table_hash_inited);
}


void
Rpl_filter::get_ignore_table(String* str)
{
  table_rule_ent_hash_to_str(str, &ignore_table_hash, ignore_table_hash_inited);
}


void
Rpl_filter::get_wild_do_table(String* str)
{
  table_rule_ent_dynamic_array_to_str(str, &wild_do_table, wild_do_table_inited);
}


void
Rpl_filter::get_wild_ignore_table(String* str)
{
  table_rule_ent_dynamic_array_to_str(str, &wild_ignore_table, wild_ignore_table_inited);
}

void
Rpl_filter::get_rewrite_db(String *str)
{
  str->length(0);
  if (!rewrite_db.is_empty())
  {
    I_List_iterator<i_string_pair> it(rewrite_db);
    i_string_pair* s;
    while ((s= it++))
    {
      str->append('(');
      str->append(s->key);
      str->append(',');
      str->append(s->val);
      str->append(')');
      str->append(',');
    }
    // Remove last ',' str->chop();
    str->chop();
  }
}

const char*
Rpl_filter::get_rewrite_db(const char* db, size_t *new_len)
{
  if (rewrite_db.is_empty() || !db)
    return db;
  I_List_iterator<i_string_pair> it(rewrite_db);
  i_string_pair* tmp;

  while ((tmp=it++))
  {
    /*
      Filters will follow the setting of lower_case_table_name
      to be case sensitive when setting lower_case_table_name=0.
      Otherwise they will be case insensitive but accent sensitive.
    */
    if (!my_strcasecmp(table_alias_charset, tmp->key, db))
    {
      *new_len= strlen(tmp->val);
      return tmp->val;
    }
  }
  return db;
}


I_List<i_string>*
Rpl_filter::get_do_db()
{
  return &do_db;
}
  

I_List<i_string>*
Rpl_filter::get_ignore_db()
{
  return &ignore_db;
}

bool Sql_cmd_change_repl_filter::execute(THD *thd)
{
  DBUG_ENTER("Sql_cmd_change_rpl_filter::execute");
  bool rc= change_rpl_filter(thd);
  DBUG_RETURN(rc);
}

void Sql_cmd_change_repl_filter::set_filter_value(List<Item>* item_list,
                                                  options_mysqld filter_type)
{
  DBUG_ENTER("Sql_cmd_change_repl_filter::set_filter_rule");
  switch (filter_type)
  {
  case OPT_REPLICATE_DO_DB:
    do_db_list= item_list;
    break;
  case OPT_REPLICATE_IGNORE_DB:
    ignore_db_list= item_list;
    break;
  case OPT_REPLICATE_DO_TABLE:
    do_table_list= item_list;
    break;
  case OPT_REPLICATE_IGNORE_TABLE:
    ignore_table_list= item_list;
    break;
  case OPT_REPLICATE_WILD_DO_TABLE:
    wild_do_table_list= item_list;
    break;
  case OPT_REPLICATE_WILD_IGNORE_TABLE:
    wild_ignore_table_list= item_list;
    break;
  case OPT_REPLICATE_REWRITE_DB:
    rewrite_db_pair_list= item_list;
    break;
  default:
    /* purecov: begin deadcode */
    DBUG_ASSERT(0);
    break;
    /* purecov: end */
  }
  DBUG_VOID_RETURN;
}

/**
  Execute a CHANGE REPLICATION FILTER statement to set filter rules.

  @param thd A pointer to the thread handler object.

  @param mi Pointer to Master_info object belonging to the slave's IO
  thread.

  @retval FALSE success
  @retval TRUE error
 */
bool Sql_cmd_change_repl_filter::change_rpl_filter(THD* thd)
{
  DBUG_ENTER("change_rpl_filter");
  bool ret= false;
#ifdef HAVE_REPLICATION
  int thread_mask;
  Master_info *mi= NULL;

  if (check_global_access(thd, SUPER_ACL))
    DBUG_RETURN(ret= true);

  /* @Global filter:  Currently, after WL#1697, replication
    filters should act on all the channels. Before setting
    the replication filters, we  shall check the status
    of all SQL threads.
    Logic is lock all mi->rli->run_locks(),check the status of
    SQL threads; then set the replication filter and unlock
    all mi->rli->run_locks()

    however after the advent of WL#7361, replication filters
    would act on a single channel. This part of the code
    will be properly fixed in that WL.
  */
  channel_map.wrlock();

  mi= channel_map.get_default_channel_mi();

  if (!mi)
  {
    my_message(ER_SLAVE_CONFIGURATION, ER(ER_SLAVE_CONFIGURATION),
               MYF(0));
    ret= true;
    goto err;
  }

  for (mi_map::iterator it= channel_map.begin(); it!= channel_map.end(); it++)
  {
    mi= it->second;
    if (mi)
      mysql_mutex_lock(&mi->rli->run_lock); /* lock slave_sql_thread */
  }

  /* check the running status of all SQL threads */

  for (mi_map::iterator it= channel_map.begin(); it!= channel_map.end(); it++)
  {
    mi= it->second;
    if (mi)
      init_thread_mask(&thread_mask, mi, 0 /*not inverse*/);
    if (thread_mask & SLAVE_SQL) /* We refuse if any slave thread is running */
    {
      my_message(ER_SLAVE_SQL_THREAD_MUST_STOP, ER(ER_SLAVE_SQL_THREAD_MUST_STOP), MYF(0));
      ret= true;
      break;
    }
  }

  if (!ret)
  {
    if (!rpl_filter->set_do_db(do_db_list)
        && !rpl_filter->set_ignore_db(ignore_db_list)
        && !rpl_filter->set_do_table(do_table_list)
        && !rpl_filter->set_ignore_table(ignore_table_list)
        && !rpl_filter->set_wild_do_table(wild_do_table_list)
        && !rpl_filter->set_wild_ignore_table(wild_ignore_table_list)
        && rpl_filter->set_db_rewrite(rewrite_db_pair_list))
    {
      /* purecov: begin inspected */
      my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), 0);
      ret= true;
    /* purecov: end */
    }
  }

  for (mi_map::iterator it= channel_map.begin(); it !=channel_map.end(); it++)
  {
   mi= it->second;
   if (mi)
     mysql_mutex_unlock(&mi->rli->run_lock);
  }

  if (ret)
    goto err;

  my_ok(thd);

err:
  channel_map.unlock();
#endif //HAVE_REPLICATION
  DBUG_RETURN(ret);
}
