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

#ifndef NDB_OBJECT_ID_MAP_HPP
#define NDB_OBJECT_ID_MAP_HPP

#include <ndb_global.h>
//#include <NdbMutex.h>
#include <NdbOut.hpp>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

//#define DEBUG_OBJECTMAP

/**
  * Global ObjectMap
  */
class NdbObjectIdMap //: NdbLockable
{
public:
  STATIC_CONST( InvalidId = ~(Uint32)0 );
  NdbObjectIdMap(NdbMutex*, Uint32 initalSize = 128, Uint32 expandSize = 10);
  ~NdbObjectIdMap();

  Uint32 map(void * object);
  void * unmap(Uint32 id, void *object);
  
  void * getObject(Uint32 id);
private:
  Uint32 m_size;
  Uint32 m_expandSize;
  Uint32 m_firstFree;
  union MapEntry {
     Uint32 m_next;
     void * m_obj;
  } * m_map;

  NdbMutex * m_mutex;
  int expand(Uint32 newSize);
};

inline
Uint32
NdbObjectIdMap::map(void * object){
  
  //  lock();
  
  if(m_firstFree == InvalidId && expand(m_expandSize))
    return InvalidId;
  
  Uint32 ff = m_firstFree;
  m_firstFree = m_map[ff].m_next;
  m_map[ff].m_obj = object;
  
  //  unlock();
  
  DBUG_PRINT("info",("NdbObjectIdMap::map(0x%lx) %u", (long) object, ff<<2));

  return ff<<2;
}

inline
void *
NdbObjectIdMap::unmap(Uint32 id, void *object){

  Uint32 i = id>>2;

  //  lock();
  if(i < m_size){
    void * obj = m_map[i].m_obj;
    if (object == obj) {
      m_map[i].m_next = m_firstFree;
      m_firstFree = i;
    } else {
      g_eventLogger.error("NdbObjectIdMap::unmap(%u, 0x%x) obj=0x%x",
                          id, (long) object, (long) obj);
      DBUG_PRINT("error",("NdbObjectIdMap::unmap(%u, 0x%lx) obj=0x%lx",
                          id, (long) object, (long) obj));
      return 0;
    }
    
    //  unlock();
    
    DBUG_PRINT("info",("NdbObjectIdMap::unmap(%u) obj=0x%lx", id, (long) obj));
    
    return obj;
  }
  return 0;
}

inline void *
NdbObjectIdMap::getObject(Uint32 id){
  // DBUG_PRINT("info",("NdbObjectIdMap::getObject(%u) obj=0x%x", id,  m_map[id>>2].m_obj));
  id >>= 2;
  if(id < m_size){
    return m_map[id].m_obj;
  }
  return 0;
}
#endif
