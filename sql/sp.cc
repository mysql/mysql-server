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

// *opened=true means we opened ourselves
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
  if ((defstr= get_field(&thd->mem_root, table->field[2])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  // Get additional information
  if ((creator= get_field(&thd->mem_root, table->field[3])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  modified= table->field[4]->val_int();
  created= table->field[5]->val_int();

  if ((ptr= get_field(&thd->mem_root, table->field[6])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  if (ptr[0] == 'N')
    suid= 0;

  table->field[7]->val_str(&str, &str);
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

    table->field[0]->store(name, namelen, system_charset_info);
    table->field[1]->store((longlong)type);
    table->field[2]->store(def, deflen, system_charset_info);
    table->field[3]->store(creator, (uint)strlen(creator), system_charset_info);
    ((Field_timestamp *)table->field[5])->set_time();
    if (suid)
      table->field[6]->store((longlong)suid);
    if (comment)
      table->field[7]->store(comment, commentlen, system_charset_info);

    if (table->file->write_row(table->record[0]))
      ret= SP_WRITE_ROW_FAILED;
    else
      ret= SP_OK;
  }

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

  sp= sp_cache_lookup(&thd->sp_proc_cache, name, namelen);
  if (sp)
  {
    sp_cache_remove(&thd->sp_proc_cache, sp);
    delete sp;
  }
  ret= db_drop_routine(thd, TYPE_ENUM_PROCEDURE, name, namelen);

  DBUG_RETURN(ret);
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

  sp= sp_cache_lookup(&thd->sp_func_cache, name, namelen);
  if (sp)
  {
    sp_cache_remove(&thd->sp_func_cache, sp);
    delete sp;
  }
  ret= db_drop_routine(thd, TYPE_ENUM_FUNCTION, name, namelen);

  DBUG_RETURN(ret);
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
