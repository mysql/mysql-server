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

#ifndef DBDICT_H
#define DBDICT_H

/**
 * Dict : Dictionary Block
 */

#include <ndb_limits.h>
#include <trigger_definitions.h>
#include <pc.hpp>
#include <ArrayList.hpp>
#include <DLHashTable.hpp>
#include <CArray.hpp>
#include <KeyTable2.hpp>
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
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/AlterIndx.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/CreateEvnt.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include "SchemaFile.hpp"
#include <blocks/mutexes.hpp>
#include <SafeCounter.hpp>
#include <RequestTracker.hpp>

#ifdef DBDICT_C
// Debug Macros

/*--------------------------------------------------------------*/
// Constants for CONTINUEB
/*--------------------------------------------------------------*/
#define ZPACK_TABLE_INTO_PAGES 0
#define ZSEND_GET_TAB_RESPONSE 3


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
#define ZALLOCATE 1 //Variable number of page for NDBFS
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
#define ZNUMBER_OF_PAGES (2 * ZMAX_PAGES_OF_TABLE_DEFINITION + 2)
#define ZNO_OF_FRAGRECORD 5

/*--------------------------------------------------------------*/
// Error codes
/*--------------------------------------------------------------*/
#define ZNODE_FAILURE_ERROR 704
#endif

/**
 * Systable NDB$EVENTS_0
 */

#define EVENT_SYSTEM_TABLE_NAME "sys/def/NDB$EVENTS_0"
#define EVENT_SYSTEM_TABLE_LENGTH 6

struct sysTab_NDBEVENTS_0 {
  char   NAME[MAX_TAB_NAME_SIZE];
  Uint32 EVENT_TYPE;
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
   * Shared table / index record.  Most of this is permanent data stored
   * on disk.  Index trigger ids are volatile.
   */
  struct TableRecord : public MetaData::Table {
    /****************************************************
     *    Support variables for table handling
     ****************************************************/

    /*     Active page which is sent to disk */
    Uint32 activePage;

    /**    File pointer received from disk   */
    Uint32 filePtr[2];

    /**    Pointer to first attribute in table */
    Uint32 firstAttribute;

    /*    Pointer to first page of table description */
    Uint32 firstPage;

    /**    Pointer to last attribute in table */
    Uint32 lastAttribute;

#ifdef HAVE_TABLE_REORG    
    /*    Second table used by this table (for table reorg) */
    Uint32 secondTable;
#endif
    /*    Next record in Pool */
    Uint32 nextPool;

    /*    Next record in hash table */
    Uint32 nextHash;

    /*    Previous record in Pool */
    Uint32 prevPool;

    /*    Previous record in hash table */
    Uint32 prevHash;

    enum TabState {
      NOT_DEFINED = 0,
      REORG_TABLE_PREPARED = 1,
      DEFINING = 2,
      CHECKED = 3,
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

    inline bool equal(TableRecord & rec) const {
      return strcmp(tableName, rec.tableName) == 0;
    }

    inline Uint32 hashValue() const {
      Uint32 h = 0;
      for (const char* p = tableName; *p != 0; p++)
        h = (h << 5) + h + (*p);
      return h;
    }

    /**  frm data for this table */
    /** TODO Could preferrably be made dynamic size */
    Uint32 frmLen;
    char frmData[MAX_FRM_DATA_SIZE];

    Uint32 fragmentCount;
  };

  typedef Ptr<TableRecord> TableRecordPtr;
  ArrayPool<TableRecord> c_tableRecordPool;
  DLHashTable<TableRecord> c_tableRecordHash;

  /**
   * Table attributes.  Permanent data.
   *
   * Indexes have an attribute list which duplicates primary table
   * attributes.  This is wrong but convenient.
   */
  struct AttributeRecord : public MetaData::Attribute {
    union {    
    /** Pointer to the next attribute used by ArrayPool */
    Uint32 nextPool;

    /** Pointer to the next attribute used by DLHash */
    Uint32 nextHash;
    };

    /** Pointer to the previous attribute used by DLHash */
    Uint32 prevHash;

    /** Pointer to the next attribute in table */
    Uint32 nextAttrInTable;

    inline bool equal(AttributeRecord & rec) const {
      return strcmp(attributeName, rec.attributeName) == 0;
    }

    inline Uint32 hashValue() const {
      Uint32 h = 0;
      for (const char* p = attributeName; *p != 0; p++)
        h = (h << 5) + h + (*p);
      return h;
    }
  };

