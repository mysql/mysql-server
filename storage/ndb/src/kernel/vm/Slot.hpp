/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef SLOT_HPP
#define SLOT_HPP

#include "blocks/record_types.hpp"
#include "ndb_global.h"
#include "ndb_limits.h"
#include "ndb_types.h"
#include "vm/Pool.hpp"

#define JAM_FILE_ID 507

// Slot must have standard layout

class Slot
{
 public:
  STATIC_CONST(TYPE_ID = RT_FREE);
  Slot();
  Uint32 m_magic;
  Uint32 nextList;
  Uint32 prevList;
};

template <typename SlotPool>
class LocalSlotPool
{
 public:
  typedef Slot Type;

  LocalSlotPool(SlotPool* pool, Uint32 slot_size);
  void getPtr(Ptr<Slot>& ptr) const;

 private:
  SlotPool* const m_pool;
  const Uint32 m_slot_size;
};

inline Slot::Slot()
    : m_magic(Magic::make(TYPE_ID)), nextList(RNIL), prevList(RNIL)
{
}

template <typename P>
inline LocalSlotPool<P>::LocalSlotPool(P* pool, Uint32 slot_size)
    : m_pool(pool), m_slot_size(slot_size)
{
}

template <typename P>
inline void LocalSlotPool<P>::getPtr(Ptr<Slot>& ptr) const
{
  m_pool->getPtr(ptr, m_slot_size);
}

#undef JAM_FILE_ID

#endif
