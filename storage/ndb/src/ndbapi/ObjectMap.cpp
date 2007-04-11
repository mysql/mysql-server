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

#include "ObjectMap.hpp"

NdbObjectIdMap::NdbObjectIdMap(NdbMutex* mutex, Uint32 sz, Uint32 eSz)
{
  m_size = 0;
  m_firstFree = InvalidId;
  m_map = 0;
  m_mutex = mutex;
  m_expandSize = eSz;
  expand(sz);
#ifdef DEBUG_OBJECTMAP
  ndbout_c("NdbObjectIdMap:::NdbObjectIdMap(%u)", sz);
#endif
}

NdbObjectIdMap::~NdbObjectIdMap()
{
  free(m_map);
}

int NdbObjectIdMap::expand(Uint32 incSize)
{
  NdbMutex_Lock(m_mutex);
  Uint32 newSize = m_size + incSize;
  MapEntry * tmp = (MapEntry*)realloc(m_map, newSize * sizeof(MapEntry));

  if (likely(tmp != 0))
  {
    m_map = tmp;
    
    for(Uint32 i = m_size; i < newSize; i++){
      m_map[i].m_next = i + 1;
    }
    m_firstFree = m_size;
    m_map[newSize-1].m_next = InvalidId;
    m_size = newSize;
  }
  else
  {
    NdbMutex_Unlock(m_mutex);
    g_eventLogger.error("NdbObjectIdMap::expand: realloc(%u*%u) failed",
                        newSize, sizeof(MapEntry));
    return -1;
  }
  NdbMutex_Unlock(m_mutex);
  return 0;
}
