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

#ifndef NdbDictionaryImpl_H
#define NdbDictionaryImpl_H

#include <ndb_types.h>
#include <kernel_types.h>
#include <NdbError.hpp>
#include <BaseString.hpp>
#include <Vector.hpp>
#include <UtilBuffer.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbDictionary.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <Ndb.hpp>
#include "NdbWaiter.hpp"
#include "DictCache.hpp"

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
  BaseString m_defaultValue;
  NdbTableImpl * m_blobTable;

  /**
   * Internal types and sizes, and aggregates
   */
  Uint32 m_attrSize;            // element size (size when arraySize==1)
  Uint32 m_arraySize;           // length or maxlength+1/2 for Var* types
  Uint32 m_arrayType;           // NDB_ARRAYTYPE_FIXED or _VAR
  Uint32 m_storageType;         // NDB_STORAGETYPE_MEMORY or _DISK
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

  /**
   * Equality/assign
   */
  bool equal(const NdbColumnImpl&) const;

  static NdbColumnImpl & getImpl(NdbDictionary::Column & t);
  static const NdbColumnImpl & getImpl(const NdbDictionary::Column & t);
  NdbDictionary::Column * m_facade;

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
  void setName(const char * name);
  const char * getName() const;
  void setFragmentCount(Uint32 count);
  Uint32 getFragmentCount() const;
  void setFrm(const void* data, Uint32 len);
  const void * getFrmData() const;
  Uint32 getFrmLength() const;
  void setFragmentData(const void* data, Uint32 len);
  const void * getFragmentData() const;
  Uint32 getFragmentDataLen() const;
  void setTablespaceNames(const void* data, Uint32 len);
  Uint32 getTablespaceNamesLen() const;
  const void * getTablespaceNames() const;
  void setTablespaceData(const void* data, Uint32 len);
  const void * getTablespaceData() const;
  Uint32 getTablespaceDataLen() const;
  void setRangeListData(const void* data, Uint32 len);
  const void * getRangeListData() const;
  Uint32 getRangeListDataLen() const;

  const char * getMysqlName() const;
  void updateMysqlName();

  int aggregate(NdbError& error);
  int validate(NdbError& error);

  Uint32 m_changeMask;
  Uint32 m_primaryTableId;
  BaseString m_internalName;
  BaseString m_externalName;
  BaseString m_mysqlName;
  BaseString m_newExternalName; // Used for alter table
  UtilBuffer m_frm; 
  UtilBuffer m_newFrm;       // Used for alter table
  UtilBuffer m_ts_name;      //Tablespace Names
  UtilBuffer m_new_ts_name;  //Tablespace Names
  UtilBuffer m_ts;           //TablespaceData
  UtilBuffer m_new_ts;       //TablespaceData
  UtilBuffer m_fd;           //FragmentData
  UtilBuffer m_new_fd;       //FragmentData
  UtilBuffer m_range;        //Range Or List Array
  UtilBuffer m_new_range;    //Range Or List Array
  NdbDictionary::Object::FragmentType m_fragmentType;

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
  void computeAggregates();
  void buildColumnHash(); 

  /**
   * Fragment info
   */
  Uint32 m_hashValueMask;
  Uint32 m_hashpointerValue;
  Vector<Uint16> m_fragments;

  Uint64 m_max_rows;
  Uint64 m_min_rows;
  Uint32 m_default_no_part_flag;
  bool m_linear_flag;
  bool m_logging;
  bool m_temporary;
  bool m_row_gci;
  bool m_row_checksum;
  int m_kvalue;
  int m_minLoadFactor;
  int m_maxLoadFactor;
  Uint16 m_keyLenInWords;
  Uint16 m_fragmentCount;

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

  /**
   * Disk stuff
   */
  BaseString m_tablespace_name;
  Uint32 m_tablespace_id;
  Uint32 m_tablespace_version;
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
  void setName(const char * name);
  const char * getName() const;
  void setTable(const NdbDictionary::Table& table);
  const NdbDictionary::Table * getTable() const;
  void setTable(const char * table);
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

  void assign(const NdbTablespaceImpl&);

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

  void assign(const NdbLogfileGroupImpl&);

  static NdbLogfileGroupImpl & getImpl(NdbDictionary::LogfileGroup & t);
  static const NdbLogfileGroupImpl& getImpl(const 
					    NdbDictionary::LogfileGroup&);
  NdbDictionary::LogfileGroup * m_facade;
};

