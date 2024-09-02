/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TRIX_H
#define TRIX_H

#include <trigger_definitions.h>
#include <DataBuffer.hpp>
#include <SimpleProperties.hpp>
#include <SimulatedBlock.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/IndexStatSignal.hpp>
#include <signaldata/TuxBound.hpp>
#include "portlib/ndb_compiler.h"

#define JAM_FILE_ID 432

#define ZNOT_FOUND 626

// Error codes
#define INTERNAL_ERROR_ILLEGAL_CALL 4344
#define INTERNAL_ERROR_TRIX_BUSY 4345

/**
 * TRIX - This block manages triggers and index (in coop with DICT)
 */
class Trix : public SimulatedBlock {
 public:
  Trix(Block_context &);
  ~Trix() override;

 public:
  // Subscription data, when communicating with SUMA

  enum RequestType {
    REORG_COPY = 0,
    REORG_DELETE = 1,
    INDEX_BUILD = 2,
    STAT_UTIL = 3  // PK op of HEAD table directly via DBUTIL
    ,
    STAT_CLEAN = 4,
    STAT_SCAN = 5,
    FK_BUILD = 6
    // ALTER_TABLE
  };
  typedef DataBuffer<11, ArrayPool<DataBufferSegment<11>>> AttrOrderBuffer;

 private:
  // Private attributes

  BLOCK_DEFINES(Trix);

  // Declared but not defined
  // DBtrix(const Trix &obj);
  // void operator = (const Trix &);

  AttrOrderBuffer::DataBufferPool c_theAttrOrderBufferPool;

  struct SubscriptionRecord {
    SubscriptionRecord(AttrOrderBuffer::DataBufferPool &aop)
        : attributeOrder(aop) {}
    enum RequestFlags {
      RF_WAIT_GCP = 0x1,
      RF_NO_DISK = 0x2,
      RF_TUP_ORDER = 0x4
    };
    Uint32 m_flags;
    RequestType requestType;
    BlockReference userReference;  // For user
    Uint32 connectionPtr;          // For user
    Uint32 subscriptionId;         // For Suma
    Uint32 schemaTransId;
    Uint32 subscriptionKey;  // For Suma
    Uint32 prepareId;        // For DbUtil
    Uint32 indexType;
    Uint32 sourceTableId;
    Uint32 targetTableId;
    AttrOrderBuffer attributeOrder;
    Uint32 noOfIndexColumns;
    Uint32 noOfKeyColumns;
    Uint32 parallelism;
    Uint32 fragCount;
    Uint32 fragId;
    Uint32 syncPtr;
    BuildIndxRef::ErrorCode errorCode;
    bool subscriptionCreated;
    bool pendingSubSyncContinueConf;
    Uint32 expectedConf;  // Count in n UTIL_EXECUTE_CONF + 1 SUB_SYNC_CONF
    Uint64 m_rows_processed;
    Uint64 m_gci;
    Uint32 m_statPtrI;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };

  typedef Ptr<SubscriptionRecord> SubscriptionRecPtr;
  typedef ArrayPool<SubscriptionRecord> SubscriptionRecord_pool;
  typedef DLList<SubscriptionRecord_pool> SubscriptionRecord_list;

  /**
   * The pool of node records
   */
  SubscriptionRecord_pool c_theSubscriptionRecPool;
  RSS_AP_SNAPSHOT(c_theSubscriptionRecPool);

  /**
   * The list of other subscriptions
   */
  SubscriptionRecord_list c_theSubscriptions;

  /*
   * Ordered index stats.  Implements sub-ops of DBDICT index stat
   * schema op.  Each sub-op is a simple REQ which seizes and releases
   * a stat op here before returning CONF or REF.  A stat op always has
   * an associated SubscriptionRecord.  It is used for SUMA index scans
   * and as proxy for PK ops to DBUTIL.
   */

  bool c_statGetMetaDone;
  struct SysColumn {
    Uint32 pos;
    const char *name;
    bool keyFlag;
  };
  struct SysTable {
    const char *name;
    mutable Uint32 tableId;
    const Uint32 columnCount;
    const SysColumn *columnList;
  };
  struct SysIndex {
    const char *name;
    mutable Uint32 tableId;
    mutable Uint32 indexId;
  };
  static const SysColumn g_statMetaHead_column[];
  static const SysColumn g_statMetaSample_column[];
  static const SysTable g_statMetaHead;
  static const SysTable g_statMetaSample;
  static const SysIndex g_statMetaSampleX1;

