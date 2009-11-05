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

#if 0
#define DEBUG_CRASH() assert(false)
#else
#define DEBUG_CRASH()
#endif

//#define TEST_SCANREQ

/* Various error codes that are not specific to NdbQuery. */
STATIC_CONST(Err_MemoryAlloc = 4000);
STATIC_CONST(Err_SendFailed = 4002);
STATIC_CONST(Err_UnknownColumn = 4004);
STATIC_CONST(Err_ReceiveFromNdbFailed = 4008);
STATIC_CONST(Err_NodeFailCausedAbort = 4028);
STATIC_CONST(Err_MixRecAttrAndRecord = 4284);
STATIC_CONST(Err_DifferentTabForKeyRecAndAttrRec = 4287);

/* A 'void' index for a tuple in internal parent / child correlation structs .*/
STATIC_CONST(tupleNotFound = 0xffffffff);

/** Set to true to trace incomming signals.*/
const bool traceSignals = false;


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

    void clear()
    { m_vector.clear(); }

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
  /** True if there is a pending CONF messages for this stream.*/
  bool m_pendingConf;

  /** Prepare for receiving first results. */
  int prepare();

  /** Prepare for receiving next batch of scan results. */
  void reset();

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
    return m_pendingResults==0 && !m_pendingConf;
  }

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbResultStream&);

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
  m_receiver(operation.getQuery().getNdbTransaction().getNdb(), &operation),  // FIXME? Use Ndb recycle lists
  m_transidAICount(0),
  m_correlToTupNumMap(),
  m_pendingResults(0),
  m_pendingConf(false),
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

    const size_t batchRows = m_operation.getQuery().getMaxBatchRows();
    if (m_operation.getNoOfParentOperations()>0) {
      assert (m_operation.getNoOfParentOperations()==1);
      m_parentTupleCorr = new Uint32[batchRows];
      if (unlikely(m_parentTupleCorr==NULL))
        return Err_MemoryAlloc;
    }

    if (m_operation.getNoOfChildOperations()>0) {
      const size_t correlatedChilds =  batchRows
                                     * m_operation.getNoOfChildOperations();
      m_childTupleIdx = new Uint32[correlatedChilds];
      if (unlikely(m_childTupleIdx==NULL))
        return Err_MemoryAlloc;

      for (unsigned i=0; i<correlatedChilds; i++) {
        m_childTupleIdx[i] = tupleNotFound;
      }
    }
  }

  // Root operation need a CONF to complete
  m_pendingConf = (&m_operation.getRoot()==&m_operation);
  m_pendingResults = 0; // Set by exec..CONF when expected #rows are known

  return 0;
} //NdbResultStream::prepare


void
NdbResultStream::reset()
{
  assert (m_operation.getQueryDef().isScanQuery());

  // Root scan-operation need a ScanTabConf to complete
  m_transidAICount = 0;
  m_pendingResults = 0;
  m_pendingConf    = (&m_operation.getRoot()==&m_operation);

  if (m_parentTupleCorr!=NULL) {
//  const size_t batchRows = m_operation.getQuery().getMaxBatchRows();
  }

  if (m_childTupleIdx!=NULL) {
    const size_t correlatedChilds =  m_operation.getQuery().getMaxBatchRows()
                                   * m_operation.getNoOfChildOperations();
    for (unsigned i=0; i<correlatedChilds; i++) {
      m_childTupleIdx[i] = tupleNotFound;
    }
  }

  m_correlToTupNumMap.clear();

  m_receiver.prepareSend();
} //NdbResultStream::reset


void 
NdbResultStream::setParentTupleCorr(Uint32 rowNo, Uint32 correlationNum) const
{
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


///////////////////////////////////////////
/////////  NdbQuery API methods ///////////
///////////////////////////////////////////

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
NdbQuery::close(bool forceSend)
{
  m_impl.close(forceSend);
}

NdbTransaction*
NdbQuery::getNdbTransaction() const
{
  return &m_impl.getNdbTransaction();
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
  if (unlikely(column==NULL)) {
    m_impl.getQuery().setErrorCode(QRY_REQ_ARG_IS_NULL);
    return NULL;
  }
  return m_impl.getValue(NdbColumnImpl::getImpl(*column), resultBuffer);
}

int
NdbQueryOperation::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  if (unlikely(rec==0 || resBuffer==0)) {
    m_impl.getQuery().setErrorCode(QRY_REQ_ARG_IS_NULL);
    return -1;
  }
  return m_impl.setResultRowBuf(rec, resBuffer, result_mask);
}

int
NdbQueryOperation::setResultRowRef (
                       const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask)
{
  // FIXME: Errors must be set in the NdbError object owned by this operation.
  if (unlikely(rec==0)) {
    m_impl.getQuery().setErrorCode(QRY_REQ_ARG_IS_NULL);
    return -1;
  }
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
                           const NdbQueryDefImpl& queryDef):
  m_interface(*this),
  m_state(Initial),
  m_tcState(Inactive),
  m_next(NULL),
  m_queryDef(queryDef),
  m_error(),
  m_transaction(trans),
  m_scanTransaction(NULL),
  m_operations(0),
  m_countOperations(0),
  m_pendingStreams(0),
  m_parallelism(0),
  m_maxBatchRows(0),
  m_applStreams(),
  m_fullStreams(),
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
  // NOTE: m_operations[] was allocated as a single memory chunk with
  // placement new construction of each operation.
  // Requires explicit call to d'tor of each operation before memory is free'ed.
  if (m_operations != NULL) {
    for (int i=m_countOperations-1; i>=0; --i)
    { m_operations[i].~NdbQueryOperationImpl();
    }
    free(m_operations);
    m_operations = NULL;
  }
  m_state = Destructed;
}

