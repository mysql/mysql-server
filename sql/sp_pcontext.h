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

#ifndef _SP_PCONTEXT_H_
#define _SP_PCONTEXT_H_

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

typedef enum
{
  sp_param_in,
  sp_param_out,
  sp_param_inout
} sp_param_mode_t;

typedef struct sp_pvar
{
  LEX_STRING name;
  enum enum_field_types type;
  sp_param_mode_t mode;
  uint offset;			// Offset in current frame
  my_bool isset;
  Item *dflt;
} sp_pvar_t;

typedef struct sp_label
{
  char *name;
  uint ip;			// Instruction index
  my_bool isbegin;		// For ITERATE error checking
} sp_label_t;

typedef struct sp_cond_type
{
  enum { number, state, warning, notfound, exception } type;
  char sqlstate[6];
  uint mysqlerr;
} sp_cond_type_t;

typedef struct sp_cond
{
  LEX_STRING name;
  sp_cond_type_t *val;
} sp_cond_t;

typedef struct sp_scope
{
  uint vars, conds, curs;
} sp_scope_t;

class sp_pcontext : public Sql_alloc
{
  sp_pcontext(const sp_pcontext &); /* Prevent use of these */
  void operator=(sp_pcontext &);

 public:

  sp_pcontext();

  // Free memory
  void
  destroy();

  // For error checking of duplicate things
  void
  push_scope();

  void
  pop_scope();

  //
  // Parameters and variables
  //

  inline uint
  max_framesize()
  {
    return m_framesize;
  }

  inline uint
  current_framesize()
  {
    return m_pvar.elements;
  }

  inline uint
  params()
  {
    return m_params;
  }

  // Set the number of parameters to the current esize
  inline void
  set_params()
  {
    m_params= m_pvar.elements;
  }

  inline void
  set_type(uint i, enum enum_field_types type)
  {
    sp_pvar_t *p= find_pvar(i);

    if (p)
      p->type= type;
  }

  inline void
  set_isset(uint i, my_bool val)
  {
    sp_pvar_t *p= find_pvar(i);

    if (p)
      p->isset= val;
  }

  inline void
  set_default(uint i, Item *it)
  {
    sp_pvar_t *p= find_pvar(i);

    if (p)
      p->dflt= it;
  }

  void
  push_pvar(LEX_STRING *name, enum enum_field_types type, sp_param_mode_t mode);

  // Pop the last 'num' slots of the frame
  inline void
  pop_pvar(uint num = 1)
  {
    while (num--)
      pop_dynamic(&m_pvar);
  }

  // Find by name
  sp_pvar_t *
  find_pvar(LEX_STRING *name, my_bool scoped=0);

  // Find by index
  sp_pvar_t *
  find_pvar(uint i)
  {
    sp_pvar_t *p;

    if (i < m_pvar.elements)
      get_dynamic(&m_pvar, (gptr)&p, i);
    else
      p= NULL;
    return p;
  }

  //
  // Labels
  //

  sp_label_t *
  push_label(char *name, uint ip);

  sp_label_t *
  find_label(char *name);

  inline sp_label_t *
  last_label()
  {
    return m_label.head();
  }

  inline sp_label_t *
  pop_label()
  {
    return m_label.pop();
  }

  //
  // Conditions
  //

  void
  push_cond(LEX_STRING *name, sp_cond_type_t *val);

  inline void
  pop_cond(uint num)
  {
    while (num--)
      pop_dynamic(&m_cond);
  }

  sp_cond_type_t *
  find_cond(LEX_STRING *name, my_bool scoped=0);

  //
  // Handlers
  //

  inline void
  add_handler()
  {
    m_handlers+= 1;
  }

  inline uint
  handlers()
  {
    return m_handlers;
  }

  //
  // Cursors
  //

  void
  push_cursor(LEX_STRING *name);

  my_bool
  find_cursor(LEX_STRING *name, uint *poff, my_bool scoped=0);

  inline void
  pop_cursor(uint num)
  {
    while (num--)
      pop_dynamic(&m_cursor);
  }

  inline uint
  cursors()
  {
    return m_cursmax;
  }

private:

  uint m_params;		// The number of parameters
  uint m_framesize;		// The maximum framesize
  uint m_handlers;		// The total number of handlers
  uint m_cursmax;		// The maximum number of cursors

  DYNAMIC_ARRAY m_pvar;		// Parameters/variables
  DYNAMIC_ARRAY m_cond;		// Conditions
  DYNAMIC_ARRAY m_cursor;	// Cursors
  DYNAMIC_ARRAY m_scopes;	// For error checking

  List<sp_label_t> m_label;	// The label list

}; // class sp_pcontext : public Sql_alloc


#endif /* _SP_PCONTEXT_H_ */
