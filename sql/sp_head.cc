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

#ifdef __GNUC__
#pragma implementation
#endif

#include "mysql_priv.h"
#include "sp_head.h"
#include "sp_pcontext.h"
#include "sp_rcontext.h"

/* Evaluate a (presumed) func item. Always returns an item, the parameter
** if nothing else.
*/
static Item *
eval_func_item(Item *it, enum enum_field_types type)
{
  it= it->this_item();

  /* QQ Which way do we do this? Or is there some even better way? */
#if 1
  /* QQ Obey the declared type of the variable */
  switch (type)
  {
  case MYSQL_TYPE_TINY:
  case MYSQL_TYPE_SHORT:
  case MYSQL_TYPE_LONG:
  case MYSQL_TYPE_LONGLONG:
  case MYSQL_TYPE_INT24:
    it= new Item_int(it->val_int());
    break;
  case MYSQL_TYPE_DECIMAL:
  case MYSQL_TYPE_FLOAT:
  case MYSQL_TYPE_DOUBLE:
    it= new Item_real(it->val());
    break;
  case MYSQL_TYPE_VAR_STRING:
  case MYSQL_TYPE_STRING:
  case MYSQL_TYPE_TIMESTAMP:
  case MYSQL_TYPE_DATE:
  case MYSQL_TYPE_TIME:
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_YEAR:
  case MYSQL_TYPE_NEWDATE:
    {
      char buffer[MAX_FIELD_WIDTH];
      String tmp(buffer, sizeof(buffer), default_charset_info);

      (void)it->val_str(&tmp);
      it= new Item_string(buffer, sizeof(buffer), default_charset_info);
      break;
    }
  case MYSQL_TYPE_NULL:
    it= new Item_null();	// A NULL is a NULL is a NULL...
    break;
  case MYSQL_TYPE_ENUM:
  case MYSQL_TYPE_SET:
  case MYSQL_TYPE_TINY_BLOB:
  case MYSQL_TYPE_MEDIUM_BLOB:
  case MYSQL_TYPE_LONG_BLOB:
  case MYSQL_TYPE_BLOB:
  case MYSQL_TYPE_GEOMETRY:
    /* QQ Don't know what to do with the rest. */
    break;
  }
#else
  /* QQ This looks simpler, but is wrong? It disregards the variable's type. */
  switch (it->result_type())
  {
  case REAL_RESULT:
    it= new Item_real(it->val());
    break;
  case INT_RESULT:
    it= new Item_int(it->val_int());
    break;
  case STRING_RESULT:
    {
      char buffer[MAX_FIELD_WIDTH];
      String tmp(buffer, sizeof(buffer), default_charset_info);

      (void)it->val_str(&tmp);
      it= new Item_string(buffer, sizeof(buffer), default_charset_info);
      break;
    }
  default:
    /* QQ Don't know what to do with the rest. */
    break;
  }
#endif
  return it;
}

sp_head::sp_head(LEX_STRING *name, LEX *lex)
{
  const char *dstr = (const char*)lex->buf;

  m_mylex= lex;
  m_name= new Item_string(name->str, name->length, default_charset_info);
  m_defstr= new Item_string(dstr, lex->end_of_query - lex->buf,
			    default_charset_info);
  my_init_dynamic_array(&m_instr, sizeof(sp_instr *), 16, 8);
}

int
sp_head::create(THD *thd)
{
  String *name= m_name->const_string();
  String *def= m_defstr->const_string();

  return sp_create_procedure(thd,
			     name->c_ptr(), name->length(),
			     def->c_ptr(), def->length());
}

