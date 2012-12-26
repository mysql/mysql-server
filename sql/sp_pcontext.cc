/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql_priv.h"
#include "unireg.h"
#include "sp_pcontext.h"
#include "sp_head.h"


bool sp_condition_value::equals(const sp_condition_value *cv) const
{
  DBUG_ASSERT(cv);

  if (this == cv)
    return true;

  if (type != cv->type)
    return false;

  switch (type)
  {
  case sp_condition_value::ERROR_CODE:
    return (mysqlerr == cv->mysqlerr);

  case sp_condition_value::SQLSTATE:
    return (strcmp(sql_state, cv->sql_state) == 0);

  default:
    return true;
  }
}


void sp_pcontext::init(uint var_offset,
                       uint cursor_offset,
                       int num_case_expressions)
{
  m_var_offset= var_offset;
  m_cursor_offset= cursor_offset;
  m_num_case_exprs= num_case_expressions;

  m_labels.empty();
}


sp_pcontext::sp_pcontext()
  : Sql_alloc(),
  m_level(0),
  m_max_var_index(0), m_max_cursor_index(0),
  m_parent(NULL), m_pboundary(0),
  m_scope(REGULAR_SCOPE)
{
  init(0, 0, 0);
}


sp_pcontext::sp_pcontext(sp_pcontext *prev, sp_pcontext::enum_scope scope)
  : Sql_alloc(),
  m_level(prev->m_level + 1),
  m_max_var_index(0), m_max_cursor_index(0),
  m_parent(prev), m_pboundary(0),
  m_scope(scope)
{
  init(prev->m_var_offset + prev->m_max_var_index,
       prev->current_cursor_count(),
       prev->get_num_case_exprs());
}


sp_pcontext::~sp_pcontext()
{
  for (int i= 0; i < m_children.elements(); ++i)
    delete m_children.at(i);
}


sp_pcontext *sp_pcontext::push_context(THD *thd, sp_pcontext::enum_scope scope)
{
  sp_pcontext *child= new (thd->mem_root) sp_pcontext(this, scope);

  if (child)
    m_children.append(child);
  return child;
}


sp_pcontext *sp_pcontext::pop_context()
{
  m_parent->m_max_var_index+= m_max_var_index;

  uint submax= max_cursor_index();
  if (submax > m_parent->m_max_cursor_index)
    m_parent->m_max_cursor_index= submax;

  if (m_num_case_exprs > m_parent->m_num_case_exprs)
    m_parent->m_num_case_exprs= m_num_case_exprs;

  return m_parent;
}


