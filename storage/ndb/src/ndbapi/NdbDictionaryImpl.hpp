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

#ifndef NdbDictionaryImpl_H
#define NdbDictionaryImpl_H

#include <ndb_types.h>
#include <kernel_types.h>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <Vector.hpp>
#include <UtilBuffer.hpp>
#include <SimpleProperties.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbDictionary.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <Ndb.hpp>
#include "DictCache.hpp"
#include <signaldata/DictSignal.hpp>

class ListTablesReq;

bool
is_ndb_blob_table(const char* name, Uint32* ptab_id = 0, Uint32* pcol_no = 0);
bool
is_ndb_blob_table(const class NdbTableImpl* t);

extern int ndb_dictionary_is_mysqld;
#define ASSERT_NOT_MYSQLD assert(ndb_dictionary_is_mysqld == 0)

class NdbDictObjectImpl {
public:
  int m_id;
  Uint32 m_version;
  NdbDictionary::Object::Type m_type;
  NdbDictionary::Object::Status m_status;
  
  bool change();
  
  static NdbDictObjectImpl & getImpl(NdbDictionary::ObjectId & t) { 
    return t.m_impl;
  }
  static const NdbDictObjectImpl & getImpl(const NdbDictionary::ObjectId & t){
    return t.m_impl;
  }
  
protected:
  friend class NdbDictionary::ObjectId;

  NdbDictObjectImpl(NdbDictionary::Object::Type type) :
    m_type(type),
    m_status(NdbDictionary::Object::New) {
    m_id = -1;
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
  int m_column_no;
  CHARSET_INFO * m_cs;          // not const in MySQL

  bool m_pk;
  bool m_distributionKey;
  bool m_nullable;
  bool m_autoIncrement;
  Uint64 m_autoIncrementInitialValue;
  UtilBuffer m_defaultValue;
  /*
   * Table holding the blob parts.
   *
   * Note that if getPartSize() is 0, there is no parts table, so
   * m_blobTable==NULL.
   */
  NdbTableImpl * m_blobTable;

  /**
   * Internal types and sizes, and aggregates
   */
  Uint32 m_orgAttrSize;
  Uint32 m_attrSize;            // element size (size when arraySize==1)
  Uint32 m_arraySize;           // length or maxlength+1/2 for Var* types
  Uint32 m_arrayType;           // NDB_ARRAYTYPE_FIXED or _VAR
  Uint32 m_storageType;         // NDB_STORAGETYPE_MEMORY or _DISK
                                // for blob, storage type of NDB$DATA
  bool m_dynamic;

  bool m_indexSourced;          // Computed when column defined, or when
                                // table schema read.  Not stored in kernel 

  /*
   * NdbTableImpl: if m_pk, 0-based index of key in m_attrId order
   * NdbIndexImpl: m_column_no of primary table column
   */
  Uint32 m_keyInfoPos;
  // TODO: use bits in attr desc 2
  bool getInterpretableType() const ;
  bool getCharType() const;
  bool getStringType() const;
  bool getBlobType() const;

  int m_blobVersion;            // if blob, NDB_BLOB_V1 or NDB_BLOB_V2
  int getBlobVersion() const;
  void setBlobVersion(int blobVersion);

  /**
   * Equality/assign
   */
  bool equal(const NdbColumnImpl&) const;

  static NdbColumnImpl & getImpl(NdbDictionary::Column & t);
  static const NdbColumnImpl & getImpl(const NdbDictionary::Column & t);
  NdbDictionary::Column * m_facade;

  static void create_pseudo_columns();
  static void destory_pseudo_columns();
  static NdbDictionary::Column * create_pseudo(const char *);

  // Get total length in bytes, used by NdbOperation
  bool get_var_length(const void* value, Uint32& len) const;
};

class NdbTableImpl : public NdbDictionary::Table, public NdbDictObjectImpl {
public:
  NdbTableImpl();
  NdbTableImpl(NdbDictionary::Table &);
  ~NdbTableImpl();
  
  void init();
  int setName(const char * name);
  const char * getName() const;
  void setFragmentCount(Uint32 count);
  Uint32 getFragmentCount() const;
  int setFrm(const void* data, Uint32 len);
  const void * getFrmData() const;
  Uint32 getFrmLength() const;

  int setFragmentData(const Uint32* data, Uint32 cnt);
  const Uint32 * getFragmentData() const;
  Uint32 getFragmentDataLen() const;

  int setRangeListData(const Int32* data, Uint32 cnt);
  const Int32 * getRangeListData() const;
  Uint32 getRangeListDataLen() const;

  Uint32 getFragmentNodes(Uint32 fragmentId, 
                          Uint32* nodeIdArrayPtr,
                          Uint32 arraySize) const;

  const char * getMysqlName() const;
  int updateMysqlName();

  bool matchDb(const char * name, size_t len) const;

  int aggregate(NdbError& error);
  int validate(NdbError& error);

  Uint32 m_primaryTableId;
  BaseString m_internalName; // db/schema/table
  BaseString m_externalName; //           table
  BaseString m_mysqlName;    //        db/table
  UtilBuffer m_frm; 
  Vector<Uint32> m_fd;
  Vector<Int32> m_range;
  NdbDictionary::Object::FragmentType m_fragmentType;

  int getDbName(char * buf, size_t len) const;
  int getSchemaName(char * buf, size_t len) const;
  void setDbSchema(const char * db, const char * schema);

  /**
   * 
   */
  Uint32 m_columnHashMask;
  Vector<Uint32> m_columnHash;
  /*
    List of all columns in the table.
    Note that for index table objects, there is one additional column at the
    end, NDB$TNODE (ordered index) or NDB$PK. This must be taken into account
    if iterating over columns.
  */
  Vector<NdbColumnImpl *> m_columns;
  Uint32 m_noOfAutoIncColumns;
  void computeAggregates();
  int buildColumnHash(); 

  /**
   * Fragment info
   */
  Uint32 m_hashValueMask;
  Uint32 m_hashpointerValue;
  Vector<Uint16> m_fragments;
  Vector<Uint16> m_hash_map;

