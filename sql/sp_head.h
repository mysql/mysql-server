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

class sp_head : public Sql_alloc
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

public:

  int m_type;			// TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE
  enum enum_field_types m_returns; // For FUNCTIONs only
  my_bool m_simple_case;	// TRUE if parsing simple case, FALSE otherwise
#if 0
  // We're not using this at the moment.
  List<char *> m_calls;		// Called procedures.
  List<char *> m_tables;	// Used tables.
#endif

  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }

  static void operator delete(void *ptr, size_t size)
  {
    /* Empty */
  }

  sp_head(LEX_STRING *name, LEX *lex);

  int
  create(THD *thd);
  
  int
  execute_function(THD *thd, Item **args, uint argcount, Item **resp);

  int
  execute_procedure(THD *thd, List<Item> *args);

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
    String *n= m_name->const_string();

    if (lenp)
      *lenp= n->length();
    return n->c_ptr();
  }

  inline Item_result result()
  {
    return sp_map_result_type(m_returns);
  }

private:

  Item_string *m_name;
  Item_string *m_defstr;
  sp_pcontext *m_pcont;		// Parse context
  LEX m_lex;			// Temp. store for the other lex
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
    sp_instr *in= NULL;

    if (i < m_instr.elements)
      get_dynamic(&m_instr, (gptr)&in, i);
    return in;
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
    : sp_instr(ip)
  {}

  virtual ~sp_instr_stmt()
  {}

  virtual int execute(THD *thd, uint *nextp);

  inline void
  set_lex(LEX *lex)
  {
    memcpy(&m_lex, lex, sizeof(LEX));
  }

  inline LEX *
  get_lex()
  {
    return &m_lex;
  }

private:

  LEX m_lex;			// My own lex

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


class sp_instr_return : public sp_instr
{
  sp_instr_return(const sp_instr_return &);	/* Prevent use of these */
  void operator=(sp_instr_return &);

public:

  sp_instr_return(uint ip, Item *val, enum enum_field_types type)
    : sp_instr(ip), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_return()
  {}

  virtual int execute(THD *thd, uint *nextp);

protected:

  Item *m_value;
  enum enum_field_types m_type;

}; // class sp_instr_return : public sp_instr

#endif /* _SP_HEAD_H_ */