  struct StatOp {
    struct Meta {
      GetTabInfoConf m_conf;
      Callback m_cb;
    };
    struct Data {
      Int32 m_head_found;
      Uint32 m_indexId;
      Uint32 m_indexVersion;
      Uint32 m_tableId;
      Uint32 m_fragCount;
      Uint32 m_valueFormat;
      Uint32 m_sampleVersion;
      Uint32 m_loadTime;
      Uint32 m_sampleCount;
      Uint32 m_keyBytes;
      Uint32 *m_statKey;
      Uint32 *m_statValue;
      Data() {
        m_head_found = -1;
        m_sampleVersion = 0;
      }
    };
    struct Attr {
      Uint32 *m_attr;
      Uint32 m_attrMax;
      Uint32 m_attrSize;
      Uint32 *m_data;
      Uint32 m_dataMax;
      Uint32 m_dataSize;
      Attr() {}
    };
    struct Util {
      Uint32 m_prepareId;
      bool m_not_found;
      Callback m_cb;
      Util() {
        m_prepareId = RNIL;
        m_not_found = false;  // read + ZNOT_FOUND
      }
    };
    struct Clean {
      Uint32 m_cleanCount;
      // bounds on index_id, index_version, sample_version
      Uint32 m_bound[3 * 3];
      Uint32 m_boundSize;
      Clean() {}
    };
    struct Scan {
      Uint32 m_sampleCount;
      Uint32 m_keyBytes;
      Scan() {}
    };
    struct Drop {};
    struct Send {
      const SysTable *m_sysTable;
      Uint32 m_operationType;  // UtilPrepareReq::OperationTypeValue
      Uint32 m_prepareId;
      Send() {}
    };
    IndexStatImplReq m_req;
    Uint32 m_requestType;
    const char *m_requestName;
    Uint32 m_subRecPtrI;
    Meta m_meta;
    Data m_data;
    Attr m_attr;
    Util m_util;
    Clean m_clean;
    Scan m_scan;
    Drop m_drop;
    Send m_send;
    Uint32 m_errorCode;
    Uint32 m_errorLine;
    union {
      Uint32 m_ownPtrI;
      Uint32 nextPool;
    };
    StatOp() {
      m_subRecPtrI = RNIL;
      m_errorCode = 0;
      m_errorLine = 0;
    }
  };
  typedef Ptr<StatOp> StatOpPtr;
  typedef ArrayPool<StatOp> StatOp_pool;

  StatOp_pool c_statOpPool;
  RSS_AP_SNAPSHOT(c_statOpPool);

  /* Max schema object build batchsize from config */
  Uint32 c_maxUIBuildBatchSize;
  Uint32 c_maxFKBuildBatchSize;
  Uint32 c_maxReorgBuildBatchSize;

  // System start
  void execREAD_CONFIG_REQ(Signal *signal);
  void execSTTOR(Signal *signal);

  // Node management
  void execNODE_FAILREP(Signal *signal);

  // Debugging
  void execDUMP_STATE_ORD(Signal *signal);

  void execDBINFO_SCANREQ(Signal *signal);

  // Build index
  void execBUILD_INDX_IMPL_REQ(Signal *signal);
  void execBUILD_INDX_IMPL_CONF(Signal *signal);
  void execBUILD_INDX_IMPL_REF(Signal *signal);

  void execCOPY_DATA_IMPL_REQ(Signal *signal);

  // Build FK
  void execBUILD_FK_IMPL_REQ(Signal *);

  void execUTIL_PREPARE_CONF(Signal *signal);
  void execUTIL_PREPARE_REF(Signal *signal);
  void execUTIL_EXECUTE_CONF(Signal *signal);
  void execUTIL_EXECUTE_REF(Signal *signal);
  void execUTIL_RELEASE_CONF(Signal *signal);
  void execUTIL_RELEASE_REF(Signal *signal);

  // Suma signals
  void execSUB_CREATE_CONF(Signal *signal);
  void execSUB_CREATE_REF(Signal *signal);
  void execSUB_REMOVE_CONF(Signal *signal);
  void execSUB_REMOVE_REF(Signal *signal);
  void execSUB_SYNC_CONF(Signal *signal);
  void execSUB_SYNC_REF(Signal *signal);
  void execSUB_SYNC_CONTINUE_REQ(Signal *signal);
  void execSUB_TABLE_DATA(Signal *signal);

  // GCP
  void execWAIT_GCP_REF(Signal *);
  void execWAIT_GCP_CONF(Signal *);

  // Utility functions
  void setupSubscription(Signal *signal, SubscriptionRecPtr subRecPtr);
  void startTableScan(Signal *signal, SubscriptionRecPtr subRecPtr);
  void prepareInsertTransactions(Signal *signal, SubscriptionRecPtr subRecPtr);
  void executeBuildInsertTransaction(Signal *signal, SubscriptionRecPtr);
  void executeReorgTransaction(Signal *, SubscriptionRecPtr, Uint32);
  void executeBuildFKTransaction(Signal *signal, SubscriptionRecPtr);
  void buildComplete(Signal *signal, SubscriptionRecPtr subRecPtr);
  void wait_gcp(Signal *, SubscriptionRecPtr subRecPtr, Uint32 delay = 0);
  void buildFailed(Signal *signal, SubscriptionRecPtr subRecPtr,
                   BuildIndxRef::ErrorCode);
  void checkParallelism(Signal *signal, SubscriptionRecord *subRec);

