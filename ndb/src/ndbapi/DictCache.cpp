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

#include <ndb_global.h>
#include "DictCache.hpp"
#include "NdbDictionaryImpl.hpp"
#include <NdbTick.h>
#include <NdbCondition.h>
#include <NdbSleep.h>

Ndb_local_table_info::Ndb_local_table_info(NdbTableImpl *table_impl, Uint32 sz)
{
  m_table_impl= table_impl;
  if (sz)
    m_local_data= malloc(sz);
  else
    m_local_data= 0;
}

Ndb_local_table_info::~Ndb_local_table_info()
{
  if (m_local_data)
    free(m_local_data);
}

LocalDictCache::LocalDictCache(){
  m_tableHash.createHashTable();
}

LocalDictCache::~LocalDictCache(){
  m_tableHash.releaseHashTable();
}

Ndb_local_table_info * 
LocalDictCache::get(const char * name){
  const Uint32 len = strlen(name);
  return m_tableHash.getData(name, len);
}

void 
LocalDictCache::put(const char * name, Ndb_local_table_info * tab_info){
  const Uint32 id = tab_info->m_table_impl->m_tableId;
  
  m_tableHash.insertKey(name, strlen(name), id, tab_info);
}

void
LocalDictCache::drop(const char * name){
  Ndb_local_table_info *info= m_tableHash.deleteKey(name, strlen(name));
  DBUG_ASSERT(info != 0);
  delete info;
}

/*****************************************************************
 * Global cache
 */
GlobalDictCache::GlobalDictCache(){
  m_tableHash.createHashTable();
  m_waitForTableCondition = NdbCondition_Create();
}

GlobalDictCache::~GlobalDictCache(){
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(0);
  while(curr != 0){
    Vector<TableVersion> * vers = curr->theData;
    const unsigned sz = vers->size();
    for(unsigned i = 0; i<sz ; i++){
      if((* vers)[i].m_impl != 0)
	delete (* vers)[i].m_impl;
    }
    delete curr->theData;
    curr = m_tableHash.getNext(curr);
  }
  
  m_tableHash.releaseHashTable();
  NdbCondition_Destroy(m_waitForTableCondition);
}

#include <NdbOut.hpp>

NdbTableImpl * 
GlobalDictCache::get(const char * name)
{
  const Uint32 len = strlen(name);
  Vector<TableVersion> * versions = 0;  
  versions = m_tableHash.getData(name, len);
  if(versions == 0){
    versions = new Vector<TableVersion>(2);
    m_tableHash.insertKey(name, len, 0, versions);
  }

  int waitTime = 100;

  bool retreive = false;
  while(versions->size() > 0 && !retreive){
    TableVersion * ver = & versions->back();
    switch(ver->m_status){
    case OK:
      ver->m_refCount++;
      return ver->m_impl;
    case DROPPED:
      retreive = true; // Break loop
      break;
    case RETREIVING:
      NdbCondition_WaitTimeout(m_waitForTableCondition, m_mutex, waitTime);
      continue;
    }
  }
  
  /**
   * Create new...
   */
  TableVersion tmp;
  tmp.m_version = 0;
  tmp.m_impl = 0;
  tmp.m_status = RETREIVING;
  tmp.m_refCount = 1; // The one retreiving it
  versions->push_back(tmp);
  return 0;
}

NdbTableImpl *
GlobalDictCache::put(const char * name, NdbTableImpl * tab)
{
  const Uint32 len = strlen(name);
  Vector<TableVersion> * vers = m_tableHash.getData(name, len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }
  
  TableVersion & ver = vers->back();
  if(ver.m_status != RETREIVING || 
     ver.m_impl != 0 || 
     ver.m_version != 0 || 
     ver.m_refCount == 0){
    abort();
  }
  
  if(tab == 0){
    // No table found in db
    vers->erase(sz - 1);
  } else {
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = OK;
  }
  
  NdbCondition_Broadcast(m_waitForTableCondition);
  return tab;
} 

void
GlobalDictCache::drop(NdbTableImpl * tab)
{
  unsigned i;
  const Uint32 len = strlen(tab->m_internalName.c_str());
  Vector<TableVersion> * vers = 
    m_tableHash.getData(tab->m_internalName.c_str(), len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }
  
  for(i = 0; i < sz; i++){
    TableVersion & ver = (* vers)[i];
    if(ver.m_impl == tab){
      if(ver.m_refCount == 0 || ver.m_status == RETREIVING || 
	 ver.m_version != tab->m_version){
	ndbout_c("Dropping with refCount=%d status=%d impl=%p",
		 ver.m_refCount, ver.m_status, ver.m_impl);
	break;
      }
      
      ver.m_refCount--;
      ver.m_status = DROPPED;
      if(ver.m_refCount == 0){
	delete ver.m_impl;
	vers->erase(i);
      }
      return;
    }
  }
  
  for(i = 0; i<sz; i++){
    TableVersion & ver = (* vers)[i];
    ndbout_c("%d: version: %d refCount: %d status: %d impl: %p",
	     i, ver.m_version, ver.m_refCount, ver.m_status, ver.m_impl);
  }
  
  abort();
}

void
GlobalDictCache::release(NdbTableImpl * tab){
  unsigned i;
  const Uint32 len = strlen(tab->m_internalName.c_str());
  Vector<TableVersion> * vers = 
    m_tableHash.getData(tab->m_internalName.c_str(), len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and then there should be a record
    abort(); 
  }
  
  for(i = 0; i < sz; i++){
    TableVersion & ver = (* vers)[i];
    if(ver.m_impl == tab){
      if(ver.m_refCount == 0 || ver.m_status == RETREIVING || 
	 ver.m_version != tab->m_version){
	ndbout_c("Releasing with refCount=%d status=%d impl=%p",
		 ver.m_refCount, ver.m_status, ver.m_impl);
	break;
      }
      
      ver.m_refCount--;
      return;
    }
  }
  
  for(i = 0; i<sz; i++){
    TableVersion & ver = (* vers)[i];
    ndbout_c("%d: version: %d refCount: %d status: %d impl: %p",
	     i, ver.m_version, ver.m_refCount, ver.m_status, ver.m_impl);
  }
  
  abort();
}

template class Vector<GlobalDictCache::TableVersion>;
