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

class sp_rcontext : public Sql_alloc
{
  sp_rcontext(const sp_rcontext &); /* Prevent use of these */
  void operator=(sp_rcontext &);

 public:

  sp_rcontext(uint size)
    : m_count(0), m_size(size), m_result(NULL)
  {
    m_frame = (Item **)sql_alloc(size * sizeof(Item*));
    m_outs = (int *)sql_alloc(size * sizeof(int));
  }

  ~sp_rcontext()
  {
    // Not needed?
    //sql_element_free(m_frame);
  }

  inline void
  push_item(Item *i)
  {
    if (m_count < m_size)
      m_frame[m_count++] = i;
  }

  inline void
  set_item(uint idx, Item *i)
  {
    if (idx < m_count)
      m_frame[idx] = i;
  }

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

private:

  uint m_count;
  uint m_size;
  Item **m_frame;
  int  *m_outs;
  Item *m_result;		// For FUNCTIONs

}; // class sp_rcontext : public Sql_alloc

#endif /* _SP_RCONTEXT_H_ */
