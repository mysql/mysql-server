/* Copyright (C) 2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "mysql_priv.h"
#include "sp.h"
#include "sp_head.h"
#include "sp_cache.h"

/*
 *
 * DB storage of Stored PROCEDUREs and FUNCTIONs
 *
 */

#define MYSQL_PROC_FIELD_NAME       0
#define MYSQL_PROC_FIELD_TYPE       1
#define MYSQL_PROC_FIELD_DEFINITION 2
#define MYSQL_PROC_FIELD_CREATOR    3
#define MYSQL_PROC_FIELD_MODIFIED   4
#define MYSQL_PROC_FIELD_CREATED    5
#define MYSQL_PROC_FIELD_SUID       6
#define MYSQL_PROC_FIELD_COMMENT    7
#define MYSQL_PROC_FIELD_COUNT      8

/* *opened=true means we opened ourselves */
static int
db_find_routine_aux(THD *thd, int type, char *name, uint namelen,
		    enum thr_lock_type ltype, TABLE **tablep, bool *opened)
{
  DBUG_ENTER("db_find_routine_aux");
  DBUG_PRINT("enter", ("type: %d name: %*s", type, namelen, name));
  TABLE *table;
  byte key[65];			// We know name is 64 and the enum is 1 byte
  uint keylen;
  int ret;

  // Put the key together
  keylen= namelen;
  if (keylen > sizeof(key)-1)
    keylen= sizeof(key)-1;
  memcpy(key, name, keylen);
  memset(key+keylen, (int)' ', sizeof(key)-1 - keylen);	// Pad with space
  key[sizeof(key)-1]= type;
  keylen= sizeof(key);

  for (table= thd->open_tables ; table ; table= table->next)
    if (strcmp(table->table_cache_key, "mysql") == 0 &&
	strcmp(table->real_name, "proc") == 0)
      break;
  if (table)
    *opened= FALSE;
  else
  {
    TABLE_LIST tables;

    memset(&tables, 0, sizeof(tables));
    tables.db= (char*)"mysql";
    tables.real_name= tables.alias= (char*)"proc";
    if (! (table= open_ltable(thd, &tables, ltype)))
    {
      *tablep= NULL;
      DBUG_RETURN(SP_OPEN_TABLE_FAILED);
    }
    *opened= TRUE;
  }

  if (table->file->index_read_idx(table->record[0], 0,
				  key, keylen,
				  HA_READ_KEY_EXACT))
  {
    *tablep= NULL;
    DBUG_RETURN(SP_KEY_NOT_FOUND);
  }
  *tablep= table;

  DBUG_RETURN(SP_OK);
}

