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

#ifndef NdbDictionaryImpl_H
#define NdbDictionaryImpl_H

#include <ndb_types.h>
#include <kernel_types.h>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <Vector.hpp>
#include <UtilBuffer.hpp>
#include <NdbDictionary.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <Ndb.hpp>
#include "NdbWaiter.hpp"
#include "DictCache.hpp"

class NdbDictObjectImpl {
public:
  Uint32 m_version;
  NdbDictionary::Object::Status m_status;
  
  bool change();
protected:
  NdbDictObjectImpl() :
    m_status(NdbDictionary::Object::New) {
  }
};

/**
 * Column
 */
class NdbColumnImpl : public NdbDictionary::Column {
public:
  NdbColumnImpl();
  NdbColumnImpl(NdbDictionary::Column &); // This is not a copy constructor
  ~NdbColumnImpl();
  NdbColumnImpl& operator=(const NdbColumnImpl&);
  void init(Type t = Unsigned);
  
  int m_attrId;
  BaseString m_name;
  NdbDictionary::Column::Type m_type;
  int m_precision;
  int m_scale;
  int m_length;
  CHARSET_INFO * m_cs;          // not const in MySQL
  
  bool m_pk;
  bool m_distributionKey;
  bool m_nullable;
  bool m_autoIncrement;
  Uint64 m_autoIncrementInitialValue;
  BaseString m_defaultValue;
  NdbTableImpl * m_blobTable;

  /**
   * Internal types and sizes, and aggregates
   */
  Uint32 m_attrSize;            // element size (size when arraySize==1)
  Uint32 m_arraySize;           // length or length+2 for Var* types
  Uint32 m_keyInfoPos;
  // TODO: use bits in attr desc 2
  bool getInterpretableType() const ;
  bool getCharType() const;
  bool getStringType() const;
  bool getBlobType() const;

  /**
   * Equality/assign
   */
  bool equal(const NdbColumnImpl&) const;

  static NdbColumnImpl & getImpl(NdbDictionary::Column & t);
  static const NdbColumnImpl & getImpl(const NdbDictionary::Column & t);
  NdbDictionary::Column * m_facade;

  static NdbDictionary::Column * create_pseudo(const char *);
};

class NdbTableImpl : public NdbDictionary::Table, public NdbDictObjectImpl {
public:
  NdbTableImpl();
  NdbTableImpl(NdbDictionary::Table &);
  ~NdbTableImpl();
  
  void init();
  void setName(const char * name);
  const char * getName() const;
  const char * getMysqlName() const;
  void updateMysqlName();

  Uint32 m_changeMask;
  Uint32 m_tableId;
  Uint32 m_primaryTableId;
  BaseString m_internalName;
  BaseString m_externalName;
  BaseString m_mysqlName;
  BaseString m_newExternalName; // Used for alter table
  UtilBuffer m_frm; 
  UtilBuffer m_ng;
  NdbDictionary::Object::FragmentType m_fragmentType;

  /**
   * 
   */
  Uint32 m_columnHashMask;
  Vector<Uint32> m_columnHash;
  Vector<NdbColumnImpl *> m_columns;
  void buildColumnHash(); 

  /**
   * Fragment info
   */
  Uint32 m_hashValueMask;
  Uint32 m_hashpointerValue;
  Vector<Uint16> m_fragments;

  bool m_logging;
  int m_kvalue;
  int m_minLoadFactor;
  int m_maxLoadFactor;
  Uint16 m_keyLenInWords;
  Uint16 m_fragmentCount;

  NdbDictionaryImpl * m_dictionary;
  NdbIndexImpl * m_index;
  NdbColumnImpl * getColumn(unsigned attrId);
  NdbColumnImpl * getColumn(const char * name);
  const NdbColumnImpl * getColumn(unsigned attrId) const;
  const NdbColumnImpl * getColumn(const char * name) const;
  
  /**
   * Index only stuff
   */
  BaseString m_primaryTable;
  NdbDictionary::Index::Type m_indexType;

  /**
   * Aggregates
   */
  Uint8 m_noOfKeys;
  Uint8 m_noOfDistributionKeys;
  Uint8 m_noOfBlobs;
  
  Uint8 m_replicaCount;

  /**
   * Equality/assign
   */
  bool equal(const NdbTableImpl&) const;
  void assign(const NdbTableImpl&);

