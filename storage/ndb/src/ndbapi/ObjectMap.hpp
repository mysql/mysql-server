/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_OBJECT_ID_MAP_HPP
#define NDB_OBJECT_ID_MAP_HPP

#include <ndb_global.h>
#include <NdbOut.hpp>

#include <NdbMutex.h>
#include <EventLogger.hpp>

// #define DEBUG_OBJECTMAP

/**
 * Global ObjectMap
 */
class NdbObjectIdMap {
 public:
  static constexpr Uint32 InvalidId = 0x7fffffff;

 private:
  /**
   * class NdbObjectIdMap is only intended to be used through
   * class NdbImpl.
   */
  friend class NdbImpl;

  NdbObjectIdMap(Uint32 initialSize, Uint32 expandSize, NdbMutex *mutex);
  ~NdbObjectIdMap();

  Uint32 map(void *object);
  void *unmap(Uint32 id, void *object);

  void *getObject(Uint32 id) const;

  // mutex belonging to the NdbImpl object that owns this object map
  NdbMutex *m_mutex;
  const Uint32 m_expandSize;
  Uint32 m_size;
  Uint32 m_firstFree;
  /**
   * We put released entries at the end of the free list. That way, we delay
   * re-use of an object id as long as possible. This minimizes the chance
   * of sending an incoming message to the wrong object because the recipient
   * object id was reused.
   */
  Uint32 m_lastFree;

  class MapEntry {
   public:
    bool isFree() const { return (m_val & 1) == 1; }

    Uint32 getNext() const {
      assert(isFree());
      return static_cast<Uint32>(m_val >> 1);
    }

    void setNext(Uint32 next) { m_val = (next << 1) | 1; }

    void *getObj() const {
      assert((m_val & 3) == 0);
      return reinterpret_cast<void *>(m_val);
    }

    void setObj(void *obj) {
      m_val = reinterpret_cast<UintPtr>(obj);
      assert((m_val & 3) == 0);
    }

   private:
    /**
     * This holds either a pointer to a mapped object *or* the index of the
     * next entry in the free list. If it is a pointer, then the two least
     * significant bits should be zero (requiring all mapped objects to be
     * four-byte aligned). If it is an index, then bit 0 should be set.
     */
    UintPtr m_val;
  };

  MapEntry *m_map;

  int expand(Uint32 newSize);
  // For debugging purposes.
  bool checkConsistency();
};

inline Uint32 NdbObjectIdMap::map(void *object) {
  if (m_firstFree == InvalidId) {
    if (expand(m_expandSize) != 0) return InvalidId;
  }

  const Uint32 ff = m_firstFree;
  m_firstFree = m_map[ff].getNext();
  m_map[ff].setObj(object);

  DBUG_PRINT("info", ("NdbObjectIdMap::map(%p) %u", object, ff << 2));

  return ff << 2;
}

inline void *NdbObjectIdMap::unmap(Uint32 id, void *object) {
  const Uint32 i = id >> 2;

  assert(i < m_size);
  if (i < m_size) {
    void *const obj = m_map[i].getObj();
    if (object == obj) {
      m_map[i].setNext(InvalidId);
      if (m_firstFree == InvalidId) {
        m_firstFree = i;
      } else {
        m_map[m_lastFree].setNext(i);
      }
      m_lastFree = i;
    } else {
      g_eventLogger->error("NdbObjectIdMap::unmap(%u, %p) obj=%p", id, object,
                           obj);
      DBUG_PRINT("error",
                 ("NdbObjectIdMap::unmap(%u, %p) obj=%p", id, object, obj));
      assert(false);
      return nullptr;
    }

    DBUG_PRINT("info", ("NdbObjectIdMap::unmap(%u) obj=%p", id, obj));

    return obj;
  }
  return nullptr;
}

inline void *NdbObjectIdMap::getObject(Uint32 id) const {
  // DBUG_PRINT("info",("NdbObjectIdMap::getObject(%u) obj=0x%x", id,
  // m_map[id>>2].m_obj));
  id >>= 2;
  if (id < m_size) {
    if (m_map[id].isFree()) {
      return nullptr;
    } else {
      return m_map[id].getObj();
    }
  }
  return nullptr;
}
#endif