static int
db_find_routine(THD *thd, int type, char *name, uint namelen, sp_head **sphp)
{
  DBUG_ENTER("db_find_routine");
  DBUG_PRINT("enter", ("type: %d name: %*s", type, namelen, name));
  extern int yyparse(void *thd);
  TABLE *table;
  const char *defstr;
  int ret;
  bool opened;
  const char *creator;
  longlong created;
  longlong modified;
  bool suid= 1;
  char *ptr;
  uint length;
  char buff[65];
  String str(buff, sizeof(buff), &my_charset_bin);

  ret= db_find_routine_aux(thd, type, name, namelen, TL_READ, &table, &opened);
  if (ret != SP_OK)
    goto done;

  if (table->fields != MYSQL_PROC_FIELD_COUNT)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  if ((defstr= get_field(&thd->mem_root,
			 table->field[MYSQL_PROC_FIELD_DEFINITION])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  // Get additional information
  if ((creator= get_field(&thd->mem_root,
			  table->field[MYSQL_PROC_FIELD_CREATOR])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  modified= table->field[MYSQL_PROC_FIELD_MODIFIED]->val_int();
  created= table->field[MYSQL_PROC_FIELD_CREATED]->val_int();

  if ((ptr= get_field(&thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_SUID])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  if (ptr[0] == 'N')
    suid= 0;

  table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= strmake_root(&thd->mem_root, str.ptr(), length);

  if (opened)
  {
    close_thread_tables(thd, 0, 1);
    opened= FALSE;
  }

  {
    LEX *oldlex= thd->lex;
    enum enum_sql_command oldcmd= thd->lex->sql_command;

    lex_start(thd, (uchar*)defstr, strlen(defstr));
    if (yyparse(thd) || thd->is_fatal_error || thd->lex->sphead == NULL)
    {
      LEX *newlex= thd->lex;
      sp_head *sp= newlex->sphead;

      if (sp)
      {
	if (oldlex != newlex)
	  sp->restore_lex(thd);
	delete sp;
	newlex->sphead= NULL;
      }
      ret= SP_PARSE_ERROR;
    }
    else
    {
      *sphp= thd->lex->sphead;
      (*sphp)->sp_set_info((char *) creator, (uint) strlen(creator),
			   created, modified, suid,
			   ptr, length);
    }
    thd->lex->sql_command= oldcmd;
  }

 done:
  if (opened)
    close_thread_tables(thd);
  DBUG_RETURN(ret);
}

static int
db_create_routine(THD *thd, int type,
		  char *name, uint namelen, char *def, uint deflen,
		  char *comment, uint commentlen, bool suid)
{
  DBUG_ENTER("db_create_routine");
  DBUG_PRINT("enter", ("type: %d name: %*s def: %*s", type, namelen, name, deflen, def));
  int ret;
  TABLE *table;
  TABLE_LIST tables;
  char creator[HOSTNAME_LENGTH+USERNAME_LENGTH+2];

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";

  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    restore_record(table, default_values); // Get default values for fields
    strxmov(creator, thd->user, "@", thd->host_or_ip, NullS);

    if (table->fields != MYSQL_PROC_FIELD_COUNT)
    {
      ret= SP_GET_FIELD_FAILED;
      goto done;
    }
    table->field[MYSQL_PROC_FIELD_NAME]->store(name, namelen,
					       system_charset_info);
    table->field[MYSQL_PROC_FIELD_TYPE]->store((longlong)type);
    table->field[MYSQL_PROC_FIELD_DEFINITION]->store(def, deflen,
						     system_charset_info);
    table->field[MYSQL_PROC_FIELD_CREATOR]->store(creator,
						  (uint)strlen(creator),
						  system_charset_info);
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_CREATED])->set_time();
    if (!suid)
      table->field[MYSQL_PROC_FIELD_SUID]->store((longlong) 1);
    if (comment)
      table->field[MYSQL_PROC_FIELD_COMMENT]->store(comment, commentlen,
						    system_charset_info);

    if (table->file->write_row(table->record[0]))
      ret= SP_WRITE_ROW_FAILED;
    else
      ret= SP_OK;
  }

done:
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}

static int
db_drop_routine(THD *thd, int type, char *name, uint namelen)
{
  DBUG_ENTER("db_drop_routine");
  DBUG_PRINT("enter", ("type: %d name: %*s", type, namelen, name));
  TABLE *table;
  int ret;
  bool opened;

  ret= db_find_routine_aux(thd, type, name, namelen, TL_WRITE, &table, &opened);
  if (ret == SP_OK)
  {
    if (table->file->delete_row(table->record[0]))
      ret= SP_DELETE_ROW_FAILED;
  }

  if (opened)
    close_thread_tables(thd);
  DBUG_RETURN(ret);
}

static int
db_update_routine(THD *thd, int type, char *name, uint namelen,
		  char *newname, uint newnamelen,
		  char *comment, uint commentlen, enum suid_behaviour suid)
{
  DBUG_ENTER("db_update_routine");
  DBUG_PRINT("enter", ("type: %d name: %*s", type, namelen, name));
  TABLE *table;
  int ret;
  bool opened;

  ret= db_find_routine_aux(thd, type, name, namelen, TL_WRITE, &table, &opened);
  if (ret == SP_OK)
  {
    store_record(table,record[1]);
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_MODIFIED])->set_time();
    if (suid)
      table->field[MYSQL_PROC_FIELD_SUID]->store((longlong) suid);
    if (newname)
      table->field[MYSQL_PROC_FIELD_NAME]->store(newname,
						 newnamelen,
						 system_charset_info);
    if (comment)
      table->field[MYSQL_PROC_FIELD_COMMENT]->store(comment,
						    commentlen,
						    system_charset_info);
    if ((table->file->update_row(table->record[1],table->record[0])))
      ret= SP_WRITE_ROW_FAILED;
  }
  if (opened)
    close_thread_tables(thd);
  DBUG_RETURN(ret);
}

