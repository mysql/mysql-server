/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include "DictCache.hpp"
#include "NdbDictionaryImpl.hpp"
#include <NdbTick.h>
#include <NdbCondition.h>
#include <NdbSleep.h>
#include <EventLogger.hpp>

static NdbTableImpl * f_invalid_table = nullptr;
static NdbTableImpl * f_altered_table = nullptr;

// If we are linked with libstdc++ then thread safe
// initialization of the shared table objects can be simplified
#ifdef HAVE_CXXABI_H
#include <my_pthread.h>

static my_pthread_once_t once_control = MY_PTHREAD_ONCE_INIT;

void init_static_variables( void )
{
  static NdbTableImpl _invalid_table;
  static NdbTableImpl _altered_table;
  f_invalid_table = &_invalid_table;
  f_altered_table = &_altered_table;
}
#else
extern NdbMutex *g_ndb_connection_mutex;
static int ndb_dict_cache_count = 0;
#endif

Ndb_local_table_info *
Ndb_local_table_info::create(NdbTableImpl *table_impl, Uint32 sz)
{
  Uint32 tot_size= sizeof(Ndb_local_table_info) - sizeof(Uint64)
    + ((sz+7) & ~7); // round to Uint64
  void *data= malloc(tot_size);
  if (data == nullptr)
    return nullptr;
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
  m_tuple_id_range.reset();
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
LocalDictCache::get(const BaseString& name){
  ASSERT_NOT_MYSQLD;
  return m_tableHash.getData(name.c_str(), name.length());
}

void 
LocalDictCache::put(const BaseString& name, Ndb_local_table_info * tab_info){
  ASSERT_NOT_MYSQLD;
  const Uint32 id = tab_info->m_table_impl->m_id;
  m_tableHash.insertKey(name.c_str(), name.length(), id, tab_info);
}

void
LocalDictCache::drop(const BaseString& name){
  ASSERT_NOT_MYSQLD;
  Ndb_local_table_info *info=
      m_tableHash.deleteKey(name.c_str(), name.length());
  assert(info != nullptr);
  Ndb_local_table_info::destroy(info);
}

/*****************************************************************
 * Global cache
 */
GlobalDictCache::GlobalDictCache(){
  DBUG_ENTER("GlobalDictCache::GlobalDictCache");
  // Initialize static variables
#ifdef HAVE_CXXABI_H
  my_pthread_once(&once_control, init_static_variables);
#else
  NdbMutex_Lock(g_ndb_connection_mutex);
  if (f_invalid_table == nullptr)
    f_invalid_table = new NdbTableImpl();
  if (f_altered_table == nullptr)
    f_altered_table = new NdbTableImpl();
  ndb_dict_cache_count++;
  NdbMutex_Unlock(g_ndb_connection_mutex);
#endif
  m_tableHash.createHashTable();
  m_waitForTableCondition = NdbCondition_Create();
  DBUG_VOID_RETURN;
}

GlobalDictCache::~GlobalDictCache(){
  DBUG_ENTER("GlobalDictCache::~GlobalDictCache");
#ifndef HAVE_CXXABI_H
  NdbMutex_Lock(g_ndb_connection_mutex);
  if (--ndb_dict_cache_count == 0)
  {
    if (f_invalid_table)
    {
      delete f_invalid_table;
      f_invalid_table = nullptr;
    }
    if (f_altered_table)
    {
      delete f_altered_table;
      f_altered_table = nullptr;
    }
  }
  NdbMutex_Unlock(g_ndb_connection_mutex);
#endif
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(nullptr);
  while(curr != nullptr){
    Vector<TableVersion> * vers = curr->theData;
    const unsigned sz = vers->size();
    for(unsigned i = 0; i<sz ; i++){
      TableVersion tv= (*vers)[i];
      DBUG_PRINT("  ", ("vers[%d]: ver: %d, refCount: %d, status: %d",
                        i, tv.m_version, tv.m_refCount, tv.m_status));
      if(tv.m_impl != nullptr)
      {
        DBUG_PRINT("  ", ("m_impl: internalname: %s",
                          tv.m_impl->m_internalName.c_str()));
	delete (* vers)[i].m_impl;
      }
    }
    delete curr->theData;
    curr->theData= nullptr;
    curr = m_tableHash.getNext(curr);
  }
  m_tableHash.releaseHashTable();
  NdbCondition_Destroy(m_waitForTableCondition);
  DBUG_VOID_RETURN;
}

void GlobalDictCache::printCache()
{
  DBUG_ENTER("GlobalDictCache::printCache");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(nullptr);
  while(curr != nullptr){
    DBUG_PRINT("curr", ("len: %d, hash: %d, lk: %d, str: %s",
                        curr->len, curr->hash, curr->localkey1,
                        (char*) curr->str));
    if (curr->theData){
      Vector<TableVersion> * vers = curr->theData;
      const unsigned sz = vers->size();
      for(unsigned i = 0; i<sz ; i++){
        TableVersion tv= (*vers)[i];
        DBUG_PRINT("  ", ("impl: %p  vers[%d]: ver: %d, refCount: %d, status: %d",
                          tv.m_impl, i, tv.m_version, tv.m_refCount, tv.m_status));
        if(tv.m_impl != nullptr)
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
GlobalDictCache::get(const BaseString& name, int *error)
{
  DBUG_ENTER("GlobalDictCache::get");
  DBUG_PRINT("enter", ("name: %s", name.c_str()));

  const Uint32 len = name.length();
  Vector<TableVersion> * versions = nullptr;
  versions = m_tableHash.getData(name.c_str(), len);
  if(versions == nullptr){
    versions = new Vector<TableVersion>(2);
    if (versions == nullptr)
    {
      *error = -1;
      DBUG_RETURN(0);
    }
    m_tableHash.insertKey(name.c_str(), len, 0, versions);
  }

  int waitTime = 100;

  bool retreive = false;
  while(versions->size() > 0 && !retreive){
    TableVersion * ver = & versions->back();
    switch(ver->m_status){
    case OK:
      if (ver->m_impl->m_status == NdbDictionary::Object::Invalid)
      {
        ver->m_status = DROPPED;
        retreive = true; // Break loop
        if (ver->m_refCount == 0)
        {
          delete ver->m_impl;
          versions->erase(versions->size() - 1);
        }
        break;
      }
      ver->m_refCount++;
      DBUG_PRINT("info", ("Table OK tab: %p  version=%x.%x refCount=%u",
                          ver->m_impl,
                          ver->m_impl->m_version & 0xFFFFFF,
                          ver->m_impl->m_version >> 24,
                          ver->m_refCount));
      DBUG_RETURN(ver->m_impl);
    case DROPPED:
      retreive = true; // Break loop
      break;
    case RETREIVING:
      DBUG_PRINT("info", ("Wait for retrieving thread"));
      NdbCondition_WaitTimeout(m_waitForTableCondition, m_mutex, waitTime);
      continue;
    }
  }
  
  /**
   * Create new...
   */
  TableVersion tmp;
  tmp.m_version = 0;
  tmp.m_impl = nullptr;
  tmp.m_status = RETREIVING;
  tmp.m_refCount = 1; // The one retrieving it
  if (versions->push_back(tmp))
  {
    *error = -1;
     DBUG_RETURN(0);
  }
  DBUG_PRINT("info", ("No table found"));
  DBUG_RETURN(0);
}

NdbTableImpl *
GlobalDictCache::put(const BaseString& name, NdbTableImpl * tab)
{
  DBUG_ENTER("GlobalDictCache::put");
  DBUG_PRINT("enter", ("tab: %p  name: %s, internal_name: %s version: %x.%x",
                       tab, name.c_str(),
                       tab ? tab->m_internalName.c_str() : "tab NULL",
                       tab ? tab->m_version & 0xFFFFFF : 0,
                       tab ? tab->m_version >> 24 : 0));

  Vector<TableVersion> * vers =
      m_tableHash.getData(name.c_str(), name.length());
  if(vers == nullptr){
    // Should always tried to retrieve it first 
    // and thus there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retrieve it first 
    // and thus there should be a record
    abort(); 
  }
  
  TableVersion & ver = vers->back();
  if(ver.m_status != RETREIVING || 
     !(ver.m_impl == nullptr || 
       ver.m_impl == f_invalid_table || ver.m_impl == f_altered_table) || 
     ver.m_version != 0 || 
     ver.m_refCount == 0){
    abort();
  }
  
  if(tab == nullptr)
  {
    DBUG_PRINT("info", ("No table found in db"));
    vers->erase(sz - 1);
  } 
  else if (ver.m_impl == nullptr) {
    DBUG_PRINT("info", ("Table OK"));
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = OK;
  } 
  else if (ver.m_impl == f_invalid_table) 
  {
    DBUG_PRINT("info", ("Table DROPPED invalid"));
    ver.m_impl = tab;
    ver.m_version = tab->m_version;
    ver.m_status = DROPPED;
    ver.m_impl->m_status = NdbDictionary::Object::Invalid;    
  }
  else if(ver.m_impl == f_altered_table)
  {
    DBUG_PRINT("info", ("Table DROPPED altered"));
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

unsigned
GlobalDictCache::get_size()
{
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(nullptr);
  int sz = 0;
  while(curr != nullptr){
    sz += curr->theData->size();
    curr = m_tableHash.getNext(curr);
  }
  if (sz)
  {
    printCache();
  }
  return sz;
}

void
GlobalDictCache::invalidate_all()
{
  DBUG_ENTER("GlobalDictCache::invalidate_all");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(nullptr);
  while(curr != nullptr){
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
GlobalDictCache::invalidateDb(const char * name, size_t len)
{
  DBUG_ENTER("GlobalDictCache::invalidateDb");
  NdbElement_t<Vector<TableVersion> > * curr = m_tableHash.getNext(nullptr);
  while(curr != nullptr)
  {
    Vector<TableVersion> * vers = curr->theData;
    if (vers->size())
    {
      TableVersion * ver = & vers->back();
      if (ver->m_status != RETREIVING)
      {
        if (ver->m_impl->matchDb(name, len))
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
    }
    curr = m_tableHash.getNext(curr);
  }
  DBUG_VOID_RETURN;
}

void
GlobalDictCache::release(const NdbTableImpl * tab, int invalidate)
{
  DBUG_ENTER("GlobalDictCache::release");
  DBUG_PRINT("enter", ("tab: %p  internal_name: %s",
                       tab, tab->m_internalName.c_str()));

  unsigned i;
  Vector<TableVersion> * vers = 
    m_tableHash.getData(tab->m_internalName.c_str(),
                        tab->m_internalName.length());
  if(vers == nullptr){
    // Should always tried to retrieve it first 
    // and thus there should be a record
    abort(); 
  }

  const Uint32 sz = vers->size();
  if(sz == 0){
    // Should always tried to retrieve it first 
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
      if (ver.m_impl->m_status == NdbDictionary::Object::Invalid || invalidate)
      {
        ver.m_impl->m_status = NdbDictionary::Object::Invalid;
        ver.m_status = DROPPED;
      }
      if (ver.m_refCount == 0 && ver.m_status == DROPPED)
      {
        DBUG_PRINT("info", ("refCount is zero, deleting m_impl"));
        delete ver.m_impl;
        vers->erase(i);
      }
      DBUG_VOID_RETURN;
    }
  }
  
  for(i = 0; i<sz; i++){
    TableVersion & ver = (* vers)[i];
    g_eventLogger->info("%d: version: %d refCount: %d status: %d impl: %p", i,
                        ver.m_version, ver.m_refCount, ver.m_status,
                        ver.m_impl);
  }
  
  abort();
}

void
GlobalDictCache::alter_table_rep(const BaseString& name,
				 Uint32 tableId, 
				 Uint32 tableVersion,
				 bool altered)
{
  DBUG_ENTER("GlobalDictCache::alter_table_rep");
  Vector<TableVersion> * vers =
    m_tableHash.getData(name.c_str(), name.length());
  
  if(vers == nullptr)
  {
    DBUG_VOID_RETURN;
  }

  const Uint32 sz = vers->size();
  if(sz == 0)
  {
    DBUG_VOID_RETURN;
  }
  
  for(Uint32 i = 0; i < sz; i++)
  {
    TableVersion & ver = (* vers)[i];
    if(ver.m_version == tableVersion && ver.m_impl && 
       (Uint32) ver.m_impl->m_id == tableId)
    {
      ver.m_status = DROPPED;
      ver.m_impl->m_status = altered ? 
	NdbDictionary::Object::Altered : NdbDictionary::Object::Invalid;
      if (ver.m_refCount == 0)
      {
        delete ver.m_impl;
        vers->erase(i);
      }
      DBUG_VOID_RETURN;
    }

    if(i == sz - 1 && ver.m_status == RETREIVING)
    {
      ver.m_impl = altered ? f_altered_table : f_invalid_table;
      DBUG_VOID_RETURN;
    } 
  }
  DBUG_VOID_RETURN;
}

int
GlobalDictCache::chg_ref_count(const NdbTableImpl * impl, int value)
{
  DBUG_ENTER("GlobalDictCache::chg_ref_count");

  Vector<TableVersion> * vers = 
    m_tableHash.getData(impl->m_internalName.c_str(),
                        impl->m_internalName.length());
  if(vers == nullptr)
  {
    DBUG_RETURN(-1);
  }

  const Uint32 sz = vers->size();
  if(sz == 0)
  {
    DBUG_RETURN(-1);
  }
  
  for(Uint32 i = 0; i < sz; i++)
  {
    TableVersion & ver = (* vers)[i];
    if(ver.m_impl == impl)
    {
      if (value == +1)
      {
        DBUG_PRINT("info", ("%s id=%u ver=0x%x: inc old ref count %u",
                            impl->m_internalName.c_str(), impl->m_id,
                            impl->m_version, ver.m_refCount));
        ver.m_refCount++;
      }
      else if (value == -1)
      {
        DBUG_PRINT("info", ("%s id=%u ver=0x%x: dec old ref count %u",
                            impl->m_internalName.c_str(), impl->m_id,
                            impl->m_version, ver.m_refCount));
        if (ver.m_refCount == 0)
          abort();
        ver.m_refCount--;
        if (ver.m_refCount == 0)
        {
          delete ver.m_impl;
          vers->erase(i);
        }
      }
      else
        abort();
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(0);
}

template class Vector<GlobalDictCache::TableVersion>;
