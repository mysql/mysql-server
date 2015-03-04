/*
   Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>
#include <NdbDictionary.hpp>
#include <NdbIndexScanOperation.hpp>
#include "NdbQueryBuilder.hpp"
#include "NdbQueryOperation.hpp"
#include "API.hpp"
#include "NdbQueryBuilderImpl.hpp"
#include "NdbQueryOperationImpl.hpp"
#include "NdbInterpretedCode.hpp"

#include <signaldata/TcKeyReq.hpp>
#include <signaldata/TcKeyRef.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/QueryTree.hpp>
#include <signaldata/DbspjErr.hpp>

#include "AttributeHeader.hpp"

#include <Bitmask.hpp>

#if 0
#define DEBUG_CRASH() assert(false)
#else
#define DEBUG_CRASH()
#endif

/** To prevent compiler warnings about variables that are only used in asserts
 * (when building optimized version).
 */
#define UNUSED(x) ((void)(x))

// To force usage of SCAN_NEXTREQ even for small scans resultsets:
// - '0' is default (production) value
// - '4' is normally a good value for testing batch related problems
static const int enforcedBatchSize = 0;

// Use double buffered ResultSets, may later change 
// to be more adaptive based on query type
static const bool useDoubleBuffers = true;

/* Various error codes that are not specific to NdbQuery. */
static const int Err_TupleNotFound = 626;
static const int Err_FalsePredicate = 899;
static const int Err_MemoryAlloc = 4000;
static const int Err_SendFailed = 4002;
static const int Err_FunctionNotImplemented = 4003;
static const int Err_UnknownColumn = 4004;
static const int Err_ReceiveTimedOut = 4008;
static const int Err_NodeFailCausedAbort = 4028;
static const int Err_ParameterError = 4118;
static const int Err_SimpleDirtyReadFailed = 4119;
static const int Err_WrongFieldLength = 4209;
static const int Err_ReadTooMuch = 4257;
static const int Err_InvalidRangeNo = 4286;
static const int Err_DifferentTabForKeyRecAndAttrRec = 4287;
static const int Err_KeyIsNULL = 4316;
static const int Err_FinaliseNotCalled = 4519;
static const int Err_InterpretedCodeWrongTab = 4524;

/* A 'void' index for a tuple in internal parent / child correlation structs .*/
static const Uint16 tupleNotFound = 0xffff;

/** Set to true to trace incomming signals.*/
static const bool traceSignals = false;

enum
{
  /**
   * Set NdbQueryOperationImpl::m_parallelism to this value to indicate that
   * scan parallelism should be adaptive.
   */
  Parallelism_adaptive = 0xffff0000,

  /**
   * Set NdbQueryOperationImpl::m_parallelism to this value to indicate that
   * all fragments should be scanned in parallel.
   */
  Parallelism_max = 0xffff0001
};

/**
 * A class for accessing the correlation data at the end of a tuple (for 
 * scan queries). These data have the following layout:
 *
 * Word 0: AttributeHeader
 * Word 1, upper halfword: tuple id of parent tuple.
 * Word 1, lower halfword: tuple id of this tuple.
 * Word 2: Id of receiver for root operation (where the ancestor tuple of this
 *         tuple will go).
 *
 * Both tuple identifiers are unique within this batch and root fragment.
 * With these identifiers, it is possible to relate a tuple to its parent and 
 * children. That way, results for child operations can be updated correctly
 * when the application iterates over the results of the root scan operation.
 */
class TupleCorrelation
{
public:
  static const Uint32 wordCount = 1;

  explicit TupleCorrelation()
  : m_correlation((tupleNotFound<<16) | tupleNotFound)
  {}

  /** Conversion to/from Uint32 to store/fetch from buffers */
  explicit TupleCorrelation(Uint32 val)
  : m_correlation(val)
  {}

  Uint32 toUint32() const 
  { return m_correlation; }

  Uint16 getTupleId() const
  { return m_correlation & 0xffff;}

  Uint16 getParentTupleId() const
  { return m_correlation >> 16;}

private:
  Uint32 m_correlation;
}; // class TupleCorrelation

class CorrelationData
{
public:
  static const Uint32 wordCount = 3;

  explicit CorrelationData(const Uint32* tupleData, Uint32 tupleLength):
    m_corrPart(tupleData + tupleLength - wordCount)
  {
    assert(tupleLength >= wordCount);
    assert(AttributeHeader(m_corrPart[0]).getAttributeId() 
           == AttributeHeader::CORR_FACTOR64);
    assert(AttributeHeader(m_corrPart[0]).getByteSize() == 2*sizeof(Uint32));
    assert(getTupleCorrelation().getTupleId()<tupleNotFound);
    assert(getTupleCorrelation().getParentTupleId()<tupleNotFound);
  }

  Uint32 getRootReceiverId() const
  { return m_corrPart[2];}

  const TupleCorrelation getTupleCorrelation() const
  { return TupleCorrelation(m_corrPart[1]); }

private:
  const Uint32* const m_corrPart;
}; // class CorrelationData
 
/**
 * If a query has a scan operation as its root, then that scan will normally 
 * read from several fragments of its target table. Each such root fragment
 * scan, along with any child lookup operations that are spawned from it,
 * runs independently, in the sense that:
 * - The API will know when it has received all data from a fragment for a 
 *   given batch and all child operations spawned from it.
 * - When one fragment is complete (for a batch) the API will make these data
 *   avaliable to the application, even if other fragments are not yet complete.
 * - The tuple identifiers that are used for matching children with parents are
 *   only guaranteed to be unique within one batch, operation, and root 
 *   operation fragment. Tuples derived from different root fragments must 
 *   thus be kept apart.
 * 
 * This class manages the state of one such read operation, from one particular
 * fragment of the target table of the root operation. If the root operation
 * is a lookup, then there will be only one instance of this class.
 */
class NdbRootFragment {
public:
  /** Build hash map for mapping from root receiver id to NdbRootFragment 
   * instance.*/
  static void buildReciverIdMap(NdbRootFragment* frags, 
                                Uint32 noOfFrags);

  /** Find NdbRootFragment instance corresponding to a given root receiver id.*/
  static NdbRootFragment* receiverIdLookup(NdbRootFragment* frags, 
                                           Uint32 noOfFrags, 
                                           Uint32 receiverId);

  explicit NdbRootFragment();

  ~NdbRootFragment();

  /**
   * Initialize object.
   * @param query Enclosing query.
   * @param fragNo This object manages state for reading from the fragNo'th 
   * fragment that the root operation accesses.
   */
  void init(NdbQueryImpl& query, Uint32 fragNo); 

  static void clear(NdbRootFragment* frags, Uint32 noOfFrags);

  Uint32 getFragNo() const
  { return m_fragNo; }

  /**
   * Prepare for receiving another batch of results.
   */
  void prepareNextReceiveSet();

  bool hasRequestedMore() const;

  /**
   * Prepare for reading another batch of results.
   */
  void grabNextResultSet();                     // Need mutex lock

  bool hasReceivedMore() const;                 // Need mutex lock

  void setReceivedMore();                       // Need mutex lock

  void incrOutstandingResults(Int32 delta)
  {
    if (traceSignals) {
      ndbout << "incrOutstandingResults: " << m_outstandingResults
             << ", with: " << delta
             << endl;
    }
    m_outstandingResults += delta;
    assert(!(m_confReceived && m_outstandingResults<0));
  }

  void throwRemainingResults()
  {
    if (traceSignals) {
      ndbout << "throwRemainingResults: " << m_outstandingResults
             << endl;
    }
    m_outstandingResults = 0;
    m_confReceived = true; 
    postFetchRelease();
  }

  void setConfReceived(Uint32 tcPtrI);

  /** 
   * The root operation will read from a number of fragments of a table.
   * This method checks if all results for the current batch has been 
   * received for a given fragment. This includes both results for the root
   * operation and any child operations. Note that child operations may access
   * other fragments; the fragment number only refers to what 
   * the root operation does.
   *
   * @return True if current batch is complete for this fragment.
   */
  bool isFragBatchComplete() const
  { 
    assert(m_fragNo!=voidFragNo);
    return m_confReceived && m_outstandingResults==0; 
  }

  /**
   * Get the result stream that handles results derived from this root 
   * fragment for a particular operation.
   * @param operationNo The id of the operation.
   * @return The result stream for this root fragment.
   */
  NdbResultStream& getResultStream(Uint32 operationNo) const;

  NdbResultStream& getResultStream(const NdbQueryOperationImpl& op) const
  { return getResultStream(op.getQueryOperationDef().getOpNo()); }

  Uint32 getReceiverId() const;
  Uint32 getReceiverTcPtrI() const;

  /**
   * @return True if there are no more batches to be received for this fragment.
   */
  bool finalBatchReceived() const;

  /**
   * @return True if there are no more results from this root fragment (for 
   * the current batch).
   */
  bool isEmpty() const;

  /** 
   * This method is used for marking which streams belonging to this
   * NdbRootFragment which has remaining batches for a sub scan
   * instantiated from the current batch of its parent operation.
   */
  void setRemainingSubScans(Uint32 nodeMask)
  { 
    m_remainingScans = nodeMask;
  }

  /** Release resources after last row has been returned */
  void postFetchRelease();

private:
  /** No copying.*/
  NdbRootFragment(const NdbRootFragment&);
  NdbRootFragment& operator=(const NdbRootFragment&);

  STATIC_CONST( voidFragNo = 0xffffffff);

  /** Enclosing query.*/
  NdbQueryImpl* m_query;

  /** Number of the root operation fragment.*/
  Uint32 m_fragNo;

  /** For processing results originating from this root fragment (Array of).*/
  NdbResultStream* m_resultStreams;

  /**
   * Number of requested (pre-)fetches which has either not completed 
   * from datanodes yet, or which are completed, but not consumed.
   * (Which implies they are also counted in m_availResultSets)
   */
  Uint32 m_pendingRequests;

  /**
   * Number of available 'm_pendingRequests' ( <= m_pendingRequests)
   * which has been completely received. Will be made available
   * for reading by calling ::grabNextResultSet() 
   */
  Uint32 m_availResultSets;   // Need mutex

  /**
   * The number of outstanding TCKEYREF or TRANSID_AI messages to receive
   * for the fragment. This includes both messages related to the
   * root operation and any descendant operation that was instantiated as
   * a consequence of tuples found by the root operation.
   * This number may temporarily be negative if e.g. TRANSID_AI arrives 
   * before SCAN_TABCONF. 
   */
  Int32 m_outstandingResults;

  /**
   * This is an array with one element for each fragment that the root
   * operation accesses (i.e. one for a lookup, all for a table scan).
   *
   * Each element is true iff a SCAN_TABCONF (for that fragment) or 
   * TCKEYCONF message has been received
   */
  bool m_confReceived;

  /**
   * A bitmask of operation id's for which we will receive more
   * ResultSets in a NEXTREQ.
   */
  Uint32 m_remainingScans;

  /** 
   * Used for implementing a hash map from root receiver ids to a 
   * NdbRootFragment instance. m_idMapHead is the index of the first
   * NdbRootFragment in the m_fragNo'th hash bucket. 
   */
  int m_idMapHead;

  /** 
   * Used for implementing a hash map from root receiver ids to a 
   * NdbRootFragment instance. m_idMapNext is the index of the next
   * NdbRootFragment in the same hash bucket as this one. 
   */
  int m_idMapNext;
}; //NdbRootFragment

/**
 * 'class NdbResultSet' is a helper for 'class NdbResultStream'.
 *  It manages the buffers which rows are received into and
 *  read from.
 */
class NdbResultSet
{
  friend class NdbResultStream;

public:
  explicit NdbResultSet();

  void init(NdbQueryImpl& query,
            Uint32 maxRows, Uint32 bufferSize);

  void prepareReceive(NdbReceiver& receiver)
  {
    m_rowCount = 0;
    receiver.prepareReceive(m_buffer);
  }

  Uint32 getRowCount() const
  { return m_rowCount; }

private:
  /** No copying.*/
  NdbResultSet(const NdbResultSet&);
  NdbResultSet& operator=(const NdbResultSet&);

  /** The buffers which we receive the results into */
  NdbReceiverBuffer* m_buffer;

  /** Array of TupleCorrelations for all rows in m_buffer */
  TupleCorrelation* m_correlations;

  /** The current #rows in 'm_buffer'.*/
  Uint32 m_rowCount;

}; // class NdbResultSet

/** 
 * This class manages the subset of result data for one operation that is 
 * derived from one fragment of the root operation. Note that the result tuples
 * may come from any fragment, but they all have initial ancestors from the 
 * same fragment of the root operation.  
 * For each operation there will thus be one NdbResultStream for each fragment
 * that the root operation reads from (one in the case of lookups.)
 * This class has an NdbReceiver object for processing tuples as well as 
 * structures for correlating child and parent tuples.
 */
class NdbResultStream {
public:

  /**
   * @param operation The operation for which we will receive results.
   * @param rootFragNo 0..n-1 when the root operation reads from n fragments.
   */
  explicit NdbResultStream(NdbQueryOperationImpl& operation,
                           NdbRootFragment& rootFrag);

  ~NdbResultStream();

  /** 
   * Prepare for receiving first results. 
   */
  void prepare();

  /** Prepare for receiving next batch of scan results. */
  void prepareNextReceiveSet();
    
  NdbReceiver& getReceiver()
  { return m_receiver; }

  const NdbReceiver& getReceiver() const
  { return m_receiver; }

  const char* getCurrentRow()
  { return m_receiver.getCurrentRow(); }

  /**
   * Process an incomming tuple for this stream. Extract parent and own tuple 
   * ids and pass it on to m_receiver.
   *
   * @param ptr buffer holding tuple.
   * @param len buffer length.
   */
  void execTRANSID_AI(const Uint32 *ptr, Uint32 len,
                      TupleCorrelation correlation);

  /**
   * A complete batch has been received for a fragment on this NdbResultStream,
   * Update whatever required before the appl. are allowed to navigate the result.
   * @return true if node and all its siblings have returned all rows.
   */ 
  bool prepareResultSet(Uint32 remainingScans);

  /**
   * Navigate within the current ResultSet to resp. first and next row.
   * For non-parent operations in the pushed query, navigation is with respect
   * to any preceding parents which results in this ResultSet depends on.
   * Returns either the tupleNo within TupleSet[] which we navigated to, or 
   * tupleNotFound().
   */
  Uint16 firstResult();
  Uint16 nextResult();

  /** 
   * Returns true if last row matching the current parent tuple has been 
   * consumed.
   */
  bool isEmpty() const
  { return m_iterState == Iter_finished; }

  /** 
   * This method 
   * returns true if this result stream holds the last batch of a sub scan.
   * This means that it is the last batch of the scan that was instantiated 
   * from the current batch of its parent operation.
   */
  bool isSubScanComplete(Uint32 remainingScans) const
  { 
    /**
     * Find the node number seen by the SPJ block. Since a unique index
     * operation will have two distincts nodes in the tree used by the
     * SPJ block, this number may be different from 'opNo'.
     */
    const Uint32 internalOpNo = m_operation.getInternalOpNo();

    const bool complete = !((remainingScans >> internalOpNo) & 1);
    return complete; 
  }

  bool isScanQuery() const
  { return (m_properties & Is_Scan_Query); }

  bool isScanResult() const
  { return (m_properties & Is_Scan_Result); }

  bool isInnerJoin() const
  { return (m_properties & Is_Inner_Join); }

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbResultStream&);

  /**
   * TupleSet contain two logically distinct set of information:
   *
   *  - Child/Parent correlation set required to correlate
   *    child tuples with its parents. Child/Tuple pairs are indexed
   *    by tuple number which is the same as the order in which tuples
   *    appear in the NdbReceiver buffers.
   *
   *  - A HashMap on 'm_parentId' used to locate tuples correlated
   *    to a parent tuple. Indexes by hashing the parentId such that:
   *     - [hash(parentId)].m_hash_head will then index the first
   *       TupleSet entry potential containing the parentId to locate.
   *     - .m_hash_next in the indexed TupleSet may index the next TupleSet 
   *       to considder.
   *       
   * Both the child/parent correlation set and the parentId HashMap has been
   * folded into the same structure on order to reduce number of objects 
   * being dynamically allocated. 
   * As an advantage this results in an autoscaling of the hash bucket size .
   *
   * Structure is only present if 'isScanQuery'
   */
  class TupleSet {
  public:
                        // Tuple ids are unique within this batch and stream
    Uint16 m_parentId;  // Id of parent tuple which this tuple is correlated with
    Uint16 m_tupleId;   // Id of this tuple

    Uint16 m_hash_head; // Index of first item in TupleSet[] matching a hashed parentId.
    Uint16 m_hash_next; // 'next' index matching 

    bool   m_skip;      // Skip this tuple in result processing for now

    /** If the n'th bit is set, then a matching tuple for the n,th child has been seen. 
     * This information is needed when generating left join tuples for those tuples
     * that had no matching children.*/
    Bitmask<(NDB_SPJ_MAX_TREE_NODES+31)/32> m_hasMatchingChild;

    explicit TupleSet() : m_hash_head(tupleNotFound)
    {}

  private:
    /** No copying.*/
    TupleSet(const TupleSet&);
    TupleSet& operator=(const TupleSet&);
  };

private:
  /**
   * This stream handles results derived from specified 
   * m_rootFrag of the root operation.
   */
  const NdbRootFragment& m_rootFrag;
 
  /** Operation to which this resultStream belong.*/
  NdbQueryOperationImpl& m_operation;

  /** ResultStream for my parent operation, or NULL if I am root */
  NdbResultStream* const m_parent;

  const enum properties
  {
    Is_Scan_Query = 0x01,
    Is_Scan_Result = 0x02,
    Is_Inner_Join = 0x10
  } m_properties;

  /** The receiver object that unpacks transid_AI messages.*/
  NdbReceiver m_receiver;

  /**
   * ResultSets are received into and read from this stream,
   * possibly doublebuffered,
   */
  NdbResultSet m_resultSets[2];
  Uint32 m_read;  // We read from m_resultSets[m_read]
  Uint32 m_recv;  // We receive into m_resultSets[m_recv]

  /** This is the state of the iterator used by firstResult(), nextResult().*/
  enum
  {
    /** The first row has not been fetched yet.*/
    Iter_notStarted,
    /** Is iterating the ResultSet, (implies 'm_currentRow!=tupleNotFound').*/
    Iter_started,  
    /** Last row for current ResultSet has been returned.*/
    Iter_finished
  } m_iterState;

  /**
   * Tuple id of the current tuple, or 'tupleNotFound'
   * if Iter_notStarted or Iter_finished. 
   */
  Uint16 m_currentRow;
  
  /** Max #rows which this stream may recieve in its TupleSet structures */
  Uint32 m_maxRows;

  /** TupleSet contains the correlation between parent/childs */
  TupleSet* m_tupleSet;

  void buildResultCorrelations();

  Uint16 getTupleId(Uint16 tupleNo) const
  { return (m_tupleSet) ? m_tupleSet[tupleNo].m_tupleId : 0; }

  Uint16 getCurrentTupleId() const
  { return (m_currentRow==tupleNotFound) ? tupleNotFound : getTupleId(m_currentRow); }

  Uint16 findTupleWithParentId(Uint16 parentId) const;

  Uint16 findNextTuple(Uint16 tupleNo) const;

  /** No copying.*/
  NdbResultStream(const NdbResultStream&);
  NdbResultStream& operator=(const NdbResultStream&);
}; //class NdbResultStream

//////////////////////////////////////////////
/////////  NdbBulkAllocator methods ///////////
//////////////////////////////////////////////