  // index stats
  StatOp &statOpGetPtr(Uint32 statPtrI);
  bool statOpSeize(Uint32 &statPtrI);
  void statOpRelease(StatOp &);
  void execINDEX_STAT_IMPL_REQ(Signal *);
  // sys tables metadata
  void statMetaGetHead(Signal *, StatOp &);
  void statMetaGetHeadCB(Signal *, Uint32 statPtrI, Uint32 ret);
  void statMetaGetSample(Signal *, StatOp &);
  void statMetaGetSampleCB(Signal *, Uint32 statPtrI, Uint32 ret);
  void statMetaGetSampleX1(Signal *, StatOp &);
  void statMetaGetSampleX1CB(Signal *, Uint32 statPtrI, Uint32 ret);
  void sendGetTabInfoReq(Signal *, StatOp &, const char *name);
  void execGET_TABINFO_CONF(Signal *);
  void execGET_TABINFO_REF(Signal *);
  // continue
  void statGetMetaDone(Signal *, StatOp &);
  // head table ops
  void statHeadRead(Signal *, StatOp &);
  void statHeadReadCB(Signal *, Uint32 statPtrI, Uint32 ret);
  void statHeadInsert(Signal *, StatOp &);
  void statHeadInsertCB(Signal *, Uint32 statPtrI, Uint32 ret);
  void statHeadUpdate(Signal *, StatOp &);
  void statHeadUpdateCB(Signal *, Uint32 statPtrI, Uint32 ret);
  void statHeadDelete(Signal *, StatOp &);
  void statHeadDeleteCB(Signal *, Uint32 statPtrI, Uint32 ret);
  // util
  void statUtilPrepare(Signal *, StatOp &);
  void statUtilPrepareConf(Signal *, Uint32 statPtrI);
  void statUtilPrepareRef(Signal *, Uint32 statPtrI);
  void statUtilExecute(Signal *, StatOp &);
  void statUtilExecuteConf(Signal *, Uint32 statPtrI);
  void statUtilExecuteRef(Signal *, Uint32 statPtrI);
  void statUtilRelease(Signal *, StatOp &);
  void statUtilReleaseConf(Signal *, Uint32 statPtrI);
  // continue
  void statReadHeadDone(Signal *, StatOp &);
  void statInsertHeadDone(Signal *, StatOp &);
  void statUpdateHeadDone(Signal *, StatOp &);
  void statDeleteHeadDone(Signal *, StatOp &);
  // clean
  void statCleanBegin(Signal *, StatOp &);
  void statCleanPrepare(Signal *, StatOp &);
  void statCleanExecute(Signal *, StatOp &);
  void statCleanRelease(Signal *, StatOp &);
  void statCleanEnd(Signal *, StatOp &);
  // scan
  void statScanBegin(Signal *, StatOp &);
  void statScanPrepare(Signal *, StatOp &);
  void statScanExecute(Signal *, StatOp &);
  void statScanRelease(Signal *, StatOp &);
  void statScanEnd(Signal *, StatOp &);
  // drop
  void statDropBegin(Signal *, StatOp &);
  void statDropEnd(Signal *, StatOp &);
  // send
  void statSendPrepare(Signal *, StatOp &);
  void statSendExecute(Signal *, StatOp &);
  void statSendRelease(Signal *, StatOp &);
  // data
  void statDataPtr(StatOp &, Uint32 i, Uint32 *&dptr, Uint32 &bytes);
  void statDataOut(StatOp &, Uint32 i);
  void statDataIn(StatOp &, Uint32 i);
  // abort ongoing
  void statAbortUtil(Signal *, StatOp &);
  void statAbortUtilCB(Signal *, Uint32 statPtrI, Uint32 ret);
  // conf and ref
  void statOpSuccess(Signal *, StatOp &);
  void statOpConf(Signal *, StatOp &);
  void statOpError(Signal *, StatOp &, Uint32 errorCode, Uint32 errorLine,
                   const Uint32 *supress = 0);
  void statOpAbort(Signal *, StatOp &);
  void statOpRef(Signal *, StatOp &);
  void statOpRef(Signal *, const IndexStatImplReq *, Uint32 errorCode,
                 Uint32 errorLine);
  void statOpEvent(StatOp &, const char *level, const char *msg, ...)
      ATTRIBUTE_FORMAT(printf, 4, 5);
  // debug
  friend class NdbOut &operator<<(NdbOut &, const StatOp &stat);
};

#undef JAM_FILE_ID

#endif
