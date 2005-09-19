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

#ifndef NDB_OBJECT_ID_MAP_HPP
#define NDB_OBJECT_ID_MAP_HPP

#include <ndb_global.h>
//#include <NdbMutex.h>
#include <NdbOut.hpp>

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
  void expand(Uint32 newSize);
};

inline
NdbObjectIdMap::NdbObjectIdMap(NdbMutex* mutex, Uint32 sz, Uint32 eSz) {
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

inline
NdbObjectIdMap::~NdbObjectIdMap(){
  free(m_map);
}

inline
Uint32
NdbObjectIdMap::map(void * object){
  
  //  lock();
  
  if(m_firstFree == InvalidId){
    expand(m_expandSize);
  }
  
  Uint32 ff = m_firstFree;
  m_firstFree = m_map[ff].m_next;
  m_map[ff].m_obj = object;
  
  //  unlock();
  
#ifdef DEBUG_OBJECTMAP
  ndbout_c("NdbObjectIdMap::map(0x%x) %u", object, ff<<2);
#endif

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
      ndbout_c("Error: NdbObjectIdMap::::unmap(%u, 0x%x) obj=0x%x", id, object, obj);
      return 0;
    }
    
    //  unlock();
    
#ifdef DEBUG_OBJECTMAP
    ndbout_c("NdbObjectIdMap::unmap(%u) obj=0x%x", id, obj);
#endif
    
    return obj;
  }
  return 0;
}

inline void *
NdbObjectIdMap::getObject(Uint32 id){
#ifdef DEBUG_OBJECTMAP
  ndbout_c("NdbObjectIdMap::getObject(%u) obj=0x%x", id,  m_map[id>>2].m_obj);
#endif
  id >>= 2;
  if(id < m_size){
    return m_map[id].m_obj;
  }
  return 0;
}

inline void
NdbObjectIdMap::expand(Uint32 incSize){
  NdbMutex_Lock(m_mutex);
  Uint32 newSize = m_size + incSize;
  MapEntry * tmp = (MapEntry*)realloc(m_map, newSize * sizeof(MapEntry));

  if (likely(tmp != 0))
  {
    m_map = tmp;
    
    for(Uint32 i = m_size; i<newSize; i++){
      m_map[i].m_next = i + 1;
    }
    m_firstFree = m_size;
    m_map[newSize-1].m_next = InvalidId;
    m_size = newSize;
  }
  else
  {
    ndbout_c("NdbObjectIdMap::expand unable to expand!!");
  }
  NdbMutex_Unlock(m_mutex);
}

#endif
