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
#include "sp_pcontext.h"
#include "sp_head.h"

sp_pcontext::sp_pcontext()
  : Sql_alloc(), m_params(0), m_framesize(0), m_genlab(0)
{
  VOID(my_init_dynamic_array(&m_pvar, sizeof(sp_pvar_t *), 16, 8));
  m_label.empty();
}

void
sp_pcontext::destroy()
{
  delete_dynamic(&m_pvar);
  m_label.empty();
}


/* This does a linear search (from newer to older variables, in case
** we have shadowed names).
** It's possible to have a more efficient allocation and search method,
** but it might not be worth it. The typical number of parameters and
** variables will in most cases be low (a handfull).
** And this is only called during parsing.
*/
sp_pvar_t *
sp_pcontext::find_pvar(LEX_STRING *name)
{
  uint i = m_pvar.elements;

  while (i-- > 0)
  {
    sp_pvar_t *p= find_pvar(i);
    uint len= (p->name.length > name->length ?
	       p->name.length : name->length);

    if (my_strncasecmp(system_charset_info,
		       name->str,
		       p->name.str,
		       len) == 0)
    {
      return p;
    }
  }
  return NULL;
}

void
sp_pcontext::push(LEX_STRING *name, enum enum_field_types type,
		  sp_param_mode_t mode)
{
  sp_pvar_t *p= (sp_pvar_t *)sql_alloc(sizeof(sp_pvar_t));

  if (p)
  {
    if (m_pvar.elements == m_framesize)
      m_framesize += 1;
    p->name.str= name->str;
    p->name.length= name->length;
    p->type= type;
    p->mode= mode;
    p->offset= m_pvar.elements;
    p->isset= (mode == sp_param_out ? FALSE : TRUE);
    insert_dynamic(&m_pvar, (gptr)&p);
  }
}

sp_label_t *
sp_pcontext::push_label(char *name, uint ip)
{
  sp_label_t *lab = (sp_label_t *)sql_alloc(sizeof(sp_label_t));

  if (lab)
  {
    lab->name= name;
    lab->ip= ip;
    m_label.push_front(lab);
  }
  return lab;
}

sp_label_t *
sp_pcontext::find_label(char *name)
{
  List_iterator_fast<sp_label_t> li(m_label);
  sp_label_t *lab;

  while ((lab= li++))
    if (my_strcasecmp(system_charset_info, name, lab->name) == 0)
      return lab;

  return NULL;
}