NdbBulkAllocator::NdbBulkAllocator(size_t objSize)
  :m_objSize(objSize),
   m_maxObjs(0),
   m_buffer(NULL),
   m_nextObjNo(0)
{}

int NdbBulkAllocator::init(Uint32 maxObjs)
{
  assert(m_buffer == NULL);
  m_maxObjs = maxObjs;
  // Add check for buffer overrun.
  m_buffer = new char[m_objSize*m_maxObjs+1];
  if (unlikely(m_buffer == NULL))
  {
    return Err_MemoryAlloc;
  }
  m_buffer[m_maxObjs * m_objSize] = endMarker;
  return 0;
}

void NdbBulkAllocator::reset(){
  // Overrun check.
  assert(m_buffer == NULL || m_buffer[m_maxObjs * m_objSize] == endMarker); 
  delete [] m_buffer;
  m_buffer = NULL;
  m_nextObjNo = 0;
  m_maxObjs = 0;
}

void* NdbBulkAllocator::allocObjMem(Uint32 noOfObjs)
{
  assert(m_nextObjNo + noOfObjs <=  m_maxObjs);
  void * const result = m_buffer+m_objSize*m_nextObjNo;
  m_nextObjNo += noOfObjs;
  return m_nextObjNo > m_maxObjs ? NULL : result;
}

///////////////////////////////////////////
/////////  NdbResultSet methods ///////////
///////////////////////////////////////////
NdbResultSet::NdbResultSet() :
  m_buffer(NULL),
  m_correlations(NULL),
  m_rowCount(0)
{}

void
NdbResultSet::init(NdbQueryImpl& query,
                   Uint32 maxRows,
                   Uint32 bufferSize)
{
  {
    NdbBulkAllocator& bufferAlloc = query.getRowBufferAlloc();
    Uint32 *buffer = reinterpret_cast<Uint32*>(bufferAlloc.allocObjMem(bufferSize));
    m_buffer = NdbReceiver::initReceiveBuffer(buffer, bufferSize, maxRows);

    if (query.getQueryDef().isScanQuery())
    {
      m_correlations = reinterpret_cast<TupleCorrelation*>
                         (bufferAlloc.allocObjMem(maxRows*sizeof(TupleCorrelation)));
    }
  }
}

//////////////////////////////////////////////
/////////  NdbResultStream methods ///////////
//////////////////////////////////////////////

NdbResultStream::NdbResultStream(NdbQueryOperationImpl& operation,
                                 NdbRootFragment& rootFrag)
:
  m_rootFrag(rootFrag),
  m_operation(operation),
  m_parent(operation.getParentOperation()
        ? &rootFrag.getResultStream(*operation.getParentOperation())
        : NULL),
  m_properties(
    (enum properties)
     ((operation.getQueryDef().isScanQuery()
       ? Is_Scan_Query : 0)
     | (operation.getQueryOperationDef().isScanOperation()
       ? Is_Scan_Result : 0)
     | (operation.getQueryOperationDef().getMatchType() != NdbQueryOptions::MatchAll
       ? Is_Inner_Join : 0))),
  m_receiver(operation.getQuery().getNdbTransaction().getNdb()),
  m_resultSets(), m_read(0xffffffff), m_recv(0),
  m_iterState(Iter_finished),
  m_currentRow(tupleNotFound),
  m_maxRows(0),
  m_tupleSet(NULL)
{};

NdbResultStream::~NdbResultStream()
{
  for (int i = static_cast<int>(m_maxRows)-1; i >= 0; i--)
  {
    m_tupleSet[i].~TupleSet();
  }
}

void
NdbResultStream::prepare()
{
  NdbQueryImpl &query = m_operation.getQuery();

  const Uint32 batchBufferSize = m_operation.getBatchBufferSize();
  if (isScanQuery())
  {
    /* Parent / child correlation is only relevant for scan type queries
     * Don't create a m_tupleSet with these correlation id's for lookups!
     */
    m_maxRows  = m_operation.getMaxBatchRows();
    m_tupleSet = 
      new (query.getTupleSetAlloc().allocObjMem(m_maxRows)) 
      TupleSet[m_maxRows];

    // Scan results may be double buffered
    m_resultSets[0].init(query, m_maxRows, batchBufferSize); 
    m_resultSets[1].init(query, m_maxRows, batchBufferSize);
  }
  else
  {
    m_maxRows = 1;
    m_resultSets[0].init(query, m_maxRows, batchBufferSize);
  }

  /* Alloc buffer for unpacked NdbRecord row */
  const Uint32 rowSize = m_operation.getRowSize();
  assert((rowSize % sizeof(Uint32)) == 0);
  char *rowBuffer = reinterpret_cast<char*>(query.getRowBufferAlloc().allocObjMem(rowSize));
  assert(rowBuffer != NULL);

  m_receiver.init(NdbReceiver::NDB_QUERY_OPERATION, &m_operation);
  m_receiver.do_setup_ndbrecord(
                          m_operation.getNdbRecord(),
                          rowBuffer,
                          false, /*read_range_no*/
                          false  /*read_key_info*/);
} //NdbResultStream::prepare

/** Locate, and return 'tupleNo', of first tuple with specified parentId.
 *  parentId == tupleNotFound is use as a special value for iterating results
 *  from the root operation in the order which they was inserted by 
 *  ::buildResultCorrelations()
 *
 *  Position of 'currentRow' is *not* updated and should be modified by callee
 *  if it want to keep the new position.
 */
Uint16
NdbResultStream::findTupleWithParentId(Uint16 parentId) const
{
  assert ((parentId==tupleNotFound) == (m_parent==NULL));

  if (likely(m_resultSets[m_read].m_rowCount>0))
  {
    if (m_tupleSet==NULL)
    {
      assert (m_resultSets[m_read].m_rowCount <= 1);
      return 0;
    }

    const Uint16 hash = (parentId % m_maxRows);
    Uint16 currentRow = m_tupleSet[hash].m_hash_head;
    while (currentRow != tupleNotFound)
    {
      assert(currentRow < m_maxRows);
      if (m_tupleSet[currentRow].m_skip == false &&
          m_tupleSet[currentRow].m_parentId == parentId)
      {
        return currentRow;
      }
      currentRow = m_tupleSet[currentRow].m_hash_next;
    }
  }
  return tupleNotFound;
} //NdbResultStream::findTupleWithParentId()


/** Locate, and return 'tupleNo', of next tuple with same parentId as currentRow
 *  Position of 'currentRow' is *not* updated and should be modified by callee
 *  if it want to keep the new position.
 */
Uint16
NdbResultStream::findNextTuple(Uint16 tupleNo) const
{
  if (tupleNo!=tupleNotFound && m_tupleSet!=NULL)
  {
    assert(tupleNo < m_maxRows);
    Uint16 parentId = m_tupleSet[tupleNo].m_parentId;
    Uint16 nextRow  = m_tupleSet[tupleNo].m_hash_next;

    while (nextRow != tupleNotFound)
    {
      assert(nextRow < m_maxRows);
      if (m_tupleSet[nextRow].m_skip == false &&
          m_tupleSet[nextRow].m_parentId == parentId)
      {
        return nextRow;
      }
      nextRow = m_tupleSet[nextRow].m_hash_next;
    }
  }
  return tupleNotFound;
} //NdbResultStream::findNextTuple()


Uint16
NdbResultStream::firstResult()
{
  Uint16 parentId = tupleNotFound;
  if (m_parent!=NULL)
  {
    parentId = m_parent->getCurrentTupleId();
    if (parentId == tupleNotFound)
    {
      m_currentRow = tupleNotFound;
      m_iterState = Iter_finished;
      return tupleNotFound;
    }
  }
    
  if ((m_currentRow=findTupleWithParentId(parentId)) != tupleNotFound)
  {
    m_iterState = Iter_started;
    const char *p = m_receiver.getRow(m_resultSets[m_read].m_buffer, m_currentRow);
    assert(p != NULL);  ((void)p);
    return m_currentRow;
  }

  m_iterState = Iter_finished;
  return tupleNotFound;
} //NdbResultStream::firstResult()

Uint16
NdbResultStream::nextResult()
{
  // Fetch next row for this stream
  if (m_currentRow != tupleNotFound &&
      (m_currentRow=findNextTuple(m_currentRow)) != tupleNotFound)
  {
    m_iterState = Iter_started;
    const char *p = m_receiver.getRow(m_resultSets[m_read].m_buffer, m_currentRow);
    assert(p != NULL);  ((void)p);
    return m_currentRow;
  }
  m_iterState = Iter_finished;
  return tupleNotFound;
} //NdbResultStream::nextResult()

/**
 * Callback when a TRANSID_AI signal (receive row) is processed.
 */
void
NdbResultStream::execTRANSID_AI(const Uint32 *ptr, Uint32 len,
                                TupleCorrelation correlation)
{
  NdbResultSet& receiveSet = m_resultSets[m_recv];
  if (isScanQuery())
  {
    /**
     * Store TupleCorrelation.
     */
    receiveSet.m_correlations[receiveSet.m_rowCount] = correlation;
  }

  m_receiver.execTRANSID_AI(ptr, len);
  receiveSet.m_rowCount++;
} // NdbResultStream::execTRANSID_AI()

/**
 * Make preparation for another batch of results to be received.
 * This NdbResultStream, and all its sibling will receive a batch
 * of results from the datanodes.
 */
void
NdbResultStream::prepareNextReceiveSet()
{
  if (isScanQuery())          // Doublebuffered ResultSet[] if isScanQuery()
  {
    m_recv = (m_recv+1) % 2;  // Receive into next ResultSet
    assert(m_recv != m_read);
  }

  m_resultSets[m_recv].prepareReceive(m_receiver);

  /**
   * If this stream will get new rows in the next batch, then so will
   * all of its descendants.
   */
  for (Uint32 childNo = 0; childNo < m_operation.getNoOfChildOperations();
       childNo++)
  {
    NdbQueryOperationImpl& child = m_operation.getChildOperation(childNo);
    m_rootFrag.getResultStream(child).prepareNextReceiveSet();
  }
} //NdbResultStream::prepareNextReceiveSet

/**
 * Make preparations for another batch of result to be read:
 *  - Advance to next NdbResultSet. (or reuse last)
 *  - Fill in parent/child result correlations in m_tupleSet[]
 *  - ... or reset m_tupleSet[] if we reuse the previous.
 *  - Apply inner/outer join filtering to remove non qualifying 
 *    rows.
 */
bool 
NdbResultStream::prepareResultSet(Uint32 remainingScans)
{
  bool isComplete = isSubScanComplete(remainingScans); //Childs with more rows

  /**
   * Prepare NdbResultSet for reading - either the next
   * 'new' received from datanodes or reuse the last as has been 
   * determined by ::prepareNextReceiveSet()
   */
  const bool newResults = (m_read != m_recv);
  m_read = m_recv;
  const NdbResultSet& readResult = m_resultSets[m_read];

  if (m_tupleSet!=NULL)
  {
    if (newResults)
    {
      buildResultCorrelations();
    }
    else
    {
      // Makes all rows in 'TupleSet' available (clear 'm_skip' flag)
      for (Uint32 tupleNo=0; tupleNo<readResult.getRowCount(); tupleNo++)
      {
        m_tupleSet[tupleNo].m_skip = false;
      }
    }
  }

  /**
   * Recursively iterate all child results depth first. 
   * Filter away any result rows which should not be visible (yet) - 
   * Either due to incomplete child batches, or the join being an 'inner join'.
   * Set result itterator state to 'before first' resultrow.
   */
  for (Uint32 childNo=0; childNo < m_operation.getNoOfChildOperations(); childNo++)
  {
    const NdbQueryOperationImpl& child = m_operation.getChildOperation(childNo);
    NdbResultStream& childStream = m_rootFrag.getResultStream(child);
    const bool allSubScansComplete = childStream.prepareResultSet(remainingScans);

    Uint32 childId = child.getQueryOperationDef().getOpNo();

    /* Condition 1) & 2) calc'ed outside loop, see comments further below: */
    const bool skipNonMatches = !allSubScansComplete ||      // 1)
                                childStream.isInnerJoin();   // 2)

    if (m_tupleSet!=NULL)
    {
      for (Uint32 tupleNo=0; tupleNo<readResult.getRowCount(); tupleNo++)
      {
        if (!m_tupleSet[tupleNo].m_skip)
        {
          Uint16 tupleId = getTupleId(tupleNo);
          if (childStream.findTupleWithParentId(tupleId)!=tupleNotFound)
            m_tupleSet[tupleNo].m_hasMatchingChild.set(childId);

          /////////////////////////////////
          //  No child matched for this row. Making parent row visible
          //  will cause a NULL (outer join) row to be produced.
          //  Skip NULL row production when:
          //    1) Some child batches are not complete; they may contain later matches.
          //    2) Join type is 'inner join', skip as no child are matching.
          //    3) A match was found in a previous batch.
          //  Condition 1) & 2) above is precalculated in 'bool skipNonMatches'
          //
          else if (skipNonMatches                                       // 1 & 2)
               ||  m_tupleSet[tupleNo].m_hasMatchingChild.get(childId)) // 3)
            m_tupleSet[tupleNo].m_skip = true;
        }
      }
    }
    isComplete &= allSubScansComplete;
  }

  // Set current position 'before first'
  m_iterState = Iter_notStarted;
  m_currentRow = tupleNotFound;

  return isComplete; 
} // NdbResultStream::prepareResultSet()


/**
 * Fill m_tupleSet[] with correlation data between parent 
 * and child tuples. The 'TupleCorrelation' is stored
 * in an array of TupleCorrelations in each ResultSet
 * by execTRANSID_AI().
 *
 * NOTE: In order to reduce work done when holding the 
 * transporter mutex, the 'TupleCorrelation' is only stored
 * in the buffer when it arrives. Later (here) we build the
 * correlation hashMap immediately before we prepare to 
 * read the NdbResultSet.
 */
void 
NdbResultStream::buildResultCorrelations()
{
  const NdbResultSet& readResult = m_resultSets[m_read];

//if (m_tupleSet!=NULL)
  {
    /* Clear the hashmap structures */
    for (Uint32 i=0; i<m_maxRows; i++)
    {
      m_tupleSet[i].m_hash_head = tupleNotFound;
    }

    /* Rebuild correlation & hashmap from 'readResult' */
    for (Uint32 tupleNo=0; tupleNo<readResult.m_rowCount; tupleNo++)
    {
      const Uint16 tupleId  = readResult.m_correlations[tupleNo].getTupleId();
      const Uint16 parentId = (m_parent!=NULL) 
                                ? readResult.m_correlations[tupleNo].getParentTupleId()
                                : tupleNotFound;

      m_tupleSet[tupleNo].m_skip     = false;
      m_tupleSet[tupleNo].m_parentId = parentId;
      m_tupleSet[tupleNo].m_tupleId  = tupleId;
      m_tupleSet[tupleNo].m_hasMatchingChild.clear();

      /* Insert into parentId-hashmap */
      const Uint16 hash = (parentId % m_maxRows);
      if (m_parent==NULL)
      {
        /* Root stream: Insert sequentially in hash_next to make it
         * possible to use ::findTupleWithParentId() and ::findNextTuple()
         * to navigate even the root operation.
         */
        /* Link into m_hash_next in order to let ::findNextTuple() navigate correctly */
        if (tupleNo==0)
          m_tupleSet[hash].m_hash_head  = tupleNo;
        else
          m_tupleSet[tupleNo-1].m_hash_next  = tupleNo;
        m_tupleSet[tupleNo].m_hash_next  = tupleNotFound;
      }
      else
      {
        /* Insert parentId in HashMap */
        m_tupleSet[tupleNo].m_hash_next = m_tupleSet[hash].m_hash_head;
        m_tupleSet[hash].m_hash_head  = tupleNo;
      }
    }
  }
} // NdbResultStream::buildResultCorrelations


///////////////////////////////////////////
////////  NdbRootFragment methods /////////
///////////////////////////////////////////
void NdbRootFragment::buildReciverIdMap(NdbRootFragment* frags, 
                                        Uint32 noOfFrags)
{
  for(Uint32 fragNo = 0; fragNo < noOfFrags; fragNo++)
  {
    const Uint32 receiverId = frags[fragNo].getReceiverId();
    /** 
     * For reasons unknow, NdbObjectIdMap shifts ids two bits to the left,
     * so we must do the opposite to get a good hash distribution.
     */
    assert((receiverId & 0x3) == 0);
    const int hash = 
      (receiverId >> 2) % noOfFrags;
    frags[fragNo].m_idMapNext = frags[hash].m_idMapHead;
    frags[hash].m_idMapHead = fragNo;
  } 
}

//static
NdbRootFragment* 
NdbRootFragment::receiverIdLookup(NdbRootFragment* frags, 
                                  Uint32 noOfFrags, 
                                  Uint32 receiverId)
{
  /** 
   * For reasons unknow, NdbObjectIdMap shifts ids two bits to the left,
   * so we must do the opposite to get a good hash distribution.
   */
  assert((receiverId  & 0x3) == 0);
  const int hash = (receiverId >> 2) % noOfFrags;
  int current = frags[hash].m_idMapHead;
  assert(current < static_cast<int>(noOfFrags));
  while (current >= 0 && frags[current].getReceiverId() != receiverId)
  {
    current = frags[current].m_idMapNext;
    assert(current < static_cast<int>(noOfFrags));
  }
  if (unlikely (current < 0))
  {
    return NULL;
  }
  else
  {
    return frags+current;
  }
}


NdbRootFragment::NdbRootFragment():
  m_query(NULL),
  m_fragNo(voidFragNo),
  m_resultStreams(NULL),
  m_pendingRequests(0),
  m_availResultSets(0),
  m_outstandingResults(0),
  m_confReceived(false),
  m_remainingScans(0xffffffff),
  m_idMapHead(-1),
  m_idMapNext(-1)
{
}

NdbRootFragment::~NdbRootFragment()
{
  assert(m_resultStreams==NULL);
}

void NdbRootFragment::init(NdbQueryImpl& query, Uint32 fragNo)
{
  assert(m_fragNo==voidFragNo);
  m_query = &query;
  m_fragNo = fragNo;

  m_resultStreams = reinterpret_cast<NdbResultStream*>
     (query.getResultStreamAlloc().allocObjMem(query.getNoOfOperations()));
  assert(m_resultStreams!=NULL);

  for (unsigned opNo=0; opNo<query.getNoOfOperations(); opNo++) 
  {
    NdbQueryOperationImpl& op = query.getQueryOperation(opNo);
    new (&m_resultStreams[opNo]) NdbResultStream(op,*this);
    m_resultStreams[opNo].prepare();
  }
}

/**
 * Release what we want need anymore after last available row has been 
 * returned from datanodes.
 */ 
void
NdbRootFragment::postFetchRelease()
{
  if (m_resultStreams != NULL)
  { 
    for (unsigned opNo=0; opNo<m_query->getNoOfOperations(); opNo++) 
    {
      m_resultStreams[opNo].~NdbResultStream();
    }
  }
  /**
   * Don't 'delete' the object as it was in-place constructed from
   * ResultStreamAlloc'ed memory. Memory is released by
   * ResultStreamAlloc::reset().
   */
  m_resultStreams = NULL;
}

NdbResultStream&
NdbRootFragment::getResultStream(Uint32 operationNo) const
{
  assert(m_resultStreams);
  return m_resultStreams[operationNo];
}

/**
 * Throw any pending ResultSets from specified rootFrags[]
 */
//static
void NdbRootFragment::clear(NdbRootFragment* rootFrags, Uint32 noOfFrags)
{
  if (rootFrags != NULL)
  {
    for (Uint32 fragNo = 0; fragNo < noOfFrags; fragNo++)
    {
      rootFrags[fragNo].m_pendingRequests = 0;
      rootFrags[fragNo].m_availResultSets = 0;
    }
  }
}

