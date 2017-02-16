/* Copyright (c) 2000, 2013, Oracle and/or its affiliates. All rights reserved.
   
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

#include "sql_priv.h"
#include "mysqld.h"                             // system_charset_info
#include "rpl_filter.h"
#include "hash.h"                               // my_hash_free
#include "table.h"                              // TABLE_LIST

#define TABLE_RULE_HASH_SIZE   16
#define TABLE_RULE_ARR_SIZE   16

Rpl_filter::Rpl_filter() : 
  table_rules_on(0), do_table_inited(0), ignore_table_inited(0),
  wild_do_table_inited(0), wild_ignore_table_inited(0)
{
  do_db.empty();
  ignore_db.empty();
  rewrite_db.empty();
}


Rpl_filter::~Rpl_filter() 
{
  if (do_table_inited) 
    my_hash_free(&do_table);
  if (ignore_table_inited)
    my_hash_free(&ignore_table);
  if (wild_do_table_inited)
    free_string_array(&wild_do_table);
  if (wild_ignore_table_inited)
    free_string_array(&wild_ignore_table);
  free_string_list(&do_db);
  free_string_list(&ignore_db);
  free_list(&rewrite_db);
}


#ifndef MYSQL_CLIENT
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
    char hash_key[SAFE_NAME_LEN*2+2];
    char *end;
    uint len;

    if (!tables->updating) 
      continue;
    some_tables_updating= 1;
    end= strmov(hash_key, tables->db ? tables->db : db);
    *end++= '.';
    len= (uint) (strmov(end, tables->table_name) - hash_key);
    if (do_table_inited) // if there are any do's
    {
      if (my_hash_search(&do_table, (uchar*) hash_key, len))
	DBUG_RETURN(1);
    }
    if (ignore_table_inited) // if there are any ignores
    {
      if (my_hash_search(&ignore_table, (uchar*) hash_key, len))
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
              !do_table_inited && !wild_do_table_inited);
}

#endif

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
      if (!strcmp(tmp->ptr, db))
	DBUG_RETURN(1); // match
    }
    DBUG_PRINT("exit", ("Don't replicate"));
    DBUG_RETURN(0);
  }
  else // there are some elements in the don't, otherwise we cannot get here
  {
    I_List_iterator<i_string> it(ignore_db);
    i_string* tmp;

    while ((tmp=it++))
    {
      if (!strcmp(tmp->ptr, db))
      {
        DBUG_PRINT("exit", ("Don't replicate"));
	DBUG_RETURN(0); // match
      }
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

  char hash_key[SAFE_NAME_LEN+2];
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


/**
  Parse and add the given comma-separated sequence of filter rules.

  @param  spec  Comma-separated sequence of filter rules.
  @param  add   Callback member function to add a filter rule.

  @return true if error, false otherwise.
*/

int
Rpl_filter::parse_filter_rule(const char* spec, Add_filter add)
{
  int status= 0;
  char *arg, *ptr, *pstr;

  if (!spec)
    return false;
  
  if (! (ptr= my_strdup(spec, MYF(MY_WME))))
    return true;

  pstr= ptr;

  while (pstr)
  {
    arg= pstr;

    /* Parse token string. */
    pstr= strpbrk(arg, ",");

    /* NUL terminate the token string. */
    if (pstr)
      *pstr++= '\0';

    /* Skip an empty token string. */
    if (arg[0] == '\0')
      continue;

    /* Skip leading spaces.  */
    while (my_isspace(system_charset_info, *arg))
      arg++;

    status= (this->*add)(arg);

    if (status)
      break;
  }

  my_free(ptr);

  return status;
}


int 
Rpl_filter::add_do_table(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_do_table");
  if (!do_table_inited)
    init_table_rule_hash(&do_table, &do_table_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_table_rule(&do_table, table_spec));
}
  

int 
Rpl_filter::add_ignore_table(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_ignore_table");
  if (!ignore_table_inited)
    init_table_rule_hash(&ignore_table, &ignore_table_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_table_rule(&ignore_table, table_spec));
}


int
Rpl_filter::set_do_table(const char* table_spec)
{
  int status;

  if (do_table_inited)
    my_hash_reset(&do_table);

  status= parse_filter_rule(table_spec, &Rpl_filter::add_do_table);

  if (!do_table.records)
  {
    my_hash_free(&do_table);
    do_table_inited= 0;
  }

  return status;
}


int
Rpl_filter::set_ignore_table(const char* table_spec)
{
  int status;

  if (ignore_table_inited)
    my_hash_reset(&ignore_table);

  status= parse_filter_rule(table_spec, &Rpl_filter::add_ignore_table);

  if (!ignore_table.records)
  {
    my_hash_free(&ignore_table);
    ignore_table_inited= 0;
  }

  return status;
}


