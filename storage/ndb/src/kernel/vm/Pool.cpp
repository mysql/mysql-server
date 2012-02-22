/*
   Copyright (C) 2006, 2008 MySQL AB
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


#include "Pool.hpp"
#include "SimulatedBlock.hpp"

void*
Pool_context::alloc_page(Uint32 type_id, Uint32 *i)
{
  return m_block->m_ctx.m_mm.alloc_page(type_id, i,
                                        Ndbd_mem_manager::NDB_ZONE_LO);
}
  
void 
Pool_context::release_page(Uint32 type_id, Uint32 i)
{
  m_block->m_ctx.m_mm.release_page(type_id, i);
}

void*
Pool_context::get_memroot() const
{
  return m_block->m_ctx.m_mm.get_memroot();
}

void
Pool_context::handleAbort(int err, const char * msg) const
{
  m_block->progError(__LINE__, err, msg);
}
