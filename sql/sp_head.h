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

#ifdef USE_PRAGMA_INTERFACE
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

uint
sp_get_flags_for_command(LEX *lex);

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
  /*
    Key representing routine in the set of stored routines used by statement.
    Consists of 1-byte routine type and m_qname (which usually refences to
    same buffer). Note that one must complete initialization of the key by
    calling set_routine_type().
  */
  LEX_STRING m_sroutines_key;

  sp_name(LEX_STRING name)
    : m_name(name)
  {
    m_db.str= m_qname.str= m_sroutines_key.str= 0;
    m_db.length= m_qname.length= m_sroutines_key.length= 0;
  }

  sp_name(LEX_STRING db, LEX_STRING name)
    : m_db(db), m_name(name)
  {
    m_qname.str= m_sroutines_key.str= 0;
    m_qname.length= m_sroutines_key.length= 0;
  }

  /*
    Creates temporary sp_name object from key, used mainly
    for SP-cache lookups.
  */
  sp_name(char *key, uint key_len)
  {
    m_sroutines_key.str= key;
    m_sroutines_key.length= key_len;
    m_name.str= m_qname.str= key + 1;
    m_name.length= m_qname.length= key_len - 1;
    m_db.str= 0;
    m_db.length= 0;
  }

  // Init. the qualified name from the db and name.
  void init_qname(THD *thd);	// thd for memroot allocation

  void set_routine_type(char type)
  {
    m_sroutines_key.str[0]= type;
  }

  ~sp_name()
  {}
};

sp_name *
sp_name_current_db_new(THD *thd, LEX_STRING name);


class sp_head :private Query_arena
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

  MEM_ROOT main_mem_root;
public:
  /* Possible values of m_flags */
  enum {
    HAS_RETURN= 1,              // For FUNCTIONs only: is set if has RETURN
    IN_SIMPLE_CASE= 2,          // Is set if parsing a simple CASE
    IN_HANDLER= 4,              // Is set if the parser is in a handler body
    MULTI_RESULTS= 8,           // Is set if a procedure with SELECT(s)
    CONTAINS_DYNAMIC_SQL= 16,   // Is set if a procedure with PREPARE/EXECUTE
    IS_INVOKED= 32,             // Is set if this sp_head is being used
    HAS_SET_AUTOCOMMIT_STMT = 64 // Is set if a procedure with 'set autocommit'
  };

  int m_type;			// TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE
  uint m_flags;                 // Boolean attributes of a stored routine
  enum enum_field_types m_returns; // For FUNCTIONs only
  Field::geometry_type m_geom_returns;
  CHARSET_INFO *m_returns_cs;	// For FUNCTIONs only
  TYPELIB *m_returns_typelib;	// For FUNCTIONs only
  uint m_returns_len;		// For FUNCTIONs only
  uint m_returns_pack;		// For FUNCTIONs only
  uchar *m_tmp_query;		// Temporary pointer to sub query string
  uint m_old_cmq;		// Old CLIENT_MULTI_QUERIES value
  st_sp_chistics *m_chistics;
  ulong m_sql_mode;		// For SHOW CREATE and execution
  LEX_STRING m_qname;		// db.name
  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_params;
  LEX_STRING m_body;
  LEX_STRING m_defstr;
  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;
  longlong m_created;
  longlong m_modified;
  /*
    Set containing names of stored routines used by this routine.
    Note that unlike elements of similar set for statement elements of this
    set are not linked in one list. Because of this we are able save memory
    by using for this set same objects that are used in 'sroutines' sets
    for statements of which this stored routine consists.
  */
  HASH m_sroutines;
  // Pointers set during parsing
  uchar *m_param_begin, *m_param_end, *m_body_begin;

  /* security context for SP procedure/function in case we switch it*/
  st_security_context m_security_ctx;

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

  TYPELIB *
  create_typelib(List<String> *src);

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

  Field *make_field(uint max_length, const char *name, TABLE *dummy);

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

  /* Add tables used by routine to the table list. */
  bool add_used_tables_to_table_list(THD *thd,
                                     TABLE_LIST ***query_tables_last_ptr);

  /*
    Check if this stored routine contains statements disallowed
    in a stored function or trigger, and set an appropriate error message
    if this is the case.
  */
  bool is_not_allowed_in_function(const char *where)
  {
    if (m_flags & CONTAINS_DYNAMIC_SQL)
      my_error(ER_STMT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0), "Dynamic SQL");
    else if (m_flags & MULTI_RESULTS)
      my_error(ER_SP_NO_RETSET, MYF(0), where);
    else if (m_flags & HAS_SET_AUTOCOMMIT_STMT)
      my_error(ER_SP_CANT_SET_AUTOCOMMIT, MYF(0));
    return test(m_flags &
		(CONTAINS_DYNAMIC_SQL|MULTI_RESULTS|HAS_SET_AUTOCOMMIT_STMT));
  }
