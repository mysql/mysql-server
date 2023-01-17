/*
   Copyright (c) 2006, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef DYNARR256_HPP
#define DYNARR256_HPP

#include "util/require.h"
#include "Pool.hpp"
#include <NdbMutex.h>

#ifdef ERROR_INSERT
#include "SimulatedBlock.hpp" // For cerrorInsert
#endif

#define JAM_FILE_ID 299


class DynArr256;
struct DA256Page;

class DynArr256Pool
{
  friend class DynArr256;
  friend class Dbtup;
public:
  DynArr256Pool();
  
  void init(Uint32 type_id, const Pool_context& pc);
  void init(NdbMutex*, Uint32 type_id, const Pool_context& pc);

  /**
    Memory usage data, used for populating Ndbinfo::pool_entry structures.
   */
  struct Info
  {
    // Number of pages (DA256Page) allocated.
    Uint32 pg_count;
    // Size of each page in bytes.
    Uint32 pg_byte_sz;
    // Number of nodes (DA256Node) in use.
    Uint64 inuse_nodes;
    // Size of each DA256Node in bytes.
    Uint32 node_byte_sz;
    // Number of nodes that fit in a page.
    Uint32 nodes_per_page;
  };
    
  const Info getInfo() const;
               
  Uint32 getUsed()   { return m_used; }  // # entries currently seized
  Uint32 getUsedHi() { return m_usedHi;} // high water mark for getUsed()

protected:
  Uint32 m_type_id;
  Uint32 m_first_free;
  Uint32 m_last_free;
  Pool_context m_ctx;
  struct DA256Page* m_memroot;
  NdbMutex * m_mutex;
  // Number of nodes (DA256Node) in use.
  Uint64 m_inuse_nodes;
  // Number of pages (DA256Page) allocated.
  Uint32 m_pg_count;  
  Uint32 m_used;
  Uint32 m_usedHi;

private:
  Uint32 seize();
  void release(Uint32);
#ifdef ERROR_INSERT
  Uint32 get_ERROR_INSERT_VALUE() const;
#endif
};

class DynArr256
{
public:
  class Head
  {
    friend class DynArr256;
  public:
#if defined VM_TRACE || defined ERROR_INSERT
    Head() { m_ptr_i = RNIL; m_sz = 0; m_no_of_nodes = 0; m_high_pos = 0; }
#else
    Head() { m_ptr_i = RNIL; m_sz = 0; m_no_of_nodes = 0; }
#endif
    ~Head()
    {
      assert(m_sz == 0);
      assert(m_no_of_nodes == 0);
    }

    bool isEmpty() const { return m_sz == 0;}
    
    // Get allocated array size in bytes.
    Uint32 getByteSize() const;

  private:
    Uint32 m_ptr_i;
    Uint32 m_sz;
    // Number of DA256Nodes allocated.
    Int32 m_no_of_nodes;
#if defined VM_TRACE || defined ERROR_INSERT
    Uint32 m_high_pos;
#endif
  };
  
  DynArr256(DynArr256Pool * pool, Head& head) : 
    m_head(head), m_pool(pool){}
  
  Uint32* set(Uint32 pos);
  Uint32* get(Uint32 pos) const ;
  Uint32* get_dirty(Uint32 pos) const ;

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
  DynArr256Pool * m_pool;
  
  bool expand(Uint32 pos);
  void handle_invalid_ptr(Uint32 pos, Uint32 ptrI, Uint32 p0);

private:
#ifdef ERROR_INSERT
  Uint32 get_ERROR_INSERT_VALUE() const;
#endif
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

inline
Uint32 * DynArr256::get(Uint32 pos) const
{
#if defined VM_TRACE || defined ERROR_INSERT
  // In debug this function will abort if used
  // with pos not already mapped by call to set.
  // Use get_dirty if return NULL is wanted instead.
  require((m_head.m_sz > 0) && (pos <= m_head.m_high_pos));
#endif
  return get_dirty(pos);
}

#ifdef ERROR_INSERT
inline
Uint32 DynArr256Pool::get_ERROR_INSERT_VALUE() const
{
  return m_ctx.m_block->cerrorInsert;
}

inline
Uint32 DynArr256::get_ERROR_INSERT_VALUE() const
{
  return m_pool->get_ERROR_INSERT_VALUE();
}
#endif

#undef JAM_FILE_ID

#endif
