/* Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_priv.h"
#include "unireg.h"
#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation
#endif

#include "sp_pcontext.h"
#include "sp_head.h"

/* Initial size for the dynamic arrays in sp_pcontext */
#define PCONTEXT_ARRAY_INIT_ALLOC 16
/* Increment size for the dynamic arrays in sp_pcontext */
#define PCONTEXT_ARRAY_INCREMENT_ALLOC 8

/*
  Sanity check for SQLSTATEs. Will not check if it's really an existing
  state (there are just too many), but will check length and bad characters.
  Returns TRUE if it's ok, FALSE if it's bad.
*/
bool
sp_cond_check(LEX_STRING *sqlstate)
{
  int i;
  const char *p;

  if (sqlstate->length != 5)
    return FALSE;
  for (p= sqlstate->str, i= 0 ; i < 5 ; i++)
  {
    char c = p[i];

    if ((c < '0' || '9' < c) &&
	(c < 'A' || 'Z' < c))
      return FALSE;
  }
  /* SQLSTATE class '00' : completion condition */
  if (strncmp(sqlstate->str, "00", 2) == 0)
    return FALSE;
  return TRUE;
}

sp_pcontext::sp_pcontext()
  : Sql_alloc(),
  m_max_var_index(0), m_max_cursor_index(0), m_max_handler_index(0),
  m_context_handlers(0), m_parent(NULL), m_pboundary(0),
  m_label_scope(LABEL_DEFAULT_SCOPE)
{
  (void) my_init_dynamic_array(&m_vars, sizeof(sp_variable_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_case_expr_id_lst, sizeof(int),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_conds, sizeof(sp_cond_type_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_cursors, sizeof(LEX_STRING),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_handlers, sizeof(sp_cond_type_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  m_label.empty();
  m_children.empty();

  m_var_offset= m_cursor_offset= 0;
  m_num_case_exprs= 0;
}

sp_pcontext::sp_pcontext(sp_pcontext *prev, label_scope_type label_scope)
  : Sql_alloc(),
  m_max_var_index(0), m_max_cursor_index(0), m_max_handler_index(0),
  m_context_handlers(0), m_parent(prev), m_pboundary(0),
  m_label_scope(label_scope)
{
  (void) my_init_dynamic_array(&m_vars, sizeof(sp_variable_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_case_expr_id_lst, sizeof(int),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_conds, sizeof(sp_cond_type_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_cursors, sizeof(LEX_STRING),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  (void) my_init_dynamic_array(&m_handlers, sizeof(sp_cond_type_t *),
                             PCONTEXT_ARRAY_INIT_ALLOC,
                             PCONTEXT_ARRAY_INCREMENT_ALLOC);
  m_label.empty();
  m_children.empty();

  m_var_offset= prev->m_var_offset + prev->m_max_var_index;
  m_cursor_offset= prev->current_cursor_count();
  m_num_case_exprs= prev->get_num_case_exprs();
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
  delete_dynamic(&m_vars);
  delete_dynamic(&m_case_expr_id_lst);
  delete_dynamic(&m_conds);
  delete_dynamic(&m_cursors);
  delete_dynamic(&m_handlers);
}

sp_pcontext *
sp_pcontext::push_context(label_scope_type label_scope)
{
  sp_pcontext *child= new sp_pcontext(this, label_scope);

  if (child)
    m_children.push_back(child);
  return child;
}

sp_pcontext *
sp_pcontext::pop_context()
{
  m_parent->m_max_var_index+= m_max_var_index;

  uint submax= max_handler_index();
  if (submax > m_parent->m_max_handler_index)
    m_parent->m_max_handler_index= submax;

  submax= max_cursor_index();
  if (submax > m_parent->m_max_cursor_index)
    m_parent->m_max_cursor_index= submax;

  if (m_num_case_exprs > m_parent->m_num_case_exprs)
    m_parent->m_num_case_exprs= m_num_case_exprs;

  return m_parent;
}

uint
sp_pcontext::diff_handlers(sp_pcontext *ctx, bool exclusive)
{
  uint n= 0;
  sp_pcontext *pctx= this;
  sp_pcontext *last_ctx= NULL;

  while (pctx && pctx != ctx)
  {
    n+= pctx->m_context_handlers;
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return (exclusive && last_ctx ? n - last_ctx->m_context_handlers : n);
  return 0;			// Didn't find ctx
}

uint
sp_pcontext::diff_cursors(sp_pcontext *ctx, bool exclusive)
{
  uint n= 0;
  sp_pcontext *pctx= this;
  sp_pcontext *last_ctx= NULL;

  while (pctx && pctx != ctx)
  {
    n+= pctx->m_cursors.elements;
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return  (exclusive && last_ctx ? n - last_ctx->m_cursors.elements : n);
  return 0;			// Didn't find ctx
}

/*
  This does a linear search (from newer to older variables, in case
  we have shadowed names).
  It's possible to have a more efficient allocation and search method,
  but it might not be worth it. The typical number of parameters and
  variables will in most cases be low (a handfull).
  ...and, this is only called during parsing.
*/
sp_variable_t *
sp_pcontext::find_variable(LEX_STRING *name, my_bool scoped)
{
  uint i= m_vars.elements - m_pboundary;

  while (i--)
  {
    sp_variable_t *p;

    get_dynamic(&m_vars, (uchar*)&p, i);
    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)p->name.str, p->name.length) == 0)
    {
      return p;
    }
  }
  if (!scoped && m_parent)
    return m_parent->find_variable(name, scoped);
  return NULL;
}

/*
  Find a variable by offset from the top.
  This used for two things:
  - When evaluating parameters at the beginning, and setting out parameters
    at the end, of invokation. (Top frame only, so no recursion then.)
  - For printing of sp_instr_set. (Debug mode only.)
*/
sp_variable_t *
sp_pcontext::find_variable(uint offset)
{
  if (m_var_offset <= offset && offset < m_var_offset + m_vars.elements)
  {                           // This frame
    sp_variable_t *p;

    get_dynamic(&m_vars, (uchar*)&p, offset - m_var_offset);
    return p;
  }
  if (m_parent)
    return m_parent->find_variable(offset); // Some previous frame
  return NULL;                  // index out of bounds
}

sp_variable_t *
sp_pcontext::push_variable(LEX_STRING *name, enum enum_field_types type,
                           sp_param_mode_t mode)
{
  sp_variable_t *p= (sp_variable_t *)sql_alloc(sizeof(sp_variable_t));

  if (!p)
    return NULL;

  ++m_max_var_index;

  p->name.str= name->str;
  p->name.length= name->length;
  p->type= type;
  p->mode= mode;
  p->offset= current_var_count();
  p->dflt= NULL;
  if (insert_dynamic(&m_vars, (uchar*)&p))
    return NULL;
  return p;
}


sp_label_t *
sp_pcontext::push_label(char *name, uint ip)
{
  sp_label_t *lab = (sp_label_t *)sql_alloc(sizeof(sp_label_t));

  if (lab)
  {
    lab->name= name;
    lab->ip= ip;
    lab->type= SP_LAB_IMPL;
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

  /*
    Note about exception handlers.
    See SQL:2003 SQL/PSM (ISO/IEC 9075-4:2003),
    section 13.1 <compound statement>,
    syntax rule 4.
    In short, a DECLARE HANDLER block can not refer
    to labels from the parent context, as they are out of scope.
  */
  if (m_parent && (m_label_scope == LABEL_DEFAULT_SCOPE))
    return m_parent->find_label(name);
  return NULL;
}

int
sp_pcontext::push_cond(LEX_STRING *name, sp_cond_type_t *val)
{
  sp_cond_t *p= (sp_cond_t *)sql_alloc(sizeof(sp_cond_t));

  if (p == NULL)
    return 1;
  p->name.str= name->str;
  p->name.length= name->length;
  p->val= val;
  return insert_dynamic(&m_conds, (uchar *)&p);
}

/*
  See comment for find_variable() above
*/
sp_cond_type_t *
sp_pcontext::find_cond(LEX_STRING *name, my_bool scoped)
{
  uint i= m_conds.elements;

  while (i--)
  {
    sp_cond_t *p;

    get_dynamic(&m_conds, (uchar*)&p, i);
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

/*
  This only searches the current context, for error checking of
  duplicates.
  Returns TRUE if found.
*/
bool
sp_pcontext::find_handler(sp_cond_type_t *cond)
{
  uint i= m_handlers.elements;

  while (i--)
  {
    sp_cond_type_t *p;

    get_dynamic(&m_handlers, (uchar*)&p, i);
    if (cond->type == p->type)
    {
      switch (p->type)
      {
      case sp_cond_type_t::number:
	if (cond->mysqlerr == p->mysqlerr)
	  return TRUE;
	break;
      case sp_cond_type_t::state:
	if (strcmp(cond->sqlstate, p->sqlstate) == 0)
	  return TRUE;
	break;
      default:
	return TRUE;
      }
    }
  }
  return FALSE;
}

int
sp_pcontext::push_cursor(LEX_STRING *name)
{
  LEX_STRING n;

  if (m_cursors.elements == m_max_cursor_index)
    m_max_cursor_index+= 1;
  n.str= name->str;
  n.length= name->length;
  return insert_dynamic(&m_cursors, (uchar *)&n);
}

/*
  See comment for find_variable() above
*/
my_bool
sp_pcontext::find_cursor(LEX_STRING *name, uint *poff, my_bool scoped)
{
  uint i= m_cursors.elements;

  while (i--)
  {
    LEX_STRING n;

    get_dynamic(&m_cursors, (uchar*)&n, i);
    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)n.str, n.length) == 0)
    {
      *poff= m_cursor_offset + i;
      return TRUE;
    }
  }
  if (!scoped && m_parent)
    return m_parent->find_cursor(name, poff, scoped);
  return FALSE;
}


void
sp_pcontext::retrieve_field_definitions(List<Create_field> *field_def_lst)
{
  /* Put local/context fields in the result list. */

  for (uint i = 0; i < m_vars.elements; ++i)
  {
    sp_variable_t *var_def;
    get_dynamic(&m_vars, (uchar*) &var_def, i);

    field_def_lst->push_back(&var_def->field_def);
  }

  /* Put the fields of the enclosed contexts in the result list. */

  List_iterator_fast<sp_pcontext> li(m_children);
  sp_pcontext *ctx;

  while ((ctx = li++))
    ctx->retrieve_field_definitions(field_def_lst);
}

/*
  Find a cursor by offset from the top.
  This is only used for debugging.
*/
my_bool
sp_pcontext::find_cursor(uint offset, LEX_STRING *n)
{
  if (m_cursor_offset <= offset &&
      offset < m_cursor_offset + m_cursors.elements)
  {                           // This frame
    get_dynamic(&m_cursors, (uchar*)n, offset - m_cursor_offset);
    return TRUE;
  }
  if (m_parent)
    return m_parent->find_cursor(offset, n); // Some previous frame
  return FALSE;                 // index out of bounds
}
