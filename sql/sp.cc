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

static sp_head *
sp_find_cached_function(THD *thd, char *name, uint namelen);

/*
 *
 * DB storage of Stored PROCEDUREs and FUNCTIONs
 *
 */

// *openeed=true means we opened ourselves
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
  LEX *tmplex;
  TABLE *table;
  const char *defstr;
  int ret;
  bool opened;

  // QQ Set up our own mem_root here???
  ret= db_find_routine_aux(thd, type, name, namelen, TL_READ, &table, &opened);
  if (ret != SP_OK)
    goto done;
  if ((defstr= get_field(&thd->mem_root, table->field[2])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  if (opened)
  {
    close_thread_tables(thd, 0, 1);
    table= NULL;
  }

  tmplex= lex_start(thd, (uchar*)defstr, strlen(defstr));
  if (yyparse(thd) || thd->is_fatal_error || tmplex->sphead == NULL)
    ret= SP_PARSE_ERROR;
  else
    *sphp= tmplex->sphead;

 done:
  if (table && opened)
    close_thread_tables(thd);
  DBUG_RETURN(ret);
}

static int
db_create_routine(THD *thd, int type,
		  char *name, uint namelen, char *def, uint deflen)
{
  DBUG_ENTER("db_create_routine");
  DBUG_PRINT("enter", ("type: %d name: %*s def: %*s", type, namelen, name, deflen, def));
  int ret;
  TABLE *table;
  TABLE_LIST tables;

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";

  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    restore_record(table, 2);	// Get default values for fields

    table->field[0]->store(name, namelen, system_charset_info);
    table->field[1]->store((longlong)type);
    table->field[2]->store(def, deflen, system_charset_info);

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

  if (db_find_routine(thd, TYPE_ENUM_PROCEDURE,
		      name->str, name->length, &sp) != SP_OK)
    sp= NULL;

  DBUG_RETURN(sp);
}

int
sp_create_procedure(THD *thd, char *name, uint namelen, char *def, uint deflen)
{
  DBUG_ENTER("sp_create_procedure");
  DBUG_PRINT("enter", ("name: %*s def: %*s", namelen, name, deflen, def));
  int ret;

  ret= db_create_routine(thd, TYPE_ENUM_PROCEDURE, name, namelen, def, deflen);

  DBUG_RETURN(ret);
}

int
sp_drop_procedure(THD *thd, char *name, uint namelen)
{
  DBUG_ENTER("sp_drop_procedure");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  int ret;

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

  sp= sp_find_cached_function(thd, name->str, name->length);
  if (! sp)
  {
    if (db_find_routine(thd, TYPE_ENUM_FUNCTION,
			name->str, name->length, &sp) != SP_OK)
      sp= NULL;
  }
  DBUG_RETURN(sp);
}

int
sp_create_function(THD *thd, char *name, uint namelen, char *def, uint deflen)
{
  DBUG_ENTER("sp_create_function");
  DBUG_PRINT("enter", ("name: %*s def: %*s", namelen, name, deflen, def));
  int ret;

  ret= db_create_routine(thd, TYPE_ENUM_FUNCTION, name, namelen, def, deflen);

  DBUG_RETURN(ret);
}

int
sp_drop_function(THD *thd, char *name, uint namelen)
{
  DBUG_ENTER("sp_drop_function");
  DBUG_PRINT("enter", ("name: %*s", namelen, name));
  int ret;

  ret= db_drop_routine(thd, TYPE_ENUM_FUNCTION, name, namelen);

  DBUG_RETURN(ret);
}

// QQ Temporary until the function call detection in sql_lex has been reworked.
bool
sp_function_exists(THD *thd, LEX_STRING *name)
{
  TABLE *table;
  bool ret= FALSE;
  bool opened;

  if (db_find_routine_aux(thd, TYPE_ENUM_FUNCTION,
			  name->str, name->length, TL_READ,
			  &table, &opened) == SP_OK)
  {
    ret= TRUE;
  }
  if (opened)
    close_thread_tables(thd, 0, 1);
  return ret;
}


/*
 *
 *   The temporary FUNCTION cache. (QQ This will be rehacked later, but
 *   it's needed now to make functions work at all.)
 *
 */

void
sp_add_fun_to_lex(LEX *lex, LEX_STRING fun)
{
  List_iterator_fast<char> li(lex->spfuns);
  char *fn;

  while ((fn= li++))
  {
    uint len= strlen(fn);

    if (my_strnncoll(system_charset_info,
		     (const uchar *)fn, len,
		     (const uchar *)fun.str, fun.length) == 0)
      break;
  }
  if (! fn)
  {
    char *s= sql_strmake(fun.str, fun.length);
    lex->spfuns.push_back(s);
  }
}

void
sp_merge_funs(LEX *dst, LEX *src)
{
  List_iterator_fast<char> li(src->spfuns);
  char *fn;

  while ((fn= li++))
  {
    LEX_STRING lx;

    lx.str= fn; lx.length= strlen(fn);
    sp_add_fun_to_lex(dst, lx);
  }
}

/* QQ Not terribly efficient right now, but it'll do for starters.
      We should actually open the mysql.proc table just once. */
int
sp_cache_functions(THD *thd, LEX *lex)
{
  List_iterator<char> li(lex->spfuns);
  char *fn;
  enum_sql_command cmd= lex->sql_command;
  int ret= 0;

  while ((fn= li++))
  {
    List_iterator_fast<sp_head> lisp(thd->spfuns);
    sp_head *sp;

    while ((sp= lisp++))
    {
      if (my_strcasecmp(system_charset_info, fn, sp->name()) == 0)
	break;
    }
    if (sp)
      continue;
    if (db_find_routine(thd, TYPE_ENUM_FUNCTION, fn, strlen(fn), &sp) == SP_OK)
    {
      ret= sp_cache_functions(thd, &thd->lex);
      if (ret)
	break;
      thd->spfuns.push_back(sp);
    }
    else
    {
      send_error(thd, ER_SP_DOES_NOT_EXIST);
      ret= 1;
    }
  }
  lex->sql_command= cmd;
  return ret;
}

void
sp_clear_function_cache(THD *thd)
{
  List_iterator_fast<sp_head> li(thd->spfuns);
  sp_head *sp;

  while ((sp= li++))
    sp->destroy();
  thd->spfuns.empty();
}

static sp_head *
sp_find_cached_function(THD *thd, char *name, uint namelen)
{
  List_iterator_fast<sp_head> li(thd->spfuns);
  sp_head *sp;

  while ((sp= li++))
  {
    uint len;
    const uchar *n= (const uchar *)sp->name(&len);

    if (my_strnncoll(system_charset_info,
		     (const uchar *)name, namelen,
		     n, len) == 0)
      break;
  }
  return sp;
}
