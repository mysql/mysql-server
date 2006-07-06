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

static NdbTableImpl f_invalid_table;
static NdbTableImpl f_altered_table;

Ndb_local_table_info *
Ndb_local_table_info::create(NdbTableImpl *table_impl, Uint32 sz)
{
  Uint32 tot_size= sizeof(Ndb_local_table_info) - sizeof(Uint64)
    + ((sz+7) & ~7); // round to Uint64
  void *data= malloc(tot_size);
  if (data == 0)
    return 0;
  memset(data, 0, tot_size);
  new (data) Ndb_local_table_info(table_impl);
  return (Ndb_local_table_info *) data;
}

void Ndb_local_table_info::destroy(Ndb_local_table_info *info)
{
  free((void *)info);
}

Ndb_local_table_info::Ndb_local_table_info(NdbTableImpl *table_impl)
{
  m_table_impl= table_impl;
  m_first_tuple_id = ~(Uint64)0;
  m_last_tuple_id = ~(Uint64)0;
}

Ndb_local_table_info::~Ndb_local_table_info()
{
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
  Ndb_local_table_info::destroy(info);
}

/*****************************************************************
 * Global cache
 */
GlobalDictCache::GlobalDictCache(){
  DBUG_ENTER("GlobalDictCache::GlobalDictCache");
  m_tableHash.createHashTable();
  m_waitForTableCondition = NdbCondition_Create();
  DBUG_VOID_RETURN;
}

GlobalDictCache::~GlobalDictCache(){
  DBUG_ENTER("GlobalDictCache::~GlobalDictCache");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(0);
  while(curr != 0){
    Vector<TableVersion> * vers = curr->theData;
    const unsigned sz = vers->size();
    for(unsigned i = 0; i<sz ; i++){
      if((* vers)[i].m_impl != 0)
	delete (* vers)[i].m_impl;
    }
    delete curr->theData;
    curr->theData= NULL;
    curr = m_tableHash.getNext(curr);
  }
  m_tableHash.releaseHashTable();
  NdbCondition_Destroy(m_waitForTableCondition);
  DBUG_VOID_RETURN;
}

void GlobalDictCache::printCache()
{
  DBUG_ENTER("GlobalDictCache::printCache");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(0);
  while(curr != 0){
    DBUG_PRINT("curr", ("len: %d, hash: %d, lk: %d, str: %s",
                        curr->len, curr->hash, curr->localkey1, curr->str));
    if (curr->theData){
      Vector<TableVersion> * vers = curr->theData;
      const unsigned sz = vers->size();
      for(unsigned i = 0; i<sz ; i++){
        TableVersion tv= (*vers)[i];
        DBUG_PRINT("  ", ("vers[%d]: ver: %d, refCount: %d, status: %d",
                          sz, tv.m_version, tv.m_refCount, tv.m_status));
        if(tv.m_impl != 0)
        {
          DBUG_PRINT("  ", ("m_impl: internalname: %s",
                            tv.m_impl->m_internalName.c_str()));
        }
      }
    }
    else
    {
      DBUG_PRINT("  ", ("NULL"));
    }
    curr = m_tableHash.getNext(curr);
  }
  DBUG_VOID_RETURN;
}

NdbTableImpl *
GlobalDictCache::get(const char * name)
{
  DBUG_ENTER("GlobalDictCache::get");
  DBUG_PRINT("enter", ("name: %s", name));

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
      DBUG_RETURN(ver->m_impl);
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
  DBUG_RETURN(0);
}

NdbTableImpl *
GlobalDictCache::put(const char * name, NdbTableImpl * tab)
{
  DBUG_ENTER("GlobalDictCache::put");
  DBUG_PRINT("enter", ("name: %s, internal_name: %s",
                       name, tab ? tab->m_internalName.c_str() : "tab NULL"));

  const Uint32 len = strlen(name);
  Vector<TableVersion> * vers = m_tableHash.getData(name, len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }
  
  TableVersion & ver = vers->back();
  if(ver.m_status != RETREIVING || 
     !(ver.m_impl == 0 || 
       ver.m_impl == &f_invalid_table || ver.m_impl == &f_altered_table) || 
     ver.m_version != 0 || 
     ver.m_refCount == 0){
    abort();
  }
  
  if(tab == 0)
  {
    DBUG_PRINT("info", ("No table found in db"));
    vers->erase(sz - 1);
  } 
  else if (ver.m_impl == 0) {
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = OK;
  } 
  else if (ver.m_impl == &f_invalid_table) 
  {
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = DROPPED;
    ver.m_impl->m_status = NdbDictionary::Object::Invalid;    
  }
  else if(ver.m_impl == &f_altered_table)
  {
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = DROPPED;
    ver.m_impl->m_status = NdbDictionary::Object::Altered;    
  }
  else
  {
    abort();
  }
  NdbCondition_Broadcast(m_waitForTableCondition);
  DBUG_RETURN(tab);
} 

