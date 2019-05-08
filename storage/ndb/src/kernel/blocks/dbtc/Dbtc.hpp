/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DBTC_H
#define DBTC_H

#ifndef DBTC_STATE_EXTRACT
#include <ndb_limits.h>
#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <RWPool.hpp>
#include <DLHashTable.hpp>
#include <IntrusiveList.hpp>
#include <DataBuffer.hpp>
#include <Bitmask.hpp>
#include <AttributeList.hpp>
#include <signaldata/DihScanTab.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/LqhTransConf.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/EventReport.hpp>
#include <trigger_definitions.h>
#include <SignalCounter.hpp>
#include <KeyTable.hpp>
#include <portlib/NdbTick.h>
#include "TransientPool.hpp"
#include "TransientSlotPool.hpp"
#endif


#define JAM_FILE_ID 350

#define TIME_TRACK_HISTOGRAM_RANGES 32
#define TIME_TRACK_LOG_HISTOGRAM_RANGES 5
#define TIME_TRACK_INITIAL_RANGE_VALUE 50

#ifdef DBTC_C
/*
 * 2.2 LOCAL SYMBOLS
 * -----------------
 */
#define Z8NIL 255
#define ZCLOSED 2
#define ZCOMMITING 0			 /* VALUE FOR TRANSTATUS        */
#define ZCOMMIT_SETUP 2
#define ZCONTINUE_ABORT_080 4
#define ZGCP_FILESIZE 10
#define ZINTSPH1 1
#define ZINTSPH2 2
#define ZINTSPH3 3
#define ZINTSPH6 6
#define ZLASTPHASE 255
#define ZNODEBUF_FILESIZE 2000
#define ZNR_OF_SEIZE 10
#define ZSCANREC_FILE_SIZE 100
#define ZSCAN_OPREC_FILE_SIZE 400
#define ZSPH1 1
#define ZTABREC_FILESIZE 16
#define ZTAKE_OVER_ACTIVE 1
#define ZTAKE_OVER_IDLE 0
#define ZTC_CONNECT_FILESIZE 200
#define ZTCOPCONF_SIZE 6

// ----------------------------------------
// Error Codes for Scan
// ----------------------------------------
#define ZNO_CONCURRENCY_ERROR 242
#define ZTOO_HIGH_CONCURRENCY_ERROR 244
#define ZNO_SCANREC_ERROR 245
#define ZNO_FRAGMENT_ERROR 246
#define ZSCAN_AI_LEN_ERROR 269
#define ZSCAN_LQH_ERROR 270
#define ZSCAN_FRAG_LQH_ERROR 274

#define ZSCANTIME_OUT_ERROR 296
#define ZSCANTIME_OUT_ERROR2 297

// ----------------------------------------
// Error Codes for transactions
// ----------------------------------------
#define ZSTATE_ERROR 202
#define ZLENGTH_ERROR 207 // Also Scan
#define ZERO_KEYLEN_ERROR 208
#define ZSIGNAL_ERROR 209
#define ZGET_ATTRBUF_ERROR 217 // Also Scan
#define ZGET_DATAREC_ERROR 218
#define ZMORE_AI_IN_TCKEYREQ_ERROR 220
#define ZTOO_MANY_FIRED_TRIGGERS 221
#define ZCOMMITINPROGRESS 230
#define ZROLLBACKNOTALLOWED 232
#define ZNO_FREE_TC_CONNECTION 233 // Also Scan
#define ZABORTINPROGRESS 237
#define ZPREPAREINPROGRESS 238
#define ZWRONG_SCHEMA_VERSION_ERROR 241 // Also Scan
#define ZSCAN_NODE_ERROR 250
#define ZTRANS_STATUS_ERROR 253
#define ZTIME_OUT_ERROR 266
#define ZSIMPLE_READ_WITHOUT_AI 271
#define ZNO_AI_WITH_UPDATE 272
#define ZSEIZE_API_COPY_ERROR 275
#define ZSCANINPROGRESS 276
#define ZABORT_ERROR 277
#define ZCOMMIT_TYPE_ERROR 278

#define ZNO_FREE_TC_MARKER 279
#define ZNO_FREE_TC_MARKER_DATABUFFER 273
#define ZNODE_SHUTDOWN_IN_PROGRESS 280
#define ZCLUSTER_SHUTDOWN_IN_PROGRESS 281
#define ZWRONG_STATE 282
#define ZINCONSISTENT_TRIGGER_STATE 293
#define ZCLUSTER_IN_SINGLEUSER_MODE 299

#define ZDROP_TABLE_IN_PROGRESS 283
#define ZNO_SUCH_TABLE 284
#define ZUNKNOWN_TABLE_ERROR 285
#define ZNODEFAIL_BEFORE_COMMIT 286
#define ZINDEX_CORRUPT_ERROR 287
#define ZSCAN_FRAGREC_ERROR 291
#define ZMISSING_TRIGGER_DATA 240
#define ZINCONSISTENT_INDEX_USE 4349

// ----------------------------------------
// Seize error
// ----------------------------------------
#define ZNO_FREE_API_CONNECTION 219
#define ZSYSTEM_NOT_STARTED_ERROR 203

// ----------------------------------------
// Release errors
// ----------------------------------------
#define ZINVALID_CONNECTION 229


#define ZNOT_FOUND 626
#define ZALREADYEXIST 630
#define ZNOTUNIQUE 893
#define ZFK_NO_PARENT_ROW_EXISTS 255
#define ZFK_CHILD_ROW_EXISTS 256

#define ZINVALID_KEY 290
#define ZUNLOCKED_IVAL_TOO_HIGH 294
#define ZUNLOCKED_OP_HAS_BAD_STATE 295 
#define ZBAD_DIST_KEY 298
#define ZTRANS_TOO_BIG 261
#endif

class Dbtc
#ifndef DBTC_STATE_EXTRACT
  : public SimulatedBlock