  static NdbTableImpl & getImpl(NdbDictionary::Table & t);
  static NdbTableImpl & getImpl(const NdbDictionary::Table & t);
  NdbDictionary::Table * m_facade;
  
  /**
   * Return count
   */
  Uint32 get_nodes(Uint32 hashValue, const Uint16** nodes) const ;
};

class NdbIndexImpl : public NdbDictionary::Index, public NdbDictObjectImpl {
public:
  NdbIndexImpl();
  NdbIndexImpl(NdbDictionary::Index &);
  ~NdbIndexImpl();

  void init();
  void setName(const char * name);
  const char * getName() const;
  void setTable(const char * table);
  const char * getTable() const;
  const NdbTableImpl * getIndexTable() const;

  Uint32 m_indexId;
  BaseString m_internalName;
  BaseString m_externalName;
  BaseString m_tableName;
  Vector<NdbColumnImpl *> m_columns;
  Vector<int> m_key_ids;
  NdbDictionary::Index::Type m_type;

  bool m_logging;
  
  NdbTableImpl * m_table;
  
  static NdbIndexImpl & getImpl(NdbDictionary::Index & t);
  static NdbIndexImpl & getImpl(const NdbDictionary::Index & t);
  NdbDictionary::Index * m_facade;
};

class NdbEventImpl : public NdbDictionary::Event, public NdbDictObjectImpl {
public:
  NdbEventImpl();
  NdbEventImpl(NdbDictionary::Event &);
  ~NdbEventImpl();

  void init();
  void setName(const char * name);
  const char * getName() const;
  void setTable(const NdbDictionary::Table& table);
  void setTable(const char * table);
  const char * getTableName() const;
  void addTableEvent(const NdbDictionary::Event::TableEvent t);
  void setDurability(NdbDictionary::Event::EventDurability d);
  NdbDictionary::Event::EventDurability  getDurability() const;
  void addEventColumn(const NdbColumnImpl &c);
  int getNoOfEventColumns() const;

  void print() {
    ndbout_c("NdbEventImpl: id=%d, key=%d",
	     m_eventId,
	     m_eventKey);
  };

  Uint32 m_eventId;
  Uint32 m_eventKey;
  Uint32 m_tableId;
  Uint32 m_tableVersion;
  AttributeMask m_attrListBitmask;
  BaseString m_name;
  Uint32 mi_type;
  NdbDictionary::Event::EventDurability m_dur;


  NdbTableImpl *m_tableImpl;
  BaseString m_tableName;
  Vector<NdbColumnImpl *> m_columns;
  Vector<unsigned> m_attrIds;

  static NdbEventImpl & getImpl(NdbDictionary::Event & t);
  static NdbEventImpl & getImpl(const NdbDictionary::Event & t);
  NdbDictionary::Event * m_facade;
};


class NdbDictInterface {
public:
  NdbDictInterface(NdbError& err) : m_error(err) {
    m_reference = 0;
    m_masterNodeId = 0;
    m_transporter= NULL;
  }
  ~NdbDictInterface();
  
  bool setTransporter(class Ndb * ndb, class TransporterFacade * tf);
  bool setTransporter(class TransporterFacade * tf);
  
  // To abstract the stuff thats made in all create/drop/lists below
  int
  dictSignal(NdbApiSignal* signal, 
	     LinearSectionPtr ptr[3], int noLPTR,
	     const int useMasterNodeId,
	     const Uint32 RETRIES,
	     const WaitSignalType wst,
	     const int theWait,
	     const int *errcodes,
	     const int noerrcodes,
	     const int temporaryMask = 0);

  int createOrAlterTable(class Ndb & ndb, NdbTableImpl &, bool alter);

  int createTable(class Ndb & ndb, NdbTableImpl &);
  int createTable(NdbApiSignal* signal, LinearSectionPtr ptr[3]);

  int alterTable(class Ndb & ndb, NdbTableImpl &);
  int alterTable(NdbApiSignal* signal, LinearSectionPtr ptr[3]);

  int createIndex(class Ndb & ndb,
		  NdbIndexImpl &, 
		  const NdbTableImpl &);
  int createIndex(NdbApiSignal* signal, LinearSectionPtr ptr[3]);
  
  int createEvent(class Ndb & ndb, NdbEventImpl &, int getFlag);
  int createEvent(NdbApiSignal* signal, LinearSectionPtr ptr[3], int noLSP);
  
