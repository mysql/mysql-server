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

#ifndef DBUTIL_H
#define DBUTIL_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <NodeBitmask.hpp>

#include <ArrayList.hpp>
#include <ArrayPool.hpp>
#include <SLList.hpp>
#include <DLList.hpp>
#include <DLFifoList.hpp>
#include <DataBuffer.hpp>
#include <KeyTable.hpp>

#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/UtilPrepare.hpp>
#include <signaldata/UtilExecute.hpp>
#include <signaldata/UtilLock.hpp>
#include <SimpleProperties.hpp>

#define UTIL_WORDS_PER_PAGE 1023

/**
 * @class DbUtil 
 * @brief Database utilities
 *
 * This block implements transactional services which can be used by other 
 * blocks.
 *
 * @section secSequence   Module: The Sequence Service
 *
 * A sequence is a varaible stored in the database.  Each time it is 
 * requested with "NextVal" it returns a unique number.  If requested 
 * with "CurrVal" it returns the current number.
 * 
 * - Request: SEQUENCE_REQ 
 *   Requests the 'NextVal' or 'CurrVal' for sequence variable 'sequenceId'.
 *
 * - Response: SEQUENCE_CONF / REF (if failure)
 *   Returns value requested.
 */
class DbUtil : public SimulatedBlock
{
public:
  DbUtil(const class Configuration & conf);
  virtual ~DbUtil();
  BLOCK_DEFINES(DbUtil);
  
protected:
  /**
   * Startup & Misc
   */
  void execSTTOR(Signal* signal);
  void execNDB_STTOR(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execCONTINUEB(Signal* signal);

  /**
   * Sequence Service : Public interface
   */
  void execUTIL_SEQUENCE_REQ(Signal* signal);
  void execUTIL_SEQUENCE_REF(Signal* signal);
  void execUTIL_SEQUENCE_CONF(Signal* signal);

  /**
   * Prepare Service : Public interface
   */
  void execUTIL_PREPARE_REQ(Signal* signal);
  void execUTIL_PREPARE_CONF(Signal* signal);
  void execUTIL_PREPARE_REF(Signal* signal);

  /**
   * Delete Service : Public interface
   */
  void execUTIL_DELETE_REQ(Signal* signal);
  void execUTIL_DELETE_REF(Signal* signal);
  void execUTIL_DELETE_CONF(Signal* signal);

  /**
   * Execute Service : Public interface
   */
  void execUTIL_EXECUTE_REQ(Signal* signal);
  void execUTIL_EXECUTE_REF(Signal* signal);
  void execUTIL_EXECUTE_CONF(Signal* signal);

  /**
   * Prepare Release Service : Public interface
   */
  void execUTIL_RELEASE_REQ(Signal* signal);
  void execUTIL_RELEASE_CONF(Signal* signal);
  void execUTIL_RELEASE_REF(Signal* signal);

  /**
   * Backend interface to a used TC service 
   */
  void execTCSEIZECONF(Signal* signal);
  void execTCKEYCONF(Signal* signal);
  void execTCKEYREF(Signal* signal);
  void execTCROLLBACKREP(Signal* signal);
  void execTCKEY_FAILCONF(Signal* signal);
  void execTCKEY_FAILREF(Signal* signal);
  void execTRANSID_AI(Signal* signal);

  /**
   * Backend interface to a used DICT service
   */
  void execGET_TABINFOREF(Signal*);
  void execGET_TABINFO_CONF(Signal* signal);

private:
  
public:
  struct PreparedOperation;

  typedef DataBuffer<11> KeyInfoBuffer;
  typedef KeyInfoBuffer::ConstDataBufferIterator KeyInfoIterator;
  typedef DataBuffer<11> AttrInfoBuffer;
  typedef AttrInfoBuffer::ConstDataBufferIterator AttrInfoIterator;
  typedef DataBuffer<11> ResultSetBuffer;
  typedef DataBuffer<11>  ResultSetInfoBuffer;
  typedef DataBuffer<1>  AttrMappingBuffer;
  
  /** 
   * @struct  Page32
   * @brief   For storing SimpleProperties objects and similar temporary data
   */
  struct Page32 {
    Uint32  data[UTIL_WORDS_PER_PAGE];
    Uint32  nextPool;                  // Note: This used as data when seized
  };

  /**
   * @struct  Prepare
   * @brief   Info regarding prepare request (contains a prepared operation)
   *
   * The prepare phase interprets the table and attribute names sent
   * in the prepare request from the client and asks DICT for meta
   * information.
   */
  struct Prepare {
    Prepare(ArrayPool<Page32> & ap) : preparePages(ap) {}

    /*** Client info ***/
    Uint32 clientRef;
    Uint32 clientData;

    /** 
     * SimpleProp sent in UTIL_PREPARE_REQ 
     *
     * Example format:
     * - UtilPrepareReq::NoOfOperations=1
     * - UtilPrepareReq::OperationType=UtilPrepareReq::Delete
     * - UtilPrepareReq::TableName="SYSTAB_0"
     * - UtilPrepareReq::AttributeName="SYSKEY_0"
     */
    Uint32 prepDataLen;
    Array<Page32>  preparePages;  

    /*** PreparedOperation constructed in Prepare phase ***/
    Ptr<PreparedOperation> prepOpPtr;

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;

    void print() const {
      ndbout << "[-Prepare-" << endl
	     << " clientRef: " << clientRef
	     << ", clientData: " << clientData
	     << "]" << endl;
    }
  };
    
  /**
   * @struct  PreparedOperation
   * @brief   Contains instantiated TcKeyReq signaldata for operation
   * 
   * The prepare phase is finished by storing the request in a
   * PreparedOperation record.
   */
  struct PreparedOperation {
    PreparedOperation(AttrMappingBuffer::DataBufferPool & am,
		      AttrInfoBuffer::DataBufferPool & ai,
		      ResultSetInfoBuffer::DataBufferPool & rs) :
      releaseFlag(false), attrMapping(am), attrInfo(ai), rsInfo(rs)
    {
      pkBitmask.clear();
    }

    /*** Various Operation Info ***/
    Uint32    keyLen;          // Length of primary key (fixed size is assumed)
    Uint32    rsLen;           // Size of result set
    Uint32    noOfKeyAttr;     // Number of key attributes
    Uint32    noOfAttr;        // Number of attributes
    bool      releaseFlag;     // flag if operation release after completion

    /**
     * Attribute Mapping
     *
     * This datastructure (buffer of AttributeHeader:s) are used to map 
     * each execute request to a TCKEYREQ train of signals.
     *
     * The datastructure contains (AttributeId, Position) pairs, where
     * - AttributeId  is id used in database, and
     * - Position     is position of attribute value in TCKEYREQ keyinfo
     *                part of the train of signals which will be send to TC.
     *                Position == 0x3fff means it should *not* be sent
     *                in keyinfo part.
     */
    AttrMappingBuffer    attrMapping;

    /*** First signal in tckeyreq train ***/
    Uint32    tckeyLenInBytes;    // TcKeyReq total signal length (in bytes)
    Uint32    keyDataPos;         // Where to store keydata[] in tckey signal
                                  // (in #words from base in tckey signal)
    TcKeyReq  tckey;              // Signaldata for first signal in train
    
    /*** Attrinfo signals sent to TC (part of tckeyreq train) ***/
    AttrInfoBuffer       attrInfo;

    /*** Result of executed operation ***/
    ResultSetInfoBuffer  rsInfo;    

    Bitmask<MAX_ATTRIBUTES_IN_TABLE> pkBitmask;

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
    
    void print() const {
      ndbout << "[-PreparedOperation-" << endl
	     << " keyLen: " << keyLen
	     << ", rsLen: " << rsLen
	     << ", noOfKeyAttr: " << noOfKeyAttr 
	     << ", noOfAttr: " << noOfAttr 
	     << ", tckeyLenInBytes: " << tckeyLenInBytes 
	     << ", keyDataPos: " << keyDataPos << endl
	     << "-AttrMapping- (AttrId, KeyPos)-pairs "
	     << "(Pos=3fff if non-key attr):" << endl;
      attrMapping.print(stdout);
      ndbout << "[-tckey- ";
      printTCKEYREQ(stdout, (Uint32*)&tckey, 8, 0);
      ndbout << "[-attrInfo- ";
      attrInfo.print(stdout);
      ndbout << "[-rsInfo- ";
      rsInfo.print(stdout);
      ndbout << "]]]]" << endl;
    }
  };
  
  /**
   * @struct  Operation
   * @brief   Used in execution (contains resultset and buffers for result)
   */
  struct Operation {
    Operation(KeyInfoBuffer::DataBufferPool & ki, 
	      AttrInfoBuffer::DataBufferPool & ai,
	      ResultSetBuffer::DataBufferPool & _rs) :
      prepOp_i(RNIL), keyInfo(ki), attrInfo(ai), rs(_rs) {}
    
    PreparedOperation *            prepOp;
    Uint32                         prepOp_i;
    KeyInfoBuffer                        keyInfo;
    AttrInfoBuffer                       attrInfo;
    ResultSetBuffer                      rs;
    ResultSetBuffer::DataBufferIterator  rsIterator;
    
    Uint32 transPtrI;
    
    Uint32 rsRecv;
    Uint32 rsExpect;
    inline bool complete() const { return rsRecv == rsExpect; }
    
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };

    void print() const {
      ndbout << "[-Operation-" << endl
	     << " transPtrI: " << transPtrI
	     << ", rsRecv: " << rsRecv;
      ndbout << "[-PreparedOperation-" << endl;
      prepOp->print();
      ndbout << "[-keyInfo-" << endl;
      keyInfo.print(stdout);
      ndbout << "[-attrInfo-" << endl;
      attrInfo.print(stdout);
      ndbout << "]]" << endl;
    }
  };