#endif
{
public:

#ifndef DBTC_STATE_EXTRACT
  /**
   * Incase of mt-TC...only one instance will perform actual take-over
   *   let this be TAKE_OVER_INSTANCE
   */
  STATIC_CONST( TAKE_OVER_INSTANCE = 1 );
#endif

  enum ConnectionState {
    CS_CONNECTED = 0,
    CS_DISCONNECTED = 1,
    CS_STARTED = 2,
    CS_RECEIVING = 3,
    CS_RESTART = 7,
    CS_ABORTING = 8,
    CS_COMPLETING = 9,
    CS_COMPLETE_SENT = 10,
    CS_PREPARE_TO_COMMIT = 11,
    CS_COMMIT_SENT = 12,
    CS_START_COMMITTING = 13,
    CS_COMMITTING = 14,
    CS_REC_COMMITTING = 15,
    CS_WAIT_ABORT_CONF = 16,
    CS_WAIT_COMPLETE_CONF = 17,
    CS_WAIT_COMMIT_CONF = 18,
    CS_FAIL_ABORTING = 19,
    CS_FAIL_ABORTED = 20,
    CS_FAIL_PREPARED = 21,
    CS_FAIL_COMMITTING = 22,
    CS_FAIL_COMMITTED = 23,
    CS_FAIL_COMPLETED = 24,
    CS_START_SCAN = 25,

    /**
     * Sending FIRE_TRIG_REQ
     */
    CS_SEND_FIRE_TRIG_REQ = 26,

    /**
     * Waiting for FIRE_TRIG_CONF/REF (or operations generated by this)
     */
    CS_WAIT_FIRE_TRIG_REQ = 27
  };

#ifndef DBTC_STATE_EXTRACT
  enum OperationState {
    OS_CONNECTED = 1,
    OS_OPERATING = 2,
    OS_PREPARED = 3,
    OS_COMMITTING = 4,
    OS_COMMITTED = 5,
    OS_COMPLETING = 6,
    OS_COMPLETED = 7,

    OS_ABORTING = 9,
    OS_ABORT_SENT = 10,
    OS_TAKE_OVER = 11,
    OS_WAIT_DIH = 12,
    OS_WAIT_KEYINFO = 13,
    OS_WAIT_ATTR = 14,
    OS_WAIT_COMMIT_CONF = 15,
    OS_WAIT_ABORT_CONF = 16,
    OS_WAIT_COMPLETE_CONF = 17,

    OS_FIRE_TRIG_REQ = 19,
  };

  enum AbortState {
    AS_IDLE = 0,
    AS_ACTIVE = 1
  };

  enum HostState {
    HS_ALIVE = 0,
    HS_DEAD = 1
  };

  enum LqhTransState {
    LTS_IDLE = 0,
    LTS_ACTIVE = 1
  };

  enum FailState {
    FS_IDLE = 0,
    FS_LISTENING = 1,
    FS_COMPLETING = 2
  };

  enum SystemStartState {
    SSS_TRUE = 0,
    SSS_FALSE = 1
  };

  enum TimeOutCheckState {
    TOCS_TRUE = 0,
    TOCS_FALSE = 1
  };

  enum ReturnSignal {
    RS_NO_RETURN = 0,
    RS_TCKEYCONF = 1,
    RS_TC_COMMITCONF = 3,
    RS_TCROLLBACKCONF = 4,
    RS_TCROLLBACKREP = 5
  };
  
  enum IndexOperationState {
    IOS_NOOP = 0,
    IOS_INDEX_ACCESS = 1,
    IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF = 2,
    IOS_INDEX_ACCESS_WAIT_FOR_TRANSID_AI = 3
  };
  
  enum IndexState {
    IS_BUILDING = 0,          // build in progress, start state at create
    IS_ONLINE = 1,            // ready to use
    IS_OFFLINE = 2            // not in use
  };

  /* Sub states of IndexOperation while waiting for TransId_AI
   * from index table lookup
   */
  enum IndexTransIdAIState {
    ITAS_WAIT_HEADER   = 0,     // Initial state
    ITAS_WAIT_FRAGID   = 1,     // Waiting for fragment id word
    ITAS_WAIT_KEY      = 2,     // Waiting for (more) key information
    ITAS_ALL_RECEIVED  = 3,     // All TransIdAI info received
    ITAS_WAIT_KEY_FAIL = 4     // Failed collecting key
  };
  
  /**--------------------------------------------------------------------------
   * LOCAL SYMBOLS PER 'SYMBOL-VALUED' VARIABLE
   *
   *
   *            NSYMB ZTC_CONNECT_FILESIZE  = 200
   *            NSYMB ZHOST_FILESIZE        = 16  
   *            NSYMB ZDATABUF_FILESIZE     = 4000
   *            NSYMB ZATTRBUF_FILESIZE     = 4000 
   *            NSYMB ZGCP_FILESIZE         = 10 
   *
   *
   *  ABORTED CODES
   *  TPHASE    NSYMB ZSPH1 = 1
   *            NSYMB ZLASTPHASE = 255
   *
   * 
   * LQH_TRANS
   *       NSYMB ZTRANS_ABORTED = 1 
   *       NSYMB ZTRANS_PREPARED = 2 
   *       NSYMB ZTRANS_COMMITTED = 3
   *       NSYMB ZCOMPLETED_LQH_TRANS = 4
   *       NSYMB ZTRANS_COMPLETED = 5 
   *
   * 
   * TAKE OVER 
   *       NSYMB ZTAKE_OVER_IDLE = 0
   *       NSYMB ZTAKE_OVER_ACTIVE = 1
   * 
   * ATTRBUF (ATTRBUF_RECORD)
   *          NSYMB ZINBUF_DATA_LEN = 24
   *          NSYMB ZINBUF_NEXTFREE = 25    (NOT USED )
   *          NSYMB ZINBUF_PREV = 26   
   *          NSYMB ZINBUF_NEXT = 27    
   -------------------------------------------------------------------------*/
  /*
    2.3 RECORDS AND FILESIZES
    -------------------------
  */
  typedef DataBufferSegment<11, RT_DBTC_ATTRIBUTE_BUFFER> AttributeBufferSegment;
  typedef TransientPool<AttributeBufferSegment> AttributeBuffer_pool;
  STATIC_CONST(DBTC_ATTRIBUTE_BUFFER_TRANSIENT_POOL_INDEX = 0);
  typedef DataBuffer<11, AttributeBuffer_pool, RT_DBTC_ATTRIBUTE_BUFFER>
            AttributeBuffer;
  typedef LocalDataBuffer<11, AttributeBuffer_pool, RT_DBTC_ATTRIBUTE_BUFFER>
            LocalAttributeBuffer;

  /* **************************************************************** */
  /* ---------------------------------------------------------------- */
  /* ------------------- TRIGGER AND INDEX DATA --------------------- */
  /* ---------------------------------------------------------------- */
  /* **************************************************************** */
  /* ********* DEFINED TRIGGER DATA ********* */
  /* THIS RECORD FORMS LISTS OF ACTIVE        */
  /* TRIGGERS FOR EACH TABLE.                 */
  /* THE RECORDS ARE MANAGED BY A TRIGGER     */
  /* POOL WHERE A TRIGGER RECORD IS SEIZED    */
  /* WHEN A TRIGGER IS ACTIVATED AND RELEASED */
  /* WHEN THE TRIGGER IS DEACTIVATED.         */
  /* **************************************** */
  struct TcDefinedTriggerData {
    TcDefinedTriggerData() {}
    /**
     * Trigger id, used to identify the trigger
     */
    UintR triggerId;

    Uint32 refCount;

    /**
     * Trigger type, defines what the trigger is used for
     */
    TriggerType::Value triggerType;
    
    /**
     * Trigger type, defines what the trigger is used for
     */
    TriggerEvent::Value triggerEvent;
    
    /**
     * Next ptr (used in pool/list)
     */
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    
    /**
     * Index id, only used by secondary_index triggers.  This is same as
     * index table id in DICT.
     **/
    union {
      Uint32 indexId; // For unique index trigger
      Uint32 tableId; // For reorg trigger
      Uint32 fkId;    // For FK trigger
    };

    /**
     * Prev pointer (used in list)
     */
    Uint32 prevList;

    Uint32 oldTriggerIds[2]; // For upgrade :(

    inline void print(NdbOut & s) const { 
      s << "[DefinedTriggerData = " << triggerId << "]"; 
    }
  };
  typedef Ptr<TcDefinedTriggerData> DefinedTriggerPtr;
  typedef ArrayPool<TcDefinedTriggerData> TcDefinedTriggerData_pool;
  typedef DLList<TcDefinedTriggerData_pool> TcDefinedTriggerData_list;
  
  /**
   * Pool of trigger data record
   */
  TcDefinedTriggerData_pool c_theDefinedTriggerPool;
  RSS_AP_SNAPSHOT(c_theDefinedTriggerPool);

  /**
   * The list of active triggers
   */  
  TcDefinedTriggerData_list c_theDefinedTriggers;

  alignas(64) AttributeBuffer_pool c_theAttributeBufferPool;
  RSS_AP_SNAPSHOT(c_theAttributeBufferPool);

  typedef DataBufferSegment<5, RT_DBTC_COMMIT_ACK_MARKER_BUFFER>
              CommitAckMarkerSegment;
  typedef TransientPool<CommitAckMarkerSegment> CommitAckMarkerBuffer_pool;
  STATIC_CONST(DBTC_COMMIT_ACK_MARKER_BUFFER_TRANSIENT_POOL_INDEX = 1);
  typedef DataBuffer<5,
                     CommitAckMarkerBuffer_pool,
                     RT_DBTC_COMMIT_ACK_MARKER_BUFFER> CommitAckMarkerBuffer;
  typedef LocalDataBuffer<5,
                          CommitAckMarkerBuffer_pool,
                          RT_DBTC_COMMIT_ACK_MARKER_BUFFER>
                              LocalCommitAckMarkerBuffer;

  alignas(64) CommitAckMarkerBuffer_pool c_theCommitAckMarkerBufferPool;
  RSS_AP_SNAPSHOT(c_theCommitAckMarkerBufferPool);

  /* ********** FIRED TRIGGER DATA ********** */
  /* THIS RECORD FORMS LISTS OF FIRED         */
  /* TRIGGERS FOR A TRANSACTION.              */
  /* THE RECORDS ARE MANAGED BY A TRIGGER     */
  /* POOL WHERE A TRIGGER RECORD IS SEIZED    */
  /* WHEN A TRIGGER IS ACTIVATED AND RELEASED */
  /* WHEN THE TRIGGER IS DEACTIVATED.         */
  /* **************************************** */
  struct TcFiredTriggerData
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_FIRED_TRIGGER_DATA );

    TcFiredTriggerData()
      : m_magic(Magic::make(TYPE_ID))
    {}

    Uint32 m_magic;

    /**
     * Trigger id, used to identify the trigger
     **/
    Uint32 triggerId;
    
    /**
     * The operation that fired the trigger
     */
    Uint32 fireingOperation;

    /**
     * The fragment id of the firing operation. This will be appended
     * to the Primary Key such that the record can be found even in the
     * case of user defined partitioning.
     */
    Uint32 fragId;

    /**
     * Used for scrapping in case of node failure
     */
    NodeId nodeId;

    /**
     * Trigger type, defines what the trigger is used for
     */
    TriggerType::Value triggerType;

    /**
     * Trigger type, defines what the trigger is used for
     */
    TriggerEvent::Value triggerEvent;

    /**
     * Trigger attribute info, primary key value(s)
     */
    AttributeBuffer::Head keyValues;
    
    /**
     * Trigger attribute info, attribute value(s) before operation
     */
    AttributeBuffer::Head beforeValues;
    
    /**
     * Trigger attribute info, attribute value(s) after operation
     */
    AttributeBuffer::Head afterValues;

    /**
     * Next ptr (used in list/hash)
     */
    union
    {
      Uint32 nextList;
      Uint32 nextHash;
    };
    
    /**
     * Prev pointer (used in list)
     */
    union {
      Uint32 prevList;
      Uint32 prevHash;
    };
    
    inline void print(NdbOut & s) const { 
      s << "[FiredTriggerData = " << triggerId << "]"; 
    }

    inline Uint32 hashValue() const {
      return fireingOperation ^ nodeId;
    }

    inline bool equal(const TcFiredTriggerData & rec) const {
      return fireingOperation == rec.fireingOperation && nodeId == rec.nodeId;
    }
  };
  typedef Ptr<TcFiredTriggerData> FiredTriggerPtr;
  typedef TransientPool<TcFiredTriggerData> TcFiredTriggerData_pool;
  STATIC_CONST(DBTC_FIRED_TRIGGER_DATA_TRANSIENT_POOL_INDEX = 2);
  typedef LocalDLFifoList<TcFiredTriggerData_pool> Local_TcFiredTriggerData_fifo;
  typedef DLHashTable<TcFiredTriggerData_pool> TcFiredTriggerData_hash;
  
  /**
   * Pool of trigger data record
   */
  alignas(64) TcFiredTriggerData_pool c_theFiredTriggerPool;
  TcFiredTriggerData_hash c_firedTriggerHash;
  RSS_AP_SNAPSHOT(c_theFiredTriggerPool);

  Uint32 c_maxNumberOfDefinedTriggers;

  // Max number of outstanding FireTrigRequests per transaction
  static const Uint32 MaxOutstandingFireTrigReqPerTrans = 32;

  struct AttrInfoRecord {
    /**
     * Pre-allocated AttrInfo signal
     */
    AttrInfo attrInfo;

    /**
     * Next ptr (used in pool/list)
     */
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    /**
     * Prev pointer (used in list)
     */
    Uint32 prevList;
  };


  /* ************* INDEX DATA *************** */
  /* THIS RECORD FORMS LISTS OF ACTIVE        */
  /* INDEX FOR EACH TABLE.                    */
  /* THE RECORDS ARE MANAGED BY A INDEX       */
  /* POOL WHERE AN INDEX RECORD IS SEIZED     */
  /* WHEN AN INDEX IS CREATED AND RELEASED    */
  /* WHEN THE INDEX IS DROPPED.               */
  /* **************************************** */
  struct TcIndexData {
    TcIndexData() :
      indexState(IS_OFFLINE)
    {}

    /**
     *  IndexState
     */
    IndexState indexState;

    /**
     * Index id, same as index table id in DICT
     */
    Uint32 indexId;
    
    /**
     * Index attribute list.  Only the length is used in v21x.
     */
    IndexAttributeList attributeList;

    /**
     * Primary table id, the primary table to be indexed
     */
    Uint32 primaryTableId;

    /**
     *  Primary key position in secondary table
     */
    Uint32 primaryKeyPos;

    /**
     * Next ptr (used in pool/list)
     */
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    /**
     * Prev pointer (used in list)
     */
    Uint32 prevList;
  };
  
  typedef Ptr<TcIndexData> TcIndexDataPtr;
  typedef ArrayPool<TcIndexData> TcIndexData_pool;
  typedef DLList<TcIndexData_pool> TcIndexData_list;

  /**
   * Pool of index data record
   */
  TcIndexData_pool c_theIndexPool;
  RSS_AP_SNAPSHOT(c_theIndexPool);
  
  /**
   * The list of defined indexes
   */  
  TcIndexData_list c_theIndexes;
  UintR c_maxNumberOfIndexes;

  struct TcIndexOperation {
    STATIC_CONST( TYPE_ID = RT_DBTC_INDEX_OPERATION );

    TcIndexOperation() :
      m_magic(Magic::make(TYPE_ID)),
      indexOpState(IOS_NOOP),
      pendingKeyInfo(0),
      keyInfoSectionIVal(RNIL),
      pendingAttrInfo(0),
      attrInfoSectionIVal(RNIL),
      transIdAIState(ITAS_WAIT_HEADER),
      pendingTransIdAI(0),
      transIdAISectionIVal(RNIL),
      indexReadTcConnect(RNIL),
      savedFlags(0)
    {}

    ~TcIndexOperation()
    {
    }
    
    Uint32 m_magic;

    // Index data
    Uint32 indexOpId;
    IndexOperationState indexOpState; // Used to mark on-going TcKeyReq
    Uint32 pendingKeyInfo;
    Uint32 keyInfoSectionIVal;
    Uint32 pendingAttrInfo;
    Uint32 attrInfoSectionIVal;
    IndexTransIdAIState transIdAIState;
    Uint32 pendingTransIdAI;
    Uint32 transIdAISectionIVal; // For accumulating TransId_AI
    Uint32 fragmentId;
    
    TcKeyReq tcIndxReq;
    UintR connectionIndex;
    UintR indexReadTcConnect; //
    
    Uint32 savedFlags; // Saved transaction flags

    /**
     * Next ptr (used in list)
     */
    Uint32 nextList;
    /**
     * Prev pointer (used in list)
     */
    Uint32 prevList;
  };
  
  typedef Ptr<TcIndexOperation> TcIndexOperationPtr;
  typedef TransientPool<TcIndexOperation> TcIndexOperation_pool;
  STATIC_CONST(DBTC_INDEX_OPERATION_TRANSIENT_POOL_INDEX = 3);
  typedef LocalDLList<TcIndexOperation_pool> LocalTcIndexOperation_dllist;

  /**
   * Pool of index data record
   */
  alignas(64) TcIndexOperation_pool c_theIndexOperationPool;
  RSS_AP_SNAPSHOT(c_theIndexOperationPool);

  struct TcFKData {
    TcFKData() {}

    Uint32 m_magic;

    union {
      Uint32 key;
      Uint32 fkId;
    };

    /**
     * Columns used in parent table
     */
    IndexAttributeList parentTableColumns;

    /**
     * Columns used in child table
     */
    IndexAttributeList childTableColumns;

    Uint32 parentTableId; // could be unique index table...
    Uint32 childTableId;  //
    Uint32 childIndexId;  // (could be tableId too)
    Uint32 bits;          // CreateFKImplReq::Bits

    Uint32 nextPool;
    Uint32 nextHash;
    Uint32 prevHash;

    Uint32 hashValue() const {
      return key;
    }

    bool equal(const TcFKData& obj) const {
      return key == obj.key;
    }
  };

  typedef RecordPool<RWPool<TcFKData> > FK_pool;
  typedef KeyTable<FK_pool> FK_hash;

  FK_pool c_fk_pool;
  FK_hash c_fk_hash;

  /************************** TC CONNECT RECORD ************************/
  /* *******************************************************************/
  /* TC CONNECT RECORD KEEPS ALL INFORMATION TO CARRY OUT A TRANSACTION*/
  /* THE TRANSACTION CONTROLLER ESTABLISHES CONNECTIONS TO DIFFERENT   */
  /* BLOCKS TO CARRY OUT THE TRANSACTION. THERE CAN BE SEVERAL RECORDS */
  /* PER ACTIVE TRANSACTION. THE TC CONNECT RECORD COOPERATES WITH THE */
  /* API CONNECT RECORD FOR COMMUNICATION WITH THE API AND WITH THE    */
  /* LQH CONNECT RECORD FOR COMMUNICATION WITH THE LQH'S INVOLVED IN   */
  /* THE TRANSACTION. TC CONNECT RECORD IS PERMANENTLY CONNECTED TO A  */
  /* RECORD IN DICT AND ONE IN DIH. IT CONTAINS A LIST OF ACTIVE LQH   */
  /* CONNECT RECORDS AND A LIST OF STARTED BUT NOT ACTIVE LQH CONNECT  */
  /* RECORDS. IT DOES ALSO CONTAIN A LIST OF ALL OPERATIONS THAT ARE   */
  /* EXECUTED WITH THE TC CONNECT RECORD.                              */
  /*******************************************************************>*/
  /*       TC_CONNECT RECORD ALIGNED TO BE 128 BYTES                   */
  /*******************************************************************>*/
  struct TcConnectRecord
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_CONNECT_RECORD );

    TcConnectRecord()
    : m_magic(Magic::make(TYPE_ID)),
      commitAckMarker(RNIL),
      tcConnectstate(OS_CONNECTED),
      apiConnect(RNIL),
      nextList(RNIL),
      noOfNodes(0),
      m_special_op_flags(0),
      prevList(RNIL),
      triggeringOperation(RNIL),
      indexOp(RNIL),
      currentTriggerId(RNIL)
    {
      NdbTick_Invalidate(&m_start_ticks);
      thePendingTriggers.init();
    }

    Uint32 m_magic;
    //---------------------------------------------------
    // First 16 byte cache line. Those variables are only
    // used in error cases.
    //---------------------------------------------------
    UintR  tcOprec;      /* TC OPREC of operation being taken over       */
    Uint16 failData[4];  /* Failed nodes when taking over an operation   */
    UintR  nextTcFailHash;

    //---------------------------------------------------
    // Second 16 byte cache line. Those variables are used
    // from LQHKEYCONF to sending COMMIT and COMPLETED.
    //---------------------------------------------------
    UintR lastLqhCon;        /* Connect record in last replicas Lqh record   */
    Uint16 lastLqhNodeId;    /* Node id of last replicas Lqh                 */
    Uint16 m_execAbortOption;/* TcKeyReq::ExecuteAbortOption */
    UintR  commitAckMarker;  /* CommitMarker I value */

    //---------------------------------------------------
    // Third 16 byte cache line. The hottest variables.
    //---------------------------------------------------
    OperationState tcConnectstate;         /* THE STATE OF THE CONNECT*/
    UintR apiConnect;                      /* POINTER TO API CONNECT RECORD */
    UintR nextList;                   /* NEXT TC RECORD*/
    Uint8 dirtyOp;
    Uint8 opSimple;
    Uint8 lastReplicaNo;     /* NUMBER OF THE LAST REPLICA IN THE OPERATION */
    Uint8 noOfNodes;         /* TOTAL NUMBER OF NODES IN OPERATION          */
    Uint8 operation;         /* OPERATION TYPE                              */
                             /* 0 = READ REQUEST                            */
                             /* 1 = UPDATE REQUEST                          */
                             /* 2 = INSERT REQUEST                          */
                             /* 3 = DELETE REQUEST                          */
    Uint16 m_special_op_flags; // See ApiConnectRecord::SpecialOpFlags
    enum SpecialOpFlags {
      SOF_NORMAL = 0,
      SOF_INDEX_TABLE_READ = 1,       // Read index table
      SOF_REORG_TRIGGER = 4,          // A reorg trigger
      SOF_REORG_MOVING = 8,           // A record that should be moved
      SOF_TRIGGER = 16,               // A trigger
      SOF_REORG_COPY = 32,
      SOF_REORG_DELETE = 64,
      SOF_DEFERRED_UK_TRIGGER = 128,  // Op has deferred trigger
      SOF_DEFERRED_FK_TRIGGER = 256,
      SOF_FK_READ_COMMITTED = 512,    // reply to TC even for dirty read
      SOF_FULLY_REPLICATED_TRIGGER = 1024,
      SOF_UTIL_FLAG = 2048            // Sender to TC is DBUTIL (higher prio)
    };

    static inline bool isIndexOp(Uint16 flags) {
      return (flags & SOF_INDEX_TABLE_READ) != 0;
    }

    //---------------------------------------------------
    // Fourth 16 byte cache line. The mildly hot variables.
    // tcNodedata expands 4 Bytes into the next cache line
    // with indexes almost never used.
    //---------------------------------------------------
    UintR clientData;           /* SENDERS OPERATION POINTER              */
    UintR prevList;        /* DOUBLY LINKED LIST OF TC CONNECT RECORDS*/
    UintR savePointId;

    Uint16 tcNodedata[4];
    /* Instance key to send to LQH.  Receiver maps it to actual instance. */
    Uint16 lqhInstanceKey;

    // Trigger data
    UintR numFiredTriggers;      // As reported by lqhKeyConf
    UintR numReceivedTriggers;   // FIRE_TRIG_ORD
    UintR triggerExecutionCount;// No of outstanding op due to triggers
    UintR savedState[LqhKeyConf::SignalLength];
    /**
     * The list of pending fired triggers
     */
    Local_TcFiredTriggerData_fifo::Head thePendingTriggers;

    UintR triggeringOperation;  // Which operation was "cause" of this op

    // Index data
    UintR indexOp;
    UintR currentTriggerId;
    union {
      Uint32 attrInfoLen;
      Uint32 triggerErrorCode;
    };
    NDB_TICKS m_start_ticks;
  };

  typedef Ptr<TcConnectRecord> TcConnectRecordPtr;
  typedef TransientPool<TcConnectRecord> TcConnectRecord_pool;
  STATIC_CONST(DBTC_CONNECT_RECORD_TRANSIENT_POOL_INDEX = 4);
  typedef LocalDLFifoList<TcConnectRecord_pool> LocalTcConnectRecord_fifo;

  /************************** API CONNECT RECORD ***********************
   * The API connect record contains the connection record to which the
   * application connects.  
   *
   * The application can send one operation at a time.  It can send a 
   * new operation immediately after sending the previous operation. 
   * Thereby several operations can be active in one transaction within TC. 
   * This is achieved by using the API connect record. 
   * Each active operation is handled by the TC connect record. 
   * As soon as the TC connect record has sent the     
   * request to the LQH it is ready to receive new operations. 
   * The LQH connect record takes care of waiting for an operation to      
   * complete.  
   * When an operation has completed on the LQH connect record, 
   * a new operation can be started on this LQH connect record. 
   *******************************************************************
   *                                                                   
   *       API CONNECT RECORD ALIGNED TO BE 256 BYTES                  
   ********************************************************************/
  
  /*******************************************************************>*/
  // We break out the API Timer for optimisation on scanning rather than
  // on fast access.
  /*******************************************************************>*/
  struct ApiConTimers
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_API_CONNECT_TIMERS );
    STATIC_CONST( INDEX_BITS = 3 );
    STATIC_CONST( INDEX_MASK = (1 << INDEX_BITS) - 1 );
    STATIC_CONST( INDEX_MAX_COUNT = (1 << INDEX_BITS) - 2 );

    struct TimerEntry
    {
      Uint32 m_timer;
      Uint32 m_apiConnectRecord;
    };

    ApiConTimers():
      m_magic(Magic::make(TYPE_ID)),
      m_count(0),
      m_top(0),
      nextList(RNIL),
      prevList(RNIL)
    {}

    static void static_asserts()
    {
      STATIC_ASSERT(sizeof(ApiConTimers) ==
                      sizeof(TimerEntry) << INDEX_BITS);
    }

    Uint32 m_magic;
    Uint16 m_count;
    Uint16 m_top;
    Uint32 nextList;
    Uint32 prevList;
    TimerEntry m_entries[INDEX_MAX_COUNT];
  };

  typedef Ptr<ApiConTimers> ApiConTimersPtr;
  typedef TransientPool<ApiConTimers> ApiConTimers_pool;
  STATIC_CONST(DBTC_API_CONNECT_TIMERS_TRANSIENT_POOL_INDEX = 5);
  typedef LocalDLFifoList<ApiConTimers_pool> LocalApiConTimers_list;

  alignas(64) ApiConTimers_pool c_apiConTimersPool;
  RSS_AP_SNAPSHOT(c_apiConTimersPool);
  LocalApiConTimers_list::Head c_apiConTimersList;
  ApiConTimers* c_currentApiConTimers;

  /**
   * Limit the resource (signal/job buffer) usage of a transaction
   * by limiting :
   * - max cascading scans (FK child scans) and
   * - trigger operations.
   * An FK child scan is executed alone exclusively.
   */
  static const Uint8 MaxCascadingScansPerTransaction = 1;
  static const Uint32 MaxExecutingTriggerOpsPerTrans = 32;

  struct ApiConnectRecord
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_API_CONNECT_RECORD );

    enum ConnectionKind
    {
      CK_NONE = 0,
      CK_FREE,
      CK_USER,
      CK_COPY,
      CK_FAIL
    };

    ApiConnectRecord();
    
    Uint32 m_magic;
    //---------------------------------------------------
    // First 16 byte cache line. Hot variables.
    //---------------------------------------------------
    Uint32 m_apiConTimer;
    Uint32 m_apiConTimer_line; // Last line updating timer
    ConnectionState apiConnectstate;
    ConnectionKind apiConnectkind;
    UintR transid[2];
    LocalTcConnectRecord_fifo::Head tcConnect;
    
    //---------------------------------------------------
    // Second 16 byte cache line. Hot variables.
    //---------------------------------------------------
    UintR lqhkeyconfrec;
    UintR cachePtr;
    UintR currSavePointId;
    UintR counter;
    
    //---------------------------------------------------
    // Third 16 byte cache line. First and second cache
    // line plus this will be enough for copy API records.
    // Variables used in late phases.
    //---------------------------------------------------
    UintR nextGcpConnect;
    UintR prevGcpConnect;
    UintR gcpPointer;
    UintR ndbapiConnect;
    
    //---------------------------------------------------
    // Fourth 16 byte cache line. Only used in late phases.
    // Plus 4 bytes of error handling.
    //---------------------------------------------------
    UintR nextApiConnect; // free, freeFail, ctransidFailHash, PREPARE_COMMIT
    BlockReference ndbapiBlockref;
    UintR apiCopyRecord;
    Uint64 globalcheckpointid;
    
    //---------------------------------------------------
    // Second 64 byte cache line starts. First 16 byte
    // cache line in this one. Variables primarily used
    // in early phase.
    //---------------------------------------------------
    UintR lqhkeyreqrec;
    union {
      Uint32 buddyPtr;
      Int32 pendingTriggers; // For deferred triggers
    };
    union {
      UintR apiScanRec;
      UintR commitAckMarker;
    };

    /** 
     * num_commit_ack_markers
     *
     * Number of operations sent by this transaction 
     * to LQH with their CommitAckMarker flag set.
     *
     * Includes marked operations currently in-progress and 
     * those which prepared successfully, 
     * Excludes failed operations (LQHKEYREF)
     */
    Uint32 num_commit_ack_markers;
    Uint32 m_write_count;
    ReturnSignal returnsignal;
    AbortState abortState;

    enum TransactionFlags
    {
      TF_INDEX_OP_RETURN = 1,
      TF_TRIGGER_PENDING = 2, // Used to mark waiting for a CONTINUEB
      TF_EXEC_FLAG       = 4,
      TF_COMMIT_ACK_MARKER_RECEIVED = 8,
      TF_DEFERRED_CONSTRAINTS = 16, // check constraints in deferred fashion
      TF_DEFERRED_UK_TRIGGERS = 32, // trans has deferred UK triggers
      TF_DEFERRED_FK_TRIGGERS = 64, // trans has deferred FK triggers
      TF_DISABLE_FK_CONSTRAINTS = 128,
      TF_LATE_COMMIT = 256, // Wait sending apiCommit until complete phase done

      TF_END = 0
    };
    Uint32 m_flags;

    Uint16 m_special_op_flags; // Used to mark on-going TcKeyReq as indx table

    Uint8 takeOverRec;
    Uint8 currentReplicaNo;

    Uint8 tckeyrec; // Changed from R

    Uint8 tcindxrec;

    enum ApiFailStates
    {
      AFS_API_OK = 0,
      AFS_API_FAILED = 1,
      AFS_API_DISCONNECTED = 2
    };
    Uint8 apiFailState;

    Uint8 timeOutCounter;
    Uint8 singleUserMode;
    
    Uint16 returncode;
    Uint16 takeOverInd;
    //---------------------------------------------------
    // Error Handling variables. If cache line 32 bytes
    // ensures that cache line is still only read in
    // early phases.
    //---------------------------------------------------
    UintR currentTcConnect;
    BlockReference tcBlockref;
    UintR failureNr;
    
    //---------------------------------------------------
    // Second 64 byte cache line. Third 16 byte cache line
    // in this one. Variables primarily used in early phase
    // and checked in late phase.
    // Fourth cache line is the tcSendArray that is used
    // when two and three operations are responded to in
    // parallel. The first two entries in tcSendArray is
    // part of the third cache line.
    //---------------------------------------------------
    //---------------------------------------------------
    // timeOutCounter is used waiting for ABORTCONF, COMMITCONF
    // and COMPLETECONF
    //---------------------------------------------------
    UintR tcSendArray[6];
    NdbNodeBitmask m_transaction_nodes; 
    
    // Trigger data
    
    /**
     * The list of fired triggers
     */  
    Local_TcFiredTriggerData_fifo::Head theFiredTriggers;

    // Count the outstanding FIRE_TRIG_REQs of a transaction.
    // Limit it in order to avoid job buffer overload
    Uint32 m_outstanding_fire_trig_req;

    // First and last indices of the local tc connect pointers that will
    // be used to send fire trigger reqs when resumed in execFireTrigConf
    // or in execCONTINUEB
    UintR m_firstTcConnectPtrI_FT;
    UintR m_lastTcConnectPtrI_FT;


    // Index data
    
    UintR noIndexOp;     // No outstanding index ops

    // Index op return context
    UintR indexOp;
    UintR clientData;
    Uint32 errorData;
    UintR attrInfoLen;
    Uint32 immediateTriggerId;  // Id of trigger op being fired NOW
    Uint32 firedFragId;
    
    UintR accumulatingIndexOp;
    UintR executingIndexOp;
    UintR tcIndxSendArray[6];
    NDB_TICKS m_start_ticks;
    LocalTcIndexOperation_dllist::Head theSeizedIndexOperations;

