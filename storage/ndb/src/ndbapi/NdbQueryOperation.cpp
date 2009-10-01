/*
   Copyright (C) 2009 Sun Microsystems Inc
    All rights reserved. Use is subject to license terms.

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


#include "NdbQueryOperationImpl.hpp"
#include <ndb_global.h>
#include "NdbQueryBuilder.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include "signaldata/TcKeyReq.hpp"
#include "signaldata/ScanTab.hpp"
#include "signaldata/QueryTree.hpp"

#include "AttributeHeader.hpp"
#include "NdbRecord.hpp"
#include "NdbRecAttr.hpp"
#include "TransporterFacade.hpp"
#include "NdbApiSignal.hpp"
#include "NdbTransaction.hpp"

/* Various error codes that are not specific to NdbQuery. */
STATIC_CONST(Err_MemoryAlloc = 4000);
STATIC_CONST(Err_SendFailed = 4002);
STATIC_CONST(Err_UnknownColumn = 4004);
STATIC_CONST(Err_ReceiveFromNdbFailed = 4008);
STATIC_CONST(Err_NodeFailCausedAbort = 4028);
STATIC_CONST(Err_MixRecAttrAndRecord = 4284);
STATIC_CONST(Err_DifferentTabForKeyRecAndAttrRec = 4287);

/* A 'void' index for a tuple in internal parent / child correlation structs .*/
STATIC_CONST( tupleNotFound = 0xffffffff);

/** For scans, we receiver n parallel streams of data. There is a 
  * NdbResultStream object for each such stream. (For lookups, there is a 
  * single result stream.)
  */
class NdbResultStream {
public:
  /** A map from tuple correlation Id to tuple number.*/
  class TupleIdMap {
  public:
    explicit TupleIdMap():m_vector(){}

    void put(Uint16 id, Uint32 num);

    Uint32 get(Uint16 id) const;

  private:
    struct Pair{
      /** Tuple id, unique within this batch and stream.*/
      Uint16 m_id;
      /** Tuple number, among tuples received in this stream.*/
      Uint16 m_num;
    };
    Vector<Pair> m_vector;

    /** No copying.*/
    TupleIdMap(const TupleIdMap&);
    TupleIdMap& operator=(const TupleIdMap&);
  }; // class TupleIdMap

  explicit NdbResultStream(NdbQueryOperationImpl& operation, Uint32 streamNo);

  ~NdbResultStream();

  /** Stream number within operation (0 - parallelism)*/
  const Uint32 m_streamNo;
  /** The receiver object that unpacks transid_AI messages.*/
  NdbReceiver m_receiver;
  /** The number of transid_AI messages received.*/
  Uint32 m_transidAICount;
  /** A map from tuple correlation Id to tuple number.*/
  TupleIdMap m_correlToTupNumMap;
  /** Number of pending TCKEYREF or TRANSID_AI messages for this stream.*/
  int m_pendingResults;
  /** True if there is a pending SCAN_TABCONF messages for this stream.*/
  bool m_pendingScanTabConf;
  /** Prepare for receiving results. */
  int prepare();

  Uint32 getChildTupleIdx(Uint32 childNo, Uint32 tupleNo) const;
  void setChildTupleIdx(Uint32 childNo, Uint32 tupleNo, Uint32 index);
    
  /** Get the correlation number of the parent of a given row. This number
   * can be use.
   */
  Uint32 getParentTupleCorr(Uint32 rowNo) const { 
    return m_parentTupleCorr[rowNo];
  }

  void setParentTupleCorr(Uint32 rowNo, Uint32 correlationNum) const;
    
  /** Check if batch is complete for this stream. */
  bool isBatchComplete() const { 
    return m_pendingResults==0 && !m_pendingScanTabConf;
  }

private:
  /** Operation to which this resultStream belong.*/
  NdbQueryOperationImpl& m_operation;

  /** One-dimensional array. For each tuple, this array holds the correlation
   * number of the corresponding parent tuple.
   */
  Uint32* m_parentTupleCorr;

  /** Two dimenional array of indexes to child tuples 
   * ([childOperationNo, ownTupleNo]) This is used for finding the child 
   * tuple in the corresponding resultStream of 
   * the child operation.
   */
  Uint32* m_childTupleIdx;

  /** No copying.*/
  NdbResultStream(const NdbResultStream&);
  NdbResultStream& operator=(const NdbResultStream&);
}; //class NdbResultStream

NdbQuery::NdbQuery(NdbQueryImpl& impl):
  m_impl(impl)
{}

NdbQuery::~NdbQuery()
{}

Uint32
NdbQuery::getNoOfOperations() const
{
  return m_impl.getNoOfOperations();
}

NdbQueryOperation*
NdbQuery::getQueryOperation(Uint32 index) const
{
  return &m_impl.getQueryOperation(index).getInterface();
}

NdbQueryOperation*
NdbQuery::getQueryOperation(const char* ident) const
{
  NdbQueryOperationImpl* op = m_impl.getQueryOperation(ident);
  return (op) ? &op->getInterface() : NULL;
}

Uint32
NdbQuery::getNoOfParameters() const
{
  return m_impl.getNoOfParameters();
}

const NdbParamOperand*
NdbQuery::getParameter(const char* name) const
{
  return m_impl.getParameter(name);
}

const NdbParamOperand*
NdbQuery::getParameter(Uint32 num) const
{
  return m_impl.getParameter(num);
}

NdbQuery::NextResultOutcome
NdbQuery::nextResult(bool fetchAllowed, bool forceSend)
{
  return m_impl.nextResult(fetchAllowed, forceSend);
}

void
NdbQuery::close(bool forceSend, bool release)
{
  m_impl.close(forceSend,release);
}

NdbTransaction*
NdbQuery::getNdbTransaction() const
{
  return m_impl.getNdbTransaction();
}

const NdbError& 
NdbQuery::getNdbError() const {
  return m_impl.getNdbError();
};

NdbQueryOperation::NdbQueryOperation(NdbQueryOperationImpl& impl)
  :m_impl(impl)
{}
NdbQueryOperation::~NdbQueryOperation()
{}

Uint32
NdbQueryOperation::getNoOfParentOperations() const
{
  return m_impl.getNoOfParentOperations();
}

NdbQueryOperation*
NdbQueryOperation::getParentOperation(Uint32 i) const
{
  return &m_impl.getParentOperation(i).getInterface();
}

Uint32 
NdbQueryOperation::getNoOfChildOperations() const
{
  return m_impl.getNoOfChildOperations();
}

NdbQueryOperation* 
NdbQueryOperation::getChildOperation(Uint32 i) const
{
  return &m_impl.getChildOperation(i).getInterface();
}

const NdbQueryOperationDef&
NdbQueryOperation::getQueryOperationDef() const
{
  return m_impl.getQueryOperationDef().getInterface();
}

NdbQuery& 
NdbQueryOperation::getQuery() const {
  return m_impl.getQuery().getInterface();
};

NdbRecAttr*
NdbQueryOperation::getValue(const char* anAttrName,
			    char* resultBuffer)
{
  return m_impl.getValue(anAttrName, resultBuffer);
}

NdbRecAttr*
NdbQueryOperation::getValue(Uint32 anAttrId, 
			    char* resultBuffer)
{
  return m_impl.getValue(anAttrId, resultBuffer);
}

NdbRecAttr*
NdbQueryOperation::getValue(const NdbDictionary::Column* column, 
			    char* resultBuffer)
{
  return m_impl.getValue(column, resultBuffer);
}

int
NdbQueryOperation::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  // FIXME: Errors must be set in the NdbError object owned by this operation.
  if (unlikely(rec==0 || resBuffer==0))
    return QRY_REQ_ARG_IS_NULL;
  return m_impl.setResultRowBuf(rec, resBuffer, result_mask);
}

int
NdbQueryOperation::setResultRowRef (
                       const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask)
{
  // FIXME: Errors must be set in the NdbError object owned by this operation.
  if (unlikely(rec==0))
    return QRY_REQ_ARG_IS_NULL;
  return m_impl.setResultRowRef(rec, bufRef, result_mask);
}

bool
NdbQueryOperation::isRowNULL() const
{
  return m_impl.isRowNULL();
}

bool
NdbQueryOperation::isRowChanged() const
{
  return m_impl.isRowChanged();
}


///////////////////////////////////////////
/////////  NdbQueryImpl methods ///////////
///////////////////////////////////////////

