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

#ifndef TRIX_H
#define TRIX_H

#include <SimulatedBlock.hpp>
#include <trigger_definitions.h>
#include <DataBuffer.hpp>
#include <SimpleProperties.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/BuildIndx.hpp>

// Error codes
#define INTERNAL_ERROR_ILLEGAL_CALL 4344
#define INTERNAL_ERROR_TRIX_BUSY 4345

/**
 * TRIX - This block manages triggers and index (in coop with DICT)
 */
class Trix : public SimulatedBlock
{
public:
  Trix(Block_context&);
  virtual ~Trix();

public:
  // Subscription data, when communicating with SUMA

  enum RequestType {
    TABLE_REORG = 0,
    INDEX_BUILD = 1
  };
  typedef DataBuffer<11> AttrOrderBuffer;

private:
  // Private attributes
  
  BLOCK_DEFINES(Trix);

  // Declared but not defined
  //DBtrix(const Trix &obj);
  //void operator = (const Trix &);

  // Block state
  enum BlockState {
    NOT_STARTED,
    STARTED,
    NODE_FAILURE,
    IDLE,
    BUSY
  };

  BlockState c_blockState;

  // Node data needed when communicating with remote TRIX:es
  struct NodeRecord {
    bool alive;
    BlockReference trixRef;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  
  typedef Ptr<NodeRecord> NodeRecPtr;

  /**
   * The pool of node records
   */
  ArrayPool<NodeRecord> c_theNodeRecPool;

  /**
   * The list of other NDB nodes
   */  
  DLList<NodeRecord> c_theNodes;

  Uint32 c_masterNodeId;
  BlockReference c_masterTrixRef;
  Uint16 c_noNodesFailed;
  Uint16 c_noActiveNodes;

  AttrOrderBuffer::DataBufferPool c_theAttrOrderBufferPool;

  struct SubscriptionRecord {
    SubscriptionRecord(AttrOrderBuffer::DataBufferPool & aop):
      attributeOrder(aop)
    {}
    RequestType requestType;
    BlockReference userReference; // For user
    Uint32 connectionPtr; // For user
    Uint32 subscriptionId; // For Suma
    Uint32 subscriptionKey; // For Suma
    Uint32 prepareId; // For DbUtil
    Uint32 indexType;
    Uint32 sourceTableId;
    Uint32 targetTableId;
    AttrOrderBuffer attributeOrder;
    Uint32 noOfIndexColumns;
    Uint32 noOfKeyColumns;
    Uint32 parallelism;
    BuildIndxRef::ErrorCode errorCode;
    bool subscriptionCreated;
    bool pendingSubSyncContinueConf;
    Uint32 expectedConf; // Count in n UTIL_EXECUTE_CONF + 1 SUB_SYNC_CONF
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  
  typedef Ptr<SubscriptionRecord> SubscriptionRecPtr;

  /**
   * The pool of node records
   */
  ArrayPool<SubscriptionRecord> c_theSubscriptionRecPool;

  /**
   * The list of other subscriptions
   */  
  DLList<SubscriptionRecord> c_theSubscriptions;

  // System start
  void execREAD_CONFIG_REQ(Signal* signal);
  void execSTTOR(Signal* signal);
  void execNDB_STTOR(Signal* signal);

  // Node management
  void execREAD_NODESCONF(Signal* signal);
  void execREAD_NODESREF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  // Debugging
  void execDUMP_STATE_ORD(Signal* signal);

  // Build index
  void execBUILDINDXREQ(Signal* signal);
  void execBUILDINDXCONF(Signal* signal);
  void execBUILDINDXREF(Signal* signal);

  void execUTIL_PREPARE_CONF(Signal* signal);
  void execUTIL_PREPARE_REF(Signal* signal);
  void execUTIL_EXECUTE_CONF(Signal* signal);
  void execUTIL_EXECUTE_REF(Signal* signal);
  void execUTIL_RELEASE_CONF(Signal* signal);
  void execUTIL_RELEASE_REF(Signal* signal);

  // Suma signals
  void execSUB_CREATE_CONF(Signal* signal);
  void execSUB_CREATE_REF(Signal* signal);
  void execSUB_REMOVE_CONF(Signal* signal);
  void execSUB_REMOVE_REF(Signal* signal);
  void execSUB_SYNC_CONF(Signal* signal);
  void execSUB_SYNC_REF(Signal* signal);
  void execSUB_SYNC_CONTINUE_REQ(Signal* signal);
  void execSUB_TABLE_DATA(Signal* signal);

  // Utility functions
  void setupSubscription(Signal* signal, SubscriptionRecPtr subRecPtr);
  void startTableScan(Signal* signal, SubscriptionRecPtr subRecPtr);
  void prepareInsertTransactions(Signal* signal, SubscriptionRecPtr subRecPtr);
  void executeInsertTransaction(Signal* signal, SubscriptionRecPtr subRecPtr,
				SegmentedSectionPtr headerPtr,
				SegmentedSectionPtr dataPtr);
  void buildComplete(Signal* signal, SubscriptionRecPtr subRecPtr);
  void buildFailed(Signal* signal, 
		   SubscriptionRecPtr subRecPtr, 
		   BuildIndxRef::ErrorCode);
  void checkParallelism(Signal* signal, SubscriptionRecord* subRec);
};

#endif
