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
  : Sql_alloc(), m_params(0), m_framesize(0), m_handlers(0), m_cursmax(0),
    m_hndlrlev(0)
{
  VOID(my_init_dynamic_array(&m_pvar, sizeof(sp_pvar_t *), 16, 8));
  VOID(my_init_dynamic_array(&m_cond, sizeof(sp_cond_type_t *), 16, 8));
  VOID(my_init_dynamic_array(&m_cursor, sizeof(LEX_STRING), 16, 8));
  VOID(my_init_dynamic_array(&m_scopes, sizeof(sp_scope_t), 16, 8));
  VOID(my_init_dynamic_array(&m_glabel, sizeof(sp_label_t *), 16, 8));
  m_label.empty();
}

void
sp_pcontext::destroy()
{
  delete_dynamic(&m_pvar);
  delete_dynamic(&m_cond);
  delete_dynamic(&m_cursor);
  delete_dynamic(&m_scopes);
  delete_dynamic(&m_glabel);
  m_label.empty();
}

void
sp_pcontext::push_scope()
{
  sp_scope_t s;

  s.vars= m_pvar.elements;
  s.conds= m_cond.elements;
  s.hndlrs= m_hndlrlev;
  s.curs= m_cursor.elements;
  s.glab= m_glabel.elements;
  insert_dynamic(&m_scopes, (gptr)&s);
}

void
sp_pcontext::pop_scope(sp_scope_t *sp)
{
  byte *p= pop_dynamic(&m_scopes);

  if (sp && p)
    memcpy(sp, p, sizeof(sp_scope_t));
}

void
sp_pcontext::diff_scopes(uint sold, sp_scope_t *diffs)
{
  uint snew= m_scopes.elements;
  sp_scope_t scope;

  diffs->vars= diffs->conds= diffs->hndlrs= diffs->curs= diffs->glab= 0;
  while (snew-- > sold)
  {
    get_dynamic(&m_scopes, (gptr)&scope, snew);
    diffs->vars+= scope.vars;
    diffs->conds+= scope.conds;
    diffs->hndlrs+= scope.hndlrs;
    diffs->curs+= scope.curs;
    diffs->glab+= scope.glab;
  }
  if (sold)
  {
    get_dynamic(&m_scopes, (gptr)&scope, sold-1);
    diffs->vars-= scope.vars;
    diffs->conds-= scope.conds;
    diffs->hndlrs-= scope.hndlrs;
    diffs->curs-= scope.curs;
    diffs->glab-= scope.glab;
  }
}


/* This does a linear search (from newer to older variables, in case
** we have shadowed names).
** It's possible to have a more efficient allocation and search method,
** but it might not be worth it. The typical number of parameters and
** variables will in most cases be low (a handfull).
** ...and, this is only called during parsing.
*/
sp_pvar_t *
sp_pcontext::find_pvar(LEX_STRING *name, my_bool scoped)
{
  uint i = m_pvar.elements;
  uint limit;

  if (! scoped || m_scopes.elements == 0)
    limit= 0;
  else
  {
    sp_scope_t s;

    get_dynamic(&m_scopes, (gptr)&s, m_scopes.elements-1);
    limit= s.vars;
  }
      
  while (i-- > limit)
  {
    sp_pvar_t *p;

    get_dynamic(&m_pvar, (gptr)&p, i);
    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)p->name.str, p->name.length) == 0)
    {
      return p;
    }
  }
  return NULL;
}

void
sp_pcontext::push_pvar(LEX_STRING *name, enum enum_field_types type,
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
    p->dflt= NULL;
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
    lab->type= SP_LAB_GOTO;
    lab->scopes= 0;
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

void
sp_pcontext::push_cond(LEX_STRING *name, sp_cond_type_t *val)
{
  sp_cond_t *p= (sp_cond_t *)sql_alloc(sizeof(sp_cond_t));

  if (p)
  {
    p->name.str= name->str;
    p->name.length= name->length;
    p->val= val;
    insert_dynamic(&m_cond, (gptr)&p);
  }
}

/*
 * See comment for find_pvar() above
 */
sp_cond_type_t *
sp_pcontext::find_cond(LEX_STRING *name, my_bool scoped)
{
  uint i = m_cond.elements;
  uint limit;

  if (! scoped || m_scopes.elements == 0)
    limit= 0;
  else
  {
    sp_scope_t s;

    get_dynamic(&m_scopes, (gptr)&s, m_scopes.elements-1);
    limit= s.conds;
  }
      
  while (i-- > limit)
  {
    sp_cond_t *p;

    get_dynamic(&m_cond, (gptr)&p, i);
    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)p->name.str, p->name.length) == 0)
    {
      return p->val;
    }
  }
  return NULL;
}

void
sp_pcontext::push_cursor(LEX_STRING *name)
{
  LEX_STRING n;

  n.str= name->str;
  n.length= name->length;
  insert_dynamic(&m_cursor, (gptr)&n);
  if (m_cursor.elements > m_cursmax)
    m_cursmax= m_cursor.elements;
}

/*
 * See comment for find_pvar() above
 */
my_bool
sp_pcontext::find_cursor(LEX_STRING *name, uint *poff, my_bool scoped)
{
  uint i = m_cursor.elements;
  uint limit;

  if (! scoped || m_scopes.elements == 0)
    limit= 0;
  else
  {
    sp_scope_t s;

    get_dynamic(&m_scopes, (gptr)&s, m_scopes.elements-1);
    limit= s.curs;
  }
      
  while (i-- > limit)
  {
    LEX_STRING n;

    get_dynamic(&m_cursor, (gptr)&n, i);
    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)n.str, n.length) == 0)
    {
      *poff= i;
      return TRUE;
    }
  }
  return FALSE;
}

sp_label_t *
sp_pcontext::push_glabel(char *name, uint ip)
{
  sp_label_t *lab = (sp_label_t *)sql_alloc(sizeof(sp_label_t));

  if (lab)
  {
    lab->name= name;
    lab->ip= ip;
    lab->type= SP_LAB_GOTO;
    lab->scopes= 0;
    insert_dynamic(&m_glabel, (gptr)&lab);
  }
  return lab;
}

sp_label_t *
sp_pcontext::find_glabel(char *name)
{
  uint i= m_glabel.elements;

  while (i--)
  {
    sp_label_t *lab;

    get_dynamic(&m_glabel, (gptr)&lab, i);
    if (my_strcasecmp(system_charset_info, name, lab->name) == 0)
      return lab;
  }
  return NULL;
}

void
sp_pcontext::pop_glabel(uint count)
{
  (void)pop_dynamic(&m_glabel);  
}
