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
  
  /*
    offset -- basically, this is an index of variable in the scope of root
    parsing context. This means, that all variables in a stored routine
    have distinct indexes/offsets.
  */
  uint offset;

  Item *dflt;
  create_field field_def;
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
  total_pvars()
  {
    return m_total_pvars;
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
  set_default(uint i, Item *it)
  {
    sp_pvar_t *p= find_pvar(i);

    if (p)
      p->dflt= it;
  }

  sp_pvar_t *
  push_pvar(LEX_STRING *name, enum enum_field_types type, sp_param_mode_t mode);

  /*
    Retrieve definitions of fields from the current context and its
    children.
  */
  void
  retrieve_field_definitions(List<create_field> *field_def_lst);

  // Find by name
  sp_pvar_t *
  find_pvar(LEX_STRING *name, my_bool scoped=0);

  // Find by offset
  sp_pvar_t *
  find_pvar(uint offset);

  /*
    Set the current scope boundary (for default values).
    The argument is the number of variables to skip.   
  */
  inline void
  declare_var_boundary(uint n)
  {
    m_pboundary= n;
  }

  /*
    CASE expressions support.
  */

  inline int
  register_case_expr()
  {
    return m_num_case_exprs++;
  }

  inline int
  get_num_case_exprs() const
  {
    return m_num_case_exprs;
  }

  inline bool
  push_case_expr_id(int case_expr_id)
  {
    return insert_dynamic(&m_case_expr_id_lst, (gptr) &case_expr_id);
  }

  inline void
  pop_case_expr_id()
  {
    pop_dynamic(&m_case_expr_id_lst);
  }

  inline int
  get_current_case_expr_id() const
  {
    int case_expr_id;

    get_dynamic((DYNAMIC_ARRAY*)&m_case_expr_id_lst, (gptr) &case_expr_id,
                m_case_expr_id_lst.elements - 1);

    return case_expr_id;
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

  /* Find by offset (for debugging only) */
  my_bool
  find_cursor(uint offset, LEX_STRING *n);

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

  /*
    m_total_pvars -- number of variables (including all types of arguments)
    in this context including all children contexts.
    
    m_total_pvars >= m_pvar.elements.

    m_total_pvars of the root parsing context contains number of all
    variables (including arguments) in all enclosed contexts.
  */
  uint m_total_pvars;		

  // The maximum sub context's framesizes
  uint m_csubsize;
  uint m_hsubsize;
  uint m_handlers;		// No. of handlers in this context

private:

  sp_pcontext *m_parent;	// Parent context

  /*
    m_poffset -- basically, this is an index of the first variable in this
    parsing context.
    
    m_poffset is 0 for root context.

    Since now each variable is stored in separate place, no reuse is done,
    so m_poffset is different for all enclosed contexts.
  */
  uint m_poffset;

  uint m_coffset;		// Cursor offset for this context

  /*
    Boundary for finding variables in this context. This is the number
    of variables currently "invisible" to default clauses.
    This is normally 0, but will be larger during parsing of
    DECLARE ... DEFAULT, to get the scope right for DEFAULT values.
  */
  uint m_pboundary;

  int m_num_case_exprs;

  DYNAMIC_ARRAY m_pvar;		// Parameters/variables
  DYNAMIC_ARRAY m_case_expr_id_lst; /* Stack of CASE expression ids. */
  DYNAMIC_ARRAY m_cond;		// Conditions
  DYNAMIC_ARRAY m_cursor;	// Cursors
  DYNAMIC_ARRAY m_handler;	// Handlers, for checking of duplicates

  List<sp_label_t> m_label;	// The label list

  List<sp_pcontext> m_children;	// Children contexts, used for destruction

}; // class sp_pcontext : public Sql_alloc


#endif /* _SP_PCONTEXT_H_ */
