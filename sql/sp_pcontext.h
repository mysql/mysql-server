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

#ifdef USE_PRAGMA_INTERFACE
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


#define SP_LAB_REF   0		// Unresolved reference (for goto)
#define SP_LAB_GOTO  1		// Free goto label
#define SP_LAB_BEGIN 2		// Label at BEGIN
#define SP_LAB_ITER  3		// Label at iteration control

typedef struct sp_label
{
  char *name;
  uint ip;			// Instruction index
  int type;			// begin/iter or ref/free 
  sp_pcontext *ctx;             // The label's context
} sp_label_t;

typedef struct sp_cond_type
{
  enum { number, state, warning, notfound, exception } type;
  char sqlstate[6];
  uint mysqlerr;
} sp_cond_type_t;

/* Sanity check for SQLSTATEs. Will not check if it's really an existing
 * state (there are just too many), but will check length bad characters.
 */
extern bool
sp_cond_check(LEX_STRING *sqlstate);

typedef struct sp_cond
{
  LEX_STRING name;
  sp_cond_type_t *val;
} sp_cond_t;


/*
  This seems to be an "SP parsing context" or something.
*/

class sp_pcontext : public Sql_alloc
{
  sp_pcontext(const sp_pcontext &); /* Prevent use of these */
  void operator=(sp_pcontext &);

 public:

  sp_pcontext(sp_pcontext *prev);

  // Free memory
  void
  destroy();

  sp_pcontext *
  push_context();

  // Returns the previous context, not the one we pop
  sp_pcontext *
  pop_context();

  sp_pcontext *
  parent_context()
  {
    return m_parent;
  }

  uint
  diff_handlers(sp_pcontext *ctx);

  uint
  diff_cursors(sp_pcontext *ctx);


  //
  // Parameters and variables
  //

  inline uint
  max_pvars()
  {
    return m_psubsize + m_pvar.elements;
  }

  inline uint
  current_pvars()
  {
    return m_poffset + m_pvar.elements;
  }

  inline uint
  context_pvars()
  {
    return m_pvar.elements;
  }

  inline uint
  pvar_context2index(uint i)
  {
    return m_poffset + i;
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
    sp_label_t *lab= m_label.head();

    if (!lab && m_parent)
      lab= m_parent->last_label();
    return lab;
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
  push_handler(sp_cond_type_t *cond)
  {
    insert_dynamic(&m_handler, (gptr)&cond);
  }

  bool
  find_handler(sp_cond_type *cond);

  inline uint
  max_handlers()
  {
    return m_hsubsize + m_handlers;
  }

  inline void
  add_handlers(uint n)
  {
    m_handlers+= n;
  }

  //
  // Cursors
  //

  void
  push_cursor(LEX_STRING *name);

  my_bool
  find_cursor(LEX_STRING *name, uint *poff, my_bool scoped=0);

  inline uint
  max_cursors()
  {
    return m_csubsize + m_cursor.elements;
  }

  inline uint
  current_cursors()
  {
    return m_coffset + m_cursor.elements;
  }

protected:

  // The maximum sub context's framesizes
  uint m_psubsize;		
  uint m_csubsize;
  uint m_hsubsize;
  uint m_handlers;		// No. of handlers in this context

private:

  sp_pcontext *m_parent;	// Parent context

  uint m_poffset;		// Variable offset for this context
  uint m_coffset;		// Cursor offset for this context

  DYNAMIC_ARRAY m_pvar;		// Parameters/variables
  DYNAMIC_ARRAY m_cond;		// Conditions
  DYNAMIC_ARRAY m_cursor;	// Cursors
  DYNAMIC_ARRAY m_handler;	// Handlers, for checking of duplicates

  List<sp_label_t> m_label;	// The label list

  List<sp_pcontext> m_children;	// Children contexts, used for destruction

}; // class sp_pcontext : public Sql_alloc


#endif /* _SP_PCONTEXT_H_ */