int 
Rpl_filter::add_wild_do_table(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_wild_do_table");
  if (!wild_do_table_inited)
    init_table_rule_array(&wild_do_table, &wild_do_table_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_wild_table_rule(&wild_do_table, table_spec));
}
  

int 
Rpl_filter::add_wild_ignore_table(const char* table_spec) 
{
  DBUG_ENTER("Rpl_filter::add_wild_ignore_table");
  if (!wild_ignore_table_inited)
    init_table_rule_array(&wild_ignore_table, &wild_ignore_table_inited);
  table_rules_on= 1;
  DBUG_RETURN(add_wild_table_rule(&wild_ignore_table, table_spec));
}


int
Rpl_filter::set_wild_do_table(const char* table_spec)
{
  int status;

  if (wild_do_table_inited)
    free_string_array(&wild_do_table);

  status= parse_filter_rule(table_spec, &Rpl_filter::add_wild_do_table);

  if (!wild_do_table.elements)
  {
    delete_dynamic(&wild_do_table);
    wild_do_table_inited= 0;
  }

  return status;
}


int
Rpl_filter::set_wild_ignore_table(const char* table_spec)
{
  int status;

  if (wild_ignore_table_inited)
    free_string_array(&wild_ignore_table);

  status= parse_filter_rule(table_spec, &Rpl_filter::add_wild_ignore_table);

  if (!wild_ignore_table.elements)
  {
    delete_dynamic(&wild_ignore_table);
    wild_ignore_table_inited= 0;
  }

  return status;
}


void
Rpl_filter::add_db_rewrite(const char* from_db, const char* to_db)
{
  i_string_pair *db_pair = new i_string_pair(from_db, to_db);
  rewrite_db.push_back(db_pair);
}


int 
Rpl_filter::add_table_rule(HASH* h, const char* table_spec)
{
  const char* dot = strchr(table_spec, '.');
  if (!dot) return 1;
  // len is always > 0 because we know the there exists a '.'
  uint len = (uint)strlen(table_spec);
  TABLE_RULE_ENT* e = (TABLE_RULE_ENT*)my_malloc(sizeof(TABLE_RULE_ENT)
						 + len, MYF(MY_WME));
  if (!e) return 1;
  e->db= (char*)e + sizeof(TABLE_RULE_ENT);
  e->tbl_name= e->db + (dot - table_spec) + 1;
  e->key_len= len;
  memcpy(e->db, table_spec, len);

  return my_hash_insert(h, (uchar*)e);
}


/*
  Add table expression with wildcards to dynamic array
*/

int 
Rpl_filter::add_wild_table_rule(DYNAMIC_ARRAY* a, const char* table_spec)
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
  return insert_dynamic(a, (uchar*)&e);
}


int
Rpl_filter::add_string_list(I_List<i_string> *list, const char* spec)
{
  char *str;
  i_string *node;

  if (! (str= my_strdup(spec, MYF(MY_WME))))
    return true;

  if (! (node= new i_string(str)))
  {
    my_free(str);
    return true;
  }

  list->push_back(node);

  return false;
}


int
Rpl_filter::add_do_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_do_db");
  DBUG_RETURN(add_string_list(&do_db, table_spec));
}


int
Rpl_filter::add_ignore_db(const char* table_spec)
{
  DBUG_ENTER("Rpl_filter::add_ignore_db");
  DBUG_RETURN(add_string_list(&ignore_db, table_spec));
}


int
Rpl_filter::set_do_db(const char* db_spec)
{
  free_string_list(&do_db);
  return parse_filter_rule(db_spec, &Rpl_filter::add_do_db);
}


int
Rpl_filter::set_ignore_db(const char* db_spec)
{
  free_string_list(&ignore_db);
  return parse_filter_rule(db_spec, &Rpl_filter::add_ignore_db);
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
  my_hash_init(h, system_charset_info,TABLE_RULE_HASH_SIZE,0,0,
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
    if (!my_wildcmp(system_charset_info, key, key_end, 
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
  table_rule_ent_hash_to_str(str, &do_table, do_table_inited);
}


void
Rpl_filter::get_ignore_table(String* str)
{
  table_rule_ent_hash_to_str(str, &ignore_table, ignore_table_inited);
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


bool
Rpl_filter::rewrite_db_is_empty()
{
  return rewrite_db.is_empty();
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
    if (!strcmp(tmp->key, db))
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


void
Rpl_filter::db_rule_ent_list_to_str(String* str, I_List<i_string>* list)
{
  I_List_iterator<i_string> it(*list);
  i_string* s;

  str->length(0);

  while ((s= it++))
  {
    str->append(s->ptr);
    str->append(',');
  }

  // Remove last ','
  if (!str->is_empty())
    str->chop();
}


void
Rpl_filter::get_do_db(String* str)
{
  db_rule_ent_list_to_str(str, get_do_db());
}


void
Rpl_filter::get_ignore_db(String* str)
{
  db_rule_ent_list_to_str(str, get_ignore_db());
}
