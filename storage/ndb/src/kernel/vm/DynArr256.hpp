/*
   Copyright (C) 2006, 2007 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DYNARR256_HPP
#define DYNARR256_HPP

#include "Pool.hpp"
#include <NdbMutex.h>

class DynArr256;
struct DA256Page;

class DynArr256Pool
{
  friend class DynArr256;
public:
  DynArr256Pool();
  
  void init(Uint32 type_id, const Pool_context& pc);
  void init(NdbMutex*, Uint32 type_id, const Pool_context& pc);
  
protected:
  Uint32 m_type_id;
  Uint32 m_first_free;
  Uint32 m_last_free;
  Pool_context m_ctx;
  struct DA256Page* m_memroot;
  NdbMutex * m_mutex;
  
private:
  Uint32 seize();
  void release(Uint32);
};

class DynArr256
{
public:
  struct Head
  {
#ifdef VM_TRACE
    Head() { m_ptr_i = RNIL; m_sz = 0; m_high_pos = 0; }
#else
    Head() { m_ptr_i = RNIL; m_sz = 0;}
#endif
    
    Uint32 m_ptr_i;
    Uint32 m_sz;
#ifdef VM_TRACE
    Uint32 m_high_pos;
#endif

    bool isEmpty() const { return m_sz == 0;}
  };
  
  DynArr256(DynArr256Pool & pool, Head& head) : 
    m_head(head), m_pool(pool){}
  
  Uint32* set(Uint32 pos);
  Uint32* get(Uint32 pos) const ;
  
  struct ReleaseIterator
  {
    Uint32 m_sz;
    Uint32 m_pos;
    Uint32 m_ptr_i[5];
  };
  
  void init(ReleaseIterator&);
  /**
   * return 0 - done
   *        1 - data (in retptr)
   *        2 - nodata
   */
  Uint32 release(ReleaseIterator&, Uint32* retptr);
  Uint32 trim(Uint32 trim_pos, ReleaseIterator&);
  Uint32 truncate(Uint32 trunc_pos, ReleaseIterator&, Uint32* retptr);
protected:
  Head & m_head;
  DynArr256Pool & m_pool;
  
  bool expand(Uint32 pos);
  void handle_invalid_ptr(Uint32 pos, Uint32 ptrI, Uint32 p0);
};

inline
Uint32 DynArr256::release(ReleaseIterator& iter, Uint32* retptr)
{
  return truncate(0, iter, retptr);
}

inline
Uint32 DynArr256::trim(Uint32 pos, ReleaseIterator& iter)
{
  return truncate(pos, iter, NULL);
}

#endif
