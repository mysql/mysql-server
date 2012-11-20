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

#ifndef DBDICT_H
#define DBDICT_H

/**
 * Dict : Dictionary Block
 */
#include <ndb_limits.h>
#include <trigger_definitions.h>
#include <pc.hpp>
#include <DLHashTable.hpp>
#include <DLFifoList.hpp>
#include <CArray.hpp>
#include <KeyTable.hpp>
#include <KeyTable2.hpp>
#include <KeyTable2Ref.hpp>
#include <SimulatedBlock.hpp>
#include <SimpleProperties.hpp>
#include <SignalCounter.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <signaldata/GetTableId.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/DropTable.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/ListTables.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/AlterIndx.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/CreateEvnt.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include <signaldata/DictLock.hpp>
#include <signaldata/SumaImpl.hpp>
#include "SchemaFile.hpp"
#include <blocks/mutexes.hpp>
#include <SafeCounter.hpp>
#include <RequestTracker.hpp>
#include <Rope.hpp>
#include <signaldata/DictObjOp.hpp>
#include <signaldata/DropFilegroupImpl.hpp>
#include <SLList.hpp>
#include <LockQueue.hpp>

#ifdef DBDICT_C
// Debug Macros

/*--------------------------------------------------------------*/
// Constants for CONTINUEB
/*--------------------------------------------------------------*/
#define ZPACK_TABLE_INTO_PAGES 0
#define ZSEND_GET_TAB_RESPONSE 3
#define ZDROP_TAB_WAIT_GCI     4

/*--------------------------------------------------------------*/
// Other constants in alphabetical order
/*--------------------------------------------------------------*/
#define ZNOMOREPHASES 255

/*--------------------------------------------------------------*/
// Schema file defines
/*--------------------------------------------------------------*/
#define ZSCHEMA_WORDS 4

/*--------------------------------------------------------------*/
// Page constants
/*--------------------------------------------------------------*/
#define ZBAT_SCHEMA_FILE 0 //Variable number of page for NDBFS
#define ZBAT_TABLE_FILE 1 //Variable number of page for NDBFS
#define ZPAGE_HEADER_SIZE 32
#define ZPOS_PAGE_SIZE 16
#define ZPOS_CHECKSUM 17
#define ZPOS_VERSION 18
#define ZPOS_PAGE_HEADER_SIZE 19

/*--------------------------------------------------------------*/
// Size constants
/*--------------------------------------------------------------*/
#define ZFS_CONNECT_SIZE 4
#define ZSIZE_OF_PAGES_IN_WORDS 8192
#define ZLOG_SIZE_OF_PAGES_IN_WORDS 13
#define ZMAX_PAGES_OF_TABLE_DEFINITION 8
#define ZNUMBER_OF_PAGES (ZMAX_PAGES_OF_TABLE_DEFINITION + 1)
#define ZNO_OF_FRAGRECORD 5

/*--------------------------------------------------------------*/
// Error codes
/*--------------------------------------------------------------*/
#define ZNODE_FAILURE_ERROR 704
#endif

/**
 * Systable NDB$EVENTS_0
 */
#define EVENT_SYSTEM_TABLE_LENGTH 8

struct sysTab_NDBEVENTS_0 {
  char   NAME[MAX_TAB_NAME_SIZE];
  Uint32 EVENT_TYPE;
  Uint32 TABLEID;
  Uint32 TABLEVERSION;
  char   TABLE_NAME[MAX_TAB_NAME_SIZE];
  Uint32 ATTRIBUTE_MASK[MAXNROFATTRIBUTESINWORDS];
  Uint32 SUBID;
  Uint32 SUBKEY;
};

/**
 *  DICT - This blocks handles all metadata
 */
class Dbdict: public SimulatedBlock {
public:
  /*
   *   2.3 RECORD AND FILESIZES
   */

  /**
   * Table attributes.  Permanent data.
   *
   * Indexes have an attribute list which duplicates primary table
   * attributes.  This is wrong but convenient.
   */
  struct AttributeRecord {
    AttributeRecord(){}

    /* attribute id */
    Uint16 attributeId;

    /* Attribute number within tuple key (counted from 1) */
    Uint16 tupleKey;

    /* Attribute name (unique within table) */
    RopeHandle attributeName;

    /* Attribute description (old-style packed descriptor) */
    Uint32 attributeDescriptor;

    /* Extended attributes */
    Uint32 extType;
    Uint32 extPrecision;
    Uint32 extScale;
    Uint32 extLength;

    /* Autoincrement flag, only for ODBC/SQL */
    bool autoIncrement;

    /* Default value as null-terminated string, only for ODBC/SQL */
    RopeHandle defaultValue;

    struct {
      Uint32 m_name_len;
      const char * m_name_ptr;
      RopePool * m_pool;
    } m_key;

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
    Uint32 nextHash;
    Uint32 prevHash;
 
    Uint32 hashValue() const { return attributeName.hashValue();}
    bool equal(const AttributeRecord& obj) const { 
      if(obj.hashValue() == hashValue()){
	ConstRope r(* m_key.m_pool, obj.attributeName);
	return r.compare(m_key.m_name_ptr, m_key.m_name_len) == 0;
      }
      return false;
    }

    /** Singly linked in internal (attributeId) order */
    // TODO use DL template when possible to have more than 1
    Uint32 nextAttributeIdPtrI;
  };
  typedef Ptr<AttributeRecord> AttributeRecordPtr;
  ArrayPool<AttributeRecord> c_attributeRecordPool;
  DLHashTable<AttributeRecord> c_attributeRecordHash;
  RSS_AP_SNAPSHOT(c_attributeRecordPool);

  /**
   * Shared table / index record.  Most of this is permanent data stored
   * on disk.  Index trigger ids are volatile.
   */
  struct TableRecord {
    TableRecord(){}
    Uint32 maxRowsLow;
    Uint32 maxRowsHigh;
    Uint32 minRowsLow;
    Uint32 minRowsHigh;
    /* Table id (array index in DICT and other blocks) */
    Uint32 tableId;
    Uint32 m_obj_ptr_i;

    /* Table version (incremented when tableId is re-used) */
    Uint32 tableVersion;

    /* Table name (may not be unique under "alter table") */
    RopeHandle tableName;

    /* Type of table or index */
    DictTabInfo::TableType tableType;

    /* Is table or index online (this flag is not used in DICT) */
    bool online;

    /* Primary table of index otherwise RNIL */
    Uint32 primaryTableId;

    /* Type of fragmentation (small/medium/large) */
    DictTabInfo::FragmentType fragmentType;

    /* Global checkpoint identity when table created */
    Uint32 gciTableCreated;

    /* Is the table logged (i.e. data survives system restart) */
    enum Bits
    {
      TR_Logged       = 0x1,
      TR_RowGCI       = 0x2,
      TR_RowChecksum  = 0x4,
      TR_Temporary    = 0x8,
      TR_ForceVarPart = 0x10
    };
    Uint16 m_bits;

    /* Number of attibutes in table */
    Uint16 noOfAttributes;

    /* Number of null attributes in table (should be computed) */
    Uint16 noOfNullAttr;

    /* Number of primary key attributes (should be computed) */
    Uint16 noOfPrimkey;

    /* Length of primary key in words (should be computed) */
    /* For ordered index this is tree node size in words */
    Uint16 tupKeyLength;

    /** */
    Uint16 noOfCharsets;

    /* K value for LH**3 algorithm (only 6 allowed currently) */
    Uint8 kValue;

    /* Local key length in words (currently 1) */
    Uint8 localKeyLen;

    /*
     * Parameter for hash algorithm that specifies the load factor in
     * percentage of fill level in buckets. A high value means we are
     * splitting early and that buckets are only lightly used. A high
     * value means that we have fill the buckets more and get more
     * likelihood of overflow buckets.
     */
    Uint8 maxLoadFactor;

    /*
      Flag to indicate default number of partitions
    */
    bool defaultNoPartFlag;

    /*
      Flag to indicate using linear hash function
    */
    bool linearHashFlag;

    /*
     * Used when shrinking to decide when to merge buckets.  Hysteresis
     * is thus possible. Should be smaller but not much smaller than
     * maxLoadFactor
     */
    Uint8 minLoadFactor;

    /* Convenience routines */
    bool isTable() const;
    bool isIndex() const;
    bool isUniqueIndex() const;
    bool isNonUniqueIndex() const;
    bool isHashIndex() const;
    bool isOrderedIndex() const;
    
    /****************************************************
     *    Support variables for table handling
     ****************************************************/

    /*     Active page which is sent to disk */
    Uint32 activePage;

    /**    File pointer received from disk   */
    Uint32 filePtr[2];

    /**    Pointer to first attribute in table */
    DLFifoList<AttributeRecord>::Head m_attributes;

    /*    Pointer to first page of table description */
    Uint32 firstPage;

    Uint32 nextPool;

    enum TabState {
      NOT_DEFINED = 0,
      DEFINING = 2,
      DEFINED = 4,
      PREPARE_DROPPING = 5,
      DROPPING = 6,
      BACKUP_ONGOING = 7
    };
    TabState tabState;

    /*    State when returning from TC_SCHVERREQ */
    enum TabReturnState {
      TRS_IDLE = 0,
      ADD_TABLE = 1,
      SLAVE_SYSTEM_RESTART = 2,
      MASTER_SYSTEM_RESTART = 3
    };
    TabReturnState tabReturnState;

    /**    Number of words */
    Uint32 packedSize;

    /**   Index state (volatile data) */
    enum IndexState {
      IS_UNDEFINED = 0,         // initial
      IS_OFFLINE = 1,           // index table created
      IS_BUILDING = 2,          // building (local state)
      IS_DROPPING = 3,          // dropping (local state)
      IS_ONLINE = 4,            // online
      IS_BROKEN = 9             // build or drop aborted
    };
    IndexState indexState;

    /**   Trigger ids of index (volatile data) */
    Uint32 insertTriggerId;
    Uint32 updateTriggerId;
    Uint32 deleteTriggerId;
    Uint32 customTriggerId;     // ordered index
    Uint32 buildTriggerId;      // temp during build

    /**  Index state in other blocks on this node */
    enum IndexLocal {
      IL_CREATED_TC = 1 << 0    // created in TC
    };
    Uint32 indexLocal;
    
    Uint32 noOfNullBits;
    