#ifdef ERROR_INSERT
    Uint32 continueBCount;  // ERROR_INSERT 8082
#endif
    Uint8 m_pre_commit_pass;

    bool isExecutingDeferredTriggers() const {
      return apiConnectstate == CS_SEND_FIRE_TRIG_REQ ||
        apiConnectstate == CS_WAIT_FIRE_TRIG_REQ ;
    }

    // number of on-going cascading scans (FK child scans) at a transaction.
    Uint8 cascading_scans_count;

    // Number of on-going trigger operations at a transaction
    // Limit them in order to avoid the transaction
    // overloading node resources (signal/job buffers).
    Uint32 m_executing_trigger_ops;

    // Trigger execution loop active
    bool m_inExecuteTriggers;
    /**
     * ExecTriggersGuard
     *
     * Used to avoid recursive calls of executeTriggers
     */
    class ExecTriggersGuard
    {
      ApiConnectRecord* m_recPtr;
    public:
      ExecTriggersGuard(ApiConnectRecord* recPtr)
      {
        if (recPtr->m_inExecuteTriggers)
        {
          m_recPtr = NULL;
        }
        else
        {
          m_recPtr = recPtr;
          m_recPtr->m_inExecuteTriggers = true;
        }
      }

      ~ExecTriggersGuard()
      {
        if (m_recPtr)
        {
          assert(m_recPtr->m_inExecuteTriggers == true);
          m_recPtr->m_inExecuteTriggers = false;
        }
      }

      bool canExecNow() const
      {
        assert(m_recPtr == NULL ||
               m_recPtr->m_inExecuteTriggers);
        return (m_recPtr != NULL);
      }
    };
  };
  
  typedef TransientPool<ApiConnectRecord> ApiConnectRecord_pool;
  STATIC_CONST(DBTC_API_CONNECT_RECORD_TRANSIENT_POOL_INDEX = 6);
  typedef LocalDLFifoList<ApiConnectRecord_pool, IA_GcpConnect>
      LocalApiConnectRecord_gcp_list;
  typedef LocalSLFifoList<ApiConnectRecord_pool, IA_ApiConnect>
      LocalApiConnectRecord_api_fifo;
  typedef LocalSLList<ApiConnectRecord_pool, IA_ApiConnect>
      LocalApiConnectRecord_api_list;
  typedef Ptr<ApiConnectRecord> ApiConnectRecordPtr;

  class PrefetchApiConTimer
  {
  public:
    PrefetchApiConTimer(const ApiConTimers_pool& pool, ApiConnectRecordPtr apiConPtr, bool for_write)
    : m_apiConTimers(NULL), m_timer_index(0), checked(false)
    {
      m_apiConPtr = apiConPtr;
      const Uint32 apiConTimer = m_apiConPtr.p->m_apiConTimer;
      ApiConTimersPtr apiConTimers;
      require(apiConTimer != RNIL);
      apiConTimers.i = apiConTimer >> ApiConTimers::INDEX_BITS;
      if (for_write)
      {
        require(pool.getUncheckedPtrRW(apiConTimers));
      }
      else
      {
        require(pool.getUncheckedPtrRO(apiConTimers));
      }
      m_timer_index = apiConTimer & ApiConTimers::INDEX_MASK;
      m_apiConTimers = apiConTimers.p;
    }

    bool check_ptr()
    {
      if (unlikely(!Magic::check_ptr(m_apiConTimers)))
      {
        return false;
      }
      assert(m_timer_index < m_apiConTimers->m_top);
      assert(m_apiConTimers->m_count > 0);
      assert(m_apiConTimers->m_entries[m_timer_index].m_apiConnectRecord ==
             m_apiConPtr.i);
      checked = true;
      return true;
    }

    void set_timer(Uint32 value, Uint32 line)
    {
      if (!checked)
      {
        require(check_ptr());
      }
      m_apiConTimers->m_entries[m_timer_index].m_timer = value;
      m_apiConPtr.p->m_apiConTimer_line = line;
    }

    Uint32 get_timer()
    {
      if (!checked)
      {
        require(check_ptr());
      }
      return m_apiConTimers->m_entries[m_timer_index].m_timer;
    }
  private:
    ApiConnectRecordPtr m_apiConPtr;
    ApiConTimers* m_apiConTimers;
    Uint32 m_timer_index;
    bool checked;
  };

  void setApiConTimer(ApiConnectRecordPtr apiConPtr, Uint32 value, Uint32 line)
#ifdef DBTC_MAIN
;
#else
  {
    const Uint32 apiConTimer = apiConPtr.p->m_apiConTimer;
    ApiConTimersPtr apiConTimers;
    ndbrequire(apiConTimer != RNIL);
    apiConTimers.i = apiConTimer >> ApiConTimers::INDEX_BITS;
    c_apiConTimersPool.getPtr(apiConTimers);
    const Uint32 timer_index = apiConTimer & ApiConTimers::INDEX_MASK;
    ndbassert(timer_index < apiConTimers.p->m_top);
    ndbassert(apiConTimers.p->m_count > 0);
    ndbassert(apiConTimers.p->m_entries[timer_index].m_apiConnectRecord ==
                apiConPtr.i);

    apiConTimers.p->m_entries[timer_index].m_timer = value;
    apiConPtr.p->m_apiConTimer_line = line;
  }
