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
  : m_params(0), m_framesize(0), m_i(0), m_genlab(0)
{
  m_pvar_size = 16;
  m_pvar = (sp_pvar_t *)my_malloc(m_pvar_size * sizeof(sp_pvar_t), MYF(MY_WME));
  if (m_pvar)
    memset(m_pvar, 0, m_pvar_size * sizeof(sp_pvar_t));
  m_label.empty();
}

void
sp_pcontext::grow()
{
  uint sz = m_pvar_size + 8;
  sp_pvar_t *a = (sp_pvar_t *)my_realloc((char *)m_pvar,
					 sz * sizeof(sp_pvar_t),
					 MYF(MY_WME | MY_ALLOW_ZERO_PTR));

  if (a)
  {
    m_pvar_size = sz;
    m_pvar = a;
  }
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
  uint i = m_i;

  while (i-- > 0)
  {
    uint len= m_pvar[i].name->const_string()->length();

    if (name->length > len)
      len= name->length;
    if (my_strncasecmp(system_charset_info,
		       name->str,
		       m_pvar[i].name->const_string()->ptr(),
		       len) == 0)
    {
      return m_pvar + i;
    }
  }
  return NULL;
}

void
sp_pcontext::push(LEX_STRING *name, enum enum_field_types type,
		  sp_param_mode_t mode)
{
  if (m_i >= m_pvar_size)
    grow();
  if (m_i < m_pvar_size)
  {
    if (m_i == m_framesize)
      m_framesize += 1;
    m_pvar[m_i].name= new Item_string(name->str, name->length,
				      default_charset_info);
    m_pvar[m_i].type= type;
    m_pvar[m_i].mode= mode;
    m_pvar[m_i].offset= m_i;
    m_pvar[m_i].isset= (mode == sp_param_out ? FALSE : TRUE);
    m_i += 1;
  }
}

sp_label_t *
sp_pcontext::push_label(char *name, uint ip)
{
  sp_label_t *lab = (sp_label_t *)my_malloc(sizeof(sp_label_t), MYF(MY_WME));

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
    if (strcasecmp(name, lab->name) == 0)
      return lab;

  return NULL;
}
