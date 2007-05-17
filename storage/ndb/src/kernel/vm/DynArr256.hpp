/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef DYNARR256_HPP
#define DYNARR256_HPP

#include "Pool.hpp"

class DynArr256;
struct DA256Page;

class DynArr256Pool
{
  friend class DynArr256;
public:
  DynArr256Pool();
  
  void init(Uint32 type_id, const Pool_context& pc);
  
protected:
  Uint32 m_type_id;
  Uint32 m_first_free;
  Pool_context m_ctx;
  struct DA256Page* m_memroot;
  
private:
  Uint32 seize();
  void release(Uint32);
};

class DynArr256
{
public:
  struct Head
  {
    Head() { m_ptr_i = RNIL; m_sz = 0;}
    
    Uint32 m_ptr_i;
    Uint32 m_sz;

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
protected:
  Head & m_head;
  DynArr256Pool & m_pool;
  
  bool expand(Uint32 pos);
  void handle_invalid_ptr(Uint32 pos, Uint32 ptrI, Uint32 p0);
};

#endif
