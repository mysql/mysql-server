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

sp_pcontext::sp_pcontext(sp_pcontext *prev)
  : Sql_alloc(), m_psubsize(0), m_csubsize(0), m_hsubsize(0),
    m_handlers(0), m_parent(prev)
{
  VOID(my_init_dynamic_array(&m_pvar, sizeof(sp_pvar_t *), 16, 8));
  VOID(my_init_dynamic_array(&m_cond, sizeof(sp_cond_type_t *), 16, 8));
  VOID(my_init_dynamic_array(&m_cursor, sizeof(LEX_STRING), 16, 8));
  m_label.empty();
  m_children.empty();
  if (!prev)
    m_poffset= m_coffset= 0;
  else
  {
    m_poffset= prev->current_pvars();
    m_coffset= prev->current_cursors();
  }
}

void
sp_pcontext::destroy()
{
  List_iterator_fast<sp_pcontext> li(m_children);
  sp_pcontext *child;

  while ((child= li++))
    child->destroy();

  m_children.empty();
  m_label.empty();
  delete_dynamic(&m_pvar);
  delete_dynamic(&m_cond);
  delete_dynamic(&m_cursor);
}

sp_pcontext *
sp_pcontext::push_context()
{
  sp_pcontext *child= new sp_pcontext(this);

  if (child)
    m_children.push_back(child);
  return child;
}

sp_pcontext *
sp_pcontext::pop_context()
{
  uint submax= max_pvars();

  if (submax > m_parent->m_psubsize)
    m_parent->m_psubsize= submax;
  submax= max_handlers();
  if (submax > m_parent->m_hsubsize)
    m_parent->m_hsubsize= submax;
  submax= max_cursors();
  if (submax > m_parent->m_csubsize)
    m_parent->m_csubsize= submax;
  return m_parent;
}

uint
sp_pcontext::diff_handlers(sp_pcontext *ctx)
{
  uint n= 0;
  sp_pcontext *pctx= this;

  while (pctx && pctx != ctx)
  {
    n+= pctx->m_handlers;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return n;
  return 0;			// Didn't find ctx
}

uint
sp_pcontext::diff_cursors(sp_pcontext *ctx)
{
  uint n= 0;
  sp_pcontext *pctx= this;

  while (pctx && pctx != ctx)
    pctx= pctx->parent_context();
  if (pctx)
    return ctx->current_cursors() - pctx->current_cursors();
  return 0;			// Didn't find ctx
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
  uint i= m_pvar.elements;

  while (i--)
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
  if (!scoped && m_parent)
    return m_parent->find_pvar(name, scoped);
  return NULL;
}

void
sp_pcontext::push_pvar(LEX_STRING *name, enum enum_field_types type,
		       sp_param_mode_t mode)
{
  sp_pvar_t *p= (sp_pvar_t *)sql_alloc(sizeof(sp_pvar_t));

  if (p)
  {
    if (m_pvar.elements == m_psubsize)
      m_psubsize+= 1;
    p->name.str= name->str;
    p->name.length= name->length;
    p->type= type;
    p->mode= mode;
    p->offset= current_pvars();
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
    lab->ctx= this;
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

  if (m_parent)
    return m_parent->find_label(name);
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
  uint i= m_cond.elements;

  while (i--)
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
  if (!scoped && m_parent)
    return m_parent->find_cond(name, scoped);
  return NULL;
}

void
sp_pcontext::push_cursor(LEX_STRING *name)
{
  LEX_STRING n;

  if (m_cursor.elements == m_csubsize)
    m_csubsize+= 1;
  n.str= name->str;
  n.length= name->length;
  insert_dynamic(&m_cursor, (gptr)&n);
}

/*
 * See comment for find_pvar() above
 */
my_bool
sp_pcontext::find_cursor(LEX_STRING *name, uint *poff, my_bool scoped)
{
  uint i= m_cursor.elements;

  while (i--)
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
  if (!scoped && m_parent)
    return m_parent->find_cursor(name, poff, scoped);
  return FALSE;
}