struct NdbFileImpl : public NdbDictObjectImpl {
  NdbFileImpl(NdbDictionary::Object::Type t);

  Uint64 m_size;
  Uint32 m_free;
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

  void assign(const NdbDatafileImpl&);

  static NdbDatafileImpl & getImpl(NdbDictionary::Datafile & t);
  static const NdbDatafileImpl & getImpl(const NdbDictionary::Datafile & t);
  NdbDictionary::Datafile * m_facade;
};

class NdbUndofileImpl : public NdbDictionary::Undofile, public NdbFileImpl {
public:
  NdbUndofileImpl();
  NdbUndofileImpl(NdbDictionary::Undofile &);
  ~NdbUndofileImpl();

  void assign(const NdbUndofileImpl&);

  static NdbUndofileImpl & getImpl(NdbDictionary::Undofile & t);
  static const NdbUndofileImpl & getImpl(const NdbDictionary::Undofile & t);
  NdbDictionary::Undofile * m_facade;
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
  int dictSignal(NdbApiSignal* signal, LinearSectionPtr ptr[3], int secs,
		 int nodeId, // -1 any, 0 = master, >1 = specified
		 WaitSignalType wst,
		 int timeout, Uint32 RETRIES,
		 const int *errcodes = 0, int temporaryMask = 0);

  int createOrAlterTable(class Ndb & ndb, NdbTableImpl &, bool alter);

  int createTable(class Ndb & ndb, NdbTableImpl &);
  int alterTable(class Ndb & ndb, NdbTableImpl &);
  int dropTable(const NdbTableImpl &);

  int createIndex(class Ndb & ndb, const NdbIndexImpl &, const NdbTableImpl &);
  int dropIndex(const NdbIndexImpl &, const NdbTableImpl &);
  
  int createEvent(class Ndb & ndb, NdbEventImpl &, int getFlag);
  int dropEvent(const NdbEventImpl &);
  int dropEvent(NdbApiSignal* signal, LinearSectionPtr ptr[3], int noLSP);

  int executeSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &);
  int stopSubscribeEvent(class Ndb & ndb, NdbEventOperationImpl &);
  
  int listObjects(NdbDictionary::Dictionary::List& list, Uint32 requestData, bool fullyQualifiedNames);
  int listObjects(NdbApiSignal* signal);
  
  NdbTableImpl * getTable(int tableId, bool fullyQualifiedNames);
  NdbTableImpl * getTable(const BaseString& name, bool fullyQualifiedNames);
  NdbTableImpl * getTable(class NdbApiSignal * signal, 
			  LinearSectionPtr ptr[3],
			  Uint32 noOfSections, bool fullyQualifiedNames);

  int forceGCPWait();

  static int parseTableInfo(NdbTableImpl ** dst, 
			    const Uint32 * data, Uint32 len,
			    bool fullyQualifiedNames,
                            Uint32 version= 0xFFFFFFFF);

  static int parseFileInfo(NdbFileImpl &dst,
			   const Uint32 * data, Uint32 len);

  static int parseFilegroupInfo(NdbFilegroupImpl &dst,
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
  
  const NdbError &getNdbError() const;  
  NdbError & m_error;
private:
  Uint32 m_reference;
  Uint32 m_masterNodeId;
  
  NdbWaiter m_waiter;
  class TransporterFacade * m_transporter;
  
  friend class Ndb;
  friend class NdbDictionaryImpl;
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

  void execCREATE_FILE_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_FILE_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  
  void execCREATE_FILEGROUP_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execCREATE_FILEGROUP_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  void execDROP_FILE_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_FILE_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  
  void execDROP_FILEGROUP_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execDROP_FILEGROUP_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  
  void execWAIT_GCP_CONF(NdbApiSignal *, LinearSectionPtr ptr[3]);
  void execWAIT_GCP_REF(NdbApiSignal *, LinearSectionPtr ptr[3]);

  Uint32 m_fragmentId;
  UtilBuffer m_buffer;
};

