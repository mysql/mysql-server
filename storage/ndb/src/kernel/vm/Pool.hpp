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

struct Record_info
{
  Uint32 m_size;
  Uint32 m_offset_next_pool;
  Uint32 m_type_id;
};

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
  struct Resource_limit* m_resource_limit;
};

#endif
