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

// Finds the SP 'name'. Currently this always reads from the database
// and prepares (parse) it, but in the future it will first look in
// the in-memory cache for SPs. (And store newly prepared SPs there of
// course.)
sp_head *
sp_find_procedure(THD *thd, Item_string *iname)
{
  extern int yyparse(void *thd);
  LEX *tmplex;
  TABLE *table;
  TABLE_LIST tables;
  const char *defstr;
  String *name;
  sp_head *sp = NULL;

  name = iname->const_string();
  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";
  if (! (table= open_ltable(thd, &tables, TL_READ)))
    return NULL;

  if (table->file->index_read_idx(table->record[0], 0,
				  (byte*)name->c_ptr(), name->length(),
				  HA_READ_KEY_EXACT))
    goto done;

  if ((defstr= get_field(&thd->mem_root, table, 1)) == NULL)
    goto done;

  // QQ Set up our own mem_root here???
  tmplex= lex_start(thd, (uchar*)defstr, strlen(defstr));
  if (yyparse(thd) || thd->fatal_error || tmplex->sphead == NULL)
    goto done;			// Error
  else
    sp = tmplex->sphead;

 done:
  if (table)
    close_thread_tables(thd);
  return sp;
}

int
sp_create_procedure(THD *thd, char *name, uint namelen, char *def, uint deflen)
{
  int ret= 0;
  TABLE *table;
  TABLE_LIST tables;

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.real_name= tables.alias= (char*)"proc";
  /* Allow creation of procedures even if we can't open proc table */
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
  {
    ret= -1;
    goto done;
  }

  restore_record(table, 2);	// Get default values for fields

  table->field[0]->store(name, namelen, default_charset_info);
  table->field[1]->store(def, deflen, default_charset_info);

  ret= table->file->write_row(table->record[0]);

 done:
  close_thread_tables(thd);
  return ret;
}

int
sp_drop_procedure(THD *thd, char *name, uint namelen)
{
  TABLE *table;
  TABLE_LIST tables;

  tables.db= (char *)"mysql";
  tables.real_name= tables.alias= (char *)"proc";
  if (! (table= open_ltable(thd, &tables, TL_WRITE)))
    goto err;
  if (! table->file->index_read_idx(table->record[0], 0,
				    (byte *)name, namelen,
				    HA_READ_KEY_EXACT))
  {
    int error;

    if ((error= table->file->delete_row(table->record[0])))
      table->file->print_error(error, MYF(0));
  }
  close_thread_tables(thd);
  return 0;

 err:
  close_thread_tables(thd);
  return -1;
}