NdbQueryImpl::NdbQueryImpl(NdbTransaction& trans, 
                           const NdbQueryDefImpl& queryDef, 
                           NdbQueryImpl* next):
  m_interface(*this),
  m_error(),
  m_transaction(trans),
  m_operations(0),
  m_countOperations(0),
  m_tcKeyConfReceived(false),
  m_pendingStreams(0),
  m_next(next),
  m_queryDef(queryDef),
  m_parallelism(0),
  m_maxBatchRows(0),
  m_applStreams(),
  m_fullStreams(),
  m_isPruned(false),
  m_hashValue(0),
  m_signal(0),
  m_attrInfo(),
  m_keyInfo()
{
  // Allocate memory for all m_operations[] in a single chunk
  m_countOperations = queryDef.getNoOfOperations();
  size_t  size = m_countOperations * sizeof(NdbQueryOperationImpl);
  m_operations = static_cast<NdbQueryOperationImpl*> (malloc(size));
  assert (m_operations);

  // Then; use placement new to construct each individual 
  // NdbQueryOperationImpl object in m_operations
  for (Uint32 i=0; i<m_countOperations; ++i)
  {
    const NdbQueryOperationDefImpl& def = queryDef.getQueryOperation(i);
    new(&m_operations[i]) NdbQueryOperationImpl(*this, def);
  }

  // Serialized QueryTree definition is first part of ATTRINFO.
  m_attrInfo.append(queryDef.getSerialized());
}

NdbQueryImpl::~NdbQueryImpl()
{
  if (m_signal) {
    printf("TODO: Deallocate TC signal\n");
  }

  // NOTE: m_operations[] was allocated as a single memory chunk with
  // placement new construction of each operation.
  // Requires explicit call to d'tor of each operation before memory is free'ed.
  if (m_operations != NULL) {
    for (int i=m_countOperations-1; i>=0; --i)
    { m_operations[i].~NdbQueryOperationImpl();
    }
    free(m_operations);
  }
}

//static
NdbQueryImpl*
NdbQueryImpl::buildQuery(NdbTransaction& trans, 
                         const NdbQueryDefImpl& queryDef, 
                         NdbQueryImpl* next)
{
  return new NdbQueryImpl(trans, queryDef, next);
}


/** Assign supplied parameter values to the parameter placeholders
 *  Created when the query was defined.
 *  Values are *copied* into this NdbQueryImpl object:
 *  Memory location used as source for parameter values don't have
 *  to be valid after this assignment.
 */
int
NdbQueryImpl::assignParameters(const constVoidPtr paramValues[])
{
  /**
   * Immediately build the serialize parameter representation in order 
   * to avoid storing param values elsewhere until query is executed.
   * Also calculate prunable property, and possibly its hashValue.
   */
  // Build explicit key/filter/bounds for root operation, possibly refering paramValues
  const int error = getRoot().getQueryOperationDef()
      .prepareKeyInfo(m_keyInfo, paramValues, m_isPruned, m_hashValue);
  if (unlikely(error != 0))
    return error;

  // Serialize parameter values for the other (non-root) operations
  // (No need to serialize for root (i==0) as root key is part of keyInfo above)
  for (Uint32 i=1; i<getNoOfOperations(); ++i)
  {
    if (getQueryDef().getQueryOperation(i).getNoOfParameters() > 0)
    {
      const int error = getQueryOperation(i).serializeParams(paramValues);
      if (unlikely(error != 0))
        return error;
    }
  }

  return 0;
}

Uint32
NdbQueryImpl::getNoOfOperations() const
{
  return m_countOperations;
}

NdbQueryOperationImpl&
NdbQueryImpl::getQueryOperation(Uint32 index) const
{
  assert(index<m_countOperations);
  return m_operations[index];
}

NdbQueryOperationImpl*
NdbQueryImpl::getQueryOperation(const char* ident) const
{
  for(Uint32 i = 0; i<m_countOperations; i++){
    if(strcmp(m_operations[i].getQueryOperationDef().getName(), ident) == 0){
      return &m_operations[i];
    }
  }
  return NULL;
}

Uint32
NdbQueryImpl::getNoOfParameters() const
{
  return 0;  // FIXME
}

const NdbParamOperand*
NdbQueryImpl::getParameter(const char* name) const
{
  return NULL; // FIXME
}

const NdbParamOperand*
NdbQueryImpl::getParameter(Uint32 num) const
{
  return NULL; // FIXME
}

NdbQuery::NextResultOutcome
NdbQueryImpl::nextResult(bool fetchAllowed, bool forceSend)
{
  /* To minimize lock contention, each operation has two instances 
   * of StreamStack (which contains result streams). m_applStreams is only
   * accessed by the application thread, so it is safe to use it without 
   * locks.*/
  while(m_applStreams.top() != NULL
        && !m_applStreams.top()->m_receiver.nextResult()){
    m_applStreams.pop();
  }

  if (unlikely(m_applStreams.top()==NULL)) {
    /* m_applStreams is empty, so we cannot get more results without 
     * possibly blocking.*/
    if (fetchAllowed) {
      /* fetchMoreResults() will either copy streams that are already
       * complete (under mutex protection), or block until more data arrives.*/
      const FetchResult fetchResult = fetchMoreResults(forceSend);
      switch (fetchResult) {
      case FetchResult_otherError:
        // FIXME: copy semantics from NdbScanOperation.
        setErrorCode(Err_NodeFailCausedAbort); // Node fail
        return NdbQuery::NextResult_error;
      case FetchResult_sendFail:
        // FIXME: copy semantics from NdbScanOperation.
        setErrorCode(Err_NodeFailCausedAbort); // Node fail
        return NdbQuery::NextResult_error;
      case FetchResult_nodeFail:
        setErrorCode(Err_NodeFailCausedAbort); // Node fail
        return NdbQuery::NextResult_error;
      case FetchResult_timeOut:
        setErrorCode(Err_ReceiveFromNdbFailed); // Timeout
        return NdbQuery::NextResult_error;
      case FetchResult_ok:
        break;
      case FetchResult_scanComplete:
        return NdbQuery::NextResult_scanComplete;
      default:
        assert(false);
      }
    } else { 
      // There are no more cached records in NdbApi
      return NdbQuery::NextResult_bufferEmpty; 
    }
  }

  /* Make results from root operation available to the user.*/
  NdbQueryOperationImpl& root = getRoot();
  const char* const rootBuff = m_applStreams.top()->m_receiver.get_row();
  assert(rootBuff!=NULL);
  if (root.m_resultStyle==NdbQueryOperationImpl::Style_NdbRecAttr) {
    root.fetchRecAttrResults(m_applStreams.top()->m_streamNo);
  } else if (root.m_resultStyle==NdbQueryOperationImpl::Style_NdbRecord) {
    if (root.m_resultRef!=NULL) {
      // Set application pointer to point into internal buffer.
      *root.m_resultRef = rootBuff;
    } else { 
      // Copy result to buffer supplied by application.
      memcpy(root.m_resultBuffer, rootBuff, 
             m_applStreams.top()->m_receiver.m_record.m_ndb_record
             ->m_row_size);
    }
  }
  /* Make results from non-root operation availabel to the user.*/
  if (getQueryDef().isScanQuery()) {
    const Uint32 rowNo 
      = m_applStreams.top()->m_receiver.getCurrentRow() - 1;
    for (Uint32 i = 0; i<root.getNoOfChildOperations(); i++) {
      /* For each child, fetch the right row.*/
      root.getChildOperation(i)
        .updateChildResult(m_applStreams.top()->m_streamNo, 
                           m_applStreams.top()->getChildTupleIdx(i,rowNo));
    }
  } else {
    /* Fetch results for all non-root lookups also.*/
    for (Uint32 i = 1; i<getNoOfOperations(); i++) {
      NdbQueryOperationImpl& operation = getQueryOperation(i);
      assert(operation.m_resultStreams[0]->m_transidAICount<=1);

      operation.m_isRowNull = (operation.m_resultStreams[0]->m_transidAICount==0);
      // Check if there was a result for this operation.
      if (operation.m_isRowNull==false) {
        const char* buff = operation.m_resultStreams[0]->m_receiver.get_row();

        if (operation.m_resultStyle==NdbQueryOperationImpl::Style_NdbRecAttr) {
          operation.fetchRecAttrResults(0);
        } else if (operation.m_resultStyle==NdbQueryOperationImpl::Style_NdbRecord) {
          if(operation.m_resultRef!=NULL){
            // Set application pointer to point into internal buffer.
            *operation.m_resultRef = buff;
          }else{
            // Copy result to buffer supplied by application.
            memcpy(operation.m_resultBuffer, buff, 
                   operation.m_resultStreams[0]
                   ->m_receiver.m_record.m_ndb_record
                   ->m_row_size);
          }
        }
      } else {
        // This operation gave no results.
        if (operation.m_resultRef!=NULL) {
          // Set application pointer to NULL.
          *operation.m_resultRef = NULL;
        }
      }
    }
  }
  return NdbQuery::NextResult_gotRow;
} //NdbQueryImpl::nextResult