uint sp_pcontext::diff_handlers(const sp_pcontext *ctx, bool exclusive) const
{
  uint n= 0;
  const sp_pcontext *pctx= this;
  const sp_pcontext *last_ctx= NULL;

  while (pctx && pctx != ctx)
  {
    n+= pctx->m_handlers.elements();
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return (exclusive && last_ctx ? n - last_ctx->m_handlers.elements() : n);
  return 0;			// Didn't find ctx
}


uint sp_pcontext::diff_cursors(const sp_pcontext *ctx, bool exclusive) const
{
  uint n= 0;
  const sp_pcontext *pctx= this;
  const sp_pcontext *last_ctx= NULL;

  while (pctx && pctx != ctx)
  {
    n+= pctx->m_cursors.elements();
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return  (exclusive && last_ctx ? n - last_ctx->m_cursors.elements() : n);
  return 0;			// Didn't find ctx
}


sp_variable *sp_pcontext::find_variable(LEX_STRING name,
                                        bool current_scope_only) const
{
  uint i= m_vars.elements() - m_pboundary;

  while (i--)
  {
    sp_variable *p= m_vars.at(i);

    if (my_strnncoll(system_charset_info,
		     (const uchar *)name.str, name.length,
		     (const uchar *)p->name.str, p->name.length) == 0)
    {
      return p;
    }
  }

  return (!current_scope_only && m_parent) ?
    m_parent->find_variable(name, false) :
    NULL;
}


sp_variable *sp_pcontext::find_variable(uint offset) const
{
  if (m_var_offset <= offset && offset < m_var_offset + m_vars.elements())
    return m_vars.at(offset - m_var_offset);  // This frame

  return m_parent ?
         m_parent->find_variable(offset) :    // Some previous frame
         NULL;                                // Index out of bounds
}


sp_variable *sp_pcontext::add_variable(THD *thd,
                                       LEX_STRING name,
                                       enum enum_field_types type,
                                       sp_variable::enum_mode mode)
{
  sp_variable *p=
    new (thd->mem_root) sp_variable(name, type,mode, current_var_count());

  if (!p)
    return NULL;

  ++m_max_var_index;

  return m_vars.append(p) ? NULL : p;
}


sp_label *sp_pcontext::push_label(THD *thd, LEX_STRING name, uint ip)
{
  sp_label *label=
    new (thd->mem_root) sp_label(name, ip, sp_label::IMPLICIT, this);

  if (!label)
    return NULL;

  m_labels.push_front(label);

  return label;
}


sp_label *sp_pcontext::find_label(LEX_STRING name)
{
  List_iterator_fast<sp_label> li(m_labels);
  sp_label *lab;

  while ((lab= li++))
  {
    if (my_strcasecmp(system_charset_info, name.str, lab->name.str) == 0)
      return lab;
  }

  /*
    Note about exception handlers.
    See SQL:2003 SQL/PSM (ISO/IEC 9075-4:2003),
    section 13.1 <compound statement>,
    syntax rule 4.
    In short, a DECLARE HANDLER block can not refer
    to labels from the parent context, as they are out of scope.
  */
  return (m_parent && (m_scope == REGULAR_SCOPE)) ?
         m_parent->find_label(name) :
         NULL;
}


bool sp_pcontext::add_condition(THD *thd,
                                LEX_STRING name,
                                sp_condition_value *value)
{
  sp_condition *p= new (thd->mem_root) sp_condition(name, value);

  if (p == NULL)
    return true;

  return m_conditions.append(p);
}


sp_condition_value *sp_pcontext::find_condition(LEX_STRING name,
                                                bool current_scope_only) const
{
  uint i= m_conditions.elements();

  while (i--)
  {
    sp_condition *p= m_conditions.at(i);

    if (my_strnncoll(system_charset_info,
		     (const uchar *) name.str, name.length,
		     (const uchar *) p->name.str, p->name.length) == 0)
    {
      return p->value;
    }
  }

  return (!current_scope_only && m_parent) ?
    m_parent->find_condition(name, false) :
    NULL;
}


sp_handler *sp_pcontext::add_handler(THD *thd,
                                     sp_handler::enum_type type)
{
  sp_handler *h= new (thd->mem_root) sp_handler(type, this);

  if (!h)
    return NULL;

  return m_handlers.append(h) ? NULL : h;
}


bool sp_pcontext::check_duplicate_handler(
  const sp_condition_value *cond_value) const
{
  for (int i= 0; i < m_handlers.elements(); ++i)
  {
    sp_handler *h= m_handlers.at(i);

    List_iterator_fast<sp_condition_value> li(h->condition_values);
    sp_condition_value *cv;

    while ((cv= li++))
    {
      if (cond_value->equals(cv))
        return true;
    }
  }

  return false;
}


sp_handler*
sp_pcontext::find_handler(const char *sql_state,
                          uint sql_errno,
                          Sql_condition::enum_severity_level severity) const
{
  sp_handler *found_handler= NULL;
  sp_condition_value *found_cv= NULL;

  for (int i= 0; i < m_handlers.elements(); ++i)
  {
    sp_handler *h= m_handlers.at(i);

    List_iterator_fast<sp_condition_value> li(h->condition_values);
    sp_condition_value *cv;

    while ((cv= li++))
    {
      switch (cv->type)
      {
      case sp_condition_value::ERROR_CODE:
        if (sql_errno == cv->mysqlerr &&
            (!found_cv ||
             found_cv->type > sp_condition_value::ERROR_CODE))
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::SQLSTATE:
        if (strcmp(sql_state, cv->sql_state) == 0 &&
            (!found_cv ||
             found_cv->type > sp_condition_value::SQLSTATE))
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::WARNING:
        if ((is_sqlstate_warning(sql_state) ||
             severity == Sql_condition::SL_WARNING) && !found_cv)
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::NOT_FOUND:
        if (is_sqlstate_not_found(sql_state) && !found_cv)
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::EXCEPTION:
        if (is_sqlstate_exception(sql_state) &&
            severity == Sql_condition::SL_ERROR && !found_cv)
        {
          found_cv= cv;
          found_handler= h;
        }
        break;
      }
    }
  }

  if (found_handler)
    return found_handler;


  // There is no appropriate handler in this parsing context. We need to look up
  // in parent contexts. There might be two cases here:
  //
  // 1. The current context has REGULAR_SCOPE. That means, it's a simple
  // BEGIN..END block:
  //     ...
  //     BEGIN
  //       ... # We're here.
  //     END
  //     ...
  // In this case we simply call find_handler() on parent's context recursively.
  //
  // 2. The current context has HANDLER_SCOPE. That means, we're inside an
  // SQL-handler block:
  //   ...
  //   DECLARE ... HANDLER FOR ...
  //   BEGIN
  //     ... # We're here.
  //   END
  //   ...
  // In this case we can not just call parent's find_handler(), because
  // parent's handler don't catch conditions from this scope. Instead, we should
  // try to find first parent context (we might have nested handler
  // declarations), which has REGULAR_SCOPE (i.e. which is regular BEGIN..END
  // block).

  const sp_pcontext *p= this;

  while (p && p->m_scope == HANDLER_SCOPE)
    p= p->m_parent;

  if (!p || !p->m_parent)
    return NULL;

  return p->m_parent->find_handler(sql_state, sql_errno, severity);
}


bool sp_pcontext::add_cursor(LEX_STRING name)
{
  if (m_cursors.elements() == (int) m_max_cursor_index)
    ++m_max_cursor_index;

  return m_cursors.append(name);
}


bool sp_pcontext::find_cursor(LEX_STRING name,
                              uint *poff,
                              bool current_scope_only) const
{
  uint i= m_cursors.elements();

  while (i--)
  {
    LEX_STRING n= m_cursors.at(i);

    if (my_strnncoll(system_charset_info,
		     (const uchar *) name.str, name.length,
		     (const uchar *) n.str, n.length) == 0)
    {
      *poff= m_cursor_offset + i;
      return true;
    }
  }

  return (!current_scope_only && m_parent) ?
    m_parent->find_cursor(name, poff, false) :
    false;
}


void sp_pcontext::retrieve_field_definitions(
  List<Create_field> *field_def_lst) const
{
  /* Put local/context fields in the result list. */

  for (int i= 0; i < m_vars.elements(); ++i)
  {
    sp_variable *var_def= m_vars.at(i);

    field_def_lst->push_back(&var_def->field_def);
  }

  /* Put the fields of the enclosed contexts in the result list. */

  for (int i= 0; i < m_children.elements(); ++i)
    m_children.at(i)->retrieve_field_definitions(field_def_lst);
}


const LEX_STRING *sp_pcontext::find_cursor(uint offset) const
{
  if (m_cursor_offset <= offset &&
      offset < m_cursor_offset + m_cursors.elements())
  {
    return &m_cursors.at(offset - m_cursor_offset);   // This frame
  }

  return m_parent ?
         m_parent->find_cursor(offset) :  // Some previous frame
         NULL;                            // Index out of bounds
}
