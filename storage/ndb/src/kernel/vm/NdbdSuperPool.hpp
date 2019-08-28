/*
   Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBD_SUPER_POOL_HPP
#define NDBD_SUPER_POOL_HPP

#include "SuperPool.hpp"

#define JAM_FILE_ID 306


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


#undef JAM_FILE_ID

#endif
