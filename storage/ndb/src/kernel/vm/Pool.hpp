/* Copyright (C) 2003 MySQL AB

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

#ifndef NDB_POOL_HPP
#define NDB_POOL_HPP

#include <kernel_types.h>

/**
 * Type id is 11 bits record type, and 5 bits resource id
 *   -> 2048 different kind of records and 32 different resource groups
 * 
 * Resource id is used to handle configuration parameters
 *
 * see blocks/records_types.hpp
 */
#define RG_BITS 5
#define RG_MASK ((1 << RG_BITS) - 1)
#define MAKE_TID(TID,RG) ((TID << RG_BITS) | RG)

/**
 * Record_info
 *
 */
struct Record_info
{
  Uint16 m_size;
  Uint16 m_type_id;
  Uint16 m_offset_next_pool;
  Uint16 m_offset_magic;
};

/**
 * Resource_limit
 */
struct Resource_limit
{
  Uint32 m_min;
  Uint32 m_max;
  Uint32 m_curr;
  Uint32 m_resource_id;
};

struct Pool_context
{
  class SimulatedBlock* m_block;
  
  /**
   * Alloc consekutive pages
   *
   *   @param i   : out : i value of first page
   *   @return    : pointer to first page (NULL if failed)
   *
   * Will handle resource limit 
   */
  void* alloc_page(Uint32 type_id, Uint32 *i);
  
  /**
   * Release pages
   * 
   *   @param i   : in : i value of first page
   *   @param p   : in : pointer to first page
   */
  void release_page(Uint32 type_id, Uint32 i, void* p);

  /**
   * Alloc consekutive pages
   *
   *   @param cnt : in/out : no of requested pages, 
   *                return no of allocated (undefined return NULL)
   *                out will never be > in
   *   @param i   : out : i value of first page
   *   @param min : in : will never allocate less than min
   *   @return    : pointer to first page (NULL if failed)
   *
   * Will handle resource limit 
   */
  void* alloc_pages(Uint32 type_id, Uint32 *i, Uint32 *cnt, Uint32 min =1);
  
  /**
   * Release pages
   * 
   *   @param i   : in : i value of first page
   *   @param p   : in : pointer to first page
   *   @param cnt : in : no of pages to release
   */
  void release_pages(Uint32 type_id, Uint32 i, void* p, Uint32 cnt);
  
  /**
   * Pool abort
   *   Only know issue is getPtr with invalid i-value.
   *   If other emerges, we will add argument to this method
   */
  struct AbortArg
  {
    Uint32 m_expected_magic;
    Uint32 m_found_magic;
    Uint32 i;
    void * p;
  };
  void handle_abort(const AbortArg &);
};

template <typename T>
struct Ptr 
{
  T * p;
  Uint32 i;
  inline bool isNull() const { return i == RNIL; }
  inline void setNull() { i = RNIL; }
};

template <typename T>
struct ConstPtr 
{
  const T * p;
  Uint32 i;
  inline bool isNull() const { return i == RNIL; }
  inline void setNull() { i = RNIL; }
};

#ifdef XX_DOCUMENTATION_XX
/**
 * Any pool should implement the following
 */
struct Pool
{
public:
  Pool();
  
  void init(const Record_info& ri, const Pool_context& pc);

  bool seize(Uint32*, void**);
  void* seize(Uint32*);

  void release(Uint32 i, void* p);
  void * getPtr(Uint32 i);
};
#endif

#endif