  /**
   * @struct  Transaction
   * @brief   Used in execution (contains list of operations)
   */
  struct Transaction {
    Transaction(ArrayPool<Page32> & ap, ArrayPool<Operation> & op) :
      executePages(ap), operations(op) {}

    Uint32 clientRef;
    Uint32 clientData;
    Array<Page32>  executePages;  

    Uint32 gsn;         // Request type (SEQUENCE, DELETE, etc)
    union {
      /**
       * Sequence transaction
       */
      struct {
	Uint32 sequenceId;
	Uint32 requestType;
      } sequence;
    };
    
    Uint32 connectPtr;
    Uint32 transId[2];
    SLList<Operation> operations;

    Uint32 errorCode;
    Uint32 noOfRetries;
    Uint32 sent;        // No of operations sent
    Uint32 recv;        // No of completed operations received
    inline bool complete() const { return sent == recv; };

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;

    void print() const {
      ndbout << "[-Transaction-" << endl
	     << " clientRef: " << clientRef
	     << ", clientData: " << clientData
	     << ", gsn: " << gsn 
	     << ", errorCode: " << errorCode 
	     << endl 
	     << " sent: " << sent << " operations" 
	     << ", recv: " << recv << " completed operations";
      OperationPtr opPtr;
      this->operations.first(opPtr);
      while(opPtr.i != RNIL){
	ndbout << "[-Operation-" << endl;
	opPtr.p->print();
        this->operations.next(opPtr);
      }
      ndbout << "]" << endl;
    }
  };