NdbQueryImpl::FetchResult
NdbQueryImpl::fetchMoreResults(bool forceSend){
  assert(!forceSend); // FIXME
  assert(m_applStreams.top() == NULL);

  /* Check if there are any more completed streams available.*/
  if(m_queryDef.isScanQuery()){
    Ndb* const ndb = getNdbTransaction()->getNdb();
    
    TransporterFacade* const facade 
      = ndb->theImpl->m_transporter_facade;
    /* This part needs to be done under mutex due to synchronization with 
     * receiver thread. */
    PollGuard poll_guard(facade,
                         &ndb->theImpl->theWaiter,
                         ndb->theNdbBlockNumber);
    while(true){
      /* m_fullStreams contains any streams that are complete (for this batch)
       * but have not yet been moved (under mutex protection) to 
       * m_applStreams.*/
      if(m_fullStreams.top()==NULL){
        if(getRoot().isBatchComplete()){
          /* FIXME: Add code to ask for the next batch if necessary.*/
          const bool scanComplete = true;
          if(scanComplete){
            /* FIXME: Close scans properly. This would involve sending
             * SCAN_NEXTREQ*/
            return FetchResult_scanComplete;
          }else{
            // FIXME: Ask for new scan batch.
          }
        }
        /* More results are on the way, so we wait for them.*/
        const FetchResult waitResult = static_cast<FetchResult>
          (poll_guard.wait_scan(3*facade->m_waitfor_timeout, 
                                getNdbTransaction()->getConnectedNodeId(), 
                                forceSend));
        if(waitResult != FetchResult_ok){
          return waitResult;
        }
      }
      /* Move full streams from receiver thread's container to application 
       *  thread's container.*/
      while(m_fullStreams.top()!=NULL){
        m_applStreams.push(*m_fullStreams.top());
        m_fullStreams.pop();
      }
      // Iterate over the new streams until we find one that contains data.
      while(m_applStreams.top() != NULL
            && !m_applStreams.top()->m_receiver.nextResult()){
        m_applStreams.pop();
      }
      if(m_applStreams.top() != NULL){
        return FetchResult_ok;
      }
    } // while(true)
  } else {
    /* The root operation is a lookup. Lookups are guaranteed to be complete
     * before NdbTransaction::execute() returns. Therefore we do not set
     * the lock, because we know that the signal receiver thread will not
     * be accessing  m_fullStreams at this time.*/
    if(m_fullStreams.top()==NULL){
      /* Getting here means that the application called nextResult() twice
       * for a lookup query.*/
      return FetchResult_scanComplete;
    }else{
      /* Move stream from receiver thread's container to application 
       *  thread's container.*/
      m_applStreams.push(*m_fullStreams.top());
      m_fullStreams.pop();
      assert(m_fullStreams.top()==NULL); // Only one stream for lookups.
      // Check if there is a result row.
      if(m_applStreams.top()->m_receiver.nextResult()){
        return FetchResult_ok;
      }else{
        return FetchResult_scanComplete;
      }
    }
  }
} //NdbQueryImpl::fetchMoreResults

void 
NdbQueryImpl::buildChildTupleLinks(Uint32 streamNo)
{
  assert(getRoot().m_resultStreams[streamNo]->isBatchComplete());
  for (Uint32 i = 0; i<getNoOfOperations(); i++) {
    m_operations[i].buildChildTupleLinks(streamNo);
  }
}
  
void 
NdbQueryImpl::closeSingletonScans()
{
  assert(!getQueryDef().isScanQuery());
  for(Uint32 i = 0; i<getNoOfOperations(); i++){
    NdbQueryOperationImpl& operation = getQueryOperation(i);
    for(Uint32 streamNo = 0; streamNo < getParallelism(); streamNo++){
      NdbResultStream& resultStream = *operation.m_resultStreams[streamNo];
      /** Now we have received all tuples for all operations. We can thus call
       *  execSCANOPCONF() with the right row count.
       */
      resultStream.m_receiver.execSCANOPCONF(RNIL, 0, 
                                             resultStream.m_transidAICount);
    } 
  }
  /* nextResult() will later move it from m_fullStreams to m_applStreams
   * undex mutex protection.
   */
  m_fullStreams.push(*getRoot().m_resultStreams[0]);
} //NdbQueryImpl::closeSingletonScans

void
NdbQueryImpl::close(bool forceSend, bool release)
{
  // FIXME
}

NdbTransaction*
NdbQueryImpl::getNdbTransaction() const
{
  return &m_transaction;
}

void
NdbQueryImpl::setErrorCodeAbort(int aErrorCode){
  m_error.code = aErrorCode;
  getNdbTransaction()->theErrorLine = 0;
  getNdbTransaction()->theErrorOperation = NULL;
  getNdbTransaction()->setOperationErrorCodeAbort(aErrorCode);
}

bool 
NdbQueryImpl::execTCKEYCONF(){
  ndbout << "NdbQueryImpl::execTCKEYCONF()  m_pendingStreams=" 
         << m_pendingStreams << endl;
  assert(!getQueryDef().isScanQuery());
  m_tcKeyConfReceived = true;
  if(m_pendingStreams==0){
    for(Uint32 i = 0; i < getNoOfOperations(); i++){
      assert(getQueryOperation(i).isBatchComplete());
    }
  }
  if(m_pendingStreams==0){
    closeSingletonScans();
    return true;
  }else{
    return false;
  }
}

bool 
NdbQueryImpl::incPendingStreams(int increment){
  m_pendingStreams += increment;
  if(m_pendingStreams==0 && m_tcKeyConfReceived){
    for(Uint32 i = 0; i < getNoOfOperations(); i++){
      assert(getQueryOperation(i).isBatchComplete());
    }
  }
  if(m_pendingStreams==0 && m_tcKeyConfReceived){
    if(!getQueryDef().isScanQuery()){
      closeSingletonScans();
    }
    return true;
  }else{
    return false;
  }
}