class NdbDictionaryImpl;
class GlobalCacheInitObject
{
public:
  NdbDictionaryImpl *m_dict;
  const BaseString &m_name;
  GlobalCacheInitObject(NdbDictionaryImpl *dict,
                        const BaseString &name) :
    m_dict(dict),
    m_name(name)
  {}
  virtual ~GlobalCacheInitObject() {}
  virtual int init(NdbTableImpl &tab) const = 0;
};

class NdbRecord {
public:
  /* Flag bits for the entire NdbRecord. */
  enum RecFlags
  {
    /*
      This flag tells whether this NdbRecord is a PK record for the table,
      ie. that it describes _exactly_ the primary key attributes, no more and
      no less. This is a requirement for the PK record used in read/update.
    */
    RecIsKeyRecord= 0x1,

    /*
      This flag tells whether this NdbRecord includes _at least_ all PK columns
      (and possibly other columns), which is a requirement for insert.
    */
    RecHasAllKeys= 0x2,

    /* This NdbRecord is for an ordered index, not a table. */
    RecIsIndex= 0x4,

    /* This NdbRecord has at least one blob. */
    RecHasBlob= 0x8,

    /*
      The table has at least one blob (though the NdbRecord may not include
      it). This is needed so that deleteTuple() can know to delete all blob
      parts.
    */
    RecTableHasBlob= 0x10
  };

  /* Flag bits for individual columns in the NdbRecord. */
  enum ColFlags
  {
    /*
      This flag tells whether the column is part of the primary key, used
      for insert.
    */
    IsKey=   0x1,
    /* This flag is true if column is disk based. */
    IsDisk= 0x2,
    /* True if column can be NULL and has a NULL bit. */
    IsNullable= 0x04,
    /*
      Flags for determining the actual length of data (which for varsize
      columns is different from the maximum size.
      The flags are mutually exclusive.
    */
    IsVar1ByteLen= 0x08,
    IsVar2ByteLen= 0x10,
    /* Flag for column that is a part of the distribution key. */
    IsDistributionKey= 0x20,
    /* Flag for blob columns. */
    IsBlob= 0x40,
    /* 
       Flag for special handling of short varchar for index keys, which is
       used by mysqld to avoid converting index key rows.
    */
    IsMysqldShrinkVarchar= 0x80,
    /* Bitfield stored in the internal mysqld format. */
    IsMysqldBitfield= 0x100
  };

  struct Attr
  {
    Uint32 attrId;
    Uint32 column_no;
    /*
      The index_attrId member is the attribute id in the index table object,
      which is used to specify ordered index bounds in KEYINFO signal.
      Note that this is different from the normal attribute id in the main
      table, unless the ordered index is on columns (0..N).
    */
    Uint32 index_attrId;
    /* Offset of data from the start of a row. */
    Uint32 offset;
    /*
      Maximum size of the attribute. This is duplicated here to avoid having
      to dig into Table object for every attribute fetch/store.
    */
    Uint32 maxSize;
    /* Number of bits in a bitfield. */
    Uint32 bitCount;

    /* Flags, or-ed from enum ColFlags. */
    Uint32 flags;

    /* Character set information, for ordered index merge sort. */
    CHARSET_INFO *charset_info;
    /* Function used to compare attributes during merge sort. */
    NdbSqlUtil::Cmp *compare_function;


    /* NULL bit location (only for nullable columns, ie. flags&IsNullable). */
    Uint32 nullbit_byte_offset;
    Uint32 nullbit_bit_in_byte;

    bool get_var_length(const char *row, Uint32& len) const
    {
      if (flags & IsVar1ByteLen)
        len= 1 + *((Uint8*)(row+offset));
      else if (flags & IsVar2ByteLen)
        len= 2 + uint2korr(row+offset);
      else
        len= maxSize;
      return len <= maxSize;
    }
    bool is_null(const char *row) const
    {
      return (flags & IsNullable) &&
             (row[nullbit_byte_offset] & (1 << nullbit_bit_in_byte));
    }
    /*
      Mysqld uses a slightly different format for storing varchar in
      index keys; the length is always two bytes little endian, even
      for max size < 256.
      This converts to the usual format expected by NDB kernel.
    */
    bool shrink_varchar(const char *row, Uint32& out_len, char *buf) const
    {
      const char *p= row + offset;
      Uint32 len= uint2korr(p);
      if (len >= 256 || len >= maxSize)
        return false;
      buf[0]= (unsigned char)len;
      memcpy(buf+1, p+2, len);
      out_len= len + 1;
      return true;
    }
    /*
      Accessing mysqld format bitfields.
      For internal use in myqsld.
      In mysqld, fractional bytes of each bit field are stored inside the
      null bytes area.
    */
    void get_mysqld_bitfield(const char *src_row, char *dst_buffer) const;
    void put_mysqld_bitfield(char *dst_row, const char *src_buffer) const;
  };