#endif
  Uint32 getApiConTimer(const ApiConnectRecordPtr apiConPtr) const
  {
    const Uint32 apiConTimer = apiConPtr.p->m_apiConTimer;
    ApiConTimersPtr apiConTimers;
    ndbrequire(apiConTimer != RNIL);
    apiConTimers.i = apiConTimer >> ApiConTimers::INDEX_BITS;
    c_apiConTimersPool.getPtr(apiConTimers);
    const Uint32 timer_index = apiConTimer & ApiConTimers::INDEX_MASK;
    ndbassert(timer_index < apiConTimers.p->m_top);
    ndbassert(apiConTimers.p->m_count > 0);
    ndbassert(apiConTimers.p->m_entries[timer_index].m_apiConnectRecord ==
                apiConPtr.i);

    return apiConTimers.p->m_entries[timer_index].m_timer;
  }

  bool seizeApiConTimer(ApiConnectRecordPtr apiConPtr)
  {
    ndbrequire(apiConPtr.p->m_apiConTimer == RNIL);
    if (unlikely(c_currentApiConTimers == NULL) ||
        (c_currentApiConTimers->m_top == ApiConTimers::INDEX_MAX_COUNT))
    {
      jam();
      ApiConTimersPtr apiConTimersptr;
      if (unlikely(!c_apiConTimersPool.seize(apiConTimersptr)))
      {
        jam();
        return false;
      }
      LocalApiConTimers_list timers_list(c_apiConTimersPool,
                                         c_apiConTimersList);
      timers_list.addLast(apiConTimersptr);
      c_currentApiConTimers = apiConTimersptr.p;
    }
    ApiConTimers* apiConTimers = c_currentApiConTimers;
    const Uint32 timer_index = apiConTimers->m_top;
    const Uint32 apiConTimer =
      (c_apiConTimersList.getLast() << ApiConTimers::INDEX_BITS) | timer_index;
    apiConTimers->m_entries[timer_index].m_timer = 0;
    apiConTimers->m_entries[timer_index].m_apiConnectRecord = apiConPtr.i;
    apiConTimers->m_top++;
    apiConTimers->m_count++;
    apiConPtr.p->m_apiConTimer = apiConTimer;
    apiConPtr.p->m_apiConTimer_line = 0;
    return true;
  }

  void releaseApiConTimer(ApiConnectRecordPtr apiConPtr)
  {
    const Uint32 apiConTimer = apiConPtr.p->m_apiConTimer;
    ndbrequire(apiConTimer != RNIL);

    ApiConTimersPtr apiConTimers;
    apiConTimers.i = apiConTimer >> ApiConTimers::INDEX_BITS;
    c_apiConTimersPool.getPtr(apiConTimers);
    const Uint32 timer_index = apiConTimer & ApiConTimers::INDEX_MASK;
    apiConTimers.p->m_entries[timer_index].m_timer = 0;
    apiConTimers.p->m_entries[timer_index].m_apiConnectRecord = RNIL;
    ndbassert(apiConTimers.p->m_count > 0);
    apiConTimers.p->m_count--;
    if (apiConTimers.p->m_count == 0)
    {
      jam();
      LocalApiConTimers_list timers_list(c_apiConTimersPool,
                                         c_apiConTimersList);
      timers_list.remove(apiConTimers);
      c_apiConTimersPool.release(apiConTimers);
      checkPoolShrinkNeed(DBTC_API_CONNECT_TIMERS_TRANSIENT_POOL_INDEX,
                          c_apiConTimersPool);
      if (apiConTimers.p == c_currentApiConTimers)
      {
        jam();
        if (timers_list.last(apiConTimers))
        {
          c_currentApiConTimers = apiConTimers.p;
        }
        else
        {
          c_currentApiConTimers = NULL;
        }
      }
    }
    apiConPtr.p->m_apiConTimer = RNIL;
  }

  // ********************** CACHE RECORD **************************************
  //---------------------------------------------------------------------------
  // This record is used between reception of TCKEYREQ and sending of LQHKEYREQ
  // It is separated so as to improve the cache hit rate and also to minimise 
  // the necessary memory storage in NDB Cluster.
  //---------------------------------------------------------------------------

  struct CacheRecord
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_CACHE_RECORD );

    CacheRecord()
      : m_magic(Magic::make(TYPE_ID))
    {}

    Uint32 m_magic;

    /* Fields used by TCKEYREQ/TCINDXREQ/SCANTABREQ */
      Uint32 keyInfoSectionI;   /* KeyInfo section I-val */
      Uint32 attrInfoSectionI;  /* AttrInfo section I-val */

      // TODO : Consider using section length + other vars for this 
      UintR  currReclenAi;      /* AttrInfo words received so far */
      Uint16 attrlength;        /* Total AttrInfo length */
      Uint16 save1;             /* KeyInfo words received so far */
      Uint16 keylen;            /* KEY LENGTH SENT BY REQUEST SIGNAL */
    
      /* Distribution information */
      // TODO : Consider placing this info into other records
      Uint8  distributionKeyIndicator;
      Uint8  viaSPJFlag;        /* Send request via the SPJ block.*/
      UintR  distributionKey;
    /* End of fields used by TCKEYREQ/TCINDXREQ/SCANTABREQ */
    

    /* TCKEYREQ/TCINDXREQ only fields */
      UintR  schemaVersion;/* SCHEMA VERSION USED IN TRANSACTION         */
      UintR  tableref;     /* POINTER TO THE TABLE IN WHICH THE FRAGMENT EXISTS*/
    
      UintR  fragmentid;   /* THE COMPUTED FRAGMENT ID                     */
      UintR  hashValue;    /* THE HASH VALUE USED TO LOCATE FRAGMENT       */
    
      Uint8  m_special_hash; // collation or distribution key
      Uint8  m_no_hash;      // Hash not required for LQH (special variant)
      Uint8  m_no_disk_flag; 
      Uint8  m_op_queue;
      Uint8  lenAiInTckeyreq;  /* LENGTH OF ATTRIBUTE INFORMATION IN TCKEYREQ */
    
      Uint8  fragmentDistributionKey;  /* DIH generation no */
    
      /**
       * EXECUTION MODE OF OPERATION                    
       * 0 = NORMAL EXECUTION, 1 = INTERPRETED EXECUTION
       */
      Uint8  opExec;
      Uint8  m_read_committed_base;
    
      /* Use of Long signals */
      Uint8  isLongTcKeyReq;   /* Incoming TcKeyReq used long signal */
      Uint8  useLongLqhKeyReq; /* Outgoing LqhKeyReq should be long */
    
      Uint32 scanInfo;
    
      Uint32 scanTakeOverInd;
      Uint32 unlockNodeId;     /* NodeId for unlock operation */
    /* End of TCKEYREQ/TCINDXREQ only fields */

  };
  
  typedef Ptr<CacheRecord> CacheRecordPtr;
  typedef TransientPool<CacheRecord> CacheRecord_pool;
  STATIC_CONST(DBTC_CACHE_RECORD_TRANSIENT_POOL_INDEX = 7);
  CacheRecord m_local_cache_record;
  
  /* ************************ HOST RECORD ********************************** */
  /********************************************************/
  /* THIS RECORD CONTAINS ALIVE-STATUS ON ALL NODES IN THE*/
  /* SYSTEM                                               */
  /********************************************************/
  struct HostRecord {
    struct PackedWordsContainer lqh_pack[MAX_NDBMT_LQH_THREADS+1];
    struct PackedWordsContainer packTCKEYCONF;
    HostState hostStatus;
    LqhTransState lqhTransStatus;
    bool  inPackedList;

    Uint32 m_location_domain_id;

    enum NodeFailBits
    {
      NF_TAKEOVER          = 0x1,
      NF_CHECK_SCAN        = 0x2,
      NF_CHECK_TRANSACTION = 0x4,
      NF_BLOCK_HANDLE      = 0x8,
      NF_NODE_FAIL_BITS    = 0xF // All bits...
    };
    Uint32 m_nf_bits;
    NdbNodeBitmask m_lqh_trans_conf;
    /**
     * Indicator if any history to track yet
     *
     * Tracking scan and scan errors (API node)
     * Tracking read key, write key and index key operations
     * (API node and primary DB node)
     * Tracking scan frag and scan frag errors (API node)
     * Tracking transactions (API node)
     */
    Uint32 time_tracked;
    Uint64 time_track_scan_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_scan_error_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_read_key_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_write_key_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_index_key_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_key_error_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_scan_frag_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_scan_frag_error_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_transaction_histogram[TIME_TRACK_HISTOGRAM_RANGES];
    Uint64 time_track_transaction_error_histogram[TIME_TRACK_HISTOGRAM_RANGES];
  };
  Uint32 m_my_location_domain_id;
  
  typedef Ptr<HostRecord> HostRecordPtr;
  
  /* *********** TABLE RECORD ********************************************* */

  /********************************************************/
  /* THIS RECORD CONTAINS THE CURRENT SCHEMA VERSION OF   */
  /* ALL TABLES IN THE SYSTEM.                            */
  /********************************************************/
  struct TableRecord {
    TableRecord() {}
    Uint32 currentSchemaVersion;
    Uint16 m_flags;
    Uint8 tableType;
    Uint8 singleUserMode;

    enum {
      TR_ENABLED      = 1 << 0,
      TR_DROPPING     = 1 << 1,
      TR_STORED_TABLE = 1 << 2,
      TR_PREPARED     = 1 << 3
      ,TR_USER_DEFINED_PARTITIONING = 1 << 4
      ,TR_READ_BACKUP = (1 << 5)
      ,TR_FULLY_REPLICATED = (1<<6)
      ,TR_DELAY_COMMIT = (1 << 7)
    };
    Uint8 get_enabled()     const { return (m_flags & TR_ENABLED)      != 0; }
    Uint8 get_dropping()    const { return (m_flags & TR_DROPPING)     != 0; }
    Uint8 get_storedTable() const { return (m_flags & TR_STORED_TABLE) != 0; }
    Uint8 get_prepared()    const { return (m_flags & TR_PREPARED)     != 0; }
    void set_enabled(Uint8 f)     { f ? m_flags |= (Uint16)TR_ENABLED      : m_flags &= ~(Uint16)TR_ENABLED; }
    void set_dropping(Uint8 f)    { f ? m_flags |= (Uint16)TR_DROPPING     : m_flags &= ~(Uint16)TR_DROPPING; }
    void set_storedTable(Uint8 f) { f ? m_flags |= (Uint16)TR_STORED_TABLE : m_flags &= ~(Uint16)TR_STORED_TABLE; }
    void set_prepared(Uint8 f)    { f ? m_flags |= (Uint16)TR_PREPARED : m_flags &= ~(Uint16)TR_PREPARED; }

    Uint8 get_user_defined_partitioning() const {
      return (m_flags & TR_USER_DEFINED_PARTITIONING) != 0;
    }

    void set_user_defined_partitioning(Uint8 f) {
      f ?
        m_flags |= (Uint16)TR_USER_DEFINED_PARTITIONING :
        m_flags &= ~(Uint16)TR_USER_DEFINED_PARTITIONING;
    }

    Uint8 noOfKeyAttr;
    Uint8 hasCharAttr;
    Uint8 noOfDistrKeys;
    Uint8 hasVarKeys;

    bool checkTable(Uint32 schemaVersion) const {
      return !get_dropping() &&
	((/** normal transaction path */
          get_enabled() &&
          table_version_major(schemaVersion) ==
          table_version_major(currentSchemaVersion)) 
         ||
         (/** 
           * unique index is relaxed for DbUtil and transactions ongoing
           * while index is created
           */
          get_prepared() && schemaVersion == currentSchemaVersion &&
          DictTabInfo::isUniqueIndex(tableType)));
    }
    
    Uint32 getErrorCode(Uint32 schemaVersion) const;
  };
  typedef Ptr<TableRecord> TableRecordPtr;

  /**
   * Specify the location of a fragment. The 'blockRef' is either
   * the specific LQH where the fragId resides, or the SPJ block
   * responsible for scaning this fragment, if 'viaSPJ'.
   */
  struct ScanFragLocation
  {
    Uint32 blockRef;
    Uint32 fragId;
  };