    /**  frm data for this table */
    RopeHandle frmData;
    RopeHandle tsData;
    RopeHandle ngData;
    RopeHandle rangeData;

    Uint32 fragmentCount;
    Uint32 m_tablespace_id;

    /*
     * Access rights to table during single user mode
     */
    Uint8 singleUserMode;
  };

  typedef Ptr<TableRecord> TableRecordPtr;
  ArrayPool<TableRecord> c_tableRecordPool;
  RSS_AP_SNAPSHOT(c_tableRecordPool);

  /**  Node Group and Tablespace id+version + range or list data.
    *  This is only stored temporarily in DBDICT during an ongoing
    *  change.
    *  TODO RONM: Look into improvements of this
    */
  Uint32 c_fragDataLen;
  Uint16 c_fragData[MAX_NDB_PARTITIONS];
  Uint32 c_tsIdData[2*MAX_NDB_PARTITIONS];

  /**
   * Triggers.  This is volatile data not saved on disk.  Setting a
   * trigger online creates the trigger in TC (if index) and LQH-TUP.
   */
  struct TriggerRecord {
    TriggerRecord() {}

    /** Trigger state */
    enum TriggerState { 
      TS_NOT_DEFINED = 0,
      TS_DEFINING = 1,
      TS_OFFLINE  = 2,   // created globally in DICT
      TS_BUILDING = 3,
      TS_DROPPING = 4,
      TS_ONLINE = 5      // activated globally
    };
    TriggerState triggerState;

    /** Trigger state in other blocks on this node */
    enum IndexLocal {
      TL_CREATED_TC = 1 << 0,   // created in TC
      TL_CREATED_LQH = 1 << 1   // created in LQH-TUP
    };
    Uint32 triggerLocal;

    /** Trigger name, used by DICT to identify the trigger */ 
    RopeHandle triggerName;

    /** Trigger id, used by TRIX, TC, LQH, and TUP to identify the trigger */
    Uint32 triggerId;
    Uint32 m_obj_ptr_i;

    /** Table id, the table the trigger is defined on */
    Uint32 tableId;

    /** Trigger type, defines what the trigger is used for */
    TriggerType::Value triggerType;
    
    /** Trigger action time, defines when the trigger should fire */
    TriggerActionTime::Value triggerActionTime;
    
    /** Trigger event, defines what events the trigger should monitor */
    TriggerEvent::Value triggerEvent;
    
    /** Monitor all replicas */
    bool monitorReplicas;

    /** Monitor all, the trigger monitors changes of all attributes in table */
    bool monitorAllAttributes;

    /** Monitor all, the trigger monitors changes of all attributes in table */
    bool reportAllMonitoredAttributes;
        
    /**
     * Attribute mask, defines what attributes are to be monitored.
     * Can be seen as a compact representation of SQL column name list.
     */
    AttributeMask attributeMask;

    /** Index id, only used by secondary_index triggers */
    Uint32 indexId;

    /** Pointer to the next attribute used by ArrayPool */
    Uint32 nextPool;
  };
  
  Uint32 c_maxNoOfTriggers;
  typedef Ptr<TriggerRecord> TriggerRecordPtr;
  ArrayPool<TriggerRecord> c_triggerRecordPool;
  RSS_AP_SNAPSHOT(c_triggerRecordPool);

  /**
   * Information for each FS connection.
   ***************************************************************************/
  struct FsConnectRecord {
    enum FsState {
      IDLE = 0,
      OPEN_WRITE_SCHEMA = 1,
      WRITE_SCHEMA = 2,
      CLOSE_WRITE_SCHEMA = 3,
      OPEN_READ_SCHEMA1 = 4,
      OPEN_READ_SCHEMA2 = 5,
      READ_SCHEMA1 = 6,
      READ_SCHEMA2 = 7,
      CLOSE_READ_SCHEMA = 8,
      OPEN_READ_TAB_FILE1 = 9,
      OPEN_READ_TAB_FILE2 = 10,
      READ_TAB_FILE1 = 11,
      READ_TAB_FILE2 = 12,
      CLOSE_READ_TAB_FILE = 13,
      OPEN_WRITE_TAB_FILE = 14,
      WRITE_TAB_FILE = 15,
      CLOSE_WRITE_TAB_FILE = 16
    };
    /** File Pointer for this file system connection */
    Uint32 filePtr;

    /** Reference of owner record */
    Uint32 ownerPtr;

    /** State of file system connection */
    FsState fsState;

    /** Used by Array Pool for free list handling */
    Uint32 nextPool;
  };
  
  typedef Ptr<FsConnectRecord> FsConnectRecordPtr;
  ArrayPool<FsConnectRecord> c_fsConnectRecordPool;

  /**
   * This record stores all the information about a node and all its attributes
   ***************************************************************************/
  struct NodeRecord {
    enum NodeState {
      API_NODE = 0,
      NDB_NODE_ALIVE = 1,
      NDB_NODE_DEAD = 2
    };
    bool hotSpare;
    NodeState nodeState;
  };

  typedef Ptr<NodeRecord> NodeRecordPtr;
  CArray<NodeRecord> c_nodes;
  NdbNodeBitmask c_aliveNodes;
  
  struct PageRecord {
    Uint32 word[8192];
  };
  
  typedef Ptr<PageRecord> PageRecordPtr;
  CArray<PageRecord> c_pageRecordArray;

  struct SchemaPageRecord {
    Uint32 word[NDB_SF_PAGE_SIZE_IN_WORDS];
  };

  CArray<SchemaPageRecord> c_schemaPageRecordArray;

  unsigned g_trace;
  DictTabInfo::Table c_tableDesc;

  /**
   * A page for create index table signal.
   */
  PageRecord c_indexPage;

  struct File {
    File() {}
    
    Uint32 key;
    Uint32 m_magic;
    Uint32 m_version;
    Uint32 m_obj_ptr_i;
    Uint32 m_filegroup_id;
    Uint32 m_type;
    Uint64 m_file_size;
    Uint64 m_file_free;
    RopeHandle m_path;

    Uint32 m_warningFlags; // move to op in 7.0
    
    Uint32 nextList;
    union {
      Uint32 prevList;
      Uint32 nextPool;
    };
    Uint32 nextHash, prevHash;

    Uint32 hashValue() const { return key;}
    bool equal(const File& obj) const { return key == obj.key;}
  };
  typedef Ptr<File> FilePtr;
  typedef RecordPool<File, RWPool> File_pool;
  typedef DLListImpl<File_pool, File> File_list;
  typedef LocalDLListImpl<File_pool, File> Local_file_list;
  typedef KeyTableImpl<File_pool, File> File_hash;
  
  struct Filegroup {
    Filegroup(){}

    Uint32 key;
    Uint32 m_obj_ptr_i;
    Uint32 m_magic;
    
    Uint32 m_type;
    Uint32 m_version;
    RopeHandle m_name;

    union {
      struct {
	Uint32 m_extent_size;
	Uint32 m_default_logfile_group_id;
      } m_tablespace;
      
      struct {
	Uint32 m_undo_buffer_size;
	File_list::HeadPOD m_files;
      } m_logfilegroup;
    };

    Uint32 m_warningFlags; // move to op in 7.0
    
    union {
      Uint32 nextPool;
      Uint32 nextList;
      Uint32 nextHash;
    };
    Uint32 prevHash;

    Uint32 hashValue() const { return key;}
    bool equal(const Filegroup& obj) const { return key == obj.key;}
  };
  typedef Ptr<Filegroup> FilegroupPtr;
  typedef RecordPool<Filegroup, RWPool> Filegroup_pool;
  typedef KeyTableImpl<Filegroup_pool, Filegroup> Filegroup_hash;
  
  File_pool c_file_pool;
  Filegroup_pool c_filegroup_pool;
  File_hash c_file_hash;
  Filegroup_hash c_filegroup_hash;
  
  RopePool c_rope_pool;
  RSS_AP_SNAPSHOT(c_rope_pool);

  struct DictObject {
    DictObject() {}
    Uint32 m_id;
    Uint32 m_type;
    Uint32 m_ref_count;
    RopeHandle m_name;  
    union {
      struct {
	Uint32 m_name_len;
	const char * m_name_ptr;
	RopePool * m_pool;
      } m_key;
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 nextHash;
    Uint32 prevHash;
    
    Uint32 hashValue() const { return m_name.hashValue();}
    bool equal(const DictObject& obj) const { 
      if(obj.hashValue() == hashValue()){
	ConstRope r(* m_key.m_pool, obj.m_name);
	return r.compare(m_key.m_name_ptr, m_key.m_name_len) == 0;
      }
      return false;
    }
  };
  
  DLHashTable<DictObject> c_obj_hash; // Name
  ArrayPool<DictObject> c_obj_pool;
  RSS_AP_SNAPSHOT(c_obj_pool);
  
  DictObject * get_object(const char * name){
    return get_object(name, strlen(name) + 1);
  }
  
  DictObject * get_object(const char * name, Uint32 len){
    return get_object(name, len, Rope::hash(name, len));
  }
  
  DictObject * get_object(const char * name, Uint32 len, Uint32 hash);

  void release_object(Uint32 obj_ptr_i){
    release_object(obj_ptr_i, c_obj_pool.getPtr(obj_ptr_i));
  }
  
  void release_object(Uint32 obj_ptr_i, DictObject* obj_ptr_p);

  void increase_ref_count(Uint32 obj_ptr_i);
  void decrease_ref_count(Uint32 obj_ptr_i);

public:
  Dbdict(Block_context& ctx);
  virtual ~Dbdict();

private:
  BLOCK_DEFINES(Dbdict);

  // Signal receivers
  void execDICTSTARTREQ(Signal* signal);
  
  void execGET_TABINFOREQ(Signal* signal);
  void execGET_TABLEDID_REQ(Signal* signal);
  void execGET_TABINFO_REF(Signal* signal);
  void execGET_TABINFO_CONF(Signal* signal);
  void execCONTINUEB(Signal* signal);