  typedef Ptr<AttributeRecord> AttributeRecordPtr;
  ArrayPool<AttributeRecord> c_attributeRecordPool;
  DLHashTable<AttributeRecord> c_attributeRecordHash;

  /**
   * Triggers.  This is volatile data not saved on disk.  Setting a
   * trigger online creates the trigger in TC (if index) and LQH-TUP.
   */
  struct TriggerRecord {

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
    char triggerName[MAX_TAB_NAME_SIZE];

    /** Trigger id, used by TRIX, TC, LQH, and TUP to identify the trigger */
    Uint32 triggerId;

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
    
    /**
     * Attribute mask, defines what attributes are to be monitored.
     * Can be seen as a compact representation of SQL column name list.
     */
    AttributeMask attributeMask;

    /** Index id, only used by secondary_index triggers */
    Uint32 indexId;

    union {
    /** Pointer to the next attribute used by ArrayPool */
    Uint32 nextPool;

    /** Next record in hash table */
    Uint32 nextHash;
    };
    
    /** Previous record in hash table */
    Uint32 prevHash;

    /** Equal function, used by DLHashTable */
    inline bool equal(TriggerRecord & rec) const {
       return strcmp(triggerName, rec.triggerName) == 0;
    }
    
    /** Hash value function, used by DLHashTable */
    inline Uint32 hashValue() const {
      Uint32 h = 0;
      for (const char* p = triggerName; *p != 0; p++)
        h = (h << 5) + h + (*p);
      return h;
    }
  };
  
  Uint32 c_maxNoOfTriggers;
  typedef Ptr<TriggerRecord> TriggerRecordPtr;
  ArrayPool<TriggerRecord> c_triggerRecordPool;
  DLHashTable<TriggerRecord> c_triggerRecordHash;

  /**
   * Information for each FS connection.
   ****************************************************************************/
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
   ****************************************************************************/
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
  
  /**
   * This record stores all the information about a table and all its attributes
   ****************************************************************************/
  struct PageRecord {
    Uint32 word[8192];
  };
  
  typedef Ptr<PageRecord> PageRecordPtr;
  CArray<PageRecord> c_pageRecordArray;

  /**
   * A page for create index table signal.
   */
  PageRecord c_indexPage;

public:
  Dbdict(const class Configuration &);
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

  void execSUB_SYNC_CONF(Signal* signal);
  void execSUB_SYNC_REF (Signal* signal);

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
    Uint32 noOfPages;
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
    Uint32 noOfPages;
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
    /** State, indicates from where it was called */
    enum SchemaReadState {
      IDLE = 0,
      INITIAL_READ = 1
    };
    SchemaReadState schemaReadState;
  };
  ReadSchemaRecord c_readSchemaRecord;

private:
  /**
   * This record stores all the state needed 
   * when a schema file is being written to disk
   ****************************************************************************/
  struct WriteSchemaRecord {
    /** Page Id of schema page */
    Uint32 pageId;
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
    /**    Schema page       */
    Uint32 schemaPage;

    /**    Old Schema page (used at node restart)   */
    Uint32 oldSchemaPage;
    
    Callback m_callback;
  };
  SchemaRecord c_schemaRecord;

  void initSchemaFile(SchemaFile *, Uint32 sz);
  void computeChecksum(SchemaFile *);
  bool validateChecksum(const SchemaFile *);
  SchemaFile::TableEntry * getTableEntry(void * buf, Uint32 tableId, 
					 bool allowTooBig = false);

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
  
  enum BlockState {
    BS_IDLE = 0,
    BS_CREATE_TAB = 1,
    BS_BUSY = 2,
    BS_NODE_FAILURE = 3
  };
  BlockState c_blockState;

  struct PackTable {
    
    enum PackTableState {
      PTS_IDLE = 0,
      PTS_ADD_TABLE_MASTER = 1,
      PTS_ADD_TABLE_SLAVE = 2,
      PTS_GET_TAB = 3,
      PTS_RESTART = 4
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
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_coordinatorRef;
    
    Uint32 m_errorCode;
    void setErrorCode(Uint32 c){ if(m_errorCode == 0) m_errorCode = c;}

    // For alter table
    Uint32 m_changeMask;
    bool m_alterTableFailed;
    AlterTableRef m_alterTableRef;
    Uint32 m_alterTableId;