  typedef Ptr<Page32>             Page32Ptr;
  typedef Ptr<Prepare>            PreparePtr;
  typedef Ptr<Transaction>        TransactionPtr;
  typedef Ptr<Operation>          OperationPtr;
  typedef Ptr<PreparedOperation>  PreparedOperationPtr;

  Uint32                          c_transId[2];
  ArrayPool<Page32>               c_pagePool;
  ArrayPool<Prepare>              c_preparePool;
  ArrayPool<Operation>            c_operationPool;
  ArrayPool<PreparedOperation>    c_preparedOperationPool;
  ArrayPool<Transaction>          c_transactionPool;

  DataBuffer<1>::DataBufferPool   c_attrMappingPool;
  DataBuffer<11>::DataBufferPool  c_dataBufPool;
  DLList<Prepare>                 c_runningPrepares;
  DLList<PreparedOperation>       c_runningPreparedOperations;
  DLList<Transaction>             c_seizingTransactions; // Being seized at TC
  DLList<Transaction>             c_runningTransactions; // Seized and now exec.
  
  void getTransId(Transaction *);
  void initResultSet(ResultSetBuffer &, const ResultSetInfoBuffer &);
  void runTransaction(Signal* signal, TransactionPtr);
  void runOperation(Signal* signal, TransactionPtr &, OperationPtr &, Uint32);
  void sendKeyInfo(Signal* signal, 
		   KeyInfo* keyInfo,
		   const KeyInfoBuffer & keyBuf,
		   KeyInfoIterator & kit);
  void sendAttrInfo(Signal*, 
		    AttrInfo* attrInfo, 
		    const AttrInfoBuffer &,
		    AttrInfoIterator & ait);
  int getResultSet(Signal* signal, const Transaction * transP,
		   struct LinearSectionPtr sectionsPtr[]);
  void finishTransaction(Signal*, TransactionPtr);
  void releaseTransaction(TransactionPtr transPtr);
  void hardcodedPrepare();
  void connectTc(Signal* signal);
  void reportSequence(Signal*, const Transaction *);
  void readPrepareProps(Signal* signal, 
			SimpleProperties::Reader* reader, 
			Uint32 senderData);
  void prepareOperation(Signal*, PreparePtr);
  void sendUtilPrepareRef(Signal*, UtilPrepareRef::ErrorCode, Uint32, Uint32);
  void sendUtilExecuteRef(Signal*, UtilExecuteRef::ErrorCode, 
			  Uint32, Uint32, Uint32);
  void releasePrepare(PreparePtr);
  void releasePreparedOperation(PreparedOperationPtr);