int
NdbQueryImpl::prepareSend()
{
  assert (m_pendingStreams==0);

  // Determine execution parameters 'parallelism' and 'batch size'.
  // May be user specified (TODO), and/or,  limited/specified by config values
  //
  if (getQueryDef().isScanQuery())
  {
    m_pendingStreams = getRoot().getQueryOperationDef().getTable().getFragmentCount();
    m_tcKeyConfReceived = true;

    // Parallelism may be user specified, else(==0) use default
    if (m_parallelism == 0 || m_parallelism > m_pendingStreams) {
      m_parallelism = m_pendingStreams;
    }

    Ndb* const ndb = getNdbTransaction()->getNdb();
    TransporterFacade *tp = ndb->theImpl->m_transporter_facade;

    Uint32 batchRows = m_maxBatchRows; // >0: User specified prefered value, ==0: Use default CFG values

    // Calculate batchsize for query as minimum batchRows for all m_operations[].
    // Ignore calculated 'batchByteSize' and 'firstBatchRows' here - Recalculated
    // when building signal after max-batchRows has been determined.
    for (Uint32 i = 0; i < m_countOperations; i++) {
      Uint32 batchByteSize, firstBatchRows;
      NdbReceiver::calculate_batch_size(tp,
                                    m_operations[i].m_ndbRecord,
                                    m_operations[i].m_firstRecAttr,
                                    0, // Key size.
                                    m_parallelism,
                                    batchRows,
                                    batchByteSize,
                                    firstBatchRows);
      assert (batchRows>0);
      assert (firstBatchRows==batchRows);
    }
    m_maxBatchRows = batchRows;
  }
  else
  {
    m_pendingStreams = m_parallelism = 1;
    /* We will always receive a TCKEYCONF signal, even if the root operation
     * yields no result.*/
    m_tcKeyConfReceived = false;
    m_maxBatchRows = 1;
  }

  // 1. Build receiver structures for each QueryOperation.
  // 2. Fill in parameters (into ATTRINFO) for QueryTree.
  //    (Has to complete *after* ::prepareReceiver() as QueryTree params
  //     refer receiver id's.)
  //
  for (Uint32 i = 0; i < m_countOperations; i++) {
    int error;
    if (unlikely((error = m_operations[i].prepareReceiver()) != 0)
              || (error = m_operations[i].prepareAttrInfo(m_attrInfo)) != 0) {
      setErrorCodeAbort(error);
      return -1;
    }
  }

  if (unlikely(m_attrInfo.isMemoryExhausted() || m_keyInfo.isMemoryExhausted())) {
    setErrorCodeAbort(Err_MemoryAlloc);
    return -1;
  }

  if (unlikely(m_attrInfo.getSize() > ScanTabReq::MaxTotalAttrInfo  ||
               m_keyInfo.getSize()  > ScanTabReq::MaxTotalAttrInfo)) {
    setErrorCodeAbort(4257); // TODO: find a more suitable errorcode, 
    return -1;
  }

  // Setup m_applStreams and m_fullStreams for receiving results
  int error;
  if (unlikely((error = m_applStreams.prepare(m_parallelism)) != 0)
            || (error = m_fullStreams.prepare(m_parallelism)) != 0) {
    setErrorCodeAbort(error);
    return -1;
  }

#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized ATTRINFO : ";
  for(Uint32 i = 0; i < m_attrInfo.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", m_attrInfo.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryImpl::prepareSend


/******************************************************************************
int doSend()

Return Value:   Return >0 : send was succesful, returns number of signals sent
                Return -1: In all other case.   
Parameters:     nodeId: Receiving processor node
Remark:         Send a TCKEYREQ or SCAN_TABREQ (long) signal depending of 
                the query being either a lookup or scan type. 
                KEYINFO and ATTRINFO are included as part of the long signal
******************************************************************************/
int
NdbQueryImpl::doSend(int nodeId, bool lastFlag)
{
  Ndb& ndb = *m_transaction.getNdb();
  TransporterFacade *tp = ndb.theImpl->m_transporter_facade;

  // TODO: move signal allocation and building outside mutex protected ::doSend()
  m_signal = ndb.getSignal();
  if (m_signal == NULL) {
    setErrorCodeAbort(Err_MemoryAlloc);  // Allocation failure
    return -1; 
  }

  const NdbQueryOperationDefImpl& rootDef = getRoot().getQueryOperationDef();
  const NdbTableImpl* const rootTable = rootDef.getIndex()
    ? rootDef.getIndex()->getIndexTable()
    : &rootDef.getTable();

  Uint32 tTableId = rootTable->m_id;
  Uint32 tSchemaVersion = rootTable->m_version;
  Uint64 transId = m_transaction.getTransactionId();

  if (rootDef.isScanOperation())
  {
    Uint32 scan_flags = 0;  // TODO: Specify with ScanOptions::SO_SCANFLAGS
    Uint32 parallel = m_parallelism;

    bool tupScan = (scan_flags & NdbScanOperation::SF_TupScan);
    bool rangeScan= false;

    /* Handle IndexScan specifics */
    if ( (int) rootTable->m_indexType ==
         (int) NdbDictionary::Index::OrderedIndex )
    {
      rangeScan = true;
      tupScan = false;
    }

    assert (m_parallelism > 0);
    assert (m_maxBatchRows > 0);

    m_signal->setSignal(GSN_SCAN_TABREQ);

    ScanTabReq * const scanTabReq = CAST_PTR(ScanTabReq, m_signal->getDataPtrSend());
    Uint32 reqInfo = 0;

    scanTabReq->apiConnectPtr = m_transaction.theTCConPtr;
    scanTabReq->spare = 0;  // Unused in later protocoll versions
    scanTabReq->tableId = tTableId;
    scanTabReq->tableSchemaVersion = tSchemaVersion;
    scanTabReq->storedProcId = 0xFFFF;
    scanTabReq->transId1 = (Uint32) transId;
    scanTabReq->transId2 = (Uint32) (transId >> 32);
    scanTabReq->buddyConPtr = m_transaction.theBuddyConPtr;

    Uint32 batchRows = m_maxBatchRows;
    Uint32 batchByteSize, firstBatchRows;
    NdbReceiver::calculate_batch_size(tp,
                                      getRoot().m_ndbRecord,
                                      getRoot().m_firstRecAttr,
                                      0, // Key size.
                                      m_parallelism,
                                      batchRows,
                                      batchByteSize,
                                      firstBatchRows);
    assert (batchRows==m_maxBatchRows);
    ScanTabReq::setScanBatch(reqInfo, batchRows);
    scanTabReq->batch_byte_size = batchByteSize;
    scanTabReq->first_batch_size = firstBatchRows;

    ScanTabReq::setViaSPJFlag(reqInfo, 1);
    ScanTabReq::setParallelism(reqInfo, parallel);
    ScanTabReq::setRangeScanFlag(reqInfo, rangeScan);
    ScanTabReq::setTupScanFlag(reqInfo, tupScan);

    // Assume LockMode LM_ReadCommited, set related lock flags
    ScanTabReq::setLockMode(reqInfo, false);  // not exclusive
    ScanTabReq::setHoldLockFlag(reqInfo, false);
    ScanTabReq::setReadCommittedFlag(reqInfo, true);

//  m_keyInfo = (scan_flags & NdbScanOperation::SF_KeyInfo) ? 1 : 0;

    // If scan is pruned, use optional 'distributionKey' to hold hashvalue
    if (m_isPruned)
    {
//    printf("Build pruned SCANREQ, w/ hashValue:%d\n", m_hashValue);
      ScanTabReq::setDistributionKeyFlag(reqInfo, 1);
      scanTabReq->distributionKey= m_hashValue;
      m_signal->setLength(ScanTabReq::StaticLength + 1);
    } else {
      m_signal->setLength(ScanTabReq::StaticLength);
    }
    scanTabReq->requestInfo = reqInfo;

    /**
     * Then send the signal:
     *
     * SCANTABREQ always has 2 mandatory sections and an optional
     * third section
     * Section 0 : List of receiver Ids NDBAPI has allocated 
     *             for the scan
     * Section 1 : ATTRINFO section
     * Section 2 : Optional KEYINFO section
     */
    GenericSectionPtr secs[3];
    Uint32 prepared_receivers[64];  // TODO: 64 is a temp hack
 
    const NdbQueryOperationImpl& queryOp = getRoot();
    for(Uint32 i = 0; i<m_parallelism; i++){
      prepared_receivers[i] = queryOp.getReceiver(i).getId();
    }

    LinearSectionIterator receiverIdIterator(prepared_receivers,
                                             m_parallelism);

    secs[0].sectionIter= &receiverIdIterator;
    secs[0].sz= m_parallelism;

    LinearSectionIterator attrInfoIter(m_attrInfo.addr(),m_attrInfo.getSize());
    secs[1].sectionIter= &attrInfoIter;
    secs[1].sz= m_attrInfo.getSize();

    Uint32 numSections= 2;

    LinearSectionIterator keyInfoIter(m_keyInfo.addr(), m_keyInfo.getSize());
    if (m_keyInfo.getSize() > 0)
    {
      secs[2].sectionIter= &keyInfoIter;
      secs[2].sz= m_keyInfo.getSize();
      numSections= 3;
    }
  
    /* Send Fragmented as SCAN_TABREQ can be large */
    const int res = tp->sendFragmentedSignal(m_signal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCodeAbort(Err_SendFailed);  // Error: 'Send to NDB failed'
      return -1;
    }

  } else {

    m_signal->setSignal(GSN_TCKEYREQ);  // TODO: or GSN_TCINDXREQ

    TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, m_signal->getDataPtrSend());

    tcKeyReq->apiConnectPtr   = m_transaction.theTCConPtr;
    tcKeyReq->apiOperationPtr = getRoot().getIdOfReceiver();
    tcKeyReq->tableId = tTableId;
    tcKeyReq->tableSchemaVersion = tSchemaVersion;
    tcKeyReq->transId1 = (Uint32) transId;
    tcKeyReq->transId2 = (Uint32) (transId >> 32);

    Uint32 attrLen = 0;
    tcKeyReq->setAttrinfoLen(attrLen, 0); // Not required for long signals.
    tcKeyReq->setAPIVersion(attrLen, NDB_VERSION);
    tcKeyReq->attrLen = attrLen;

    Uint32 reqInfo = 0;
    TcKeyReq::setOperationType(reqInfo, NdbOperation::ReadRequest);
    TcKeyReq::setViaSPJFlag(reqInfo, true);
    TcKeyReq::setKeyLength(reqInfo, 0);            // This is a long signal
    TcKeyReq::setAIInTcKeyReq(reqInfo, 0);         // Not needed
    TcKeyReq::setInterpretedFlag(reqInfo, false);  // Encoded in QueryTree

    // TODO: Set these flags less forcefully
    TcKeyReq::setStartFlag(reqInfo, true);         // TODO, must implememt
    TcKeyReq::setExecuteFlag(reqInfo, true);       // TODO, must implement
    TcKeyReq::setNoDiskFlag(reqInfo, true);
    TcKeyReq::setAbortOption(reqInfo, NdbOperation::AO_IgnoreError);

    TcKeyReq::setDirtyFlag(reqInfo, true);
    TcKeyReq::setSimpleFlag(reqInfo, true);
    tcKeyReq->requestInfo = reqInfo;

    m_signal->setLength(TcKeyReq::StaticLength);

/****
    // Unused optional part located after TcKeyReq::StaticLength
    tcKeyReq->scanInfo = 0;
    tcKeyReq->distrGroupHashValue = 0;
    tcKeyReq->distributionKeySize = 0;
    tcKeyReq->storedProcId = 0xFFFF;
***/

/**** TODO ... maybe - from NdbOperation::prepareSendNdbRecord(AbortOption ao)
    Uint8 abortOption= (ao == DefaultAbortOption) ?
      (Uint8) m_abortOption : (Uint8) ao;
  
    m_abortOption= theSimpleIndicator && theOperationType==ReadRequest ?
      (Uint8) AO_IgnoreError : (Uint8) abortOption;

    TcKeyReq::setAbortOption(reqInfo, m_abortOption);
    TcKeyReq::setCommitFlag(tcKeyReq->requestInfo, theCommitIndicator);
*****/

    LinearSectionIterator keyInfoIter (m_keyInfo.addr(), m_keyInfo.getSize());
    LinearSectionIterator attrInfoIter(m_attrInfo.addr(),m_attrInfo.getSize());

    GenericSectionPtr secs[2];
    secs[TcKeyReq::KeyInfoSectionNum].sectionIter= &keyInfoIter;
    secs[TcKeyReq::KeyInfoSectionNum].sz= m_keyInfo.getSize();
    Uint32 numSections= 1;

    if (m_attrInfo.getSize() > 0)
    {
      secs[TcKeyReq::AttrInfoSectionNum].sectionIter= &attrInfoIter;
      secs[TcKeyReq::AttrInfoSectionNum].sz= m_attrInfo.getSize();
      numSections= 2;
    }

    const int res = tp->sendSignal(m_signal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCodeAbort(Err_SendFailed);  // Error: 'Send to NDB failed'
      return -1;
    }
  }

  /* Todo : Consider calling NdbOperation::postExecuteRelease()
   * Ideally it should be called outside TP mutex, so not added
   * here yet
   */
  m_transaction.OpSent();

  // TODO: Move outside mutex
  ndb.releaseSignal(m_signal);
  m_signal = NULL;

  return 1;
} // NdbQueryImpl::doSend()


NdbQueryImpl::StreamStack::StreamStack():
  m_size(0),
  m_current(-1),
  m_array(NULL){
}

int
NdbQueryImpl::StreamStack::prepare(int size)
{
  assert(m_array==NULL);
  assert(m_size==0);
  if (size > 0) 
  { m_size = size;
    m_array = new NdbResultStream*[size];
    if (unlikely(m_array==NULL))
      return Err_MemoryAlloc;
  }
  return 0;
}

void
NdbQueryImpl::StreamStack::push(NdbResultStream& stream){
  m_current++;
  assert(m_current<m_size);
  m_array[m_current] = &stream; 
}


////////////////////////////////////////////////
/////////  NdbResultStream methods ///////////
////////////////////////////////////////////////

void 
NdbResultStream::TupleIdMap::put(Uint16 id, Uint32 num){
  const Pair p = {id, num};
  m_vector.push_back(p);
}

Uint32 
NdbResultStream::TupleIdMap::get(Uint16 id) const {
  for(Uint32 i=0; i<m_vector.size(); i++){
    if(m_vector[i].m_id == id){
      return m_vector[i].m_num;
    }
  }
  return tupleNotFound;
}

NdbResultStream::NdbResultStream(NdbQueryOperationImpl& operation, Uint32 streamNo):
  m_streamNo(streamNo),
  m_receiver(operation.getQuery().getNdbTransaction()->getNdb(), &operation),  // FIXME? Use Ndb recycle lists
  m_transidAICount(0),
  m_correlToTupNumMap(),
  m_pendingResults(0),
  m_pendingScanTabConf(false),
  m_operation(operation),
  m_parentTupleCorr(NULL),
  m_childTupleIdx(NULL)
{};

NdbResultStream::~NdbResultStream() { 
  delete[] m_childTupleIdx; 
  delete[] m_parentTupleCorr; 
}

int  // Return 0 if ok, else errorcode
NdbResultStream::prepare()
{
  /* Parrent / child correlation is only relevant for scan type queries
   * Don't create m_parentTupleCorr[] and m_childTupleIdx[] for lookups!
   * Neither is these structures required for operations not having respective
   * child or parent operations.
   */
  if (m_operation.getQueryDef().isScanQuery()) {

    // Root scan-operation need a ScanTabConf to complete
    m_pendingScanTabConf = (&m_operation.getRoot()==&m_operation);

    const size_t batchRows = m_operation.getQuery().getMaxBatchRows();
    if (m_operation.getNoOfParentOperations()>0) {
      assert (m_operation.getNoOfParentOperations()==1);
      m_parentTupleCorr = new Uint32[batchRows];
      if (unlikely(m_parentTupleCorr==NULL))
        return Err_MemoryAlloc;
    }

    if (m_operation.getNoOfChildOperations()>0) {
      m_childTupleIdx = new Uint32[batchRows * m_operation.getNoOfChildOperations()];
      if (unlikely(m_childTupleIdx==NULL))
        return Err_MemoryAlloc;

      for (unsigned i=0; 
           i<batchRows * m_operation.getNoOfChildOperations(); 
           i++) {
        m_childTupleIdx[i] = tupleNotFound;
      }
    }
  } else {  // Lookup query
    // Root lookup operation need a CONF or REF reply to complete
    m_pendingResults = (&m_operation.getRoot()==&m_operation);
  }
  return 0;
}

void 
NdbResultStream::setParentTupleCorr(Uint32 rowNo, Uint32 correlationNum) const {
  if (m_parentTupleCorr) {
    m_parentTupleCorr[rowNo] = correlationNum;
  } else {
    assert (m_operation.getNoOfParentOperations()==0 || !m_operation.getQueryDef().isScanQuery());
  }
}

void
NdbResultStream::setChildTupleIdx(Uint32 childNo, Uint32 tupleNo, Uint32 index)
{
  assert (tupleNo < m_operation.getQuery().getMaxBatchRows());
  unsigned ix = (tupleNo*m_operation.getNoOfChildOperations()) + childNo;
  m_childTupleIdx[ix] = index;
}

Uint32
NdbResultStream::getChildTupleIdx(Uint32 childNo, Uint32 tupleNo) const
{
  assert (tupleNo < m_operation.getQuery().getMaxBatchRows());
  unsigned ix = (tupleNo*m_operation.getNoOfChildOperations()) + childNo;
  return m_childTupleIdx[ix];
}

////////////////////////////////////////////////////
/////////  NdbQueryOperationImpl methods ///////////
////////////////////////////////////////////////////

NdbQueryOperationImpl::NdbQueryOperationImpl(
           NdbQueryImpl& queryImpl,
           const NdbQueryOperationDefImpl& def):
  m_interface(*this),
  m_magic(MAGIC),
  m_queryImpl(queryImpl),
  m_operationDef(def),
  m_parents(def.getNoOfParentOperations()),
  m_children(def.getNoOfChildOperations()),
  m_resultStreams(NULL),
  m_params(),
  m_userProjection(def.getTable()),
  m_resultStyle(Style_None),
  m_batchBuffer(NULL),
  m_resultBuffer(NULL),
  m_resultRef(NULL),
  m_isRowNull(false),
  m_ndbRecord(NULL),
  m_firstRecAttr(NULL),
  m_lastRecAttr(NULL)
{ 
  // Fill in operations parent refs, and append it as child of its parents
  for (Uint32 p=0; p<def.getNoOfParentOperations(); ++p)
  { 
    const NdbQueryOperationDefImpl& parent = def.getParentOperation(p);
    const Uint32 ix = parent.getQueryOperationIx();
    assert (ix < m_queryImpl.getNoOfOperations());
    m_parents.push_back(&m_queryImpl.getQueryOperation(ix));
    m_queryImpl.getQueryOperation(ix).m_children.push_back(this);
  }
}

NdbQueryOperationImpl::~NdbQueryOperationImpl(){
  Ndb* const ndb = m_queryImpl.getNdbTransaction()->getNdb();
  // Check against buffer overun.
#ifndef NDEBUG // Buffer overrun check activated.
  assert(m_batchBuffer==NULL ||
         (m_batchBuffer[m_batchByteSize*getQuery().getParallelism()] 
          == 'a' &&
          m_batchBuffer[m_batchByteSize*getQuery().getParallelism()+1] 
          == 'b' &&
          m_batchBuffer[m_batchByteSize*getQuery().getParallelism()+2] 
          == 'c' &&
          m_batchBuffer[m_batchByteSize*getQuery().getParallelism()+3] 
          == 'd'));
#endif
  delete[] m_batchBuffer;
  if (m_resultStreams) {
    for(Uint32 i = 0; i<getQuery().getParallelism(); i ++){
      delete m_resultStreams[i];
    }
    delete[] m_resultStreams;
  }

  NdbRecAttr* recAttr;
  while(recAttr!=NULL){
    ndb->releaseRecAttr(recAttr);
    recAttr = recAttr->next();
  }
}


Uint32
NdbQueryOperationImpl::getNoOfParentOperations() const
{
  return m_parents.size();
}

NdbQueryOperationImpl&
NdbQueryOperationImpl::getParentOperation(Uint32 i) const
{
  return *m_parents[i];
}

Uint32 
NdbQueryOperationImpl::getNoOfChildOperations() const
{
  return m_children.size();
}

NdbQueryOperationImpl&
NdbQueryOperationImpl::getChildOperation(Uint32 i) const
{
  return *m_children[i];
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const char* anAttrName,
                            char* resultBuffer)
{
  const NdbDictionary::Column* const column 
    = m_operationDef.getTable().getColumn(anAttrName);
  if(unlikely(column==NULL)){
    getQuery().setErrorCodeAbort(Err_UnknownColumn);
    return NULL;
  } else {
    return getValue(column, resultBuffer);
  }
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            Uint32 anAttrId, 
                            char* resultBuffer)
{
  const NdbDictionary::Column* const column 
    = m_operationDef.getTable().getColumn(anAttrId);
  if(unlikely(column==NULL)){
    getQuery().setErrorCodeAbort(Err_UnknownColumn);
    return NULL;
  } else {
    return getValue(column, resultBuffer);
  }
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const NdbDictionary::Column* column, 
                            char* resultBuffer)
{
  if(unlikely(m_resultStyle == Style_NdbRecord)){
    getQuery().setErrorCode(Err_MixRecAttrAndRecord);
    return NULL;
  }
  m_resultStyle = Style_NdbRecAttr;
  const int addResult = m_userProjection.addColumn(*column);
  if(unlikely(addResult !=0)){
    getQuery().setErrorCode(addResult);
    return NULL;
  }
  Ndb* const ndb = getQuery().getNdbTransaction()->getNdb();
  NdbRecAttr* const recAttr = ndb->getRecAttr();
  if(unlikely(recAttr == NULL)){
    getQuery().setErrorCodeAbort(Err_MemoryAlloc);
  }
  if(unlikely(recAttr->setup(column, resultBuffer))){
    ndb->releaseRecAttr(recAttr);
    getQuery().setErrorCodeAbort(Err_MemoryAlloc);
    return NULL;
  }
  // Append to tail of list.
  if(m_firstRecAttr == NULL){
    m_firstRecAttr = recAttr;
  }else{
    m_lastRecAttr->next(recAttr);
  }
  m_lastRecAttr = recAttr;
  assert(recAttr->next()==NULL);
  return recAttr;
}

static bool isSetInMask(const unsigned char* mask, int bitNo){
  return mask[bitNo>>3] & 1<<(bitNo&7);
}

int
NdbQueryOperationImpl::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  if (rec->tableId != 
      static_cast<Uint32>(m_operationDef.getTable().getTableId())){
    /* The key_record and attribute_record in primary key operation do not 
       belong to the same table.*/
    getQuery().setErrorCode(Err_DifferentTabForKeyRecAndAttrRec);
    return -1;
  }
  if(unlikely(m_resultStyle==Style_NdbRecAttr)){
    /* Cannot mix NdbRecAttr and NdbRecord methods in one operation. */
    getQuery().setErrorCode(Err_MixRecAttrAndRecord);
    return -1;
  }else if(unlikely(m_resultStyle==Style_NdbRecord)){
    getQuery().setErrorCode(QRY_RESULT_ROW_ALREADY_DEFINED);
    return -1;
  }
  m_ndbRecord = rec;
  m_resultStyle = Style_NdbRecord;
  m_resultBuffer = resBuffer;
  assert(m_batchBuffer==NULL);
    for(Uint32 i = 0; i<rec->noOfColumns; i++){
    if(likely(result_mask==NULL || isSetInMask(result_mask, i))){
      m_userProjection.addColumn(*m_operationDef.getTable()
                                 .getColumn(rec->columns[i].column_no));
    }
  }
  return 0;
}

