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

Item_result
sp_map_result_type(enum enum_field_types type);

struct sp_label;
class sp_instr;
struct sp_cond_type;
struct sp_pvar;

class sp_head : public Sql_alloc
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

public:

  int m_type;			// TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE
  enum enum_field_types m_returns; // For FUNCTIONs only
  my_bool m_has_return;		// For FUNCTIONs only
  my_bool m_simple_case;	// TRUE if parsing simple case, FALSE otherwise
  my_bool m_multi_results;	// TRUE if a procedure with SELECT(s)
  uint m_old_cmq;		// Old CLIENT_MULTI_QUERIES value
  st_sp_chistics *m_chistics;
#if NOT_USED_NOW
  // QQ We're not using this at the moment.
  List<char *> m_calls;		// Called procedures.
  List<char *> m_tables;	// Used tables.
#endif
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
  init_strings(THD *thd, LEX *lex, LEX_STRING *name);

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

  inline void
  add_instr(sp_instr *i)
  {
    insert_dynamic(&m_instr, (gptr)&i);
  }

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
		st_sp_chistics *chistics);

  inline void reset_thd_mem_root(THD *thd)
  {
    m_thd_root= thd->mem_root;
    thd->mem_root= m_mem_root;
    m_free_list= thd->free_list; // Keep the old list
    thd->free_list= NULL;	// Start a new one
    m_thd= thd;
  }

  inline void restore_thd_mem_root(THD *thd)
  {
    Item *flist= m_free_list;	// The old list
    m_free_list= thd->free_list; // Get the new one
    thd->free_list= flist;	// Restore the old one
    m_mem_root= thd->mem_root;
    thd->mem_root= m_thd_root;
    m_thd= NULL;
  }

private:

  MEM_ROOT m_mem_root;		// My own mem_root
  MEM_ROOT m_thd_root;		// Temp. store for thd's mem_root
  Item *m_free_list;		// Where the items go
  THD *m_thd;			// Set if we have reset mem_root

  sp_pcontext *m_pcont;		// Parse context
  List<LEX> m_lex;		// Temp. store for the other lex
  DYNAMIC_ARRAY m_instr;	// The "instructions"
  typedef struct
  {
    struct sp_label *lab;
    sp_instr *instr;
  } bp_t;
  List<bp_t> m_backpatch;	// Instructions needing backpatching

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

  // Should give each a name or type code for debugging purposes?
  sp_instr(uint ip)
    : Sql_alloc(), m_ip(ip)
  {}

  virtual ~sp_instr()
  {}

  // Execute this instrution. '*nextp' will be set to the index of the next
  // instruction to execute. (For most instruction this will be the
  // instruction following this one.)
  // Returns 0 on success, non-zero if some error occured.
  virtual int
  execute(THD *thd, uint *nextp)
  {				// Default is a no-op.
    *nextp = m_ip+1;		// Next instruction
    return 0;
  }

protected:

  uint m_ip;			// My index

}; // class sp_instr : public Sql_alloc


//
// Call out to some prepared SQL statement.
//
class sp_instr_stmt : public sp_instr
{
  sp_instr_stmt(const sp_instr_stmt &);	/* Prevent use of these */
  void operator=(sp_instr_stmt &);

public:

  sp_instr_stmt(uint ip)
    : sp_instr(ip), m_lex(NULL)
  {}

  virtual ~sp_instr_stmt();

  virtual int execute(THD *thd, uint *nextp);

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

  sp_instr_set(uint ip, uint offset, Item *val, enum enum_field_types type)
    : sp_instr(ip), m_offset(offset), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_offset;		// Frame offset
  Item *m_value;
  enum enum_field_types m_type;	// The declared type

}; // class sp_instr_set : public sp_instr


class sp_instr_jump : public sp_instr
{
  sp_instr_jump(const sp_instr_jump &);	/* Prevent use of these */
  void operator=(sp_instr_jump &);

public:

  sp_instr_jump(uint ip)
    : sp_instr(ip)
  {}

  sp_instr_jump(uint ip, uint dest)
    : sp_instr(ip), m_dest(dest)
  {}

  virtual ~sp_instr_jump()
  {}

  virtual int execute(THD *thd, uint *nextp);

  virtual void
  set_destination(uint dest)
  {
    m_dest= dest;
  }

protected:

  int m_dest;			// Where we will go

}; // class sp_instr_jump : public sp_instr


class sp_instr_jump_if : public sp_instr_jump
{
  sp_instr_jump_if(const sp_instr_jump_if &); /* Prevent use of these */
  void operator=(sp_instr_jump_if &);

public:

  sp_instr_jump_if(uint ip, Item *i)
    : sp_instr_jump(ip), m_expr(i)
  {}

  sp_instr_jump_if(uint ip, Item *i, uint dest)
    : sp_instr_jump(ip, dest), m_expr(i)
  {}

  virtual ~sp_instr_jump_if()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  Item *m_expr;			// The condition

}; // class sp_instr_jump_if : public sp_instr_jump