struct st_used_field
{
  const char *field_name;
  uint field_length;
  enum enum_field_types field_type;
  Field *field;
};

static struct st_used_field init_fields[]=
{
  { "Name",     NAME_LEN, MYSQL_TYPE_STRING,    0},
  { "Type",            9, MYSQL_TYPE_STRING,    0},
  { "Creator",        77, MYSQL_TYPE_STRING,    0},
  { "Modified",        0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Created",         0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Suid",            1, MYSQL_TYPE_STRING,    0},
  { "Comment",  NAME_LEN, MYSQL_TYPE_STRING,    0},
  { 0,                 0, MYSQL_TYPE_STRING,    0}
};

static int
print_field_values(THD *thd, TABLE *table,
		   struct st_used_field *used_fields,
		   int type, const char *wild)
{
  Protocol *protocol= thd->protocol;

  if (table->field[MYSQL_PROC_FIELD_TYPE]->val_int() == type)
  {
    String *tmp_string= new String();
    struct st_used_field *used_field= used_fields;

    get_field(&thd->mem_root, used_field->field, tmp_string);
    if (!wild || !wild[0] || !wild_compare(tmp_string->ptr(), wild, 0))
    {
      protocol->prepare_for_resend();
      protocol->store(tmp_string);
      for (used_field++;
	   used_field->field_name;
	   used_field++)
      {
	switch (used_field->field_type) {
	case MYSQL_TYPE_TIMESTAMP:
	  {
	    TIME tmp_time;
	    ((Field_timestamp *) used_field->field)->get_time(&tmp_time);
	    protocol->store(&tmp_time);
	  }
	  break;
	default:
	  {
	    String *tmp_string1= new String();
	    get_field(&thd->mem_root, used_field->field, tmp_string1);
	    protocol->store(tmp_string1);
	  }
	  break;
	}
      }
      if (protocol->write())
	return SP_INTERNAL_ERROR;
    }
  }
  return SP_OK;
}

static int
db_show_routine_status(THD *thd, int type, const char *wild)
{
  DBUG_ENTER("db_show_routine_status");

  TABLE *table;
  TABLE_LIST tables;
  int res;

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";

  if (! (table= open_ltable(thd, &tables, TL_READ)))
  {
    res= SP_OPEN_TABLE_FAILED;
    goto done;
  }
  else
  {
    Item *item;
    List<Item> field_list;
    struct st_used_field *used_field;
    st_used_field used_fields[array_elements(init_fields)];

    memcpy((char*) used_fields, (char*) init_fields, sizeof(used_fields));
    /* Init header */
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      switch (used_field->field_type) {
      case MYSQL_TYPE_TIMESTAMP:
	field_list.push_back(item=new Item_datetime(used_field->field_name));
	break;
      default:
	field_list.push_back(item=new Item_empty_string(used_field->field_name,
							used_field->
							field_length));
	break;
      }
    }
    /* Print header */
    if (thd->protocol->send_fields(&field_list,1))
    {
      res= SP_INTERNAL_ERROR;
      goto err_case;
    }

    /* Init fields */
    setup_tables(&tables);
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      TABLE_LIST *not_used;
      Item_field *field= new Item_field("mysql", "proc",
					used_field->field_name);
      if (!(used_field->field= find_field_in_tables(thd, field, &tables, 
						    &not_used, TRUE)))
      {
	res= SP_INTERNAL_ERROR;
	goto err_case1;
      }
    }

    table->file->index_init(0);
    if ((res= table->file->index_first(table->record[0])))
    {
      if (res == HA_ERR_END_OF_FILE)
	res= 0;
      else
	res= SP_INTERNAL_ERROR;
      goto err_case1;
    }
    if ((res= print_field_values(thd, table, used_fields, type, wild)))
      goto err_case1;
    while (!table->file->index_next(table->record[0]))
    {
      if ((res= print_field_values(thd, table, used_fields, type, wild)))
	goto err_case1;
    }
    res= SP_OK;
  }

 err_case1:
  send_eof(thd);
 err_case:
  close_thread_tables(thd);
 done:
  DBUG_RETURN(res);
}


/*
 *
 * PROCEDURE
 *
 */