  /***************************************************************************
   * Lock manager
   */
  struct LockQueueElement {
    Uint32 m_senderData;
    Uint32 m_senderRef;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<LockQueueElement> LockQueueElementPtr;

  struct LockQueue {
    LockQueue(){}
    LockQueue(Uint32 id) : m_queue() { m_lockId = id; m_lockKey = 0;}
    union {
      Uint32 m_lockId;
      Uint32 key;
    };
    Uint32 m_lockKey;
    DLFifoList<LockQueueElement>::Head m_queue;
    union {
      Uint32 nextHash;
      Uint32 nextPool;
    };
    Uint32 prevHash;
    
    Uint32 hashValue() const {
      return m_lockId;
    }
    bool equal(const LockQueue & rec) const {
      return m_lockId == rec.m_lockId;
    }
  };
  typedef Ptr<LockQueue> LockQueuePtr;
  
  
  ArrayPool<LockQueue> c_lockQueuePool;
  ArrayPool<LockQueueElement> c_lockElementPool;
  KeyTable<LockQueue> c_lockQueues;
  
  void execUTIL_CREATE_LOCK_REQ(Signal* signal);
  void execUTIL_DESTORY_LOCK_REQ(Signal* signal);
  void execUTIL_LOCK_REQ(Signal* signal);
  void execUTIL_UNLOCK_REQ(Signal* signal);
  
  void sendLOCK_REF(Signal*, const UtilLockReq * req, UtilLockRef::ErrorCode);
  void sendLOCK_CONF(Signal*, LockQueue *, LockQueueElement *);

  void sendUNLOCK_REF(Signal*, const UtilUnlockReq*, UtilUnlockRef::ErrorCode);
  void sendUNLOCK_CONF(Signal*, LockQueue *, LockQueueElement *);

  // For testing of mutex:es
  void mutex_created(Signal* signal, Uint32 mutexId, Uint32 retVal);
  void mutex_destroyed(Signal* signal, Uint32 mutexId, Uint32 retVal);
  void mutex_locked(Signal* signal, Uint32 mutexId, Uint32 retVal);
  void mutex_unlocked(Signal* signal, Uint32 mutexId, Uint32 retVal);
};

#endif