void
GlobalDictCache::drop(NdbTableImpl * tab)
{
  DBUG_ENTER("GlobalDictCache::drop");
  DBUG_PRINT("enter", ("internal_name: %s", tab->m_internalName.c_str()));

  unsigned i;
  const Uint32 len = strlen(tab->m_internalName.c_str());
  Vector<TableVersion> * vers = 
    m_tableHash.getData(tab->m_internalName.c_str(), len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }

  for(i = 0; i < sz; i++){
    TableVersion & ver = (* vers)[i];
    if(ver.m_impl == tab){
      if(ver.m_refCount == 0 || ver.m_status == RETREIVING ||
	 ver.m_version != tab->m_version){
	DBUG_PRINT("info", ("Dropping with refCount=%d status=%d impl=%p",
                            ver.m_refCount, ver.m_status, ver.m_impl));
	break;
      }
      DBUG_PRINT("info", ("Found table to drop, i: %d, name: %s",
                          i, ver.m_impl->m_internalName.c_str()));
      ver.m_refCount--;
      ver.m_status = DROPPED;
      if(ver.m_refCount == 0){
        DBUG_PRINT("info", ("refCount is zero, deleting m_impl"))
	delete ver.m_impl;
	vers->erase(i);
      }
      DBUG_VOID_RETURN;
    }
  }

  for(i = 0; i<sz; i++){
    TableVersion & ver = (* vers)[i];
    DBUG_PRINT("info", ("%d: version: %d refCount: %d status: %d impl: %p",
                        i, ver.m_version, ver.m_refCount,
                        ver.m_status, ver.m_impl));
  }
  
  abort();
}


unsigned
GlobalDictCache::get_size()
{
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(0);
  int sz = 0;
  while(curr != 0){
    sz += curr->theData->size();
    curr = m_tableHash.getNext(curr);
  }
  return sz;
}

void
GlobalDictCache::invalidate_all()
{
  DBUG_ENTER("GlobalDictCache::invalidate_all");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(0);
  while(curr != 0){
    Vector<TableVersion> * vers = curr->theData;
    if (vers->size())
    {
      TableVersion * ver = & vers->back();
      if (ver->m_status != RETREIVING)
      {
        ver->m_impl->m_status = NdbDictionary::Object::Invalid;
        ver->m_status = DROPPED;
        if (ver->m_refCount == 0)
        {
          delete ver->m_impl;
          vers->erase(vers->size() - 1);
        }
      }
    }
    curr = m_tableHash.getNext(curr);
  }
  DBUG_VOID_RETURN;
}

void
GlobalDictCache::release(NdbTableImpl * tab)
{
  DBUG_ENTER("GlobalDictCache::release");
  DBUG_PRINT("enter", ("internal_name: %s", tab->m_internalName.c_str()));

  unsigned i;
  const Uint32 len = strlen(tab->m_internalName.c_str());
  Vector<TableVersion> * vers = 
    m_tableHash.getData(tab->m_internalName.c_str(), len);
  if(vers == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retreive it first 
    // and thus there should be a record
    abort(); 
  }
  
  for(i = 0; i < sz; i++){
    TableVersion & ver = (* vers)[i];
    if(ver.m_impl == tab){
      if(ver.m_refCount == 0 || ver.m_status == RETREIVING || 
	 ver.m_version != tab->m_version){
	DBUG_PRINT("info", ("Releasing with refCount=%d status=%d impl=%p",
                            ver.m_refCount, ver.m_status, ver.m_impl));
	break;
      }
      
      ver.m_refCount--;
      DBUG_VOID_RETURN;
    }
  }
  
  for(i = 0; i<sz; i++){
    TableVersion & ver = (* vers)[i];
    DBUG_PRINT("info", ("%d: version: %d refCount: %d status: %d impl: %p",
                        i, ver.m_version, ver.m_refCount,
                        ver.m_status, ver.m_impl));
  }
  
  abort();
}

void
GlobalDictCache::alter_table_rep(const char * name, 
				 Uint32 tableId, 
				 Uint32 tableVersion,
				 bool altered)
{
  const Uint32 len = strlen(name);
  Vector<TableVersion> * vers = 
    m_tableHash.getData(name, len);
  
  if(vers == 0)
  {
    return;
  }

  const Uint32 sz = vers->size();
  if(sz == 0)
  {
    return;
  }
  
  for(Uint32 i = 0; i < sz; i++)
  {
    TableVersion & ver = (* vers)[i];
    if(ver.m_version == tableVersion && ver.m_impl && 
       ver.m_impl->m_tableId == tableId)
    {
      ver.m_status = DROPPED;
      ver.m_impl->m_status = altered ? 
	NdbDictionary::Object::Altered : NdbDictionary::Object::Invalid;
      return;
    }

    if(i == sz - 1 && ver.m_status == RETREIVING)
    {
      ver.m_impl = altered ? &f_altered_table : &f_invalid_table;
      return;
    } 
  }
}

template class Vector<GlobalDictCache::TableVersion>;
