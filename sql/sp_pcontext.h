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

typedef struct
{
  Item_string *name;
  enum enum_field_types type;
  sp_param_mode_t mode;
  uint offset;			// Offset in current frame
  my_bool isset;
} sp_pvar_t;

typedef struct sp_label
{
  char *name;
  uint ip;			// Instruction index
} sp_label_t;

class sp_pcontext : public Sql_alloc
{
  sp_pcontext(const sp_pcontext &); /* Prevent use of these */
  void operator=(sp_pcontext &);

 public:

  sp_pcontext();

  inline uint
  max_framesize()
  {
    return m_framesize;
  }

  inline uint
  current_framesize()
  {
    return m_i;
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
    m_params= m_i;
  }

  inline void
  set_type(uint i, enum enum_field_types type)
  {
    if (i < m_i)
      m_pvar[i].type= type;
  }

  inline void
  set_isset(uint i, my_bool val)
  {
    if (i < m_i)
      m_pvar[i].isset= val;
  }

  void
  push(LEX_STRING *name, enum enum_field_types type, sp_param_mode_t mode);

  // Pop the last 'num' slots of the frame
  inline void
  pop(uint num = 1)
  {
    if (num < m_i)
      m_i -= num;
  }

  // Find by name
  sp_pvar_t *
  find_pvar(LEX_STRING *name);

  // Find by index
  sp_pvar_t *
  find_pvar(uint i)
  {
    if (i >= m_i)
      return NULL;
    return m_pvar+i;
  }

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

private:

  uint m_params;		// The number of parameters
  uint m_framesize;		// The maximum framesize
  uint m_i;			// The current index (during parsing)

  sp_pvar_t *m_pvar;
  uint m_pvar_size;		// Current size of m_pvar.

  void
  grow();

  List<sp_label_t> m_label;	// The label list
  uint m_genlab;		// Gen. label counter

}; // class sp_pcontext : public Sql_alloc


#endif /* _SP_PCONTEXT_H_ */