int
NdbQueryOperationImpl::setResultRowRef (
                       const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask)
{
  m_resultRef = &bufRef;
  return setResultRowBuf(rec, NULL, result_mask);
}

void
NdbQueryOperationImpl::fetchRecAttrResults(Uint32 streamNo){
  NdbRecAttr* recAttr = m_firstRecAttr;
  Uint32 posInRow = 0;
  while(recAttr != NULL){
    const char *attrData = NULL;
    Uint32 attrSize = 0;
    const int retVal1 = m_resultStreams[streamNo]->m_receiver
      .getScanAttrData(attrData, attrSize, posInRow);
    assert(retVal1==0);
    assert(attrData!=NULL);
    const bool retVal2 = recAttr
      ->receive_data(reinterpret_cast<const Uint32*>(attrData), attrSize);
    assert(retVal2);
    recAttr = recAttr->next();
  }
}

void 
NdbQueryOperationImpl::updateChildResult(Uint32 streamNo, Uint32 rowNo){
  if (rowNo==tupleNotFound) {
    /* This operation gave no result for the current parent tuple.*/ 
    m_isRowNull = true;
    if(m_resultRef!=NULL){
      // Set the pointer supplied by the application to NULL.
      *m_resultRef = NULL;
    }
    /* We should not give any results for the descendants either.*/
    for(Uint32 i = 0; i<getNoOfChildOperations(); i++){
      getChildOperation(i).updateChildResult(0, tupleNotFound);
    }
  }else{
    /* Pick the proper row for a lookup that is a descentdant of the scan.
     * We iterate linearly over the results of the root scan operation, but
     * for the descendant we must use the m_childTupleIdx index to pick the
     * tuple that corresponds to the current parent tuple.*/
    m_isRowNull = false;
    NdbResultStream& resultStream = *m_resultStreams[streamNo];
    assert(rowNo < resultStream.m_receiver.m_result_rows);
    /* Use random rather than sequential access on receiver, since we
    * iterate over results using an indexed structure.*/
    resultStream.m_receiver.setCurrentRow(rowNo);
    const char* buff = resultStream.m_receiver.get_row();
    assert(buff!=NULL);
    if(m_resultStyle==Style_NdbRecAttr){
      fetchRecAttrResults(streamNo);
    }else if(m_resultStyle==Style_NdbRecord){
      if(m_resultRef!=NULL){
        // Set application pointer to point into internal buffer.
        *m_resultRef = buff;
      }else{
        assert(m_resultBuffer!=NULL);
        // Copy result to buffer supplied by application.
        memcpy(m_resultBuffer, buff, 
               resultStream.m_receiver.m_record.m_ndb_record->m_row_size);
      }
    }
    /* Call recursively for the children of this operation.*/
    for(Uint32 i = 0; i<getNoOfChildOperations(); i++){
      getChildOperation(i).updateChildResult(streamNo, 
                                             resultStream
                                               .getChildTupleIdx(i, rowNo));
    }
  }
}