  Uint64 m_max_rows;
  Uint64 m_min_rows;
  Uint32 m_default_no_part_flag;
  bool m_linear_flag;
  bool m_logging;
  bool m_temporary;
  bool m_row_gci;
  bool m_row_checksum;
  bool m_force_var_part;
  bool m_has_default_values; 
  int m_kvalue;
  int m_minLoadFactor;
  int m_maxLoadFactor;
  Uint16 m_keyLenInWords;
  Uint16 m_fragmentCount;
  Uint8 m_single_user_mode;
  Uint8 m_storageType;  // NDB_STORAGETYPE_MEMORY or _DISK or DEFAULT
  Uint8 m_extra_row_gci_bits;
  Uint8 m_extra_row_author_bits;

  NdbIndexImpl * m_index;
  NdbColumnImpl * getColumn(unsigned attrId);
  NdbColumnImpl * getColumn(const char * name);
  const NdbColumnImpl * getColumn(unsigned attrId) const;
  const NdbColumnImpl * getColumn(const char * name) const;
  
  /**
   * Index only stuff
   */
  BaseString m_primaryTable;    // Name of table indexed by us
  NdbDictionary::Object::Type m_indexType;

  /**
   * Aggregates
   */
  Uint8 m_noOfKeys;
  // if all pk = dk then this is zero!
  Uint8 m_noOfDistributionKeys;
  Uint8 m_noOfBlobs;
  Uint8 m_noOfDiskColumns;
  Uint8 m_replicaCount;

  /**
   * Default NdbRecord for this table or index
   * Currently used by old-Api scans to use NdbRecord API internally.
   */
  NdbRecord *m_ndbrecord;

  /**
   * Bitmask describing PK column positions for this table
   * Currently used by old-Api scans to use NdbRecord API internally.
   */
  const unsigned char * m_pkMask;

  /**
   * Equality/assign
   */
  bool equal(const NdbTableImpl&) const;
  int assign(const NdbTableImpl&);

  static NdbTableImpl & getImpl(NdbDictionary::Table & t);
  static NdbTableImpl & getImpl(const NdbDictionary::Table & t);
  NdbDictionary::Table * m_facade;
  
  /**
   * Return count
   */
  Uint32 get_nodes(Uint32 partitionId, const Uint16** nodes) const ;
  
  /**
   * Disk stuff
   */
  BaseString m_tablespace_name;
  Uint32 m_tablespace_id;
  Uint32 m_tablespace_version;

  Uint32 m_hash_map_id;
  Uint32 m_hash_map_version;
};

class NdbIndexImpl : public NdbDictionary::Index, public NdbDictObjectImpl {
public:
  NdbIndexImpl();
  NdbIndexImpl(NdbDictionary::Index &);
  ~NdbIndexImpl();

  void init();
  int setName(const char * name);
  const char * getName() const;
  int setTable(const char * table);
  const char * getTable() const;
  const NdbTableImpl * getIndexTable() const;

  BaseString m_internalName;
  BaseString m_externalName;
  BaseString m_tableName;
  Uint32 m_table_id;
  Uint32 m_table_version;
  Vector<NdbColumnImpl *> m_columns;
  Vector<int> m_key_ids;

  bool m_logging;
  bool m_temporary;
  
  /*
    The m_table member refers to the NDB table object that holds the actual
    index, not the table that is indexed by the index (so it is of index
    type, not table type).
  */
  NdbTableImpl * m_table;
  
  static NdbIndexImpl & getImpl(NdbDictionary::Index & t);
  static NdbIndexImpl & getImpl(const NdbDictionary::Index & t);
  NdbDictionary::Index * m_facade;
};

class NdbOptimizeTableHandleImpl : public NdbDictionary::OptimizeTableHandle
{
  enum State { CREATED, INITIALIZED, FINISHED, ABORTED, CLOSED };
private:
  int start();

  State m_state;
  Ndb *m_ndb;
  struct fifo_element_st {
    fifo_element_st(const NdbTableImpl *tab, fifo_element_st *prev)
      { 
        table = tab; 
        previous = prev; 
        next = NULL;
        if (prev) 
          prev->next = this; 
      } 
    const NdbTableImpl * table;
    fifo_element_st * previous;
    fifo_element_st * next;
  };
  const NdbTableImpl * m_table;
  fifo_element_st * m_table_queue;
  fifo_element_st * m_table_queue_first;
  fifo_element_st * m_table_queue_end;
  NdbTransaction * m_trans;
  NdbScanOperation * m_scan_op;

  NdbDictionary::OptimizeTableHandle * m_facade;
public:
  NdbOptimizeTableHandleImpl(NdbDictionary::OptimizeTableHandle &);
  ~NdbOptimizeTableHandleImpl();
  static NdbOptimizeTableHandleImpl & getImpl(NdbDictionary::OptimizeTableHandle &h)
    { return h.m_impl; }

  int init(Ndb *ndb, const NdbTableImpl &table);
  int next();
  int close();
};

class NdbOptimizeIndexHandleImpl : public NdbDictionary::OptimizeIndexHandle
{
  enum State { CREATED, INITIALIZED, FINISHED, ABORTED, CLOSED };
private:
  State m_state;
  Ndb *m_ndb;
  const NdbIndexImpl *m_index;
  class NdbDictionary::OptimizeTableHandle m_optimize_table_handle;
  NdbDictionary::OptimizeIndexHandle * m_facade;
public:
  NdbOptimizeIndexHandleImpl(NdbDictionary::OptimizeIndexHandle &);
  ~NdbOptimizeIndexHandleImpl();
  static NdbOptimizeIndexHandleImpl & getImpl(NdbDictionary::OptimizeIndexHandle &h)
    { return h.m_impl; }

  int init(Ndb *ndb, const NdbIndexImpl &index);
  int next();
  int close();
};

class NdbEventImpl : public NdbDictionary::Event, public NdbDictObjectImpl {
  friend class NdbDictInterface;
  friend class NdbDictionaryImpl;
  friend class NdbEventOperation;
  friend class NdbEventOperationImpl;
  friend class NdbEventBuffer;
  friend class EventBufData_hash;
  friend class NdbBlob;
public:
  NdbEventImpl();
  NdbEventImpl(NdbDictionary::Event &);
  ~NdbEventImpl();