#define NUM_FRAG_LOCATIONS_IN_ARRAY 32

  struct ScanFragLocationRec
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_FRAG_LOCATION );

    ScanFragLocationRec()
    : m_magic(Magic::make(TYPE_ID)),
      nextList(RNIL)
    {}

    Uint32 m_magic;

    /**
     * Next ptr (used in pool/list)
     */
    Uint32 nextList;

    Uint32 m_first_index;
    Uint32 m_next_index;
    struct ScanFragLocation m_frag_location_array[NUM_FRAG_LOCATIONS_IN_ARRAY];
  };

  typedef Ptr<ScanFragLocationRec> ScanFragLocationPtr;
  typedef TransientPool<ScanFragLocationRec> ScanFragLocation_pool;
  STATIC_CONST(DBTC_FRAG_LOCATION_TRANSIENT_POOL_INDEX = 8);
  typedef SLFifoList<ScanFragLocation_pool> ScanFragLocation_list;
  typedef LocalSLFifoList<ScanFragLocation_pool> Local_ScanFragLocation_list;

  alignas(64) ScanFragLocation_pool m_fragLocationPool;
  RSS_AP_SNAPSHOT(m_fragLocationPool);

  /**
   * There is max 16 ScanFragRec's for 
   * each scan started in TC. Each ScanFragRec is used by
   * a scan fragment "process" that scans one fragment at a time. 
   * It will receive max 16 tuples in each request
   */
  struct ScanFragRec {
    STATIC_CONST( TYPE_ID = RT_DBTC_SCAN_FRAGMENT );
    ScanFragRec();
    /**
     * ScanFragState      
     *  WAIT_GET_PRIMCONF : Waiting for DIGETPRIMCONF when starting a new 
     *   fragment scan (Obsolete; Checked for, but never set)
     *  LQH_ACTIVE : The scan process has sent a command to LQH and is
     *   waiting for the response
     *  LQH_ACTIVE_CLOSE : The scan process has sent close to LQH and is
     *   waiting for the response (Unused)
     *  DELIVERED : The result have been delivered, this scan frag process 
     *   are waiting for a SCAN_NEXTREQ to tell us to continue scanning
     *  RETURNING_FROM_DELIVERY : SCAN_NEXTREQ received and continuing scan
     *   soon (Unused)
     *  QUEUED_FOR_DELIVERY : Result queued in TC and waiting for delivery
     *   to API
     *  COMPLETED : The fragment scan processes has completed and finally
     *    sent a SCAN_PROCCONF
     */
    enum ScanFragState {
      IDLE = 0,
      WAIT_GET_PRIMCONF = 1,
      LQH_ACTIVE = 2,
      DELIVERED = 4,
      QUEUED_FOR_DELIVERY = 6,
      COMPLETED = 7
    };
    Uint32 m_magic;
    // Timer for checking timeout of this fragment scan
    Uint32  scanFragTimer;

    // Fragment id as reported back by DIGETNODESREQ
    Uint32 lqhScanFragId;

    // Blockreference of LQH 
    BlockReference lqhBlockref;

    // getNodeInfo.m_connectCount, set at seize used so that
    // I don't accidently kill a starting node
    Uint32 m_connectCount;

    // State of this fragment scan
    ScanFragState scanFragState;

    // Id of the ScanRecord this fragment scan belongs to
    Uint32 scanRec;

    // The value of fragmentCompleted in the last received SCAN_FRAGCONF
    Uint8 m_scan_frag_conf_status;

    inline void startFragTimer(Uint32 timeVal){
      scanFragTimer = timeVal;
    }
    inline void stopFragTimer(void){
      scanFragTimer = 0;
    }

    Uint32 m_ops;
    Uint32 m_apiPtr;
    Uint32 m_totalLen;
    Uint32 nextList;
    Uint32 prevList;
    NDB_TICKS m_start_ticks;
  };
  
  typedef Ptr<ScanFragRec> ScanFragRecPtr;
  typedef TransientPool<ScanFragRec> ScanFragRec_pool;
  STATIC_CONST(DBTC_SCAN_FRAGMENT_TRANSIENT_POOL_INDEX = 9);
  typedef SLList<ScanFragRec_pool> ScanFragRec_sllist;
  typedef DLList<ScanFragRec_pool> ScanFragRec_dllist;
  typedef LocalDLList<ScanFragRec_pool> Local_ScanFragRec_dllist;

  /**
   * Each scan allocates one ScanRecord to store information 
   * about the current scan
   *
   */
  struct ScanRecord {
    STATIC_CONST( TYPE_ID = RT_DBTC_SCAN_RECORD );
    ScanRecord()
    : m_magic(Magic::make(TYPE_ID)),
      scanState(IDLE),
      scanApiRec(RNIL)
    {
      NdbTick_Invalidate(&m_start_ticks);
    }
    /** NOTE! This is the old comment for ScanState. - MASV
     *       STATE TRANSITIONS OF SCAN_STATE. SCAN_STATE IS THE STATE 
     *       VARIABLE OF THE RECEIVE AND DELIVERY PROCESS.
     *       THE PROCESS HAS THREE STEPS IT GOES THROUGH.  
     *       1) THE INITIAL STATES WHEN RECEIVING DATA FOR THE SCAN.         
     *          - WAIT_SCAN_TAB_INFO                                         
     *          - WAIT_AI                                                    
     *          - WAIT_FRAGMENT_COUNT                                        
     *       2) THE EXECUTION STATES WHEN THE SCAN IS PERFORMED.             
     *          - SCAN_NEXT_ORDERED                                          
     *          - DELIVERED                                                  
     *          - QUEUED_DELIVERED                                           
     *       3) THE CLOSING STATE WHEN THE SCAN PROCESS IS CLOSING UP 
     *          EVERYTHING.        
     *          - CLOSING_SCAN                                               
     *       INITIAL START WHEN SCAN_TABREQ RECEIVED                         
     *       -> WAIT_SCAN_TAB_INFO (IF ANY SCAN_TABINFO TO BE RECEIVED)      
     *       -> WAIT_AI (IF NO SCAN_TAB_INFO BUT ATTRINFO IS RECEIVED)       
     *       -> WAIT_FRAGMENT_COUNT (IF NEITHER SCAN_TABINFO OR ATTRINFO 
     *                               RECEIVED)       
     *                                                                       
     *       WAIT_SCAN_TAB_INFO TRANSITIONS:                                 
     *       -> WAIT_SCAN_TABINFO (WHEN MORE SCAN_TABINFO RECEIVED)          
     *       -> WAIT_AI (WHEN ATTRINFO RECEIVED AFTER RECEIVING ALL 
     *                    SCAN_TABINFO)        
     *       -> WAIT_FRAGMENT_COUNT (WHEN NO ATTRINFO RECEIVED AFTER 
     *                               RECEIVING ALL SCAN_TABINFO )            
     *       WAIT_AI TRANSITIONS:                                            
     *       -> WAIT_AI (WHEN MORE ATTRINFO RECEIVED)                        
     *       -> WAIT_FRAGMENT_COUNT (WHEN ALL ATTRINFO RECEIVED)             
     *                                                                       
     *       WAIT_FRAGMENT_COUNT TRANSITIONS:                                
     *       -> SCAN_NEXT_ORDERED                                            
     *                                                                       
     *       SCAN_NEXT_ORDERED TRANSITIONS:                                  
     *       -> DELIVERED (WHEN FIRST SCAN_FRAGCONF ARRIVES WITH OPERATIONS 
     *                     TO REPORT IN IT)                                  
     *       -> CLOSING_SCAN (WHEN SCAN IS CLOSED BY SCAN_NEXTREQ OR BY SOME 
     *                        ERROR)      
     *
     *       DELIVERED TRANSITIONS:
     *       -> SCAN_NEXT_ORDERED (IF SCAN_NEXTREQ ARRIVES BEFORE ANY NEW 
     *                             OPERATIONS TO REPORT ARRIVES)
     *       -> QUEUED_DELIVERED (IF NEW OPERATION TO REPORT ARRIVES BEFORE 
     *                            SCAN_NEXTREQ)
     *       -> CLOSING_SCAN (WHEN SCAN IS CLOSED BY SCAN_NEXTREQ OR BY SOME 
     *                        ERROR)      
     *   
     *       QUEUED_DELIVERED TRANSITIONS:  
     *       -> DELIVERED (WHEN SCAN_NEXTREQ ARRIVES AND QUEUED OPERATIONS 
     *                     TO REPORT ARE SENT TO THE APPLICATION)
     *       -> CLOSING_SCAN (WHEN SCAN IS CLOSED BY SCAN_NEXTREQ OR BY 
     *                        SOME ERROR)      
     */
    enum ScanState {
      IDLE = 0,
      WAIT_SCAN_TAB_INFO = 1,
      WAIT_AI = 2,
      WAIT_FRAGMENT_COUNT = 3,
      RUNNING = 4,
      CLOSING_SCAN = 5
    };

    // State of this scan
    Uint32 m_magic;
    ScanState scanState;
    Uint32 scanKeyInfoPtr;
    Uint32 scanAttrInfoPtr;

    // List of fragment locations as reported by DIH
    ScanFragLocation_list::Head m_fragLocations;

    ScanFragRec_dllist::Head m_running_scan_frags;  // Currently in LQH
    union { Uint32 m_queued_count; Uint32 scanReceivedOperations; };
    ScanFragRec_dllist::Head m_queued_scan_frags;   // In TC !sent to API
    ScanFragRec_dllist::Head m_delivered_scan_frags;// Delivered to API
    
    // Id of the next fragment to be scanned. Used by scan fragment 
    // processes when they are ready for the next fragment
    Uint32 scanNextFragId;
    
    // Total number of fragments in the table we are scanning
    Uint32 scanNoFrag;

    // Length of expected attribute information
    Uint32 m_booked_fragments_count;

    // Reference to ApiConnectRecord
    Uint32 scanApiRec;

    // Number of scan frag processes that belong to this scan 
    Uint32 scanParallel;

    // Schema version used by this scan
    Uint32 scanSchemaVersion;

    // Index of stored procedure belonging to this scan
    Uint32 scanStoredProcId;

    // The index of table that is scanned
    Uint32 scanTableref;
    Uint32 m_scan_cookie;

    // Number of operation records per scanned fragment
    // Number of operations in first batch
    // Max number of bytes per batch
    union {
      Uint16 first_batch_size_rows;
      Uint16 batch_size_rows;
    };
    Uint32 batch_byte_size;
    Uint32 m_scan_block_no;

    Uint32 scanRequestInfo; // ScanFrag format

    // Close is ordered
    bool m_close_scan_req;
    // All SCAN_FRAGCONS should be passed on to the API as SCAN_TABCONFS.
    // This is needed to correctly propagate 'node masks' when scanning via the
    // SPJ block.
    bool m_pass_all_confs;

    /**
     * Send opcount/total len as different words
     */
    bool m_4word_conf;
    bool m_read_committed_base;

    /**
     *
     */
    bool m_scan_dist_key_flag;
    Uint32 m_scan_dist_key;
    Uint32 m_read_any_node;
    NDB_TICKS m_start_ticks;
  };
  typedef Ptr<ScanRecord> ScanRecordPtr;
  typedef TransientPool<ScanRecord> ScanRecord_pool;
  STATIC_CONST(DBTC_SCAN_RECORD_TRANSIENT_POOL_INDEX = 10);
  
  /*************************************************************************>*/
  /*                     GLOBAL CHECKPOINT INFORMATION RECORD                */
  /*                                                                         */
  /*       THIS RECORD IS USED TO STORE THE GLOBALCHECKPOINT NUMBER AND A 
   *       COUNTER DURING THE COMPLETION PHASE OF THE TRANSACTION            */
  /*************************************************************************>*/
  /*                                                                         */
  /*       GCP RECORD ALIGNED TO BE 32 BYTES                                 */
  /*************************************************************************>*/
  struct GcpRecord
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_GCP_RECORD );

    GcpRecord()
      : m_magic(Magic::make(TYPE_ID))
    {}

    Uint32 m_magic;
    Uint16 gcpNomoretransRec;
    LocalApiConnectRecord_gcp_list::Head apiConnectList;
    UintR nextList;
    Uint64 gcpId;
  }; /* p2c: size = 32 bytes */
  
  typedef Ptr<GcpRecord> GcpRecordPtr;
  typedef TransientPool<GcpRecord> GcpRecord_pool;
  STATIC_CONST(DBTC_GCP_RECORD_TRANSIENT_POOL_INDEX = 11);
  typedef LocalSLFifoList<GcpRecord_pool> LocalGcpRecord_list;

  /*************************************************************************>*/
  /*               TC_FAIL_RECORD                                            */
  /*       THIS RECORD IS USED WHEN HANDLING TAKE OVER OF ANOTHER FAILED 
   *       TC NODE.       */
  /*************************************************************************>*/
  struct TcFailRecord {
    Uint16 queueList[MAX_NDB_NODES];
    Uint8 takeOverProcState[MAX_NDB_NODES];
    UintR completedTakeOver;
    UintR currentHashIndexTakeOver;
    Uint32 maxInstanceId;
    bool   takeOverFailed;
    bool   handledOneTransaction;
    Uint32 takeOverInstanceId;
    FailState failStatus;
    Uint16 queueIndex;
    Uint16 takeOverNode;
  };
  typedef Ptr<TcFailRecord> TcFailRecordPtr;

public:
  Dbtc(Block_context&, Uint32 instanceNumber = 0);
  virtual ~Dbtc();