int
sp_head::execute(THD *thd)
{
  int ret= 0;
  sp_instr *p;
  sp_pcontext *pctx = m_mylex->spcont;
  uint csize = pctx->max_framesize();
  uint params = pctx->params();
  sp_rcontext *octx = thd->spcont;
  sp_rcontext *nctx = NULL;

  if (csize > 0)
  {
    uint i;
    List_iterator_fast<Item> li(m_mylex->value_list);
    Item *it = li++;		// Skip first one, it's the procedure name

    nctx = new sp_rcontext(csize);
    // QQ: No error checking whatsoever right now
    for (i = 0 ; (it= li++) && i < params ; i++)
    {
      sp_pvar_t *pvar = pctx->find_pvar(i);

      // QQ Passing an argument is, in a sense, a "SET". We have to evaluate
      //    any expression at this point.
      nctx->push_item(it->this_item());
      // Note: If it's OUT or INOUT, it must be a variable.
      // QQ: Need to handle "global" user/host variables too!!!
      if (!pvar || pvar->mode == sp_param_in)
	nctx->set_oindex(i, -1);
      else
	nctx->set_oindex(i, static_cast<Item_splocal *>(it)->get_offset());
    }
    // The rest of the frame are local variables which are all IN.
    // QQ We haven't found any hint of what the value is when unassigned,
    //    so we set it to NULL for now. It's an error to refer to an
    //    unassigned variable (which should be detected by the parser).
    for (; i < csize ; i++)
      nctx->push_item(NULL);
    thd->spcont= nctx;
  }

  {				// Execute instructions...
    uint ip= 0;
    my_bool nsok= thd->net.no_send_ok;

    thd->net.no_send_ok= TRUE;	// Don't send_ok() during execution

    while (ret == 0)
    {
      int offset;
      sp_instr *i;

      i = get_instr(ip);	// Returns NULL when we're done.
      if (i == NULL)
	break;
      ret= i->execute(thd, &offset);
      ip += offset;
    }

    thd->net.no_send_ok= nsok;
  }

  // Don't copy back OUT values if we got an error
  if (ret == 0 && csize > 0)
  {
    // Copy back all OUT or INOUT values to the previous frame
    for (uint i = 0 ; i < params ; i++)
    {
      int oi = nctx->get_oindex(i);

      if (oi >= 0)
	octx->set_item(nctx->get_oindex(i), nctx->get_item(i));
    }

    thd->spcont= octx;
  }

  return ret;
}


void
sp_head::reset_lex(THD *thd)
{
  memcpy(&m_lex, &thd->lex, sizeof(LEX)); // Save old one
  /* Reset most stuff. The length arguments doesn't matter here. */
  lex_start(thd, m_lex.buf, m_lex.end_of_query - m_lex.ptr);
  /* We must reset ptr and end_of_query again */
  thd->lex.ptr= m_lex.ptr;
  thd->lex.end_of_query= m_lex.end_of_query;
  /* And keep the SP stuff too */
  thd->lex.sphead = m_lex.sphead;
  thd->lex.spcont = m_lex.spcont;
  /* QQ Why isn't this reset by lex_start() ??? */
  thd->lex.col_list.empty();
  thd->lex.ref_list.empty();
  thd->lex.drop_list.empty();
  thd->lex.alter_list.empty();
  thd->lex.interval_list.empty();
  thd->lex.users_list.empty();
  thd->lex.columns.empty();
  thd->lex.key_list.empty();
  thd->lex.create_list.empty();
  thd->lex.insert_list= NULL;
  thd->lex.field_list.empty();
  thd->lex.value_list.empty();
  thd->lex.many_values.empty();
  thd->lex.var_list.empty();
  thd->lex.param_list.empty();
  thd->lex.proc_list.empty();
  thd->lex.auxilliary_table_list.empty();
}

void
sp_head::restore_lex(THD *thd)
{
  // Update some state in the old one first
  m_lex.ptr= thd->lex.ptr;
  m_lex.next_state= thd->lex.next_state;
  // QQ Append tables, fields, etc. from the current lex to mine
  memcpy(&thd->lex, &m_lex, sizeof(LEX)); // Restore lex
}

// Finds the SP 'name'. Currently this always reads from the database
// and prepares (parse) it, but in the future it will first look in
// the in-memory cache for SPs. (And store newly prepared SPs there of
// course.)
sp_head *
sp_find(THD *thd, Item_string *iname)
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
sp_drop(THD *thd, char *name, uint namelen)
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



//
// sp_instr_stmt
//
int
sp_instr_stmt::execute(THD *thd, int *offsetp)
{
  LEX olex;			// The other lex

  memcpy(&olex, &thd->lex, sizeof(LEX)); // Save the other lex

  memcpy(&thd->lex, &m_lex, sizeof(LEX)); // Use my own lex
  thd->lex.thd = thd;

  mysql_execute_command(thd);

  memcpy(&thd->lex, &olex, sizeof(LEX)); // Restore the other lex

  *offsetp = 1;
  return 0;
}

//
// sp_instr_set
//
int
sp_instr_set::execute(THD *thd, int *offsetp)
{
  thd->spcont->set_item(m_offset, eval_func_item(m_value, m_type));
  *offsetp = 1;
  return 0;
}