  void init();
  int setName(const char * name);
  const char * getName() const;
  int setTable(const NdbDictionary::Table& table);
  const NdbDictionary::Table * getTable() const;
  int setTable(const char * table);
  const char * getTableName() const;
  void addTableEvent(const NdbDictionary::Event::TableEvent t);
  bool getTableEvent(const NdbDictionary::Event::TableEvent t) const;
  void setDurability(NdbDictionary::Event::EventDurability d);
  NdbDictionary::Event::EventDurability  getDurability() const;
  void setReport(NdbDictionary::Event::EventReport r);
  NdbDictionary::Event::EventReport  getReport() const;
  int getNoOfEventColumns() const;
  const NdbDictionary::Column * getEventColumn(unsigned no) const;

  void print() {
    ndbout_c("NdbEventImpl: id=%d, key=%d",
	     m_eventId,
	     m_eventKey);
  };

  Uint32 m_eventId;
  Uint32 m_eventKey;
  AttributeMask m_attrListBitmask;
  Uint32 m_table_id;
  Uint32 m_table_version;
  BaseString m_name;
  Uint32 mi_type;
  NdbDictionary::Event::EventDurability m_dur;
  NdbDictionary::Event::EventReport m_rep;
  bool m_mergeEvents;

  BaseString m_tableName;
  Vector<NdbColumnImpl *> m_columns;
  Vector<unsigned> m_attrIds;

  static NdbEventImpl & getImpl(NdbDictionary::Event & t);
  static NdbEventImpl & getImpl(const NdbDictionary::Event & t);
  NdbDictionary::Event * m_facade;
private:
  NdbTableImpl *m_tableImpl;
  void setTable(NdbTableImpl *tableImpl);
};

struct NdbFilegroupImpl : public NdbDictObjectImpl {
  NdbFilegroupImpl(NdbDictionary::Object::Type t);

  BaseString m_name;
  NdbDictionary::AutoGrowSpecification m_grow_spec;

  union {
    Uint32 m_extent_size;
    Uint32 m_undo_buffer_size;
  };

  BaseString m_logfile_group_name;
  Uint32 m_logfile_group_id;
  Uint32 m_logfile_group_version;
  Uint64 m_undo_free_words;
};

class NdbTablespaceImpl : public NdbDictionary::Tablespace, 
			  public NdbFilegroupImpl {
public:
  NdbTablespaceImpl();
  NdbTablespaceImpl(NdbDictionary::Tablespace &);
  ~NdbTablespaceImpl();

  int assign(const NdbTablespaceImpl&);

  static NdbTablespaceImpl & getImpl(NdbDictionary::Tablespace & t);
  static const NdbTablespaceImpl & getImpl(const NdbDictionary::Tablespace &);
  NdbDictionary::Tablespace * m_facade;
};

class NdbLogfileGroupImpl : public NdbDictionary::LogfileGroup, 
			    public NdbFilegroupImpl {
public:
  NdbLogfileGroupImpl();
  NdbLogfileGroupImpl(NdbDictionary::LogfileGroup &);
  ~NdbLogfileGroupImpl();

  int assign(const NdbLogfileGroupImpl&);

  static NdbLogfileGroupImpl & getImpl(NdbDictionary::LogfileGroup & t);
  static const NdbLogfileGroupImpl& getImpl(const 
					    NdbDictionary::LogfileGroup&);
  NdbDictionary::LogfileGroup * m_facade;
};

struct NdbFileImpl : public NdbDictObjectImpl {
  NdbFileImpl(NdbDictionary::Object::Type t);

  Uint64 m_size;
  Uint64 m_free;
  BaseString m_path;
  BaseString m_filegroup_name;
  Uint32 m_filegroup_id;
  Uint32 m_filegroup_version;
};

class NdbDatafileImpl : public NdbDictionary::Datafile, public NdbFileImpl {
public:
  NdbDatafileImpl();
  NdbDatafileImpl(NdbDictionary::Datafile &);
  ~NdbDatafileImpl();

  int assign(const NdbDatafileImpl&);

  static NdbDatafileImpl & getImpl(NdbDictionary::Datafile & t);
  static const NdbDatafileImpl & getImpl(const NdbDictionary::Datafile & t);
  NdbDictionary::Datafile * m_facade;
};

class NdbUndofileImpl : public NdbDictionary::Undofile, public NdbFileImpl {
public:
  NdbUndofileImpl();
  NdbUndofileImpl(NdbDictionary::Undofile &);
  ~NdbUndofileImpl();

  int assign(const NdbUndofileImpl&);

  static NdbUndofileImpl & getImpl(NdbDictionary::Undofile & t);
  static const NdbUndofileImpl & getImpl(const NdbDictionary::Undofile & t);
  NdbDictionary::Undofile * m_facade;
};

class NdbHashMapImpl : public NdbDictionary::HashMap, public NdbDictObjectImpl
{
public:
  NdbHashMapImpl();
  NdbHashMapImpl(NdbDictionary::HashMap &);
  ~NdbHashMapImpl();

  int assign(const NdbHashMapImpl& src);

  BaseString m_name;
  Vector<Uint32> m_map;
  NdbDictionary::HashMap * m_facade;

  static NdbHashMapImpl & getImpl(NdbDictionary::HashMap & t){
    return t.m_impl;
  }

  static const NdbHashMapImpl & getImpl(const NdbDictionary::HashMap & t){
    return t.m_impl;
  }


};

class NdbDictInterface {
public:
  // one transaction per Dictionary instance is supported
  struct Tx {
    // api-side schema op, currently only for alter table
    struct Op {
      Uint32 m_gsn;
      NdbTableImpl* m_impl;
    };
    enum State {
      NotStarted,
      Started,
      Committed,
      Aborted
    };
    State m_state;
    NdbError m_error;
    Uint32 m_transId;   // API
    Uint32 m_transKey;  // DICT
    Vector<Op> m_op;
    Tx() :
      m_state(NotStarted),
      m_transId(0),
      m_transKey(0)
    {
      m_error.code = 0;
    }
    Uint32 transId() const {
      return (m_state == Started) ? m_transId : 0;
    }
    Uint32 transKey() const {
      return (m_state == Started) ? m_transKey : 0;
    }
    Uint32 requestFlags() const {
      Uint32 flags = 0;
      // not yet supported in DICT
      return flags;
    }
  };