class sp_instr_jump_if_not : public sp_instr_jump
{
  sp_instr_jump_if_not(const sp_instr_jump_if_not &); /* Prevent use of these */
  void operator=(sp_instr_jump_if_not &);

public:

  sp_instr_jump_if_not(uint ip, Item *i)
    : sp_instr_jump(ip), m_expr(i)
  {}

  sp_instr_jump_if_not(uint ip, Item *i, uint dest)
    : sp_instr_jump(ip, dest), m_expr(i)
  {}

  virtual ~sp_instr_jump_if_not()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  Item *m_expr;			// The condition

}; // class sp_instr_jump_if_not : public sp_instr_jump


class sp_instr_freturn : public sp_instr
{
  sp_instr_freturn(const sp_instr_freturn &);	/* Prevent use of these */
  void operator=(sp_instr_freturn &);

public:

  sp_instr_freturn(uint ip, Item *val, enum enum_field_types type)
    : sp_instr(ip), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_freturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

protected:

  Item *m_value;
  enum enum_field_types m_type;

}; // class sp_instr_freturn : public sp_instr


class sp_instr_hpush_jump : public sp_instr_jump
{
  sp_instr_hpush_jump(const sp_instr_hpush_jump &); /* Prevent use of these */
  void operator=(sp_instr_hpush_jump &);

public:

  sp_instr_hpush_jump(uint ip, int htype, uint fp)
    : sp_instr_jump(ip), m_type(htype), m_frame(fp)
  {
    m_handler= ip+1;
    m_cond.empty();
  }

  virtual ~sp_instr_hpush_jump()
  {
    m_cond.empty();
  }

  virtual int execute(THD *thd, uint *nextp);

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

  sp_instr_hpop(uint ip, uint count)
    : sp_instr(ip), m_count(count)
  {}

  virtual ~sp_instr_hpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_count;

}; // class sp_instr_hpop : public sp_instr


class sp_instr_hreturn : public sp_instr
{
  sp_instr_hreturn(const sp_instr_hreturn &);	/* Prevent use of these */
  void operator=(sp_instr_hreturn &);

public:

  sp_instr_hreturn(uint ip, uint fp)
    : sp_instr(ip), m_frame(fp)
  {}

  virtual ~sp_instr_hreturn()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_frame;

}; // class sp_instr_hreturn : public sp_instr


class sp_instr_cpush : public sp_instr
{
  sp_instr_cpush(const sp_instr_cpush &); /* Prevent use of these */
  void operator=(sp_instr_cpush &);

public:

  sp_instr_cpush(uint ip, LEX *lex)
    : sp_instr(ip), m_lex(lex)
  {}

  virtual ~sp_instr_cpush();

  virtual int execute(THD *thd, uint *nextp);

private:

  LEX *m_lex;

}; // class sp_instr_cpush : public sp_instr


class sp_instr_cpop : public sp_instr
{
  sp_instr_cpop(const sp_instr_cpop &); /* Prevent use of these */
  void operator=(sp_instr_cpop &);

public:

  sp_instr_cpop(uint ip, uint count)
    : sp_instr(ip), m_count(count)
  {}

  virtual ~sp_instr_cpop()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_count;

}; // class sp_instr_cpop : public sp_instr


class sp_instr_copen : public sp_instr_stmt
{
  sp_instr_copen(const sp_instr_copen &); /* Prevent use of these */
  void operator=(sp_instr_copen &);

public:

  sp_instr_copen(uint ip, uint c)
    : sp_instr_stmt(ip), m_cursor(c)
  {}

  virtual ~sp_instr_copen()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_cursor;		// Stack index

}; // class sp_instr_copen : public sp_instr_stmt


class sp_instr_cclose : public sp_instr
{
  sp_instr_cclose(const sp_instr_cclose &); /* Prevent use of these */
  void operator=(sp_instr_cclose &);

public:

  sp_instr_cclose(uint ip, uint c)
    : sp_instr(ip), m_cursor(c)
  {}

  virtual ~sp_instr_cclose()
  {}

  virtual int execute(THD *thd, uint *nextp);

private:

  uint m_cursor;

}; // class sp_instr_cclose : public sp_instr


class sp_instr_cfetch : public sp_instr
{
  sp_instr_cfetch(const sp_instr_cfetch &); /* Prevent use of these */
  void operator=(sp_instr_cfetch &);

public:

  sp_instr_cfetch(uint ip, uint c)
    : sp_instr(ip), m_cursor(c)
  {
    m_varlist.empty();
  }

  virtual ~sp_instr_cfetch()
  {}

  virtual int execute(THD *thd, uint *nextp);

  void add_to_varlist(struct sp_pvar *var)
  {
    m_varlist.push_back(var);
  }

private:

  uint m_cursor;
  List<struct sp_pvar> m_varlist;

}; // class sp_instr_cfetch : public sp_instr


struct st_sp_security_context
{
  bool changed;
  uint master_access;
  uint db_access;
  char *db;
  uint db_length;
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
