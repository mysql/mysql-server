/* -*- C++ -*- */
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

#ifndef _SP_HEAD_H_
#define _SP_HEAD_H_

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

#include <stddef.h>

// Values for the type enum. This reflects the order of the enum declaration
// in the CREATE TABLE command.
#define TYPE_ENUM_FUNCTION  1
#define TYPE_ENUM_PROCEDURE 2
#define TYPE_ENUM_TRIGGER   3

Item_result
sp_map_result_type(enum enum_field_types type);

bool
sp_multi_results_command(enum enum_sql_command cmd);

struct sp_label;
class sp_instr;
struct sp_cond_type;
struct sp_pvar;

class sp_name : public Sql_alloc
{
public:

  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_qname;

  sp_name(LEX_STRING name)
    : m_name(name)
  {
    m_db.str= m_qname.str= 0;
    m_db.length= m_qname.length= 0;
  }

  sp_name(LEX_STRING db, LEX_STRING name)
    : m_db(db), m_name(name)
  {
    m_qname.str= 0;
    m_qname.length= 0;
  }

  // Init. the qualified name from the db and name.
  void init_qname(THD *thd);	// thd for memroot allocation

  ~sp_name()
  {}
};

sp_name *
sp_name_current_db_new(THD *thd, LEX_STRING name);


class sp_head :private Item_arena
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

public:

  int m_type;			// TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE
  enum enum_field_types m_returns; // For FUNCTIONs only
  CHARSET_INFO *m_returns_cs;	// For FUNCTIONs only
  my_bool m_has_return;		// For FUNCTIONs only
  my_bool m_simple_case;	// TRUE if parsing simple case, FALSE otherwise
  my_bool m_multi_results;	// TRUE if a procedure with SELECT(s)
  my_bool m_in_handler;		// TRUE if parser in a handler body
  uint m_old_cmq;		// Old CLIENT_MULTI_QUERIES value
  st_sp_chistics *m_chistics;
  ulong m_sql_mode;		// For SHOW CREATE
#if NOT_USED_NOW
  // QQ We're not using this at the moment.
  List<char *> m_calls;		// Called procedures.
  List<char *> m_tables;	// Used tables.
#endif
  LEX_STRING m_qname;		// db.name
  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_params;
  LEX_STRING m_retstr;		// For FUNCTIONs only
  LEX_STRING m_body;
  LEX_STRING m_defstr;
  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;
  longlong m_created;
  longlong m_modified;
  // Pointers set during parsing
  uchar *m_param_begin, *m_param_end, *m_returns_begin, *m_returns_end,
    *m_body_begin;

  static void *
  operator new(size_t size);

  static void
  operator delete(void *ptr, size_t size);

  sp_head();

  // Initialize after we have reset mem_root
  void
  init(LEX *lex);

  // Initialize strings after parsing header
  void
  init_strings(THD *thd, LEX *lex, sp_name *name);

  int
  create(THD *thd);

  virtual ~sp_head();

  // Free memory
  void
  destroy();

  int
  execute_function(THD *thd, Item **args, uint argcount, Item **resp);

  int
  execute_procedure(THD *thd, List<Item> *args);

  int
  show_create_procedure(THD *thd);

  int
  show_create_function(THD *thd);

  void
  add_instr(sp_instr *instr);

  inline uint
  instructions()
  {
    return m_instr.elements;
  }

  inline sp_instr *
  last_instruction()
  {
    sp_instr *i;

    get_dynamic(&m_instr, (gptr)&i, m_instr.elements-1);
    return i;
  }

  // Resets lex in 'thd' and keeps a copy of the old one.
  void
  reset_lex(THD *thd);

  // Restores lex in 'thd' from our copy, but keeps some status from the
  // one in 'thd', like ptr, tables, fields, etc.
  void
  restore_lex(THD *thd);

  // Put the instruction on the backpatch list, associated with the label.
  void
  push_backpatch(sp_instr *, struct sp_label *);

  // Update all instruction with this label in the backpatch list to
  // the current position.
  void
  backpatch(struct sp_label *);

  // Check that no unresolved references exist.
  // If none found, 0 is returned, otherwise errors have been issued
  // and -1 is returned.
  // This is called by the parser at the end of a create procedure/function.
  int
  check_backpatch(THD *thd);

  char *name(uint *lenp = 0) const
  {
    if (lenp)
      *lenp= m_name.length;
    return m_name.str;
  }

  char *create_string(THD *thd, ulong *lenp);

  inline Item_result result()
  {
    return sp_map_result_type(m_returns);
  }

  void set_info(char *definer, uint definerlen,
		longlong created, longlong modified,
		st_sp_chistics *chistics, ulong sql_mode);

  void reset_thd_mem_root(THD *thd);

  void restore_thd_mem_root(THD *thd);

  void optimize();
  void opt_mark(uint ip);

  inline sp_instr *
  get_instr(uint i)
  {
    sp_instr *ip;

    if (i < m_instr.elements)
      get_dynamic(&m_instr, (gptr)&ip, i);
    else
      ip= NULL;
    return ip;
  }