  NdbDictInterface(Tx& tx, NdbError& err, int& warn) :
    m_tx(tx), m_error(err), m_warn(warn) {
    m_reference = 0;
    m_masterNodeId = 0;
    m_impl = 0;
  }
  ~NdbDictInterface();
  
  bool setTransporter(class Ndb * ndb);
  class TransporterFacade *getTransporter() const;
  
  // To abstract the stuff thats made in all create/drop/lists below
  int dictSignal(NdbApiSignal* signal, LinearSectionPtr ptr[3], int secs,
		 int nodeId, // -1 any, 0 = master, >1 = specified
		 Uint32 waitsignaltype,
		 int timeout, Uint32 RETRIES,
		 const int *errcodes = 0, int temporaryMask = 0);

  int createTable(class Ndb & ndb, NdbTableImpl &);
  bool supportedAlterTable(const NdbTableImpl &,
			   NdbTableImpl &);
  int alterTable(class Ndb & ndb, const NdbTableImpl &, NdbTableImpl&, Uint32&);
  void syncInternalName(Ndb & ndb, NdbTableImpl &impl);
  int compChangeMask(const NdbTableImpl &old_impl,
                     const NdbTableImpl &impl,
                     Uint32 &change_mask);
  int serializeTableDesc(Ndb & ndb,
                         NdbTableImpl & impl,
                         UtilBufferWriter & w);
  int sendAlterTable(const NdbTableImpl &impl,
                     Uint32 change_mask,
                     UtilBufferWriter &w);
  int sendCreateTable(const NdbTableImpl &impl, UtilBufferWriter &w);

  int dropTable(const NdbTableImpl &);

  int createIndex(class Ndb & ndb, const NdbIndexImpl &, const NdbTableImpl &,
                  bool offline);
  int dropIndex(const NdbIndexImpl &, const NdbTableImpl &);
  int doIndexStatReq(class Ndb& ndb,
                     const NdbIndexImpl&, const NdbTableImpl&,
                     Uint32 requestType);
  int doIndexStatReq(class Ndb& ndb,
                     Uint32 indexId, Uint32 indexVersion, Uint32 tableId,
                     Uint32 requestType);
  
  int createEvent(class Ndb & ndb, NdbEventImpl &, int getFlag);
  int dropEvent(const NdbEventImpl &);
  int dropEvent(NdbApiSignal* signal, LinearSectionPtr ptr[3], int noLSP);

  int executeSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &, Uint32&);
  int stopSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &);
  
  int listObjects(NdbDictionary::Dictionary::List& list,
                  ListTablesReq& ltreq, bool fullyQualifiedNames);
  int listObjects(NdbApiSignal* signal, bool& listTablesLongSignal);

  int unpackListTables(NdbDictionary::Dictionary::List& list,
                       bool fullyQualifiedNames);
  int unpackOldListTables(NdbDictionary::Dictionary::List& list,
                          bool fullyQualifiedNames);
  
  NdbTableImpl * getTable(int tableId, bool fullyQualifiedNames);
  NdbTableImpl * getTable(const BaseString& name, bool fullyQualifiedNames);
  NdbTableImpl * getTable(class NdbApiSignal * signal, 
			  LinearSectionPtr ptr[3],
			  Uint32 noOfSections, bool fullyQualifiedNames);

  int forceGCPWait(int type);
  int getRestartGCI(Uint32 *);

  static int parseTableInfo(NdbTableImpl ** dst, 
			    const Uint32 * data, Uint32 len,
			    bool fullyQualifiedNames,
                            Uint32 version= 0xFFFFFFFF);

  static int parseFileInfo(NdbFileImpl &dst,
			   const Uint32 * data, Uint32 len);

  static int parseFilegroupInfo(NdbFilegroupImpl &dst,
				const Uint32 * data, Uint32 len);
  
  static int parseHashMapInfo(NdbHashMapImpl& dst,
                              const Uint32 * data, Uint32 len);

  int create_file(const NdbFileImpl &, const NdbFilegroupImpl&, 
		  bool overwrite, NdbDictObjectImpl*);
  int drop_file(const NdbFileImpl &);
  int create_filegroup(const NdbFilegroupImpl &, NdbDictObjectImpl*);
  int drop_filegroup(const NdbFilegroupImpl &);
  
  int get_filegroup(NdbFilegroupImpl&, NdbDictionary::Object::Type, Uint32);
  int get_filegroup(NdbFilegroupImpl&,NdbDictionary::Object::Type,const char*);
  int get_file(NdbFileImpl&, NdbDictionary::Object::Type, int, int);
  int get_file(NdbFileImpl&, NdbDictionary::Object::Type, int, const char *);
  
  static int create_index_obj_from_table(NdbIndexImpl ** dst, 
					 NdbTableImpl* index_table,
					 const NdbTableImpl* primary_table);

  int create_hashmap(const NdbHashMapImpl&, NdbDictObjectImpl*, Uint32 flags);
  int get_hashmap(NdbHashMapImpl&, Uint32 id);
  int get_hashmap(NdbHashMapImpl&, const char * name);

  int beginSchemaTrans(bool retry711 = true);
  int endSchemaTrans(Uint32 flags);
  Tx & m_tx; // shared with NdbDictionaryImpl

  bool checkAllNodeVersionsMin(Uint32 minNdbVersion) const;

  const NdbError &getNdbError() const;  
  NdbError & m_error;
  int & m_warn;