/**
 * Check if there has been requested more ResultSets from
 * this fragment which has not been consumed yet.
 * (This is also a candicate check for ::hasReceivedMore())
 */
bool NdbRootFragment::hasRequestedMore() const
{
  return (m_pendingRequests > 0);
}

/**
 * Signal that another complete ResultSet is available for 
 * this NdbRootFragment.
 * Need mutex lock as 'm_availResultSets' is accesed both from
 * receiver and application thread.
 */
void NdbRootFragment::setReceivedMore()        // Need mutex
{
  assert(m_availResultSets==0);
  m_availResultSets++;
}

/**
 * Check if another ResultSets has been received and is available
 * for reading. It will be given to the application thread when it
 * call ::grabNextResultSet().
 * Need mutex lock as 'm_availResultSets' is accesed both from
 * receiver and application thread.
 */
bool NdbRootFragment::hasReceivedMore() const  // Need mutex
{
  return (m_availResultSets > 0);
}

void NdbRootFragment::prepareNextReceiveSet()
{
  assert(m_fragNo!=voidFragNo);
  assert(m_outstandingResults == 0);

  for (unsigned opNo=0; opNo<m_query->getNoOfOperations(); opNo++) 
  {
    NdbResultStream& resultStream = getResultStream(opNo);
    if (!resultStream.isSubScanComplete(m_remainingScans))
    {
      /**
       * Reset resultStream and all its descendants, since all these
       * streams will get a new set of rows in the next batch.
       */ 
      resultStream.prepareNextReceiveSet();
    }
  }
  m_confReceived = false;
  m_pendingRequests++;
}

/**
 * Let the application thread takes ownership of an available
 * ResultSet, prepare it for reading first row.
 * Need mutex lock as 'm_availResultSets' is accesed both from
 * receiver and application thread.
 */
void NdbRootFragment::grabNextResultSet()  // Need mutex
{
  assert(m_availResultSets>0);
  m_availResultSets--;

  assert(m_pendingRequests>0);
  m_pendingRequests--;

  NdbResultStream& rootStream = getResultStream(0);
  rootStream.prepareResultSet(m_remainingScans);  

  /* Position at the first (sorted?) row available from this fragments.
   */
  rootStream.firstResult();
}

void NdbRootFragment::setConfReceived(Uint32 tcPtrI)
{ 
  /* For a query with a lookup root, there may be more than one TCKEYCONF
     message. For a scan, there should only be one SCAN_TABCONF per root
     fragment. 
  */
  assert(!getResultStream(0).isScanQuery() || !m_confReceived);
  getResultStream(0).getReceiver().m_tcPtrI = tcPtrI;
  m_confReceived = true; 
}

bool NdbRootFragment::finalBatchReceived() const
{
  return m_confReceived && getReceiverTcPtrI()==RNIL;
}

bool NdbRootFragment::isEmpty() const
{ 
  return getResultStream(0).isEmpty();
}

/**
 * SPJ requests are identified by the receiver-id of the
 * *root* ResultStream for each RootFragment. Furthermore
 * a NEXTREQ use the tcPtrI saved in this ResultStream to
 * identify the 'cursor' to restart.
 *
 * We provide some convenient accessors for fetching this info 
 */
Uint32 NdbRootFragment::getReceiverId() const
{
  return getResultStream(0).getReceiver().getId();
}