private:

  MEM_ROOT m_thd_root;		// Temp. store for thd's mem_root
  THD *m_thd;			// Set if we have reset mem_root
  char *m_thd_db;		// Original thd->db pointer

  sp_pcontext *m_pcont;		// Parse context
  List<LEX> m_lex;		// Temp. store for the other lex
  DYNAMIC_ARRAY m_instr;	// The "instructions"
  typedef struct
  {
    struct sp_label *lab;
    sp_instr *instr;
  } bp_t;
  List<bp_t> m_backpatch;	// Instructions needing backpatching

  int
  execute(THD *thd);

}; // class sp_head : public Sql_alloc


//
// "Instructions"...
//

class sp_instr : public Sql_alloc
{
  sp_instr(const sp_instr &);	/* Prevent use of these */
  void operator=(sp_instr &);

public:

  uint marked;
  Item *free_list;              // My Items
  uint m_ip;			// My index
  sp_pcontext *m_ctx;		// My parse context

  // Should give each a name or type code for debugging purposes?
  sp_instr(uint ip, sp_pcontext *ctx)
    :Sql_alloc(), marked(0), free_list(0), m_ip(ip), m_ctx(ctx)
  {}

  virtual ~sp_instr()
  { free_items(free_list); }

  // Execute this instrution. '*nextp' will be set to the index of the next
  // instruction to execute. (For most instruction this will be the
  // instruction following this one.)
  // Returns 0 on success, non-zero if some error occured.
  virtual int execute(THD *thd, uint *nextp) = 0;

  virtual void print(String *str) = 0;

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {}

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return m_ip+1;
  }

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  virtual void opt_move(uint dst, List<sp_instr> *ibp)
  {
    m_ip= dst;
  }

}; // class sp_instr : public Sql_alloc


//
// Call out to some prepared SQL statement.
//
class sp_instr_stmt : public sp_instr
{
  sp_instr_stmt(const sp_instr_stmt &);	/* Prevent use of these */
  void operator=(sp_instr_stmt &);

public:

  sp_instr_stmt(uint ip, sp_pcontext *ctx)
    : sp_instr(ip, ctx), m_lex(NULL)
  {}

  virtual ~sp_instr_stmt();

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  inline void
  set_lex(LEX *lex)
  {
    m_lex= lex;
  }

  inline LEX *
  get_lex()
  {
    return m_lex;
  }

protected:

  int exec_stmt(THD *thd, LEX *lex); // Execute a statement

private:

  LEX *m_lex;			// My own lex

}; // class sp_instr_stmt : public sp_instr


class sp_instr_set : public sp_instr
{
  sp_instr_set(const sp_instr_set &);	/* Prevent use of these */
  void operator=(sp_instr_set &);

public:

  TABLE_LIST *tables;

  sp_instr_set(uint ip, sp_pcontext *ctx,
	       uint offset, Item *val, enum enum_field_types type)
    : sp_instr(ip, ctx),
      tables(NULL), m_offset(offset), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_offset;		// Frame offset
  Item *m_value;
  enum enum_field_types m_type;	// The declared type

}; // class sp_instr_set : public sp_instr


/*
  Set user variable instruction.
  Used in functions and triggers to set user variables because we don't
  want use sp_instr_stmt + "SET @a:=..." statement in this case since
  latter will close all tables and thus will ruin execution of statement
  calling/invoking this function/trigger.
*/
class sp_instr_set_user_var : public sp_instr
{
  sp_instr_set_user_var(const sp_instr_set_user_var &);
  void operator=(sp_instr_set_user_var &);

public:

  sp_instr_set_user_var(uint ip, LEX_STRING var, Item *val)
    : sp_instr(ip), m_set_var_item(var, val)
  {}