private:
  Uint32 m_reference;
  Uint32 m_masterNodeId;

  class NdbImpl * m_impl;
  
  friend class Ndb;
  friend class NdbImpl;
  friend class NdbDictionaryImpl;
  static void execSignal(void* dictImpl, 
			 const class NdbApiSignal* signal,
			 const struct LinearSectionPtr ptr[3]);
  
  static void execNodeStatus(void* dictImpl, Uint32, Uint32);
  
  void execGET_TABINFO_REF(const NdbApiSignal *, const LinearSectionPtr p[3]);
  void execGET_TABINFO_CONF(const NdbApiSignal *, const LinearSectionPtr p[3]);
  void execCREATE_TABLE_REF(const NdbApiSignal *, const LinearSectionPtr p[3]);
  void execCREATE_TABLE_CONF(const NdbApiSignal *, const LinearSectionPtr [3]);
  void execALTER_TABLE_REF(const NdbApiSignal *, const LinearSectionPtr pr[3]);
  void execALTER_TABLE_CONF(const NdbApiSignal *, const LinearSectionPtr p[3]);

  void execCREATE_INDX_REF(const NdbApiSignal *, const LinearSectionPtr pr[3]);
  void execCREATE_INDX_CONF(const NdbApiSignal *, const LinearSectionPtr p[3]);
  void execDROP_INDX_REF(const NdbApiSignal *, const LinearSectionPtr ptr[3]);
  void execDROP_INDX_CONF(const NdbApiSignal *, const LinearSectionPtr ptr[3]);

  void execINDEX_STAT_CONF(const NdbApiSignal *, const LinearSectionPtr ptr[3]);
  void execINDEX_STAT_REF(const NdbApiSignal *, const LinearSectionPtr ptr[3]);

  void execCREATE_EVNT_REF(const NdbApiSignal *, const LinearSectionPtr pr[3]);
  void execCREATE_EVNT_CONF(const NdbApiSignal *, const LinearSectionPtr p[3]);
  void execSUB_START_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execSUB_START_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execSUB_STOP_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execSUB_STOP_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execDROP_EVNT_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execDROP_EVNT_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);

  void execDROP_TABLE_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execDROP_TABLE_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execOLD_LIST_TABLES_CONF(const NdbApiSignal*,
				const LinearSectionPtr ptr[3]);
  void execLIST_TABLES_CONF(const NdbApiSignal*, const LinearSectionPtr pt[3]);

  void execCREATE_FILE_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execCREATE_FILE_CONF(const NdbApiSignal*, const LinearSectionPtr pr[3]);

  void execCREATE_FILEGROUP_REF(const NdbApiSignal*,
				const LinearSectionPtr ptr[3]);
  void execCREATE_FILEGROUP_CONF(const NdbApiSignal*,
				 const LinearSectionPtr ptr[3]);

  void execDROP_FILE_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execDROP_FILE_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);

  void execDROP_FILEGROUP_REF(const NdbApiSignal*, const LinearSectionPtr [3]);
  void execDROP_FILEGROUP_CONF(const NdbApiSignal*,
			       const LinearSectionPtr ptr[3]);

  void execSCHEMA_TRANS_BEGIN_CONF(const NdbApiSignal*,
				   const LinearSectionPtr ptr[3]);
  void execSCHEMA_TRANS_BEGIN_REF(const NdbApiSignal*,
				  const LinearSectionPtr ptr[3]);
  void execSCHEMA_TRANS_END_CONF(const NdbApiSignal*,
				 const LinearSectionPtr ptr[3]);
  void execSCHEMA_TRANS_END_REF(const NdbApiSignal*,
				const LinearSectionPtr ptr[3]);
  void execSCHEMA_TRANS_END_REP(const NdbApiSignal*, const LinearSectionPtr ptr[3]);

  void execWAIT_GCP_CONF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);
  void execWAIT_GCP_REF(const NdbApiSignal*, const LinearSectionPtr ptr[3]);

  void execCREATE_HASH_MAP_REF(const NdbApiSignal*,
			       const LinearSectionPtr ptr[3]);
  void execCREATE_HASH_MAP_CONF(const NdbApiSignal*,
				const LinearSectionPtr ptr[3]);

  Uint32 m_fragmentId;
  UtilBuffer m_buffer;

  Uint32 m_noOfTables;
  UtilBuffer m_tableData;
  UtilBuffer m_tableNames;

  union {
    struct SubStartConfData {
      Uint32 m_buckets;
    } m_sub_start_conf;
    struct WaitGcpData {
      Uint32 gci_hi;
      Uint32 gci_lo;
    } m_wait_gcp_conf;
  } m_data;
};

class NdbDictionaryImpl;
class GlobalCacheInitObject
{
public:
  const BaseString &m_name;
  GlobalCacheInitObject(const BaseString &name) :
    m_name(name)
  {}
  virtual ~GlobalCacheInitObject() {}
  virtual int init(NdbDictionaryImpl *dict, NdbTableImpl &tab) const = 0;
};

class NdbDictionaryImpl : public NdbDictionary::Dictionary {
public:
  NdbDictionaryImpl(Ndb &ndb);
  NdbDictionaryImpl(Ndb &ndb, NdbDictionary::Dictionary & f);
  ~NdbDictionaryImpl();

  bool setTransporter(class Ndb * ndb, class TransporterFacade * tf);
  bool setTransporter(class TransporterFacade * tf);

  int createTable(NdbTableImpl &t, NdbDictObjectImpl &);
  int optimizeTable(const NdbTableImpl &t,
                    NdbOptimizeTableHandleImpl &h);
  int optimizeIndex(const NdbIndexImpl &index,
                    NdbOptimizeIndexHandleImpl &h);
  int createBlobTables(const NdbTableImpl& t);
  bool supportedAlterTable(NdbTableImpl &old_impl, NdbTableImpl &impl);
  int alterTable(NdbTableImpl &old_impl, NdbTableImpl &impl);
  int dropTable(const char * name);
  int dropTable(NdbTableImpl &);
  int dropBlobTables(NdbTableImpl &);
  int renameBlobTables(const NdbTableImpl &old_impl, const NdbTableImpl &impl);
  int invalidateObject(NdbTableImpl &);
  int removeCachedObject(NdbTableImpl &);

  int createIndex(NdbIndexImpl &ix, bool offline);
  int createIndex(NdbIndexImpl &ix, NdbTableImpl & tab, bool offline);
  int dropIndex(const char * indexName, 
		const char * tableName);
  int dropIndex(NdbIndexImpl &, const char * tableName);
  NdbTableImpl * getIndexTable(NdbIndexImpl * index, 
			       NdbTableImpl * table);