sp_head *
sp_find_procedure(THD *thd, LEX_STRING *name)
{
  DBUG_ENTER("sp_find_procedure");
  sp_head *sp;

  DBUG_PRINT("enter", ("name: %*s", name->length, name->str));

  sp= sp_cache_lookup(&thd->sp_proc_cache, name->str, name->length);
  if (! sp)
  {
    if (db_find_routine(thd, TYPE_ENUM_PROCEDURE,
			name->str, name->length, &sp) == SP_OK)
    {
      sp_cache_insert(&thd->sp_proc_cache, sp);
    }
  }

  DBUG_RETURN(sp);
}

int
sp_create_procedure(THD *thd, char *name, uint namelen, char *def, uint deflen,
		    char *comment, uint commentlen, bool suid)
{
  DBUG_ENTER("sp_create_procedure");
  DBUG_PRINT("enter", ("name: %*s def: %*s", namelen, name, deflen, def));
  int ret;

  ret= db_create_routine(thd, TYPE_ENUM_PROCEDURE, name, namelen, def, deflen,
			 comment, commentlen, suid);

  DBUG_RETURN(ret);
}

int
sp_drop_procedure(THD *thd, char *name, uint namelen)
{
  DBUG_ENTER("sp_drop_procedure");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  sp_head *sp;
  int ret;

  sp= sp_cache_remove(&thd->sp_proc_cache, name, namelen);
  if (sp)
    delete sp;
  ret= db_drop_routine(thd, TYPE_ENUM_PROCEDURE, name, namelen);

  DBUG_RETURN(ret);
}

int
sp_update_procedure(THD *thd, char *name, uint namelen,
		    char *newname, uint newnamelen,
		    char *comment, uint commentlen, enum suid_behaviour suid)
{
  DBUG_ENTER("sp_update_procedure");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  sp_head *sp;
  int ret;

  sp= sp_cache_remove(&thd->sp_proc_cache, name, namelen);
  if (sp)
    delete sp;
  ret= db_update_routine(thd, TYPE_ENUM_PROCEDURE, name, namelen,
			 newname, newnamelen,
			 comment, commentlen, suid);

  DBUG_RETURN(ret);
}

int
sp_show_create_procedure(THD *thd, LEX_STRING *name)
{
  DBUG_ENTER("sp_show_create_procedure");
  DBUG_PRINT("enter", ("name: %*s", name->length, name->str));
  sp_head *sp;

  sp= sp_find_procedure(thd, name);
  if (sp) 
   DBUG_RETURN(sp->show_create_procedure(thd));

  DBUG_RETURN(SP_KEY_NOT_FOUND);
}

int
sp_show_status_procedure(THD *thd, const char *wild)
{
  DBUG_ENTER("sp_show_status_procedure");
  DBUG_RETURN(db_show_routine_status(thd, TYPE_ENUM_PROCEDURE, wild));
}

/*
 *
 * FUNCTION
 *
 */

sp_head *
sp_find_function(THD *thd, LEX_STRING *name)
{
  DBUG_ENTER("sp_find_function");
  sp_head *sp;

  DBUG_PRINT("enter", ("name: %*s", name->length, name->str));

  sp= sp_cache_lookup(&thd->sp_func_cache, name->str, name->length);
  if (! sp)
  {
    if (db_find_routine(thd, TYPE_ENUM_FUNCTION,
			name->str, name->length, &sp) != SP_OK)
      sp= NULL;
  }
  DBUG_RETURN(sp);
}

int
sp_create_function(THD *thd, char *name, uint namelen, char *def, uint deflen,
		   char *comment, uint commentlen, bool suid)
{
  DBUG_ENTER("sp_create_function");
  DBUG_PRINT("enter", ("name: %*s def: %*s", namelen, name, deflen, def));
  int ret;

  ret= db_create_routine(thd, TYPE_ENUM_FUNCTION, name, namelen, def, deflen,
			 comment, commentlen, suid);

  DBUG_RETURN(ret);
}

int
sp_drop_function(THD *thd, char *name, uint namelen)
{
  DBUG_ENTER("sp_drop_function");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  sp_head *sp;
  int ret;

  sp= sp_cache_remove(&thd->sp_func_cache, name, namelen);
  if (sp)
    delete sp;
  ret= db_drop_routine(thd, TYPE_ENUM_FUNCTION, name, namelen);

  DBUG_RETURN(ret);
}