  int dropTable(const NdbTableImpl &);
  int dropTable(NdbApiSignal* signal, LinearSectionPtr ptr[3]);

  int dropIndex(const NdbIndexImpl &, const NdbTableImpl &);
  int dropIndex(NdbApiSignal* signal, LinearSectionPtr ptr[3]);

  int dropEvent(const NdbEventImpl &);
  int dropEvent(NdbApiSignal* signal, LinearSectionPtr ptr[3], int noLSP);

  int executeSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &);
  int stopSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &);
  
  int listObjects(NdbDictionary::Dictionary::List& list, Uint32 requestData, bool fullyQualifiedNames);
  int listObjects(NdbApiSignal* signal);
  
/*  NdbTableImpl * getTable(int tableId, bool fullyQualifiedNames); */
  NdbTableImpl * getTable(const BaseString& name, bool fullyQualifiedNames);
  NdbTableImpl * getTable(class NdbApiSignal * signal, 
			  LinearSectionPtr ptr[3],
			  Uint32 noOfSections, bool fullyQualifiedNames);

  int forceGCPWait();

  static int parseTableInfo(NdbTableImpl ** dst, 
			    const Uint32 * data, Uint32 len,
			    bool fullyQualifiedNames);
  
  static int create_index_obj_from_table(NdbIndexImpl ** dst, 
					 NdbTableImpl* index_table,
					 const NdbTableImpl* primary_table);
  const NdbError &getNdbError() const;  
  NdbError & m_error;
private:
  Uint32 m_reference;
  Uint32 m_masterNodeId;
  
  NdbWaiter m_waiter;
  class TransporterFacade * m_transporter;
  
  friend class Ndb;
  static void execSignal(void* dictImpl, 
			 class NdbApiSignal* signal, 
			 struct LinearSectionPtr ptr[3]);
  
  static void execNodeStatus(void* dictImpl, Uint32, 
			     bool alive, bool nfCompleted);  
  
  void execGET_TABINFO_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execGET_TABINFO_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_TABLE_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_TABLE_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execALTER_TABLE_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execALTER_TABLE_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  void execCREATE_INDX_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_INDX_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_INDX_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_INDX_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  void execCREATE_EVNT_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_EVNT_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execSUB_START_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execSUB_START_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execSUB_STOP_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execSUB_STOP_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_EVNT_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_EVNT_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  void execDROP_TABLE_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_TABLE_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execLIST_TABLES_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  void execWAIT_GCP_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execWAIT_GCP_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  Uint32 m_fragmentId;
  UtilBuffer m_buffer;
};

class NdbDictionaryImpl : public NdbDictionary::Dictionary {
public:
  NdbDictionaryImpl(Ndb &ndb);
  NdbDictionaryImpl(Ndb &ndb, NdbDictionary::Dictionary & f);
  ~NdbDictionaryImpl();

  bool setTransporter(class Ndb * ndb, class TransporterFacade * tf);
  bool setTransporter(class TransporterFacade * tf);

  int createTable(NdbTableImpl &t);
  int createBlobTables(NdbTableImpl &);
  int addBlobTables(NdbTableImpl &);
  int alterTable(NdbTableImpl &t);
  int dropTable(const char * name);
  int dropTable(NdbTableImpl &);
  int dropBlobTables(NdbTableImpl &);
  int invalidateObject(NdbTableImpl &);
  int removeCachedObject(NdbTableImpl &);

  int createIndex(NdbIndexImpl &ix);
  int dropIndex(const char * indexName, 
		const char * tableName);
  int dropIndex(NdbIndexImpl &, const char * tableName);
  NdbTableImpl * getIndexTable(NdbIndexImpl * index, 
			       NdbTableImpl * table);

  int createEvent(NdbEventImpl &);
  int dropEvent(const char * eventName);

  int executeSubscribeEvent(NdbEventOperationImpl &);
  int stopSubscribeEvent(NdbEventOperationImpl &);

  int forceGCPWait();

  int listObjects(List& list, NdbDictionary::Object::Type type);
  int listIndexes(List& list, Uint32 indexId);