  int updateIndexStat(const NdbIndexImpl&, const NdbTableImpl&);
  int updateIndexStat(Uint32 indexId, Uint32 indexVersion, Uint32 tableId);
  int deleteIndexStat(const NdbIndexImpl&, const NdbTableImpl&);
  int deleteIndexStat(Uint32 indexId, Uint32 indexVersion, Uint32 tableId);

  int createEvent(NdbEventImpl &);
  int createBlobEvents(NdbEventImpl &);
  int dropEvent(const char * eventName, int force);
  int dropEvent(const NdbEventImpl &);
  int dropBlobEvents(const NdbEventImpl &);
  int listEvents(List& list);

  int executeSubscribeEvent(NdbEventOperationImpl &, Uint32 & buckets);
  int stopSubscribeEvent(NdbEventOperationImpl &);

  int forceGCPWait(int type);
  int getRestartGCI(Uint32*);

  int listObjects(List& list, NdbDictionary::Object::Type type, 
                  bool fullyQualified);
  int listIndexes(List& list, Uint32 indexId);

  NdbTableImpl * getTableGlobal(const char * tableName);
  NdbIndexImpl * getIndexGlobal(const char * indexName,
                                NdbTableImpl &ndbtab);
  NdbIndexImpl * getIndexGlobal(const char * indexName,
                                const char * tableName);
  int alterTableGlobal(NdbTableImpl &orig_impl, NdbTableImpl &impl);
  int dropTableGlobal(NdbTableImpl &);
  int dropIndexGlobal(NdbIndexImpl & impl);
  int releaseTableGlobal(const NdbTableImpl & impl, int invalidate);
  int releaseIndexGlobal(const NdbIndexImpl & impl, int invalidate);

  NdbTableImpl * getTable(const char * tableName, void **data= 0);
  NdbTableImpl * getBlobTable(const NdbTableImpl&, uint col_no);
  NdbTableImpl * getBlobTable(uint tab_id, uint col_no);
  void putTable(NdbTableImpl *impl);
  int getBlobTables(NdbTableImpl &);
  Ndb_local_table_info*
    get_local_table_info(const BaseString& internalTableName);
  NdbIndexImpl * getIndex(const char * indexName,
			  const char * tableName);
  NdbIndexImpl * getIndex(const char * indexName, const NdbTableImpl& prim);
  NdbEventImpl * getEvent(const char * eventName, NdbTableImpl* = NULL);
  NdbEventImpl * getBlobEvent(const NdbEventImpl& ev, uint col_no);
  NdbEventImpl * getEventImpl(const char * internalName);

  int createDatafile(const NdbDatafileImpl &, bool force, NdbDictObjectImpl*);
  int dropDatafile(const NdbDatafileImpl &);
  int createUndofile(const NdbUndofileImpl &, bool force, NdbDictObjectImpl*);
  int dropUndofile(const NdbUndofileImpl &);

  int createTablespace(const NdbTablespaceImpl &, NdbDictObjectImpl*);
  int dropTablespace(const NdbTablespaceImpl &);

  int createLogfileGroup(const NdbLogfileGroupImpl &, NdbDictObjectImpl*);
  int dropLogfileGroup(const NdbLogfileGroupImpl &);

  int beginSchemaTrans(bool retry711 = true);
  int endSchemaTrans(Uint32 flags);
  bool hasSchemaTrans() const
    { return (m_tx.m_state == NdbDictInterface::Tx::Started); }
  NdbDictInterface::Tx m_tx;

  const NdbError & getNdbError() const;
  NdbError m_error;
  int m_warn;
  Uint32 m_local_table_data_size;

  LocalDictCache m_localHash;
  GlobalDictCache * m_globalHash;

  static NdbDictionaryImpl & getImpl(NdbDictionary::Dictionary & t);
  static const NdbDictionaryImpl & getImpl(const NdbDictionary::Dictionary &t);
  NdbDictionary::Dictionary * m_facade;
  int initialiseColumnData(bool isIndex,
                           Uint32 flags,
                           const NdbDictionary::RecordSpecification *recSpec,
                           Uint32 colNum,
                           NdbRecord *rec);

  NdbDictInterface m_receiver;
  Ndb & m_ndb;

  NdbIndexImpl* getIndexImpl(const char * externalName,
                             const BaseString& internalName,
                             NdbTableImpl &tab,
                             NdbTableImpl &prim);
  NdbIndexImpl * getIndexImpl(const char * name,
                              const BaseString& internalName);


  int createDefaultNdbRecord(NdbTableImpl* tableOrIndex,
                             const NdbTableImpl* baseTableForIndex);

  bool validateRecordSpec(const NdbDictionary::RecordSpecification *recSpec,
                          Uint32 length,
                          Uint32 flags);

  NdbRecord *createRecord(const NdbTableImpl *table,
                          const NdbDictionary::RecordSpecification *recSpec,
                          Uint32 length,
                          Uint32 elemSize,
                          Uint32 flags,
                          bool defaultRecord);
  void releaseRecord_impl(NdbRecord *rec);

  static NdbDictionary::RecordType 
  getRecordType(const NdbRecord* record);
  static const char* getRecordTableName(const NdbRecord* record);
  static const char* getRecordIndexName(const NdbRecord* record);
  static bool getNextAttrIdFrom(const NdbRecord* record,
                                Uint32 startAttrId,
                                Uint32& nextAttrId);
  static bool getOffset(const NdbRecord* record,
                        Uint32 attrId,
                        Uint32& offset);
  static bool getNullBitOffset(const NdbRecord* record,
                               Uint32 attrId,
                               Uint32& nullbit_byte_offset,
                               Uint32& nullbit_bit_in_byte);
  static const char* getValuePtr(const NdbRecord* record,
                                 const char* row,
                                 Uint32 attrId);
  static char* getValuePtr(const NdbRecord* record,
                           char* row,
                           Uint32 attrId);
  static bool isNull(const NdbRecord* record,
                     const char* row,
                     Uint32 attrId);
  static int setNull(const NdbRecord* record,
                     char* row,
                     Uint32 attrId,
                     bool value);
  static Uint32 getRecordRowLength(const NdbRecord* record);