int
sp_update_function(THD *thd, char *name, uint namelen,
		    char *newname, uint newnamelen,
		    char *comment, uint commentlen, enum suid_behaviour suid)
{
  DBUG_ENTER("sp_update_procedure");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  sp_head *sp;
  int ret;

  sp= sp_cache_remove(&thd->sp_func_cache, name, namelen);
  if (sp)
    delete sp;
  ret= db_update_routine(thd, TYPE_ENUM_FUNCTION, name, namelen,
			 newname, newnamelen,
			 comment, commentlen, suid);

  DBUG_RETURN(ret);
}

int
sp_show_create_function(THD *thd, LEX_STRING *name)
{
  DBUG_ENTER("sp_show_create_function");
  DBUG_PRINT("enter", ("name: %*s", name->length, name->str));
  sp_head *sp;

  sp= sp_find_function(thd, name);
  if (sp)
    DBUG_RETURN(sp->show_create_function(thd));

  DBUG_RETURN(SP_KEY_NOT_FOUND);
}

int
sp_show_status_function(THD *thd, const char *wild)
{
  DBUG_ENTER("sp_show_status_function");
  DBUG_RETURN(db_show_routine_status(thd, TYPE_ENUM_FUNCTION, wild));
}

// QQ Temporary until the function call detection in sql_lex has been reworked.
bool
sp_function_exists(THD *thd, LEX_STRING *name)
{
  TABLE *table;
  bool ret= FALSE;
  bool opened= FALSE;

  if (sp_cache_lookup(&thd->sp_func_cache, name->str, name->length) ||
      db_find_routine_aux(thd, TYPE_ENUM_FUNCTION,
			  name->str, name->length, TL_READ,
			  &table, &opened) == SP_OK)
  {
    ret= TRUE;
  }
  if (opened)
    close_thread_tables(thd, 0, 1);
  return ret;
}


byte *
sp_lex_spfuns_key(const byte *ptr, uint *plen, my_bool first)
{
  LEX_STRING *lsp= (LEX_STRING *)ptr;
  *plen= lsp->length;
  return (byte *)lsp->str;
}

void
sp_add_fun_to_lex(LEX *lex, LEX_STRING fun)
{
  if (! hash_search(&lex->spfuns, (byte *)fun.str, fun.length))
  {
    LEX_STRING *ls= (LEX_STRING *)sql_alloc(sizeof(LEX_STRING));
    ls->str= sql_strmake(fun.str, fun.length);
    ls->length= fun.length;

    my_hash_insert(&lex->spfuns, (byte *)ls);
  }
}

void
sp_merge_funs(LEX *dst, LEX *src)
{
  for (uint i=0 ; i < src->spfuns.records ; i++)
  {
    LEX_STRING *ls= (LEX_STRING *)hash_element(&src->spfuns, i);

    if (! hash_search(&dst->spfuns, (byte *)ls->str, ls->length))
      my_hash_insert(&dst->spfuns, (byte *)ls);
  }
}

int
sp_cache_functions(THD *thd, LEX *lex)
{
  HASH *h= &lex->spfuns;
  int ret= 0;

  for (uint i=0 ; i < h->records ; i++)
  {
    LEX_STRING *ls= (LEX_STRING *)hash_element(h, i);

    if (! sp_cache_lookup(&thd->sp_func_cache, ls->str, ls->length))
    {
      sp_head *sp;
      LEX *oldlex= thd->lex;
      LEX *newlex= new st_lex;

      thd->lex= newlex;
      if (db_find_routine(thd, TYPE_ENUM_FUNCTION, ls->str, ls->length, &sp)
	  == SP_OK)
      {
	ret= sp_cache_functions(thd, newlex);
	delete newlex;
	thd->lex= oldlex;
	if (ret)
	  break;
	sp_cache_insert(&thd->sp_func_cache, sp);
      }
      else
      {
	delete newlex;
	thd->lex= oldlex;
	net_printf(thd, ER_SP_DOES_NOT_EXIST, "FUNCTION", ls->str);
	ret= 1;
	break;
      }
    }
  }
  return ret;
}
