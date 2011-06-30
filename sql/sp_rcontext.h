/* -*- C++ -*- */
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

#ifndef _SP_RCONTEXT_H_
#define _SP_RCONTEXT_H_

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "sql_class.h"                    // select_result_interceptor

struct sp_cond_type;
class sp_cursor;
struct sp_variable;
class sp_lex_keeper;
class sp_instr_cpush;
class Query_arena;
class sp_head;
class sp_pcontext;
class Item_cache;
typedef class st_select_lex_unit SELECT_LEX_UNIT;
class Server_side_cursor;

#define SP_HANDLER_NONE      0
#define SP_HANDLER_EXIT      1
#define SP_HANDLER_CONTINUE  2
#define SP_HANDLER_UNDO      3

typedef struct
{
  /** Condition caught by this HANDLER. */
  struct sp_cond_type *cond;
  /** Location (instruction pointer) of the handler code. */
  uint handler;
  /** Handler type (EXIT, CONTINUE). */
  int type;
} sp_handler_t;

typedef struct
{
  /** Instruction pointer of the active handler. */
  uint ip;
  /** Handler index of the active handler. */
  uint index;
} sp_active_handler_t;

/*
  This class is a runtime context of a Stored Routine. It is used in an
  execution and is intended to contain all dynamic objects (i.e.  objects, which
  can be changed during execution), such as:
    - stored routine variables;
    - cursors;
    - handlers;

  Runtime context is used with sp_head class. sp_head class is intended to
  contain all static things, related to the stored routines (code, for example).
  sp_head instance creates runtime context for the execution of a stored
  routine.

  There is a parsing context (an instance of sp_pcontext class), which is used
  on parsing stage. However, now it contains some necessary for an execution
  things, such as definition of used stored routine variables. That's why
  runtime context needs a reference to the parsing context.
*/

class sp_rcontext : public Sql_alloc
{
  sp_rcontext(const sp_rcontext &); /* Prevent use of these */
  void operator=(sp_rcontext &);

 public:

  /*
    Arena used to (re) allocate items on . E.g. reallocate INOUT/OUT
    SP parameters when they don't fit into prealloced items. This
    is common situation with String items. It is used mainly in
    sp_eval_func_item().
  */
  Query_arena *callers_arena;

  /*
    End a open result set before start executing a continue/exit
    handler if one is found as otherwise the client will hang
    due to a violation of the client/server protocol.
  */
  bool end_partial_result_set;

#ifndef DBUG_OFF
  /*
    The routine for which this runtime context is created. Used for checking
    if correct runtime context is used for variable handling.
  */
  sp_head *sp;
#endif

  sp_rcontext(sp_pcontext *root_parsing_ctx, Field *return_value_fld,
              sp_rcontext *prev_runtime_ctx);
  bool init(THD *thd);

  ~sp_rcontext();

  int
  set_variable(THD *thd, uint var_idx, Item **value);

  Item *
  get_item(uint var_idx);

  Item **
  get_item_addr(uint var_idx);

  bool
  set_return_value(THD *thd, Item **return_value_item);

  inline bool
  is_return_value_set() const
  {
    return m_return_value_set;
  }

  /*
    SQL handlers support.
  */

  void push_handler(struct sp_cond_type *cond, uint h, int type);

  void pop_handlers(uint count);

  bool
  find_handler(THD *thd,
               uint sql_errno,
               const char *sqlstate,
               MYSQL_ERROR::enum_warning_level level,
               const char *msg);

  MYSQL_ERROR *
  raised_condition() const;

  void
  push_hstack(uint h);

  uint
  pop_hstack();

  bool
  activate_handler(THD *thd,
                   uint *ip,
                   sp_instr *instr,
                   Query_arena *execute_arena,
                   Query_arena *backup_arena);


  void
  exit_handler();

  void
  push_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i);

  void
  pop_cursors(uint count);

  inline void
  pop_all_cursors()
  {
    pop_cursors(m_ccount);
  }

  inline sp_cursor *
  get_cursor(uint i)
  {
    return m_cstack[i];
  }

  /*
    CASE expressions support.
  */

  int
  set_case_expr(THD *thd, int case_expr_id, Item **case_expr_item_ptr);

  Item *
  get_case_expr(int case_expr_id);

  Item **
  get_case_expr_addr(int case_expr_id);

private:
  sp_pcontext *m_root_parsing_ctx;

  /* Virtual table for storing variables. */
  TABLE *m_var_table;

  /*
    Collection of Item_field proxies, each of them points to the corresponding
    field in m_var_table.
  */
  Item **m_var_items;

  /*
    This is a pointer to a field, which should contain return value for stored
    functions (only). For stored procedures, this pointer is NULL.
  */
  Field *m_return_value_fld;

  /*
    Indicates whether the return value (in m_return_value_fld) has been set
    during execution.
  */
  bool m_return_value_set;

  /**
    TRUE if the context is created for a sub-statement.
  */
  bool in_sub_stmt;

  sp_handler_t *m_handler;      // Visible handlers

  /**
    SQL conditions caught by each handler.
    This is an array indexed by handler index.
  */
  MYSQL_ERROR *m_raised_conditions;

  uint m_hcount;                // Stack pointer for m_handler
  uint *m_hstack;               // Return stack for continue handlers
  uint m_hsp;                   // Stack pointer for m_hstack
  /** Active handler stack. */
  sp_active_handler_t *m_in_handler;
  uint m_ihsp;                  // Stack pointer for m_in_handler
  int m_hfound;                 // Set by find_handler; -1 if not found

  sp_cursor **m_cstack;
  uint m_ccount;

  Item_cache **m_case_expr_holders;

  /* Previous runtime context (NULL if none) */
  sp_rcontext *m_prev_runtime_ctx;

private:
  bool init_var_table(THD *thd);
  bool init_var_items();

  Item_cache *create_case_expr_holder(THD *thd, const Item *item);

  int set_variable(THD *thd, Field *field, Item **value);
}; // class sp_rcontext : public Sql_alloc


/*
  An interceptor of cursor result set used to implement
  FETCH <cname> INTO <varlist>.
*/

class Select_fetch_into_spvars: public select_result_interceptor
{
  List<struct sp_variable> *spvar_list;
  uint field_count;
public:
  Select_fetch_into_spvars() {}               /* Remove gcc warning */
  uint get_field_count() { return field_count; }
  void set_spvar_list(List<struct sp_variable> *vars) { spvar_list= vars; }

  virtual bool send_eof() { return FALSE; }
  virtual bool send_data(List<Item> &items);
  virtual int prepare(List<Item> &list, SELECT_LEX_UNIT *u);
};


/* A mediator between stored procedures and server side cursors */

class sp_cursor : public Sql_alloc
{
public:

  sp_cursor(sp_lex_keeper *lex_keeper, sp_instr_cpush *i);

  virtual ~sp_cursor()
  {
    destroy();
  }

  sp_lex_keeper *
  get_lex_keeper() { return m_lex_keeper; }

  int
  open(THD *thd);

  int
  close(THD *thd);

  inline my_bool
  is_open()
  {
    return test(server_side_cursor);
  }

  int
  fetch(THD *, List<struct sp_variable> *vars);

  inline sp_instr_cpush *
  get_instr()
  {
    return m_i;
  }

private:

  Select_fetch_into_spvars result;
  sp_lex_keeper *m_lex_keeper;
  Server_side_cursor *server_side_cursor;
  sp_instr_cpush *m_i;		// My push instruction
  void
  destroy();

}; // class sp_cursor : public Sql_alloc

#endif /* _SP_RCONTEXT_H_ */