  /* Empty NdbRecord column mask for user convenience */
  static const Uint32 m_emptyMask[MAXNROFATTRIBUTESINWORDS];

private:
  NdbTableImpl * fetchGlobalTableImplRef(const GlobalCacheInitObject &obj);
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
int
NdbColumnImpl::getBlobVersion() const {
  return m_blobVersion;
}

inline
void
NdbColumnImpl::setBlobVersion(int blobVersion) {
  if (blobVersion == NDB_BLOB_V1) {
    m_arrayType = NDB_ARRAYTYPE_FIXED;
  } else if (blobVersion == NDB_BLOB_V2) {
    // always 2 length bytes for head+inline
    m_arrayType = NDB_ARRAYTYPE_MEDIUM_VAR;
  }
  // invalid value should be detected at validate
  m_blobVersion = blobVersion;
}

inline
bool
NdbColumnImpl::get_var_length(const void* value, Uint32& len) const
{
  DBUG_ENTER("NdbColumnImpl::get_var_length");
  Uint32 max_len = m_attrSize * m_arraySize;
  switch (m_arrayType) {
  case NDB_ARRAYTYPE_SHORT_VAR:
    len = 1 + *((Uint8*)value);
    DBUG_PRINT("info", ("SHORT_VAR: len=%u max_len=%u", len, max_len));
    break;
  case NDB_ARRAYTYPE_MEDIUM_VAR:
    len = 2 + uint2korr((char*)value);
    DBUG_PRINT("info", ("MEDIUM_VAR: len=%u max_len=%u", len, max_len));
    break;
  default:
    len = max_len;
    DBUG_PRINT("info", ("FIXED: len=%u max_len=%u", len, max_len));
    DBUG_RETURN(true);
  }
  DBUG_RETURN(len <= max_len);
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
bool
NdbTableImpl::matchDb(const char * name, size_t len) const
{
  return 
    len < m_internalName.length() &&
    memcmp(name, m_internalName.c_str(), len) == 0;
}

inline
Uint32
Hash( const char* str ){
  Uint32 h = 0;
  size_t len = strlen(str);
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

class InitTable : public GlobalCacheInitObject
{
public:
  InitTable(const BaseString &name) :
    GlobalCacheInitObject(name)
  {}
  int init(NdbDictionaryImpl *dict, NdbTableImpl &tab) const
  {
    int res= dict->getBlobTables(tab);
    if (res == 0)
      res= dict->createDefaultNdbRecord(&tab, NULL);
    
    return res;
  }
};

inline
NdbTableImpl *
NdbDictionaryImpl::getTableGlobal(const char * table_name)
{
  if (unlikely(strchr(table_name, '$') != 0)) {
    if (is_ndb_blob_table(table_name)) 
    {
      /* Could attempt to get the Blob table here, but
       * instead we will generate an error.
       * The non-global getTable() calls can fetch Blob
       * tables correctly if necessary.
       *
       * 4307 Invalid Table name
       */
      m_error.code = 4307;
      return NULL;
    }
  }

  const BaseString internal_tabname(m_ndb.internalize_table_name(table_name));
  return fetchGlobalTableImplRef(InitTable(internal_tabname));
}

inline
NdbTableImpl *
NdbDictionaryImpl::getTable(const char * table_name, void **data)
{
  DBUG_ENTER("NdbDictionaryImpl::getTable");
  DBUG_PRINT("enter", ("table: %s", table_name));

  if (unlikely(strchr(table_name, '$') != 0)) {
    Uint32 tab_id, col_no;
    if (is_ndb_blob_table(table_name, &tab_id, &col_no)) {
      NdbTableImpl* t = getBlobTable(tab_id, col_no);
      DBUG_RETURN(t);
    }
  }

  const BaseString internal_tabname(m_ndb.internalize_table_name(table_name));
  Ndb_local_table_info *info=
    get_local_table_info(internal_tabname);
  if (info == 0)
    DBUG_RETURN(0);
  if (data)
    *data= info->m_local_data;
  DBUG_RETURN(info->m_table_impl);
}

inline
Ndb_local_table_info * 
NdbDictionaryImpl::get_local_table_info(const BaseString& internalTableName)
{
  DBUG_ENTER("NdbDictionaryImpl::get_local_table_info");
  DBUG_PRINT("enter", ("table: %s", internalTableName.c_str()));

  Ndb_local_table_info *info= m_localHash.get(internalTableName.c_str());
  if (info == 0)
  {
    NdbTableImpl *tab=
      fetchGlobalTableImplRef(InitTable(internalTableName));
    if (tab)
    {
      info= Ndb_local_table_info::create(tab, m_local_table_data_size);
      if (info)
      {
        m_localHash.put(internalTableName.c_str(), info);
      }
    }
  }
  DBUG_RETURN(info); // autoincrement already initialized
}

class InitIndex : public GlobalCacheInitObject
{
public:
  const char *m_index_name;
  const NdbTableImpl &m_prim;

  InitIndex(const BaseString &internal_indexname,
	    const char *index_name,
	    const NdbTableImpl &prim) :
    GlobalCacheInitObject(internal_indexname),
    m_index_name(index_name),
    m_prim(prim)
    {}
  
  int init(NdbDictionaryImpl *dict, NdbTableImpl &tab) const {
    DBUG_ENTER("InitIndex::init");
    DBUG_ASSERT(tab.m_indexType != NdbDictionary::Object::TypeUndefined);
    /**
     * Create index impl
     */
    NdbIndexImpl* idx;
    if(NdbDictInterface::create_index_obj_from_table(&idx, &tab, &m_prim) == 0)
    {
      idx->m_table = &tab;
      if (!idx->m_externalName.assign(m_index_name) ||
          !idx->m_internalName.assign(m_name))
        DBUG_RETURN(4000);
      tab.m_index = idx;

      /* Finally, create default NdbRecord for this index */
      DBUG_RETURN(dict->createDefaultNdbRecord(&tab, &m_prim));
    }
    DBUG_RETURN(1);
  }
};

inline
NdbIndexImpl * 
NdbDictionaryImpl::getIndexGlobal(const char * index_name,
                                  NdbTableImpl &ndbtab)
{
  DBUG_ENTER("NdbDictionaryImpl::getIndexGlobal");
  const BaseString
    internal_indexname(m_ndb.internalize_index_name(&ndbtab, index_name));
  int retry= 2;

  while (retry)
  {
    NdbTableImpl *tab=
      fetchGlobalTableImplRef(InitIndex(internal_indexname,
					index_name, ndbtab));
    if (tab)
    {
      // tab->m_index sould be set. otherwise tab == 0
      NdbIndexImpl *idx= tab->m_index;
      if (idx->m_table_id != (unsigned)ndbtab.getObjectId() ||
          idx->m_table_version != (unsigned)ndbtab.getObjectVersion())
      {
        releaseIndexGlobal(*idx, 1);
        retry--;
        continue;
      }
      DBUG_RETURN(idx);
    }
    break;
  }
  {
    // Index not found, try old format
    const BaseString
      old_internal_indexname(m_ndb.old_internalize_index_name(&ndbtab, 
							      index_name));
    retry= 2;
    while (retry)
    {
      NdbTableImpl *tab=
	fetchGlobalTableImplRef(InitIndex(old_internal_indexname,
					  index_name, ndbtab));
      if (tab)
      {
	// tab->m_index sould be set. otherwise tab == 0
	NdbIndexImpl *idx= tab->m_index;
	if (idx->m_table_id != (unsigned)ndbtab.getObjectId() ||
	    idx->m_table_version != (unsigned)ndbtab.getObjectVersion())
	{
	  releaseIndexGlobal(*idx, 1);
	  retry--;
	  continue;
	}
	DBUG_RETURN(idx);
      }
      break;
    }
  }
  m_error.code= 4243;
  DBUG_RETURN(0);
}

inline
NdbIndexImpl * 
NdbDictionaryImpl::getIndexGlobal(const char * indexName,
                                  const char * tableName)
{
  DBUG_ENTER("NdbDictionaryImpl::getIndexGlobal");
  NdbTableImpl * t = getTableGlobal(tableName);
  if(t == NULL)
    DBUG_RETURN(0);
  DBUG_RETURN(getIndexGlobal(indexName, *t));
}

inline int
NdbDictionaryImpl::releaseTableGlobal(const NdbTableImpl & impl, int invalidate)
{
  DBUG_ENTER("NdbDictionaryImpl::releaseTableGlobal");
  DBUG_PRINT("enter", ("internal_name: %s", impl.m_internalName.c_str()));
  m_globalHash->lock();
  m_globalHash->release(&impl, invalidate);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

inline int
NdbDictionaryImpl::releaseIndexGlobal(const NdbIndexImpl & impl, int invalidate)
{
  DBUG_ENTER("NdbDictionaryImpl::releaseIndexGlobal");
  DBUG_PRINT("enter", ("internal_name: %s", impl.m_internalName.c_str()));
  m_globalHash->lock();
  m_globalHash->release(impl.m_table, invalidate);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

inline
NdbIndexImpl * 
NdbDictionaryImpl::getIndex(const char * index_name,
			    const char * table_name)
{
  if (table_name == 0)
  {
    assert(0);
    m_error.code= 4243;
    return 0;
  }
  
  
  NdbTableImpl* prim = getTable(table_name);
  if (prim == 0)
  {
    m_error.code= 4243;
    return 0;
  }

  return getIndex(index_name, *prim);
}

inline
NdbIndexImpl * 
NdbDictionaryImpl::getIndex(const char* index_name,
			    const NdbTableImpl& prim)
{

  const BaseString
    internal_indexname(m_ndb.internalize_index_name(&prim, index_name));

  Ndb_local_table_info *info= m_localHash.get(internal_indexname.c_str());
  NdbTableImpl *tab;
  if (info == 0)
  {
    tab= fetchGlobalTableImplRef(InitIndex(internal_indexname,
					   index_name,
					   prim));
    if (!tab)
      goto retry;

    info= Ndb_local_table_info::create(tab, 0);
    if (!info)
      goto retry;
    m_localHash.put(internal_indexname.c_str(), info);
  }
  else
    tab= info->m_table_impl;
  
  return tab->m_index;

retry:
  // Index not found, try fetching it from current database
  const BaseString
    old_internal_indexname(m_ndb.old_internalize_index_name(&prim, index_name));

  info= m_localHash.get(old_internal_indexname.c_str());
  if (info == 0)
  {
    tab= fetchGlobalTableImplRef(InitIndex(old_internal_indexname,
					   index_name,
					   prim));
    if (!tab)
      goto err;
    
    info= Ndb_local_table_info::create(tab, 0);
    if (!info)
      goto err;
    m_localHash.put(old_internal_indexname.c_str(), info);
  }
  else
    tab= info->m_table_impl;
  
  return tab->m_index;
  
err:
  m_error.code= 4243;
  return 0;
}

inline
NdbTablespaceImpl &
NdbTablespaceImpl::getImpl(NdbDictionary::Tablespace & t){
  return t.m_impl;
}

inline
const NdbTablespaceImpl &
NdbTablespaceImpl::getImpl(const NdbDictionary::Tablespace & t){
  return t.m_impl;
}

inline
NdbLogfileGroupImpl &
NdbLogfileGroupImpl::getImpl(NdbDictionary::LogfileGroup & t){
  return t.m_impl;
}

inline
const NdbLogfileGroupImpl &
NdbLogfileGroupImpl::getImpl(const NdbDictionary::LogfileGroup & t){
  return t.m_impl;
}

inline
NdbDatafileImpl &
NdbDatafileImpl::getImpl(NdbDictionary::Datafile & t){
  return t.m_impl;
}

inline
const NdbDatafileImpl &
NdbDatafileImpl::getImpl(const NdbDictionary::Datafile & t){
  return t.m_impl;
}

inline
NdbUndofileImpl &
NdbUndofileImpl::getImpl(NdbDictionary::Undofile & t){
  return t.m_impl;
}

inline
const NdbUndofileImpl &
NdbUndofileImpl::getImpl(const NdbDictionary::Undofile & t){
  return t.m_impl;
}

#endif
