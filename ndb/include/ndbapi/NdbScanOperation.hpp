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

/*****************************************************************************
 * Name:          NdbScanOperation.hpp
 * Include:
 * Link:
 * Author:        Martin Sköld
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Table scan support
 * Documentation:
 * Adjust:  2002-04-01  Martin Sköld   First version.
 ****************************************************************************/

#ifndef NdbScanOperation_H
#define NdbScanOperation_H

#include <NdbOperation.hpp>

class NdbBlob;
class NdbResultSet;

/**
 * @class NdbScanOperation
 * @brief Class of scan operations for use in transactions.  
 */
class NdbScanOperation : public NdbOperation {
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbBlob;
public:
  /**
   * readTuples returns a NdbResultSet where tuples are stored.
   * Tuples are not stored in NdbResultSet until execute(NoCommit) 
   * has been executed and nextResult has been called.
   * 
   * @param parallel  Scan parallelism
   * @param batch No of rows to fetch from each fragment at a time
   * @param LockMode  Scan lock handling   
   * @note specifying 0 for batch and parallall means max performance
   */ 
  int readTuples(LockMode = LM_Read, 
		 Uint32 batch = 0, Uint32 parallel = 0);
  
  inline int readTuples(int parallell){
    return readTuples(LM_Read, 0, parallell);
  }
  
  inline int readTuplesExclusive(int parallell = 0){
    return readTuples(LM_Exclusive, 0, parallell);
  }
  
  NdbBlob* getBlobHandle(const char* anAttrName);
  NdbBlob* getBlobHandle(Uint32 anAttrId);

  /**
   * Get the next tuple in a scan transaction. 
   * 
   * After each call to NdbResult::nextResult
   * the buffers and NdbRecAttr objects defined in 
   * NdbOperation::getValue are updated with values 
   * from the scanned tuple. 
   *
   * @param  fetchAllowed  If set to false, then fetching is disabled
   *
   * The NDB API will contact the NDB Kernel for more tuples 
   * when necessary to do so unless you set the fetchAllowed 
   * to false. 
   * This will force NDB to process any records it
   * already has in it's caches. When there are no more cached 
   * records it will return 2. You must then call nextResult
   * with fetchAllowed = true in order to contact NDB for more 
   * records.
   *
   * fetchAllowed = false is useful when you want to update or 
   * delete all the records fetched in one transaction(This will save a
   *  lot of round trip time and make updates or deletes of scanned 
   * records a lot faster). 
   * While nextResult(false)
   * returns 0 take over the record to another transaction. When 
   * nextResult(false) returns 2 you must execute and commit the other 
   * transaction. This will cause the locks to be transferred to the 
   * other transaction, updates or deletes will be made and then the 
   * locks will be released.
   * After that, call nextResult(true) which will fetch new records and
   * cache them in the NdbApi. 
   * 
   * @note  If you don't take over the records to another transaction the 
   *        locks on those records will be released the next time NDB Kernel
   *        is contacted for more records.
   *
   * @note  Please contact for examples of efficient scan
   *        updates and deletes.
   * 
   * @note  See ndb/examples/ndbapi_scan_example for usage.
   *
   * @return 
   * -  -1: if unsuccessful,<br>
   * -   0: if another tuple was received, and<br> 
   * -   1: if there are no more tuples to scan.
   * -   2: if there are no more cached records in NdbApi
   */
  int nextResult(bool fetchAllowed = true, bool forceSend = false);

  /**
   * Close result set (scan)
   */
  void close(bool forceSend = false);

  /**
   * Restart
   */
  int restart(bool forceSend = false);
  
  /**
   * Transfer scan operation to an updating transaction. Use this function 
   * when a scan has found a record that you want to update. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForUpdate using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(it's taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to update the found tuple.
   *
   * @param updateTrans the update transaction connection.
   * @return an NdbOperation or NULL.
   */
  NdbOperation* updateCurrentTuple();
  NdbOperation*	updateCurrentTuple(NdbConnection* updateTrans);

  /**
   * Transfer scan operation to a deleting transaction. Use this function 
   * when a scan has found a record that you want to delete. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForDelete using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(its taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to delete the found tuple.
   *
   * @param deleteTrans the delete transaction connection.
   * @return an NdbOperation or NULL.
   */
  int deleteCurrentTuple();
  int deleteCurrentTuple(NdbConnection* takeOverTransaction);
  
protected:
  NdbScanOperation(Ndb* aNdb);
  virtual ~NdbScanOperation();

  int nextResultImpl(bool fetchAllowed = true, bool forceSend = false);
  virtual void release();
  
  int close_impl(class TransporterFacade*, bool forceSend = false);

  // Overloaded methods from NdbCursorOperation
  int executeCursor(int ProcessorId);

  // Overloaded private methods from NdbOperation
  int init(const NdbTableImpl* tab, NdbConnection* myConnection);
  int prepareSend(Uint32  TC_ConnectPtr, Uint64  TransactionId);
  int doSend(int ProcessorId);
  void checkForceSend(bool forceSend);

  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  NdbConnection *m_transConnection;

  // Scan related variables
  Uint32 theParallelism;
  Uint32 m_keyInfo;

  int getFirstATTRINFOScan();
  int doSendScan(int ProcessorId);
  int prepareSendScan(Uint32 TC_ConnectPtr, Uint64 TransactionId);
  
  int fix_receivers(Uint32 parallel);
  void reset_receivers(Uint32 parallel, Uint32 ordered);
  Uint32* m_array; // containing all arrays below
  Uint32 m_allocated_receivers;
  NdbReceiver** m_receivers;      // All receivers

  Uint32* m_prepared_receivers;   // These are to be sent

  /**
   * owned by API/user thread
   */
  Uint32 m_current_api_receiver;
  Uint32 m_api_receivers_count;
  NdbReceiver** m_api_receivers;  // These are currently used by api
  
  /**
   * owned by receiver thread
   */
  Uint32 m_conf_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_conf_receivers; // receive thread puts them here
  
  /**
   * owned by receiver thread
   */
  Uint32 m_sent_receivers_count;  // NOTE needs mutex to access
  NdbReceiver** m_sent_receivers; // receive thread puts them here
  
  int send_next_scan(Uint32 cnt, bool close, bool forceSend = false);
  void receiver_delivered(NdbReceiver*);
  void receiver_completed(NdbReceiver*);
  void execCLOSE_SCAN_REP();

  int getKeyFromKEYINFO20(Uint32* data, unsigned size);
  NdbOperation*	takeOverScanOp(OperationType opType, NdbConnection*);
  
  Uint32 m_ordered;
  Uint32 m_read_range_no;
};

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(){
  return updateCurrentTuple(m_transConnection);
}

inline
NdbOperation* 
NdbScanOperation::updateCurrentTuple(NdbConnection* takeOverTrans){
  return takeOverScanOp(NdbOperation::UpdateRequest, 
			takeOverTrans);
}

inline
int
NdbScanOperation::deleteCurrentTuple(){
  return deleteCurrentTuple(m_transConnection);
}

inline
int
NdbScanOperation::deleteCurrentTuple(NdbConnection * takeOverTrans){
  void * res = takeOverScanOp(NdbOperation::DeleteRequest, 
			      takeOverTrans);
  if(res == 0)
    return -1;
  return 0;
}

#endif
