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
#include "sp_rcontext.h"
#include "sp_pcontext.h"

sp_rcontext::sp_rcontext(uint fsize, uint hmax)
  : m_count(0), m_fsize(fsize), m_result(NULL), m_hcount(0), m_hsp(0)
{
  m_frame= (Item **)sql_alloc(fsize * sizeof(Item*));
  m_outs= (int *)sql_alloc(fsize * sizeof(int));
  m_handler= (sp_handler_t *)sql_alloc(hmax * sizeof(sp_handler_t));
  m_hstack= (uint *)sql_alloc(hmax * sizeof(uint));
  m_saved.empty();
}

int
sp_rcontext::find_handler(uint sql_errno)
{
  if (m_hfound >= 0)
    return 1;			// Already got one

  const char *sqlstate= mysql_errno_to_sqlstate(sql_errno);
  int i= m_hcount, found= 0;

  while (!found && i--)
  {
    sp_cond_type_t *cond= m_handler[i].cond;

    switch (cond->type)
    {
    case sp_cond_type_t::number:
      if (sql_errno == cond->mysqlerr)
	found= 1;
      break;
    case sp_cond_type_t::state:
      if (strcmp(sqlstate, cond->sqlstate) == 0)
	found= 1;
      break;
    case sp_cond_type_t::warning:
      if (sqlstate[0] == '0' && sqlstate[0] == '1')
	found= 1;
      break;
    case sp_cond_type_t::notfound:
      if (sqlstate[0] == '0' && sqlstate[0] == '2')
	found= 1;
      break;
    case sp_cond_type_t::exception:
      if (sqlstate[0] != '0' || sqlstate[0] > '2')
	found= 1;
      break;
    }
  }
  if (found)
    m_hfound= i;
  return found;
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