Uint32 NdbRootFragment::getReceiverTcPtrI() const
{
  return getResultStream(0).getReceiver().m_tcPtrI;
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

int
NdbQuery::setBound(const NdbRecord *keyRecord,
                   const NdbIndexScanOperation::IndexBound *bound)
{
  const int error = m_impl.setBound(keyRecord,bound);
  if (unlikely(error)) {
    m_impl.setErrorCode(error);
    return -1;
  } else {
    return 0;
  }
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

int NdbQuery::isPrunable(bool& prunable) const
{
  return m_impl.isPrunable(prunable);
}

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
  return m_impl.setResultRowRef(rec, bufRef, result_mask);
}

int
NdbQueryOperation::setOrdering(NdbQueryOptions::ScanOrdering ordering)
{
  return m_impl.setOrdering(ordering);
}

NdbQueryOptions::ScanOrdering
NdbQueryOperation::getOrdering() const
{
  return m_impl.getOrdering();
}

int NdbQueryOperation::setParallelism(Uint32 parallelism){
  return m_impl.setParallelism(parallelism);
}

int NdbQueryOperation::setMaxParallelism(){
  return m_impl.setMaxParallelism();
}

int NdbQueryOperation::setAdaptiveParallelism(){
  return m_impl.setAdaptiveParallelism();
}

int NdbQueryOperation::setBatchSize(Uint32 batchSize){
  return m_impl.setBatchSize(batchSize);
}

int NdbQueryOperation::setInterpretedCode(const NdbInterpretedCode& code) const
{
  return m_impl.setInterpretedCode(code);
}

NdbQuery::NextResultOutcome
NdbQueryOperation::firstResult()
{
  return m_impl.firstResult();
}

NdbQuery::NextResultOutcome
NdbQueryOperation::nextResult(bool fetchAllowed, bool forceSend)
{
  return m_impl.nextResult(fetchAllowed, forceSend);
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

/////////////////////////////////////////////////
/////////  NdbQueryParamValue methods ///////////
/////////////////////////////////////////////////

enum Type
{
  Type_NULL,
  Type_raw,        // Raw data formated according to bound Column format.
  Type_raw_shrink, // As Type_raw, except short VarChar has to be shrinked.
  Type_string,     // '\0' terminated C-type string, char/varchar data only
  Type_Uint16,
  Type_Uint32,
  Type_Uint64,
  Type_Double
};

NdbQueryParamValue::NdbQueryParamValue(Uint16 val) : m_type(Type_Uint16)
{ m_value.uint16 = val; }

NdbQueryParamValue::NdbQueryParamValue(Uint32 val) : m_type(Type_Uint32)
{ m_value.uint32 = val; }

NdbQueryParamValue::NdbQueryParamValue(Uint64 val) : m_type(Type_Uint64)
{ m_value.uint64 = val; }

NdbQueryParamValue::NdbQueryParamValue(double val) : m_type(Type_Double)
{ m_value.dbl = val; }

// C-type string, terminated by '\0'
NdbQueryParamValue::NdbQueryParamValue(const char* val) : m_type(Type_string)
{ m_value.string = val; }

// Raw data
NdbQueryParamValue::NdbQueryParamValue(const void* val, bool shrinkVarChar)
 : m_type(shrinkVarChar ? Type_raw_shrink : Type_raw)
{ m_value.raw = val; }

// NULL-value, also used as optional end marker 
NdbQueryParamValue::NdbQueryParamValue() : m_type(Type_NULL)
{}


int 
NdbQueryParamValue::serializeValue(const class NdbColumnImpl& column,
                                   Uint32Buffer& dst,
                                   Uint32& len,
                                   bool& isNull) const
{
  const Uint32 maxSize = column.getSizeInBytes();
  isNull = false;
  // Start at (32-bit) word boundary.
  dst.skipRestOfWord();

  // Fetch parameter value and length.
  // Rudimentary typecheck of paramvalue: At least length should be as expected:
  //  - Extend with more types if required
  //  - Considder to add simple type conversion, ex: Int64 -> Int32
  //  - Or 
  //     -- Represent all exact numeric as Int64 and convert to 'smaller' int
  //     -- Represent all floats as Double and convert to smaller floats
  //
  switch(m_type)
  {
    case Type_NULL:
      isNull = true;
      len = 0;
      break;

    case Type_Uint16:
      if (unlikely(column.getType() != NdbDictionary::Column::Smallint &&
                   column.getType() != NdbDictionary::Column::Smallunsigned))
        return QRY_PARAMETER_HAS_WRONG_TYPE;
      
      len = static_cast<Uint32>(sizeof(m_value.uint16));
      DBUG_ASSERT(len == maxSize);
      dst.appendBytes(&m_value.uint16, len);
      break;

    case Type_Uint32:
      if (unlikely(column.getType() != NdbDictionary::Column::Int &&
                   column.getType() != NdbDictionary::Column::Unsigned))
        return QRY_PARAMETER_HAS_WRONG_TYPE;

      len = static_cast<Uint32>(sizeof(m_value.uint32));
      DBUG_ASSERT(len == maxSize);
      dst.appendBytes(&m_value.uint32, len);
      break;

    case Type_Uint64:
      if (unlikely(column.getType() != NdbDictionary::Column::Bigint &&
                   column.getType() != NdbDictionary::Column::Bigunsigned))
        return QRY_PARAMETER_HAS_WRONG_TYPE;

      len = static_cast<Uint32>(sizeof(m_value.uint64));
      DBUG_ASSERT(len == maxSize);
      dst.appendBytes(&m_value.uint64, len);
      break;

    case Type_Double:
      if (unlikely(column.getType() != NdbDictionary::Column::Double))
        return QRY_PARAMETER_HAS_WRONG_TYPE;

      len = static_cast<Uint32>(sizeof(m_value.dbl));
      DBUG_ASSERT(len == maxSize);
      dst.appendBytes(&m_value.dbl, len);
      break;

    case Type_string:
      if (unlikely(column.getType() != NdbDictionary::Column::Char &&
                   column.getType() != NdbDictionary::Column::Varchar &&
                   column.getType() != NdbDictionary::Column::Longvarchar))
        return QRY_PARAMETER_HAS_WRONG_TYPE;
      {
        len  = static_cast<Uint32>(strlen(m_value.string));
        if (unlikely(len > maxSize))
          return QRY_CHAR_PARAMETER_TRUNCATED;
        
        dst.appendBytes(m_value.string, len);
      }
      break;

    case Type_raw:
      // 'Raw' data is readily formated according to the bound column 
      if (likely(column.m_arrayType == NDB_ARRAYTYPE_FIXED))
      {
        len = maxSize;
        dst.appendBytes(m_value.raw, maxSize);
      }
      else if (column.m_arrayType == NDB_ARRAYTYPE_SHORT_VAR)
      {
        len  = 1+*((Uint8*)(m_value.raw));

        DBUG_ASSERT(column.getType() == NdbDictionary::Column::Varchar ||
                    column.getType() == NdbDictionary::Column::Varbinary);
        if (unlikely(len > 1+static_cast<Uint32>(column.getLength())))
          return QRY_CHAR_PARAMETER_TRUNCATED;

        dst.appendBytes(m_value.raw, len);
      }
      else if (column.m_arrayType == NDB_ARRAYTYPE_MEDIUM_VAR)
      {
        len  = 2+uint2korr((Uint8*)m_value.raw);

        DBUG_ASSERT(column.getType() == NdbDictionary::Column::Longvarchar ||
                    column.getType() == NdbDictionary::Column::Longvarbinary);
        if (unlikely(len > 2+static_cast<Uint32>(column.getLength())))
          return QRY_CHAR_PARAMETER_TRUNCATED;
        dst.appendBytes(m_value.raw, len);
      }
      else
      {
        DBUG_ASSERT(0);
      }
      break;

    case Type_raw_shrink:
      // Only short VarChar can be shrinked
      if (unlikely(column.m_arrayType != NDB_ARRAYTYPE_SHORT_VAR))
        return QRY_PARAMETER_HAS_WRONG_TYPE;

      DBUG_ASSERT(column.getType() == NdbDictionary::Column::Varchar ||
                  column.getType() == NdbDictionary::Column::Varbinary);

      {
        // Convert from two-byte to one-byte length field.
        len = 1+uint2korr((Uint8*)m_value.raw);
        assert(len <= 0x100);

        if (unlikely(len > 1+static_cast<Uint32>(column.getLength())))
          return QRY_CHAR_PARAMETER_TRUNCATED;

        const Uint8 shortLen = static_cast<Uint8>(len-1);
        dst.appendBytes(&shortLen, 1);
        dst.appendBytes(((Uint8*)m_value.raw)+2, shortLen);
      }
      break;

    default:
      assert(false);
  }
  if (unlikely(dst.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  }
  return 0;
} // NdbQueryParamValue::serializeValue

///////////////////////////////////////////
/////////  NdbQueryImpl methods ///////////
///////////////////////////////////////////

NdbQueryImpl::NdbQueryImpl(NdbTransaction& trans, 
                           const NdbQueryDefImpl& queryDef):
  m_interface(*this),
  m_state(Initial),
  m_tcState(Inactive),
  m_next(NULL),
  m_queryDef(&queryDef),
  m_error(),
  m_errorReceived(0),
  m_transaction(trans),
  m_scanTransaction(NULL),
  m_operations(0),
  m_countOperations(0),
  m_globalCursor(0),
  m_pendingFrags(0),
  m_rootFragCount(0),
  m_rootFrags(NULL),
  m_applFrags(),
  m_finalBatchFrags(0),
  m_num_bounds(0),
  m_shortestBound(0xffffffff),
  m_attrInfo(),
  m_keyInfo(),
  m_startIndicator(false),
  m_commitIndicator(false),
  m_prunability(Prune_No),
  m_pruneHashVal(0),
  m_operationAlloc(sizeof(NdbQueryOperationImpl)),
  m_tupleSetAlloc(sizeof(NdbResultStream::TupleSet)),
  m_resultStreamAlloc(sizeof(NdbResultStream)),
  m_pointerAlloc(sizeof(void*)),
  m_rowBufferAlloc(sizeof(char))
{
  // Allocate memory for all m_operations[] in a single chunk
  m_countOperations = queryDef.getNoOfOperations();
  const int error = m_operationAlloc.init(m_countOperations);
  if (unlikely(error != 0))
  {
    setErrorCode(error);
    return;
  }
  m_operations = reinterpret_cast<NdbQueryOperationImpl*>
    (m_operationAlloc.allocObjMem(m_countOperations));

  // Then; use placement new to construct each individual 
  // NdbQueryOperationImpl object in m_operations
  for (Uint32 i=0; i<m_countOperations; ++i)
  {
    const NdbQueryOperationDefImpl& def = queryDef.getQueryOperation(i);
    new(&m_operations[i]) NdbQueryOperationImpl(*this, def);
    // Failed to create NdbQueryOperationImpl object.
    if (m_error.code != 0) 
    {
      // Destroy those objects that we have already constructed.
      for (int j = static_cast<int>(i)-1; j>= 0; j--)
      { 
        m_operations[j].~NdbQueryOperationImpl();
      }
      m_operations = NULL;
      return;
    }
  }

  // Serialized QueryTree definition is first part of ATTRINFO.
  m_attrInfo.append(queryDef.getSerialized());
}

NdbQueryImpl::~NdbQueryImpl()
{
  /** BEWARE:
   *  Don't refer NdbQueryDef or NdbQueryOperationDefs after 
   *  NdbQuery::close() as at this stage the appliaction is 
   *  allowed to destruct the Def's.
   */
  assert(m_state==Closed);
  assert(m_rootFrags==NULL);

  // NOTE: m_operations[] was allocated as a single memory chunk with
  // placement new construction of each operation.
  // Requires explicit call to d'tor of each operation before memory is free'ed.
  if (m_operations != NULL) {
    for (int i=m_countOperations-1; i>=0; --i)
    { m_operations[i].~NdbQueryOperationImpl();
    }
    m_operations = NULL;
  }
  m_state = Destructed;
}

void
NdbQueryImpl::postFetchRelease()
{
  if (m_rootFrags != NULL)
  {
    for (unsigned i=0; i<m_rootFragCount; i++)
    { m_rootFrags[i].postFetchRelease();
    }
  }
  if (m_operations != NULL)
  {
    for (unsigned i=0; i<m_countOperations; i++)
    { m_operations[i].postFetchRelease();
    }
  }
  delete[] m_rootFrags;
  m_rootFrags = NULL;

  m_rowBufferAlloc.reset();
  m_tupleSetAlloc.reset();
  m_resultStreamAlloc.reset();
}


//static
NdbQueryImpl*
NdbQueryImpl::buildQuery(NdbTransaction& trans, 
                         const NdbQueryDefImpl& queryDef)
{
  assert(queryDef.getNoOfOperations() > 0);
  // Check for online upgrade/downgrade.
  if (unlikely(!ndb_join_pushdown(trans.getNdb()->getMinDbNodeVersion())))
  {
    trans.setOperationErrorCodeAbort(Err_FunctionNotImplemented);
    return NULL;
  }
  NdbQueryImpl* const query = new NdbQueryImpl(trans, queryDef);
  if (unlikely(query==NULL)) {
    trans.setOperationErrorCodeAbort(Err_MemoryAlloc);
    return NULL;
  }
  if (unlikely(query->m_error.code != 0))
  {
    // Transaction error code set already.
    query->release();
    return NULL;
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
NdbQueryImpl::assignParameters(const NdbQueryParamValue paramValues[])
{
  /**
   * Immediately build the serialized parameter representation in order 
   * to avoid storing param values elsewhere until query is executed.
   * Also calculates prunable property, and possibly its hashValue.
   */
  // Build explicit key/filter/bounds for root operation, possibly refering paramValues
  const int error = getRoot().prepareKeyInfo(m_keyInfo, paramValues);
  if (unlikely(error != 0))
  {
    setErrorCode(error);
    return -1;
  }

  // Serialize parameter values for the other (non-root) operations
  // (No need to serialize for root (i==0) as root key is part of keyInfo above)
  for (Uint32 i=1; i<getNoOfOperations(); ++i)
  {
    if (getQueryDef().getQueryOperation(i).getNoOfParameters() > 0)
    {
      const int error = getQueryOperation(i).serializeParams(paramValues);
      if (unlikely(error != 0))
      {
        setErrorCode(error);
        return -1;
      }
    }
  }
  assert(m_state<Defined);
  m_state = Defined;
  return 0;
} // NdbQueryImpl::assignParameters


static int
insert_bound(Uint32Buffer& keyInfo, const NdbRecord *key_record,
                                              Uint32 column_index,
                                              const char *row,
                                              Uint32 bound_type)
{
  char buf[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];
  const NdbRecord::Attr *column= &key_record->columns[column_index];

  bool is_null= column->is_null(row);
  Uint32 len= 0;
  const void *aValue= row+column->offset;

  if (!is_null)
  {
    bool len_ok;
    /* Support for special mysqld varchar format in keys. */
    if (column->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      len_ok= column->shrink_varchar(row, len, buf);
      aValue= buf;
    }
    else
    {
      len_ok= column->get_var_length(row, len);
    }
    if (!len_ok) {
      return Err_WrongFieldLength;
    }
  }

  AttributeHeader ah(column->index_attrId, len);
  keyInfo.append(bound_type);
  keyInfo.append(ah.m_value);
  keyInfo.appendBytes(aValue,len);

  return 0;
}


int
NdbQueryImpl::setBound(const NdbRecord *key_record,
                       const NdbIndexScanOperation::IndexBound *bound)
{
  m_prunability = Prune_Unknown;
  if (unlikely(key_record == NULL || bound==NULL))
    return QRY_REQ_ARG_IS_NULL;

  if (unlikely(getRoot().getQueryOperationDef().getType()
               != NdbQueryOperationDef::OrderedIndexScan))
  {
    return QRY_WRONG_OPERATION_TYPE;
  }

  assert(m_state >= Defined);
  if (m_state != Defined)
  {
    return QRY_ILLEGAL_STATE;
  }

  int startPos = m_keyInfo.getSize();

  // We don't handle both NdbQueryIndexBound defined in ::scanIndex()
  // in combination with a later ::setBound(NdbIndexScanOperation::IndexBound)
//assert (m_bound.lowKeys==0 && m_bound.highKeys==0);

  if (unlikely(bound->range_no != m_num_bounds ||
               bound->range_no > NdbIndexScanOperation::MaxRangeNo))
  {
 // setErrorCodeAbort(4286);
    return Err_InvalidRangeNo;
  }

  Uint32 key_count= bound->low_key_count;
  Uint32 common_key_count= key_count;
  if (key_count < bound->high_key_count)
    key_count= bound->high_key_count;
  else
    common_key_count= bound->high_key_count;

  if (m_shortestBound > common_key_count)
  {
    m_shortestBound = common_key_count;
  }
  /* Has the user supplied an open range (no bounds)? */
  const bool openRange= ((bound->low_key == NULL || bound->low_key_count == 0) && 
                         (bound->high_key == NULL || bound->high_key_count == 0));
  if (likely(!openRange))
  {
    /* If low and high key pointers are the same and key counts are
     * the same, we send as an Eq bound to save bandwidth.
     * This will not send an EQ bound if :
     *   - Different numbers of high and low keys are EQ
     *   - High and low keys are EQ, but use different ptrs
     */
    const bool isEqRange= 
      (bound->low_key == bound->high_key) &&
      (bound->low_key_count == bound->high_key_count) &&
      (bound->low_inclusive && bound->high_inclusive); // Does this matter?

    if (isEqRange)
    {
      /* Using BoundEQ will result in bound being sent only once */
      for (unsigned j= 0; j<key_count; j++)
      {
        const int error=
          insert_bound(m_keyInfo, key_record, key_record->key_indexes[j],
                                bound->low_key, NdbIndexScanOperation::BoundEQ);
        if (unlikely(error))
          return error;
      }
    }
    else
    {
      /* Distinct upper and lower bounds, must specify them independently */
      /* Note :  Protocol allows individual columns to be specified as EQ
       * or some prefix of columns.  This is not currently supported from
       * NDBAPI.
       */
      for (unsigned j= 0; j<key_count; j++)
      {
        Uint32 bound_type;
        /* If key is part of lower bound */
        if (bound->low_key && j<bound->low_key_count)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= bound->low_inclusive  || j+1 < bound->low_key_count ?
            NdbIndexScanOperation::BoundLE : NdbIndexScanOperation::BoundLT;
          const int error=
            insert_bound(m_keyInfo, key_record, key_record->key_indexes[j],
                                  bound->low_key, bound_type);
          if (unlikely(error))
            return error;
        }
        /* If key is part of upper bound */
        if (bound->high_key && j<bound->high_key_count)
        {
          /* Inclusive if defined, or matching rows can include this value */
          bound_type= bound->high_inclusive  || j+1 < bound->high_key_count ?
            NdbIndexScanOperation::BoundGE : NdbIndexScanOperation::BoundGT;
          const int error=
            insert_bound(m_keyInfo, key_record, key_record->key_indexes[j],
                                  bound->high_key, bound_type);
          if (unlikely(error))
            return error;
        }
      }
    }
  }
  else
  {
    /* Open range - all rows must be returned.
     * To encode this, we'll request all rows where the first
     * key column value is >= NULL
     */
    AttributeHeader ah(0, 0);
    m_keyInfo.append(NdbIndexScanOperation::BoundLE);
    m_keyInfo.append(ah.m_value);
  }

  Uint32 length = m_keyInfo.getSize()-startPos;
  if (unlikely(m_keyInfo.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  } else if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; // Query definition too large.
  } else if (likely(length > 0)) {
    m_keyInfo.put(startPos, m_keyInfo.get(startPos) | (length << 16) | (bound->range_no << 4));
  }

#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized KEYINFO w/ bounds for indexScan root : ";
  for (Uint32 i = startPos; i < m_keyInfo.getSize(); i++) {
    char buf[12];
    sprintf(buf, "%.8x", m_keyInfo.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  m_num_bounds++;
  return 0;
} // NdbQueryImpl::setBound()


Uint32
NdbQueryImpl::getNoOfOperations() const
{
  return m_countOperations;
}

Uint32
NdbQueryImpl::getNoOfLeafOperations() const
{
  return getQueryOperation(Uint32(0)).getNoOfLeafOperations();
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

/**
 * NdbQueryImpl::nextResult() - The 'global' cursor on the query results
 *
 * Will itterate and fetch results for all combinations of results from the NdbOperations
 * which this query consists of. Except for the root operations which will follow any 
 * optinal ScanOrdering, we have no control of the ordering which the results from the
 * QueryOperations appear in.
 */

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

  assert (m_globalCursor < getNoOfOperations());

  while (m_state != EndOfData)  // Or likely:  return when 'gotRow'
  {
    NdbQuery::NextResultOutcome res =
      getQueryOperation(m_globalCursor).nextResult(fetchAllowed,forceSend);

    if (unlikely(res == NdbQuery::NextResult_error))
      return res;

    else if (res == NdbQuery::NextResult_scanComplete)
    {
      if (m_globalCursor == 0)  // Completed reading all results from root
        break;
      m_globalCursor--;         // Get 'next' from  ancestor
    }

    else if (res == NdbQuery::NextResult_gotRow)
    {
      // Position to 'firstResult()' for all childs.
      // Update m_globalCursor to itterate from last operation with results next time
      //
      for (uint child=m_globalCursor+1; child<getNoOfOperations(); child++)
      {
        res = getQueryOperation(child).firstResult();
        if (unlikely(res == NdbQuery::NextResult_error))
          return res;
        else if (res == NdbQuery::NextResult_gotRow)
          m_globalCursor = child;
      }
      return NdbQuery::NextResult_gotRow;
    }
    else
    {
      assert (res == NdbQuery::NextResult_bufferEmpty);
      return res;
    }
  }

  assert (m_state == EndOfData);
  return NdbQuery::NextResult_scanComplete;

} //NdbQueryImpl::nextResult()


/**
 * Local cursor component which implements the special case of 'next' on the 
 * root operation of entire NdbQuery. In addition to fetch 'next' result from
 * the root operation, we should also retrieve more results from the datanodes
 * if required and allowed. 
 */
NdbQuery::NextResultOutcome
NdbQueryImpl::nextRootResult(bool fetchAllowed, bool forceSend)
{
  /* To minimize lock contention, each query has the separate root fragment 
   * conatiner 'm_applFrags'. m_applFrags is only accessed by the application
   * thread, so it is safe to use it without locks.
   */
  while (m_state != EndOfData)  // Or likely:  return when 'gotRow' or error
  {
    const NdbRootFragment* rootFrag = m_applFrags.getCurrent();
    if (unlikely(rootFrag==NULL))
    {
      /* m_applFrags is empty, so we cannot get more results without 
       * possibly blocking.
       *
       * ::awaitMoreResults() will either copy fragments that are already
       * complete (under mutex protection), or block until data
       * previously requested arrives.
       */
      const FetchResult fetchResult = awaitMoreResults(forceSend);
      switch (fetchResult) {

      case FetchResult_ok:          // OK - got data wo/ error
        assert(m_state != Failed);
        rootFrag = m_applFrags.getCurrent();
        assert (rootFrag!=NULL);
        break;

      case FetchResult_noMoreData:  // No data, no error
        assert(m_state != Failed);
        assert (m_applFrags.getCurrent()==NULL);
        getRoot().nullifyResult();
        m_state = EndOfData;
        postFetchRelease();
        return NdbQuery::NextResult_scanComplete;

      case FetchResult_noMoreCache: // No cached data, no error
        assert(m_state != Failed);
        assert (m_applFrags.getCurrent()==NULL);
        getRoot().nullifyResult();
        if (fetchAllowed)
        {
          break;  // ::sendFetchMore() may request more results
        }
        return NdbQuery::NextResult_bufferEmpty;

      case FetchResult_gotError:    // Error in 'm_error.code'
        assert (m_error.code != 0);
        return NdbQuery::NextResult_error;

      default:
        assert(false);
      }
    }
    else
    {
      rootFrag->getResultStream(0).nextResult();   // Consume current
      m_applFrags.reorganize();                    // Calculate new current
      // Reorg. may update 'current' RootFragment
      rootFrag = m_applFrags.getCurrent();
    }

    /**
     * If allowed to request more rows from datanodes, we do this asynch
     * and request more rows as soon as we have consumed all rows from a
     * fragment. ::awaitMoreResults() may eventually block and wait for these
     * when required.
     */
    if (fetchAllowed)
    {
      // Ask for a new batch if we emptied some.
      NdbRootFragment** frags;
      const Uint32 cnt = m_applFrags.getFetchMore(frags);
      if (cnt > 0 && sendFetchMore(frags, cnt, forceSend) != 0)
      {
        return NdbQuery::NextResult_error;
      }
    }

    if (rootFrag!=NULL)
    {
      getRoot().fetchRow(rootFrag->getResultStream(0));
      return NdbQuery::NextResult_gotRow;
    }
  } // m_state != EndOfData

  assert (m_state == EndOfData);
  return NdbQuery::NextResult_scanComplete;
} //NdbQueryImpl::nextRootResult()


/**
 * Wait for more scan results which already has been REQuested to arrive.
 * @return 0 if some rows did arrive, a negative value if there are errors (in m_error.code),
 * and 1 of there are no more rows to receive.
 */
NdbQueryImpl::FetchResult
NdbQueryImpl::awaitMoreResults(bool forceSend)
{
  assert(m_applFrags.getCurrent() == NULL);

  /* Check if there are any more completed fragments available.*/
  if (getQueryDef().isScanQuery())
  {
    assert (m_scanTransaction);
    assert (m_state==Executing);

    NdbImpl* const ndb = m_transaction.getNdb()->theImpl;
    {
      /* This part needs to be done under mutex due to synchronization with 
       * receiver thread.
       */
      PollGuard poll_guard(*ndb);

      /* There may be pending (asynchronous received, mutex protected) errors
       * from TC / datanodes. Propogate these into m_error.code in 'API space'.
       */
      while (likely(!hasReceivedError()))
      {
        /* Scan m_rootFrags (under mutex protection) for fragments
         * which has received a complete batch. Add these to m_applFrags.
         */
        m_applFrags.prepareMoreResults(m_rootFrags,m_rootFragCount);
        if (m_applFrags.getCurrent() != NULL)
        {
          return FetchResult_ok;
        }

        /* There are no more available fragment results available without
         * first waiting for more to be received from the datanodes
         */
        if (m_pendingFrags == 0)
        {
          // 'No more *pending* results', ::sendFetchMore() may make more available
          return (m_finalBatchFrags < getRootFragCount()) ? FetchResult_noMoreCache 
                                                          : FetchResult_noMoreData;
        }

        const Uint32 timeout  = ndb->get_waitfor_timeout();
        const Uint32 nodeId   = m_transaction.getConnectedNodeId();
        const Uint32 seq      = m_transaction.theNodeSequence;

        /* More results are on the way, so we wait for them.*/
        const FetchResult waitResult = static_cast<FetchResult>
          (poll_guard.wait_scan(3*timeout, 
                                nodeId, 
                                forceSend));

        if (ndb->getNodeSequence(nodeId) != seq)
          setFetchTerminated(Err_NodeFailCausedAbort,false);
        else if (likely(waitResult == FetchResult_ok))
          continue;
        else if (waitResult == FetchResult_timeOut)
          setFetchTerminated(Err_ReceiveTimedOut,false);
        else
          setFetchTerminated(Err_NodeFailCausedAbort,false);

        assert (m_state != Failed);
      } // while(!hasReceivedError())
    } // Terminates scope of 'PollGuard'

    // Fall through only if ::hasReceivedError()
    assert (m_error.code);
    return FetchResult_gotError;
  }
  else // is a Lookup query
  {
    /* The root operation is a lookup. Lookups are guaranteed to be complete
     * before NdbTransaction::execute() returns. Therefore we do not set
     * the lock, because we know that the signal receiver thread will not
     * be accessing m_rootFrags at this time.
     */
    m_applFrags.prepareMoreResults(m_rootFrags,m_rootFragCount);
    if (m_applFrags.getCurrent() != NULL)
    {
      return FetchResult_ok;
    }

    /* Getting here means that either:
     *  - No results was returned (TCKEYREF)
     *  - There was no matching row for an inner join.
     *  - or, the application called nextResult() twice for a lookup query.
     */
    assert(m_pendingFrags == 0);
    assert(m_finalBatchFrags == getRootFragCount());
    return FetchResult_noMoreData;
  } // if(getQueryDef().isScanQuery())

} //NdbQueryImpl::awaitMoreResults


/*
  ::handleBatchComplete() is intended to be called when receiving signals only.
  The PollGuard mutex is then set and the shared 'm_pendingFrags' and
  'm_finalBatchFrags' can safely be updated and ::setReceivedMore() signaled.

  returns: 'true' when application thread should be resumed.
*/
bool 
NdbQueryImpl::handleBatchComplete(NdbRootFragment& rootFrag)
{
  if (traceSignals) {
    ndbout << "NdbQueryImpl::handleBatchComplete"
           << ", fragNo=" << rootFrag.getFragNo()
           << ", pendingFrags=" << (m_pendingFrags-1)
           << ", finalBatchFrags=" << m_finalBatchFrags
           <<  endl;
  }
  assert(rootFrag.isFragBatchComplete());

  /* May received fragment data after a SCANREF() (timeout?) 
   * terminated the scan.  We are about to close this query, 
   * and didn't expect any more data - ignore it!
   */
  if (likely(m_errorReceived == 0))
  {
    assert(m_pendingFrags > 0);                // Check against underflow.
    assert(m_pendingFrags <= m_rootFragCount); // .... and overflow
    m_pendingFrags--;

    if (rootFrag.finalBatchReceived())
    {
      m_finalBatchFrags++;
      assert(m_finalBatchFrags <= m_rootFragCount);
    }

    /* When application thread ::awaitMoreResults() it will later be
     * added to m_applFrags under mutex protection.
     */
    rootFrag.setReceivedMore();
    return true;
  }
  else if (!getQueryDef().isScanQuery())  // A failed lookup query
  {
    /**
     * A lookup query will retrieve the rows as part of ::execute().
     * -> Error must be visible through API before we return control
     *    to the application.
     */
    setErrorCode(m_errorReceived);
    return true;
  }

  return false;
} // NdbQueryImpl::handleBatchComplete

int
NdbQueryImpl::close(bool forceSend)
{
  int res = 0;

  assert (m_state >= Initial && m_state < Destructed);
  if (m_state != Closed)
  {
    if (m_tcState != Inactive)
    {
      /* We have started a scan, but we have not yet received the last batch
       * for all root fragments. We must therefore close the scan to release 
       * the scan context at TC.*/
      res = closeTcCursor(forceSend);
    }

    // Throw any pending results
    NdbRootFragment::clear(m_rootFrags,m_rootFragCount);
    m_applFrags.clear();

    Ndb* const ndb = m_transaction.getNdb();
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
  }

  /** BEWARE:
   *  Don't refer NdbQueryDef or its NdbQueryOperationDefs after ::close()
   *  as the application is allowed to destruct the Def's after this point.
   */
  m_queryDef= NULL;

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
NdbQueryImpl::setErrorCode(int aErrorCode)
{
  assert (aErrorCode!=0);
  m_error.code = aErrorCode;
  m_transaction.theErrorLine = 0;
  m_transaction.theErrorOperation = NULL;

  switch(aErrorCode)
  {
    // Not realy an error. A root lookup found no match.
  case Err_TupleNotFound:
    // Simple or dirty read failed due to node failure. Transaction will be aborted.
  case Err_SimpleDirtyReadFailed:
    m_transaction.setOperationErrorCode(aErrorCode);
    break;

    // For any other error, abort the transaction.
  default:
    m_state = Failed;
    m_transaction.setOperationErrorCodeAbort(aErrorCode);
    break;
  }
}

/*
 * ::setFetchTerminated() Should only be called with mutex locked.
 * Register result fetching as completed (possibly prematurely, w/ errorCode).
 */
void
NdbQueryImpl::setFetchTerminated(int errorCode, bool needClose)
{
  assert(m_finalBatchFrags < getRootFragCount());
  if (!needClose)
  {
    m_finalBatchFrags = getRootFragCount();
  }
  if (errorCode!=0)
  {
    m_errorReceived = errorCode;
  }
  m_pendingFrags = 0;
} // NdbQueryImpl::setFetchTerminated()


/* There may be pending (asynchronous received, mutex protected) errors
 * from TC / datanodes. Propogate these into 'API space'.
 * ::hasReceivedError() Should only be called with mutex locked
 */
bool
NdbQueryImpl::hasReceivedError()
{
  if (unlikely(m_errorReceived))
  {
    setErrorCode(m_errorReceived);
    return true;
  }
  return false;
} // NdbQueryImpl::hasReceivedError


bool 
NdbQueryImpl::execTCKEYCONF()
{
  if (traceSignals) {
    ndbout << "NdbQueryImpl::execTCKEYCONF()" << endl;
  }
  assert(!getQueryDef().isScanQuery());
  NdbRootFragment& rootFrag = m_rootFrags[0];

  // We will get 1 + #leaf-nodes TCKEYCONF for a lookup...
  rootFrag.setConfReceived(RNIL);
  rootFrag.incrOutstandingResults(-1);

  bool ret = false;
  if (rootFrag.isFragBatchComplete())
  { 
    ret = handleBatchComplete(rootFrag);
  }

  if (traceSignals) {
    ndbout << "NdbQueryImpl::execTCKEYCONF(): returns:" << ret
           << ", m_pendingFrags=" << m_pendingFrags
           << ", rootStream= {" << rootFrag.getResultStream(0) << "}"
           << endl;
  }
  return ret;
} // NdbQueryImpl::execTCKEYCONF

void 
NdbQueryImpl::execCLOSE_SCAN_REP(int errorCode, bool needClose)
{
  if (traceSignals)
  {
    ndbout << "NdbQueryImpl::execCLOSE_SCAN_REP()" << endl;
  }
  setFetchTerminated(errorCode,needClose);
}

int
NdbQueryImpl::prepareSend()
{
  if (unlikely(m_state != Defined)) {
    assert (m_state >= Initial && m_state < Destructed);
    if (m_state == Failed) 
      setErrorCode(QRY_IN_ERROR_STATE);
    else
      setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return -1;
  }

  // Determine execution parameters 'batch size'.
  // May be user specified (TODO), and/or,  limited/specified by config values
  //
  if (getQueryDef().isScanQuery())
  {
    /* For the first batch, we read from all fragments for both ordered 
     * and unordered scans.*/
    if (getQueryOperation(0U).m_parallelism == Parallelism_max)
    {
      m_rootFragCount
        = getRoot().getQueryOperationDef().getTable().getFragmentCount();
    }
    else
    {
      assert(getQueryOperation(0U).m_parallelism != Parallelism_adaptive);
      m_rootFragCount
        = MIN(getRoot().getQueryOperationDef().getTable().getFragmentCount(),
              getQueryOperation(0U).m_parallelism);
    }
    Ndb* const ndb = m_transaction.getNdb();

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
    m_rootFragCount = 1;
  }

  int error = m_resultStreamAlloc.init(m_rootFragCount * getNoOfOperations());
  if (error != 0)
  {
    setErrorCode(error);
    return -1;
  }
  // Allocate space for ptrs to NdbResultStream and NdbRootFragment objects.
  error = m_pointerAlloc.init(m_rootFragCount * 
                              (OrderedFragSet::pointersPerFragment));
  if (error != 0)
  {
    setErrorCode(error);
    return -1;
  }
    
  // Some preparation for later batchsize calculations pr. (sub) scan
  getRoot().calculateBatchedRows(NULL);
  getRoot().setBatchedRows(1);

  /**
   * Calculate total amount of row buffer space for all operations and
   * fragments.
   */
  Uint32 totalBuffSize = 0;
  for (Uint32 opNo = 0; opNo < getNoOfOperations(); opNo++)
  {
    const NdbQueryOperationImpl& op = getQueryOperation(opNo);

    // Add space for batchBuffer & m_correlations
    Uint32 opBuffSize = op.getBatchBufferSize();
    if (getQueryDef().isScanQuery())
    {
      opBuffSize += (sizeof(TupleCorrelation) * op.getMaxBatchRows());
      opBuffSize *= 2;              // Scans are double buffered
    }
    opBuffSize += op.getRowSize();  // Unpacked row from buffers
    totalBuffSize += opBuffSize;
  }
  m_rowBufferAlloc.init(m_rootFragCount * totalBuffSize);

  if (getQueryDef().isScanQuery())
  {
    Uint32 totalRows = 0;
    for (Uint32 i = 0; i<getNoOfOperations(); i++)
    {
      totalRows += getQueryOperation(i).getMaxBatchRows();
    }
    error = m_tupleSetAlloc.init(2 * m_rootFragCount * totalRows);
    if (unlikely(error != 0))
    {
      setErrorCode(error);
      return -1;
    }
  }

  /**
   * Allocate and initialize fragment state variables.
   * Will also cause a ResultStream object containing a 
   * NdbReceiver to be constructed for each operation in QueryTree
   */
  m_rootFrags = new NdbRootFragment[m_rootFragCount];
  if (m_rootFrags == NULL)
  {
    setErrorCode(Err_MemoryAlloc);
    return -1;
  }
  for (Uint32 i = 0; i<m_rootFragCount; i++)
  {
    m_rootFrags[i].init(*this, i); // Set fragment number.
  }

  // Fill in parameters (into ATTRINFO) for QueryTree.
  for (Uint32 i = 0; i < m_countOperations; i++) {
    const int error = m_operations[i].prepareAttrInfo(m_attrInfo);
    if (unlikely(error))
    {
      setErrorCode(error);
      return -1;
    }
  }

  if (unlikely(m_attrInfo.isMemoryExhausted() || m_keyInfo.isMemoryExhausted())) {
    setErrorCode(Err_MemoryAlloc);
    return -1;
  }

  if (unlikely(m_attrInfo.getSize() > ScanTabReq::MaxTotalAttrInfo  ||
               m_keyInfo.getSize()  > ScanTabReq::MaxTotalAttrInfo)) {
    setErrorCode(Err_ReadTooMuch); // TODO: find a more suitable errorcode, 
    return -1;
  }

  // Setup m_applStreams and m_fullStreams for receiving results
  const NdbRecord* keyRec = NULL;
  if(getRoot().getQueryOperationDef().getIndex()!=NULL)
  {
    /* keyRec is needed for comparing records when doing ordered index scans.*/
    keyRec = getRoot().getQueryOperationDef().getIndex()->getDefaultRecord();
    assert(keyRec!=NULL);
  }
  m_applFrags.prepare(m_pointerAlloc,
                      getRoot().getOrdering(),
                      m_rootFragCount, 
                      keyRec,
                      getRoot().m_ndbRecord);

  if (getQueryDef().isScanQuery())
  {
    NdbRootFragment::buildReciverIdMap(m_rootFrags, m_rootFragCount);
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

  assert (m_pendingFrags==0);
  m_state = Prepared;
  return 0;
} // NdbQueryImpl::prepareSend



/** This iterator is used for inserting a sequence of receiver ids 
 * for the initial batch of a scan into a section via a GenericSectionPtr.*/
class InitialReceiverIdIterator: public GenericSectionIterator
{
public:
  
  InitialReceiverIdIterator(NdbRootFragment rootFrags[],
                            Uint32 cnt)
    :m_rootFrags(rootFrags),
     m_fragCount(cnt),
     m_currFragNo(0)
  {}

  virtual ~InitialReceiverIdIterator() {};
  
  /**
   * Get next batch of receiver ids. 
   * @param sz This will be set to the number of receiver ids that have been
   * put in the buffer (0 if end has been reached.)
   * @return Array of receiver ids (or NULL if end reached.
   */
  virtual const Uint32* getNextWords(Uint32& sz);

  virtual void reset()
  { m_currFragNo = 0;};
  
private:
  /** 
   * Size of internal receiver id buffer. This value is arbitrary, but 
   * a larger buffer would mean fewer calls to getNextWords(), possibly
   * improving efficiency.
   */
  static const Uint32 bufSize = 16;

  /** Set of root fragments which we want to itterate receiver ids for.*/
  NdbRootFragment* m_rootFrags;
  const Uint32 m_fragCount;

  /** The next fragment numnber to be processed. (Range for 0 to no of 
   * fragments.)*/
  Uint32 m_currFragNo;
  /** Buffer for storing one batch of receiver ids.*/
  Uint32 m_receiverIds[bufSize];
};

const Uint32* InitialReceiverIdIterator::getNextWords(Uint32& sz)
{
  /**
   * For the initial batch, we want to retrieve one batch for each fragment
   * whether it is a sorted scan or not.
   */
  if (m_currFragNo >= m_fragCount)
  {
    sz = 0;
    return NULL;
  }
  else
  {
    Uint32 cnt = 0;
    while (cnt < bufSize && m_currFragNo < m_fragCount)
    {
      m_receiverIds[cnt] = m_rootFrags[m_currFragNo].getReceiverId();
      cnt++;
      m_currFragNo++;
    }
    sz = cnt;
    return m_receiverIds;
  }
}
  
/** This iterator is used for inserting a sequence of 'TcPtrI'  
 * for a NEXTREQ to a single or multiple fragments via a GenericSectionPtr.*/
class FetchMoreTcIdIterator: public GenericSectionIterator
{
public:
  FetchMoreTcIdIterator(NdbRootFragment* rootFrags[],
                        Uint32 cnt)
    :m_rootFrags(rootFrags),
     m_fragCount(cnt),
     m_currFragNo(0)
  {}

  virtual ~FetchMoreTcIdIterator() {};
  
  /**
   * Get next batch of receiver ids. 
   * @param sz This will be set to the number of receiver ids that have been
   * put in the buffer (0 if end has been reached.)
   * @return Array of receiver ids (or NULL if end reached.
   */
  virtual const Uint32* getNextWords(Uint32& sz);

  virtual void reset()
  { m_currFragNo = 0;};
  
private:
  /** 
   * Size of internal receiver id buffer. This value is arbitrary, but 
   * a larger buffer would mean fewer calls to getNextWords(), possibly
   * improving efficiency.
   */
  static const Uint32 bufSize = 16;

  /** Set of root fragments which we want to itterate TcPtrI ids for.*/
  NdbRootFragment** m_rootFrags;
  const Uint32 m_fragCount;

  /** The next fragment numnber to be processed. (Range for 0 to no of 
   * fragments.)*/
  Uint32 m_currFragNo;
  /** Buffer for storing one batch of receiver ids.*/
  Uint32 m_receiverIds[bufSize];
};

const Uint32* FetchMoreTcIdIterator::getNextWords(Uint32& sz)
{
  /**
   * For the initial batch, we want to retrieve one batch for each fragment
   * whether it is a sorted scan or not.
   */
  if (m_currFragNo >= m_fragCount)
  {
    sz = 0;
    return NULL;
  }
  else
  {
    Uint32 cnt = 0;
    while (cnt < bufSize && m_currFragNo < m_fragCount)
    {
      m_receiverIds[cnt] = m_rootFrags[m_currFragNo]->getReceiverTcPtrI();
      cnt++;
      m_currFragNo++;
    }
    sz = cnt;
    return m_receiverIds;
  }
}

/******************************************************************************
int doSend()    Send serialized queryTree and parameters encapsulated in 
                either a SCAN_TABREQ or TCKEYREQ to TC.

NOTE:           The TransporterFacade mutex is already set by callee.

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
  if (unlikely(m_state != Prepared)) {
    assert (m_state >= Initial && m_state < Destructed);
    if (m_state == Failed) 
      setErrorCode(QRY_IN_ERROR_STATE);
    else
      setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return -1;
  }

  Ndb& ndb = *m_transaction.getNdb();
  NdbImpl * impl = ndb.theImpl;

  const NdbQueryOperationImpl& root = getRoot();
  const NdbQueryOperationDefImpl& rootDef = root.getQueryOperationDef();
  const NdbTableImpl* const rootTable = rootDef.getIndex()
    ? rootDef.getIndex()->getIndexTable()
    : &rootDef.getTable();

  Uint32 tTableId = rootTable->m_id;
  Uint32 tSchemaVersion = rootTable->m_version;

  for (Uint32 i=0; i<m_rootFragCount; i++)
  {
    m_rootFrags[i].prepareNextReceiveSet();
  }

  if (rootDef.isScanOperation())
  {
    Uint32 scan_flags = 0;  // TODO: Specify with ScanOptions::SO_SCANFLAGS

    bool tupScan = (scan_flags & NdbScanOperation::SF_TupScan);
    bool rangeScan = false;

    bool dummy;
    const int error = isPrunable(dummy);
    if (unlikely(error != 0))
      return error;

    /* Handle IndexScan specifics */
    if ( (int) rootTable->m_indexType ==
         (int) NdbDictionary::Index::OrderedIndex )
    {
      rangeScan = true;
      tupScan = false;
    }
    const Uint32 descending = 
      root.getOrdering()==NdbQueryOptions::ScanOrdering_descending ? 1 : 0;
    assert(descending==0 || (int) rootTable->m_indexType ==
           (int) NdbDictionary::Index::OrderedIndex);

    assert (root.getMaxBatchRows() > 0);

    NdbApiSignal tSignal(&ndb);
    tSignal.setSignal(GSN_SCAN_TABREQ, refToBlock(m_scanTransaction->m_tcRef));

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

    Uint32 batchRows = root.getMaxBatchRows();
    Uint32 batchByteSize;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      getRootFragCount(),
                                      batchRows,
                                      batchByteSize);
    assert(batchRows==root.getMaxBatchRows());
    assert(batchRows<=batchByteSize);

    /**
     * Check if query is a sorted scan-scan.
     * Ordering can then only be guarented by restricting
     * parent batch to contain single rows.
     * (Child scans will have 'normal' batch size).
     */
    if (root.getOrdering() != NdbQueryOptions::ScanOrdering_unordered &&
        getQueryDef().getQueryType() == NdbQueryDef::MultiScanQuery)
    {
      batchRows = 1;
    }
    ScanTabReq::setScanBatch(reqInfo, batchRows);
    scanTabReq->batch_byte_size = batchByteSize;
    scanTabReq->first_batch_size = batchRows;

    ScanTabReq::setViaSPJFlag(reqInfo, 1);
    ScanTabReq::setPassAllConfsFlag(reqInfo, 1);

    Uint32 nodeVersion = impl->getNodeNdbVersion(nodeId);
    if (!ndbd_scan_tabreq_implicit_parallelism(nodeVersion))
    {
      // Implicit parallelism implies support for greater
      // parallelism than storable explicitly in old reqInfo.
      Uint32 fragments = getRootFragCount();
      if (fragments > PARALLEL_MASK)
      {
        setErrorCode(Err_SendFailed /* TODO: TooManyFragments, to too old cluster version */);
        return -1;
      }
      ScanTabReq::setParallelism(reqInfo, fragments);
    }

    ScanTabReq::setRangeScanFlag(reqInfo, rangeScan);
    ScanTabReq::setDescendingFlag(reqInfo, descending);
    ScanTabReq::setTupScanFlag(reqInfo, tupScan);
    ScanTabReq::setNoDiskFlag(reqInfo, !root.diskInUserProjection());
    ScanTabReq::set4WordConf(reqInfo, 1);

    // Assume LockMode LM_ReadCommited, set related lock flags
    ScanTabReq::setLockMode(reqInfo, false);  // not exclusive
    ScanTabReq::setHoldLockFlag(reqInfo, false);
    ScanTabReq::setReadCommittedFlag(reqInfo, true);

//  m_keyInfo = (scan_flags & NdbScanOperation::SF_KeyInfo) ? 1 : 0;

    // If scan is pruned, use optional 'distributionKey' to hold hashvalue
    if (m_prunability == Prune_Yes)
    {
//    printf("Build pruned SCANREQ, w/ hashValue:%d\n", hashValue);
      ScanTabReq::setDistributionKeyFlag(reqInfo, 1);
      scanTabReq->distributionKey= m_pruneHashVal;
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
    GenericSectionPtr secs[3];
    InitialReceiverIdIterator receiverIdIter(m_rootFrags, m_rootFragCount);
    LinearSectionIterator attrInfoIter(m_attrInfo.addr(), m_attrInfo.getSize());
    LinearSectionIterator keyInfoIter(m_keyInfo.addr(), m_keyInfo.getSize());
 
    secs[0].sectionIter= &receiverIdIter;
    secs[0].sz= getRootFragCount();

    secs[1].sectionIter= &attrInfoIter;
    secs[1].sz= m_attrInfo.getSize();

    Uint32 numSections= 2;
    if (m_keyInfo.getSize() > 0)
    {
      secs[2].sectionIter= &keyInfoIter;
      secs[2].sz= m_keyInfo.getSize();
      numSections= 3;
    }

    /* Send Fragmented as SCAN_TABREQ can be large */
    const int res = impl->sendFragmentedSignal(&tSignal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCode(Err_SendFailed);  // Error: 'Send to NDB failed'
      return FetchResult_sendFail;
    }
    m_tcState = Active;

  } else {  // Lookup query

    NdbApiSignal tSignal(&ndb);
    tSignal.setSignal(GSN_TCKEYREQ, refToBlock(m_transaction.m_tcRef));

    TcKeyReq * const tcKeyReq = CAST_PTR(TcKeyReq, tSignal.getDataPtrSend());

    const Uint64 transId = m_transaction.getTransactionId();
    tcKeyReq->apiConnectPtr   = m_transaction.theTCConPtr;
    tcKeyReq->apiOperationPtr = root.getIdOfReceiver();
    tcKeyReq->tableId = tTableId;
    tcKeyReq->tableSchemaVersion = tSchemaVersion;
    tcKeyReq->transId1 = (Uint32) transId;
    tcKeyReq->transId2 = (Uint32) (transId >> 32);

    Uint32 attrLen = 0;
    tcKeyReq->setAttrinfoLen(attrLen, 0); // Not required for long signals.
    tcKeyReq->attrLen = attrLen;

    Uint32 reqInfo = 0;
    Uint32 interpretedFlag= root.hasInterpretedCode() && 
                            rootDef.getType() == NdbQueryOperationDef::PrimaryKeyAccess;

    TcKeyReq::setOperationType(reqInfo, NdbOperation::ReadRequest);
    TcKeyReq::setViaSPJFlag(reqInfo, true);
    TcKeyReq::setKeyLength(reqInfo, 0);            // This is a long signal
    TcKeyReq::setAIInTcKeyReq(reqInfo, 0);         // Not needed
    TcKeyReq::setInterpretedFlag(reqInfo, interpretedFlag);
    TcKeyReq::setStartFlag(reqInfo, m_startIndicator);
    TcKeyReq::setExecuteFlag(reqInfo, lastFlag);
    TcKeyReq::setNoDiskFlag(reqInfo, !root.diskInUserProjection());
    TcKeyReq::setAbortOption(reqInfo, NdbOperation::AO_IgnoreError);

    TcKeyReq::setDirtyFlag(reqInfo, true);
    TcKeyReq::setSimpleFlag(reqInfo, true);
    TcKeyReq::setCommitFlag(reqInfo, m_commitIndicator);
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

    const int res = impl->sendSignal(&tSignal, nodeId, secs, numSections);
    if (unlikely(res == -1))
    {
      setErrorCode(Err_SendFailed);  // Error: 'Send to NDB failed'
      return FetchResult_sendFail;
    }
    m_transaction.OpSent();
    m_rootFrags[0].incrOutstandingResults(1 + getNoOfOperations() +
                                          getNoOfLeafOperations());
  } // if

  assert (m_pendingFrags==0);
  m_pendingFrags = m_rootFragCount;

  // Shrink memory footprint by removing structures not required after ::execute()
  m_keyInfo.releaseExtend();
  m_attrInfo.releaseExtend();

  // TODO: Release m_interpretedCode now?

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

Return Value:   0 if send succeeded, -1 otherwise.
Parameters:     emptyFrag: Root frgament for which to ask for another batch.
Remark:
******************************************************************************/
int
NdbQueryImpl::sendFetchMore(NdbRootFragment* rootFrags[],
                            Uint32 cnt,
                            bool forceSend)
{
  assert(getQueryDef().isScanQuery());

  for (Uint32 i=0; i<cnt; i++)
  {
    NdbRootFragment* rootFrag = rootFrags[i];
    assert(rootFrag->isFragBatchComplete());
    assert(!rootFrag->finalBatchReceived());
    rootFrag->prepareNextReceiveSet();
  }

  Ndb& ndb = *getNdbTransaction().getNdb();
  NdbApiSignal tSignal(&ndb);
  tSignal.setSignal(GSN_SCAN_NEXTREQ, refToBlock(m_scanTransaction->m_tcRef));
  ScanNextReq * const scanNextReq = 
    CAST_PTR(ScanNextReq, tSignal.getDataPtrSend());
  
  assert (m_scanTransaction);
  const Uint64 transId = m_scanTransaction->getTransactionId();
  
  scanNextReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
  scanNextReq->stopScan = 0;
  scanNextReq->transId1 = (Uint32) transId;
  scanNextReq->transId2 = (Uint32) (transId >> 32);
  tSignal.setLength(ScanNextReq::SignalLength);

  FetchMoreTcIdIterator receiverIdIter(rootFrags, cnt);

  GenericSectionPtr secs[1];
  secs[ScanNextReq::ReceiverIdsSectionNum].sectionIter = &receiverIdIter;
  secs[ScanNextReq::ReceiverIdsSectionNum].sz = cnt;
  
  NdbImpl * impl = ndb.theImpl;
  Uint32 nodeId = m_transaction.getConnectedNodeId();
  Uint32 seq    = m_transaction.theNodeSequence;

  /* This part needs to be done under mutex due to synchronization with 
   * receiver thread.
   */
  PollGuard poll_guard(* impl);

  if (unlikely(hasReceivedError()))
  {
    // Errors arrived inbetween ::await released mutex, and sendFetchMore grabbed it
    return -1;
  }
  if (impl->getNodeSequence(nodeId) != seq ||
      impl->sendSignal(&tSignal, nodeId, secs, 1) != 0)
  {
    setErrorCode(Err_NodeFailCausedAbort);
    return -1;
  }
  impl->do_forceSend(forceSend);

  m_pendingFrags += cnt;
  assert(m_pendingFrags <= getRootFragCount());

  return 0;
} // NdbQueryImpl::sendFetchMore()

int
NdbQueryImpl::closeTcCursor(bool forceSend)
{
  assert (getQueryDef().isScanQuery());

  NdbImpl* const ndb = m_transaction.getNdb()->theImpl;
  const Uint32 timeout  = ndb->get_waitfor_timeout();
  const Uint32 nodeId   = m_transaction.getConnectedNodeId();
  const Uint32 seq      = m_transaction.theNodeSequence;

  /* This part needs to be done under mutex due to synchronization with 
   * receiver thread.
   */
  PollGuard poll_guard(*ndb);

  if (unlikely(ndb->getNodeSequence(nodeId) != seq))
  {
    setErrorCode(Err_NodeFailCausedAbort);
    return -1;  // Transporter disconnected and reconnected, no need to close
  }

  /* Wait for outstanding scan results from current batch fetch */
  while (m_pendingFrags > 0)
  {
    const FetchResult result = static_cast<FetchResult>
        (poll_guard.wait_scan(3*timeout, nodeId, forceSend));

    if (unlikely(ndb->getNodeSequence(nodeId) != seq))
      setFetchTerminated(Err_NodeFailCausedAbort,false);
    else if (unlikely(result != FetchResult_ok))
    {
      if (result == FetchResult_timeOut)
        setFetchTerminated(Err_ReceiveTimedOut,false);
      else
        setFetchTerminated(Err_NodeFailCausedAbort,false);
    }
    if (hasReceivedError())
    {
      break;
    }
  } // while

  assert(m_pendingFrags==0);
  NdbRootFragment::clear(m_rootFrags,m_rootFragCount);
  m_errorReceived = 0;                         // Clear errors caused by previous fetching
  m_error.code = 0;

  if (m_finalBatchFrags < getRootFragCount())  // TC has an open scan cursor.
  {
    /* Send SCAN_NEXTREQ(close) */
    const int error = sendClose(m_transaction.getConnectedNodeId());
    if (unlikely(error))
      return error;

    assert(m_finalBatchFrags+m_pendingFrags==getRootFragCount());

    /* Wait for close to be confirmed: */
    while (m_pendingFrags > 0)
    {
      const FetchResult result = static_cast<FetchResult>
          (poll_guard.wait_scan(3*timeout, nodeId, forceSend));

      if (unlikely(ndb->getNodeSequence(nodeId) != seq))
        setFetchTerminated(Err_NodeFailCausedAbort,false);
      else if (unlikely(result != FetchResult_ok))
      {
        if (result == FetchResult_timeOut)
          setFetchTerminated(Err_ReceiveTimedOut,false);
        else
          setFetchTerminated(Err_NodeFailCausedAbort,false);
      }
      if (hasReceivedError())
      {
        break;
      }
    } // while
  } // if

  return 0;
} //NdbQueryImpl::closeTcCursor


/*
 * This method is called with the PollGuard mutex held on the transporter.
 */
int
NdbQueryImpl::sendClose(int nodeId)
{
  assert(m_finalBatchFrags < getRootFragCount());
  m_pendingFrags = getRootFragCount() - m_finalBatchFrags;

  Ndb& ndb = *m_transaction.getNdb();
  NdbApiSignal tSignal(&ndb);
  tSignal.setSignal(GSN_SCAN_NEXTREQ, refToBlock(m_scanTransaction->m_tcRef));
  ScanNextReq * const scanNextReq = CAST_PTR(ScanNextReq, tSignal.getDataPtrSend());

  assert (m_scanTransaction);
  const Uint64 transId = m_scanTransaction->getTransactionId();

  scanNextReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
  scanNextReq->stopScan = true;
  scanNextReq->transId1 = (Uint32) transId;
  scanNextReq->transId2 = (Uint32) (transId >> 32);
  tSignal.setLength(ScanNextReq::SignalLength);

  NdbImpl * impl = ndb.theImpl;
  return impl->sendSignal(&tSignal, nodeId);

} // NdbQueryImpl::sendClose()


int NdbQueryImpl::isPrunable(bool& prunable)
{
  if (m_prunability == Prune_Unknown)
  {
    const int error = getRoot().getQueryOperationDef()
      .checkPrunable(m_keyInfo, m_shortestBound, prunable, m_pruneHashVal);
    if (unlikely(error != 0))
    {
      prunable = false;
      setErrorCode(error);
      return -1;
    }
    m_prunability = prunable ? Prune_Yes : Prune_No;
  }
  prunable = (m_prunability == Prune_Yes);
  return 0;
}


/****************
 * NdbQueryImpl::OrderedFragSet methods.
 ***************/

NdbQueryImpl::OrderedFragSet::OrderedFragSet():
  m_capacity(0),
  m_activeFragCount(0),
  m_fetchMoreFragCount(0),
  m_finalFragReceivedCount(0),
  m_finalFragConsumedCount(0),
  m_ordering(NdbQueryOptions::ScanOrdering_void),
  m_keyRecord(NULL),
  m_resultRecord(NULL),
  m_activeFrags(NULL),
  m_fetchMoreFrags(NULL)
{
}

NdbQueryImpl::OrderedFragSet::~OrderedFragSet() 
{ 
  m_activeFrags = NULL;
  m_fetchMoreFrags = NULL;
}

void NdbQueryImpl::OrderedFragSet::clear() 
{ 
  m_activeFragCount = 0;
  m_fetchMoreFragCount = 0; 
}

void
NdbQueryImpl::OrderedFragSet::prepare(NdbBulkAllocator& allocator,
                                      NdbQueryOptions::ScanOrdering ordering, 
                                      int capacity,                
                                      const NdbRecord* keyRecord,
                                      const NdbRecord* resultRecord)
{
  assert(m_activeFrags==NULL);
  assert(m_capacity==0);
  assert(ordering!=NdbQueryOptions::ScanOrdering_void);
  
  if (capacity > 0) 
  { 
    m_capacity = capacity;

    m_activeFrags =  
      reinterpret_cast<NdbRootFragment**>(allocator.allocObjMem(capacity)); 
    bzero(m_activeFrags, capacity * sizeof(NdbRootFragment*));

    m_fetchMoreFrags = 
      reinterpret_cast<NdbRootFragment**>(allocator.allocObjMem(capacity));
    bzero(m_fetchMoreFrags, capacity * sizeof(NdbRootFragment*));
  }
  m_ordering = ordering;
  m_keyRecord = keyRecord;
  m_resultRecord = resultRecord;
} // OrderedFragSet::prepare()


/**
 *  Get current RootFragment which to return results from.
 *  Logic relies on that ::reorganize() is called whenever the current 
 *  RootFragment is advanced to next result. This will eliminate
 *  empty RootFragments from the OrderedFragSet object
 *
 */
NdbRootFragment* 
NdbQueryImpl::OrderedFragSet::getCurrent() const
{ 
  if (m_ordering!=NdbQueryOptions::ScanOrdering_unordered)
  {
    /** 
     * Must have tuples for each (non-completed) fragment when doing ordered
     * scan.
     */
    if (unlikely(m_activeFragCount+m_finalFragConsumedCount < m_capacity))
    {
      return NULL;
    }
  }
  
  if (unlikely(m_activeFragCount==0))
  {
    return NULL;
  }
  else
  {
    assert(!m_activeFrags[m_activeFragCount-1]->isEmpty());
    return m_activeFrags[m_activeFragCount-1];
  }
} // OrderedFragSet::getCurrent()

/**
 *  Keep the FragSet ordered, both with respect to specified ScanOrdering, and
 *  such that RootFragments which becomes empty are removed from 
 *  m_activeFrags[].
 *  Thus,  ::getCurrent() should be as lightweight as possible and only has
 *  to return the 'next' available from array wo/ doing any housekeeping.
 */
void
NdbQueryImpl::OrderedFragSet::reorganize()
{
  assert(m_activeFragCount > 0);
  NdbRootFragment* const frag = m_activeFrags[m_activeFragCount-1];

  // Remove the current fragment if the batch has been emptied.
  if (frag->isEmpty())
  {
    /**
     * MT-note: Although ::finalBatchReceived() normally requires mutex,
     * its safe to call it here wo/ mutex as:
     *
     *  - 'not hasRequestedMore()' guaranty that there can't be any
     *     receiver thread simultaneously accessing the mutex protected members.
     *  -  As this fragment has already been added to (the mutex protected)
     *     class OrderedFragSet, we know that the mutex has been
     *     previously set for this 'frag'. This would have resolved
     *     any cache coherency problems related to mt'ed access to
     *     'frag->finalBatchReceived()'.
     */
    if (!frag->hasRequestedMore() && frag->finalBatchReceived())
    {
      assert(m_finalFragReceivedCount > m_finalFragConsumedCount);
      m_finalFragConsumedCount++;
    }

    /**
     * Without doublebuffering we can't 'fetchMore' for fragments until 
     * the current ResultSet has been consumed bu application.
     * (Compared to how ::prepareMoreResults() immediately 'fetchMore')
     */  
    else if (!useDoubleBuffers)
    {
      m_fetchMoreFrags[m_fetchMoreFragCount++] = frag;
    }
    m_activeFragCount--;
  }

  // Reorder fragments if add'ed nonEmpty fragment to a sorted scan.
  else if (m_ordering!=NdbQueryOptions::ScanOrdering_unordered)
  {
    /** 
     * This is a sorted scan. There are more data to be read from 
     * m_activeFrags[m_activeFragCount-1]. Move it to its proper place.
     *
     * Use binary search to find the largest record that is smaller than or
     * equal to m_activeFrags[m_activeFragCount-1].
     */
    int first = 0;
    int last = m_activeFragCount-1;
    int middle = (first+last)/2;

    while (first<last)
    {
      assert(middle<m_activeFragCount);
      const int cmpRes = compare(*frag, *m_activeFrags[middle]);
      if (cmpRes < 0)
      {
        first = middle + 1;
      }
      else if (cmpRes == 0)
      {
        last = first = middle;
      }
      else
      {
        last = middle;
      }
      middle = (first+last)/2;
    }

    // Move into correct sorted position
    if (middle < m_activeFragCount-1)
    {
      assert(compare(*frag, *m_activeFrags[middle]) >= 0);
      memmove(m_activeFrags+middle+1, 
              m_activeFrags+middle, 
              (m_activeFragCount - middle - 1) * sizeof(NdbRootFragment*));
      m_activeFrags[middle] = frag;
    }
    assert(verifySortOrder());
  }
  assert(m_activeFragCount+m_finalFragConsumedCount    <= m_capacity);
  assert(m_fetchMoreFragCount+m_finalFragReceivedCount <= m_capacity);
} // OrderedFragSet::reorganize()

void 
NdbQueryImpl::OrderedFragSet::add(NdbRootFragment& frag)
{
  assert(m_activeFragCount+m_finalFragConsumedCount < m_capacity);

  m_activeFrags[m_activeFragCount++] = &frag;  // Add avail fragment
  reorganize();                                // Move into position
} // OrderedFragSet::add()

/**
 * Scan rootFrags[] for fragments which has received a ResultSet batch.
 * Add these to m_applFrags (Require mutex protection)
 */
void 
NdbQueryImpl::OrderedFragSet::prepareMoreResults(NdbRootFragment rootFrags[], Uint32 cnt) 
{
  for (Uint32 fragNo = 0; fragNo < cnt; fragNo++)
  {
    NdbRootFragment& rootFrag = rootFrags[fragNo];
    if (rootFrag.isEmpty() &&         // Current ResultSet is empty
        rootFrag.hasReceivedMore())   // Another ResultSet is available
    {
      if (rootFrag.finalBatchReceived())
      {
        m_finalFragReceivedCount++;
      }
      /**
       * When doublebuffered fetch is active:
       * Received fragment is a candidates for immediate prefetch.
       */
      else if (useDoubleBuffers)
      {
        m_fetchMoreFrags[m_fetchMoreFragCount++] = &rootFrag;
      } // useDoubleBuffers

      rootFrag.grabNextResultSet();   // Get new ResultSet.
      add(rootFrag);                  // Make avail. to appl. thread
    }
  } // for all 'rootFrags[]'

  assert(m_activeFragCount+m_finalFragConsumedCount    <= m_capacity);
  assert(m_fetchMoreFragCount+m_finalFragReceivedCount <= m_capacity);
} // OrderedFragSet::prepareMoreResults()

/**
 * Determine if a ::sendFetchMore() should be requested at this point.
 */
Uint32 
NdbQueryImpl::OrderedFragSet::getFetchMore(NdbRootFragment** &frags)
{
  /**
   * Decides (pre-)fetch strategy:
   *
   *  1) No doublebuffered ResultSets: Immediately request prefetch.
   *     (This is fetches related to 'isEmpty' fragments)
   *  2) If ordered ResultSets; Immediately request prefetch.
   *     (Need rows from all fragments to do sort-merge)
   *  3) When unordered, reduce #NEXTREQs to TC by avoid prefetch
   *     until there are pending request to all datanodes having more
   *     ResultSets
   */
  if (m_fetchMoreFragCount > 0 &&
      (!useDoubleBuffers  ||                                         // 1)
       m_ordering != NdbQueryOptions::ScanOrdering_unordered ||      // 2)
       m_fetchMoreFragCount+m_finalFragReceivedCount >= m_capacity)) // 3)
  {
    const int cnt = m_fetchMoreFragCount;
    frags = m_fetchMoreFrags;
    m_fetchMoreFragCount = 0;
    return cnt;
  }
  return 0;
}

bool 
NdbQueryImpl::OrderedFragSet::verifySortOrder() const
{
  for (int i = 0; i<m_activeFragCount-1; i++)
  {
    if (compare(*m_activeFrags[i], *m_activeFrags[i+1]) < 0)
    {
      assert(false);
      return false;
    }
  }
  return true;
}

/**
 * Compare frags such that f1<f2 if f1 is empty but f2 is not.
 * - Othewise compare record contents.
 * @return negative if frag1<frag2, 0 if frag1 == frag2, otherwise positive.
*/
int
NdbQueryImpl::OrderedFragSet::compare(const NdbRootFragment& frag1,
                                      const NdbRootFragment& frag2) const
{
  assert(m_ordering!=NdbQueryOptions::ScanOrdering_unordered);

  /* f1<f2 if f1 is empty but f2 is not.*/  
  if(frag1.isEmpty())
  {
    if(!frag2.isEmpty())
    {
      return -1;
    }
    else
    {
      return 0;
    }
  }
  
  /* Neither stream is empty so we must compare records.*/
  return compare_ndbrecord(&frag1.getResultStream(0).getReceiver(), 
                           &frag2.getResultStream(0).getReceiver(),
                           m_keyRecord,
                           m_resultRecord,
                           m_ordering 
                           == NdbQueryOptions::ScanOrdering_descending,
                           false);
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
  m_parent(NULL),
  m_children(0),
  m_maxBatchRows(0),   // >0: User specified prefered value, ==0: Use default CFG values
  m_params(),
  m_resultBuffer(NULL),
  m_resultRef(NULL),
  m_isRowNull(true),
  m_ndbRecord(NULL),
  m_read_mask(NULL),
  m_firstRecAttr(NULL),
  m_lastRecAttr(NULL),
  m_ordering(NdbQueryOptions::ScanOrdering_unordered),
  m_interpretedCode(NULL),
  m_diskInUserProjection(false),
  m_parallelism(def.getOpNo() == 0
                ? Parallelism_max : Parallelism_adaptive),
  m_rowSize(0xffffffff),
  m_batchBufferSize(0xffffffff)
{ 
  if (m_children.expand(def.getNoOfChildOperations()))
  {
    // Memory allocation during Vector::expand() failed.
    queryImpl.setErrorCode(Err_MemoryAlloc);
    return;
  }
  // Fill in operations parent refs, and append it as child of its parent
  const NdbQueryOperationDefImpl* parent = def.getParentOperation();
  if (parent != NULL)
  { 
    const Uint32 ix = parent->getOpNo();
    assert (ix < m_queryImpl.getNoOfOperations());
    m_parent = &m_queryImpl.getQueryOperation(ix);
    const int res = m_parent->m_children.push_back(this);
    UNUSED(res);
    /** 
      Enough memory should have been allocated when creating 
      m_parent->m_children, so res!=0 should never happen.
    */
    assert(res == 0);
  }
  if (def.getType()==NdbQueryOperationDef::OrderedIndexScan)
  {  
    const NdbQueryOptions::ScanOrdering defOrdering = 
      static_cast<const NdbQueryIndexScanOperationDefImpl&>(def).getOrdering();
    if (defOrdering != NdbQueryOptions::ScanOrdering_void)
    {
      // Use value from definition, if one was set.
      m_ordering = defOrdering;
    }
  }
}

NdbQueryOperationImpl::~NdbQueryOperationImpl()
{
  /**
   * We expect ::postFetchRelease to have deleted fetch related structures when fetch completed.
   * Either by fetching through last row, or calling ::close() which forcefully terminates fetch
   */
  assert (m_firstRecAttr == NULL);
  assert (m_interpretedCode == NULL);
} //NdbQueryOperationImpl::~NdbQueryOperationImpl()

/**
 * Release what we want need anymore after last available row has been 
 * returned from datanodes.
 */ 
void
NdbQueryOperationImpl::postFetchRelease()
{
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

  // TODO: Consider if interpretedCode can be deleted imm. after ::doSend
  delete m_interpretedCode;
  m_interpretedCode = NULL;
} //NdbQueryOperationImpl::postFetchRelease()


Uint32
NdbQueryOperationImpl::getNoOfParentOperations() const
{
  return (m_parent) ? 1 : 0;
}

NdbQueryOperationImpl&
NdbQueryOperationImpl::getParentOperation(Uint32 i) const
{
  assert(i==0 && m_parent!=NULL);
  return *m_parent;
}
NdbQueryOperationImpl*
NdbQueryOperationImpl::getParentOperation() const
{
  return m_parent;
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

Int32 NdbQueryOperationImpl::getNoOfDescendantOperations() const
{
  Int32 children = 0;

  for (unsigned i = 0; i < getNoOfChildOperations(); i++)
    children += 1 + getChildOperation(i).getNoOfDescendantOperations();

  return children;
}

Uint32
NdbQueryOperationImpl::getNoOfLeafOperations() const
{
  if (getNoOfChildOperations() == 0)
  {
    return 1;
  }
  else
  {
    Uint32 sum = 0;
    for (unsigned i = 0; i < getNoOfChildOperations(); i++)
      sum += getChildOperation(i).getNoOfLeafOperations();

    return sum;
  }
}

NdbRecAttr*
NdbQueryOperationImpl::getValue(
                            const char* anAttrName,
                            char* resultBuffer)
{
  if (unlikely(anAttrName == NULL)) {
    getQuery().setErrorCode(QRY_REQ_ARG_IS_NULL);
    return NULL;
  }
  const NdbColumnImpl* const column 
    = m_operationDef.getTable().getColumn(anAttrName);
  if(unlikely(column==NULL)){
    getQuery().setErrorCode(Err_UnknownColumn);
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
    getQuery().setErrorCode(Err_UnknownColumn);
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
    getQuery().setErrorCode(Err_MemoryAlloc);
    return NULL;
  }
  if(unlikely(recAttr->setup(&column, resultBuffer))) {
    ndb->releaseRecAttr(recAttr);
    getQuery().setErrorCode(Err_MemoryAlloc);
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
  if (unlikely(rec==0)) {
    getQuery().setErrorCode(QRY_REQ_ARG_IS_NULL);
    return -1;
  }
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
  return 0;
}

int
NdbQueryOperationImpl::setResultRowRef (
                       const NdbRecord* rec,
                       const char* & bufRef,
                       const unsigned char* result_mask)
{
  m_resultRef = &bufRef;
  *m_resultRef = NULL; // No result row yet
  return setResultRowBuf(rec, NULL, result_mask);
}

NdbQuery::NextResultOutcome
NdbQueryOperationImpl::firstResult()
{
  if (unlikely(getQuery().m_state < NdbQueryImpl::Executing || 
               getQuery().m_state >= NdbQueryImpl::Closed)) {
    int state = getQuery().m_state;
    assert (state >= NdbQueryImpl::Initial && state < NdbQueryImpl::Destructed);
    if (state == NdbQueryImpl::Failed) 
      getQuery().setErrorCode(QRY_IN_ERROR_STATE);
    else
      getQuery().setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return NdbQuery::NextResult_error;
  }

  const NdbRootFragment* rootFrag;

#if 0  // TODO ::firstResult() on root operation is unused, incomplete & untested
  if (unlikely(getParentOperation()==NULL))
  {
    // Reset *all* ResultStreams, optionaly order them, and find new current among them
    for( Uint32 i = 0; i<m_queryImpl.getRootFragCount(); i++)
    {
      m_resultStreams[i]->firstResult();
    }
    rootFrag = m_queryImpl.m_applFrags.reorganize();
    assert(rootFrag==NULL || rootFrag==m_queryImpl.m_applFrags.getCurrent());
  }
  else
#endif

  {
    assert(getParentOperation()!=NULL);  // TODO, See above
    rootFrag = m_queryImpl.m_applFrags.getCurrent();
  }

  if (rootFrag != NULL)
  {
    NdbResultStream& resultStream = rootFrag->getResultStream(*this);
    if (resultStream.firstResult() != tupleNotFound)
    {
      fetchRow(resultStream);
      return NdbQuery::NextResult_gotRow;
    }
  }
  nullifyResult();
  return NdbQuery::NextResult_scanComplete;
} //NdbQueryOperationImpl::firstResult()


NdbQuery::NextResultOutcome
NdbQueryOperationImpl::nextResult(bool fetchAllowed, bool forceSend)
{
  if (unlikely(getQuery().m_state < NdbQueryImpl::Executing || 
               getQuery().m_state >= NdbQueryImpl::Closed)) {
    int state = getQuery().m_state;
    assert (state >= NdbQueryImpl::Initial && state < NdbQueryImpl::Destructed);
    if (state == NdbQueryImpl::Failed) 
      getQuery().setErrorCode(QRY_IN_ERROR_STATE);
    else
      getQuery().setErrorCode(QRY_ILLEGAL_STATE);
    DEBUG_CRASH();
    return NdbQuery::NextResult_error;
  }

  if (this == &getRoot())
  {
    return m_queryImpl.nextRootResult(fetchAllowed,forceSend);
  }
  /**
   * 'next' will never be able to return anything for a lookup operation.
   *  NOTE: This is a pure optimization shortcut! 
   */
  else if (m_operationDef.isScanOperation())
  {
    const NdbRootFragment* rootFrag = m_queryImpl.m_applFrags.getCurrent();
    if (rootFrag!=NULL)
    {
      NdbResultStream& resultStream = rootFrag->getResultStream(*this);
      if (resultStream.nextResult() != tupleNotFound)
      {
        fetchRow(resultStream);
        return NdbQuery::NextResult_gotRow;
      }
    }
  }
  nullifyResult();
  return NdbQuery::NextResult_scanComplete;
} //NdbQueryOperationImpl::nextResult()


void 
NdbQueryOperationImpl::fetchRow(NdbResultStream& resultStream)
{
  const char* buff = resultStream.getCurrentRow();
  assert(buff!=NULL || (m_firstRecAttr==NULL && m_ndbRecord==NULL));

  m_isRowNull = false;
  if (m_firstRecAttr != NULL)
  {
    // Retrieve any RecAttr (getValues()) for current row
    const int retVal = resultStream.getReceiver().get_AttrValues(m_firstRecAttr);
    assert(retVal==0);  ((void)retVal);
  }
  if (m_ndbRecord != NULL)
  {
    if (m_resultRef!=NULL)
    {
      // Set application pointer to point into internal buffer.
      *m_resultRef = buff;
    }
    else
    {
      assert(m_resultBuffer!=NULL);
      // Copy result to buffer supplied by application.
      memcpy(m_resultBuffer, buff, m_ndbRecord->m_row_size);
    }
  }
} // NdbQueryOperationImpl::fetchRow


void 
NdbQueryOperationImpl::nullifyResult()
{
  if (!m_isRowNull)
  {
    /* This operation gave no result for the current row.*/ 
    m_isRowNull = true;
    if (m_resultRef!=NULL)
    {
      // Set the pointer supplied by the application to NULL.
      *m_resultRef = NULL;
    }
    /* We should not give any results for the descendants either.*/
    for (Uint32 i = 0; i<getNoOfChildOperations(); i++)
    {
      getChildOperation(i).nullifyResult();
   }
  }
} // NdbQueryOperationImpl::nullifyResult

bool
NdbQueryOperationImpl::isRowNULL() const
{
  return m_isRowNull;
}

bool
NdbQueryOperationImpl::isRowChanged() const
{
  // FIXME: Need to be implemented as scan linked with scan is now implemented.
  return true;
}

static bool isSetInMask(const unsigned char* mask, int bitNo)
{
  return mask[bitNo>>3] & 1<<(bitNo&7);
}

int
NdbQueryOperationImpl::serializeProject(Uint32Buffer& attrInfo)
{
  Uint32 startPos = attrInfo.getSize();
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
      const NdbRecord::Attr* const col= &m_ndbRecord->columns[i];
      Uint32 attrId= col->attrId;

      if (m_read_mask == NULL || isSetInMask(m_read_mask, i))
      { if (attrId > maxAttrId)
          maxAttrId= attrId;

        readMask.set(attrId);
        requestedCols++;

        const NdbColumnImpl* const column = getQueryOperationDef().getTable()
          .getColumn(col->column_no);
        if (column->getStorageType() == NDB_STORAGETYPE_DISK)
        {
          m_diskInUserProjection = true;
        }
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
    if (recAttr->getColumn()->getStorageType() == NDB_STORAGETYPE_DISK)
    {
      m_diskInUserProjection = true;
    }
    recAttr = recAttr->next();
  }

  bool withCorrelation = getRoot().getQueryDef().isScanQuery();
  if (withCorrelation) {
    Uint32 ah;
    AttributeHeader::init(&ah, AttributeHeader::CORR_FACTOR64, 0);
    attrInfo.append(ah);
  }

  // Size of projection in words.
  Uint32 length = attrInfo.getSize() - startPos - 1 ;
  attrInfo.put(startPos, length);
  return 0;
} // NdbQueryOperationImpl::serializeProject

int NdbQueryOperationImpl::serializeParams(const NdbQueryParamValue* paramValues)
{
  if (unlikely(paramValues == NULL))
  {
    return QRY_REQ_ARG_IS_NULL;
  }

  const NdbQueryOperationDefImpl& def = getQueryOperationDef();
  for (Uint32 i=0; i<def.getNoOfParameters(); i++)
  {
    const NdbParamOperandImpl& paramDef = def.getParameter(i);
    const NdbQueryParamValue& paramValue = paramValues[paramDef.getParamIx()];

    /**
     *  Add parameter value to serialized data.
     *  Each value has a Uint32 length field (in bytes), followed by
     *  the actuall value. Allocation is in Uint32 units with unused bytes
     *  zero padded.
     **/
    const Uint32 oldSize = m_params.getSize();
    m_params.append(0); // Place holder for length.
    bool null;
    Uint32 len;
    const int error = 
      paramValue.serializeValue(*paramDef.getColumn(), m_params, len, null);
    if (unlikely(error))
      return error;
    if (unlikely(null))
      return Err_KeyIsNULL;

    if(unlikely(m_params.isMemoryExhausted())){
      return Err_MemoryAlloc;
    }
    // Back patch length field.
    m_params.put(oldSize, len);
  }
  return 0;
} // NdbQueryOperationImpl::serializeParams

Uint32
NdbQueryOperationImpl
::calculateBatchedRows(const NdbQueryOperationImpl* closestScan)
{
  const NdbQueryOperationImpl* myClosestScan;
  if (m_operationDef.isScanOperation())
  {
    myClosestScan = this;
  }
  else
  {
    myClosestScan = closestScan;
  }

  Uint32 maxBatchRows = 0;
  if (myClosestScan != NULL)
  {
    // To force usage of SCAN_NEXTREQ even for small scans resultsets
    if (DBUG_EVALUATE_IF("max_4rows_in_spj_batches", true, false))
    {
      m_maxBatchRows = 4;
    }
    else if (DBUG_EVALUATE_IF("max_64rows_in_spj_batches", true, false))
    {
      m_maxBatchRows = 64;
    }
    else if (enforcedBatchSize)
    {
      m_maxBatchRows = enforcedBatchSize;
    }

    const Ndb& ndb = *getQuery().getNdbTransaction().getNdb();

    /**
     * For each batch, a lookup operation must be able to receive as many rows
     * as the closest ancestor scan operation. 
     * We must thus make sure that we do not set a batch size for the scan 
     * that exceeds what any of its scan descendants can use.
     *
     * Ignore calculated 'batchByteSize' 
     * here - Recalculated when building signal after max-batchRows has been 
     * determined.
     */
    Uint32 batchByteSize;
    /**
     * myClosestScan->m_maxBatchRows may be zero to indicate that we
     * should use default values, or non-zero if the application had an 
     * explicit preference.
     */
    maxBatchRows = myClosestScan->m_maxBatchRows;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      getRoot().m_parallelism
                                      == Parallelism_max
                                      ? m_queryImpl.getRootFragCount()
                                      : getRoot().m_parallelism,
                                      maxBatchRows,
                                      batchByteSize);
    assert(maxBatchRows > 0);
    assert(maxBatchRows <= batchByteSize);
  }

  // Find the largest value that is acceptable to all lookup descendants.
  for (Uint32 i = 0; i < m_children.size(); i++)
  {
    const Uint32 childMaxBatchRows = 
      m_children[i]->calculateBatchedRows(myClosestScan);
    maxBatchRows = MIN(maxBatchRows, childMaxBatchRows);
  }
  
  if (m_operationDef.isScanOperation())
  {
    // Use this value for current op and all lookup descendants.
    m_maxBatchRows = maxBatchRows;
    // Return max(Unit32) to avoid interfering with batch size calculation 
    // for parent.
    return 0xffffffff;
  }
  else
  {
    return maxBatchRows;
  }
} // NdbQueryOperationImpl::calculateBatchedRows


void
NdbQueryOperationImpl::setBatchedRows(Uint32 batchedRows)
{
  if (!m_operationDef.isScanOperation())
  {
    /** Lookup operations should handle the same number of rows as 
     * the closest scan ancestor.
     */
    m_maxBatchRows = batchedRows;
  }

  for (Uint32 i = 0; i < m_children.size(); i++)
  {
    m_children[i]->setBatchedRows(m_maxBatchRows);
  }
}

int 
NdbQueryOperationImpl::prepareAttrInfo(Uint32Buffer& attrInfo)
{
  const NdbQueryOperationDefImpl& def = getQueryOperationDef();

  /**
   * Serialize parameters refered by this NdbQueryOperation.
   * Params for the complete NdbQuery is collected in a single
   * serializedParams chunk. Each operations params are 
   * proceeded by 'length' for this operation.
   */
  if (def.getType() == NdbQueryOperationDef::UniqueIndexAccess)
  {
    // Reserve memory for LookupParameters, fill in contents later when
    // 'length' and 'requestInfo' has been calculated.
    Uint32 startPos = attrInfo.getSize();
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
    Uint32 length = attrInfo.getSize() - startPos;
    if (unlikely(length > 0xFFFF)) {
      return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
    }
    QueryNodeParameters::setOpLen(param->len,
                                  QueryNodeParameters::QN_LOOKUP,
                                  length);

#ifdef __TRACE_SERIALIZATION
    ndbout << "Serialized params for index node " 
           << getInternalOpNo()-1 << " : ";
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
  Uint32 startPos = attrInfo.getSize();
  Uint32 requestInfo = 0;
  bool isRoot = (def.getOpNo()==0);

  QueryNodeParameters::OpType paramType =
       !def.isScanOperation() ? QueryNodeParameters::QN_LOOKUP
           : (isRoot) ? QueryNodeParameters::QN_SCAN_FRAG 
                      : QueryNodeParameters::QN_SCAN_INDEX;

  if (paramType == QueryNodeParameters::QN_SCAN_INDEX)
    attrInfo.alloc(QN_ScanIndexParameters::NodeSize);
  else if (paramType == QueryNodeParameters::QN_SCAN_FRAG)
    attrInfo.alloc(QN_ScanFragParameters::NodeSize);
  else
    attrInfo.alloc(QN_LookupParameters::NodeSize);

  // SPJ block assume PARAMS to be supplied before ATTR_LIST
  if (m_params.getSize() > 0 &&
      def.getType() != NdbQueryOperationDef::UniqueIndexAccess)
  {
    // parameter values has been serialized as part of NdbTransaction::createQuery()
    // Only need to append it to rest of the serialized arguments
    requestInfo |= DABits::PI_KEY_PARAMS;
    attrInfo.append(m_params);    
  }

  if (hasInterpretedCode())
  {
    requestInfo |= DABits::PI_ATTR_INTERPRET;
    const int error= prepareInterpretedCode(attrInfo);
    if (unlikely(error)) 
    {
      return error;
    }
  }

  if (m_ndbRecord==NULL && m_firstRecAttr==NULL)
  {
    // Leaf operations with empty projections are not supported.
    if (getNoOfChildOperations() == 0)
    {
      return QRY_EMPTY_PROJECTION;
    }
  }
  else
  {
    requestInfo |= DABits::PI_ATTR_LIST;
    const int error = serializeProject(attrInfo);
    if (unlikely(error)) {
      return error;
    }
  }

  if (diskInUserProjection())
  {
    requestInfo |= DABits::PI_DISK_ATTR;
  }

  Uint32 length = attrInfo.getSize() - startPos;
  if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; //Query definition too large.
  }

  if (paramType == QueryNodeParameters::QN_SCAN_INDEX)
  {
    QN_ScanIndexParameters* param = reinterpret_cast<QN_ScanIndexParameters*>(attrInfo.addr(startPos)); 
    if (unlikely(param==NULL))
      return Err_MemoryAlloc;

    Ndb& ndb = *m_queryImpl.getNdbTransaction().getNdb();

    Uint32 batchRows = getMaxBatchRows();
    Uint32 batchByteSize;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      m_queryImpl.getRootFragCount(),
                                      batchRows,
                                      batchByteSize);
    assert(batchRows == getMaxBatchRows());
    assert(batchRows <= batchByteSize);
    assert(m_parallelism == Parallelism_max ||
           m_parallelism == Parallelism_adaptive);
    if (m_parallelism == Parallelism_max)
    {
      requestInfo |= QN_ScanIndexParameters::SIP_PARALLEL;
    }
    if (def.hasParamInPruneKey())
    {
      requestInfo |= QN_ScanIndexParameters::SIP_PRUNE_PARAMS;
    }
    param->requestInfo = requestInfo;
    // Check that both values fit in param->batchSize.
    assert(getMaxBatchRows() < (1<<QN_ScanIndexParameters::BatchRowBits));
    assert(batchByteSize < (1 << (sizeof param->batchSize * 8
                                  - QN_ScanIndexParameters::BatchRowBits)));
    param->batchSize = (batchByteSize << 11) | getMaxBatchRows();
    param->resultData = getIdOfReceiver();
    QueryNodeParameters::setOpLen(param->len, paramType, length);
  }
  else if (paramType == QueryNodeParameters::QN_SCAN_FRAG)
  {
    QN_ScanFragParameters* param = reinterpret_cast<QN_ScanFragParameters*>(attrInfo.addr(startPos)); 
    if (unlikely(param==NULL))
      return Err_MemoryAlloc;

    param->requestInfo = requestInfo;
    param->resultData = getIdOfReceiver();
    QueryNodeParameters::setOpLen(param->len, paramType, length);
  }
  else
  {
    assert(paramType == QueryNodeParameters::QN_LOOKUP);
    QN_LookupParameters* param = reinterpret_cast<QN_LookupParameters*>(attrInfo.addr(startPos)); 
    if (unlikely(param==NULL))
      return Err_MemoryAlloc;

    param->requestInfo = requestInfo;
    param->resultData = getIdOfReceiver();
    QueryNodeParameters::setOpLen(param->len, paramType, length);
  }

#ifdef __TRACE_SERIALIZATION
  ndbout << "Serialized params for node " 
         << getInternalOpNo() << " : ";
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


int 
NdbQueryOperationImpl::prepareKeyInfo(
                     Uint32Buffer& keyInfo,
                     const NdbQueryParamValue* actualParam)
{
  assert(this == &getRoot()); // Should only be called for root operation.
#ifdef TRACE_SERIALIZATION
  int startPos = keyInfo.getSize();
#endif

  const NdbQueryOperationDefImpl::IndexBound* bounds = m_operationDef.getBounds();
  if (bounds)
  {
    const int error = prepareIndexKeyInfo(keyInfo, bounds, actualParam);
    if (unlikely(error))
      return error;
  }

  const NdbQueryOperandImpl* const* keys = m_operationDef.getKeyOperands();
  if (keys)
  {
    const int error = prepareLookupKeyInfo(keyInfo, keys, actualParam);
    if (unlikely(error))
      return error;
  }

  if (unlikely(keyInfo.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  }

#ifdef TRACE_SERIALIZATION
  ndbout << "Serialized KEYINFO for NdbQuery root : ";
  for (Uint32 i = startPos; i < keyInfo.getSize(); i++) {
    char buf[12];
    sprintf(buf, "%.8x", keyInfo.get(i));
    ndbout << buf << " ";
  }
  ndbout << endl;
#endif

  return 0;
} // NdbQueryOperationImpl::prepareKeyInfo


/**
 * Convert constant operand into sequence of words that may be sent to data
 * nodes.
 * @param constOp Operand to convert.
 * @param buffer Destination buffer.
 * @param len Will be set to length in bytes.
 * @return 0 if ok, otherwise error code.
 */
static int
serializeConstOp(const NdbConstOperandImpl& constOp,
                 Uint32Buffer& buffer,
                 Uint32& len)
{
  // Check that column->shrink_varchar() not specified, only used by mySQL
  // assert (!(column->flags & NdbDictionary::RecMysqldShrinkVarchar));
  buffer.skipRestOfWord();
  len = constOp.getSizeInBytes();
  Uint8 shortLen[2];
  switch (constOp.getColumn()->getArrayType()) {
    case NdbDictionary::Column::ArrayTypeFixed:
      buffer.appendBytes(constOp.getAddr(), len);
      break;

    case NdbDictionary::Column::ArrayTypeShortVar:
      // Such errors should have been caught in convert2ColumnType().
      assert(len <= 0xFF);
      shortLen[0] = (unsigned char)len;
      buffer.appendBytes(shortLen, 1);
      buffer.appendBytes(constOp.getAddr(), len);
      len+=1;
      break;

    case NdbDictionary::Column::ArrayTypeMediumVar:
      // Such errors should have been caught in convert2ColumnType().
      assert(len <= 0xFFFF);
      shortLen[0] = (unsigned char)(len & 0xFF);
      shortLen[1] = (unsigned char)(len >> 8);
      buffer.appendBytes(shortLen, 2);
      buffer.appendBytes(constOp.getAddr(), len);
      len+=2;
      break;

    default:
      assert(false);
  }
  if (unlikely(buffer.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  }
  return 0;
} // static serializeConstOp

static int
appendBound(Uint32Buffer& keyInfo,
            NdbIndexScanOperation::BoundType type, const NdbQueryOperandImpl* bound,
            const NdbQueryParamValue* actualParam) 
{
  Uint32 len = 0;

  keyInfo.append(type);
  const Uint32 oldSize = keyInfo.getSize();
  keyInfo.append(0); // Place holder for AttributeHeader

  switch(bound->getKind()){
  case NdbQueryOperandImpl::Const:
  {
    const NdbConstOperandImpl& constOp = 
      static_cast<const NdbConstOperandImpl&>(*bound);

    const int error = serializeConstOp(constOp, keyInfo, len);
    if (unlikely(error))
      return error;

    break;
  }
  case NdbQueryOperandImpl::Param:
  {
    const NdbParamOperandImpl* const paramOp 
      = static_cast<const NdbParamOperandImpl*>(bound);
    const int paramNo = paramOp->getParamIx();
    assert(actualParam != NULL);

    bool null;
    const int error = 
      actualParam[paramNo].serializeValue(*paramOp->getColumn(), keyInfo,
                                          len, null);
    if (unlikely(error))
      return error;
    if (unlikely(null))
      return Err_KeyIsNULL;
    break;
  }
  case NdbQueryOperandImpl::Linked:    // Root operation cannot have linked operands.
  default:
    assert(false);
  }
    
  // Back patch attribute header.
  keyInfo.put(oldSize, 
              AttributeHeader(bound->getColumn()->m_attrId, len).m_value);

  return 0;
} // static appendBound()


int
NdbQueryOperationImpl::prepareIndexKeyInfo(
                     Uint32Buffer& keyInfo,
                     const NdbQueryOperationDefImpl::IndexBound* bounds,
                     const NdbQueryParamValue* actualParam)
{
  int startPos = keyInfo.getSize();
  if (bounds->lowKeys==0 && bounds->highKeys==0)  // No Bounds defined
    return 0;

  const unsigned key_count = 
     (bounds->lowKeys >= bounds->highKeys) ? bounds->lowKeys : bounds->highKeys;

  for (unsigned keyNo = 0; keyNo < key_count; keyNo++)
  {
    NdbIndexScanOperation::BoundType bound_type;

    /* If upper and lower limit is equal, a single BoundEQ is sufficient */
    if (keyNo < bounds->lowKeys  &&
        keyNo < bounds->highKeys &&
        bounds->low[keyNo] == bounds->high[keyNo])
    {
      /* Inclusive if defined, or matching rows can include this value */
      bound_type= NdbIndexScanOperation::BoundEQ;
      int error = appendBound(keyInfo, bound_type, bounds->low[keyNo], actualParam);
      if (unlikely(error))
        return error;

    } else {

      /* If key is part of lower bound */
      if (keyNo < bounds->lowKeys)
      {
        /* Inclusive if defined, or matching rows can include this value */
        bound_type= bounds->lowIncl  || keyNo+1 < bounds->lowKeys ?
            NdbIndexScanOperation::BoundLE : NdbIndexScanOperation::BoundLT;

        int error = appendBound(keyInfo, bound_type, bounds->low[keyNo], actualParam);
        if (unlikely(error))
          return error;
      }

      /* If key is part of upper bound */
      if (keyNo < bounds->highKeys)
      {
        /* Inclusive if defined, or matching rows can include this value */
        bound_type= bounds->highIncl  || keyNo+1 < bounds->highKeys ?
            NdbIndexScanOperation::BoundGE : NdbIndexScanOperation::BoundGT;

        int error = appendBound(keyInfo, bound_type, bounds->high[keyNo], actualParam);
        if (unlikely(error))
          return error;
      }
    }
  }

  Uint32 length = keyInfo.getSize()-startPos;
  if (unlikely(keyInfo.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  } else if (unlikely(length > 0xFFFF)) {
    return QRY_DEFINITION_TOO_LARGE; // Query definition too large.
  } else if (likely(length > 0)) {
    keyInfo.put(startPos, keyInfo.get(startPos) | (length << 16));
  }

  m_queryImpl.m_shortestBound =(bounds->lowKeys <= bounds->highKeys) ? bounds->lowKeys : bounds->highKeys;
  return 0;
} // NdbQueryOperationImpl::prepareIndexKeyInfo


int
NdbQueryOperationImpl::prepareLookupKeyInfo(
                     Uint32Buffer& keyInfo,
                     const NdbQueryOperandImpl* const keys[],
                     const NdbQueryParamValue* actualParam)
{
  const int keyCount = m_operationDef.getIndex()!=NULL ? 
    static_cast<int>(m_operationDef.getIndex()->getNoOfColumns()) :
    m_operationDef.getTable().getNoOfPrimaryKeys();

  for (int keyNo = 0; keyNo<keyCount; keyNo++)
  {
    Uint32 dummy;

    switch(keys[keyNo]->getKind()){
    case NdbQueryOperandImpl::Const:
    {
      const NdbConstOperandImpl* const constOp 
        = static_cast<const NdbConstOperandImpl*>(keys[keyNo]);
      const int error = 
        serializeConstOp(*constOp, keyInfo, dummy);
      if (unlikely(error))
        return error;

      break;
    }
    case NdbQueryOperandImpl::Param:
    {
      const NdbParamOperandImpl* const paramOp 
        = static_cast<const NdbParamOperandImpl*>(keys[keyNo]);
      int paramNo = paramOp->getParamIx();
      assert(actualParam != NULL);

      bool null;
      const int error = 
        actualParam[paramNo].serializeValue(*paramOp->getColumn(), keyInfo, 
                                            dummy, null);

      if (unlikely(error))
        return error;
      if (unlikely(null))
        return Err_KeyIsNULL;
      break;
    }
    case NdbQueryOperandImpl::Linked:    // Root operation cannot have linked operands.
    default:
      assert(false);
    }
  }

  if (unlikely(keyInfo.isMemoryExhausted())) {
    return Err_MemoryAlloc;
  }

  return 0;
} // NdbQueryOperationImpl::prepareLookupKeyInfo


bool 
NdbQueryOperationImpl::execTRANSID_AI(const Uint32* ptr, Uint32 len)
{
  TupleCorrelation tupleCorrelation;
  NdbRootFragment* rootFrag = m_queryImpl.m_rootFrags;

  if (getQueryDef().isScanQuery())
  {
    const CorrelationData correlData(ptr, len);
    const Uint32 receiverId = correlData.getRootReceiverId();
    
    /** receiverId holds the Id of the receiver of the corresponding stream
     * of the root operation. We can thus find the correct root fragment 
     * number.
     */
    rootFrag = 
      NdbRootFragment::receiverIdLookup(m_queryImpl.m_rootFrags,
                                        m_queryImpl.getRootFragCount(), 
                                        receiverId);
    if (unlikely(rootFrag == NULL))
    {
      assert(false);
      return false;
    }

    // Extract tuple correlation.
    tupleCorrelation = correlData.getTupleCorrelation();
    len -= CorrelationData::wordCount;
  }

  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI()" 
           << ", fragment no: " << rootFrag->getFragNo()
           << ", operation no: " << getQueryOperationDef().getOpNo()
           << endl;
  }

  // Process result values.
  rootFrag->getResultStream(*this).execTRANSID_AI(ptr, len, tupleCorrelation);
  rootFrag->incrOutstandingResults(-1);

  bool ret = false;
  if (rootFrag->isFragBatchComplete())
  {
    ret = m_queryImpl.handleBatchComplete(*rootFrag);
  }

  if (false && traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI(): returns:" << ret
           << ", *this=" << *this <<  endl;
  }
  return ret;
} //NdbQueryOperationImpl::execTRANSID_AI


bool 
NdbQueryOperationImpl::execTCKEYREF(const NdbApiSignal* aSignal)
{
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTCKEYREF()" <<  endl;
  }

  /* The SPJ block does not forward TCKEYREFs for trees with scan roots.*/
  assert(!getQueryDef().isScanQuery());

  const TcKeyRef* ref = CAST_CONSTPTR(TcKeyRef, aSignal->getDataPtr());
  if (!getQuery().m_transaction.checkState_TransId(ref->transId))
  {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
    return false;
  }

  // Suppress 'TupleNotFound' status for child operations.
  if (&getRoot() == this || 
      ref->errorCode != static_cast<Uint32>(Err_TupleNotFound))
  {
    if (aSignal->getLength() == TcKeyRef::SignalLength)
    {
      // Signal may contain additional error data
      getQuery().m_error.details = (char *)UintPtr(ref->errorData);
    }
    getQuery().setFetchTerminated(ref->errorCode,false);
  }

  NdbRootFragment& rootFrag = getQuery().m_rootFrags[0];

  /**
   * Error may be either a 'soft' or a 'hard' error.
   * 'Soft error' are regarded 'informational', and we are
   * allowed to continue execution of the query. A 'hard error'
   * will terminate query, close comminication, and further
   * incomming signals to this NdbReceiver will be discarded.
   */
  switch (ref->errorCode)
  {
  case Err_TupleNotFound:  // 'Soft error' : Row not found
  case Err_FalsePredicate: // 'Soft error' : Interpreter_exit_nok
  {
    /**
     * Need to update 'outstanding' count:
     * Compensate for children results not produced.
     * (doSend() assumed all child results to be materialized)
     */
    Uint32 cnt = 1; // self
    cnt += getNoOfDescendantOperations();
    if (getNoOfChildOperations() > 0)
    {
      cnt += getNoOfLeafOperations();
    }
    rootFrag.incrOutstandingResults(- Int32(cnt));
    break;
  }
  default:                             // 'Hard error':
    rootFrag.throwRemainingResults();  // Terminate receive -> complete
  }

  bool ret = false;
  if (rootFrag.isFragBatchComplete())
  { 
    ret = m_queryImpl.handleBatchComplete(rootFrag);
  } 

  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTCKEYREF(): returns:" << ret
           << ", *this=" << *this <<  endl;
  }
  return ret;
} //NdbQueryOperationImpl::execTCKEYREF

bool
NdbQueryOperationImpl::execSCAN_TABCONF(Uint32 tcPtrI, 
                                        Uint32 rowCount,
                                        Uint32 nodeMask,
                                        NdbReceiver* receiver)
{
  assert((tcPtrI==RNIL && nodeMask==0) || 
         (tcPtrI!=RNIL && nodeMask!=0));
  assert(checkMagicNumber());
  // For now, only the root operation may be a scan.
  assert(&getRoot() == this);
  assert(m_operationDef.isScanOperation());

  NdbRootFragment* rootFrag = 
    NdbRootFragment::receiverIdLookup(m_queryImpl.m_rootFrags,
                                      m_queryImpl.getRootFragCount(), 
                                      receiver->getId());
  if (unlikely(rootFrag == NULL))
  {
    assert(false);
    return false;
  }
  // Prepare for SCAN_NEXTREQ, tcPtrI==RNIL, nodeMask==0 -> EOF
  rootFrag->setConfReceived(tcPtrI);
  rootFrag->setRemainingSubScans(nodeMask);
  rootFrag->incrOutstandingResults(rowCount);

  if(traceSignals){
    ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF"
           << " fragment no: " << rootFrag->getFragNo()
           << " rows " << rowCount
           << " nodeMask: H'" << hex << nodeMask << ")"
           << " tcPtrI " << tcPtrI
           << endl;
  }

  bool ret = false;
  if (rootFrag->isFragBatchComplete())
  {
    /* This fragment is now complete */
    ret = m_queryImpl.handleBatchComplete(*rootFrag);
  }
  if (false && traceSignals) {
    ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF():, returns:" << ret
           << ", tcPtrI=" << tcPtrI << " rowCount=" << rowCount 
           << " *this=" << *this << endl;
  }
  return ret;
} //NdbQueryOperationImpl::execSCAN_TABCONF

int
NdbQueryOperationImpl::setOrdering(NdbQueryOptions::ScanOrdering ordering)
{
  if (getQueryOperationDef().getType() != NdbQueryOperationDef::OrderedIndexScan)
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }

  if (m_parallelism != Parallelism_max)
  {
    getQuery().setErrorCode(QRY_SEQUENTIAL_SCAN_SORTED);
    return -1;
  }

  if(static_cast<const NdbQueryIndexScanOperationDefImpl&>
       (getQueryOperationDef())
     .getOrdering() != NdbQueryOptions::ScanOrdering_void)
  {
    getQuery().setErrorCode(QRY_SCAN_ORDER_ALREADY_SET);
    return -1;
  }
  
  m_ordering = ordering;
  return 0;
} // NdbQueryOperationImpl::setOrdering()

int NdbQueryOperationImpl::setInterpretedCode(const NdbInterpretedCode& code)
{
  if (code.m_instructions_length == 0)
  {
    return 0;
  }

  const NdbTableImpl& table = getQueryOperationDef().getTable();
  // Check if operation and interpreter code use the same table
  if (unlikely(table.getTableId() != code.getTable()->getTableId()
               || table_version_major(table.getObjectVersion()) != 
               table_version_major(code.getTable()->getObjectVersion())))
  {
    getQuery().setErrorCode(Err_InterpretedCodeWrongTab);
    return -1;
  }

  if (unlikely((code.m_flags & NdbInterpretedCode::Finalised) 
               == 0))
  {
    //  NdbInterpretedCode::finalise() not called.
    getQuery().setErrorCode(Err_FinaliseNotCalled);
    return -1;
  }

  // Allocate an interpreted code object if we do not have one already.
  if (likely(m_interpretedCode == NULL))
  {
    m_interpretedCode = new NdbInterpretedCode();

    if (unlikely(m_interpretedCode==NULL))
    {
      getQuery().setErrorCode(Err_MemoryAlloc);
      return -1;
    }
  }

  /* 
   * Make a deep copy, such that 'code' can be destroyed when this method 
   * returns.
   */
  const int error = m_interpretedCode->copy(code);
  if (unlikely(error))
  {
    getQuery().setErrorCode(error);
    return -1;
  }
  return 0;
} // NdbQueryOperationImpl::setInterpretedCode()

int NdbQueryOperationImpl::setParallelism(Uint32 parallelism){
  if (!getQueryOperationDef().isScanOperation())
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }
  else if (getOrdering() == NdbQueryOptions::ScanOrdering_ascending ||
           getOrdering() == NdbQueryOptions::ScanOrdering_descending)
  {
    getQuery().setErrorCode(QRY_SEQUENTIAL_SCAN_SORTED);
    return -1;
  }
  else if (getQueryOperationDef().getOpNo() > 0)
  {
    getQuery().setErrorCode(Err_FunctionNotImplemented);
    return -1;
  }
  else if (parallelism < 1 || parallelism > NDB_PARTITION_MASK)
  {
    getQuery().setErrorCode(Err_ParameterError);
    return -1;
  }
  m_parallelism = parallelism;
  return 0;
}

int NdbQueryOperationImpl::setMaxParallelism(){
  if (!getQueryOperationDef().isScanOperation())
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }
  m_parallelism = Parallelism_max;
  return 0;
}

int NdbQueryOperationImpl::setAdaptiveParallelism(){
  if (!getQueryOperationDef().isScanOperation())
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }
  else if (getQueryOperationDef().getOpNo() == 0)
  {
    getQuery().setErrorCode(Err_FunctionNotImplemented);
    return -1;
  }
  m_parallelism = Parallelism_adaptive;
  return 0;
}

int NdbQueryOperationImpl::setBatchSize(Uint32 batchSize){
  if (!getQueryOperationDef().isScanOperation())
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }
  if (this != &getRoot() && 
      batchSize < getQueryOperationDef().getTable().getFragmentCount())
  {
    /** Each SPJ block instance will scan each fragment, so the batch size
     * cannot be smaller than the number of fragments.*/
    getQuery().setErrorCode(QRY_BATCH_SIZE_TOO_SMALL);
    return -1;
  }
  m_maxBatchRows = batchSize;
  return 0;
}

bool
NdbQueryOperationImpl::hasInterpretedCode() const
{
  return (m_interpretedCode && m_interpretedCode->m_instructions_length > 0) ||
         (getQueryOperationDef().getInterpretedCode() != NULL);
} // NdbQueryOperationImpl::hasInterpretedCode

int
NdbQueryOperationImpl::prepareInterpretedCode(Uint32Buffer& attrInfo) const
{
  const NdbInterpretedCode* interpretedCode =
    (m_interpretedCode && m_interpretedCode->m_instructions_length > 0)
     ? m_interpretedCode
     : getQueryOperationDef().getInterpretedCode();

  // There should be no subroutines in a filter.
  assert(interpretedCode->m_first_sub_instruction_pos==0);
  assert(interpretedCode->m_instructions_length > 0);
  assert(interpretedCode->m_instructions_length <= 0xffff);

  // Allocate space for program and length field.
  Uint32* const buffer = 
    attrInfo.alloc(1+interpretedCode->m_instructions_length);
  if(unlikely(buffer==NULL))
  {
    return Err_MemoryAlloc;
  }

  buffer[0] = interpretedCode->m_instructions_length;
  memcpy(buffer+1, 
         interpretedCode->m_buffer, 
         interpretedCode->m_instructions_length * sizeof(Uint32));
  return 0;
} // NdbQueryOperationImpl::prepareInterpretedCode


Uint32 
NdbQueryOperationImpl::getIdOfReceiver() const {
  NdbRootFragment& rootFrag = m_queryImpl.m_rootFrags[0];
  return rootFrag.getResultStream(*this).getReceiver().getId();
}

Uint32 NdbQueryOperationImpl::getRowSize() const
{
  // Check if row size has been computed yet.
  if (m_rowSize == 0xffffffff)
  {
    m_rowSize = 
      NdbReceiver::ndbrecord_rowsize(m_ndbRecord, false);
  }
  return m_rowSize;
}

Uint32 NdbQueryOperationImpl::getBatchBufferSize() const
{
  // Check if batch buffer size has been computed yet.
  if (m_batchBufferSize == 0xffffffff)
  {
    Uint32 batchRows = getMaxBatchRows();
    Uint32 batchByteSize = 0;
    Uint32 batchFrags = 1;

    if (m_operationDef.isScanOperation())
    {
      const Ndb* const ndb = getQuery().getNdbTransaction().getNdb();
      NdbReceiver::calculate_batch_size(* ndb->theImpl,
                                        getQuery().getRootFragCount(),
                                        batchRows,
                                        batchByteSize);
      assert(batchRows == getMaxBatchRows());

      /**
       * When LQH reads a scan batch, the size of the batch is limited 
       * both to a maximal number of rows and a maximal number of bytes.
       * The latter limit is interpreted such that the batch ends when the 
       * limit has been exceeded. Consequently, the buffer must be able to
       * hold max_no_of_bytes plus one extra row. In addition,  when the
       * SPJ block executes a (pushed) child scan operation, it scans a 
       * number of fragments (possibly all) in parallel, and divides the
       * row and byte limits by the number of parallel fragments.
       * Consequently, a child scan operation may return max_no_of_bytes,
       * plus one extra row for each fragment.
       */
      if (getParentOperation() != NULL)
      {
        batchFrags = getQuery().getRootFragCount();
      }
    }

    AttributeMask readMask;
    if (m_ndbRecord != NULL)
    {
      m_ndbRecord->copyMask(readMask.rep.data, m_read_mask);
    }

    m_batchBufferSize = NdbReceiver::result_bufsize(
                                                  batchRows, 
                                                  batchByteSize,
                                                  batchFrags,
                                                  m_ndbRecord,
                                                  readMask.rep.data,
                                                  m_firstRecAttr,
                                                  0, 0);
  }
  return m_batchBufferSize;
}

/** For debugging.*/
NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl& op){
  out << "[ this: " << &op
      << "  m_magic: " << op.m_magic;
  out << " op.operationDef.getOpNo()"
      << op.m_operationDef.getOpNo();
  if (op.getParentOperation()){
    out << "  m_parent: " << op.getParentOperation(); 
  }
  for(unsigned int i = 0; i<op.getNoOfChildOperations(); i++){
    out << "  m_children[" << i << "]: " << &op.getChildOperation(i); 
  }
  out << "  m_queryImpl: " << &op.m_queryImpl;
  out << "  m_operationDef: " << &op.m_operationDef;
  out << " m_isRowNull " << op.m_isRowNull;
  out << " ]";
  return out;
}

NdbOut& operator<<(NdbOut& out, const NdbResultStream& stream){
  out << " received rows: " << stream.m_resultSets[stream.m_recv].getRowCount();
  return out;
}

 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;
