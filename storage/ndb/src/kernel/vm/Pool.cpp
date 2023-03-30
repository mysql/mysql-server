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


#include "ndbd_malloc_impl.hpp"
#include "Pool.hpp"
#include "SimulatedBlock.hpp"

#define JAM_FILE_ID 255


void*
Pool_context::alloc_page19(Uint32 type_id, Uint32 *i)
{
  return m_block->m_ctx.m_mm.alloc_page(type_id,
                                        i,
                                        Ndbd_mem_manager::NDB_ZONE_LE_19);
}

void*
Pool_context::alloc_page27(Uint32 type_id, Uint32 *i)
{
  return m_block->m_ctx.m_mm.alloc_page(type_id,
                                        i,
                                        Ndbd_mem_manager::NDB_ZONE_LE_27);
}

void*
Pool_context::alloc_page30(Uint32 type_id, Uint32 *i)
{
  return m_block->m_ctx.m_mm.alloc_page(type_id,
                                        i,
                                        Ndbd_mem_manager::NDB_ZONE_LE_30);
}

void*
Pool_context::alloc_page32(Uint32 type_id, Uint32 *i)
{
  return m_block->m_ctx.m_mm.alloc_page(type_id,
                                        i,
                                        Ndbd_mem_manager::NDB_ZONE_LE_32);
}

void 
Pool_context::release_page(Uint32 type_id, Uint32 i)
{
  m_block->m_ctx.m_mm.release_page(type_id, i);
}

Ndbd_mem_manager*
Pool_context::get_mem_manager() const
{
  return &m_block->m_ctx.m_mm;
}

void*
Pool_context::get_memroot() const
{
  return m_block->m_ctx.m_mm.get_memroot();
}

void*
Pool_context::get_valid_page(Uint32 page_num) const
{
  return m_block->m_ctx.m_mm.get_valid_page(page_num);
}

void
Pool_context::handleAbort(int err, const char * msg) const
{
  m_block->progError(__LINE__, err, msg);
}