bool
NdbQueryOperationImpl::isRowNULL() const
{
  return m_isRowNull;
}

bool
NdbQueryOperationImpl::isRowChanged() const
{
  // Will be true until scan linked with scan is implemented.
  return true;
}

// Constructor.
NdbQueryOperationImpl::UserProjection
::UserProjection(const NdbDictionary::Table& tab):
  m_columnCount(0),
  m_noOfColsInTable(tab.getNoOfColumns()),
  m_mask(),
  m_isOrdered(true),
  m_maxColNo(-1){
  assert(m_noOfColsInTable<=MAX_ATTRIBUTES_IN_TABLE);
}

int
NdbQueryOperationImpl::UserProjection
::addColumn(const NdbDictionary::Column& col){
  const int colNo = col.getColumnNo();
  assert(colNo<m_noOfColsInTable);
  if(unlikely(m_mask.get(colNo))){
    return QRY_DUPLICATE_COLUMN_IN_PROJ;
  }

  if(colNo<=m_maxColNo){
    m_isOrdered = false;
  }
  m_maxColNo = MAX(colNo, m_maxColNo);
  m_columns[m_columnCount++] = &col;
  assert(m_columnCount<=MAX_ATTRIBUTES_IN_TABLE);
  m_mask.set(colNo);
  return 0;
}

int
NdbQueryOperationImpl::UserProjection::serialize(Uint32Buffer& buffer,
                                                 ResultStyle resultStyle, 
                                                 bool withCorrelation) const{
  /**
   * If the columns in the projections are ordered according to ascending
   * column number, we can pack the projection more compactly.
   */
  size_t startPos = buffer.getSize();
  buffer.append(0U);  // Temp write firste 'length' word, update later
  switch(resultStyle){
  case Style_NdbRecord:
    assert(m_isOrdered);
    // Special case: get all columns.
    if(m_columnCount==m_noOfColsInTable){
      Uint32 ah;
      AttributeHeader::init(&ah, AttributeHeader::READ_ALL, m_columnCount);
      buffer.append(ah);
    }else{
      /* Serialize projection as a bitmap.*/
      const Uint32 wordCount = 1+m_maxColNo/32; // Size of mask.
      Uint32* dst = buffer.alloc(wordCount+1);
      AttributeHeader::init(dst, 
                            AttributeHeader::READ_PACKED, 4*wordCount);
      memcpy(dst+1, &m_mask, 4*wordCount);
    }
    break;
  case Style_NdbRecAttr: {
    /* Serialize projection as a list of column numbers.*/
    Uint32* dst = buffer.alloc(m_columnCount);
    for(int i = 0; i<m_columnCount; i++){
      AttributeHeader::init(dst+i,
                            m_columns[i]->getColumnNo(),
                            0);
    }
    break;
  }
  case Style_None:
    assert(m_columnCount==0);
    break;
  default:
    assert(false);
  }
  if(withCorrelation){
    Uint32 ah;
    AttributeHeader::init(&ah, AttributeHeader::READ_ANY_VALUE, 0);
    buffer.append(ah);
  }
  // Size of projection in words.
  size_t length = buffer.getSize() - startPos - 1 ;
  buffer.put(startPos, length);
  return 0;
}