  NdbTableImpl * getTable(const char * tableName, void **data= 0);
  Ndb_local_table_info* get_local_table_info(
    const BaseString& internalTableName, bool do_add_blob_tables);
  NdbIndexImpl * getIndex(const char * indexName,
			  const char * tableName);
  NdbEventImpl * getEvent(const char * eventName);
  NdbEventImpl * getEventImpl(const char * internalName);
  
  const NdbError & getNdbError() const;
  NdbError m_error;
  Uint32 m_local_table_data_size;

  LocalDictCache m_localHash;
  GlobalDictCache * m_globalHash;

  static NdbDictionaryImpl & getImpl(NdbDictionary::Dictionary & t);
  static const NdbDictionaryImpl & getImpl(const NdbDictionary::Dictionary &t);
  NdbDictionary::Dictionary * m_facade;

  NdbDictInterface m_receiver;
  Ndb & m_ndb;
private:
  NdbIndexImpl * getIndexImpl(const char * name,
                              const BaseString& internalName);
  Ndb_local_table_info * fetchGlobalTableImpl(const BaseString& internalName);
};

inline
NdbEventImpl &
NdbEventImpl::getImpl(const NdbDictionary::Event & t){
  return t.m_impl;
}

inline
NdbEventImpl &
NdbEventImpl::getImpl(NdbDictionary::Event & t){
  return t.m_impl;
}

inline
NdbColumnImpl &
NdbColumnImpl::getImpl(NdbDictionary::Column & t){
  return t.m_impl;
}

inline
const NdbColumnImpl &
NdbColumnImpl::getImpl(const NdbDictionary::Column & t){
  return t.m_impl;
}

inline
bool 
NdbColumnImpl::getInterpretableType() const {
  return (m_type == NdbDictionary::Column::Unsigned ||
	  m_type == NdbDictionary::Column::Bigunsigned);
}

inline
bool 
NdbColumnImpl::getCharType() const {
  return (m_type == NdbDictionary::Column::Char ||
          m_type == NdbDictionary::Column::Varchar ||
          m_type == NdbDictionary::Column::Text ||
          m_type == NdbDictionary::Column::Longvarchar);
}

inline
bool 
NdbColumnImpl::getStringType() const {
  return (m_type == NdbDictionary::Column::Char ||
          m_type == NdbDictionary::Column::Varchar ||
          m_type == NdbDictionary::Column::Longvarchar ||
          m_type == NdbDictionary::Column::Binary ||
          m_type == NdbDictionary::Column::Varbinary ||
          m_type == NdbDictionary::Column::Longvarbinary);
}
   
inline
bool 
NdbColumnImpl::getBlobType() const {
  return (m_type == NdbDictionary::Column::Blob ||
	  m_type == NdbDictionary::Column::Text);
}

inline
NdbTableImpl &
NdbTableImpl::getImpl(NdbDictionary::Table & t){
  return t.m_impl;
}

inline
NdbTableImpl &
NdbTableImpl::getImpl(const NdbDictionary::Table & t){
  return t.m_impl;
}

inline
NdbColumnImpl *
NdbTableImpl::getColumn(unsigned attrId){
  if(m_columns.size() > attrId){
    return m_columns[attrId];
  }
  return 0;
}

inline
const char *
NdbTableImpl::getMysqlName() const
{
  return m_mysqlName.c_str();
}

inline
Uint32
Hash( const char* str ){
  Uint32 h = 0;
  Uint32 len = strlen(str);
  while(len >= 4){
    h = (h << 5) + h + str[0];
    h = (h << 5) + h + str[1];
    h = (h << 5) + h + str[2];
    h = (h << 5) + h + str[3];
    len -= 4;
    str += 4;
  }
  
  switch(len){
  case 3:
    h = (h << 5) + h + *str++;
  case 2:
    h = (h << 5) + h + *str++;
  case 1:
    h = (h << 5) + h + *str++;
  }
  return h + h;
}


