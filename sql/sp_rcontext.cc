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

#if defined(WIN32) || defined(__WIN__)
#undef SAFEMALLOC				/* Problems with threads */
#endif

#include "mysql_priv.h"
#include "mysql.h"
#include "sp_head.h"
#include "sp_rcontext.h"
#include "sp_pcontext.h"

sp_rcontext::sp_rcontext(uint fsize, uint hmax, uint cmax)
  : m_count(0), m_fsize(fsize), m_result(NULL), m_hcount(0), m_hsp(0),
    m_hfound(-1), m_ccount(0)
{
  in_handler= FALSE;
  m_frame= (Item **)sql_alloc(fsize * sizeof(Item*));
  m_outs= (int *)sql_alloc(fsize * sizeof(int));
  m_handler= (sp_handler_t *)sql_alloc(hmax * sizeof(sp_handler_t));
  m_hstack= (uint *)sql_alloc(hmax * sizeof(uint));
  m_cstack= (sp_cursor **)sql_alloc(cmax * sizeof(sp_cursor *));
  m_saved.empty();
}

int
sp_rcontext::set_item_eval(uint idx, Item *i, enum_field_types type)
{
  extern Item *sp_eval_func_item(THD *thd, Item *it, enum_field_types type);
  Item *it= sp_eval_func_item(current_thd, i, type);

  if (! it)
    return -1;
  else
  {
    set_item(idx, it);
    return 0;
  }
}

bool
sp_rcontext::find_handler(uint sql_errno,
                          MYSQL_ERROR::enum_warning_level level)
{
  if (in_handler)
    return 0;			// Already executing a handler
  if (m_hfound >= 0)
    return 1;			// Already got one

  const char *sqlstate= mysql_errno_to_sqlstate(sql_errno);
  int i= m_hcount, found= -1;

  while (i--)
  {
    sp_cond_type_t *cond= m_handler[i].cond;

    switch (cond->type)
    {
    case sp_cond_type_t::number:
      if (sql_errno == cond->mysqlerr)
	found= i;		// Always the most specific
      break;
    case sp_cond_type_t::state:
      if (strcmp(sqlstate, cond->sqlstate) == 0 &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::number))
	found= i;
      break;
    case sp_cond_type_t::warning:
      if ((sqlstate[0] == '0' && sqlstate[1] == '1' ||
	   level == MYSQL_ERROR::WARN_LEVEL_WARN) &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::state))
	found= i;
      break;
    case sp_cond_type_t::notfound:
      if (sqlstate[0] == '0' && sqlstate[1] == '2' &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::state))
	found= i;
      break;
    case sp_cond_type_t::exception:
      if ((sqlstate[0] != '0' || sqlstate[1] > '2' ||
	   level == MYSQL_ERROR::WARN_LEVEL_ERROR) &&
	  (found < 0 || m_handler[found].cond->type > sp_cond_type_t::state))
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
    m_saved.push_front(m_frame[fp++]);
}

void
sp_rcontext::restore_variables(uint fp)
{
  uint i= m_count;

  while (i-- > fp)
    m_frame[i]= m_saved.pop();
}

void
sp_rcontext::push_cursor(LEX *lex)
{
  m_cstack[m_ccount++]= new sp_cursor(lex);
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

// We have split this in two to make it easy for sp_instr_copen
// to reuse the sp_instr::exec_stmt() code.
LEX *
sp_cursor::pre_open(THD *thd)
{
  if (m_isopen)
  {
    my_error(ER_SP_CURSOR_ALREADY_OPEN, MYF(0));
    return NULL;
  }

  bzero((char *)&m_mem_root, sizeof(m_mem_root));
  init_alloc_root(&m_mem_root, MEM_ROOT_BLOCK_SIZE, MEM_ROOT_PREALLOC);
  if ((m_prot= new Protocol_cursor(thd, &m_mem_root)) == NULL)
    return NULL;

  m_oprot= thd->protocol;	// Save the original protocol
  thd->protocol= m_prot;

  m_nseof= thd->net.no_send_eof;
  thd->net.no_send_eof= TRUE;
  return m_lex;
}

void
sp_cursor::post_open(THD *thd, my_bool was_opened)
{
  thd->net.no_send_eof= m_nseof; // Restore the originals
  thd->protocol= m_oprot;
  if (was_opened)
  {
    m_isopen= was_opened;
    m_current_row= m_prot->data;
  }
}

int
sp_cursor::close(THD *thd)
{
  if (! m_isopen)
  {
    my_error(ER_SP_CURSOR_NOT_OPEN, MYF(0));
    return -1;
  }
  destroy();
  return 0;
}

void
sp_cursor::destroy()
{
  if (m_prot)
  {
    delete m_prot;
    m_prot= NULL;
    free_root(&m_mem_root, MYF(0));
    bzero((char *)&m_mem_root, sizeof(m_mem_root));
  }
  m_isopen= FALSE;
}

int
sp_cursor::fetch(THD *thd, List<struct sp_pvar> *vars)
{
  List_iterator_fast<struct sp_pvar> li(*vars);
  sp_pvar_t *pv;
  MYSQL_ROW row;
  uint fldcount;

  if (! m_isopen)
  {
    my_error(ER_SP_CURSOR_NOT_OPEN, MYF(0));
    return -1;
  }
  if (m_current_row == NULL)
  {
    my_error(ER_SP_FETCH_NO_DATA, MYF(0));
    return -1;
  }

  row= m_current_row->data;
  for (fldcount= 0 ; (pv= li++) ; fldcount++)
  {
    Item *it;
    const char *s;

    if (fldcount >= m_prot->get_field_count())
    {
      my_error(ER_SP_WRONG_NO_OF_FETCH_ARGS, MYF(0));
      return -1;
    }
    s= row[fldcount];
    if (!s)
      it= new Item_null();
    else
      switch (sp_map_result_type(pv->type))
      {
      case INT_RESULT:
	it= new Item_int(s);
	break;
      case REAL_RESULT:
	it= new Item_real(s, strlen(s));
	break;
      default:
	{
	  uint len= strlen(s);
	  it= new Item_string(thd->strmake(s, len), len, thd->db_charset);
	  break;
	}
      }
    thd->spcont->set_item(pv->offset, it);
  }
  if (fldcount < m_prot->get_field_count())
  {
    my_error(ER_SP_WRONG_NO_OF_FETCH_ARGS, MYF(0));
    return -1;
  }
  m_current_row= m_current_row->next;
  return 0;
}