int NdbQueryOperationImpl::serializeParams(const constVoidPtr paramValues[])
{
  if (paramValues == NULL)
  {
    return QRY_NEED_PARAMETER;
  }

  const NdbQueryOperationDefImpl& def = getQueryOperationDef();
  for (Uint32 i=0; i<def.getNoOfParameters(); i++)
  {
    const NdbParamOperandImpl& paramDef = def.getParameter(i);
    const constVoidPtr paramValue = paramValues[paramDef.getParamIx()];
    if (paramValue == NULL)  // FIXME: May also indicate a NULL value....
    {
      return QRY_NEED_PARAMETER;
    }

    /**
     *  Add parameter value to serialized data.
     *  Each value has a Uint32 length field (in bytes), followed by
     *  the actuall value. Allocation is in Uint32 units with unused bytes
     *  zero padded.
     **/
    Uint32 len = paramDef.getColumn()->getSizeInBytes();
    m_params.append(len);          // paramValue length in #bytes
    m_params.append(paramValue,len);

    if(unlikely(m_params.isMemoryExhausted())){
      return Err_MemoryAlloc;
    }
  }
  return 0;
} // NdbQueryOperationImpl::serializeParams


int 
NdbQueryOperationImpl::prepareReceiver()
{
  const Uint32 rowSize = NdbReceiver::ndbrecord_rowsize(m_ndbRecord, m_firstRecAttr,0,false);
  m_batchByteSize = rowSize * m_queryImpl.getMaxBatchRows();
//ndbout "m_batchByteSize=" << m_batchByteSize << endl;

  if (m_batchByteSize > 0) { // 0 bytes in batch if no result requested
    size_t bufLen = m_batchByteSize*m_queryImpl.getParallelism();
#ifdef NDEBUG
    m_batchBuffer = new char[bufLen];
    if (unlikely(m_batchBuffer == NULL)) {
      return Err_MemoryAlloc;
    }
#else
    /* To be able to check for buffer overrun.*/
    m_batchBuffer = new char[bufLen+4];
    if (unlikely(m_batchBuffer == NULL)) {
      return Err_MemoryAlloc;
    }
    m_batchBuffer[bufLen+0] = 'a';
    m_batchBuffer[bufLen+1] = 'b';
    m_batchBuffer[bufLen+2] = 'c';
    m_batchBuffer[bufLen+3] = 'd';
#endif
  }

  // Construct receiver streams and prepare them for receiving scan result
  assert(m_resultStreams==NULL);
  assert(m_queryImpl.getParallelism() > 0);
  m_resultStreams = new NdbResultStream*[m_queryImpl.getParallelism()];
  if (unlikely(m_resultStreams == NULL)) {
    return Err_MemoryAlloc;
  }
  for(Uint32 i = 0; i<m_queryImpl.getParallelism(); i++) {
    m_resultStreams[i] = NULL;  // Init to legal contents for d'tor
  }
  for(Uint32 i = 0; i<m_queryImpl.getParallelism(); i++) {
    m_resultStreams[i] = new NdbResultStream(*this, i);
    if (unlikely(m_resultStreams[i] == NULL)) {
      return Err_MemoryAlloc;
    }
    const int error = m_resultStreams[i]->prepare();
    if (unlikely(error)) {
      return error;
    }

    m_resultStreams[i]->m_receiver.init(NdbReceiver::NDB_QUERY_OPERATION, 
                                        false, this);
    m_resultStreams[i]->m_receiver
      .do_setup_ndbrecord(m_ndbRecord,
                          m_queryImpl.getMaxBatchRows(), 
                          0 /*key_size*/, 
                          0 /*read_range_no*/, 
                          rowSize,
                          &m_batchBuffer[m_batchByteSize*i],
                          m_userProjection.getColumnCount());
    m_resultStreams[i]->m_receiver.prepareSend();
  }

  return 0;
}//NdbQueryOperationImpl::prepareReceiver

int 
NdbQueryOperationImpl::prepareAttrInfo(Uint32Buffer& attrInfo)
{
  // ::prepareReceiver() need to complete first:
  assert (m_resultStreams != NULL);

  const NdbQueryOperationDefImpl& def = getQueryOperationDef();

  /**
   * Serialize parameters refered by this NdbQueryOperation.
   * Params for the complete NdbQuery is collected in a single
   * serializedParams chunk. Each operations params are 
   * proceeded by 'length' for this operation.
   */
  if (def.getType() == NdbQueryOperationDefImpl::UniqueIndexAccess)
  {
    // Reserve memory for LookupParameters, fill in contents later when
    // 'length' and 'requestInfo' has been calculated.
    size_t startPos = attrInfo.getSize();
    attrInfo.alloc(QN_LookupParameters::NodeSize);
    Uint32 requestInfo = 0;

    if (m_params.getSize() > 0)
    {
      // parameter values has been serialized as part of NdbTransaction::createQuery()
      // Only need to append it to rest of the serialized arguments
      requestInfo |= DABits::PI_KEY_PARAMS;
      attrInfo.append(m_params);    
    }

    QN_LookupParameters* param = reinterpret_cast<QN_LookupParameters*>(attrInfo.addr(startPos));
    if (unlikely(param==NULL))
       return Err_MemoryAlloc;

    param->requestInfo = requestInfo;
    param->resultData = getIdOfReceiver();
    size_t length = attrInfo.getSize() - startPos;
    if (unlikely(length > 0xFFFF)) {
      return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
    } else {
      QueryNodeParameters::setOpLen(param->len,
                                    def.isScanOperation()
                                      ?QueryNodeParameters::QN_SCAN_FRAG
                                      :QueryNodeParameters::QN_LOOKUP,
				    length);
    }
#ifdef __TRACE_SERIALIZATION
    ndbout << "Serialized params for index node " 
           << m_operationDef.getQueryOperationId()-1 << " : ";
    for(Uint32 i = startPos; i < attrInfo.getSize(); i++){
      char buf[12];
      sprintf(buf, "%.8x", attrInfo.get(i));
      ndbout << buf << " ";
    }
    ndbout << endl;
#endif
  } // if (UniqueIndexAccess ...

  // Reserve memory for LookupParameters, fill in contents later when
  // 'length' and 'requestInfo' has been calculated.
  size_t startPos = attrInfo.getSize();
  attrInfo.alloc(QN_LookupParameters::NodeSize);
  Uint32 requestInfo = 0;

  // SPJ block assume PARAMS to be supplied before ATTR_LIST
  if (m_params.getSize() > 0 &&
      def.getType() == NdbQueryOperationDefImpl::PrimaryKeyAccess)
  {
    // parameter values has been serialized as part of NdbTransaction::createQuery()
    // Only need to append it to rest of the serialized arguments
    requestInfo |= DABits::PI_KEY_PARAMS;
    attrInfo.append(m_params);    
  }

  requestInfo |= DABits::PI_ATTR_LIST;
  const int error = 
    m_userProjection.serialize(attrInfo,
                               m_resultStyle,
                               getRoot().getQueryDef().isScanQuery());
  if (unlikely(error)) {
    return error;
  }

  QN_LookupParameters* param = reinterpret_cast<QN_LookupParameters*>(attrInfo.addr(startPos)); 
  if (unlikely(param==NULL))
     return Err_MemoryAlloc;

  param->requestInfo = requestInfo;
  param->resultData = getIdOfReceiver();
  size_t length = attrInfo.getSize() - startPos;
  if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  } else {
    QueryNodeParameters::setOpLen(param->len,
                                  def.isScanOperation()
                                    ?QueryNodeParameters::QN_SCAN_FRAG
                                    :QueryNodeParameters::QN_LOOKUP,
                                  length);
  }