  virtual ~sp_instr_set_user_var()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  Item_func_set_user_var m_set_var_item;
}; // class sp_instr_set_user_var : public sp_instr


/*
  Set NEW/OLD row field value instruction. Used in triggers.
*/
class sp_instr_set_trigger_field : public sp_instr
{
  sp_instr_set_trigger_field(const sp_instr_set_trigger_field &);
  void operator=(sp_instr_set_trigger_field &);

public:

  sp_instr_set_trigger_field(uint ip, LEX_STRING field_name, Item *val)
    : sp_instr(ip),
      trigger_field(Item_trigger_field::NEW_ROW, field_name.str),
      value(val)
  {}

  virtual ~sp_instr_set_trigger_field()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  bool setup_field(THD *thd, TABLE *table, enum trg_event_type event)
  {
    return trigger_field.setup_field(thd, table, event);
  }
private:

  Item_trigger_field trigger_field;
  Item *value;
}; // class sp_instr_trigger_field : public sp_instr


class sp_instr_jump : public sp_instr
{
  sp_instr_jump(const sp_instr_jump &);	/* Prevent use of these */
  void operator=(sp_instr_jump &);

public:

  uint m_dest;			// Where we will go

  sp_instr_jump(uint ip, sp_pcontext *ctx)
    : sp_instr(ip, ctx), m_dest(0), m_optdest(0)
  {}

  sp_instr_jump(uint ip, sp_pcontext *ctx, uint dest)
    : sp_instr(ip, ctx), m_dest(dest), m_optdest(0)
  {}

  virtual ~sp_instr_jump()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start);

  virtual void opt_move(uint dst, List<sp_instr> *ibp);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx)
  {
    if (m_dest == 0)		// Don't reset
      m_dest= dest;
  }

protected:

  sp_instr *m_optdest;		// Used during optimization

}; // class sp_instr_jump : public sp_instr


class sp_instr_jump_if : public sp_instr_jump
{
  sp_instr_jump_if(const sp_instr_jump_if &); /* Prevent use of these */
  void operator=(sp_instr_jump_if &);

public:

  TABLE_LIST *tables;

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i)
    : sp_instr_jump(ip, ctx), tables(NULL), m_expr(i)
  {}

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i, uint dest)
    : sp_instr_jump(ip, ctx, dest), tables(NULL), m_expr(i)
  {}

  virtual ~sp_instr_jump_if()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition

}; // class sp_instr_jump_if : public sp_instr_jump


class sp_instr_jump_if_not : public sp_instr_jump
{
  sp_instr_jump_if_not(const sp_instr_jump_if_not &); /* Prevent use of these */
  void operator=(sp_instr_jump_if_not &);

public:

  TABLE_LIST *tables;

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i)
    : sp_instr_jump(ip, ctx), tables(NULL), m_expr(i)
  {}

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, uint dest)
    : sp_instr_jump(ip, ctx, dest), tables(NULL), m_expr(i)
  {}

  virtual ~sp_instr_jump_if_not()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition

}; // class sp_instr_jump_if_not : public sp_instr_jump


class sp_instr_freturn : public sp_instr
{
  sp_instr_freturn(const sp_instr_freturn &);	/* Prevent use of these */
  void operator=(sp_instr_freturn &);

public:

  TABLE_LIST *tables;

  sp_instr_freturn(uint ip, sp_pcontext *ctx,
		   Item *val, enum enum_field_types type)
    : sp_instr(ip, ctx), tables(NULL), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_freturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

protected:

  Item *m_value;
  enum enum_field_types m_type;

}; // class sp_instr_freturn : public sp_instr


class sp_instr_hpush_jump : public sp_instr_jump
{
  sp_instr_hpush_jump(const sp_instr_hpush_jump &); /* Prevent use of these */
  void operator=(sp_instr_hpush_jump &);

public:

  sp_instr_hpush_jump(uint ip, sp_pcontext *ctx, int htype, uint fp)
    : sp_instr_jump(ip, ctx), m_type(htype), m_frame(fp)
  {
    m_handler= ip+1;
    m_cond.empty();
  }

  virtual ~sp_instr_hpush_jump()
  {
    m_cond.empty();
  }

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

  inline void add_condition(struct sp_cond_type *cond)
  {
    m_cond.push_front(cond);
  }

private:

  int m_type;			// Handler type
  uint m_frame;
  uint m_handler;		// Location of handler
  List<struct sp_cond_type> m_cond;

}; // class sp_instr_hpush_jump : public sp_instr_jump


class sp_instr_hpop : public sp_instr
{
  sp_instr_hpop(const sp_instr_hpop &);	/* Prevent use of these */
  void operator=(sp_instr_hpop &);

public:

  sp_instr_hpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_hpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx);

  virtual uint opt_mark(sp_head *sp)
  {
    if (m_count)
      marked= 1;
    return m_ip+1;
  }

private:

  uint m_count;

}; // class sp_instr_hpop : public sp_instr


class sp_instr_hreturn : public sp_instr
{
  sp_instr_hreturn(const sp_instr_hreturn &);	/* Prevent use of these */
  void operator=(sp_instr_hreturn &);

public:

  sp_instr_hreturn(uint ip, sp_pcontext *ctx, uint fp)
    : sp_instr(ip, ctx), m_frame(fp)
  {}

  virtual ~sp_instr_hreturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

private:

  uint m_frame;

}; // class sp_instr_hreturn : public sp_instr


class sp_instr_cpush : public sp_instr
{
  sp_instr_cpush(const sp_instr_cpush &); /* Prevent use of these */
  void operator=(sp_instr_cpush &);

public:

  sp_instr_cpush(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex(lex)
  {}

  virtual ~sp_instr_cpush();

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  LEX *m_lex;

}; // class sp_instr_cpush : public sp_instr


class sp_instr_cpop : public sp_instr
{
  sp_instr_cpop(const sp_instr_cpop &); /* Prevent use of these */
  void operator=(sp_instr_cpop &);

public:

  sp_instr_cpop(uint ip, sp_pcontext *ctx, uint count)
    : sp_instr(ip, ctx), m_count(count)
  {}

  virtual ~sp_instr_cpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual void backpatch(uint dest, sp_pcontext *dst_ctx);

  virtual uint opt_mark(sp_head *sp)
  {
    if (m_count)
      marked= 1;
    return m_ip+1;
  }

private:

  uint m_count;

}; // class sp_instr_cpop : public sp_instr


class sp_instr_copen : public sp_instr_stmt
{
  sp_instr_copen(const sp_instr_copen &); /* Prevent use of these */
  void operator=(sp_instr_copen &);

public:

  sp_instr_copen(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr_stmt(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_copen()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;		// Stack index

}; // class sp_instr_copen : public sp_instr_stmt


class sp_instr_cclose : public sp_instr
{
  sp_instr_cclose(const sp_instr_cclose &); /* Prevent use of these */
  void operator=(sp_instr_cclose &);

public:

  sp_instr_cclose(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_cclose()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_cursor;

}; // class sp_instr_cclose : public sp_instr


class sp_instr_cfetch : public sp_instr
{
  sp_instr_cfetch(const sp_instr_cfetch &); /* Prevent use of these */
  void operator=(sp_instr_cfetch &);

public:

  sp_instr_cfetch(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {
    m_varlist.empty();
  }

  virtual ~sp_instr_cfetch()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  void add_to_varlist(struct sp_pvar *var)
  {
    m_varlist.push_back(var);
  }

private:

  uint m_cursor;
  List<struct sp_pvar> m_varlist;

}; // class sp_instr_cfetch : public sp_instr


class sp_instr_error : public sp_instr
{
  sp_instr_error(const sp_instr_error &); /* Prevent use of these */
  void operator=(sp_instr_error &);

public:

  sp_instr_error(uint ip, sp_pcontext *ctx, int errcode)
    : sp_instr(ip, ctx), m_errcode(errcode)
  {}

  virtual ~sp_instr_error()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

private:

  int m_errcode;

}; // class sp_instr_error : public sp_instr


struct st_sp_security_context
{
  bool changed;
  uint master_access;
  uint db_access;
  char *priv_user;
  char priv_host[MAX_HOSTNAME];
  char *user;
  char *host;
  char *ip;
};

#ifndef NO_EMBEDDED_ACCESS_CHECKS
void
sp_change_security_context(THD *thd, sp_head *sp, st_sp_security_context *ctxp);
void
sp_restore_security_context(THD *thd, sp_head *sp,st_sp_security_context *ctxp);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

#endif /* _SP_HEAD_H_ */