private:
  BLOCK_DEFINES(Dbtc);

  // Transit signals
  void execPACKED_SIGNAL(Signal* signal);
  void execABORTED(Signal* signal);
  void execATTRINFO(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execKEYINFO(Signal* signal);
  void execSCAN_NEXTREQ(Signal* signal);
  void execSCAN_PROCREQ(Signal* signal);
  void execSCAN_PROCCONF(Signal* signal);
  void execTAKE_OVERTCCONF(Signal* signal);
  void execLQHKEYREF(Signal* signal);
  void execTRANSID_AI_R(Signal* signal);
  void execKEYINFO20_R(Signal* signal);
  void execROUTE_ORD(Signal* signal);
  // Received signals
  void execDUMP_STATE_ORD(Signal* signal);
  void execDBINFO_SCANREQ(Signal* signal);
  void execSEND_PACKED(Signal* signal);
  void execCOMPLETED(Signal* signal);
  void execCOMMITTED(Signal* signal);
  void execDIGETNODESREF(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void execDIVERIFYCONF(Signal* signal);
  void execDIH_SCAN_TAB_REF(Signal* signal,
                            ScanRecordPtr scanptr,
                            ApiConnectRecordPtr apiConnectptr);
  void execDIH_SCAN_TAB_CONF(Signal* signal,
                             ScanRecordPtr scanptr,
                             TableRecordPtr tabPtr,
                             ApiConnectRecordPtr apiConnectptr);
  void execGCP_NOMORETRANS(Signal* signal);
  void execLQHKEYCONF(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execREAD_NODESREF(Signal* signal);
  void execSTTOR(Signal* signal);
  void execTC_COMMITREQ(Signal* signal);
  void execTC_CLOPSIZEREQ(Signal* signal);
  void execTCGETOPSIZEREQ(Signal* signal);
  void execTCKEYREQ(Signal* signal);
  void execTCRELEASEREQ(Signal* signal);
  void execTCSEIZEREQ(Signal* signal);
  void execTCROLLBACKREQ(Signal* signal);
  void execTC_HBREP(Signal* signal);
  void execTC_SCHVERREQ(Signal* signal);
  void execTAB_COMMITREQ(Signal* signal);
  void execSCAN_TABREQ(Signal* signal);
  void execSCAN_TABINFO(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  void execLQH_TRANSCONF(Signal* signal);
  void execCOMPLETECONF(Signal* signal);
  void execCOMMITCONF(Signal* signal);
  void execABORTCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execTIME_SIGNAL(Signal* signal);
  void execAPI_FAILREQ(Signal* signal);
  void execSCAN_HBREP(Signal* signal);

  void execABORT_ALL_REQ(Signal* signal);

  void execCREATE_TRIG_IMPL_REQ(Signal* signal);
  void execDROP_TRIG_IMPL_REQ(Signal* signal);
  void execFIRE_TRIG_ORD(Signal* signal);
  void execTRIG_ATTRINFO(Signal* signal);
  void execCREATE_INDX_IMPL_REQ(Signal* signal);
  void execDROP_INDX_IMPL_REQ(Signal* signal);
  void execTCINDXREQ(Signal* signal);
  void execINDXKEYINFO(Signal* signal);
  void execINDXATTRINFO(Signal* signal);
  void execALTER_INDX_IMPL_REQ(Signal* signal);
  void execSIGNAL_DROPPED_REP(Signal* signal);

  void execFIRE_TRIG_REF(Signal*);
  void execFIRE_TRIG_CONF(Signal*);

  void execCREATE_FK_IMPL_REQ(Signal* signal);
  void execDROP_FK_IMPL_REQ(Signal* signal);

  // Index table lookup
  void execTCKEYCONF(Signal* signal);
  void execTCKEYREF(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execTCROLLBACKREP(Signal* signal);

  void execCREATE_TAB_REQ(Signal* signal);
  void execPREP_DROP_TAB_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  void checkWaitDropTabFailedLqh(Signal*, NodeId nodeId, Uint32 tableId);
  void execALTER_TAB_REQ(Signal* signal);
  void set_timeout_value(Uint32 timeOut);
  void set_appl_timeout_value(Uint32 timeOut);
  void set_no_parallel_takeover(Uint32);
  void updateBuddyTimer(ApiConnectRecordPtr);

  // Statement blocks
  void updatePackedList(Signal* signal, HostRecord* ahostptr, 
			Uint16 ahostIndex);
  void clearTcNodeData(Signal* signal, 
                       UintR TLastLqhIndicator,
                       UintR Tstart);
  void errorReport(Signal* signal, int place);
  void warningReport(Signal* signal, int place);
  void printState(Signal* signal, int place, ApiConnectRecordPtr apiConnectptr, bool force_trace = false);
  int seizeTcRecord(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  int seizeCacheRecord(Signal* signal, CacheRecordPtr& cachePtr, ApiConnectRecord* regApiPtr);
  void releaseCacheRecord(ApiConnectRecordPtr transPtr, CacheRecord*);
  void TCKEY_abort(Signal* signal, int place, ApiConnectRecordPtr apiConnectptr);
  void copyFromToLen(UintR* sourceBuffer, UintR* destBuffer, UintR copyLen);
  void reportNodeFailed(Signal* signal, NodeId nodeId);
  void sendPackedTCKEYCONF(Signal* signal,
                           HostRecord * ahostptr,
                           UintR hostId);
  void sendPackedSignal(Signal* signal,
                        struct PackedWordsContainer * container);
  Uint32 sendCommitLqh(Signal* signal,
                       TcConnectRecord * regTcPtr,
                       ApiConnectRecord* regApiPtr);
  Uint32 sendCompleteLqh(Signal* signal,
                         TcConnectRecord * const regTcPtr,
                         ApiConnectRecord* const regApiPtr);

  void startSendFireTrigReq(Signal*, Ptr<ApiConnectRecord>);

  void sendFireTrigReq(Signal*, Ptr<ApiConnectRecord>);

  Uint32 sendFireTrigReqLqh(Signal*, Ptr<TcConnectRecord>, Uint32 pass, ApiConnectRecord* regApiPtr);

  void checkWaitFireTrigConfDone(Signal*,
                                 Ptr<ApiConnectRecord>);

/**
 * These use modulo 2 hashing, so these need to be a number which is 2^n.
 */
#define TC_FAIL_HASH_SIZE 4096
#define TRANSID_FAIL_HASH_SIZE 1024

  void sendTCKEY_FAILREF(Signal* signal, ApiConnectRecord *);
  void sendTCKEY_FAILCONF(Signal* signal, ApiConnectRecord *);
  void routeTCKEY_FAILREFCONF(Signal* signal, const ApiConnectRecord *, 
			      Uint32 gsn, Uint32 len);
  void execTCKEY_FAILREFCONF_R(Signal* signal);
  void timer_handling(Signal *signal);
  void checkStartTimeout(Signal* signal);
  void checkStartFragTimeout(Signal* signal);
  void timeOutFoundFragLab(Signal* signal, Uint32 TscanConPtr);
  void timeOutLoopStartFragLab(Signal* signal, Uint32 TscanConPtr);
  int  releaseAndAbort(Signal* signal, ApiConnectRecord* regApiPtr);

  void scan_for_read_backup(Signal *, Uint32, Uint32, Uint32);
  void releaseMarker(ApiConnectRecord * const regApiPtr);

  Uint32 get_transid_fail_bucket(Uint32 transid1);
  void insert_transid_fail_hash(Uint32 transid1, ApiConnectRecordPtr apiConnectptr);
  void remove_from_transid_fail_hash(Signal *signal, Uint32 transid1, ApiConnectRecordPtr apiConnectptr);
  Uint32 get_tc_fail_bucket(Uint32 transid1, Uint32 tcOprec);
  void insert_tc_fail_hash(Uint32 transid1, Uint32 tcOprec);
  void remove_transaction_from_tc_fail_hash(Signal *signal, ApiConnectRecord* regApiPtr);

  void initTcFail(Signal* signal);
  void releaseTakeOver(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void setupFailData(Signal* signal, ApiConnectRecord* regApiPtr);
  bool findApiConnectFail(Signal* signal, Uint32 transid1, Uint32 transid2, ApiConnectRecordPtr& apiConnectptr);
  void initApiConnectFail(Signal* signal,
                          Uint32 transid1,
                          Uint32 transid2,
                          LqhTransConf::OperationStatus transStatus,
                          Uint32 reqinfo,
                          BlockReference applRef,
                          Uint64 gci,
                          NodeId nodeId,
                          ApiConnectRecordPtr apiConnectptr);
  void updateApiStateFail(Signal* signal,
                          Uint32 transid1,
                          Uint32 transid2,
                          LqhTransConf::OperationStatus transStatus,
                          Uint32 reqinfo,
                          BlockReference applRef,
                          Uint64 gci,
                          NodeId nodeId,
                          ApiConnectRecordPtr apiConnectptr);
  bool findTcConnectFail(Signal* signal,
                         Uint32 transid1,
                         Uint32 transid2,
                         Uint32 tcOprec,
                         ApiConnectRecordPtr apiConnectptr);
  void initTcConnectFail(Signal* signal,
                         Uint32 instanceKey,
                         Uint32 tcOprec,
                         Uint32 reqinfo,
                         LqhTransConf::OperationStatus transStatus,
                         NodeId nodeId,
                         Uint32 apiConnectPtr);
  void updateTcStateFail(Signal* signal,
                         Uint32 instanceKey,
                         Uint32 tcOprec,
                         Uint32 reqinfo,
                         LqhTransConf::OperationStatus transStatus,
                         NodeId nodeId,
                         ApiConnectRecordPtr apiConnecptr);

  bool handleFailedApiConnection(Signal*,
                                 Uint32 *TloopCount,
                                 Uint32 TapiFailedNode,
                                 bool apiNodeFailed,
                                 ApiConnectRecordPtr apiConnectptr);
  void set_api_fail_state(Uint32 TapiFailedNode, bool apiNodeFailed, ApiConnectRecord* regApiPtr);
  void handleApiFailState(Signal* signal, UintR anApiConnectptr);
  void handleFailedApiNode(Signal* signal,
                           UintR aFailedNode,
                           UintR anApiConnectPtr);
  void handleScanStop(Signal* signal, UintR aFailedNode);
  void initScanTcrec(Signal* signal);
  Uint32 initScanrec(ScanRecordPtr,  const class ScanTabReq*,
                     const UintR scanParallel,
                     const Uint32 apiPtr[],
                     Uint32 apiConnectPtr);
  void initScanfragrec(Signal* signal);
  void releaseScanResources(Signal*,
                            ScanRecordPtr,
                            ApiConnectRecordPtr apiConnectptr,
                            bool not_started = false);
  ScanRecordPtr seizeScanrec(Signal* signal);

  void sendDihGetNodesLab(Signal*, ScanRecordPtr, ApiConnectRecordPtr);
  bool sendDihGetNodeReq(Signal*,
                         ScanRecordPtr,
                         ScanFragLocationPtr &fragLocationPtr,
                         Uint32 scanFragId);
  void get_next_frag_location(ScanFragLocationPtr fragLocationPtr,
                              Uint32 & fragId,
                              Uint32 & blockRef);
  void get_and_step_next_frag_location(ScanFragLocationPtr & fragLocationPtr,
                                       ScanRecord *scanPtrP,
                                       Uint32 & fragId,
                                       Uint32 & blockRef);
  void sendFragScansLab(Signal*, ScanRecordPtr, ApiConnectRecordPtr);
  bool sendScanFragReq(Signal*,
                       ScanRecordPtr,
                       ScanFragRecPtr,
                       ScanFragLocationPtr & fragLocationPtr,
                       ApiConnectRecordPtr const apiConnectptr);
  void sendScanTabConf(Signal* signal, ScanRecordPtr, ApiConnectRecordPtr);
  void close_scan_req(Signal*, ScanRecordPtr, bool received_req, ApiConnectRecordPtr apiConnectptr);
  void close_scan_req_send_conf(Signal*, ScanRecordPtr, ApiConnectRecordPtr apiConnectptr);
  
  void checkGcp(Signal* signal);
  void commitGciHandling(Signal* signal, Uint64 Tgci, ApiConnectRecordPtr apiConnectptr);
  void copyApi(Ptr<ApiConnectRecord> dst, Ptr<ApiConnectRecord> src);
  void DIVER_node_fail_handling(Signal* signal, Uint64 Tgci, ApiConnectRecordPtr apiConnectptr);
  void gcpTcfinished(Signal* signal, Uint64 gci);
  void handleGcp(Signal* signal, ApiConnectRecordPtr);
  void hash(Signal* signal, CacheRecord * regCachePtr);
  bool handle_special_hash(Uint32 dstHash[4], 
                           const Uint32* src, Uint32 srcLen, 
                           Uint32 tabPtrI, bool distr);
  
  void initApiConnect(Signal* signal);
  void initApiConnectRec(Signal* signal, 
			 ApiConnectRecord * const regApiPtr,
			 bool releaseIndexOperations = false);
  void inithost(Signal* signal);
  void initialiseScanrec(Signal* signal);
  void initTable(Signal* signal);
  void initialiseTcConnect(Signal* signal);
  void linkApiToGcp(Ptr<GcpRecord>, Ptr<ApiConnectRecord>);
  void linkGciInGcilist(Ptr<GcpRecord>);
  void linkTcInConnectionlist(Signal* signal, ApiConnectRecord* regApiPtr);
  void releaseAbortResources(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void releaseApiCon(Signal* signal, UintR aApiConnectPtr);
  void releaseApiConCopy(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void releaseApiConnectFail(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void releaseAttrinfo(CacheRecordPtr cachePtr, ApiConnectRecord* regApiPtr);
  void releaseKeys(CacheRecord* regCachePtr);
  void releaseDirtyRead(Signal*, ApiConnectRecordPtr, TcConnectRecord*);
  void releaseDirtyWrite(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void releaseTcCon();
  void releaseTcConnectFail(Signal* signal);
  void releaseTransResources(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void checkPoolShrinkNeed(Uint32 pool_index, const TransientFastSlotPool& pool);
  void sendPoolShrink(Uint32 pool_index);
  bool seizeApiConnect(Signal* signal, ApiConnectRecordPtr& apiConnectptr);
  bool seizeApiConnectCopy(Signal* signal, ApiConnectRecord* regApiPtr);
  bool seizeApiConnectFail(Signal* signal, ApiConnectRecordPtr& apiConnectptr);
  [[noreturn]] void crash_gcp(Uint32 line, const char msg[]);
  void seizeGcp(Ptr<GcpRecord> & dst, Uint64 gci);
  void seizeTcConnectFail(Signal* signal);
  Ptr<ApiConnectRecord> sendApiCommitAndCopy(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void sendApiCommitSignal(Signal* signal, Ptr<ApiConnectRecord>);
  void sendApiLateCommitSignal(Signal* signal, Ptr<ApiConnectRecord> apiCopy);
  bool sendAttrInfoTrain(Signal* signal,
                         UintR TBRef,
                         Uint32 connectPtr,
                         Uint32 offset,
                         Uint32 attrInfoIVal,
                         ApiConnectRecord* regApiPtr);
  void sendContinueTimeOutControl(Signal* signal, Uint32 TapiConPtr);
  void sendlqhkeyreq(Signal* signal, 
                     BlockReference TBRef,
                     CacheRecord*,
                     ApiConnectRecord* regApiPtr);
  void sendSystemError(Signal* signal, int line);
  void sendtckeyconf(Signal* signal, UintR TcommitFlag, ApiConnectRecordPtr apiConnectptr);
  void unlinkApiConnect(Ptr<GcpRecord>, Ptr<ApiConnectRecord>);
  void unlinkAndReleaseGcp(Ptr<GcpRecord>);
  void unlinkReadyTcCon(ApiConnectRecord* regApiPtr);
  void handleFailedOperation(Signal* signal,
			     const LqhKeyRef * const lqhKeyRef, 
			     bool gotLqhKeyRef);
  void markOperationAborted(ApiConnectRecord * const regApiPtr,
			    TcConnectRecord * const regTcPtr);
  void clearCommitAckMarker(ApiConnectRecord * const regApiPtr,
			    TcConnectRecord * const regTcPtr);
  // Trigger and index handling
  int saveINDXKEYINFO(Signal* signal,
                      TcIndexOperation* indexOp,
                      const Uint32 *src, 
                      Uint32 len);
  bool receivedAllINDXKEYINFO(TcIndexOperation* indexOp);
  int saveINDXATTRINFO(Signal* signal,
                        TcIndexOperation* indexOp,
                        const Uint32 *src, 
                        Uint32 len);
  bool receivedAllINDXATTRINFO(TcIndexOperation* indexOp);
  Uint32 saveTRANSID_AI(Signal* signal,
                        TcIndexOperation* indexOp, 
                        const Uint32 *src,
                        Uint32 len);
  bool receivedAllTRANSID_AI(TcIndexOperation* indexOp);
  void readIndexTable(Signal* signal,
                      ApiConnectRecordPtr transPtr,
                      TcIndexOperation* indexOp,
                      Uint32 special_op_flags);
  void executeIndexOperation(Signal* signal, 
			     ApiConnectRecord* regApiPtr,
			     TcIndexOperation* indexOp);
  bool seizeIndexOperation(ApiConnectRecord* regApiPtr,
			   TcIndexOperationPtr& indexOpPtr);
  void releaseIndexOperation(ApiConnectRecord* regApiPtr,
			     TcIndexOperation* indexOp);
  void releaseAllSeizedIndexOperations(ApiConnectRecord* regApiPtr);
  void setupIndexOpReturn(ApiConnectRecord* regApiPtr,
			  TcConnectRecord* regTcPtr);

  void saveTriggeringOpState(Signal* signal,
			     TcConnectRecord* trigOp);
  void restoreTriggeringOpState(Signal* signal, 
				TcConnectRecord* trigOp);
  void trigger_op_finished(Signal* signal,
                           ApiConnectRecordPtr,
                           Uint32 triggerPtrI,
                           TcConnectRecord* triggeringOp,
                           Uint32 returnCode);
  void continueTriggeringOp(Signal* signal,
                            TcConnectRecord* trigOp,
                            ApiConnectRecordPtr);

  void executeTriggers(Signal* signal, ApiConnectRecordPtr const* transPtr);
  void waitToExecutePendingTrigger(Signal* signal, ApiConnectRecordPtr transPtr);
  bool executeTrigger(Signal* signal,
                      TcFiredTriggerData* firedTriggerData,
                      ApiConnectRecordPtr const* transPtr,
                      TcConnectRecordPtr* opPtr);
  void executeIndexTrigger(Signal* signal,
                           TcDefinedTriggerData* definedTriggerData,
                           TcFiredTriggerData* firedTriggerData,
                           ApiConnectRecordPtr const* transPtr,
                           TcConnectRecordPtr* opPtr);
  Uint32 appendDataToSection(Uint32& sectionIVal,
                             AttributeBuffer & src,
                             AttributeBuffer::DataBufferIterator & iter,
                             Uint32 len);
  bool appendAttrDataToSection(Uint32& sectionIVal,
                               AttributeBuffer& values,
                               bool withHeaders,
                               Uint32& attrId,
                               bool& hasNull);
  void insertIntoIndexTable(Signal* signal, 
                            TcFiredTriggerData* firedTriggerData, 
                            ApiConnectRecordPtr const* transPtr,
                            TcConnectRecordPtr* opPtr,
                            TcIndexData* indexData);
  void deleteFromIndexTable(Signal* signal, 
                            TcFiredTriggerData* firedTriggerData, 
                            ApiConnectRecordPtr const* transPtr,
                            TcConnectRecordPtr* opPtr,
                            TcIndexData* indexData);

  void executeReorgTrigger(Signal* signal,
                           TcDefinedTriggerData* definedTriggerData,
                           TcFiredTriggerData* firedTriggerData,
                           ApiConnectRecordPtr const* transPtr,
                           TcConnectRecordPtr* opPtr);

  void executeFKParentTrigger(Signal* signal,
                              TcDefinedTriggerData* definedTriggerData,
                              TcFiredTriggerData* firedTriggerData,
                              ApiConnectRecordPtr const* transPtr,
                              TcConnectRecordPtr* opPtr);

  void executeFKChildTrigger(Signal* signal,
                              TcDefinedTriggerData* definedTriggerData,
                              TcFiredTriggerData* firedTriggerData,
                              ApiConnectRecordPtr const* transPtr,
                              TcConnectRecordPtr* opPtr);

  void fk_readFromParentTable(Signal* signal,
                              TcFiredTriggerData* firedTriggerData,
                              ApiConnectRecordPtr const* transPtr,
                              TcConnectRecordPtr* opPtr,
                              TcFKData* fkData);
  void fk_readFromChildTable(Signal* signal,
                             TcFiredTriggerData* firedTriggerData,
                             ApiConnectRecordPtr const* transPtr,
                             TcConnectRecordPtr* opPtr,
                             TcFKData* fkData,
                             Uint32 op, Uint32 attrValuesPtrI);

  Uint32 fk_buildKeyInfo(Uint32& keyInfoPtrI, bool& hasNull,
                         LocalAttributeBuffer& values,
                         TcFKData* fkData,
                         bool parent);

  void fk_execTCINDXREQ(Signal*, ApiConnectRecordPtr, TcConnectRecordPtr,
                        Uint32 operation);
  void fk_scanFromChildTable(Signal* signal,
                             TcFiredTriggerData* firedTriggerData,
                             ApiConnectRecordPtr const* transPtr,
                             Uint32 opPtrI,
                             TcFKData* fkData,
                             Uint32 op, Uint32 attrValuesPtrI);
  void fk_scanFromChildTable_done(Signal* signal, TcConnectRecordPtr);
  void fk_scanFromChildTable_abort(Signal* signal, TcConnectRecordPtr);

  void execSCAN_TABREF(Signal*);
  void execSCAN_TABCONF(Signal*);
  void execKEYINFO20(Signal*);

  Uint32 fk_buildBounds(SegmentedSectionPtr & dst,
                        LocalAttributeBuffer & src,
                        TcFKData* fkData);
  Uint32 fk_constructAttrInfoSetNull(const TcFKData*);
  Uint32 fk_constructAttrInfoUpdateCascade(const TcFKData*,
                                           AttributeBuffer::Head&);

  bool executeFullyReplicatedTrigger(Signal* signal,
                                     TcDefinedTriggerData* definedTriggerData,
                                     TcFiredTriggerData* firedTriggerData,
                                     ApiConnectRecordPtr const* transPtr,
                                     TcConnectRecordPtr* opPtr);

  void releaseFiredTriggerData(Local_TcFiredTriggerData_fifo::Head* triggers);
  void abortTransFromTrigger(Signal* signal,
                             ApiConnectRecordPtr transPtr,
                             Uint32 error);
  // Generated statement blocks
  void warningHandlerLab(Signal* signal, int line);
  [[noreturn]] void systemErrorLab(Signal* signal, int line);
  void sendSignalErrorRefuseLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void scanTabRefLab(Signal* signal, Uint32 errCode, ApiConnectRecord* regApiPtr);
  void diFcountReqLab(Signal* signal, ScanRecordPtr, ApiConnectRecordPtr);
  void signalErrorRefuseLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void abort080Lab(Signal* signal);
  void sendKeyInfoTrain(Signal* signal,
                        BlockReference TBRef,
                        Uint32 connectPtr,
                        Uint32 offset,
                        Uint32 keyInfoIVal,
                        ApiConnectRecord* const regApiPtr);
  void abortScanLab(Signal* signal, ScanRecordPtr, Uint32 errCode, 
		    bool not_started, ApiConnectRecordPtr apiConnectptr);
  void sendAbortedAfterTimeout(Signal* signal, int Tcheck, ApiConnectRecordPtr apiConnectptr);
  void abort010Lab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void abort015Lab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void packLqhkeyreq(Signal* signal, 
                     BlockReference TBRef,
                     CacheRecordPtr,
                     ApiConnectRecordPtr apiConnectptr);
  void packLqhkeyreq040Lab(Signal* signal,
                           BlockReference TBRef,
                           CacheRecordPtr,
                           ApiConnectRecordPtr apiConnectptr);
  void returnFromQueuedDeliveryLab(Signal* signal);
  void insert_take_over_failed_node(Signal*, Uint32 failedNodeId);
  void startTakeOverLab(Signal* signal,
                        Uint32 instanceId,
                        Uint32 failedNodeId);
  void toCompleteHandlingLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void toCommitHandlingLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void toAbortHandlingLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void abortErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void nodeTakeOverCompletedLab(Signal* signal,
                                NodeId nodeId,
                                Uint32 maxInstanceId);
  void ndbsttorry010Lab(Signal* signal);
  void commit020Lab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void complete010Lab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void releaseAtErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void appendToSectionErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void scanKeyinfoLab(Signal* signal, CacheRecord*, ApiConnectRecordPtr);
  void scanAttrinfoLab(Signal* signal, UintR Tlen, ApiConnectRecordPtr apiConnectptr);
  void attrinfoDihReceivedLab(Signal* signal, CacheRecordPtr cachePtr, ApiConnectRecordPtr apiConnectptr);
  void aiErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void scanReleaseResourcesLab(Signal* signal);
  void scanCompletedLab(Signal* signal);
  void scanError(Signal* signal, ScanRecordPtr, Uint32 errorCode);
  void diverify010Lab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void intstartphase3x010Lab(Signal* signal);
  void sttorryLab(Signal* signal);
  void abortBeginErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void tabStateErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void wrongSchemaVersionErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void noFreeConnectionErrorLab(Signal* signal, ApiConnectRecordPtr apiConnectptr);
  void tckeyreq050Lab(Signal* signal, CacheRecordPtr cachePtr, ApiConnectRecordPtr apiConnectptr);
  void timeOutFoundLab(Signal* signal, UintR anAdd, Uint32 errCode);
  void completeTransAtTakeOverLab(Signal* signal, UintR TtakeOverInd);
  void completeTransAtTakeOverDoLast(Signal* signal, UintR TtakeOverInd);
  void completeTransAtTakeOverDoOne(Signal* signal, UintR TtakeOverInd, ApiConnectRecordPtr apiConnectptr);
  void timeOutLoopStartLab(Signal* signal, Uint32 apiConnectPtr);
  void initialiseRecordsLab(Signal* signal, UintR Tdata0, Uint32, Uint32);
  void tckeyreq020Lab(Signal* signal, CacheRecordPtr cachePtr, ApiConnectRecordPtr apiConnectptr);
  void intstartphase1x010Lab(Signal* signal, NodeId nodeId);
  void startphase1x010Lab(Signal* signal);

  void lqhKeyConf_checkTransactionState(Signal * signal,
                                        ApiConnectRecordPtr apiConnectptr);

  void checkDropTab(Signal* signal);

  void checkScanActiveInFailedLqh(Signal* signal,
				  Uint32 scanPtrI,
				  Uint32 failedNodeId);
  void checkScanFragList(Signal*, Uint32 failedNodeId, ScanRecord * scanP, 
                         Local_ScanFragRec_dllist::Head&);

  void nodeFailCheckTransactions(Signal*,Uint32 transPtrI,Uint32 failedNodeId);
  void ndbdFailBlockCleanupCallback(Signal* signal, Uint32 failedNodeId, Uint32 ignoredRc);
  void checkNodeFailComplete(Signal* signal, Uint32 failedNodeId, Uint32 bit);

  void apiFailBlockCleanupCallback(Signal* signal, Uint32 failedNodeId, Uint32 ignoredRc);
  bool isRefreshSupported() const;
  
  // Initialisation
  void initData();
  void initRecords(const ndb_mgm_configuration_iterator * mgm_cfg);

  /**
   * Functions used at completion of activities tracked for timing.
   * Currently we track
   * 1) Transactions
   * 2) Key operations
   * 3) Scan operations
   * 4) Execution of SCAN_FRAGREQ's (local scans)
   */
   void time_track_init_histogram_limits(void);
   Uint32 time_track_calculate_histogram_position(NDB_TICKS & start_ticks);

   void time_track_complete_scan(ScanRecord * const scanPtr,
                                 Uint32 apiNodeId);
   void time_track_complete_scan_error(ScanRecord * const scanPtr,
                                       Uint32 apiNodeId);
   void time_track_complete_key_operation(TcConnectRecord * const tcConnectPtr,
                                          Uint32 apiNodeId,
                                          Uint32 dbNodeId);
   void time_track_complete_index_key_operation(
     TcConnectRecord * const tcConnectPtr,
     Uint32 apiNodeId,
     Uint32 dbNodeId);
   void time_track_complete_key_operation_error(
     TcConnectRecord * const tcConnectPtr,
     Uint32 apiNodeId,
     Uint32 dbNodeId);
   void time_track_complete_scan_frag(ScanFragRec * const scanFragPtr);
   void time_track_complete_scan_frag_error(ScanFragRec *const scanFragPtr);
   void time_track_complete_transaction(ApiConnectRecord *const regApiPtr);
   void time_track_complete_transaction_error(
     ApiConnectRecord * const regApiPtr);
   Uint32 check_own_location_domain(Uint16*, Uint32);
protected:
  virtual bool getParam(const char* name, Uint32* count);
  
private:
  Uint32 c_time_track_histogram_boundary[TIME_TRACK_HISTOGRAM_RANGES];
  bool c_time_track_activated;
  // Transit signals


  alignas(64) ApiConnectRecord_pool c_apiConnectRecordPool;
  RSS_AP_SNAPSHOT(c_apiConnectRecordPool);

  alignas(64) TcConnectRecord_pool tcConnectRecord;
  TcConnectRecordPtr tcConnectptr;
  UintR ctcConnectFailCount;

  alignas(64) CacheRecord_pool c_cacheRecordPool;
  RSS_AP_SNAPSHOT(c_cacheRecordPool);

  HostRecord *hostRecord;
  HostRecordPtr hostptr;
  UintR chostFilesize;
  NdbNodeBitmask c_alive_nodes;

  Uint32 c_ongoing_take_over_cnt;
  alignas(64) GcpRecord_pool c_gcpRecordPool;
  RSS_AP_SNAPSHOT(c_gcpRecordPool);

  TableRecord *tableRecord;
  UintR ctabrecFilesize;

  UintR thashValue;
  UintR tdistrHashValue;

  UintR ttransid_ptr;
  UintR cfailure_nr;
  UintR coperationsize;
  UintR ctcTimer;
  UintR cDbHbInterval;

  Uint32 c_lqhkeyconf_direct_sent;

  Uint64 tcheckGcpId;

  // Montonically increasing counters
  struct MonotonicCounters {
    Uint64 cattrinfoCount;
    Uint64 ctransCount;
    Uint64 ccommitCount;
    Uint64 creadCount;
    Uint64 csimpleReadCount;
    Uint64 cwriteCount;
    Uint64 cabortCount;
    Uint64 c_scan_count;
    Uint64 c_range_scan_count;
    Uint64 clocalReadCount;
    Uint64 clocalWriteCount;

    // Resource usage counter(not monotonic)
    Uint32 cconcurrentOp;
    Uint32 cconcurrentScans;

    MonotonicCounters() :
     cattrinfoCount(0),
     ctransCount(0),
     ccommitCount(0),
     creadCount(0),
     csimpleReadCount(0),
     cwriteCount(0),
     cabortCount(0),
     c_scan_count(0),
     c_range_scan_count(0),
     clocalReadCount(0),
     clocalWriteCount(0),
     cconcurrentOp(0) {}

    Uint32 build_event_rep(Signal* signal)
    {
      /*
        Read saved value from CONTINUEB, subtract from
        counter and write to EVENT_REP
      */
      const Uint32 attrinfoCount =    diff(signal, 1, cattrinfoCount);
      const Uint32 transCount =       diff(signal, 3, ctransCount);
      const Uint32 commitCount =      diff(signal, 5, ccommitCount);
      const Uint32 readCount =        diff(signal, 7, creadCount);
      const Uint32 simpleReadCount =  diff(signal, 9, csimpleReadCount);
      const Uint32 writeCount =       diff(signal, 11, cwriteCount);
      const Uint32 abortCount =       diff(signal, 13, cabortCount);
      const Uint32 scan_count =       diff(signal, 15, c_scan_count);
      const Uint32 range_scan_count = diff(signal, 17, c_range_scan_count);
      const Uint32 localread_count = diff(signal, 19, clocalReadCount);
      const Uint32 localwrite_count = diff(signal, 21, clocalWriteCount);

      signal->theData[0] = NDB_LE_TransReportCounters;
      signal->theData[1] = transCount;
      signal->theData[2] = commitCount;
      signal->theData[3] = readCount;
      signal->theData[4] = simpleReadCount;
      signal->theData[5] = writeCount;
      signal->theData[6] = attrinfoCount;
      signal->theData[7] = cconcurrentOp; // Exception that confirms the rule!
      signal->theData[8] = abortCount;
      signal->theData[9] = scan_count;
      signal->theData[10] = range_scan_count;
      signal->theData[11] = localread_count;
      signal->theData[12] = localwrite_count;
      return 13;
    }

    Uint32 build_continueB(Signal* signal) const
    {
      /* Save current value of counters to CONTINUEB */
      const Uint64* vars[] = {
        &cattrinfoCount, &ctransCount, &ccommitCount,
        &creadCount, &csimpleReadCount, &cwriteCount,
        &cabortCount, &c_scan_count, &c_range_scan_count,
        &clocalReadCount, &clocalWriteCount
      };
      const size_t num = sizeof(vars)/sizeof(vars[0]);

      for (size_t i = 0; i < num; i++)
      {
        signal->theData[1+i*2] = Uint32(*vars[i] >> 32);
        signal->theData[1+i*2+1] = Uint32(*vars[i]);
      }
      return 1 + num * 2;
    }
  private:
    Uint32 diff(Signal* signal, size_t pos, Uint64 curr) const
    {
      const Uint64 old =
        (signal->theData[pos+1] | (Uint64(signal->theData[pos]) << 32));
      return (Uint32)(curr - old);
    }
  } c_counters;
  RSS_OP_SNAPSHOT(cconcurrentOp);

  Uint16 cownNodeid;
  Uint16 terrorCode;

  LocalApiConnectRecord_api_fifo::Head capiConnectPREPARE_TO_COMMITList;

  LocalGcpRecord_list::Head c_gcpRecordList;
  UintR cConcScanCount;
  RSS_OP_SNAPSHOT(cConcScanCount);

  TableRecordPtr tabptr;
  LocalApiConnectRecord_api_list::Head c_apiConnectFailList;
  Uint32 capiConnectFailCount;

  BlockReference cdihblockref;
  BlockReference cownref;                   /* OWN BLOCK REFERENCE */

  ApiConnectRecordPtr timeOutptr;

  alignas(64) ScanRecord_pool scanRecordPool;
  UintR cscanrecFileSize;
  UintR cscanrecFileSize_original;

  alignas(64) ScanFragRec_pool c_scan_frag_pool;
  RSS_AP_SNAPSHOT(c_scan_frag_pool);
  ScanFragRecPtr scanFragptr;

  BlockReference cndbcntrblockref;
  BlockInstance cspjInstanceRR;    // SPJ instance round-robin counter

  Uint16 csignalKey;
  Uint16 csystemnodes;
  Uint16 cnodes[4];
  NodeId cmasterNodeId;
  UintR cnoParallelTakeOver;
  TimeOutCheckState ctimeOutCheckFragActive;

  Uint32 ctimeOutCheckFragCounter;
  Uint32 ctimeOutCheckCounter;
  Uint32 ctimeOutValue;
  Uint32 ctimeOutCheckDelay;
  Uint32 ctimeOutCheckDelayScan;
  Uint32 ctimeOutCheckHeartbeat;
  Uint32 ctimeOutCheckLastHeartbeat;
  Uint32 ctimeOutMissedHeartbeats;
  Uint32 ctimeOutCheckHeartbeatScan;
  Uint32 ctimeOutCheckLastHeartbeatScan;
  Uint32 ctimeOutMissedHeartbeatsScan;
  Uint32 c_appl_timeout_value;

  TimeOutCheckState ctimeOutCheckActive;

  Uint64 c_elapsed_time_millis;
  NDB_TICKS c_latestTIME_SIGNAL;

  BlockReference capiFailRef;
  UintR cpackedListIndex;
  Uint16 cpackedList[MAX_NODES];
  UintR capiConnectClosing[MAX_NODES];
  UintR con_lineNodes;

  UintR tabortInd;

  BlockReference tblockref;

  Uint8 tcurrentReplicaNo;

  UintR tindex;
  UintR tmaxData;

  BlockReference tusersblkref;
  UintR tuserpointer;

  UintR ctransidFailHash[TRANSID_FAIL_HASH_SIZE];
  UintR ctcConnectFailHash[TC_FAIL_HASH_SIZE];

  /**
   * Commit Ack handling
   */
public:
  struct CommitAckMarker
  {
    STATIC_CONST( TYPE_ID = RT_DBTC_COMMIT_ACK_MARKER );

    CommitAckMarker()
    : m_magic(Magic::make(TYPE_ID)),
      apiConnectPtr(RNIL),
      apiNodeId(0)
    {}

    Uint32 m_magic;
    Uint32 transid1;
    Uint32 transid2;
    Uint32 nextHash;
    Uint32 prevHash;
    Uint32 apiConnectPtr;
    Uint16 apiNodeId;
    CommitAckMarkerBuffer::Head theDataBuffer;

    inline bool equal(const CommitAckMarker & p) const {
      return ((p.transid1 == transid1) && (p.transid2 == transid2));
    }
    
    inline Uint32 hashValue() const {
      return transid1;
    }
    bool insert_in_commit_ack_marker(Dbtc *tc,
                                     Uint32 instanceKey,
                                     NodeId nodeId);
    // insert all keys when exact keys not known
    bool insert_in_commit_ack_marker_all(Dbtc *tc,
                                         NodeId nodeId);
  };

private:
  typedef Ptr<CommitAckMarker> CommitAckMarkerPtr;
  typedef TransientPool<CommitAckMarker> CommitAckMarker_pool;
  STATIC_CONST(DBTC_COMMIT_ACK_MARKER_TRANSIENT_POOL_INDEX = 12);
  typedef DLHashTable<CommitAckMarker_pool> CommitAckMarker_hash;
  typedef CommitAckMarker_hash::Iterator CommitAckMarkerIterator;
  
  CommitAckMarker_pool m_commitAckMarkerPool;
  CommitAckMarker_hash m_commitAckMarkerHash;
  RSS_AP_SNAPSHOT(m_commitAckMarkerPool);
  
  static const Uint32 c_transient_pool_count = 13;
  TransientFastSlotPool* c_transient_pools[c_transient_pool_count];
  Bitmask<1> c_transient_pools_shrinking;

  void execTC_COMMIT_ACK(Signal* signal);
  void sendRemoveMarkers(Signal*, CommitAckMarker *, Uint32);
  void sendRemoveMarker(Signal* signal, 
			NodeId nodeId,
                        Uint32 instanceKey,
			Uint32 transid1, 
			Uint32 transid2,
                        Uint32 removed_by_fail_api);
  void removeMarkerForFailedAPI(Signal* signal, NodeId nodeId, Uint32 bucket);

  bool getAllowStartTransaction(NodeId nodeId, Uint32 table_single_user_mode) const {
    if (unlikely(getNodeState().getSingleUserMode()))
    {
      if (getNodeState().getSingleUserApi() == nodeId || table_single_user_mode)
        return true;
      else
        return false;
    }
    return getNodeState().startLevel < NodeState::SL_STOPPING_2;
  }
  
  void checkAbortAllTimeout(Signal* signal, Uint32 sleepTime);
  struct AbortAllRecord {
    AbortAllRecord() : clientRef(0), oldTimeOutValue(0) {}
    Uint32 clientData;
    BlockReference clientRef;
    
    Uint32 oldTimeOutValue;
  };
  AbortAllRecord c_abortRec;

  bool validate_filter(Signal*);
  bool match_and_print(Signal*, ApiConnectRecordPtr);
  bool ndbinfo_write_trans(Ndbinfo::Row&, ApiConnectRecordPtr);

#ifdef ERROR_INSERT
  bool testFragmentDrop(Signal* signal);
#endif

  /************************** API CONNECT RECORD ***********************/
  /* *******************************************************************/
  /* THE API CONNECT RECORD CONTAINS THE CONNECTION RECORD TO WHICH THE*/
  /* APPLICATION CONNECTS. THE APPLICATION CAN SEND ONE OPERATION AT A */
  /* TIME. IT CAN SEND A NEW OPERATION IMMEDIATELY AFTER SENDING THE   */
  /* PREVIOUS OPERATION. THEREBY SEVERAL OPERATIONS CAN BE ACTIVE IN   */
  /* ONE TRANSACTION WITHIN TC. THIS IS ACHIEVED BY USING THE API      */
  /* CONNECT RECORD. EACH ACTIVE OPERATION IS HANDLED BY THE TC        */
  /* CONNECT RECORD. AS SOON AS THE TC CONNECT RECORD HAS SENT THE     */
  /* REQUEST TO THE LQH IT IS READY TO RECEIVE NEW OPERATIONS. THE     */
  /* LQH CONNECT RECORD TAKES CARE OF WAITING FOR AN OPERATION TO      */
  /* COMPLETE. WHEN AN OPERATION HAS COMPLETED ON THE LQH CONNECT      */
  /* RECORD A NEW OPERATION CAN BE STARTED ON THIS LQH CONNECT RECORD. */
  /*******************************************************************>*/
  /*                                                                   */
  /*       API CONNECT RECORD ALIGNED TO BE 256 BYTES                  */
  /*******************************************************************>*/
  /************************** TC CONNECT RECORD ************************/
  /* *******************************************************************/
  /* TC CONNECT RECORD KEEPS ALL INFORMATION TO CARRY OUT A TRANSACTION*/
  /* THE TRANSACTION CONTROLLER ESTABLISHES CONNECTIONS TO DIFFERENT   */
  /* BLOCKS TO CARRY OUT THE TRANSACTION. THERE CAN BE SEVERAL RECORDS */
  /* PER ACTIVE TRANSACTION. THE TC CONNECT RECORD COOPERATES WITH THE */
  /* API CONNECT RECORD FOR COMMUNICATION WITH THE API AND WITH THE    */
  /* LQH CONNECT RECORD FOR COMMUNICATION WITH THE LQH'S INVOLVED IN   */
  /* THE TRANSACTION. TC CONNECT RECORD IS PERMANENTLY CONNECTED TO A  */
  /* RECORD IN DICT AND ONE IN DIH. IT CONTAINS A LIST OF ACTIVE LQH   */
  /* CONNECT RECORDS AND A LIST OF STARTED BUT NOT ACTIVE LQH CONNECT  */
  /* RECORDS. IT DOES ALSO CONTAIN A LIST OF ALL OPERATIONS THAT ARE   */
  /* EXECUTED WITH THE TC CONNECT RECORD.                              */
  /*******************************************************************>*/
  /*       TC_CONNECT RECORD ALIGNED TO BE 128 BYTES                   */
  /*******************************************************************>*/
  LocalTcConnectRecord_fifo::Head cfreeTcConnectFail;

  /* POINTER FOR THE LQH RECORD*/
  /* ************************ HOST RECORD ********************************* */
  /********************************************************/
  /* THIS RECORD CONTAINS ALIVE-STATUS ON ALL NODES IN THE*/
  /* SYSTEM                                               */
  /********************************************************/
  /*       THIS RECORD IS ALIGNED TO BE 8 BYTES.         */
  /********************************************************/
  /* ************************ TABLE RECORD ******************************** */
  /********************************************************/
  /* THIS RECORD CONTAINS THE CURRENT SCHEMA VERSION OF   */
  /* ALL TABLES IN THE SYSTEM.                            */
  /********************************************************/
  /*-------------------------------------------------------------------------*/
  /*       THE TC CONNECTION USED BY THIS SCAN.                              */
  /*-------------------------------------------------------------------------*/
  /*-------------------------------------------------------------------------*/
  /*	LENGTH READ FOR A PARTICULAR SCANNED OPERATION.			     */
  /*-------------------------------------------------------------------------*/
  /*-------------------------------------------------------------------------*/
  /*       REFERENCE TO THE SCAN RECORD FOR THIS SCAN PROCESS.               */
  /*-------------------------------------------------------------------------*/
  /* *********************************************************************** */
  /* ******$                           DATA BUFFER                   ******$ */
  /*                                                                         */
  /*       THIS BUFFER IS USED AS A GENERAL DATA STORAGE.                    */
  /* *********************************************************************** */
  /* *********************************************************************** */
  /* ******$                 ATTRIBUTE INFORMATION RECORD            ******$ */
  /*
    CAN CONTAIN ONE (1) ATTRINFO SIGNAL. ONE SIGNAL CONTAINS 24 ATTR.          
    INFO WORDS. BUT 32 ELEMENTS ARE USED TO MAKE PLEX HAPPY.                   
    SOME OF THE ELEMENTS ARE USED TO THE FOLLOWING THINGS:                     
    DATA LENGHT IN THIS RECORD IS STORED IN THE ELEMENT INDEXED BY             
    ZINBUF_DATA_LEN.                                                           
    NEXT FREE ATTRBUF IS POINTED OUT BY THE ELEMENT INDEXED BY                 
    PREVIOUS ATTRBUF IS POINTED OUT BY THE ELEMENT INDEXED BY ZINBUF_PREV      
    (NOT USED YET).                                                            
    NEXT ATTRBUF IS POINTED OUT BY THE ELEMENT INDEXED BY ZINBUF_NEXT.        
  */
  /* ********************************************************************** */
  /**************************************************************************/
  /*           GLOBAL CHECKPOINT INFORMATION RECORD                         */
  /*                                                                        */
  /*       THIS RECORD IS USED TO STORE THE GCP NUMBER AND A COUNTER        */
  /*                DURING THE COMPLETION PHASE OF THE TRANSACTION          */
  /**************************************************************************/
  /*                                                                        */
  /*       GCP RECORD ALIGNED TO BE 32 BYTES                                */
  /**************************************************************************/
  /**************************************************************************/
  /*                          TC_FAIL_RECORD                                */
  /*  THIS RECORD IS USED WHEN HANDLING TAKE OVER OF ANOTHER FAILED TC NODE.*/
  /**************************************************************************/
  TcFailRecord *tcFailRecord;
  TcFailRecordPtr tcNodeFailptr;
  /**************************************************************************/
  // Temporary variables that are not allowed to use for storage between
  // signals. They
  // can only be used in a signal to transfer values between subroutines.
  // In the long run
  // those variables should be removed and exchanged for stack
  // variable communication.
  /**************************************************************************/

  Uint32 c_gcp_ref;
  Uint32 c_gcp_data;

  Uint32 c_sttor_ref;

  Uint32 m_load_balancer_location;

#ifdef ERROR_INSERT
  // Used with ERROR_INSERT 8078 + 8079 to check API_FAILREQ handling
  Uint32 c_lastFailedApi;
#endif
  Uint32 m_deferred_enabled;
  Uint32 m_max_writes_per_trans;
  Uint32 m_take_over_operations;
#endif

#ifndef DBTC_STATE_EXTRACT
  void dump_trans(ApiConnectRecordPtr transPtr);
  bool hasOp(ApiConnectRecordPtr transPtr, Uint32 op);

public:
static Uint64 getTransactionMemoryNeed(
    const Uint32 dbtc_instance_count,
    const ndb_mgm_configuration_iterator * mgm_cfg,
    const Uint32 TakeOverOperations,
    const Uint32 MaxNoOfConcurrentIndexOperations,
    const Uint32 MaxNoOfConcurrentOperations,
    const Uint32 MaxNoOfConcurrentScans,
    const Uint32 MaxNoOfConcurrentTransactions,
    const Uint32 MaxNoOfFiredTriggers,
    const Uint32 MaxNoOfLocalScans,
    const Uint32 TransactionBufferMemory);
#endif
};

#ifndef DBTC_STATE_EXTRACT
inline void Dbtc::checkPoolShrinkNeed(const Uint32 pool_index,
                                      const TransientFastSlotPool& pool)
{
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  ndbrequire(pool_index < c_transient_pool_count);
  ndbrequire(c_transient_pools[pool_index] == &pool);
#endif
  if (pool.may_shrink())
  {
    sendPoolShrink(pool_index);
  }
}

#endif

#ifdef DBTC_MAIN
void Dbtc::setApiConTimer(ApiConnectRecordPtr apiConPtr, Uint32 value, Uint32 line)
  {
    const Uint32 apiConTimer = apiConPtr.p->m_apiConTimer;
    ApiConTimersPtr apiConTimers;
    ndbrequire(apiConTimer != RNIL);
    apiConTimers.i = apiConTimer >> ApiConTimers::INDEX_BITS;
    c_apiConTimersPool.getPtr(apiConTimers);
    const Uint32 timer_index = apiConTimer & ApiConTimers::INDEX_MASK;
    ndbassert(timer_index < apiConTimers.p->m_top);
    ndbassert(apiConTimers.p->m_count > 0);
    ndbassert(apiConTimers.p->m_entries[timer_index].m_apiConnectRecord ==
                apiConPtr.i);

    apiConTimers.p->m_entries[timer_index].m_timer = value;
    apiConPtr.p->m_apiConTimer_line = line;
  }
#endif

#undef JAM_FILE_ID

#endif
