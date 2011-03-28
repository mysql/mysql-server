/*
   Copyright (C) 2006 MySQL AB
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

#ifndef NDBD_SUPER_POOL_HPP
#define NDBD_SUPER_POOL_HPP

#include "SuperPool.hpp"

struct AllocArea;

class NdbdSuperPool : public SuperPool
{
public:
  NdbdSuperPool(class Ndbd_mem_manager&, Uint32 pageSize, Uint32 pageBits);
  
  // Destructor.
  virtual ~NdbdSuperPool();
  
  // Get new page from current area.
  virtual PtrI getNewPage();

  // Call first...on all superpools (uses malloc)
  bool init_1(); 
  
  // Call second...uses mm
  bool init_2();
  
  virtual bool allocMemory() { return allocMem() != 0; }
private:
  Uint32 allocAreaMemory(AllocArea*, Uint32 pages);
  AllocArea* allocArea();
  AllocArea* allocMem();
  
  // List of malloc areas.
  Uint32 m_shift, m_add;
  class Ndbd_mem_manager & m_mm;

  AllocArea* m_currArea;
  AllocArea* m_firstArea;
};

#endif
