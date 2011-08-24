/* Copyright (c) 2002, 2011, Oracle and/or its affiliates. All rights reserved.

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


/**
  Check if two instances of sp_condition_value are equal or not.

  @param cv another instance of sp_condition_value to check.

  @return true if the instances are equal, false otherwise.
*/

bool
sp_condition_value::equals(const sp_condition_value *cv) const
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
    return (strcmp(sqlstate, cv->sqlstate) == 0);

  default:
    return true;
  }
}


/* Initial size for the dynamic arrays in sp_pcontext */
#define PCONTEXT_ARRAY_INIT_ALLOC 16
/* Increment size for the dynamic arrays in sp_pcontext */
#define PCONTEXT_ARRAY_INCREMENT_ALLOC 8

void
sp_pcontext::init(uint var_offset,
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
  m_max_var_index(0), m_max_cursor_index(0), m_max_handler_index(0),
  m_parent(NULL), m_pboundary(0),
  m_scope(REGULAR_SCOPE)
{
  init(0, 0, 0);
}

sp_pcontext::sp_pcontext(sp_pcontext *prev, sp_pcontext::enum_scope scope)
  : Sql_alloc(),
  m_max_var_index(0), m_max_cursor_index(0), m_max_handler_index(0),
  m_parent(prev), m_pboundary(0),
  m_scope(scope)
{
  init(prev->m_var_offset + prev->m_max_var_index,
       prev->current_cursor_count(),
       prev->get_num_case_exprs());
}

void
sp_pcontext::destroy()
{
  for (int i= 0; i < m_children.elements(); ++i)
    m_children.at(i)->destroy();

  m_vars.clear();
  m_case_expr_ids.clear();
  m_conditions.clear();
  m_cursors.clear();
  m_handlers.clear();
  m_children.clear();
  m_labels.empty();
}

sp_pcontext *
sp_pcontext::push_context(sp_pcontext::enum_scope scope)
{
  sp_pcontext *child= new sp_pcontext(this, scope);

  if (child)
    m_children.append(child);
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
    n+= pctx->get_num_handlers();
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return (exclusive && last_ctx ? n - last_ctx->get_num_handlers() : n);
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
    n+= pctx->m_cursors.elements();
    last_ctx= pctx;
    pctx= pctx->parent_context();
  }
  if (pctx)
    return  (exclusive && last_ctx ? n - last_ctx->m_cursors.elements() : n);
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
sp_variable *
sp_pcontext::find_variable(LEX_STRING *name, bool scoped)
{
  uint i= m_vars.elements() - m_pboundary;

  while (i--)
  {
    sp_variable *p= m_vars.at(i);

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
sp_variable *
sp_pcontext::find_variable(uint offset)
{
  if (m_var_offset <= offset && offset < m_var_offset + m_vars.elements())
    return m_vars.at(offset - m_var_offset);  // This frame

  return m_parent ?
         m_parent->find_variable(offset) :    // Some previous frame
         NULL;                                // Index out of bounds
}

sp_variable *
sp_pcontext::push_variable(LEX_STRING *name, enum enum_field_types type,
                           sp_variable::enum_mode mode)
{
  sp_variable *p= (sp_variable *)sql_alloc(sizeof(sp_variable));

  if (!p)
    return NULL;

  ++m_max_var_index;

  p->name.str= name->str;
  p->name.length= name->length;
  p->type= type;
  p->mode= mode;
  p->offset= current_var_count();
  p->dflt= NULL;
  return m_vars.append(p) ? NULL : p;
}


sp_label *
sp_pcontext::push_label(char *name, uint ip)
{
  sp_label *label= (sp_label *)sql_alloc(sizeof(sp_label));

  if (!label)
    return NULL;

  label->name= name;
  label->ip= ip;
  label->type= sp_label::IMPLICIT;
  label->ctx= this;
  m_labels.push_front(label);

  return label;
}

sp_label *
sp_pcontext::find_label(char *name)
{
  List_iterator_fast<sp_label> li(m_labels);
  sp_label *lab;

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
  if (m_parent && (m_scope == REGULAR_SCOPE))
    return m_parent->find_label(name);
  return NULL;
}

bool
sp_pcontext::push_condition(LEX_STRING *name, sp_condition_value *value)
{
  sp_condition *p= (sp_condition *)sql_alloc(sizeof(sp_condition));

  if (p == NULL)
    return true;

  p->name.str= name->str;
  p->name.length= name->length;
  p->value= value;

  return m_conditions.append(p);
}

/*
  See comment for find_variable() above
*/
sp_condition_value *
sp_pcontext::find_condition(LEX_STRING *name, bool scoped)
{
  uint i= m_conditions.elements();

  while (i--)
  {
    sp_condition *p= m_conditions.at(i);

    if (my_strnncoll(system_charset_info,
		     (const uchar *)name->str, name->length,
		     (const uchar *)p->name.str, p->name.length) == 0)
    {
      return p->value;
    }
  }
  if (!scoped && m_parent)
    return m_parent->find_condition(name, scoped);
  return NULL;
}

/**
  This is an auxilary parsing-time function to check if an SQL-handler exists in
  the current parsing context (current scope) for the given SQL-condition. This
  function is used to check for duplicates during the parsing phase.

  This function can not be used during the execution phase to check SQL-handler
  existence because it searches for the SQL-handler in the current scope only
  (during the execution current and parent scopes should be checked according to
  the SQL-handler resolution rules).

  @param condition_value the handler condition value (not SQL-condition!).

  @retval true if such SQL-handler exists.
  @retval false otherwise.
*/
bool
sp_pcontext::check_duplicate_handler(sp_condition_value *condition_value)
{
  for (int i= 0; i < m_handlers.elements(); ++i)
  {
    sp_handler *h= m_handlers.at(i);

    List_iterator_fast<sp_condition_value> li(h->condition_values);
    sp_condition_value *cv;

    while ((cv= li++))
    {
      if (condition_value->equals(cv))
        return true;
    }
  }

  return false;
}


/**
  Find an SQL handler for the given SQL condition according to the SQL-handler
  resolution rules. This function is used at runtime.

  @param sqlstate         The error SQL state
  @param sql_errno        The error code
  @param level            The error level

  @return a pointer to the found SQL-handler or NULL.
*/

sp_handler *
sp_pcontext::find_handler(const char *sqlstate,
                          uint sql_errno,
                          Sql_condition::enum_warning_level level)
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
        if (strcmp(sqlstate, cv->sqlstate) == 0 &&
            (!found_cv ||
             found_cv->type > sp_condition_value::SQLSTATE))
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::WARNING:
        if ((is_sqlstate_warning(sqlstate) ||
             level == Sql_condition::WARN_LEVEL_WARN) && !found_cv)
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::NOT_FOUND:
        if (is_sqlstate_not_found(sqlstate) && !found_cv)
        {
          found_cv= cv;
          found_handler= h;
        }
        break;

      case sp_condition_value::EXCEPTION:
        if (is_sqlstate_exception(sqlstate) &&
            level == Sql_condition::WARN_LEVEL_ERROR && !found_cv)
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

  sp_pcontext *p= this;

  while (p && p->m_scope == HANDLER_SCOPE)
    p= p->m_parent;

  if (!p || !p->m_parent)
    return NULL;

  return p->m_parent->find_handler(sqlstate, sql_errno, level);
}


bool
sp_pcontext::push_cursor(LEX_STRING *name)
{
  LEX_STRING n;

  if (m_cursors.elements() == (int) m_max_cursor_index)
    ++m_max_cursor_index;
  n.str= name->str;
  n.length= name->length;

  return m_cursors.append(n);
}

/*
  See comment for find_variable() above
*/
bool
sp_pcontext::find_cursor(LEX_STRING *name, uint *poff, bool scoped)
{
  uint i= m_cursors.elements();

  while (i--)
  {
    LEX_STRING n= m_cursors.at(i);

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

  for (int i= 0; i < m_vars.elements(); ++i)
  {
    sp_variable *var_def= m_vars.at(i);

    field_def_lst->push_back(&var_def->field_def);
  }

  /* Put the fields of the enclosed contexts in the result list. */

  for (int i= 0; i < m_children.elements(); ++i)
    m_children.at(i)->retrieve_field_definitions(field_def_lst);
}

/*
  Find a cursor by offset from the top.
  This is only used for debugging.
*/
bool
sp_pcontext::find_cursor(uint offset, LEX_STRING *n)
{
  if (m_cursor_offset <= offset &&
      offset < m_cursor_offset + m_cursors.elements())
  {
    *n= m_cursors.at(offset - m_cursor_offset);   // This frame
    return true;
  }

  return m_parent ?
         m_parent->find_cursor(offset, n) :       // Some previous frame
         false;                                   // Index out of bounds
}