  void execDUMP_STATE_ORD(Signal* signal);
  void execHOT_SPAREREP(Signal* signal);
  void execDIADDTABCONF(Signal* signal);
  void execDIADDTABREF(Signal* signal);
  void execTAB_COMMITCONF(Signal* signal);
  void execTAB_COMMITREF(Signal* signal);
  void execGET_SCHEMA_INFOREQ(Signal* signal);
  void execSCHEMA_INFO(Signal* signal);
  void execSCHEMA_INFOCONF(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execFSCLOSECONF(Signal* signal);
  void execFSOPENCONF(Signal* signal);
  void execFSOPENREF(Signal* signal);
  void execFSREADCONF(Signal* signal);
  void execFSREADREF(Signal* signal);
  void execFSWRITECONF(Signal* signal);
  void execFSREMOVEREF(Signal*);
  void execFSREMOVECONF(Signal*);
  void execNDB_STTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execTC_SCHVERCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execAPI_FAILREQ(Signal* signal);

  void execWAIT_GCP_REF(Signal* signal);
  void execWAIT_GCP_CONF(Signal* signal);

  void execLIST_TABLES_REQ(Signal* signal);

  // Index signals
  void execCREATE_INDX_REQ(Signal* signal);
  void execCREATE_INDX_CONF(Signal* signal);
  void execCREATE_INDX_REF(Signal* signal);

  void execALTER_INDX_REQ(Signal* signal);
  void execALTER_INDX_CONF(Signal* signal);
  void execALTER_INDX_REF(Signal* signal);

  void execCREATE_TABLE_CONF(Signal* signal);
  void execCREATE_TABLE_REF(Signal* signal);

  void execDROP_INDX_REQ(Signal* signal);
  void execDROP_INDX_CONF(Signal* signal);
  void execDROP_INDX_REF(Signal* signal);

  void execDROP_TABLE_CONF(Signal* signal);
  void execDROP_TABLE_REF(Signal* signal);

  void execBUILDINDXREQ(Signal* signal);
  void execBUILDINDXCONF(Signal* signal);
  void execBUILDINDXREF(Signal* signal);

  void execBACKUP_FRAGMENT_REQ(Signal*);

  // Util signals used by Event code
  void execUTIL_PREPARE_CONF(Signal* signal);
  void execUTIL_PREPARE_REF (Signal* signal);
  void execUTIL_EXECUTE_CONF(Signal* signal);
  void execUTIL_EXECUTE_REF (Signal* signal);
  void execUTIL_RELEASE_CONF(Signal* signal);
  void execUTIL_RELEASE_REF (Signal* signal);


  // Event signals from API
  void execCREATE_EVNT_REQ (Signal* signal);
  void execCREATE_EVNT_CONF(Signal* signal);
  void execCREATE_EVNT_REF (Signal* signal);

  void execDROP_EVNT_REQ (Signal* signal);

  void execSUB_START_REQ (Signal* signal);
  void execSUB_START_CONF (Signal* signal);
  void execSUB_START_REF (Signal* signal);

  void execSUB_STOP_REQ (Signal* signal);
  void execSUB_STOP_CONF (Signal* signal);
  void execSUB_STOP_REF (Signal* signal);

  // Event signals from SUMA

  void execCREATE_SUBID_CONF(Signal* signal);
  void execCREATE_SUBID_REF (Signal* signal);

  void execSUB_CREATE_CONF(Signal* signal);
  void execSUB_CREATE_REF (Signal* signal);

  void execSUB_REMOVE_REQ(Signal* signal);
  void execSUB_REMOVE_CONF(Signal* signal);
  void execSUB_REMOVE_REF(Signal* signal);

  // Trigger signals
  void execCREATE_TRIG_REQ(Signal* signal);
  void execCREATE_TRIG_CONF(Signal* signal);
  void execCREATE_TRIG_REF(Signal* signal);
  void execALTER_TRIG_REQ(Signal* signal);
  void execALTER_TRIG_CONF(Signal* signal);
  void execALTER_TRIG_REF(Signal* signal);
  void execDROP_TRIG_REQ(Signal* signal);
  void execDROP_TRIG_CONF(Signal* signal);
  void execDROP_TRIG_REF(Signal* signal);

  void execDROP_TABLE_REQ(Signal* signal);
  
  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execPREP_DROP_TAB_REF(Signal* signal);  
  void execPREP_DROP_TAB_CONF(Signal* signal);

  void execDROP_TAB_REQ(Signal* signal);  
  void execDROP_TAB_REF(Signal* signal);  
  void execDROP_TAB_CONF(Signal* signal);

  void execCREATE_TABLE_REQ(Signal* signal);
  void execALTER_TABLE_REQ(Signal* signal);
  void execCREATE_FRAGMENTATION_REF(Signal*);
  void execCREATE_FRAGMENTATION_CONF(Signal*);
  void execCREATE_TAB_REQ(Signal* signal);
  void execADD_FRAGREQ(Signal* signal);
  void execLQHFRAGREF(Signal* signal);
  void execLQHFRAGCONF(Signal* signal);
  void execLQHADDATTREF(Signal* signal);
  void execLQHADDATTCONF(Signal* signal);
  void execCREATE_TAB_REF(Signal* signal);
  void execCREATE_TAB_CONF(Signal* signal);  
  void execALTER_TAB_REQ(Signal* signal);
  void execALTER_TAB_REF(Signal* signal);
  void execALTER_TAB_CONF(Signal* signal);
  bool check_ndb_versions() const;

  void execCREATE_FILE_REQ(Signal* signal);
  void execCREATE_FILEGROUP_REQ(Signal* signal);
  void execDROP_FILE_REQ(Signal* signal);
  void execDROP_FILEGROUP_REQ(Signal* signal);

  // Internal
  void execCREATE_FILE_REF(Signal* signal);
  void execCREATE_FILE_CONF(Signal* signal);
  void execCREATE_FILEGROUP_REF(Signal* signal);
  void execCREATE_FILEGROUP_CONF(Signal* signal);
  void execDROP_FILE_REF(Signal* signal);
  void execDROP_FILE_CONF(Signal* signal);
  void execDROP_FILEGROUP_REF(Signal* signal);
  void execDROP_FILEGROUP_CONF(Signal* signal);

  void execDICT_LOCK_REQ(Signal* signal);
  void execDICT_UNLOCK_ORD(Signal* signal);

  /*
   *  2.4 COMMON STORED VARIABLES
   */

  /**
   * This record stores all the state needed 
   * when the schema page is being sent to other nodes
   ***************************************************************************/
  struct SendSchemaRecord {
    /** Number of words of schema data */
    Uint32 noOfWords;
    /** Page Id of schema data */
    Uint32 pageId;

    Uint32 nodeId;
    SignalCounter m_SCHEMAINFO_Counter;
    
    Uint32 noOfWordsCurrentlySent;
    Uint32 noOfSignalsSentSinceDelay;

    bool inUse;
  };
  SendSchemaRecord c_sendSchemaRecord;

  /**
   * This record stores all the state needed 
   * when a table file is being read from disk
   ****************************************************************************/
  struct ReadTableRecord {
    /** Number of Pages */
    Uint32 no_of_words;
    /** Page Id*/
    Uint32 pageId;
    /** Table Id of read table */
    Uint32 tableId;
    
    bool inUse;
    Callback m_callback;
  };
  ReadTableRecord c_readTableRecord;

  /**
   * This record stores all the state needed 
   * when a table file is being written to disk
   ****************************************************************************/
  struct WriteTableRecord {
    /** Number of Pages */
    Uint32 no_of_words;
    /** Page Id*/
    Uint32 pageId;
    /** Table Files Handled, local state variable */
    Uint32 noOfTableFilesHandled;
    /** Table Id of written table */
    Uint32 tableId;
    /** State, indicates from where it was called */
    enum TableWriteState {
      IDLE = 0,
      WRITE_ADD_TABLE_MASTER = 1,
      WRITE_ADD_TABLE_SLAVE = 2,
      WRITE_RESTART_FROM_MASTER = 3,
      WRITE_RESTART_FROM_OWN = 4,
      TWR_CALLBACK = 5
    };
    TableWriteState tableWriteState;
    Callback m_callback;
  };
  WriteTableRecord c_writeTableRecord;

  /**
   * This record stores all the state needed 
   * when a schema file is being read from disk
   ****************************************************************************/
  struct ReadSchemaRecord {
    /** Page Id of schema page */
    Uint32 pageId;
    /** First page to read */
    Uint32 firstPage;
    /** Number of pages to read */
    Uint32 noOfPages;
    /** State, indicates from where it was called */
    enum SchemaReadState {
      IDLE = 0,
      INITIAL_READ_HEAD = 1,
      INITIAL_READ = 2
    };
    SchemaReadState schemaReadState;
  };
  ReadSchemaRecord c_readSchemaRecord;

  /**
   * This record stores all the state needed 
   * when a schema file is being written to disk
   ****************************************************************************/
  struct WriteSchemaRecord {
    /** Page Id of schema page */
    Uint32 pageId;
    /** Rewrite entire file */
    Uint32 newFile;
    /** First page to write */
    Uint32 firstPage;
    /** Number of pages to write */
    Uint32 noOfPages;
    /** Schema Files Handled, local state variable */
    Uint32 noOfSchemaFilesHandled;

    bool inUse;
    Callback m_callback;
  };
  WriteSchemaRecord c_writeSchemaRecord;

  /**
   * This record stores all the information needed 
   * when a file is being read from disk
   ****************************************************************************/
  struct RestartRecord {
    /**    Global check point identity       */
    Uint32 gciToRestart;

    /**    The active table at restart process */
    Uint32 activeTable;

    /**    The active table at restart process */
    BlockReference returnBlockRef;
    
    Uint32 m_pass; // 0 tablespaces/logfilegroups, 1 tables, 2 indexes
  };
  RestartRecord c_restartRecord;

  /**
   * This record stores all the information needed 
   * when a file is being read from disk
   ****************************************************************************/
  struct RetrieveRecord {
    RetrieveRecord(){ noOfWaiters = 0;}
    
    /**    Only one retrieve table definition at a time       */
    bool busyState;
    
    /**
     * No of waiting in time queue
     */
    Uint32 noOfWaiters;
    
    /**    Block Reference of retriever       */
    BlockReference blockRef;

    /**    Id of retriever       */
    Uint32 m_senderData;

    /**    Table id of retrieved table       */
    Uint32 tableId;

    Uint32 m_table_type;

    /**    Starting page to retrieve data from   */
    Uint32 retrievePage;

    /**    Number of pages retrieved   */
    Uint32 retrievedNoOfPages;

    /**    Number of words retrieved   */
    Uint32 retrievedNoOfWords;

    /**    Number of words sent currently   */
    Uint32 currentSent;

    /**
     * Long signal stuff
     */
    bool m_useLongSig;
  };
  RetrieveRecord c_retrieveRecord;

  /**
   * This record stores all the information needed 
   * when a file is being read from disk
   * 
   * This is the info stored in one entry of the schema
   * page. Each table has 4 words of info.
   * Word 1: Schema version (upper 16 bits)
   *         Table State (lower 16 bits)
   * Word 2: Number of pages of table description
   * Word 3: Global checkpoint id table was created
   * Word 4: Currently zero
   ****************************************************************************/
  struct SchemaRecord {
    /**    Schema file first page (0)   */
    Uint32 schemaPage;

    /**    Old Schema file first page (used at node restart)    */
    Uint32 oldSchemaPage;
    
    Callback m_callback;
  };
  SchemaRecord c_schemaRecord;

  /*
   * Schema file, list of schema pages.  Use an array until a pool
   * exists and NDBFS interface can use it.
   */
  struct XSchemaFile {
    SchemaFile* schemaPage;
    Uint32 noOfPages;
  };
  // 0-normal 1-old
  XSchemaFile c_schemaFile[2];

  void initSchemaFile(XSchemaFile *, Uint32 firstPage, Uint32 lastPage,
                      bool initEntries);
  void resizeSchemaFile(XSchemaFile * xsf, Uint32 noOfPages);
  void computeChecksum(XSchemaFile *, Uint32 pageNo);
  bool validateChecksum(const XSchemaFile *);
  SchemaFile::TableEntry * getTableEntry(XSchemaFile *, Uint32 tableId);

  Uint32 computeChecksum(const Uint32 * src, Uint32 len);


  /* ----------------------------------------------------------------------- */
  // Node References
  /* ----------------------------------------------------------------------- */
  Uint16 c_masterNodeId;

  /* ----------------------------------------------------------------------- */
  // Various current system properties
  /* ----------------------------------------------------------------------- */
  Uint16 c_numberNode;
  Uint16 c_noHotSpareNodes;
  Uint16 c_noNodesFailed;
  Uint32 c_failureNr;

  /* ----------------------------------------------------------------------- */
  // State variables
  /* ----------------------------------------------------------------------- */

  struct PackTable {
    
    enum PackTableState {
      PTS_IDLE = 0,
      PTS_GET_TAB = 3
    } m_state;

  } c_packTable;

  Uint32 c_startPhase;
  Uint32 c_restartType;
  bool   c_initialStart;
  bool   c_systemRestart;
  bool   c_nodeRestart;
  bool   c_initialNodeRestart;
  Uint32 c_tabinfoReceived;

  /**
   * Temporary structure used when parsing table info
   */
  struct ParseDictTabInfoRecord {
    ParseDictTabInfoRecord() { tablePtr.setNull();}
    DictTabInfo::RequestType requestType;
    Uint32 errorCode;
    Uint32 errorLine;
    
    SimpleProperties::UnpackStatus status;
    Uint32 errorKey;
    TableRecordPtr tablePtr;
  };

  // Operation records

  /**
   * Common part of operation records.  Uses KeyTable2.  Note that each
   * seize/release invokes ctor/dtor automatically.
   */
  struct OpRecordCommon {
    OpRecordCommon() {}
    Uint32 key;         // key shared between master and slaves
    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 hashValue() const {
      return key;
    }
    bool equal(const OpRecordCommon& rec) const {
      return key == rec.key;
    }
  };

  /**
   * Create table record
   */
  struct CreateTableRecord : OpRecordCommon {
    CreateTableRecord() {}
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_coordinatorRef;
    
    Uint32 m_errorCode;
    void setErrorCode(Uint32 c){ if(m_errorCode == 0) m_errorCode = c;}

    // For alter table
    Uint32 m_changeMask;
    Uint32 m_new_cols;
    bool m_alterTableFailed;
    AlterTableRef m_alterTableRef;
    Uint32 m_alterTableId;
    Uint32 m_tupAlterTabPtr;                    // Connect ptr towards TUP

    /* Previous table name (used for reverting failed table rename) */
    char previousTableName[MAX_TAB_NAME_SIZE];

    /* Previous table definition, frm (used for reverting) */
    /** TODO Could preferrably be made dynamic size */
    Uint32 previousFrmLen;
    char previousFrmData[MAX_FRM_DATA_SIZE];

    Uint32 m_tablePtrI;
    Uint32 m_tabInfoPtrI;
    Uint32 m_fragmentsPtrI;

    Uint32 m_dihAddFragPtr; // Connect ptr towards DIH
    Uint32 m_lqhFragPtr;    // Connect ptr towards LQH

    Callback m_callback;    // Who's using local create tab
    MutexHandle2<DIH_START_LCP_MUTEX> m_startLcpMutex;
    
    struct CoordinatorData {
      Uint32 m_gsn;
      SafeCounterHandle m_counter;
      CreateTabReq::RequestType m_requestType;
    } m_coordinatorData;
  };
  typedef Ptr<CreateTableRecord> CreateTableRecordPtr;

  /**
   * Drop table record
   */
  struct DropTableRecord : OpRecordCommon {
    DropTableRecord() {}
    DropTableReq m_request;
    
    Uint32 m_requestType;
    Uint32 m_coordinatorRef;
    
    Uint32 m_errorCode;
    void setErrorCode(Uint32 c){ if(m_errorCode == 0) m_errorCode = c;}

    MutexHandle2<BACKUP_DEFINE_MUTEX> m_define_backup_mutex;
    
    /**
     * When sending stuff around
     */
    struct CoordinatorData {
      Uint32 m_gsn;
      Uint32 m_block;
      SafeCounterHandle m_counter;
    } m_coordinatorData;

    struct ParticipantData {
      Uint32 m_gsn;
      Uint32 m_block;
      SignalCounter m_signalCounter;

      Callback m_callback;
    } m_participantData;
  };
  typedef Ptr<DropTableRecord> DropTableRecordPtr;

  /**
   * Request flags passed in signals along with request type and
   * propagated across operations.
   */
  struct RequestFlag {
    enum {
      RF_LOCAL = 1 << 0,        // create on local node only
      RF_NOBUILD = 1 << 1,      // no need to build index
      RF_NOTCTRIGGER = 1 << 2,  // alter trigger: no trigger in TC
      RF_FORCE = 1 << 4         // force drop
    };
  };

  /**
   * Operation record for create index.
   */
  struct OpCreateIndex : OpRecordCommon {
    // original request (index id will be added)
    CreateIndxReq m_request;
    AttributeList m_attrList;
    char m_indexName[MAX_TAB_NAME_SIZE];
    bool m_loggedIndex;
    bool m_temporaryIndex;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    CreateIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    CreateIndxRef::ErrorCode m_lastError;
    CreateIndxRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpCreateIndex() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = CreateIndxReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = CreateIndxRef::NoError;
      m_errorCode = CreateIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const CreateIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != CreateIndxRef::NoError;
    }
    bool hasError() {
      return m_errorCode != CreateIndxRef::NoError;
    }
    void setError(const CreateIndxRef* ref) {
      m_lastError = CreateIndxRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const CreateTableRef* ref) {
      m_lastError = CreateIndxRef::NoError;
      if (ref != 0) {
        switch (ref->getErrorCode()) {
        case CreateTableRef::TableAlreadyExist:
          m_lastError = CreateIndxRef::IndexExists;
          break;
        default:
          m_lastError = (CreateIndxRef::ErrorCode)ref->getErrorCode();
          break;
        }
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
        }
      }
    }
    void setError(const AlterIndxRef* ref) {
      m_lastError = CreateIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (CreateIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpCreateIndex> OpCreateIndexPtr;

  /**
   * Operation record for drop index.
   */
  struct OpDropIndex : OpRecordCommon {
    // original request
    DropIndxReq m_request;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    DropIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    DropIndxRef::ErrorCode m_lastError;
    DropIndxRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpDropIndex() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = DropIndxReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = DropIndxRef::NoError;
      m_errorCode = DropIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const DropIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != DropIndxRef::NoError;
    }
    bool hasError() {
      return m_errorCode != DropIndxRef::NoError;
    }
    void setError(const DropIndxRef* ref) {
      m_lastError = DropIndxRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = ref->getErrorCode();
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const AlterIndxRef* ref) {
      m_lastError = DropIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (DropIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const DropTableRef* ref) {
      m_lastError = DropIndxRef::NoError;
      if (ref != 0) {
	switch (ref->errorCode) {
	case DropTableRef::Busy:
	  m_lastError = DropIndxRef::Busy;
	  break;
	case DropTableRef::NoSuchTable:
	  m_lastError = DropIndxRef::IndexNotFound;
	  break;
	case DropTableRef::DropInProgress:
	  m_lastError = DropIndxRef::Busy;
	  break;
	case DropTableRef::NoDropTableRecordAvailable:
	  m_lastError = DropIndxRef::Busy;
	  break;
	default:
	  m_lastError = (DropIndxRef::ErrorCode)ref->errorCode;
	  break;
	}
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = 0;
          m_errorNode = 0;
        }
      }
    }
  };
  typedef Ptr<OpDropIndex> OpDropIndexPtr;

  /**
   * Operation record for alter index.
   */
  struct OpAlterIndex : OpRecordCommon {
    // original request plus buffer for attribute lists
    AlterIndxReq m_request;
    AttributeList m_attrList;
    AttributeList m_tableKeyList;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    AlterIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    AlterIndxRef::ErrorCode m_lastError;
    AlterIndxRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    Uint32 m_triggerCounter;
    // ctor
    OpAlterIndex() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = AlterIndxReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = AlterIndxRef::NoError;
      m_errorCode = AlterIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
      m_triggerCounter = 0;
    }
    void save(const AlterIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != AlterIndxRef::NoError;
    }
    bool hasError() {
      return m_errorCode != AlterIndxRef::NoError;
    }
    void setError(const AlterIndxRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const CreateIndxRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const DropIndxRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const BuildIndxRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = 0;
          m_errorNode = 0;
        }
      }
    }
    void setError(const CreateTrigRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const DropTrigRef* ref) {
      m_lastError = AlterIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpAlterIndex> OpAlterIndexPtr;

  /**
   * Operation record for build index.
   */
  struct OpBuildIndex : OpRecordCommon {
    // original request plus buffer for attribute lists
    BuildIndxReq m_request;
    AttributeList m_attrList;
    Id_array<MAX_ATTRIBUTES_IN_INDEX+1> m_tableKeyList;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    BuildIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    Uint32 m_constrTriggerId;
    // error info
    BuildIndxRef::ErrorCode m_lastError;
    BuildIndxRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpBuildIndex() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = BuildIndxReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = BuildIndxRef::NoError;
      m_errorCode = BuildIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const BuildIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != BuildIndxRef::NoError;
    }
    bool hasError() {
      return m_errorCode != BuildIndxRef::NoError;
    }
    void setError(const BuildIndxRef* ref) {
      m_lastError = BuildIndxRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = 0;
          m_errorNode = 0;
        }
      }
    }
    void setError(const AlterIndxRef* ref) {
      m_lastError = BuildIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const CreateTrigRef* ref) {
      m_lastError = BuildIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const DropTrigRef* ref) {
      m_lastError = BuildIndxRef::NoError;
      if (ref != 0) {
        m_lastError = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpBuildIndex> OpBuildIndexPtr;

  /**
   * Operation record for Util Signals.
   */
  struct OpSignalUtil : OpRecordCommon{
    Callback m_callback;
    Uint32 m_userData;
  };
  typedef Ptr<OpSignalUtil> OpSignalUtilPtr;

  /**
   * Operation record for subscribe-start-stop
   */
  struct OpSubEvent : OpRecordCommon {
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_errorCode;

    Uint32 m_gsn;
    Uint32 m_subscriptionId;
    Uint32 m_subscriptionKey;
    Uint32 m_subscriberRef;
    Uint32 m_subscriberData;
    union {
      SubStartConf m_sub_start_conf;
      SubStopConf m_sub_stop_conf;
    };
    RequestTracker m_reqTracker;
  };
  typedef Ptr<OpSubEvent> OpSubEventPtr;

  static const Uint32 sysTab_NDBEVENTS_0_szs[];

  /**
   * Operation record for create event.
   */
  struct OpCreateEvent : OpRecordCommon {
    // original request (event id will be added)
    CreateEvntReq m_request;
    //AttributeMask m_attrListBitmask;
    //    AttributeList m_attrList;
    sysTab_NDBEVENTS_0 m_eventRec;
    //    char m_eventName[MAX_TAB_NAME_SIZE];
    //    char m_tableName[MAX_TAB_NAME_SIZE];

    // coordinator DICT
    RequestTracker m_reqTracker;
    // state info
    CreateEvntReq::RequestType m_requestType;
    // error info
    Uint32 m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode; /* also used to store master node id
                           in case of NotMaster */
    // ctor
    OpCreateEvent() {
      memset(&m_request, 0, sizeof(m_request));
      m_requestType = CreateEvntReq::RT_UNDEFINED;
      m_errorCode = CreateEvntRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void init(const CreateEvntReq* req, Dbdict* dp) {
      m_request = *req;
      m_errorCode = CreateEvntRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
      m_requestType = req->getRequestType();
    }
    bool hasError() {
      return m_errorCode != CreateEvntRef::NoError;
    }
    void setError(const CreateEvntRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }

  };
  typedef Ptr<OpCreateEvent> OpCreateEventPtr;

  /**
   * Operation record for drop event.
   */
  struct OpDropEvent : OpRecordCommon {
    // original request
    DropEvntReq m_request;
    //    char m_eventName[MAX_TAB_NAME_SIZE];
    sysTab_NDBEVENTS_0 m_eventRec;
    RequestTracker m_reqTracker;
    // error info
    Uint32 m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // ctor
    OpDropEvent() {
      memset(&m_request, 0, sizeof(m_request));
      m_errorCode = 0;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void init(const DropEvntReq* req) {
      m_request = *req;
      m_errorCode = 0;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    bool hasError() {
      return m_errorCode != 0;
    }
    void setError(const DropEvntRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
  };
  typedef Ptr<OpDropEvent> OpDropEventPtr;

  /**
   * Operation record for create trigger.
   */
  struct OpCreateTrigger : OpRecordCommon {
    // original request (trigger id will be added)
    CreateTrigReq m_request;
    char m_triggerName[MAX_TAB_NAME_SIZE];
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    CreateTrigReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    CreateTrigRef::ErrorCode m_lastError;
    CreateTrigRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpCreateTrigger() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = CreateTrigReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = CreateTrigRef::NoError;
      m_errorCode = CreateTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const CreateTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != CreateTrigRef::NoError;
    }
    bool hasError() {
      return m_errorCode != CreateTrigRef::NoError;
    }
    void setError(const CreateTrigRef* ref) {
      m_lastError = CreateTrigRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const AlterTrigRef* ref) {
      m_lastError = CreateTrigRef::NoError;
      if (ref != 0) {
        m_lastError = (CreateTrigRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpCreateTrigger> OpCreateTriggerPtr;

  /**
   * Operation record for drop trigger.
   */
  struct OpDropTrigger : OpRecordCommon {
    // original request
    DropTrigReq m_request;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    DropTrigReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    DropTrigRef::ErrorCode m_lastError;
    DropTrigRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpDropTrigger() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = DropTrigReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = DropTrigRef::NoError;
      m_errorCode = DropTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const DropTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != DropTrigRef::NoError;
    }
    bool hasError() {
      return m_errorCode != DropTrigRef::NoError;
    }
    void setError(const DropTrigRef* ref) {
      m_lastError = DropTrigRef::NoError;
      if (ref != 0) {
        m_lastError = ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const AlterTrigRef* ref) {
      m_lastError = DropTrigRef::NoError;
      if (ref != 0) {
        m_lastError = (DropTrigRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpDropTrigger> OpDropTriggerPtr;

  /**
   * Operation record for alter trigger.
   */
  struct OpAlterTrigger : OpRecordCommon {
    // original request
    AlterTrigReq m_request;
    // nodes participating in operation
    NdbNodeBitmask m_nodes;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    AlterTrigReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
    AlterTrigRef::ErrorCode m_lastError;
    AlterTrigRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // counters
    SignalCounter m_signalCounter;
    // ctor
    OpAlterTrigger() {
      memset(&m_request, 0, sizeof(m_request));
      m_coordinatorRef = 0;
      m_requestType = AlterTrigReq::RT_UNDEFINED;
      m_requestFlag = 0;
      m_lastError = AlterTrigRef::NoError;
      m_errorCode = AlterTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const AlterTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasLastError() {
      return m_lastError != AlterTrigRef::NoError;
    }
    bool hasError() {
      return m_errorCode != AlterTrigRef::NoError;
    }
    void setError(const AlterTrigRef* ref) {
      m_lastError = AlterTrigRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const CreateTrigRef* ref) {
      m_lastError = AlterTrigRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
    void setError(const DropTrigRef* ref) {
      m_lastError = AlterTrigRef::NoError;
      if (ref != 0) {
        m_lastError = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        if (! hasError()) {
          m_errorCode = m_lastError;
          m_errorLine = ref->getErrorLine();
          m_errorNode = ref->getErrorNode();
        }
      }
    }
  };
  typedef Ptr<OpAlterTrigger> OpAlterTriggerPtr;

public:
  struct SchemaOp : OpRecordCommon {
    
    Uint32 m_clientRef; // API (for take-over)
    Uint32 m_clientData;// API
    
    Uint32 m_senderRef; // 
    Uint32 m_senderData;// transaction key value
    
    Uint32 m_errorCode;
    
    Uint32 m_obj_id;
    Uint32 m_obj_type;
    Uint32 m_obj_version;
    Uint32 m_obj_ptr_i;
    Uint32 m_vt_index;
    Callback m_callback;
  };
  typedef Ptr<SchemaOp> SchemaOpPtr;

  struct SchemaTransaction : OpRecordCommon {
    Uint32 m_senderRef; // API
    Uint32 m_senderData;// API
    
    Callback m_callback;
    SafeCounterHandle m_counter;
    NodeBitmask m_nodes;
    
    Uint32 m_errorCode;
    SchemaTransaction() {}
    void setErrorCode(Uint32 c){ if(m_errorCode == 0) m_errorCode = c;}

    /**
     * This should contain "lists" with operations
     */
    struct {
      Uint32 m_key;      // Operation key
      Uint32 m_vt_index; // Operation type
      Uint32 m_obj_id;
      DictObjOp::State m_state;
    } m_op;
  };
private:

  struct OpCreateObj : public SchemaOp {
    Uint32 m_gci;
    Uint32 m_obj_info_ptr_i;
    Uint32 m_restart;
  };
  typedef Ptr<OpCreateObj> CreateObjRecordPtr;
  
  struct OpDropObj : public SchemaOp 
  {
  };
  typedef Ptr<OpDropObj> DropObjRecordPtr;
  
  /**
   * Only used at coordinator/master
   */
  // Common operation record pool
public:
  STATIC_CONST( opCreateTableSize = sizeof(CreateTableRecord) );
  STATIC_CONST( opDropTableSize = sizeof(DropTableRecord) );
  STATIC_CONST( opCreateIndexSize = sizeof(OpCreateIndex) );
  STATIC_CONST( opDropIndexSize = sizeof(OpDropIndex) );
  STATIC_CONST( opAlterIndexSize = sizeof(OpAlterIndex) );
  STATIC_CONST( opBuildIndexSize = sizeof(OpBuildIndex) );
  STATIC_CONST( opCreateEventSize = sizeof(OpCreateEvent) );
  STATIC_CONST( opSubEventSize = sizeof(OpSubEvent) );
  STATIC_CONST( opDropEventSize = sizeof(OpDropEvent) );
  STATIC_CONST( opSignalUtilSize = sizeof(OpSignalUtil) );
  STATIC_CONST( opCreateTriggerSize = sizeof(OpCreateTrigger) );
  STATIC_CONST( opDropTriggerSize = sizeof(OpDropTrigger) );
  STATIC_CONST( opAlterTriggerSize = sizeof(OpAlterTrigger) );
  STATIC_CONST( opCreateObjSize = sizeof(OpCreateObj) );
private:
#define PTR_ALIGN(n) ((((n)+sizeof(void*)-1)>>2)&~((sizeof(void*)-1)>>2))
  union OpRecordUnion {
    Uint32 u_opCreateTable  [PTR_ALIGN(opCreateTableSize)];
    Uint32 u_opDropTable    [PTR_ALIGN(opDropTableSize)];
    Uint32 u_opCreateIndex  [PTR_ALIGN(opCreateIndexSize)];
    Uint32 u_opDropIndex    [PTR_ALIGN(opDropIndexSize)];
    Uint32 u_opCreateEvent  [PTR_ALIGN(opCreateEventSize)];
    Uint32 u_opSubEvent     [PTR_ALIGN(opSubEventSize)];
    Uint32 u_opDropEvent    [PTR_ALIGN(opDropEventSize)];
    Uint32 u_opSignalUtil   [PTR_ALIGN(opSignalUtilSize)];
    Uint32 u_opAlterIndex   [PTR_ALIGN(opAlterIndexSize)];
    Uint32 u_opBuildIndex   [PTR_ALIGN(opBuildIndexSize)];
    Uint32 u_opCreateTrigger[PTR_ALIGN(opCreateTriggerSize)];
    Uint32 u_opDropTrigger  [PTR_ALIGN(opDropTriggerSize)];
    Uint32 u_opAlterTrigger [PTR_ALIGN(opAlterTriggerSize)];
    Uint32 u_opCreateObj    [PTR_ALIGN(opCreateObjSize)];
    Uint32 nextPool;
  };
  ArrayPool<OpRecordUnion> c_opRecordPool;
  
  // Operation records
  KeyTable2<CreateTableRecord, OpRecordUnion> c_opCreateTable;
  KeyTable2<DropTableRecord, OpRecordUnion> c_opDropTable;
  KeyTable2<OpCreateIndex, OpRecordUnion> c_opCreateIndex;
  KeyTable2<OpDropIndex, OpRecordUnion> c_opDropIndex;
  KeyTable2<OpAlterIndex, OpRecordUnion> c_opAlterIndex;
  KeyTable2<OpBuildIndex, OpRecordUnion> c_opBuildIndex;
  KeyTable2C<OpCreateEvent, OpRecordUnion> c_opCreateEvent;
  KeyTable2C<OpSubEvent, OpRecordUnion> c_opSubEvent;
  KeyTable2C<OpDropEvent, OpRecordUnion> c_opDropEvent;
  KeyTable2C<OpSignalUtil, OpRecordUnion> c_opSignalUtil;
  KeyTable2<OpCreateTrigger, OpRecordUnion> c_opCreateTrigger;
  KeyTable2<OpDropTrigger, OpRecordUnion> c_opDropTrigger;
  KeyTable2<OpAlterTrigger, OpRecordUnion> c_opAlterTrigger;
  KeyTable2<SchemaOp, OpRecordUnion> c_schemaOp; 
  KeyTable2<SchemaTransaction, OpRecordUnion> c_Trans;
  KeyTable2Ref<OpCreateObj, SchemaOp, OpRecordUnion> c_opCreateObj;
  KeyTable2Ref<OpDropObj, SchemaOp, OpRecordUnion> c_opDropObj;

  // Unique key for operation  XXX move to some system table
  Uint32 c_opRecordSequence;

  void handleNdbdFailureCallback(Signal* signal, 
                                 Uint32 failedNodeId,
                                 Uint32 ignoredRc);
  void handleApiFailureCallback(Signal* signal,
                                Uint32 failedNodeId,
                                Uint32 ignoredRc);
  // Statement blocks

  /* ------------------------------------------------------------ */
  // Start/Restart Handling
  /* ------------------------------------------------------------ */
  void sendSTTORRY(Signal* signal);
  void sendNDB_STTORRY(Signal* signal);
  void initSchemaFile(Signal* signal);
  
  /* ------------------------------------------------------------ */
  // Drop Table Handling
  /* ------------------------------------------------------------ */
  void releaseTableObject(Uint32 tableId, bool removeFromHash = true);
  
  /* ------------------------------------------------------------ */
  // General Stuff
  /* ------------------------------------------------------------ */
  Uint32 getFreeObjId(Uint32 minId);
  Uint32 getFreeTableRecord(Uint32 primaryTableId);
  Uint32 getFreeTriggerRecord();
  bool getNewAttributeRecord(TableRecordPtr tablePtr,
			     AttributeRecordPtr & attrPtr);
  void packTableIntoPages(Signal* signal);
  void packTableIntoPages(SimpleProperties::Writer &, TableRecordPtr, Signal* =0);
  void packFilegroupIntoPages(SimpleProperties::Writer &,
			      FilegroupPtr,
			      const Uint32 undo_free_hi,
			      const Uint32 undo_free_lo);
  void packFileIntoPages(SimpleProperties::Writer &, FilePtr, const Uint32);
  
  void sendGET_TABINFOREQ(Signal* signal,
                          Uint32 tableId);
  void sendTC_SCHVERREQ(Signal* signal,
                        Uint32 tableId,
                        BlockReference tcRef);
  
  /* ------------------------------------------------------------ */
  // System Restart Handling
  /* ------------------------------------------------------------ */
  void initSendSchemaData(Signal* signal);
  void sendSchemaData(Signal* signal);
  Uint32 sendSCHEMA_INFO(Signal* signal, Uint32 nodeId, Uint32* pagePointer);
  void checkSchemaStatus(Signal* signal);
  void sendDIHSTARTTAB_REQ(Signal* signal);
  
  /* ------------------------------------------------------------ */
  // Receive Table Handling
  /* ------------------------------------------------------------ */
  void handleTabInfoInit(SimpleProperties::Reader &, 
			 ParseDictTabInfoRecord *,
			 bool checkExist = true);
  void handleTabInfo(SimpleProperties::Reader & it, ParseDictTabInfoRecord *,
		     DictTabInfo::Table & tableDesc);
  
  void handleAddTableFailure(Signal* signal,
                             Uint32 failureLine,
                             Uint32 tableId);
  bool verifyTableCorrect(Signal* signal, Uint32 tableId);
  
  /* ------------------------------------------------------------ */
  // Add Table Handling
  /* ------------------------------------------------------------ */
  void releaseCreateTableOp(Signal* signal, CreateTableRecordPtr createTabPtr);

  /* ------------------------------------------------------------ */
  // Add Fragment Handling
  /* ------------------------------------------------------------ */
  void sendLQHADDATTRREQ(Signal*, CreateTableRecordPtr, Uint32 attributePtrI);
  
  /* ------------------------------------------------------------ */
  // Read/Write Schema and Table files
  /* ------------------------------------------------------------ */
  void updateSchemaState(Signal* signal, Uint32 tableId, 
			 SchemaFile::TableEntry*, Callback*,
                         bool savetodisk = 1);
  void startWriteSchemaFile(Signal* signal);
  void openSchemaFile(Signal* signal,
                      Uint32 fileNo,
                      Uint32 fsPtr,
                      bool writeFlag,
                      bool newFile);
  void writeSchemaFile(Signal* signal, Uint32 filePtr, Uint32 fsPtr);
  void writeSchemaConf(Signal* signal,
                               FsConnectRecordPtr fsPtr);
  void closeFile(Signal* signal, Uint32 filePtr, Uint32 fsPtr);
  void closeWriteSchemaConf(Signal* signal,
                               FsConnectRecordPtr fsPtr);
  void initSchemaFile_conf(Signal* signal, Uint32 i, Uint32 returnCode);
  
  void writeTableFile(Signal* signal, Uint32 tableId, 
		      SegmentedSectionPtr tabInfo, Callback*);
  void startWriteTableFile(Signal* signal, Uint32 tableId);
  void openTableFile(Signal* signal, 
                     Uint32 fileNo,
                     Uint32 fsPtr,
                     Uint32 tableId,
                     bool writeFlag);
  void writeTableFile(Signal* signal, Uint32 filePtr, Uint32 fsPtr);
  void writeTableConf(Signal* signal,
                      FsConnectRecordPtr fsPtr);
  void closeWriteTableConf(Signal* signal,
                           FsConnectRecordPtr fsPtr);

  void startReadTableFile(Signal* signal, Uint32 tableId);
  void openReadTableRef(Signal* signal,
                        FsConnectRecordPtr fsPtr);
  void readTableFile(Signal* signal, Uint32 filePtr, Uint32 fsPtr);
  void readTableConf(Signal* signal,
                     FsConnectRecordPtr fsPtr);
  void readTableRef(Signal* signal,
                    FsConnectRecordPtr fsPtr);
  void closeReadTableConf(Signal* signal,
                          FsConnectRecordPtr fsPtr);

  void startReadSchemaFile(Signal* signal);
  void openReadSchemaRef(Signal* signal,
                         FsConnectRecordPtr fsPtr);
  void readSchemaFile(Signal* signal, Uint32 filePtr, Uint32 fsPtr);
  void readSchemaConf(Signal* signal, FsConnectRecordPtr fsPtr);
  void readSchemaRef(Signal* signal, FsConnectRecordPtr fsPtr);
  void closeReadSchemaConf(Signal* signal,
                           FsConnectRecordPtr fsPtr);
  bool convertSchemaFileTo_5_0_6(XSchemaFile*);

  /* ------------------------------------------------------------ */
  // Get table definitions
  /* ------------------------------------------------------------ */
  void sendGET_TABINFOREF(Signal* signal, 
			  GetTabInfoReq*,
			  GetTabInfoRef::ErrorCode errorCode);

  void sendGET_TABLEID_REF(Signal* signal, 
			   GetTableIdReq * req,
			   GetTableIdRef::ErrorCode errorCode);

  void sendGetTabResponse(Signal* signal);

  /* ------------------------------------------------------------ */
  // Indexes and triggers
  /* ------------------------------------------------------------ */

  // reactivate and rebuild indexes on start up
  void activateIndexes(Signal* signal, Uint32 i);
  void rebuildIndexes(Signal* signal, Uint32 i);

  // create index
  void createIndex_recvReply(Signal* signal, const CreateIndxConf* conf,
      const CreateIndxRef* ref);
  void createIndex_slavePrepare(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_toCreateTable(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_fromCreateTable(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_toAlterIndex(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_fromAlterIndex(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_slaveCommit(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_slaveAbort(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_sendSlaveReq(Signal* signal, OpCreateIndexPtr opPtr);
  void createIndex_sendReply(Signal* signal, OpCreateIndexPtr opPtr, bool);
  // drop index
  void dropIndex_recvReply(Signal* signal, const DropIndxConf* conf,
      const DropIndxRef* ref);
  void dropIndex_slavePrepare(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_toAlterIndex(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_fromAlterIndex(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_toDropTable(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_fromDropTable(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_slaveCommit(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_slaveAbort(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_sendSlaveReq(Signal* signal, OpDropIndexPtr opPtr);
  void dropIndex_sendReply(Signal* signal, OpDropIndexPtr opPtr, bool);
  // alter index
  void alterIndex_recvReply(Signal* signal, const AlterIndxConf* conf,
      const AlterIndxRef* ref);
  void alterIndex_slavePrepare(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_toCreateTc(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_fromCreateTc(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_toDropTc(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_fromDropTc(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_toCreateTrigger(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_fromCreateTrigger(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_toDropTrigger(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_fromDropTrigger(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_toBuildIndex(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_fromBuildIndex(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_slaveCommit(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_slaveAbort(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_sendSlaveReq(Signal* signal, OpAlterIndexPtr opPtr);
  void alterIndex_sendReply(Signal* signal, OpAlterIndexPtr opPtr, bool);
  // build index
  void buildIndex_recvReply(Signal* signal, const BuildIndxConf* conf,
      const BuildIndxRef* ref);
  void buildIndex_toCreateConstr(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_fromCreateConstr(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_buildTrix(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_toDropConstr(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_fromDropConstr(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_toOnline(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_fromOnline(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_sendSlaveReq(Signal* signal, OpBuildIndexPtr opPtr);
  void buildIndex_sendReply(Signal* signal, OpBuildIndexPtr opPtr, bool);

  // Events
  void 
  createEventUTIL_PREPARE(Signal* signal,
			  Uint32 callbackData,
			  Uint32 returnCode);
  void 
  createEventUTIL_EXECUTE(Signal *signal, 
			  Uint32 callbackData,
			  Uint32 returnCode);
  void 
  dropEventUTIL_PREPARE_READ(Signal* signal,
			     Uint32 callbackData,
			     Uint32 returnCode);
  void 
  dropEventUTIL_EXECUTE_READ(Signal* signal,
			     Uint32 callbackData,
			     Uint32 returnCode);
  void
  dropEventUTIL_PREPARE_DELETE(Signal* signal,
			       Uint32 callbackData,
			       Uint32 returnCode);
  void 
  dropEventUTIL_EXECUTE_DELETE(Signal *signal, 
			       Uint32 callbackData,
			       Uint32 returnCode);
  void
  dropEventUtilPrepareRef(Signal* signal,
			  Uint32 callbackData,
			  Uint32 returnCode);
  void
  dropEventUtilExecuteRef(Signal* signal,
			  Uint32 callbackData,
			  Uint32 returnCode);
  int
  sendSignalUtilReq(Callback *c,
		    BlockReference ref, 
		    GlobalSignalNumber gsn, 
		    Signal* signal, 
		    Uint32 length, 
		    JobBufferLevel jbuf,
		    LinearSectionPtr ptr[3],
		    Uint32 noOfSections);
  int
  recvSignalUtilReq(Signal* signal, Uint32 returnCode);

  void completeSubStartReq(Signal* signal, Uint32 ptrI,	Uint32 returnCode);
  void completeSubStopReq(Signal* signal, Uint32 ptrI, Uint32 returnCode);
  void completeSubRemoveReq(Signal* signal, Uint32 ptrI, Uint32 returnCode);
  
  void dropEvent_sendReply(Signal* signal,
			   OpDropEventPtr evntRecPtr);

  void createEvent_RT_USER_CREATE(Signal* signal,
				  OpCreateEventPtr evntRecPtr,
				  SectionHandle& handle);
  void createEventComplete_RT_USER_CREATE(Signal* signal,
					  OpCreateEventPtr evntRecPtr);
  void createEvent_RT_USER_GET(Signal*, OpCreateEventPtr, SectionHandle&);
  void createEventComplete_RT_USER_GET(Signal* signal, OpCreateEventPtr evntRecPtr);

  void createEvent_RT_DICT_AFTER_GET(Signal* signal, OpCreateEventPtr evntRecPtr);

  void createEvent_nodeFailCallback(Signal* signal, Uint32 eventRecPtrI,
				    Uint32 returnCode);
  void createEvent_sendReply(Signal* signal, OpCreateEventPtr evntRecPtr,
			     LinearSectionPtr *ptr = NULL, int noLSP = 0);

  void prepareTransactionEventSysTable (Callback *c,
					Signal* signal,
					Uint32 senderData,
					UtilPrepareReq::OperationTypeValue prepReq);
  void prepareUtilTransaction(Callback *c,
			      Signal* signal,
			      Uint32 senderData,
			      Uint32 tableId,
			      const char *tableName,
			      UtilPrepareReq::OperationTypeValue prepReq,
			      Uint32 noAttr,
			      Uint32 attrIds[],
			      const char *attrNames[]);

  void executeTransEventSysTable(Callback *c,
				 Signal *signal,
				 const Uint32 ptrI,
				 sysTab_NDBEVENTS_0& m_eventRec,
				 const Uint32 prepareId,
				 UtilPrepareReq::OperationTypeValue prepReq);
  void executeTransaction(Callback *c,
			  Signal* signal, 
			  Uint32 senderData,
			  Uint32 prepareId,
			  Uint32 noAttr,
			  LinearSectionPtr headerPtr,
			  LinearSectionPtr dataPtr);

  void parseReadEventSys(Signal *signal, sysTab_NDBEVENTS_0& m_eventRec);
  bool upgrade_suma_NotStarted(Uint32 err, Uint32 ref) const;

  // create trigger
  void createTrigger_recvReply(Signal* signal, const CreateTrigConf* conf,
      const CreateTrigRef* ref);
  void createTrigger_slavePrepare(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_masterSeize(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_slaveCreate(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_toAlterTrigger(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_fromAlterTrigger(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_slaveCommit(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_slaveAbort(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_sendSlaveReq(Signal* signal, OpCreateTriggerPtr opPtr);
  void createTrigger_sendReply(Signal* signal, OpCreateTriggerPtr opPtr, bool);
  // drop trigger
  void dropTrigger_recvReply(Signal* signal, const DropTrigConf* conf,
      const DropTrigRef* ref);
  void dropTrigger_slavePrepare(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_toAlterTrigger(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_fromAlterTrigger(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_slaveCommit(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_slaveAbort(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_sendSlaveReq(Signal* signal, OpDropTriggerPtr opPtr);
  void dropTrigger_sendReply(Signal* signal, OpDropTriggerPtr opPtr, bool);
  // alter trigger
  void alterTrigger_recvReply(Signal* signal, const AlterTrigConf* conf,
      const AlterTrigRef* ref);
  void alterTrigger_slavePrepare(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_toCreateLocal(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_fromCreateLocal(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_toDropLocal(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_fromDropLocal(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_slaveCommit(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_slaveAbort(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_sendSlaveReq(Signal* signal, OpAlterTriggerPtr opPtr);
  void alterTrigger_sendReply(Signal* signal, OpAlterTriggerPtr opPtr, bool);
  // support
  void getTableKeyList(TableRecordPtr, 
		       Id_array<MAX_ATTRIBUTES_IN_INDEX+1>& list);
  void getIndexAttr(TableRecordPtr indexPtr, Uint32 itAttr, Uint32* id);
  void getIndexAttrList(TableRecordPtr indexPtr, AttributeList& list);
  void getIndexAttrMask(TableRecordPtr indexPtr, AttributeMask& mask);

  /* ------------------------------------------------------------ */
  // Initialisation
  /* ------------------------------------------------------------ */
  void initCommonData();
  void initRecords();
  void initConnectRecord();
  void initRetrieveRecord(Signal*, Uint32, Uint32 returnCode);
  void initSchemaRecord();
  void initRestartRecord();
  void initSendSchemaRecord();
  void initReadTableRecord();
  void initWriteTableRecord();
  void initReadSchemaRecord();
  void initWriteSchemaRecord();

  void initNodeRecords();
  void initTableRecords();
  void initialiseTableRecord(TableRecordPtr tablePtr);
  void initTriggerRecords();
  void initialiseTriggerRecord(TriggerRecordPtr triggerPtr);
  void initPageRecords();

  Uint32 getFsConnRecord();

  bool getIsFailed(Uint32 nodeId) const;

  void dropTable_backup_mutex_locked(Signal* signal, Uint32, Uint32);
  void dropTableRef(Signal * signal, DropTableReq *, DropTableRef::ErrorCode);
  void dropTableWaitGci(Signal*);

  void printTables(); // For debugging only
  int handleAlterTab(AlterTabReq * req,
		     CreateTableRecord * regAlterTabPtr,
		     TableRecordPtr origTablePtr,
		     TableRecordPtr newTablePtr);
  void revertAlterTable(Signal * signal, 
			Uint32 changeMask, 
			Uint32 tableId,
			CreateTableRecord * regAlterTabPtr);
  void alterTable_backup_mutex_locked(Signal* signal, Uint32, Uint32);
  void alterTableRef(Signal * signal, 
		     AlterTableReq *, AlterTableRef::ErrorCode, 
		     ParseDictTabInfoRecord* parseRecord = NULL);
  void alterTabRef(Signal * signal, 
		   AlterTabReq *, AlterTableRef::ErrorCode, 
		   ParseDictTabInfoRecord* parseRecord = NULL);
  void alterTab_writeSchemaConf(Signal* signal, 
				Uint32 callbackData,
				Uint32 returnCode);
  void alterTab_writeTableConf(Signal* signal, 
			       Uint32 callbackData,
			       Uint32 returnCode);

  void prepDropTab_nextStep(Signal* signal, DropTableRecordPtr);
  void prepDropTab_complete(Signal* signal, DropTableRecordPtr);
  void prepDropTab_writeSchemaConf(Signal* signal, Uint32 dropTabPtrI, Uint32);

  void dropTab_localDROP_TAB_CONF(Signal* signal);
  void dropTab_nextStep(Signal* signal, DropTableRecordPtr);
  void dropTab_complete(Signal* signal, Uint32 dropTabPtrI, Uint32);
  void dropTab_writeSchemaConf(Signal* signal, Uint32 dropTabPtrI, Uint32);

  void createTab_prepare(Signal* signal, CreateTabReq * req);
  void createTab_writeSchemaConf1(Signal* signal, Uint32 callback, Uint32);
  void createTab_writeTableConf(Signal* signal, Uint32 callbackData, Uint32);
  void createTab_dih(Signal*, CreateTableRecordPtr, 
		     SegmentedSectionPtr, Callback*);
  void createTab_dihComplete(Signal* signal, Uint32 callbackData, Uint32);

  void createTab_startLcpMutex_locked(Signal* signal, Uint32, Uint32);
  void createTab_startLcpMutex_unlocked(Signal* signal, Uint32, Uint32);
  
  void createTab_commit(Signal* signal, CreateTabReq * req);  
  void createTab_writeSchemaConf2(Signal* signal, Uint32 callbackData, Uint32);
  void createTab_alterComplete(Signal*, Uint32 callbackData, Uint32);

  void createTab_drop(Signal* signal, CreateTabReq * req);
  void createTab_dropComplete(Signal* signal, Uint32 callbackData, Uint32);

  void createTab_reply(Signal* signal, CreateTableRecordPtr, Uint32 nodeId);
  void alterTab_activate(Signal*, CreateTableRecordPtr, Callback*);
  
  void restartCreateTab(Signal*, Uint32, 
			const SchemaFile::TableEntry *, 
			const SchemaFile::TableEntry *, bool);
  void restartCreateTab_readTableConf(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_writeTableConf(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_dihComplete(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_activateComplete(Signal*, Uint32 callback, Uint32);

  void restartDropTab(Signal* signal, Uint32 tableId,
                      const SchemaFile::TableEntry *, 
                      const SchemaFile::TableEntry *);
  void restartDropTab_complete(Signal*, Uint32 callback, Uint32);

  void restartDropObj(Signal*, Uint32, const SchemaFile::TableEntry *);
  void restartDropObj_readObjConf(Signal*, Uint32, Uint32);
  void restartDropObj_prepare_start_done(Signal*, Uint32, Uint32);
  void restartDropObj_prepare_complete_done(Signal*, Uint32, Uint32);
  void restartDropObj_commit_start_done(Signal*, Uint32, Uint32);
  void restartDropObj_commit_complete_done(Signal*, Uint32, Uint32);
  void restartDropObj_updateSchemaFile(Signal*, DropObjRecordPtr);
  void restartDropObj_fail(DropObjRecordPtr);

  void restart_checkSchemaStatusComplete(Signal*, Uint32 callback, Uint32);
  void restart_writeSchemaConf(Signal*, Uint32 callbackData, Uint32);
  void masterRestart_checkSchemaStatusComplete(Signal*, Uint32, Uint32);

  void sendSchemaComplete(Signal*, Uint32 callbackData, Uint32);

  void execCREATE_OBJ_REQ(Signal* signal);  
  void execCREATE_OBJ_REF(Signal* signal);  
  void execCREATE_OBJ_CONF(Signal* signal);

  void createObj_prepare_start_done(Signal* signal, Uint32 callback, Uint32);
  void createObj_writeSchemaConf1(Signal* signal, Uint32 callback, Uint32);
  void createObj_writeObjConf(Signal* signal, Uint32 callbackData, Uint32);
  void createObj_prepare_complete_done(Signal*, Uint32 callbackData, Uint32);
  void createObj_commit_start_done(Signal* signal, Uint32 callback, Uint32);
  void createObj_writeSchemaConf2(Signal* signal, Uint32 callbackData, Uint32);
  void createObj_commit_complete_done(Signal*, Uint32 callbackData, Uint32);
  void createObj_abort(Signal*, struct CreateObjReq*);
  void createObj_abort_start_done(Signal*, Uint32 callbackData, Uint32);
  void createObj_abort_writeSchemaConf(Signal*, Uint32 callbackData, Uint32);
  void createObj_abort_complete_done(Signal*, Uint32 callbackData, Uint32);  

  void schemaOp_reply(Signal* signal, SchemaTransaction *, Uint32);
  void trans_commit_start_done(Signal*, Uint32 callbackData, Uint32);
  void trans_commit_complete_done(Signal*, Uint32 callbackData, Uint32);
  void trans_abort_start_done(Signal*, Uint32 callbackData, Uint32);
  void trans_abort_complete_done(Signal*, Uint32 callbackData, Uint32);

  void execDROP_OBJ_REQ(Signal* signal);  
  void execDROP_OBJ_REF(Signal* signal);  
  void execDROP_OBJ_CONF(Signal* signal);

  void dropObj_prepare_start_done(Signal* signal, Uint32 callback, Uint32);
  void dropObj_prepare_writeSchemaConf(Signal*, Uint32 callback, Uint32);
  void dropObj_prepare_complete_done(Signal*, Uint32 callbackData, Uint32);
  void dropObj_commit_start_done(Signal*, Uint32 callbackData, Uint32);
  void dropObj_commit_writeSchemaConf(Signal*, Uint32 callback, Uint32);
  void dropObj_commit_complete_done(Signal*, Uint32 callbackData, Uint32);
  void dropObj_abort_start_done(Signal*, Uint32 callbackData, Uint32);
  void dropObj_abort_writeSchemaConf(Signal*, Uint32 callback, Uint32);
  void dropObj_abort_complete_done(Signal*, Uint32 callbackData, Uint32);
  
  void restartCreateObj(Signal*, Uint32, 
			const SchemaFile::TableEntry *,
			const SchemaFile::TableEntry *, bool);
  void restartCreateObj_readConf(Signal*, Uint32, Uint32);
  void restartCreateObj_getTabInfoConf(Signal*);
  void restartCreateObj_prepare_start_done(Signal*, Uint32, Uint32);
  void restartCreateObj_write_complete(Signal*, Uint32, Uint32);
  void restartCreateObj_prepare_complete_done(Signal*, Uint32, Uint32);
  void restartCreateObj_commit_start_done(Signal*, Uint32, Uint32);
  void restartCreateObj_commit_complete_done(Signal*, Uint32, Uint32);
  void restartCreateObj_fail(CreateObjRecordPtr);

  void execDICT_COMMIT_REQ(Signal*);
  void execDICT_COMMIT_REF(Signal*);
  void execDICT_COMMIT_CONF(Signal*);

  void execDICT_ABORT_REQ(Signal*);
  void execDICT_ABORT_REF(Signal*);
  void execDICT_ABORT_CONF(Signal*);

public:
  void createObj_commit(Signal*, struct SchemaOp*);
  void createObj_abort(Signal*, struct SchemaOp*);

  void create_fg_prepare_start(Signal* signal, SchemaOp*);
  void create_fg_prepare_complete(Signal* signal, SchemaOp*);
  void create_fg_abort_start(Signal* signal, SchemaOp*);
  void create_fg_abort_complete(Signal* signal, SchemaOp*);

  void create_file_prepare_start(Signal* signal, SchemaOp*);
  void create_file_prepare_complete(Signal* signal, SchemaOp*);
  void create_file_commit_start(Signal* signal, SchemaOp*);
  void create_file_abort_start(Signal* signal, SchemaOp*);
  void create_file_abort_complete(Signal* signal, SchemaOp*);

  void dropObj_commit(Signal*, struct SchemaOp*);
  void dropObj_abort(Signal*, struct SchemaOp*);
  void drop_file_prepare_start(Signal* signal, SchemaOp*);
  void drop_file_commit_start(Signal* signal, SchemaOp*);
  void drop_file_commit_complete(Signal* signal, SchemaOp*);
  void drop_file_abort_start(Signal* signal, SchemaOp*);
  void send_drop_file(Signal*, SchemaOp*, DropFileImplReq::RequestInfo);

  void drop_fg_prepare_start(Signal* signal, SchemaOp*);
  void drop_fg_commit_start(Signal* signal, SchemaOp*);
  void drop_fg_commit_complete(Signal* signal, SchemaOp*);
  void drop_fg_abort_start(Signal* signal, SchemaOp*);
  void send_drop_fg(Signal*, SchemaOp*, DropFilegroupImplReq::RequestInfo);

  void drop_undofile_prepare_start(Signal* signal, SchemaOp*);
  void drop_undofile_commit_complete(Signal* signal, SchemaOp*);
  
  int checkSingleUserMode(Uint32 senderRef);

  /**
   * Dict lock queue does currently uniformly handle
   *
   * - starting node
   * - schema op
   *
   * The impl. is based on DbUtil lock's (LockQueue)
   *
   * It would be very nice to use this *fully*
   * But instead of introducing extra break in schema-op
   *   a lock queue in instantiated in Dict, for easy trylock-handling
   */
  struct DictLockType;
  friend struct DictLockType;
  
  struct DictLockType {
    DictLockReq::LockType lockType;
    const char* text;
  };
  static const DictLockType* getDictLockType(Uint32 lockType);
  void sendDictLockInfoEvent(Signal*, const UtilLockReq*, const char* text);
  void debugLockInfo(Signal* signal, 
                     const char* text,
                     Uint32 rc);
  void removeStaleDictLocks(Signal* signal, const Uint32* theFailedNodes);


  Uint32 dict_lock_trylock(const DictLockReq* req);
  Uint32 dict_lock_unlock(Signal* signal, const DictLockReq* req, 
                          DictLockReq::LockType* type=0);
  
  LockQueue::Pool m_dict_lock_pool;
  LockQueue m_dict_lock;

  void sendOLD_LIST_TABLES_CONF(Signal *signal, ListTablesReq*);
  void sendLIST_TABLES_CONF(Signal *signal, ListTablesReq*);

  Uint32 c_outstanding_sub_startstop;
  NdbNodeBitmask c_sub_startstop_lock;

protected:
  virtual bool getParam(const char * param, Uint32 * retVal);
};

inline bool
Dbdict::TableRecord::isTable() const
{
  return DictTabInfo::isTable(tableType);
}

inline bool
Dbdict::TableRecord::isIndex() const
{
  return DictTabInfo::isIndex(tableType);
}

inline bool
Dbdict::TableRecord::isUniqueIndex() const
{
  return DictTabInfo::isUniqueIndex(tableType);
}

inline bool
Dbdict::TableRecord::isNonUniqueIndex() const
{
  return DictTabInfo::isNonUniqueIndex(tableType);
}

inline bool
Dbdict::TableRecord::isHashIndex() const
{
  return DictTabInfo::isHashIndex(tableType);
}

inline bool
Dbdict::TableRecord::isOrderedIndex() const
{
  return DictTabInfo::isOrderedIndex(tableType);
}


#endif
