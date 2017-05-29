/*
   Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "ObjectMap.hpp"

/**
 * GUARD_EXPAND
 *
 * Locking is required to avoid concurrent signal delivery and
 * ObjectMap expansion, which uses realloc and may free/discard
 * entries while delivery is underway.
 *
 * It is assumed that calls to map() or unmap() are serialised
 * by the 'single thread per Ndb object' principle.
 * e.g.
 *   ObjectMap readers : Receiver thread, Client thread
 *   ObjectMap writer  : Client thread
 *
 * For map()/unmap() without expand(), the underlying storage
 * will not be moved or changed except by writing of
 * pointer-sized values.
 */

#define GUARD_EXPAND Guard g(m_mutex)


NdbObjectIdMap::NdbObjectIdMap(Uint32 sz, Uint32 eSz, NdbMutex* mutex):
  m_mutex(mutex),
  m_expandSize(eSz),
  m_size(0),
  m_firstFree(InvalidId),
  m_lastFree(InvalidId),
  m_map(0)
{
  expand(sz);
#ifdef DEBUG_OBJECTMAP
  ndbout_c("NdbObjectIdMap:::NdbObjectIdMap(%u)", sz);
#endif
}

NdbObjectIdMap::~NdbObjectIdMap()
{
  assert(checkConsistency());
  free(m_map);
  m_map = NULL;
}

int NdbObjectIdMap::expand(Uint32 incSize)
{
  GUARD_EXPAND;

  assert(checkConsistency());
  MapEntry* tmp = NULL;
  const Uint32 newSize = m_size + incSize;
#ifdef TEST_MAP_REALLOC
  //DEBUG: Always move into new memory object, shred old.
  tmp = (MapEntry*)malloc(newSize * sizeof(MapEntry));
  if (m_map != NULL)
  {
    memcpy(tmp, m_map, m_size * sizeof(MapEntry));
    memset(m_map, 0x11, m_size * sizeof(MapEntry));
    free(m_map);
  }
#else
  tmp = (MapEntry*)realloc(m_map, newSize * sizeof(MapEntry));
#endif

  if (likely(tmp != 0))
  {
    m_map = tmp;
    
    for(Uint32 i = m_size; i < newSize-1; i++)
    {
      m_map[i].setNext(i+1);
    }
    m_firstFree = m_size;
    m_lastFree = newSize - 1;
    m_map[newSize-1].setNext(InvalidId);
    m_size = newSize;
    assert(checkConsistency());
  }
  else
  {
    g_eventLogger->error("NdbObjectIdMap::expand: realloc(%u*%lu) failed",
                         newSize, (unsigned long) sizeof(MapEntry));
    return -1;
  }
  return 0;
}

bool NdbObjectIdMap::checkConsistency()
{
  if (m_firstFree == InvalidId)
  {
    for (Uint32 i = 0; i<m_size; i++)
    {
      if (m_map[i].isFree())
      {
        assert(false);
        return false;
      }
    }
    return true;
  }

  Uint32 i = m_firstFree;
  while (m_map[i].getNext() != InvalidId)
  {
    i = m_map[i].getNext();
  }
  assert(i == m_lastFree);
  return i == m_lastFree;
}