  /*
    ToDo: For now we need to hang on to the Table *, since lots of the
    existing code (class NdbOperation*, class NdbScanFilter) depends
    on having access to it.
    Long-term, we want to eliminate it (instead relying only on copying
    tableId, fragmentCount etc. into the NdbRecord.
  */
  const NdbTableImpl *table;
  const NdbTableImpl *base_table;

  Uint32 tableId;
  Uint32 tableVersion;
  /* Copy of table->m_keyLenInWords. */
  Uint32 m_keyLenInWords;
  /* Total maximum size of TRANSID_AI data (for computing batch size). */
  Uint32 m_max_transid_ai_bytes;
  /* Number of distribution keys (usually == number of primary keys). */
  Uint32 m_no_of_distribution_keys;
  /* Flags, or-ed from enum RecFlags. */
  Uint32 flags;
  /* Size of row (really end of right-most defined attribute in row). */
  Uint32 m_row_size;

  /*
    Array of index (into columns[]) of primary key columns, in order.
    Physical storage for these is after columns[] array.
    This array is only fully initialised if flags&RecHasAllKeys.
  */
  const Uint32 *key_indexes;
  /* Length of key_indexes array. */
  Uint32 key_index_length;
  /*
    Array of index (into columns[]) of distribution keys, in attrId order.
    This is used to build the distribution key, which is the concatenation
    of key values in attrId order.
  */
  const Uint32 *distkey_indexes;
  /* Length of distkey_indexes array. */
  Uint32 distkey_index_length;

  /*
    m_min_distkey_prefix_length is the minimum lenght of an index prefix
    needed to include all distribution keys. In other words, it is one more
    that the index of the last distribution key in the index order.
    If the index does not include all distribution keys, it is set to 0.
    This member is only valid for an index NdbRecord.
  */
  Uint32 m_min_distkey_prefix_length;
  /* The real size of the array at the end of this struct. */
  Uint32 noOfColumns;
  struct Attr columns[1];

  /* Copy a user-supplied mask to internal mask. */
  void copyMask(Uint32 *dst, const unsigned char *src) const;

  /* Clear internal mask. */
  void clearMask(Uint32 *dst) const
  {
    BitmaskImpl::clear((NDB_MAX_ATTRIBUTES_IN_TABLE+31)>>5, dst);
  }
};


class NdbDictionaryImpl : public NdbDictionary::Dictionary {
public:
  NdbDictionaryImpl(Ndb &ndb);
  NdbDictionaryImpl(Ndb &ndb, NdbDictionary::Dictionary & f);
  ~NdbDictionaryImpl();

  bool setTransporter(class Ndb * ndb, class TransporterFacade * tf);
  bool setTransporter(class TransporterFacade * tf);

  int createTable(NdbTableImpl &t);
  int createBlobTables(NdbTableImpl& org, NdbTableImpl& created);
  int alterTable(NdbTableImpl &t);
  int dropTable(const char * name);
  int dropTable(NdbTableImpl &);
  int dropBlobTables(NdbTableImpl &);
  int invalidateObject(NdbTableImpl &);
  int removeCachedObject(NdbTableImpl &);

  int createIndex(NdbIndexImpl &ix);
  int createIndex(NdbIndexImpl &ix, NdbTableImpl & tab);
  int dropIndex(const char * indexName, 
		const char * tableName);
  int dropIndex(NdbIndexImpl &, const char * tableName);
  NdbTableImpl * getIndexTable(NdbIndexImpl * index, 
			       NdbTableImpl * table);

  int createEvent(NdbEventImpl &);
  int createBlobEvents(NdbEventImpl &);
  int dropEvent(const char * eventName);
  int dropEvent(const NdbEventImpl &);
  int dropBlobEvents(const NdbEventImpl &);