void
NdbQueryImpl::postFetchRelease()
{
  if (m_operations != NULL) {
    for (unsigned i=0; i<m_countOperations; i++)
    { m_operations[i].postFetchRelease();
    }
  }
}


//static
NdbQueryImpl*
NdbQueryImpl::buildQuery(NdbTransaction& trans, 
                         const NdbQueryDefImpl& queryDef)
{
  if (queryDef.getNoOfOperations()==0) {
    trans.setErrorCode(QRY_HAS_ZERO_OPERATIONS);
    return NULL;
  }

  NdbQueryImpl* const query = new NdbQueryImpl(trans, queryDef);
  if (query==NULL) {
    trans.setOperationErrorCodeAbort(Err_MemoryAlloc);
  }
  assert(query->m_state==Initial);
  return query;
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
  const int error = getRoot().getQueryOperationDef().prepareKeyInfo(m_keyInfo, paramValues);
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
  assert(m_state<Defined);
  m_state = Defined;
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
  if (unlikely(m_state < Executing || m_state >= Closed)) {
    assert (m_state >= Initial && m_state < Destructed);
    if (m_state == Failed)
      setErrorCode(QRY_IN_ERROR_STATE);
    else
      setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return NdbQuery::NextResult_error;
  }

  if (m_state == EndOfData) {
    return NdbQuery::NextResult_scanComplete;
  }

  /* To minimize lock contention, each operation has two instances 
   * of StreamStack (which contains result streams). m_applStreams is only
   * accessed by the application thread, so it is safe to use it without 
   * locks.
   */
  while(m_applStreams.top() != NULL
        && !m_applStreams.top()->m_receiver.nextResult()){
    m_applStreams.pop();
  }

  if (unlikely(m_applStreams.top()==NULL))
  {
    /* m_applStreams is empty, so we cannot get more results without 
     * possibly blocking.
     */
    if (fetchAllowed)
    {
      /* fetchMoreResults() will either copy streams that are already
       * complete (under mutex protection), or block until more data arrives.
       */
      const FetchResult fetchResult = fetchMoreResults(forceSend);
      switch (fetchResult) {
      case FetchResult_otherError:
        assert (m_error.code != 0);
        setErrorCode(m_error.code);
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
        for (unsigned i = 0; i<getNoOfOperations(); i++) {
          m_operations[i].m_isRowNull = true;
        }
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
  NdbResultStream* resultStream = m_applStreams.top();
  const Uint32 rowNo = resultStream->m_receiver.getCurrentRow();
  const char* const rootBuff = resultStream->m_receiver.get_row();
  assert(rootBuff!=NULL || 
         (root.m_firstRecAttr==NULL && root.m_ndbRecord==NULL));
  root.m_isRowNull = false;
  if (root.m_firstRecAttr != NULL) {
    root.fetchRecAttrResults(resultStream->m_streamNo);
  }
  if (root.m_ndbRecord != NULL) {
    if (root.m_resultRef!=NULL) {
      // Set application pointer to point into internal buffer.
      *root.m_resultRef = rootBuff;
    } else { 
      // Copy result to buffer supplied by application.
      memcpy(root.m_resultBuffer, rootBuff, 
             resultStream->m_receiver.m_record.m_ndb_record
             ->m_row_size);
    }
  }
  if (m_queryDef.isScanQuery()) {
    for (Uint32 i = 0; i<root.getNoOfChildOperations(); i++) {
      /* For each child, fetch the right row.*/
      root.getChildOperation(i)
        .updateChildResult(resultStream->m_streamNo, 
                           resultStream->getChildTupleIdx(i,rowNo));
    }
  } else { // Lookup query
    /* Fetch results for all non-root lookups also.*/
    for (Uint32 i = 1; i<getNoOfOperations(); i++) {
      NdbQueryOperationImpl& operation = getQueryOperation(i);
      NdbResultStream* resultStream = operation.m_resultStreams[0];

      assert(resultStream->m_transidAICount<=1);
      operation.m_isRowNull = (resultStream->m_transidAICount==0);

      // Check if there was a result for this operation.
      if (operation.m_isRowNull==false) {
        const char* buff = resultStream->m_receiver.get_row();

        if (operation.m_firstRecAttr != NULL) {
          operation.fetchRecAttrResults(0);
        }
        if (operation.m_ndbRecord != NULL) {
          if(operation.m_resultRef!=NULL){
            // Set application pointer to point into internal buffer.
            *operation.m_resultRef = buff;
          }else{
            // Copy result to buffer supplied by application.
            memcpy(operation.m_resultBuffer, buff, 
                   resultStream
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
    
    assert (m_state==Executing);
    assert (m_scanTransaction);

    Ndb* const ndb = m_transaction.getNdb();
    TransporterFacade* const facade = ndb->theImpl->m_transporter_facade;

    /* This part needs to be done under mutex due to synchronization with 
     * receiver thread. */
    PollGuard poll_guard(facade,
                         &ndb->theImpl->theWaiter,
                         ndb->theNdbBlockNumber);

    while (likely(m_error.code==0))
    {
      /* m_fullStreams contains any streams that are complete (for this batch)
       * but have not yet been moved (under mutex protection) to 
       * m_applStreams.*/
      if(m_fullStreams.top()==NULL){
        if(getRoot().isBatchComplete()){
          // Request another scan batch, may already be at EOF
          const int sent = sendFetchMore(m_transaction.getConnectedNodeId());
          if (sent==0) {  // EOF reached?
            m_state = EndOfData;
            postFetchRelease();
            return FetchResult_scanComplete;
          } else if (unlikely(sent<0)) {
            return FetchResult_sendFail;
          }
        } //if (isBatchComplete...

        /* More results are on the way, so we wait for them.*/
        const FetchResult waitResult = static_cast<FetchResult>
          (poll_guard.wait_scan(3*facade->m_waitfor_timeout, 
                                m_transaction.getConnectedNodeId(), 
                                forceSend));
        if(waitResult != FetchResult_ok){
          return waitResult;
        }
      } // if (m_fullStreams.top()==NULL)

      // Assert: No sporious wakeups w/ neither resultdata, nor EOF:
      assert (m_fullStreams.top()!=NULL || getRoot().isBatchComplete() || m_error.code);

      /* Move full streams from receiver thread's container to application 
       *  thread's container.*/
      while (m_fullStreams.top()!=NULL) {
        // We should never receive empty 'm_fullStreams'
        assert(m_fullStreams.top()->m_receiver.hasResults());
        m_applStreams.push(*m_fullStreams.pop());
      }
      if (m_applStreams.top() != NULL) {
        return FetchResult_ok;
      }

      // Only expect to end up here if another ::sendFetchMore() is required
      assert (getRoot().isBatchComplete() || m_error.code);
    } // while(true)

    // 'while' terminated by m_error.code
    assert (m_error.code);
    return FetchResult_otherError;

  } else { // is a Lookup query
    /* The root operation is a lookup. Lookups are guaranteed to be complete
     * before NdbTransaction::execute() returns. Therefore we do not set
     * the lock, because we know that the signal receiver thread will not
     * be accessing  m_fullStreams at this time.*/
    if(m_fullStreams.top()==NULL){
      /* Getting here means that either:
       *  - No results was returned (TCKEYREF)
       *  - or, the application called nextResult() twice for a lookup query.
       */
      m_state = EndOfData;
      postFetchRelease();
      return FetchResult_scanComplete;
    }else{
      /* Move stream from receiver thread's container to application 
       *  thread's container.*/
      m_applStreams.push(*m_fullStreams.pop());
      assert(m_fullStreams.top()==NULL); // Only one stream for lookups.
      assert(m_applStreams.top()->m_receiver.hasResults());
      return FetchResult_ok;
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
   * under mutex protection.
   */
  if (getRoot().m_resultStreams[0]->m_receiver.hasResults()) {
    m_fullStreams.push(*getRoot().m_resultStreams[0]);
  }
} //NdbQueryImpl::closeSingletonScans

int
NdbQueryImpl::close(bool forceSend)
{
  int res = 0;

  assert (m_state >= Initial && m_state < Destructed);
  Ndb* const ndb = m_transaction.getNdb();

  if (m_tcState > Inactive && m_tcState != Completed)
  {
    res = closeTcCursor(forceSend);
  }

  // Throw any pending results
  m_fullStreams.clear();
  m_applStreams.clear();

  if (m_scanTransaction != NULL)
  {
    assert (m_state != Closed);
    assert (m_scanTransaction->m_scanningQuery == this);
    m_scanTransaction->m_scanningQuery = NULL;
    ndb->closeTransaction(m_scanTransaction);
    ndb->theRemainingStartTransactions--;  // Compensate; m_scanTransaction was not a real Txn
    m_scanTransaction = NULL;
  }

  postFetchRelease();
  m_state = Closed;  // Even if it was previously 'Failed' it is closed now!
  return res;
} //NdbQueryImpl::close


void
NdbQueryImpl::release()
{ 
  assert (m_state >= Initial && m_state < Destructed);
  if (m_state != Closed) {
    close(true);  // Ignore any errors, explicit ::close() first if errors are of interest
  }

  delete this;
}

void
NdbQueryImpl::setErrorCodeAbort(int aErrorCode)
{
  m_error.code = aErrorCode;
  m_transaction.theErrorLine = 0;
  m_transaction.theErrorOperation = NULL;
  m_transaction.setOperationErrorCodeAbort(aErrorCode);
  m_state = Failed;
}


bool 
NdbQueryImpl::execTCKEYCONF()
{
  if (traceSignals) {
    ndbout << "NdbQueryImpl::execTCKEYCONF()" << endl;
  }
  assert(!getQueryDef().isScanQuery());

  NdbResultStream& rootStream = *getRoot().m_resultStreams[0];
  assert(rootStream.m_pendingConf);
  rootStream.m_pendingConf = false;

  // Result rows counted on root operation only.
  // Initially we assume all child results to be returned.
  rootStream.m_pendingResults += 1+getRoot().countAllChildOperations();

  bool ret = false;
  if (rootStream.isBatchComplete()) { 
    /* If this stream is complete, check if the query is also 
     * complete for this batch.
     */
    ret = countPendingStreams(-1);
  }

  if (traceSignals) {
    ndbout << "NdbQueryImpl::execTCKEYCONF(): returns:" << ret
           << ", m_pendingStreams=" << m_pendingStreams
           << ", rootStream=" << rootStream 
           << endl;
  }
  return ret;
}

void 
NdbQueryImpl::execCLOSE_SCAN_REP(bool needClose)
{
  if (traceSignals)
  {
    ndbout << "NdbQueryImpl::execCLOSE_SCAN_REP()" << endl;
  }
  assert (m_tcState < Completed);
  m_tcState = (needClose) ? FetchMore : Completed;
}


bool 
NdbQueryImpl::countPendingStreams(int increment)
{
  m_pendingStreams += increment;
  if (traceSignals) {
    ndbout << "NdbQueryImpl::countPendingStreams(" << increment << "): "
           << ", pendingStreams=" << m_pendingStreams <<  endl;
  }

  if (m_pendingStreams==0) {
    for (Uint32 i = 0; i < getNoOfOperations(); i++) {
      assert(getQueryOperation(i).isBatchComplete());
    }
    if (!getQueryDef().isScanQuery()) {
      closeSingletonScans();
    }
    return true;
  } else {
    return false;
  }
}

int
NdbQueryImpl::prepareSend()
{
  if (unlikely(m_state != Defined)) {
    assert (m_state >= Initial && m_state < Destructed);
    if (m_state == Failed) 
      setErrorCodeAbort(QRY_IN_ERROR_STATE);
    else
      setErrorCodeAbort(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return -1;
  }

  assert (m_pendingStreams==0);

  // Determine execution parameters 'parallelism' and 'batch size'.
  // May be user specified (TODO), and/or,  limited/specified by config values
  //
  if (getQueryDef().isScanQuery())
  {
    m_pendingStreams = getRoot().getQueryOperationDef().getTable().getFragmentCount();

    // Parallelism may be user specified, else(==0) use default
    if (m_parallelism == 0 || m_parallelism > m_pendingStreams) {
      m_parallelism = m_pendingStreams;
    }

    Ndb* const ndb = m_transaction.getNdb();
    TransporterFacade *tp = ndb->theImpl->m_transporter_facade;

    Uint32 batchRows = m_maxBatchRows; // >0: User specified prefered value, ==0: Use default CFG values

#ifdef TEST_SCANREQ
    batchRows = 1;  // To force usage of SCAN_NEXTREQ even for small scans resultsets
#endif

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

    /** Scan operations need a own sub-transaction object associated with each 
     *  query.
     */
    ndb->theRemainingStartTransactions++; // Compensate; does not start a real Txn
    NdbTransaction *scanTxn = ndb->hupp(&m_transaction);
    if (scanTxn==NULL) {
      ndb->theRemainingStartTransactions--;
      m_transaction.setOperationErrorCodeAbort(ndb->getNdbError().code);
      return -1;
    }
    scanTxn->theMagicNumber = 0x37412619;
    scanTxn->m_scanningQuery = this;
    this->m_scanTransaction = scanTxn;
  }
  else  // Lookup query
  {
    m_pendingStreams = m_parallelism = 1;
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

  m_state = Prepared;
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
NdbQueryImpl::doSend(int nodeId, bool lastFlag)  // TODO: Use 'lastFlag'
{
  if (unlikely(m_state != Prepared)) {
    assert (m_state >= Initial && m_state < Destructed);
    if (m_state == Failed) 
      setErrorCodeAbort(QRY_IN_ERROR_STATE);
    else
      setErrorCodeAbort(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return -1;
  }

  Ndb& ndb = *m_transaction.getNdb();
  TransporterFacade *tp = ndb.theImpl->m_transporter_facade;

  const NdbQueryOperationDefImpl& rootDef = getRoot().getQueryOperationDef();
  const NdbTableImpl* const rootTable = rootDef.getIndex()
    ? rootDef.getIndex()->getIndexTable()
    : &rootDef.getTable();

  Uint32 tTableId = rootTable->m_id;
  Uint32 tSchemaVersion = rootTable->m_version;

  if (rootDef.isScanOperation())
  {
    Uint32 scan_flags = 0;  // TODO: Specify with ScanOptions::SO_SCANFLAGS
    Uint32 parallel = m_parallelism;

    bool tupScan = (scan_flags & NdbScanOperation::SF_TupScan);
    bool rangeScan = false;

    bool   isPruned;
    Uint32 hashValue;
    const int error = rootDef.checkPrunable(m_keyInfo, isPruned, hashValue);
    if (unlikely(error != 0))
      return error;

    /* Handle IndexScan specifics */
    if ( (int) rootTable->m_indexType ==
         (int) NdbDictionary::Index::OrderedIndex )
    {
      rangeScan = true;
      tupScan = false;
    }

    assert (m_parallelism > 0);
    assert (m_maxBatchRows > 0);

    NdbApiSignal tSignal(&ndb);
    tSignal.setSignal(GSN_SCAN_TABREQ);

    ScanTabReq * const scanTabReq = CAST_PTR(ScanTabReq, tSignal.getDataPtrSend());
    Uint32 reqInfo = 0;

    const Uint64 transId = m_scanTransaction->getTransactionId();

    scanTabReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
    scanTabReq->buddyConPtr = m_scanTransaction->theBuddyConPtr; // 'buddy' refers 'real-transaction'->theTCConPtr
    scanTabReq->spare = 0;  // Unused in later protocoll versions
    scanTabReq->tableId = tTableId;
    scanTabReq->tableSchemaVersion = tSchemaVersion;
    scanTabReq->storedProcId = 0xFFFF;
    scanTabReq->transId1 = (Uint32) transId;
    scanTabReq->transId2 = (Uint32) (transId >> 32);

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
    if (isPruned)
    {
//    printf("Build pruned SCANREQ, w/ hashValue:%d\n", hashValue);
      ScanTabReq::setDistributionKeyFlag(reqInfo, 1);
      scanTabReq->distributionKey= hashValue;
      tSignal.setLength(ScanTabReq::StaticLength + 1);
    } else {
      tSignal.setLength(ScanTabReq::StaticLength);
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
    LinearSectionPtr secs[3];
    Uint32 receivers[64];  // TODO: 64 is a temp hack
 
    const NdbQueryOperationImpl& queryOp = getRoot();
    for(Uint32 i = 0; i<m_parallelism; i++){
      receivers[i] = queryOp.getReceiver(i).getId();
    }

    secs[0].p= receivers;
    secs[0].sz= m_parallelism;

    secs[1].p= m_attrInfo.addr();
    secs[1].sz= m_attrInfo.getSize();

    Uint32 numSections= 2;
    if (m_keyInfo.getSize() > 0)
    {
      secs[2].p= m_keyInfo.addr();
      secs[2].sz= m_keyInfo.getSize();
      numSections= 3;
    }

    /* Send Fragmented as SCAN_TABREQ can be large */
    const int res = tp->sendFragmentedSignal(&tSignal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCodeAbort(Err_SendFailed);  // Error: 'Send to NDB failed'
      return FetchResult_sendFail;
    }
    m_tcState = Fetching;

  } else {  // Lookup query

    NdbApiSignal tSignal(&ndb);
    tSignal.setSignal(GSN_TCKEYREQ);  // TODO: or GSN_TCINDXREQ

    TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, tSignal.getDataPtrSend());

    const Uint64 transId = m_transaction.getTransactionId();
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

    tSignal.setLength(TcKeyReq::StaticLength);

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

    LinearSectionPtr secs[2];
    secs[TcKeyReq::KeyInfoSectionNum].p= m_keyInfo.addr();
    secs[TcKeyReq::KeyInfoSectionNum].sz= m_keyInfo.getSize();
    Uint32 numSections= 1;

    if (m_attrInfo.getSize() > 0)
    {
      secs[TcKeyReq::AttrInfoSectionNum].p= m_attrInfo.addr();
      secs[TcKeyReq::AttrInfoSectionNum].sz= m_attrInfo.getSize();
      numSections= 2;
    }

    const int res = tp->sendSignal(&tSignal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCodeAbort(Err_SendFailed);  // Error: 'Send to NDB failed'
      return FetchResult_sendFail;
    }
    m_transaction.OpSent();
    m_tcState = Completed;
  } // if

  // Shrink memory footprint by removing structures not required after ::execute()
  m_keyInfo.releaseExtend();
  m_attrInfo.releaseExtend();

  /* Todo : Consider calling NdbOperation::postExecuteRelease()
   * Ideally it should be called outside TP mutex, so not added
   * here yet
   */

  m_state = Executing;
  return 1;
} // NdbQueryImpl::doSend()


/******************************************************************************
int sendFetchMore() - Fetch another scan batch, optionaly closing the scan
                
                Request another batch of rows to be retrieved from the scan.
                Transporter mutex is locked before this method is called. 

Return Value:   Return >0 : send was succesful, returns number of fragments
                having pending scans batches.
                Return =0 : No more rows is available -> EOF
                Return -1: In all other case.   
Parameters:     nodeId: Receiving processor node
Remark:
******************************************************************************/
int
NdbQueryImpl::sendFetchMore(int nodeId)
{
  Uint32 sent = 0;
  NdbQueryOperationImpl& root = getRoot();
  Uint32 receivers[64];  // TODO: 64 is a temp hack

  assert (root.m_resultStreams!=NULL);
  for(unsigned i = 0; i<m_parallelism; i++)
  {
    const Uint32 tcPtrI = root.getReceiver(i).m_tcPtrI;
    if (tcPtrI != RNIL) {
      receivers[sent++] = tcPtrI;
      for (unsigned op=0; op<m_countOperations; op++) {
        m_operations[op].m_resultStreams[i]->reset();
      }
    }
  }

//printf("::sendFetchMore, to nodeId:%d, sent:%d\n", nodeId, sent);
  if (sent==0)
  {
    assert (m_tcState != FetchMore);
    m_tcState = Completed;
    return 0;
  }

  m_pendingStreams = 0;
  assert (m_tcState == FetchMore);
  m_tcState = Fetching;

  Ndb& ndb = *m_transaction.getNdb();
  NdbApiSignal tSignal(&ndb);
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  ScanNextReq * const scanNextReq = CAST_PTR(ScanNextReq, tSignal.getDataPtrSend());

  assert (m_scanTransaction);
  const Uint64 transId = m_scanTransaction->getTransactionId();

  scanNextReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
  scanNextReq->stopScan = 0;
  scanNextReq->transId1 = (Uint32) transId;
  scanNextReq->transId2 = (Uint32) (transId >> 32);
  tSignal.setLength(ScanNextReq::SignalLength);

  LinearSectionPtr secs[1];
  secs[ScanNextReq::ReceiverIdsSectionNum].p = receivers;
  secs[ScanNextReq::ReceiverIdsSectionNum].sz = sent;

  TransporterFacade* tp = ndb.theImpl->m_transporter_facade;
  const int res = tp->sendSignal(&tSignal, nodeId, secs, 1);
  if (unlikely(res == -1)) {
    setErrorCodeAbort(Err_SendFailed);  // Error: 'Send to NDB failed'
    return FetchResult_sendFail;
  }

  return sent;
} // NdbQueryImpl::sendFetchMore()

int
NdbQueryImpl::closeTcCursor(bool forceSend)
{
  assert (m_queryDef.isScanQuery());

  Ndb* const ndb = m_transaction.getNdb();
  TransporterFacade* const facade = ndb->theImpl->m_transporter_facade;

  /* This part needs to be done under mutex due to synchronization with 
   * receiver thread.
   */
  PollGuard poll_guard(facade,
                       &ndb->theImpl->theWaiter,
                       ndb->theNdbBlockNumber);

  /* Wait for outstanding scan results from current batch fetch */
  while (!getRoot().isBatchComplete() && m_error.code==0)
  {
    const FetchResult waitResult = static_cast<FetchResult>
          (poll_guard.wait_scan(3*facade->m_waitfor_timeout, 
                                m_transaction.getConnectedNodeId(), 
                                forceSend));
    switch (waitResult) {
    case FetchResult_ok:
      break;
    case FetchResult_nodeFail:
      setErrorCode(Err_NodeFailCausedAbort);  // Node fail
      return -1;
    case FetchResult_timeOut:
      setErrorCode(Err_ReceiveFromNdbFailed); // Timeout
      return -1;
    default:
      assert(false);
    }
  } // while

  m_error.code = 0;  // Ignore possible errorcode caused by previous fetching

  if (m_tcState == FetchMore)  // TC has an open scan cursor.
  {
    /* Send SCANREQ(close) */
    const int error = sendClose(m_transaction.getConnectedNodeId());
    if (unlikely(error))
      return error;

    /* Wait for close to be confirmed: */
    while (m_tcState != Completed)
    {
      const FetchResult waitResult = static_cast<FetchResult>
            (poll_guard.wait_scan(3*facade->m_waitfor_timeout, 
                                  m_transaction.getConnectedNodeId(), 
                                  forceSend));
      switch (waitResult) {
      case FetchResult_ok:
        if (unlikely(m_error.code))   // Close request itself failed, keep error
        {
          setErrorCode(m_error.code);
          return -1;
        }
        break;
      case FetchResult_nodeFail:
        setErrorCode(Err_NodeFailCausedAbort);  // Node fail
        return -1;
      case FetchResult_timeOut:
        setErrorCode(Err_ReceiveFromNdbFailed); // Timeout
        return -1;
      default:
        assert(false);
      }
    } // while
  } // if

  m_tcState = Completed;
  return 0;
} //NdbQueryImpl::closeTcCursor

int
NdbQueryImpl::sendClose(int nodeId)
{
  m_pendingStreams = 0;
  assert (m_tcState == FetchMore);
  m_tcState = Fetching;

  Ndb& ndb = *m_transaction.getNdb();
  NdbApiSignal tSignal(&ndb);
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  ScanNextReq * const scanNextReq = CAST_PTR(ScanNextReq, tSignal.getDataPtrSend());

  assert (m_scanTransaction);
  const Uint64 transId = m_scanTransaction->getTransactionId();

  scanNextReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
  scanNextReq->stopScan = true;
  scanNextReq->transId1 = (Uint32) transId;
  scanNextReq->transId2 = (Uint32) (transId >> 32);
  tSignal.setLength(ScanNextReq::SignalLength);

  TransporterFacade* tp = ndb.theImpl->m_transporter_facade;
  return tp->sendSignal(&tSignal, nodeId);

} // NdbQueryImpl::sendClose()


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
  m_batchBuffer(NULL),
  m_resultBuffer(NULL),
  m_resultRef(NULL),
  m_isRowNull(true),
  m_ndbRecord(NULL),
  m_read_mask(NULL),
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

NdbQueryOperationImpl::~NdbQueryOperationImpl()
{
  // We expect ::postFetchRelease to have deleted fetch related structures when fetch completed.
  // Either by fetching through last row, or calling ::close() which forcefully terminates fetch
  assert (m_batchBuffer == NULL);
  assert (m_resultStreams == NULL);
  assert (m_firstRecAttr == NULL);
} //NdbQueryOperationImpl::~NdbQueryOperationImpl()

/**
 * Release what we want need anymore after last available row has been 
 * returned from datanodes.
 */ 
void
NdbQueryOperationImpl::postFetchRelease()
{
  if (m_batchBuffer) {
#ifndef NDEBUG // Buffer overrun check activated.
    { const size_t bufLen = m_batchByteSize*m_queryImpl.getParallelism();
      assert(m_batchBuffer[bufLen+0] == 'a' &&
             m_batchBuffer[bufLen+1] == 'b' &&
             m_batchBuffer[bufLen+2] == 'c' &&
             m_batchBuffer[bufLen+3] == 'd');
    }
#endif
    delete[] m_batchBuffer;
    m_batchBuffer = NULL;
  }

  if (m_resultStreams) {
    for(Uint32 i = 0; i<getQuery().getParallelism(); i ++){
      delete m_resultStreams[i];
    }
    delete[] m_resultStreams;
    m_resultStreams = NULL;
  }

  Ndb* const ndb = m_queryImpl.getNdbTransaction().getNdb();
  NdbRecAttr* recAttr = m_firstRecAttr;
  while (recAttr != NULL) {
    NdbRecAttr* saveRecAttr = recAttr;
    recAttr = recAttr->next();
    ndb->releaseRecAttr(saveRecAttr);
  }
  m_firstRecAttr = NULL;

  // Set API exposed info to indicate NULL-row
  m_isRowNull = true;
  if (m_resultRef!=NULL) {
    *m_resultRef = NULL;
  }
} //NdbQueryOperationImpl::postFetchRelease()


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

Uint32 NdbQueryOperationImpl::countAllChildOperations() const
{
  Uint32 childs = 0;

  for (unsigned i = 0; i < getNoOfChildOperations(); i++)
    childs += 1 + getChildOperation(i).countAllChildOperations();

  return childs;
}


NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const char* anAttrName,
                            char* resultBuffer)
{
  const NdbColumnImpl* const column 
    = m_operationDef.getTable().getColumn(anAttrName);
  if(unlikely(column==NULL)){
    getQuery().setErrorCodeAbort(Err_UnknownColumn);
    return NULL;
  } else {
    return getValue(*column, resultBuffer);
  }
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            Uint32 anAttrId, 
                            char* resultBuffer)
{
  const NdbColumnImpl* const column 
    = m_operationDef.getTable().getColumn(anAttrId);
  if(unlikely(column==NULL)){
    getQuery().setErrorCodeAbort(Err_UnknownColumn);
    return NULL;
  } else {
    return getValue(*column, resultBuffer);
  }
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const NdbColumnImpl& column, 
                            char* resultBuffer)
{
  if (unlikely(getQuery().m_state != NdbQueryImpl::Defined)) {
    int state = getQuery().m_state;
    assert (state >= NdbQueryImpl::Initial && state < NdbQueryImpl::Destructed);

    if (state == NdbQueryImpl::Failed) 
      getQuery().setErrorCode(QRY_IN_ERROR_STATE);
    else
      getQuery().setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return NULL;
  }
  Ndb* const ndb = getQuery().getNdbTransaction().getNdb();
  NdbRecAttr* const recAttr = ndb->getRecAttr();
  if(unlikely(recAttr == NULL)) {
    getQuery().setErrorCodeAbort(Err_MemoryAlloc);
    return NULL;
  }
  if(unlikely(recAttr->setup(&column, resultBuffer))) {
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

int
NdbQueryOperationImpl::setResultRowBuf (
                       const NdbRecord *rec,
                       char* resBuffer,
                       const unsigned char* result_mask)
{
  if (unlikely(getQuery().m_state != NdbQueryImpl::Defined)) {
    int state = getQuery().m_state;
    assert (state >= NdbQueryImpl::Initial && state < NdbQueryImpl::Destructed);

    if (state == NdbQueryImpl::Failed) 
      getQuery().setErrorCode(QRY_IN_ERROR_STATE);
    else
      getQuery().setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return -1;
  }
  if (rec->tableId != 
      static_cast<Uint32>(m_operationDef.getTable().getTableId())){
    /* The key_record and attribute_record in primary key operation do not 
       belong to the same table.*/
    getQuery().setErrorCode(Err_DifferentTabForKeyRecAndAttrRec);
    return -1;
  }
  if (unlikely(m_ndbRecord != NULL)) {
    getQuery().setErrorCode(QRY_RESULT_ROW_ALREADY_DEFINED);
    return -1;
  }
  m_ndbRecord = rec;
  m_read_mask = result_mask;
  m_resultBuffer = resBuffer;
  assert(m_batchBuffer==NULL);
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
NdbQueryOperationImpl::fetchRecAttrResults(Uint32 streamNo)
{
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
NdbQueryOperationImpl::updateChildResult(Uint32 streamNo, Uint32 rowNo)
{
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
    if (m_firstRecAttr != NULL) {
      fetchRecAttrResults(streamNo);
    }
    if (m_ndbRecord != NULL) {
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

static bool isSetInMask(const unsigned char* mask, int bitNo)
{
  return mask[bitNo>>3] & 1<<(bitNo&7);
}

int
NdbQueryOperationImpl::serializeProject(Uint32Buffer& attrInfo) const
{
  size_t startPos = attrInfo.getSize();
  attrInfo.append(0U);  // Temp write firste 'length' word, update later

  /**
   * If the columns in the projections are specified as 
   * in NdbRecord format, attrId are assumed to be ordered ascending.
   * In this form the projection spec. can be packed as
   * a single bitmap.
   */
  if (m_ndbRecord != NULL) {
    Bitmask<MAXNROFATTRIBUTESINWORDS> readMask;
    Uint32 requestedCols= 0;
    Uint32 maxAttrId= 0;

    for (Uint32 i= 0; i<m_ndbRecord->noOfColumns; i++)
    {
      const NdbRecord::Attr *col= &m_ndbRecord->columns[i];
      Uint32 attrId= col->attrId;

      if (m_read_mask == NULL || isSetInMask(m_read_mask, i))
      { if (attrId > maxAttrId)
          maxAttrId= attrId;

        readMask.set(attrId);
        requestedCols++;
      }
    }

    // Test for special case, get all columns:
    if (requestedCols == (unsigned)m_operationDef.getTable().getNoOfColumns()) {
      Uint32 ah;
      AttributeHeader::init(&ah, AttributeHeader::READ_ALL, requestedCols);
      attrInfo.append(ah);
    } else if (requestedCols > 0) {
      /* Serialize projection as a bitmap.*/
      const Uint32 wordCount = 1+maxAttrId/32; // Size of mask.
      Uint32* dst = attrInfo.alloc(wordCount+1);
      AttributeHeader::init(dst, 
                            AttributeHeader::READ_PACKED, 4*wordCount);
      memcpy(dst+1, &readMask, 4*wordCount);
    }
  } // if (m_ndbRecord...)

  /** Projection is specified in RecAttr format.
   *  This may also be combined with the NdbRecord format.
   */
  const NdbRecAttr* recAttr = m_firstRecAttr;
  /* Serialize projection as a list of Attribute id's.*/
  while (recAttr) {
    Uint32 ah;
    AttributeHeader::init(&ah,
                          recAttr->attrId(),
                          0);
    attrInfo.append(ah);
    recAttr = recAttr->next();
  }

  bool withCorrelation = getRoot().getQueryDef().isScanQuery();
  if (withCorrelation) {
    Uint32 ah;
    AttributeHeader::init(&ah, AttributeHeader::READ_ANY_VALUE, 0);
    attrInfo.append(ah);
  }

  // Size of projection in words.
  size_t length = attrInfo.getSize() - startPos - 1 ;
  attrInfo.put(startPos, length);
  return 0;
} // NdbQueryOperationImpl::serializeProject


int NdbQueryOperationImpl::serializeParams(const constVoidPtr paramValues[])
{
  if (unlikely(paramValues == NULL))
  {
    return QRY_NEED_PARAMETER;
  }

  const NdbQueryOperationDefImpl& def = getQueryOperationDef();
  for (Uint32 i=0; i<def.getNoOfParameters(); i++)
  {
    const NdbParamOperandImpl& paramDef = def.getParameter(i);
    const constVoidPtr paramValue = paramValues[paramDef.getParamIx()];
    if (unlikely(paramValue == NULL))  // FIXME: May also indicate a NULL value....
    {
      return QRY_NEED_PARAMETER;
    }

    /**
     *  Add parameter value to serialized data.
     *  Each value has a Uint32 length field (in bytes), followed by
     *  the actuall value. Allocation is in Uint32 units with unused bytes
     *  zero padded.
     **/
    Uint32 len = paramDef.getSizeInBytes(paramValue);
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
                          0);
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
  const int error = serializeProject(attrInfo);
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

  // Parameter values was appended to AttrInfo, shrink param buffer
  // to reduce memory footprint.
  m_params.releaseExtend();

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
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI()" << endl;
  }
  bool ret = false;
  NdbQueryOperationImpl& root = getRoot();
  NdbResultStream* rootStream = NULL;

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
    rootStream = root.m_resultStreams[streamNo];
    rootStream->m_pendingResults--;
    if (rootStream->isBatchComplete()) {
      m_queryImpl.buildChildTupleLinks(streamNo);
      /* nextResult() will later move it from m_fullStreams to m_applStreams
       * under mutex protection.*/
      if (rootStream->m_receiver.hasResults()) {
        m_queryImpl.m_fullStreams.push(*rootStream);
      }
      // Don't awake before we have data, or entire query batch completed.
      ret = rootStream->m_receiver.hasResults() || root.isBatchComplete();
    }
  } else { // Lookup query
    // The root operation is a lookup.
    NdbResultStream* const resultStream = m_resultStreams[0];
    resultStream->m_receiver.execTRANSID_AI(ptr, len);
    resultStream->m_transidAICount++;

    /* The root counts rows for all descendants also. (Like scan queries) */
    rootStream = root.m_resultStreams[0];
    rootStream->m_pendingResults--;

    if (rootStream->isBatchComplete()) {
      /* If roo stream is complete, check if the query is also 
       * complete.
       */
      ret = m_queryImpl.countPendingStreams(-1);
    }
  } // end lookup

  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI(): returns:" << ret
           << ", rootStream {" << *rootStream << "}"
           << ", *this=" << *this <<  endl;
  }
  return ret;
} //NdbQueryOperationImpl::execTRANSID_AI


bool 
NdbQueryOperationImpl::execTCKEYREF(NdbApiSignal* aSignal){
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTCKEYREF()" <<  endl;
  }

  /* The SPJ block does not forward TCKEYREFs for trees with scan roots.*/
  assert(!getQueryDef().isScanQuery());

  NdbResultStream* const rootStream = getRoot().m_resultStreams[0];

  // Compensate for childs results not produced.
  // (TCKEYCONF assumed all child results to be materialized)
  int childs = countAllChildOperations();
  rootStream->m_pendingResults -= childs+1;

  bool ret = false;
  if (rootStream->isBatchComplete()) { 
    /* The stream is complete, check if the query is also complete. */
    ret = m_queryImpl.countPendingStreams(-1);
  } 

  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTCKEYREF(): returns:" << ret
           << ", rootStream {" << *rootStream << "}"
           << ", *this=" << *this <<  endl;
  }
  return ret;
} //NdbQueryOperationImpl::execTCKEYREF

bool
NdbQueryOperationImpl::execSCAN_TABCONF(Uint32 tcPtrI, 
                                        Uint32 rowCount,
                                        NdbReceiver* receiver)
{
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF()" << endl;
  }
  // For now, only the root operation may be a scan.
  assert(&getRoot() == this);
  assert(m_operationDef.isScanOperation());
  Uint32 streamNo;
  // Find stream number.
  for(streamNo = 0; 
      streamNo<getQuery().getParallelism() && 
        &getRoot().m_resultStreams[streamNo]
        ->m_receiver != receiver; 
      streamNo++);
  assert(streamNo<getQuery().getParallelism());

  NdbResultStream& resultStream = *m_resultStreams[streamNo];
  assert(resultStream.m_pendingConf);
  resultStream.m_pendingConf = false;
  resultStream.m_pendingResults += rowCount;

  resultStream.m_receiver.m_tcPtrI = tcPtrI;  // Handle for SCAN_NEXTREQ, RNIL -> EOF
  if (tcPtrI != RNIL) {
    m_queryImpl.m_tcState = NdbQueryImpl::FetchMore;
  }

  if(traceSignals){
    ndbout << "  resultStream(root) {" << resultStream << "}" << endl;
  }

  bool ret = false;
  if (resultStream.isBatchComplete()) {
    /* This stream is now complete*/
    m_queryImpl.countPendingStreams(-1);
    m_queryImpl.buildChildTupleLinks(streamNo);
    /* nextResult() will later move it from m_fullStreams to m_applStreams
     * under mutex protection.*/
    if (resultStream.m_receiver.hasResults()) {
      m_queryImpl.m_fullStreams.push(resultStream);
    }
    // Don't awake before we have data, or query batch completed.
    ret = resultStream.m_receiver.hasResults() || isBatchComplete();
  }
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF():, returns:" << ret
           << ", tcPtrI=" << tcPtrI << " rowCount=" << rowCount 
           << " *this=" << *this << endl;
  }
  return ret;
} //NdbQueryOperationImpl::execSCAN_TABCONF

void 
NdbQueryOperationImpl::buildChildTupleLinks(Uint32 streamNo)
{
  NdbResultStream& resultStream = *m_resultStreams[streamNo];
  /* Now we have received all tuples for all operations. 
   * Set correct #rows received in the NdbReceiver.
   */
  resultStream.m_receiver.m_result_rows = resultStream.m_transidAICount;

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
  assert(m_resultStreams!=NULL);
  for(Uint32 i = 0; i < m_queryImpl.getParallelism(); i++){
    if(!m_resultStreams[i]->isBatchComplete()){
      return false;
    }
  }
  return true;
}


const NdbReceiver& 
NdbQueryOperationImpl::getReceiver(Uint32 recNo) const {
  assert(recNo<getQuery().getParallelism());
  assert(m_resultStreams!=NULL);
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
    out << "  m_resultStream[" << i << "]{" << *op.m_resultStreams[i] << "}";
  }
  out << " m_isRowNull " << op.m_isRowNull;
  out << " ]";
  return out;
}

NdbOut& operator<<(NdbOut& out, const NdbResultStream& stream){
  out << " m_transidAICount: " << stream.m_transidAICount;
  out << " m_pendingResults: " << stream.m_pendingResults;
  out << " m_pendingConf " << stream.m_pendingConf;
  return out;
}

 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;
template class Vector<NdbResultStream::TupleIdMap::Pair>;
