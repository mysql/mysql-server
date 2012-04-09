/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DictCache_H
#define DictCache_H

#include <ndb_types.h>
#include <kernel_types.h>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <Vector.hpp>
#include <UtilBuffer.hpp>
#include <NdbDictionary.hpp>
#include <Ndb.hpp>
#include <NdbCondition.h>
#include "NdbLinHash.hpp"

class Ndb_local_table_info {
public:
  static Ndb_local_table_info *create(NdbTableImpl *table_impl, Uint32 sz=0);
  static void destroy(Ndb_local_table_info *);
  NdbTableImpl *m_table_impl;

  // range of cached tuple ids per thread
  Ndb::TupleIdRange m_tuple_id_range;

  Uint64 m_local_data[1]; // Must be last member. Used to access extra space.
private:
  Ndb_local_table_info(NdbTableImpl *table_impl);
  ~Ndb_local_table_info();
};

/**
 * A non thread safe dict cache
 */
class LocalDictCache {
public:
  LocalDictCache();
  ~LocalDictCache();
  
  Ndb_local_table_info * get(const char * name);
  
  void put(const char * name, Ndb_local_table_info *);
  void drop(const char * name);
  
  NdbLinHash<Ndb_local_table_info> m_tableHash; // On name
};

/**
 * A thread safe dict cache
 */
class GlobalDictCache : public NdbLockable {
public:
  GlobalDictCache();
  ~GlobalDictCache();
  
  NdbTableImpl * get(const char * name, int *error);
  
  NdbTableImpl* put(const char * name, NdbTableImpl *);
  void release(const NdbTableImpl *, int invalidate = 0);

  void alter_table_rep(const char * name, 
		       Uint32 tableId, Uint32 tableVersion, bool altered);

  unsigned get_size();
  void invalidate_all();

  // update reference count by +1 or -1
  int inc_ref_count(const NdbTableImpl * impl) {
    return chg_ref_count(impl, +1);
  }
  int dec_ref_count(const NdbTableImpl * impl) {
    return chg_ref_count(impl, -1);
  }

  void invalidateDb(const char * name, size_t len);

public:
  enum Status {
    OK = 0,
    DROPPED = 1,
    RETREIVING = 2
  };
  
private:
  void printCache();
  int chg_ref_count(const NdbTableImpl *, int value);

  struct TableVersion {
    Uint32 m_version;
    Uint32 m_refCount;
    NdbTableImpl * m_impl;
    Status m_status;
  };
  
  NdbLinHash<Vector<TableVersion> > m_tableHash;
  NdbCondition * m_waitForTableCondition;
};

#endif