#ifdef __TRACE_SERIALIZATION
  ndbout << "Serialized params for node " 
         << m_operationDef.getQueryOperationId() << " : ";
  for(Uint32 i = startPos; i < attrInfo.getSize(); i++){
    char buf[12];
    sprintf(buf, "%.8x", attrInfo.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryOperationImpl::prepareAttrInfo


/* The tail of every record looks like this:
 * {AttributeHeader::READ_ANY_VALUE, receverId, correlationNum }
 *
*/
static const Uint32 correlationWordCount = 3;

static void getCorrelationData(const Uint32* ptr, 
                               Uint32 len,
                               Uint32& receverId,
                               Uint32& correlationNum){
  assert(len>=correlationWordCount);
  const Uint32* corrTail = ptr + len - correlationWordCount;
  const AttributeHeader attHead(corrTail[0]);
  assert(attHead.getAttributeId() == AttributeHeader::READ_ANY_VALUE);
  assert(attHead.getByteSize()==8);
  receverId = corrTail[1];
  correlationNum = corrTail[2];
}

bool 
NdbQueryOperationImpl::execTRANSID_AI(const Uint32* ptr, Uint32 len){
  ndbout << "NdbQueryOperationImpl::execTRANSID_AI(): *this="
	 << *this << endl;
  NdbQueryOperationImpl& root = getRoot();

  if(getQueryDef().isScanQuery()){
    Uint32 receiverId;
    Uint32 correlationNum;
    getCorrelationData(ptr, len, receiverId, correlationNum);
    Uint32 streamNo;
    /* receiverId now holds the Id of the receiver of the corresponding stream
    * of the root operation. We can thus find the correct stream number.*/
    for(streamNo = 0; 
        streamNo<getQuery().getParallelism() && 
          root.m_resultStreams[streamNo]->m_receiver.getId() != receiverId; 
        streamNo++);
    assert(streamNo<getQuery().getParallelism());

    // Process result values.
    NdbResultStream* const resultStream = m_resultStreams[streamNo];
    resultStream->m_receiver
      .execTRANSID_AI(ptr, len - correlationWordCount);
    resultStream->m_transidAICount++;

    /* Put into the map such that parent and child can be matched.
    * Lower 16 bits of correlationNum is for this tuple.*/
    resultStream
      ->m_correlToTupNumMap.put(correlationNum & 0xffff, 
                                resultStream->m_transidAICount-1);
    resultStream
      ->setParentTupleCorr(resultStream->m_transidAICount-1, 
                           correlationNum >> 16);
    /* For scans, the root counts rows for all descendants also.*/
    root.m_resultStreams[streamNo]->m_pendingResults--;
    if(root.m_resultStreams[streamNo]->isBatchComplete()){
      m_queryImpl.buildChildTupleLinks(streamNo);
      /* nextResult() will later move it from m_fullStreams to m_applStreams
       * undex mutex protection.*/
      m_queryImpl.m_fullStreams.push(*root.m_resultStreams[streamNo]);
    }
    return false;
  } else { // Lookup query
    // The root operation is a lookup.
    m_resultStreams[0]->m_receiver.execTRANSID_AI(ptr, len);
    m_resultStreams[0]->m_transidAICount++;

    m_resultStreams[0]->m_pendingResults--;
    /* Receiving this message means that each child has been instantiated 
     * one more. Therefore, increment the pending message count for the children.
     */
    for(Uint32 i = 0; i<getNoOfChildOperations(); i++){
      if(getChildOperation(i).m_resultStreams[0]->isBatchComplete()){
        /* This child appeared to be complete prior to receiving this message, 
         * but now we know that there will be
         * an extra instance. Therefore, increment total count of pending 
         * streams.*/
        m_queryImpl.incPendingStreams(1);
      }    
      getChildOperation(i).m_resultStreams[0]->m_pendingResults++;
      if(getChildOperation(i).m_resultStreams[0]->isBatchComplete()){
        /* This child stream appears to be complete. Therefore decrement total 
         * count of pending streams.*/
        m_queryImpl.incPendingStreams(-1);
      }
    }
    
    if(m_resultStreams[0]->m_pendingResults == 0){ 
      /* If this stream is complete, check if the query is also 
       * complete for this batch.*/
      return m_queryImpl.incPendingStreams(-1);
    }else if(m_resultStreams[0]->m_pendingResults == -1){
      /* This happens because we received results for the child before those
       * of the parent. This operation will be set as complete again when the 
       * TRANSID_AI for the parent arrives.*/
      m_queryImpl.incPendingStreams(1);
    }
    return false;
  }
}


bool 
NdbQueryOperationImpl::execTCKEYREF(NdbApiSignal* aSignal){
  ndbout << "NdbQueryOperationImpl::execTCKEYREF(): *this="
        << *this << endl;
  /* The SPJ block does not forward TCKEYREFs for trees with scan roots.*/
  assert(!getQueryDef().isScanQuery());
  if(isBatchComplete()){
    /* This happens because we received results for the child before those
     * of the parent. This stream will be set as complete again when the 
     * TRANSID_AI for the parent arrives.*/
    m_queryImpl.incPendingStreams(1);
  }  
  m_resultStreams[0]->m_pendingResults--;
  if(m_resultStreams[0]->isBatchComplete()){
    /* This stream is complete. Check if the query is also complete.*/
    return getQuery().incPendingStreams(-1);
  }
  return false;
}

void 
NdbQueryOperationImpl::execSCAN_TABCONF(Uint32 tcPtrI, 
                                        Uint32 rowCount,
                                        NdbReceiver* receiver)
{
  ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF(): tcPtrI="
         << tcPtrI << " rowCount=" << rowCount 
         << " *this=" << *this << endl;
  // For now, only the root operation may be a scan.
  assert(m_operationDef.getQueryOperationIx()==0);
  assert(m_operationDef.isScanOperation());
  Uint32 streamNo;
  // Find stream number.
  for(streamNo = 0; 
      streamNo<getQuery().getParallelism() && 
        &getRoot().m_resultStreams[streamNo]
        ->m_receiver != receiver; 
      streamNo++);
  assert(streamNo<getQuery().getParallelism());
  assert(m_resultStreams[streamNo]->m_pendingScanTabConf);
  m_resultStreams[streamNo]->m_pendingScanTabConf = false;;
  m_resultStreams[streamNo]->m_pendingResults += rowCount;
  if(m_resultStreams[streamNo]->isBatchComplete()){
    /* This stream is now complete*/
    m_queryImpl.incPendingStreams(-1);
    m_queryImpl.buildChildTupleLinks(streamNo);
    /* nextResult() will later move it from m_fullStreams to m_applStreams
     * undex mutex protection.*/
    m_queryImpl.m_fullStreams.push(*m_resultStreams[streamNo]);
  }
}

void 
NdbQueryOperationImpl::buildChildTupleLinks(Uint32 streamNo)
{
  NdbResultStream& resultStream = *m_resultStreams[streamNo];
  /* Now we have received all tuples for all operations. We can thus call
   * execSCANOPCONF() with the right row count.
   */
  resultStream.m_receiver.execSCANOPCONF(RNIL, 0, 
                                         resultStream.m_transidAICount);

  if (getNoOfParentOperations()>0) {
    assert(getNoOfParentOperations()==1);
    NdbQueryOperationImpl* parent = &getParentOperation(0);

    /* Find the number of this operation in its parent's list of children.*/
    Uint32 childNo = 0;
    while(childNo < parent->getNoOfChildOperations() &&
          this != &parent->getChildOperation(childNo)){
      childNo++;
    }
    assert(childNo < parent->getNoOfChildOperations());

    /* Make references from parent tuple to child tuple. These will be
     * used by nextResult() to fetch the proper children when iterating
     * over the result of a scan with children.
     */
    NdbResultStream& parentStream = *parent->m_resultStreams[streamNo];
    for (Uint32 tupNo = 0; tupNo<resultStream.m_transidAICount; tupNo++) {
      /* Get the correlation number of the parent tuple. This number
       * uniquely identifies the parent tuple within this stream and batch.
       */
      const Uint32 parentCorrNum = resultStream.getParentTupleCorr(tupNo);

      /* Get the number (index) of the parent tuple among those tuples 
       * received for the parent operation within this stream and batch.
       */
      const Uint32 parentTupNo = 
        parentStream.m_correlToTupNumMap.get(parentCorrNum);
      // Verify that the parent tuple exists.
      assert(parentTupNo != tupleNotFound);

      /* Verify that no child tuple has been set for this parent tuple
       * and child operation yet.
       */
      assert(parentStream.getChildTupleIdx(childNo, parentTupNo) 
             == tupleNotFound);
      /* Set this tuple as the child of its parent tuple*/
      parentStream.setChildTupleIdx(childNo, parentTupNo, tupNo);
    }
  } 
} //NdbQueryOperationImpl::buildChildTupleLinks

Uint32 
NdbQueryOperationImpl::getIdOfReceiver() const {
  return m_resultStreams[0]->m_receiver.getId();
}

bool 
NdbQueryOperationImpl::isBatchComplete() const { 
  for(Uint32 i = 0; i < getQuery().getParallelism(); i++){
    if(!m_resultStreams[i]->isBatchComplete()){
      return false;
    }
  }
  return true;
}


const NdbReceiver& 
NdbQueryOperationImpl::getReceiver(Uint32 recNo) const {
  assert(recNo<getQuery().getParallelism());
  return m_resultStreams[recNo]->m_receiver;
}


/** For debugging.*/
NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl& op){
  out << "[ this: " << &op
      << "  m_magic: " << op.m_magic;
  for(unsigned int i = 0; i<op.getNoOfParentOperations(); i++){
    out << "  m_parents[" << i << "]" << &op.getParentOperation(i); 
  }
  for(unsigned int i = 0; i<op.getNoOfChildOperations(); i++){
    out << "  m_children[" << i << "]" << &op.getChildOperation(i); 
  }
  out << "  m_queryImpl: " << &op.m_queryImpl;
  out << "  m_operationDef: " << &op.m_operationDef;
  for(Uint32 i = 0; i<op.m_queryImpl.getParallelism(); i++){
//  const NdbQueryOperationImpl::NdbResultStream& resultStream 
    const NdbResultStream& resultStream 
      = *op.m_resultStreams[i];
    out << "  m_resultStream[" << i << "]{";
    out << " m_transidAICount: " << resultStream.m_transidAICount;
    out << " m_pendingResults: " << resultStream.m_pendingResults;
    out << " m_pendingScanTabConf " << resultStream.m_pendingScanTabConf;
    out << "}";
  }
  out << " m_isRowNull " << op.m_isRowNull;
  out << " ]";
  return out;
}
 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;
template class Vector<NdbResultStream::TupleIdMap::Pair>;