  int executeSubscribeEvent(NdbEventOperationImpl &);
  int stopSubscribeEvent(NdbEventOperationImpl &);

  int forceGCPWait();

  int listObjects(List& list, NdbDictionary::Object::Type type);
  int listIndexes(List& list, Uint32 indexId);

  NdbTableImpl * getTableGlobal(const char * tableName);
  NdbIndexImpl * getIndexGlobal(const char * indexName,
                                NdbTableImpl &ndbtab);
  int alterTableGlobal(NdbTableImpl &orig_impl, NdbTableImpl &impl);
  int dropTableGlobal(NdbTableImpl &);
  int dropIndexGlobal(NdbIndexImpl & impl);
  int releaseTableGlobal(NdbTableImpl & impl, int invalidate);
  int releaseIndexGlobal(NdbIndexImpl & impl, int invalidate);

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

  NdbIndexImpl* getIndexImpl(const char * externalName,
                             const BaseString& internalName,
                             NdbTableImpl &tab,
                             NdbTableImpl &prim);
  NdbIndexImpl * getIndexImpl(const char * name,
                              const BaseString& internalName);


  NdbRecord *createRecord(const NdbTableImpl *table,
                          const NdbDictionary::RecordSpecification *recSpec,
                          Uint32 length,
                          Uint32 elemSize,
                          Uint32 flags,
                          const NdbTableImpl *base_table= 0);
  NdbRecord *createRecord(const NdbIndexImpl *index,
                          const NdbTableImpl *base_table,
                          const NdbDictionary::RecordSpecification *recSpec,
                          Uint32 length,
                          Uint32 elemSize,
                          Uint32 flags);
  void releaseRecord_impl(NdbRecord *rec);

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
bool
NdbColumnImpl::get_var_length(const void* value, Uint32& len) const
{
  Uint32 max_len = m_attrSize * m_arraySize;
  switch (m_arrayType) {
  case NDB_ARRAYTYPE_SHORT_VAR:
    len = 1 + *((Uint8*)value);
    break;
  case NDB_ARRAYTYPE_MEDIUM_VAR:
    len = 2 + uint2korr((char*)value);
    break;
  default:
    len = max_len;
    return true;
  }
  return (len <= max_len);
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

class InitTable : public GlobalCacheInitObject
{
public:
  InitTable(NdbDictionaryImpl *dict,
            const BaseString &name) :
    GlobalCacheInitObject(dict, name)
  {}
  int init(NdbTableImpl &tab) const
  {
    return m_dict->getBlobTables(tab);
  }
};

inline
NdbTableImpl *
NdbDictionaryImpl::getTableGlobal(const char * table_name)
{
  const BaseString internal_tabname(m_ndb.internalize_table_name(table_name));
  return fetchGlobalTableImplRef(InitTable(this, internal_tabname));
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
      fetchGlobalTableImplRef(InitTable(this, internalTableName));
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
    GlobalCacheInitObject(0, internal_indexname),
    m_index_name(index_name),
    m_prim(prim)
    {}
  
  int init(NdbTableImpl &tab) const {
    DBUG_ENTER("InitIndex::init");
    DBUG_ASSERT(tab.m_indexType != NdbDictionary::Object::TypeUndefined);
    /**
     * Create index impl
     */
    NdbIndexImpl* idx;
    if(NdbDictInterface::create_index_obj_from_table(&idx, &tab, &m_prim) == 0)
    {
      idx->m_table = &tab;
      idx->m_externalName.assign(m_index_name);
      idx->m_internalName.assign(m_name);
      tab.m_index = idx;
      DBUG_RETURN(0);
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

inline int
NdbDictionaryImpl::releaseTableGlobal(NdbTableImpl & impl, int invalidate)
{
  DBUG_ENTER("NdbDictionaryImpl::releaseTableGlobal");
  DBUG_PRINT("enter", ("internal_name: %s", impl.m_internalName.c_str()));
  m_globalHash->lock();
  m_globalHash->release(&impl, invalidate);
  m_globalHash->unlock();
  DBUG_RETURN(0);
}

inline int
NdbDictionaryImpl::releaseIndexGlobal(NdbIndexImpl & impl, int invalidate)
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