    /* Previous table name (used for reverting failed table rename) */
    char previousTableName[MAX_TAB_NAME_SIZE];

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
      SignalCounter m_signalCounter;
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
      RF_NOTCTRIGGER = 1 << 2   // alter trigger: no trigger in TC
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
    bool m_storedIndex;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    CreateIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    // error info
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
      m_errorCode = CreateIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const CreateIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != CreateIndxRef::NoError;
    }
    void setError(const CreateIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const CreateTableRef* ref) {
      if (ref != 0 && ! hasError()) {
        switch (ref->getErrorCode()) {
        case CreateTableRef::TableAlreadyExist:
          m_errorCode = CreateIndxRef::IndexExists;
          break;
        default:
          m_errorCode = (CreateIndxRef::ErrorCode)ref->getErrorCode();
          break;
        }
        m_errorLine = ref->getErrorLine();
      }
    }
    void setError(const AlterIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (CreateIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
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
      m_errorCode = DropIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const DropIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != DropIndxRef::NoError;
    }
    void setError(const DropIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const AlterIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (DropIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const DropTableRef* ref) {
      if (ref != 0 && ! hasError()) {
	switch(ref->errorCode) {
	case(DropTableRef::Busy):
	  m_errorCode = DropIndxRef::Busy;
	  break;
	case(DropTableRef::NoSuchTable):
	  m_errorCode = DropIndxRef::IndexNotFound;
	  break;
	case(DropTableRef::DropInProgress):
	  m_errorCode = DropIndxRef::Busy;
	  break;
	case(DropTableRef::NoDropTableRecordAvailable):
	  m_errorCode = DropIndxRef::Busy;
	  break;
	default:
	  m_errorCode = (DropIndxRef::ErrorCode)ref->errorCode;
	  break;
	}
        //m_errorLine = ref->getErrorLine();
        //m_errorNode = ref->getErrorNode();
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
    bool hasError() {
      return m_errorCode != AlterIndxRef::NoError;
    }
    void setError(const AlterIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const CreateIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const DropIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const BuildIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterIndxRef::ErrorCode)ref->getErrorCode();
      }
    }
    void setError(const CreateTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const DropTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
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
    AttributeList m_tableKeyList;
    // coordinator DICT
    Uint32 m_coordinatorRef;
    bool m_isMaster;
    // state info
    BuildIndxReq::RequestType m_requestType;
    Uint32 m_requestFlag;
    Uint32 m_constrTriggerId;
    // error info
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
//      Uint32 m_constrTriggerId = RNIL;
      m_errorCode = BuildIndxRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const BuildIndxReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != BuildIndxRef::NoError;
    }
    void setError(const BuildIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
      }
    }
    void setError(const AlterIndxRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const CreateTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const DropTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (BuildIndxRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
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
    Uint32 m_requestFlag;
    // error info
    CreateEvntRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // ctor
    OpCreateEvent() {
      memset(&m_request, 0, sizeof(m_request));
      m_requestType = CreateEvntReq::RT_UNDEFINED;
      m_requestFlag = 0;
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
      m_requestFlag = req->getRequestFlag();
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
    DropEvntRef::ErrorCode m_errorCode;
    Uint32 m_errorLine;
    Uint32 m_errorNode;
    // ctor
    OpDropEvent() {
      memset(&m_request, 0, sizeof(m_request));
      m_errorCode = DropEvntRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void init(const DropEvntReq* req) {
      m_request = *req;
      m_errorCode = DropEvntRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    bool hasError() {
      return m_errorCode != DropEvntRef::NoError;
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
      m_errorCode = CreateTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const CreateTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != CreateTrigRef::NoError;
    }
    void setError(const CreateTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const AlterTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (CreateTrigRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
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
      m_errorCode = DropTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const DropTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != DropTrigRef::NoError;
    }
    void setError(const DropTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const AlterTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (DropTrigRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
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
      m_errorCode = AlterTrigRef::NoError;
      m_errorLine = 0;
      m_errorNode = 0;
    }
    void save(const AlterTrigReq* req) {
      m_request = *req;
      m_requestType = req->getRequestType();
      m_requestFlag = req->getRequestFlag();
    }
    bool hasError() {
      return m_errorCode != AlterTrigRef::NoError;
    }
    void setError(const AlterTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const CreateTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
    void setError(const DropTrigRef* ref) {
      if (ref != 0 && ! hasError()) {
        m_errorCode = (AlterTrigRef::ErrorCode)ref->getErrorCode();
        m_errorLine = ref->getErrorLine();
        m_errorNode = ref->getErrorNode();
      }
    }
  };
  typedef Ptr<OpAlterTrigger> OpAlterTriggerPtr;

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
  KeyTable2<OpCreateEvent, OpRecordUnion> c_opCreateEvent;
  KeyTable2<OpSubEvent, OpRecordUnion> c_opSubEvent;
  KeyTable2<OpDropEvent, OpRecordUnion> c_opDropEvent;
  KeyTable2<OpSignalUtil, OpRecordUnion> c_opSignalUtil;
  KeyTable2<OpCreateTrigger, OpRecordUnion> c_opCreateTrigger;
  KeyTable2<OpDropTrigger, OpRecordUnion> c_opDropTrigger;
  KeyTable2<OpAlterTrigger, OpRecordUnion> c_opAlterTrigger;

  // Unique key for operation  XXX move to some system table
  Uint32 c_opRecordSequence;

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
  Uint32 getFreeTableRecord(Uint32 primaryTableId);
  Uint32 getFreeTriggerRecord();
  bool getNewAttributeRecord(TableRecordPtr tablePtr,
			     AttributeRecordPtr & attrPtr);
  void packTableIntoPages(Signal* signal, Uint32 tableId, Uint32 pageId);
  void packTableIntoPagesImpl(SimpleProperties::Writer &, TableRecordPtr);
  
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
  void handleTabInfo(SimpleProperties::Reader & it, ParseDictTabInfoRecord *);
  
  void handleAddTableFailure(Signal* signal,
                             Uint32 failureLine,
                             Uint32 tableId);
  bool verifyTableCorrect(Signal* signal, Uint32 tableId);
  
  /* ------------------------------------------------------------ */
  // Add Table Handling
  /* ------------------------------------------------------------ */

  /* ------------------------------------------------------------ */
  // Add Fragment Handling
  /* ------------------------------------------------------------ */
  void sendLQHADDATTRREQ(Signal*, CreateTableRecordPtr, Uint32 attributePtrI);
  
  /* ------------------------------------------------------------ */
  // Read/Write Schema and Table files
  /* ------------------------------------------------------------ */
  void updateSchemaState(Signal* signal, Uint32 tableId, 
			 SchemaFile::TableEntry*, Callback*);
  void startWriteSchemaFile(Signal* signal);
  void openSchemaFile(Signal* signal,
                      Uint32 fileNo,
                      Uint32 fsPtr,
                      bool writeFlag);
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

  void createEvent_RT_USER_CREATE(Signal* signal, OpCreateEventPtr evntRecPtr);
  void createEventComplete_RT_USER_CREATE(Signal* signal,
					  OpCreateEventPtr evntRecPtr);
  void createEvent_RT_USER_GET(Signal* signal, OpCreateEventPtr evntRecPtr);
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
  void getTableKeyList(TableRecordPtr tablePtr, AttributeList& list);
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
  
  void restartCreateTab(Signal*, Uint32, const SchemaFile::TableEntry *, bool);
  void restartCreateTab_readTableConf(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_writeTableConf(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_dihComplete(Signal* signal, Uint32 callback, Uint32);
  void restartCreateTab_activateComplete(Signal*, Uint32 callback, Uint32);

  void restartDropTab(Signal* signal, Uint32 tableId);
  void restartDropTab_complete(Signal*, Uint32 callback, Uint32);
  
  void restart_checkSchemaStatusComplete(Signal*, Uint32 callback, Uint32);
  void restart_writeSchemaConf(Signal*, Uint32 callbackData, Uint32);
  void masterRestart_checkSchemaStatusComplete(Signal*, Uint32, Uint32);

  void sendSchemaComplete(Signal*, Uint32 callbackData, Uint32);

  // global metadata support
  friend class MetaData;
  int getMetaTablePtr(TableRecordPtr& tablePtr, Uint32 tableId, Uint32 tableVersion);
  int getMetaTable(MetaData::Table& table, Uint32 tableId, Uint32 tableVersion);
  int getMetaTable(MetaData::Table& table, const char* tableName);
  int getMetaAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, Uint32 attributeId);
  int getMetaAttribute(MetaData::Attribute& attribute, const MetaData::Table& table, const char* attributeName);
};

#endif
