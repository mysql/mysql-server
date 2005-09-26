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
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#if defined(WIN32) || defined(__WIN__)
#undef SAFEMALLOC				/* Problems with threads */
#endif

#include "mysql.h"
#include "sp_head.h"
#include "sql_cursor.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"

sp_rcontext::sp_rcontext(uint fsize, uint hmax, uint cmax)
  : m_count(0), m_fsize(fsize), m_result(NULL), m_hcount(0), m_hsp(0),
    m_ihsp(0), m_hfound(-1), m_ccount(0)
{
  m_frame= (Item **)sql_alloc(fsize * sizeof(Item*));
  m_handler= (sp_handler_t *)sql_alloc(hmax * sizeof(sp_handler_t));
  m_hstack= (uint *)sql_alloc(hmax * sizeof(uint));
  m_in_handler= (uint *)sql_alloc(hmax * sizeof(uint));
  m_cstack= (sp_cursor **)sql_alloc(cmax * sizeof(sp_cursor *));
  m_saved.empty();
}


int
sp_rcontext::set_item_eval(THD *thd, uint idx, Item **item_addr,
			   enum_field_types type)
{
  Item *it;
  Item *reuse_it;
  /* sp_eval_func_item will use callers_arena */
  int res;

  reuse_it= get_item(idx);
  it= sp_eval_func_item(thd, item_addr, type, reuse_it, TRUE);
  if (! it)
    res= -1;
  else
  {
    res= 0;
    set_item(idx, it);
  }

  return res;
}

bool
sp_rcontext::find_handler(uint sql_errno,
                          MYSQL_ERROR::enum_warning_level level)
{
  if (m_hfound >= 0)
    return 1;			// Already got one

  const char *sqlstate= mysql_errno_to_sqlstate(sql_errno);
  int i= m_hcount, found= -1;

  while (i--)
  {
    sp_cond_type_t *cond= m_handler[i].cond;
    int j= m_ihsp;

    while (j--)
      if (m_in_handler[j] == m_handler[i].handler)
	break;
    if (j >= 0)
      continue;                 // Already executing this handler

    switch (cond->type)
    {
    case sp_cond_type_t::number:
      if (sql_errno == cond->mysqlerr)
	found= i;		// Always the most specific
      break;
    case sp_cond_type_t::state:
      if (strcmp(sqlstate, cond->sqlstate) == 0 &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::state))
	found= i;
      break;
    case sp_cond_type_t::warning:
      if ((sqlstate[0] == '0' && sqlstate[1] == '1' ||
	   level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
	  found < 0)
	found= i;
      break;
    case sp_cond_type_t::notfound:
      if (sqlstate[0] == '0' && sqlstate[1] == '2' &&
	  found < 0)
	found= i;
      break;
    case sp_cond_type_t::exception:
      if ((sqlstate[0] != '0' || sqlstate[1] > '2') &&
	  level == MYSQL_ERROR::WARN_LEVEL_ERROR &&
	  found < 0)
	found= i;
      break;
    }
  }
  if (found < 0)
    return FALSE;
  m_hfound= found;
  return TRUE;
}

void
sp_rcontext::save_variables(uint fp)
{
  while (fp < m_count)
  {
    m_saved.push_front(m_frame[fp]);
    m_frame[fp++]= NULL;	// Prevent reuse
  }
}

void
sp_rcontext::restore_variables(uint fp)
{
  uint i= m_count;

  while (i-- > fp)
    m_frame[i]= m_saved.pop();
}

void
sp_rcontext::push_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
{
  m_cstack[m_ccount++]= new sp_cursor(lex_keeper, i);
}

void
sp_rcontext::pop_cursors(uint count)
{
  while (count--)
  {
    delete m_cstack[--m_ccount];
  }
}


/*
 *
 *  sp_cursor
 *
 */

sp_cursor::sp_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i)
  :m_lex_keeper(lex_keeper),
   server_side_cursor(NULL),
   m_i(i)
{
  /*
    currsor can't be stored in QC, so we should prevent opening QC for
    try to write results which are absent.
  */
  lex_keeper->disable_query_cache();
}


/*
  Open an SP cursor

  SYNOPSIS
    open()
    THD		         Thread handler


  RETURN
   0 in case of success, -1 otherwise
*/

int
sp_cursor::open(THD *thd)
{
  if (server_side_cursor)
  {
    my_message(ER_SP_CURSOR_ALREADY_OPEN, ER(ER_SP_CURSOR_ALREADY_OPEN),
               MYF(0));
    return -1;
  }
  if (mysql_open_cursor(thd, (uint) ALWAYS_MATERIALIZED_CURSOR, &result,
                        &server_side_cursor))
    return -1;
  return 0;
}


int
sp_cursor::close(THD *thd)
{
  if (! server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return -1;
  }
  destroy();
  return 0;
}


void
sp_cursor::destroy()
{
  delete server_side_cursor;
  server_side_cursor= 0;
}


int
sp_cursor::fetch(THD *thd, List<struct sp_pvar> *vars)
{
  if (! server_side_cursor)
  {
    my_message(ER_SP_CURSOR_NOT_OPEN, ER(ER_SP_CURSOR_NOT_OPEN), MYF(0));
    return -1;
  }
  if (vars->elements != result.get_field_count())
  {
    my_message(ER_SP_WRONG_NO_OF_FETCH_ARGS,
               ER(ER_SP_WRONG_NO_OF_FETCH_ARGS), MYF(0));
    return -1;
  }

  result.set_spvar_list(vars);

  /* Attempt to fetch one row */
  if (server_side_cursor->is_open())
    server_side_cursor->fetch(1);

  /*
    If the cursor was pointing after the last row, the fetch will
    close it instead of sending any rows.
  */
  if (! server_side_cursor->is_open())
  {
    my_message(ER_SP_FETCH_NO_DATA, ER(ER_SP_FETCH_NO_DATA), MYF(0));
    return -1;
  }

  return 0;
}


/***************************************************************************
 Select_fetch_into_spvars
****************************************************************************/

int Select_fetch_into_spvars::prepare(List<Item> &fields, SELECT_LEX_UNIT *u)
{
  /*
    Cache the number of columns in the result set in order to easily
    return an error if column count does not match value count.
  */
  field_count= fields.elements;
  return select_result_interceptor::prepare(fields, u);
}


bool Select_fetch_into_spvars::send_data(List<Item> &items)
{
  List_iterator_fast<struct sp_pvar> pv_iter(*spvar_list);
  List_iterator_fast<Item> item_iter(items);
  sp_pvar_t *pv;
  Item *item;

  /* Must be ensured by the caller */
  DBUG_ASSERT(spvar_list->elements == items.elements);

  /*
    Assign the row fetched from a server side cursor to stored
    procedure variables.
  */
  for (; pv= pv_iter++, item= item_iter++; )
  {
    Item *reuse= thd->spcont->get_item(pv->offset);
    /* Evaluate a new item on the arena of the calling instruction */
    Item *it= sp_eval_func_item(thd, &item, pv->type, reuse, TRUE);

    thd->spcont->set_item(pv->offset, it);
  }
  return FALSE;
}
