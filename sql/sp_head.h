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

class sp_instr;

class sp_head : public Sql_alloc
{
  sp_head(const sp_head &);	/* Prevent use of these */
  void operator=(sp_head &);

public:

  static void *operator new(size_t size)
  {
    return (void*) sql_alloc((uint) size);
  }

  static void operator delete(void *ptr, size_t size)
  {
    /* Empty */
  }

  sp_head(LEX_STRING *name, LEX* lex);

  int
  create(THD *thd);
  
  int
  execute(THD *thd);

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

private:

  Item_string *m_name;
  Item_string *m_defstr;
  LEX *m_mylex;			// My own lex
  LEX m_lex;			// Temp. store for the other lex
  DYNAMIC_ARRAY m_instr;	// The "instructions"

  inline sp_instr *
  get_instr(uint i)
  {
    sp_instr *in= NULL;

    get_dynamic(&m_instr, (gptr)&in, i);
    return in;
  }

}; // class sp_head : public Sql_alloc


//
// Find a stored procedure given its name. Returns NULL if not
// found.
//
sp_head *
sp_find(THD *thd, Item_string *name);

int
sp_create_procedure(THD *thd, char *name, uint namelen, char *def, uint deflen);

int
sp_drop(THD *thd, char *name, uint namelen);


class sp_instr : public Sql_alloc
{
  sp_instr(const sp_instr &);	/* Prevent use of these */
  void operator=(sp_instr &);

public:

  // Should give each a name or type code for debugging purposes?
  sp_instr()
    : Sql_alloc()
  {}

  virtual ~sp_instr()
  {}

  // Execute this instrution. '*offsetp' will be set to an offset to the
  // next instruction to execute. (For most instruction this will be 1,
  // i.e. the following instruction.)
  // Returns 0 on success, non-zero if some error occured.
  virtual int
  execute(THD *thd, int *offsetp)
  {				// Default is a no-op.
    *offsetp = 1;		// Next instruction
    return 0;
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

  sp_instr_stmt()
    : sp_instr()
  {}

  virtual ~sp_instr_stmt()
  {}

  virtual int  execute(THD *thd, int *offsetp);

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

  sp_instr_set(uint offset, Item *val, enum enum_field_types type)
    : sp_instr(), m_offset(offset), m_value(val), m_type(type)
  {}

  virtual ~sp_instr_set()
  {}

  virtual int execute(THD *thd, int *offsetp);

private:

  uint m_offset;
  Item *m_value;
  enum enum_field_types m_type;	// The declared type

}; // class sp_instr_set : public sp_instr

#endif /* _SP_HEAD_H_ */