inline
NdbColumnImpl *
NdbTableImpl::getColumn(const char * name){

  Uint32 sz = m_columns.size();
  NdbColumnImpl** cols = m_columns.getBase();
  const Uint32 * hashtable = m_columnHash.getBase();

  if(sz > 5 && false){
    Uint32 hashValue = Hash(name) & 0xFFFE;
    Uint32 bucket = hashValue & m_columnHashMask;
    bucket = (bucket < sz ? bucket : bucket - sz);
    hashtable += bucket;
    Uint32 tmp = * hashtable;
    if((tmp & 1) == 1 ){ // No chaining
      sz = 1;
    } else {
      sz = (tmp >> 16);
      hashtable += (tmp & 0xFFFE) >> 1;
      tmp = * hashtable;
    }
    do {
      if(hashValue == (tmp & 0xFFFE)){
	NdbColumnImpl* col = cols[tmp >> 16];
	if(strncmp(name, col->m_name.c_str(), col->m_name.length()) == 0){
	  return col;
	}
      }
      hashtable++;
      tmp = * hashtable;
    } while(--sz > 0);
#if 0
    Uint32 dir = m_columnHash[bucket];
    Uint32 pos = bucket + ((dir & 0xFFFE) >> 1); 
    Uint32 cnt = dir >> 16;
    ndbout_c("col: %s hv: %x bucket: %d dir: %x pos: %d cnt: %d tmp: %d -> 0", 
	     name, hashValue, bucket, dir, pos, cnt, tmp);
#endif
    return 0;
  } else {
    for(Uint32 i = 0; i<sz; i++){
      NdbColumnImpl* col = * cols++;
      if(col != 0 && strcmp(name, col->m_name.c_str()) == 0)
	return col;
    }
  }
  return 0;
}

inline
const NdbColumnImpl *
NdbTableImpl::getColumn(unsigned attrId) const {
  if(m_columns.size() > attrId){
    return m_columns[attrId];
  }
  return 0;
}

inline
const NdbColumnImpl *
NdbTableImpl::getColumn(const char * name) const {
  Uint32 sz = m_columns.size();
  NdbColumnImpl* const * cols = m_columns.getBase();
  for(Uint32 i = 0; i<sz; i++, cols++){
    NdbColumnImpl* col = * cols;
    if(col != 0 && strcmp(name, col->m_name.c_str()) == 0)
      return col;
  }
  return 0;
}

inline
NdbIndexImpl &
NdbIndexImpl::getImpl(NdbDictionary::Index & t){
  return t.m_impl;
}

inline
NdbIndexImpl &
NdbIndexImpl::getImpl(const NdbDictionary::Index & t){
  return t.m_impl;
}

inline
NdbDictionaryImpl &
NdbDictionaryImpl::getImpl(NdbDictionary::Dictionary & t){
  return t.m_impl;
}

inline
const NdbDictionaryImpl &
NdbDictionaryImpl::getImpl(const NdbDictionary::Dictionary & t){
  return t.m_impl;
}

/*****************************************************************
 * Inline:d getters
 */

inline
NdbTableImpl *
NdbDictionaryImpl::getTable(const char * table_name, void **data)
{
  const BaseString internal_tabname(m_ndb.internalize_table_name(table_name));
  Ndb_local_table_info *info=
    get_local_table_info(internal_tabname, true);
  if (info == 0)
    return 0;
  if (data)
    *data= info->m_local_data;
  return info->m_table_impl;
}

inline
Ndb_local_table_info * 
NdbDictionaryImpl::get_local_table_info(const BaseString& internalTableName,
					bool do_add_blob_tables)
{
  Ndb_local_table_info *info= m_localHash.get(internalTableName.c_str());
  if (info == 0) {
    info= fetchGlobalTableImpl(internalTableName);
    if (info == 0) {
      return 0;
    }
  }
  if (do_add_blob_tables && info->m_table_impl->m_noOfBlobs)
    addBlobTables(*(info->m_table_impl));
  
  return info; // autoincrement already initialized
}

inline
NdbIndexImpl * 
NdbDictionaryImpl::getIndex(const char * index_name,
			    const char * table_name)
{
  if (table_name || m_ndb.usingFullyQualifiedNames())
  {
    const BaseString internal_indexname(
      (table_name)
      ?
      m_ndb.internalize_index_name(getTable(table_name), index_name)
      :
      m_ndb.internalize_table_name(index_name)); // Index is also a table

    if (internal_indexname.length())
    {
      Ndb_local_table_info * info=
        get_local_table_info(internal_indexname, false);
      if (info)
      {
	NdbTableImpl * tab= info->m_table_impl;
        if (tab->m_index == 0)
          tab->m_index= getIndexImpl(index_name, internal_indexname);
        if (tab->m_index != 0)
          tab->m_index->m_table= tab;
        return tab->m_index;
      }
    }
  }

  m_error.code= 4243;
  return 0;
}

#endif
