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

/*
 *
 * DB storage of Stored PROCEDUREs and FUNCTIONs
 *
 */

static int
db_find_routine_aux(THD *thd, int type, char *name, uint namelen,
		    enum thr_lock_type ltype, TABLE **tablep)
{
  DBUG_ENTER("db_find_routine_aux");
  DBUG_PRINT("enter", ("type: %d name: %*s", type, namelen, name));
  TABLE *table;
  TABLE_LIST tables;
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

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";
  if (! (table= open_ltable(thd, &tables, ltype)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if (table->file->index_read_idx(table->record[0], 0,
				  key, keylen,
				  HA_READ_KEY_EXACT))
  {
    close_thread_tables(thd);
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

  // QQ Set up our own mem_root here???
  ret= db_find_routine_aux(thd, type, name, namelen, TL_READ, &table);
  if (ret != SP_OK)
    goto done;
  if ((defstr= get_field(&thd->mem_root, table->field[2])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  tmplex= lex_start(thd, (uchar*)defstr, strlen(defstr));
  if (yyparse(thd) || thd->is_fatal_error || tmplex->sphead == NULL)
    ret= SP_PARSE_ERROR;
  else
    *sphp= tmplex->sphead;

 done:
  if (ret == SP_OK && table)
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

    table->field[0]->store(name, namelen, default_charset_info);
    table->field[1]->store((longlong)type);
    table->field[2]->store(def, deflen, default_charset_info);

    if (table->file->write_row(table->record[0]))
      ret= SP_WRITE_ROW_FAILED;
    else
      ret= SP_OK;
  }

  if (ret == SP_OK && table)
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

  ret= db_find_routine_aux(thd, type, name, namelen, TL_WRITE, &table);
  if (ret == SP_OK)
  {
    if (table->file->delete_row(table->record[0]))
      ret= SP_DELETE_ROW_FAILED;
  }

  if (ret == SP_OK && table)
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
  DBUG_ENTER("sp_find_function_i");
  sp_head *sp;

  DBUG_PRINT("enter", ("name: %*s", name->length, name->str));

  if (db_find_routine(thd, TYPE_ENUM_FUNCTION,
		      name->str, name->length, &sp) != SP_OK)
    sp= NULL;

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
