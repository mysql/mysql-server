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

#ifndef _SP_RCONTEXT_H_
#define _SP_RCONTEXT_H_

#ifdef __GNUC__
#pragma interface			/* gcc class implementation */
#endif

struct sp_cond_type;
class sp_cursor;
struct sp_pvar;

#define SP_HANDLER_NONE      0
#define SP_HANDLER_EXIT      1
#define SP_HANDLER_CONTINUE  2
#define SP_HANDLER_UNDO      3

typedef struct
{
  struct sp_cond_type *cond;
  uint handler;			// Location of handler
  int type;
  uint foffset;			// Frame offset for the handlers declare level
} sp_handler_t;

class sp_rcontext : public Sql_alloc
{
  sp_rcontext(const sp_rcontext &); /* Prevent use of these */
  void operator=(sp_rcontext &);

 public:

  bool in_handler;

  sp_rcontext(uint fsize, uint hmax, uint cmax);

  ~sp_rcontext()
  {
    // Not needed?
    //sql_element_free(m_frame);
    //m_saved.empty();
  }

  inline void
  push_item(Item *i)
  {
    if (m_count < m_fsize)
      m_frame[m_count++] = i;
  }

  inline void
  set_item(uint idx, Item *i)
  {
    if (idx < m_count)
      m_frame[idx] = i;
  }

  /* Returns 0 on success, -1 on (eval) failure */
  int
  set_item_eval(uint idx, Item *i, enum_field_types type);

  inline Item *
  get_item(uint idx)
  {
    return m_frame[idx];
  }

  inline void
  set_oindex(uint idx, int oidx)
  {
    m_outs[idx] = oidx;
  }

  inline int
  get_oindex(uint idx)
  {
    return m_outs[idx];
  }

  inline void
  set_result(Item *it)
  {
    m_result= it;
  }

  inline Item *
  get_result()
  {
    return m_result;
  }

  inline void
  push_handler(struct sp_cond_type *cond, uint h, int type, uint f)
  {
    m_handler[m_hcount].cond= cond;
    m_handler[m_hcount].handler= h;
    m_handler[m_hcount].type= type;
    m_handler[m_hcount].foffset= f;
    m_hcount+= 1;
  }

  inline void
  pop_handlers(uint count)
  {
    m_hcount-= count;
  }

  // Returns 1 if a handler was found, 0 otherwise.
  int
  find_handler(uint sql_errno);

  // Returns handler type and sets *ip to location if one was found
  inline int
  found_handler(uint *ip, uint *fp)
  {
    if (m_hfound < 0)
      return SP_HANDLER_NONE;
    *ip= m_handler[m_hfound].handler;
    *fp= m_handler[m_hfound].foffset;
    return m_handler[m_hfound].type;
  }

  // Clears the handler find state
  inline void
  clear_handler()
  {
    m_hfound= -1;
  }

  inline void
  push_hstack(uint h)
  {
    m_hstack[m_hsp++]= h;
  }

  inline uint
  pop_hstack()
  {
    return m_hstack[--m_hsp];
  }

  // Save variables starting at fp and up
  void
  save_variables(uint fp);

  // Restore variables down to fp
  void
  restore_variables(uint fp);

  void
  push_cursor(LEX *lex);

  void
  pop_cursors(uint count);

  void
  pop_all_cursors()
  {
    pop_cursors(m_ccount);
  }

  inline sp_cursor *
  get_cursor(uint i)
  {
    return m_cstack[i];
  }

private:

  uint m_count;
  uint m_fsize;
  Item **m_frame;
  int  *m_outs;

  Item *m_result;		// For FUNCTIONs

  sp_handler_t *m_handler;
  uint m_hcount;
  uint *m_hstack;
  uint m_hsp;
  int m_hfound;			// Set by find_handler; -1 if not found
  List<Item> m_saved;		// Saved variables

  sp_cursor **m_cstack;
  uint m_ccount;

}; // class sp_rcontext : public Sql_alloc


class sp_cursor : public Sql_alloc
{
public:

  sp_cursor(LEX *lex)
    : m_lex(lex), m_prot(NULL), m_isopen(0), m_current_row(NULL)
  {
    /* Empty */
  }

  virtual ~sp_cursor()
  {
    destroy();
  }

  // We have split this in two to make it easy for sp_instr_copen
  // to reuse the sp_instr::exec_stmt() code.
  LEX *
  pre_open(THD *thd);
  void
  post_open(THD *thd, my_bool was_opened);

  int
  close(THD *thd);

  inline my_bool
  is_open()
  {
    return m_isopen;
  }

  int
  fetch(THD *, List<struct sp_pvar> *vars);

private:

  MEM_ROOT m_mem_root;		// My own mem_root
  LEX *m_lex;
  Protocol_cursor *m_prot;
  my_bool m_isopen;
  my_bool m_nseof;		// Original no_send_eof
  Protocol *m_oprot;		// Original protcol
  MYSQL_ROWS *m_current_row;
  
  void
  destroy();

}; // class sp_cursor : public Sql_alloc

#endif /* _SP_RCONTEXT_H_ */
