/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_priv.h"
#include "unireg.h"                      // REQUIRED by other includes
#include "rpl_filter.h"
#include "hash.h"                               // my_hash_free
#include "table.h"                              // TABLE_LIST

#define TABLE_RULE_HASH_SIZE   16
#define TABLE_RULE_ARR_SIZE   16

Rpl_filter::Rpl_filter() :
  table_rules_on(0),
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
  if (do_table_array_inited)
    free_string_array(&do_table_array);
  if (ignore_table_array_inited)
    free_string_array(&ignore_table_array);
  if (wild_do_table_inited)
    free_string_array(&wild_do_table);
  if (wild_ignore_table_inited)
    free_string_array(&wild_ignore_table);
  free_list(&do_db);
  free_list(&ignore_db);
  free_list(&rewrite_db);
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
    end= strmov(hash_key, tables->db ? tables->db : db);
    *end++= '.';
    len= (uint) (strmov(end, tables->table_name) - hash_key);
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
  int len;
  end= strmov(hash_key, db);
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
  DBUG_RETURN(add_table_rule_to_array(&wild_ignore_table, table_spec));
}


void
Rpl_filter::add_db_rewrite(const char* from_db, const char* to_db)
{
  i_string_pair *db_pair = new i_string_pair(from_db, to_db);
  rewrite_db.push_back(db_pair);
}


/*
  Build do_table rules to HASH from DYNAMIC_ARRAY
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
  Build ignore_table rules to HASH from DYNAMIC_ARRAY
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

  @param[in] table_array         DYNAMIC_ARRAY stored table rules
  @param[in] table_hash          HASH for storing table rules
  @param[in] array_inited        Table rules are added to DYNAMIC_ARRAY
  @param[in] hash_inited         Table rules are added to HASH

  @return
             0           ok
             1           error
*/
int
Rpl_filter::build_table_hash_from_array(DYNAMIC_ARRAY *table_array, HASH *table_hash,
                             bool array_inited, bool *hash_inited)
{
  DBUG_ENTER("Rpl_filter::build_table_hash");

  if (array_inited)
  {
    init_table_rule_hash(table_hash, hash_inited);
    for (uint i= 0; i < table_array->elements; i++)
    {
      TABLE_RULE_ENT* e;
      get_dynamic(table_array, (uchar*)&e, i);
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
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
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
Rpl_filter::add_table_rule_to_array(DYNAMIC_ARRAY* a, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db= (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name= e->db + (dot - table_spec) + 1;
  e->key_len= len;
  memcpy(e->db, table_spec, len);

  if (insert_dynamic(a, &e))
  {
    my_free(e);
    return 1;
  }
  return 0;
}


void
Rpl_filter::add_do_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_do_db");
  i_string *db = new i_string(table_spec);
  do_db.push_back(db);
  DBUG_VOID_RETURN;
}


void
Rpl_filter::add_ignore_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_ignore_db");
  i_string *db = new i_string(table_spec);
  ignore_db.push_back(db);
  DBUG_VOID_RETURN;
}

extern "C" uchar *get_table_key(const uchar *, size_t *, my_bool);
extern "C" void free_table_ent(void* a);

uchar *get_table_key(const uchar* a, size_t *len,
                     my_bool __attribute__((unused)))
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
               get_table_key, free_table_ent, 0);
  *h_inited = 1;
}


void 
Rpl_filter::init_table_rule_array(DYNAMIC_ARRAY* a, bool* a_inited)
{
  my_init_dynamic_array(a, sizeof(TABLE_RULE_ENT*), TABLE_RULE_ARR_SIZE,
			TABLE_RULE_ARR_SIZE);
  *a_inited = 1;
}


TABLE_RULE_ENT* 
Rpl_filter::find_wild(DYNAMIC_ARRAY *a, const char* key, int len)
{
  uint i;
  const char* key_end= key + len;

  for (i= 0; i < a->elements; i++)
  {
    TABLE_RULE_ENT* e ;
    get_dynamic(a, (uchar*)&e, i);
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
Rpl_filter::free_string_array(DYNAMIC_ARRAY *a)
{
  uint i;
  for (i= 0; i < a->elements; i++)
  {
    char* p;
    get_dynamic(a, (uchar*) &p, i);
    my_free(p);
  }
  delete_dynamic(a);
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
Rpl_filter::table_rule_ent_dynamic_array_to_str(String* s, DYNAMIC_ARRAY* a,
                                                bool inited)
{
  s->length(0);
  if (inited)
  {
    for (uint i= 0; i < a->elements; i++)
    {
      TABLE_RULE_ENT* e;
      get_dynamic(a, (uchar*)&e, i);
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