private:

  MEM_ROOT *m_thd_root;		// Temp. store for thd's mem_root
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
  /*
    Multi-set representing optimized list of tables to be locked by this
    routine. Does not include tables which are used by invoked routines.

    Note: for prelocking-free SPs this multiset is constructed too.
    We do so because the same instance of sp_head may be called both
    in prelocked mode and in non-prelocked mode.
  */
  HASH m_sptabs;

  int
  execute(THD *thd);

  /*
    Merge the list of tables used by query into the multi-set of tables used
    by routine.
  */
  bool merge_table_list(THD *thd, TABLE_LIST *table, LEX *lex_for_tmp_check);
}; // class sp_head : public Sql_alloc


//
// "Instructions"...
//

class sp_instr :public Query_arena, public Sql_alloc
{
  sp_instr(const sp_instr &);	/* Prevent use of these */
  void operator=(sp_instr &);

public:

  uint marked;
  uint m_ip;			// My index
  sp_pcontext *m_ctx;		// My parse context

  // Should give each a name or type code for debugging purposes?
  sp_instr(uint ip, sp_pcontext *ctx)
    :Query_arena(0, INITIALIZED_FOR_SP), marked(0), m_ip(ip), m_ctx(ctx)
  {}

  virtual ~sp_instr()
  { free_items(); }


  /*
    Execute this instruction

    SYNOPSIS
       execute()
         thd        Thread handle
         nextp  OUT index of the next instruction to execute. (For most
                    instructions this will be the instruction following this
                    one).
 
     RETURN 
       0      on success, 
       other  if some error occured
  */
  
  virtual int execute(THD *thd, uint *nextp) = 0;

  /*
    Execute core function of instruction after all preparations (e.g.
    setting of proper LEX, saving part of the thread context have been
    done).

    Should be implemented for instructions using expressions or whole
    statements (thus having to have own LEX). Used in concert with
    sp_lex_keeper class and its descendants (there are none currently).
  */
  virtual int exec_core(THD *thd, uint *nextp);

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


/*
  Auxilary class to which instructions delegate responsibility
  for handling LEX and preparations before executing statement
  or calculating complex expression.

  Exist mainly to avoid having double hierarchy between instruction
  classes.

  TODO: Add ability to not store LEX and do any preparations if
        expression used is simple.
*/

class sp_lex_keeper
{
  /* Prevent use of these */
  sp_lex_keeper(const sp_lex_keeper &);
  void operator=(sp_lex_keeper &);
public:

  sp_lex_keeper(LEX *lex, bool lex_resp)
    : m_lex(lex), m_lex_resp(lex_resp), 
      lex_query_tables_own_last(NULL)
  {
    lex->sp_lex_in_use= TRUE;
  }
  virtual ~sp_lex_keeper()
  {
    if (m_lex_resp)
      delete m_lex;
  }

  /*
    Prepare execution of instruction using LEX, if requested check whenever
    we have read access to tables used and open/lock them, call instruction's
    exec_core() method, perform cleanup afterwards.
  */
  int reset_lex_and_exec_core(THD *thd, uint *nextp, bool open_tables,
                              sp_instr* instr);

  inline uint sql_command() const
  {
    return (uint)m_lex->sql_command;
  }

  void disable_query_cache()
  {
    m_lex->safe_to_cache_query= 0;
  }
private:

  LEX *m_lex;
  /*
    Indicates whenever this sp_lex_keeper instance responsible
    for LEX deletion.
  */
  bool m_lex_resp;

  /*
    Support for being able to execute this statement in two modes:
    a) inside prelocked mode set by the calling procedure or its ancestor.
    b) outside of prelocked mode, when this statement enters/leaves
       prelocked mode itself.
  */
  
  /*
    List of additional tables this statement needs to lock when it
    enters/leaves prelocked mode on its own.
  */
  TABLE_LIST *prelocking_tables;

  /*
    The value m_lex->query_tables_own_last should be set to this when the
    statement enters/leaves prelocked mode on its own.
  */
  TABLE_LIST **lex_query_tables_own_last;
};


//
// Call out to some prepared SQL statement.
//
class sp_instr_stmt : public sp_instr
{
  sp_instr_stmt(const sp_instr_stmt &);	/* Prevent use of these */
  void operator=(sp_instr_stmt &);

public:

  LEX_STRING m_query;		// For thd->query

  sp_instr_stmt(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE)
  {
    m_query.str= 0;
    m_query.length= 0;
  }

  virtual ~sp_instr_stmt()
  {};

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_stmt : public sp_instr


class sp_instr_set : public sp_instr
{
  sp_instr_set(const sp_instr_set &);	/* Prevent use of these */
  void operator=(sp_instr_set &);

public:

  sp_instr_set(uint ip, sp_pcontext *ctx,
	       uint offset, Item *val, enum enum_field_types type,
               LEX *lex, bool lex_resp)
    : sp_instr(ip, ctx), m_offset(offset), m_value(val), m_type(type),
      m_lex_keeper(lex, lex_resp)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  uint m_offset;		// Frame offset
  Item *m_value;
  enum enum_field_types m_type;	// The declared type
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_set : public sp_instr


/*
  Set NEW/OLD row field value instruction. Used in triggers.
*/
class sp_instr_set_trigger_field : public sp_instr
{
  sp_instr_set_trigger_field(const sp_instr_set_trigger_field &);
  void operator=(sp_instr_set_trigger_field &);

public:

  sp_instr_set_trigger_field(uint ip, sp_pcontext *ctx,
                             Item_trigger_field *trg_fld,
                             Item *val, LEX *lex)
    : sp_instr(ip, ctx),
      trigger_field(trg_fld),
      value(val), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_set_trigger_field()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

private:
  Item_trigger_field *trigger_field;
  Item *value;
  sp_lex_keeper m_lex_keeper;
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

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i, LEX *lex)
    : sp_instr_jump(ip, ctx), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  sp_instr_jump_if(uint ip, sp_pcontext *ctx, Item *i, uint dest, LEX *lex)
    : sp_instr_jump(ip, ctx, dest), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_jump_if()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_jump_if : public sp_instr_jump


class sp_instr_jump_if_not : public sp_instr_jump
{
  sp_instr_jump_if_not(const sp_instr_jump_if_not &); /* Prevent use of these */
  void operator=(sp_instr_jump_if_not &);

public:

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, LEX *lex)
    : sp_instr_jump(ip, ctx), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  sp_instr_jump_if_not(uint ip, sp_pcontext *ctx, Item *i, uint dest, LEX *lex)
    : sp_instr_jump(ip, ctx, dest), m_expr(i), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_jump_if_not()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

  virtual uint opt_shortcut_jump(sp_head *sp, sp_instr *start)
  {
    return m_ip;
  }

private:

  Item *m_expr;			// The condition
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_jump_if_not : public sp_instr_jump


class sp_instr_freturn : public sp_instr
{
  sp_instr_freturn(const sp_instr_freturn &);	/* Prevent use of these */
  void operator=(sp_instr_freturn &);

public:

  sp_instr_freturn(uint ip, sp_pcontext *ctx,
		   Item *val, enum enum_field_types type, LEX *lex)
    : sp_instr(ip, ctx), m_value(val), m_type(type), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_freturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp)
  {
    marked= 1;
    return UINT_MAX;
  }

protected:

  Item *m_value;
  enum enum_field_types m_type;
  sp_lex_keeper m_lex_keeper;

}; // class sp_instr_freturn : public sp_instr


class sp_instr_hpush_jump : public sp_instr_jump
{
  sp_instr_hpush_jump(const sp_instr_hpush_jump &); /* Prevent use of these */
  void operator=(sp_instr_hpush_jump &);

public:

  sp_instr_hpush_jump(uint ip, sp_pcontext *ctx, int htype, uint fp)
    : sp_instr_jump(ip, ctx), m_type(htype), m_frame(fp)
  {
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


class sp_instr_hreturn : public sp_instr_jump
{
  sp_instr_hreturn(const sp_instr_hreturn &);	/* Prevent use of these */
  void operator=(sp_instr_hreturn &);

public:

  sp_instr_hreturn(uint ip, sp_pcontext *ctx, uint fp)
    : sp_instr_jump(ip, ctx), m_frame(fp)
  {}

  virtual ~sp_instr_hreturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

  virtual uint opt_mark(sp_head *sp);

private:

  uint m_frame;

}; // class sp_instr_hreturn : public sp_instr


/* This is DECLARE CURSOR */
class sp_instr_cpush : public sp_instr
{
  sp_instr_cpush(const sp_instr_cpush &); /* Prevent use of these */
  void operator=(sp_instr_cpush &);

public:

  sp_instr_cpush(uint ip, sp_pcontext *ctx, LEX *lex)
    : sp_instr(ip, ctx), m_lex_keeper(lex, TRUE)
  {}

  virtual ~sp_instr_cpush()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void print(String *str);

private:

  sp_lex_keeper m_lex_keeper;

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


class sp_instr_copen : public sp_instr
{
  sp_instr_copen(const sp_instr_copen &); /* Prevent use of these */
  void operator=(sp_instr_copen &);

public:

  sp_instr_copen(uint ip, sp_pcontext *ctx, uint c)
    : sp_instr(ip, ctx), m_cursor(c)
  {}

  virtual ~sp_instr_copen()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual int exec_core(THD *thd, uint *nextp);

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


#ifndef NO_EMBEDDED_ACCESS_CHECKS
bool
sp_change_security_context(THD *thd, sp_head *sp,
                           st_security_context **backup);
void
sp_restore_security_context(THD *thd, st_security_context *backup);
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

TABLE_LIST *
sp_add_to_query_tables(THD *thd, LEX *lex,
		       const char *db, const char *name,
		       thr_lock_type locktype);

#endif /* _SP_HEAD_H_ */
