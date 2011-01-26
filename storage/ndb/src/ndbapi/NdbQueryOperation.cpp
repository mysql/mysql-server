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


#include <ndb_global.h>
#include "API.hpp"
#include <NdbQueryBuilder.hpp>
#include "NdbQueryBuilderImpl.hpp"
#include "NdbQueryOperationImpl.hpp"

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

//#define TEST_SCANREQ

/* Various error codes that are not specific to NdbQuery. */
static const int Err_TupleNotFound = 626;
static const int Err_MemoryAlloc = 4000;
static const int Err_SendFailed = 4002;
static const int Err_FunctionNotImplemented = 4003;
static const int Err_UnknownColumn = 4004;
static const int Err_ReceiveTimedOut = 4008;
static const int Err_NodeFailCausedAbort = 4028;
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
const bool traceSignals = false;

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
    assert(getTupleId()<tupleNotFound);
    assert(getParentTupleId()<tupleNotFound);
  }

  Uint32 getRootReceiverId() const
  { return m_corrPart[2];}

  Uint16 getTupleId() const
  { return m_corrPart[1] & 0xffff;}

  Uint16 getParentTupleId() const
  { return m_corrPart[1] >> 16;}

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

  /**
   * Initialize object.
   * @param query Enclosing query.
   * @param fragNo This object manages state for reading from the fragNo'th 
   * fragment that the root operation accesses.
   */
  void init(NdbQueryImpl& query, Uint32 fragNo); 

  Uint32 getFragNo() const
  { return m_fragNo; }

  /**
   * Prepare for receiving another batch.
   */
  void reset();

  void incrOutstandingResults(Int32 delta)
  {
    m_outstandingResults += delta;
  }

  void clearOutstandingResults()
  {
    m_outstandingResults = 0;
  }

  void setConfReceived();

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
  NdbResultStream& getResultStream(Uint32 operationNo) const
  { return m_query->getQueryOperation(operationNo).getResultStream(m_fragNo); }
  
  /**
   * @return True if there are no more batches to be received for this fragment.
   */
  bool finalBatchReceived() const;

  /**
   * @return True if there are no more results from this root fragment (for 
   * the current batch).
   */
  bool isEmpty() const;

private:
  STATIC_CONST( voidFragNo = 0xffffffff);

  /** Enclosing query.*/
  NdbQueryImpl* m_query;

  /** Number of the root operation fragment.*/
  Uint32 m_fragNo;

  /**
   * The number of outstanding TCKEYREF or TRANSID_AI 
   * messages for the fragment. This includes both messages related to the
   * root operation and any descendant operation that was instantiated as
   * a consequence of tuples found by the root operation.
   * This number may temporarily be negative if e.g. TRANSID_AI arrives before 
   * SCAN_TABCONF. 
   */
  Int32 m_outstandingResults;

  /**
   * This is an array with one element for each fragment that the root
   * operation accesses (i.e. one for a lookup, all for a table scan).
   *
   * Each element is true iff a SCAN_TABCONF (for that fragment) or 
   * TCKEYCONF message has been received */
  bool m_confReceived;

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
  explicit NdbResultStream(NdbQueryOperationImpl& operation, Uint32 rootFragNo);

  ~NdbResultStream();

  /** 
   * Prepare for receiving first results. 
   * @return possible error code. 
   */
  int prepare();

  /** Prepare for receiving next batch of scan results. */
  void reset();
    
  /**
   * 0..n-1 if the root operation reads from n fragments. This stream holds data
   * derived from one of those fragments.
   */
  Uint32 getRootFragNo() const
  { return m_rootFragNo; }

  NdbReceiver& getReceiver()
  { return m_receiver; }

  const NdbReceiver& getReceiver() const
  { return m_receiver; }

  Uint32 getRowCount() const
  { return m_rowCount; }

  /**
   * Process an incomming tuple for this stream. Extract parent and own tuple 
   * ids and pass it on to m_receiver.
   *
   * @param ptr buffer holding tuple.
   * @param len buffer length.
   */
  void execTRANSID_AI(const Uint32 *ptr, Uint32 len);

  /** A complete batch has been received for a fragment on this NdbResultStream,
   *  Update whatever required before the appl. are allowed to navigate the result.
   */ 
  void handleBatchComplete();

  /**
   * Navigate within the current result batch to resp. first and next row.
   * For non-parent operations in the pushed query, navigation is with respect
   * to any preceding parents which results in this NdbResultStream depends on.
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
   * This method is only used for result streams of scan operations. It is
   * used for marking a stream as holding the last batch of a sub scan. 
   * This means that it is the last batch of the scan that was instantiated 
   * from the current batch of its parent operation.
   */
  void setSubScanComplete(bool complete)
  { 
    assert(m_operation.getQueryOperationDef().isScanOperation());
    m_subScanComplete = complete; 
  }

  /** 
   * This method is only relevant for result streams of scan operations. It 
   * returns true if this result stream holds the last batch of a sub scan
   * This means that it is the last batch of the scan that was instantiated 
   * from the current batch of its parent operation.
   */
  bool isSubScanComplete() const
  { 
    assert(m_operation.getQueryOperationDef().isScanOperation());
    return m_subScanComplete; 
  }

  /** Variant of isSubScanComplete() above which checks that this resultstream
   * and all its descendants have consumed all batches of rows instantiated 
   * from their parent operation(s). */
  bool isAllSubScansComplete() const;

  /** For debugging.*/
  friend NdbOut& operator<<(NdbOut& out, const NdbResultStream&);

private:
  /** This stream handles results derived from the m_rootFragNo'th 
   * fragment of the root operation.*/
  const Uint32 m_rootFragNo;

  /** The receiver object that unpacks transid_AI messages.*/
  NdbReceiver m_receiver;

  /** Max #rows which this stream may recieve in its buffer structures */
  Uint32 m_maxRows;

  /** The number of transid_AI messages received.*/
  Uint32 m_rowCount;

  /** Operation to which this resultStream belong.*/
  NdbQueryOperationImpl& m_operation;

  /** This is the state of the iterator used by firstResult(), nextResult().*/
  enum
  {
    /** The first row has not been fetched yet.*/
    Iter_notStarted,
    /** Is iterating the resultset, (implies 'm_currentRow!=tupleNotFound').*/
    Iter_started,  
    /** Last row for current batch has been returned.*/
    Iter_finished
  } m_iterState;

  /** Tuple id of the current tuple, or 'tupleNotFound' if Iter_notStarted or Iter_finished. */
  Uint16 m_currentRow;
  
  /** 
   * This field is only used for result streams of scan operations. If set,
   * it indicates that the stream is holding the last batch of a sub scan. 
   * This means that it is the last batch of the scan that was instantiated 
   * from the current batch of its parent operation.
   */
  bool m_subScanComplete;

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
    Bitmask<NDB_SPJ_MAX_TREE_NODES> m_hasMatchingChild;

    explicit TupleSet()
    {}

  private:
    /** No copying.*/
    TupleSet(const TupleSet&);
    TupleSet& operator=(const TupleSet&);
  };

  TupleSet* m_tupleSet;

  void clearTupleSet();

  void setParentChildMap(Uint16 parentId,
                         Uint16 tupleId, 
                         Uint16 tupleNo);

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
/////////  NdbResultStream methods ///////////
//////////////////////////////////////////////

NdbResultStream::NdbResultStream(NdbQueryOperationImpl& operation, Uint32 rootFragNo):
  m_rootFragNo(rootFragNo),
  m_receiver(operation.getQuery().getNdbTransaction().getNdb(), &operation),
  m_maxRows(0),
  m_rowCount(0),
  m_operation(operation),
  m_iterState(Iter_notStarted),
  m_currentRow(tupleNotFound),
  m_subScanComplete(false),
  m_tupleSet(NULL)
{};

NdbResultStream::~NdbResultStream() { 
  delete[] m_tupleSet; 
}

int  // Return 0 if ok, else errorcode
NdbResultStream::prepare()
{
  /* Parent / child correlation is only relevant for scan type queries
   * Don't create m_parentTupleId[] and m_childTupleIdx[] for lookups!
   * Neither is these structures required for operations not having respective
   * child or parent operations.
   */
  if (m_operation.getQueryDef().isScanQuery())
  {
    m_maxRows  = m_operation.getMaxBatchRows();
    m_tupleSet = new TupleSet[m_maxRows];
    if (unlikely(m_tupleSet==NULL))
      return Err_MemoryAlloc;

    clearTupleSet();
  }
  else
    m_maxRows = 1;

  return 0;
} //NdbResultStream::prepare


void
NdbResultStream::reset()
{
  assert (m_operation.getQueryDef().isScanQuery());

  // Root scan-operation need a ScanTabConf to complete
  m_rowCount = 0;
  m_iterState = Iter_notStarted;
  m_currentRow = tupleNotFound;

  clearTupleSet();
  m_receiver.prepareSend();
  /* If this stream will get new rows in the next batch, then so will
   * all of its descendants.*/
  for (Uint32 childNo = 0; childNo < m_operation.getNoOfChildOperations();
       childNo++)
  {
    NdbQueryOperationImpl& child = m_operation.getChildOperation(childNo);
    child.getResultStream(getRootFragNo()).reset();
  }
} //NdbResultStream::reset

void
NdbResultStream::clearTupleSet()
{
  assert (m_operation.getQueryDef().isScanQuery());
  for (Uint32 i=0; i<m_maxRows; i++)
  {
    m_tupleSet[i].m_parentId = tupleNotFound;
    m_tupleSet[i].m_tupleId  = tupleNotFound;
    m_tupleSet[i].m_hash_head = tupleNotFound;
    m_tupleSet[i].m_skip = false;
    m_tupleSet[i].m_hasMatchingChild.clear();
  }
}

bool
NdbResultStream::isAllSubScansComplete() const
{ 
  if (m_operation.getQueryOperationDef().isScanOperation() && 
      !m_subScanComplete)
    return false;

  for (Uint32 childNo = 0; childNo < m_operation.getNoOfChildOperations(); 
       childNo++)
  {
    const NdbQueryOperationImpl& child = m_operation.getChildOperation(childNo);
    const NdbResultStream& childStream = child.getResultStream(getRootFragNo());
    if (!childStream.isAllSubScansComplete())
      return false;
  }
  return true;
} //NdbResultStream::isAllSubScansComplete


void
NdbResultStream::setParentChildMap(Uint16 parentId,
                                   Uint16 tupleId, 
                                   Uint16 tupleNo)
{
  assert (m_operation.getQueryDef().isScanQuery());
  assert (tupleNo < m_maxRows);
  assert (tupleId != tupleNotFound);

  for (Uint32 i = 0; i < tupleNo; i++)
  {
    // Check that tuple id is unique.
    assert (m_tupleSet[i].m_tupleId != tupleId); 
  }
  m_tupleSet[tupleNo].m_parentId = parentId;
  m_tupleSet[tupleNo].m_tupleId  = tupleId;

  const Uint16 hash = (parentId % m_maxRows);
  if (parentId == tupleNotFound)
  {
    /* Root stream: Insert sequentially in hash_next to make it
     * possible to use ::findTupleWithParentId() and ::findNextTuple()
     * to navigate even the root operation.
     */
    assert (m_operation.getParentOperation()==NULL);
    /* Link into m_hash_next in order to let ::findNextTuple() navigate correctly */
    if (tupleNo==0)
      m_tupleSet[hash].m_hash_head  = 0;
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

/** Locate, and return 'tupleNo', of first tuple with specified parentId.
 *  parentId == tupleNotFound is use as a special value for iterating results
 *  from the root operation in the order which they was inserted by ::setParentChildMap()
 *
 *  Position of 'currentRow' is *not* updated and should be modified by callee
 *  if it want to keep the new position.
 */
Uint16
NdbResultStream::findTupleWithParentId(Uint16 parentId) const
{
  assert ((parentId==tupleNotFound) == (m_operation.getParentOperation()==NULL));

  if (likely(m_rowCount>0))
  {
    if (m_tupleSet==NULL)
    {
      assert (m_rowCount <= 1);
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
  NdbQueryOperationImpl* parent = m_operation.getParentOperation();

  Uint16 parentId = tupleNotFound;
  if (parent!=NULL)
  {
    const NdbResultStream& parentStream = parent->getResultStream(m_rootFragNo);
    parentId = parentStream.getCurrentTupleId();

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
    m_receiver.setCurrentRow(m_currentRow);
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
    m_receiver.setCurrentRow(m_currentRow);
    return m_currentRow;
  }
  m_iterState = Iter_finished;
  return tupleNotFound;
} //NdbResultStream::nextResult()


void
NdbResultStream::execTRANSID_AI(const Uint32 *ptr, Uint32 len)
{
  assert(m_iterState == Iter_notStarted);
  if (m_operation.getQueryDef().isScanQuery())
  {
    const CorrelationData correlData(ptr, len);

    assert(m_operation.getRoot().getResultStream(m_rootFragNo)
           .m_receiver.getId() == correlData.getRootReceiverId());

    m_receiver.execTRANSID_AI(ptr, len - CorrelationData::wordCount);

    /**
     * Keep correlation data between parent and child tuples.
     * Since tuples may arrive in any order, we cannot match
     * parent and child until all tuples (for this batch and 
     * root fragment) have arrived.
     */
    setParentChildMap(m_operation.getParentOperation()==NULL
                      ? tupleNotFound
                      : correlData.getParentTupleId(),
                      correlData.getTupleId(),
                      m_rowCount);
  }
  else
  {
    // Lookup query.
    m_receiver.execTRANSID_AI(ptr, len);
  }
  m_rowCount++;
  /* Set correct #rows received in the NdbReceiver.
   */
  getReceiver().m_result_rows = getRowCount();
} // NdbResultStream::execTRANSID_AI()


/**
 * A fresh batch of results has arrived for this ResultStream (and all its parent / childs)
 * Filter away any result rows which should not be visible (yet) - Either due to incomplete
 * child batches, or the join being an 'inner join'.
 * Set result itterator state to 'before first' resultrow.
 */
void 
NdbResultStream::handleBatchComplete()
{
  for (Uint32 tupleNo=0; tupleNo<getRowCount(); tupleNo++)
  {
    m_tupleSet[tupleNo].m_skip = false;
  }

  for (Uint32 childNo=0; childNo < m_operation.getNoOfChildOperations(); childNo++)
  {
    const NdbQueryOperationImpl& child = m_operation.getChildOperation(childNo);
    NdbResultStream& childStream = child.getResultStream(m_rootFragNo);
    childStream.handleBatchComplete();

    const bool isInnerJoin = child.getQueryOperationDef().getMatchType() != NdbQueryOptions::MatchAll;
    const bool allSubScansComplete = childStream.isAllSubScansComplete();

    for (Uint32 tupleNo=0; tupleNo<getRowCount(); tupleNo++)
    {
      if (!m_tupleSet[tupleNo].m_skip)
      {
        Uint16 tupleId = getTupleId(tupleNo);
        if (childStream.findTupleWithParentId(tupleId)!=tupleNotFound)
          m_tupleSet[tupleNo].m_hasMatchingChild.set(childNo);

        /////////////////////////////////
        //  No child matched for this row. Making parent row visible
        //  will cause a NULL (outer join) row to be produced.
        //  Skip NULL row production when:
        //    1) Some child batches are not complete; they may contain later matches.
        //    2) A match was found in a previous batch.
        //    3) Join type is 'inner join', skip as no child are matching.
        //
        else if (!allSubScansComplete                                 // 1)
             ||  m_tupleSet[tupleNo].m_hasMatchingChild.get(childNo)  // 2)
             ||  isInnerJoin)                                         // 3)
          m_tupleSet[tupleNo].m_skip = true;
      }
    }
  }
  m_currentRow = tupleNotFound;
  m_iterState = Iter_notStarted;
} // NdbResultStream::handleBatchComplete()


///////////////////////////////////////////
/////////  NdbRootFragment methods ///////////
///////////////////////////////////////////
void NdbRootFragment::buildReciverIdMap(NdbRootFragment* frags, 
                                        Uint32 noOfFrags)
{
  for(Uint32 fragNo = 0; fragNo < noOfFrags; fragNo++)
  {
    const Uint32 receiverId = 
      frags[fragNo].getResultStream(0).getReceiver().getId();
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
  while (current >= 0 && 
         frags[current].getResultStream(0).getReceiver().getId() 
         != receiverId)
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
  m_outstandingResults(0),
  m_confReceived(false),
  m_idMapHead(-1),
  m_idMapNext(-1)
{
}

void NdbRootFragment::init(NdbQueryImpl& query, Uint32 fragNo)
{
  assert(m_fragNo==voidFragNo);
  m_query = &query;
  m_fragNo = fragNo;
}

void NdbRootFragment::reset()
{
  assert(m_fragNo!=voidFragNo);
  assert(m_outstandingResults == 0);
  assert(m_confReceived);
  m_confReceived = false;
}

void NdbRootFragment::setConfReceived()
{ 
  /* For a query with a lookup root, there may be more than one TCKEYCONF
     message. For a scan, there should only be one SCAN_TABCONF per root
     fragment. 
  */
  assert(!m_query->getQueryDef().isScanQuery() || !m_confReceived);
  m_confReceived = true; 
}

bool NdbRootFragment::finalBatchReceived() const
{
  return getResultStream(0).getReceiver().m_tcPtrI==RNIL;
}

bool  NdbRootFragment::isEmpty() const
{ 
  return getResultStream(0).isEmpty();
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
  m_queryDef(queryDef),
  m_error(),
  m_transaction(trans),
  m_scanTransaction(NULL),
  m_operations(0),
  m_countOperations(0),
  m_globalCursor(0),
  m_pendingFrags(0),
  m_rootFragCount(0),
  m_rootFrags(NULL),
  m_applFrags(),
  m_fullFrags(),
  m_finalBatchFrags(0),
  m_num_bounds(0),
  m_shortestBound(0xffffffff),
  m_attrInfo(),
  m_keyInfo(),
  m_startIndicator(false),
  m_commitIndicator(false),
  m_prunability(Prune_No),
  m_pruneHashVal(0)
{
  // Allocate memory for all m_operations[] in a single chunk
  m_countOperations = queryDef.getNoOfOperations();
  Uint32  size = m_countOperations * 
    static_cast<Uint32>(sizeof(NdbQueryOperationImpl));
  m_operations = static_cast<NdbQueryOperationImpl*> (::operator new(size));
  if (unlikely(m_operations == NULL))
  {
    setErrorCode(Err_MemoryAlloc);
    return;
  }

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
      for (Uint32 j=0; j<=i; j++)
      { 
        m_operations[j].~NdbQueryOperationImpl();
      }
      ::operator delete(m_operations);
      m_operations = NULL;
      return;
    }
  }

  // Serialized QueryTree definition is first part of ATTRINFO.
  m_attrInfo.append(queryDef.getSerialized());
}

NdbQueryImpl::~NdbQueryImpl()
{
 
  // Do this to check that m_queryDef still exists.
  assert(getNoOfOperations() == m_queryDef.getNoOfOperations());

  // NOTE: m_operations[] was allocated as a single memory chunk with
  // placement new construction of each operation.
  // Requires explicit call to d'tor of each operation before memory is free'ed.
  if (m_operations != NULL) {
    for (int i=m_countOperations-1; i>=0; --i)
    { m_operations[i].~NdbQueryOperationImpl();
    }
    ::operator delete(m_operations);
    m_operations = NULL;
  }
  delete[] m_rootFrags;
  m_rootFrags = NULL;
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
  if (unlikely(query==NULL)) {
    trans.setOperationErrorCodeAbort(Err_MemoryAlloc);
    return NULL;
  }
  if (unlikely(query->m_error.code != 0))
  {
    // Transaction error code set already.
    delete query;
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
  if (unlikely(bound==NULL))
    return QRY_REQ_ARG_IS_NULL;

  assert (getRoot().getQueryOperationDef().getType() 
          == NdbQueryOperationDef::OrderedIndexScan);
  int startPos = m_keyInfo.getSize();

  // We don't handle both NdbQueryIndexBound defined in ::scanIndex()
  // in combination with a later ::setBound(NdbIndexScanOperation::IndexBound)
//assert (m_bound.lowKeys==0 && m_bound.highKeys==0);

  if (unlikely(bound->range_no > NdbIndexScanOperation::MaxRangeNo))
  {
 // setErrorCodeAbort(4286);
    return Err_InvalidRangeNo;
  }
  assert (bound->range_no == m_num_bounds);
  m_num_bounds++;

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

  assert(m_state<=Defined);
  m_state = Defined;
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
  /* To minimize lock contention, each query has two separate root fragment 
   * conatiners (m_fullFrags and m_applFrags). m_applFrags is only
   * accessed by the application thread, so it is safe to use it without 
   * locks.
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
      assert(rootFrag->isFragBatchComplete());
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
      // Ask for a new batch if we emptied one.
      NdbRootFragment* emptyFrag = m_applFrags.getEmpty();
      while (emptyFrag != NULL)
      {
        if (sendFetchMore(*emptyFrag, forceSend) != 0)
        {
          return NdbQuery::NextResult_error;
        }        
        emptyFrag = m_applFrags.getEmpty();
      }
    }

    if (rootFrag!=NULL)
    {
      assert(rootFrag->isFragBatchComplete());
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
  if (m_queryDef.isScanQuery())
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
        /* m_fullFrags contains any fragments that are complete (for this batch)
         * but have not yet been moved (under mutex protection) to 
         * m_applFrags.
         */
        NdbRootFragment* frag;
        while ((frag=m_fullFrags.pop()) != NULL)
        {
          m_applFrags.add(*frag);
        }
        if (m_applFrags.getCurrent() != NULL)
        {
          return FetchResult_ok;
        }

        /* There are noe more available frament results available without
         * first waiting for more to be received from datanodes
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
     * be accessing m_fullFrags at this time.
     */
    NdbRootFragment* frag;
    if ((frag=m_fullFrags.pop()) != NULL)
    {
      m_applFrags.add(*frag);
    }
    assert(m_fullFrags.pop()==NULL); // Only one stream for lookups.

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
  } // if(m_queryDef.isScanQuery())

} //NdbQueryImpl::awaitMoreResults


/*
  ::handleBatchComplete() is intended to be called when receiving signals only.
  The PollGuard mutex is then set and the shared 'm_pendingFrags', 
  'm_finalBatchFrags' and 'm_fullFrags' can safely be updated.

  returns: 'true' when application thread should be resumed.
*/
bool 
NdbQueryImpl::handleBatchComplete(Uint32 fragNo)
{
  if (traceSignals) {
    ndbout << "NdbQueryImpl::handleBatchComplete, fragNo=" << fragNo
           << ", pendingFrags=" << (m_pendingFrags-1)
           << ", finalBatchFrags=" << m_finalBatchFrags
           <<  endl;
  }
  bool resume = false;

  /* May received fragment data after a SCANREF() (timeout?) 
   * terminated the scan.  We are about to close this query, 
   * and didn't expect any more data - ignore it!
   */
  if (likely(m_fullFrags.m_errorCode == 0))
  {
    NdbQueryOperationImpl& root = getRoot();
    NdbRootFragment& rootFrag = m_rootFrags[fragNo];
    assert(rootFrag.isFragBatchComplete());

    assert(m_pendingFrags > 0);                // Check against underflow.
    assert(m_pendingFrags <= m_rootFragCount); // .... and overflow
    m_pendingFrags--;

    if (rootFrag.finalBatchReceived())
    {
      m_finalBatchFrags++;
      assert(m_finalBatchFrags <= m_rootFragCount);
    }

    if (getQueryDef().isScanQuery())
    {
      // Only required for scans
      root.getResultStream(fragNo).handleBatchComplete();  

      // Only ordered scans has to wait until all pending completed
      resume = (m_pendingFrags==0) ||
               (root.m_ordering==NdbQueryOptions::ScanOrdering_unordered);
    }
    else
    {
      assert(root.m_resultStreams[fragNo]->getReceiver().m_tcPtrI==RNIL);
      assert(m_finalBatchFrags==1);
      assert(m_pendingFrags==0);  // Lookup query should be complete now.
      resume = true;   
    }

    /* Position at the first (sorted?) row available from this fragments.
     */
    root.m_resultStreams[fragNo]->firstResult();

    /* When application thread ::awaitMoreResults() it will later be moved
     * from m_fullFrags to m_applFrags under mutex protection.
     */
    m_fullFrags.push(rootFrag);
  }

  return resume;
} // NdbQueryImpl::handleBatchComplete

int
NdbQueryImpl::close(bool forceSend)
{
  int res = 0;

  assert (m_state >= Initial && m_state < Destructed);
  Ndb* const ndb = m_transaction.getNdb();

  if (m_tcState != Inactive)
  {
    /* We have started a scan, but we have not yet received the last batch
     * for all root fragments. We must therefore close the scan to release 
     * the scan context at TC.*/
    res = closeTcCursor(forceSend);
  }

  // Throw any pending results
  m_fullFrags.clear();
  m_applFrags.clear();

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
    /** 
     * Theses are application errorsthat means that a give method invocation fails,
     * but there is no need to abort the transaction.
     */
  case Err_FunctionNotImplemented:
  case Err_UnknownColumn:
  case Err_WrongFieldLength:
  case Err_InvalidRangeNo:
  case Err_DifferentTabForKeyRecAndAttrRec:
  case Err_KeyIsNULL:
  case QRY_REQ_ARG_IS_NULL:
  case QRY_PARAMETER_HAS_WRONG_TYPE:
  case QRY_RESULT_ROW_ALREADY_DEFINED:
  case QRY_CHAR_OPERAND_TRUNCATED:
  case QRY_WRONG_OPERATION_TYPE:
  case QRY_SEQUENTIAL_SCAN_SORTED:
  case QRY_SCAN_ORDER_ALREADY_SET:
  case QRY_MULTIPLE_SCAN_SORTED:
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
    m_fullFrags.m_errorCode = errorCode;
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
  if (unlikely(m_fullFrags.m_errorCode))
  {
    setErrorCode(m_fullFrags.m_errorCode);
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

  // We will get 1 + #leaf-nodes TCKEYCONF for a lookup...
  m_rootFrags[0].setConfReceived();
  m_rootFrags[0].incrOutstandingResults(-1);

  bool ret = false;
  if (m_rootFrags[0].isFragBatchComplete())
  { 
    ret = handleBatchComplete(0);
  }

  if (traceSignals) {
    ndbout << "NdbQueryImpl::execTCKEYCONF(): returns:" << ret
           << ", m_pendingFrags=" << m_pendingFrags
           << ", *getRoot().m_resultStreams[0]=" 
           << *getRoot().m_resultStreams[0]
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
    if (getQueryOperation(0U).m_parallelism > 0)
    {
      m_rootFragCount
        = MIN(getRoot().getQueryOperationDef().getTable().getFragmentCount(),
              getQueryOperation(0U).m_parallelism);
    }
    else
    {
      m_rootFragCount
        = getRoot().getQueryOperationDef().getTable().getFragmentCount();
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

  // Some preparation for later batchsize calculations pr. (sub) scan
  getRoot().calculateBatchedRows(NULL);
  getRoot().setBatchedRows(1);

  // 1. Build receiver structures for each QueryOperation.
  // 2. Fill in parameters (into ATTRINFO) for QueryTree.
  //    (Has to complete *after* ::prepareReceiver() as QueryTree params
  //     refer receiver id's.)
  //
  for (Uint32 i = 0; i < m_countOperations; i++) {
    int error;
    if (unlikely((error = m_operations[i].prepareReceiver()) != 0)
              || (error = m_operations[i].prepareAttrInfo(m_attrInfo)) != 0) {
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
  int error;
  if (unlikely((error = m_applFrags.prepare(getRoot().getOrdering(),
                                              m_rootFragCount, 
                                              keyRec,
                                              getRoot().m_ndbRecord)) != 0)
            || (error = m_fullFrags.prepare(m_rootFragCount)) != 0) {
    setErrorCode(error);
    return -1;
  }

  /**
   * Allocate and initialize fragment state variables.
   */
  m_rootFrags = new NdbRootFragment[m_rootFragCount];
  if(m_rootFrags == NULL)
  {
    setErrorCode(Err_MemoryAlloc);
    return -1;
  }
  else
  {
    for(Uint32 i = 0; i<m_rootFragCount; i++)
    {
      m_rootFrags[i].init(*this, i); // Set fragment number.
    }
  }

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
  
  InitialReceiverIdIterator(const NdbQueryImpl& query)
    :m_query(query),
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
  /** The query with the scan root operation that we list receiver ids for.*/
  const NdbQueryImpl& m_query;
  /** The next fragment numnber to be processed. (Range for 0 to no of 
   * fragments.)*/
  Uint32 m_currFragNo;
  /** Buffer for storing one batch of receiver ids.*/
  Uint32 m_receiverIds[bufSize];
};

const Uint32* InitialReceiverIdIterator::getNextWords(Uint32& sz)
{
  sz = 0;
  /**
   * For the initial batch, we want to retrieve one batch for each fragment
   * whether it is a sorted scan or not.
   */
  if (m_currFragNo >= m_query.getRootFragCount())
  {
    return NULL;
  }
  else
  {
    const NdbQueryOperationImpl& root = m_query.getQueryOperation(0U);
    while (sz < bufSize && 
           m_currFragNo < m_query.getRootFragCount())
    {
      m_receiverIds[sz] = root.getReceiver(m_currFragNo).getId();
      sz++;
      m_currFragNo++;
    }
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

    Uint32 batchRows = root.getMaxBatchRows();
    Uint32 batchByteSize, firstBatchRows;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      root.m_ndbRecord,
                                      root.m_firstRecAttr,
                                      0, // Key size.
                                      getRootFragCount(),
                                      batchRows,
                                      batchByteSize,
                                      firstBatchRows);
    assert(batchRows==root.getMaxBatchRows());
    assert(batchRows==firstBatchRows);
    ScanTabReq::setScanBatch(reqInfo, batchRows);
    scanTabReq->batch_byte_size = batchByteSize;
    scanTabReq->first_batch_size = firstBatchRows;

    ScanTabReq::setViaSPJFlag(reqInfo, 1);
    ScanTabReq::setPassAllConfsFlag(reqInfo, 1);
    ScanTabReq::setParallelism(reqInfo, getRootFragCount());
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
    InitialReceiverIdIterator receiverIdIter(*this);
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
    tSignal.setSignal(GSN_TCKEYREQ);

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
    tcKeyReq->setAPIVersion(attrLen, NDB_VERSION);
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
NdbQueryImpl::sendFetchMore(NdbRootFragment& emptyFrag, bool forceSend)
{
  assert(getRoot().m_resultStreams!=NULL);
  assert(!emptyFrag.finalBatchReceived());
  assert(m_queryDef.isScanQuery());

  emptyFrag.reset();

  for (unsigned opNo=0; opNo<m_countOperations; opNo++) 
  {
    const NdbQueryOperationImpl& op = getQueryOperation(opNo);
    // Check if this is a leaf scan.
    if (!op.getQueryOperationDef().hasScanDescendant() &&
        op.getQueryOperationDef().isScanOperation())
    {
      // Find first scan ancestor that is not finished.
      const NdbQueryOperationImpl* ancestor = &op;
      while (ancestor != NULL && 
             (!ancestor->getQueryOperationDef().isScanOperation() ||
              ancestor->getResultStream(emptyFrag.getFragNo())
              .isSubScanComplete())
              )
      {
        ancestor = ancestor->getParentOperation();
      }
      if (ancestor!=NULL)
      {
        /* Reset ancestor and all its descendants, since all these
         * streams will get a new set of rows in the next batch. */ 
        ancestor->getResultStream(emptyFrag.getFragNo()).reset();
      }
    }
  }

  Ndb& ndb = *getNdbTransaction().getNdb();
  NdbApiSignal tSignal(&ndb);
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
  ScanNextReq * const scanNextReq = 
    CAST_PTR(ScanNextReq, tSignal.getDataPtrSend());
  
  assert (m_scanTransaction);
  const Uint64 transId = m_scanTransaction->getTransactionId();
  
  scanNextReq->apiConnectPtr = m_scanTransaction->theTCConPtr;
  scanNextReq->stopScan = 0;
  scanNextReq->transId1 = (Uint32) transId;
  scanNextReq->transId2 = (Uint32) (transId >> 32);
  tSignal.setLength(ScanNextReq::SignalLength);

  const uint32 receiverId = 
    emptyFrag.getResultStream(0).getReceiver().m_tcPtrI;
  LinearSectionIterator receiverIdIter(&receiverId ,1);

  GenericSectionPtr secs[1];
  secs[ScanNextReq::ReceiverIdsSectionNum].sectionIter = &receiverIdIter;
  secs[ScanNextReq::ReceiverIdsSectionNum].sz = 1;
  
  NdbImpl * impl = ndb.theImpl;
  Uint32 nodeId = m_transaction.getConnectedNodeId();
  Uint32 seq    = m_transaction.theNodeSequence;

  /* This part needs to be done under mutex due to synchronization with 
   * receiver thread.
   */
  PollGuard poll_guard(* impl);

  if (unlikely(hasReceivedError()))
  {
    // Errors arrived inbetween ::await released mutex, and fetchMore grabbed it
    return -1;
  }
  if (impl->getNodeSequence(nodeId) != seq ||
      impl->sendSignal(&tSignal, nodeId, secs, 1) != 0)
  {
    setErrorCode(Err_NodeFailCausedAbort);
    return -1;
  }
  impl->do_forceSend(forceSend);

  m_pendingFrags++;
  assert(m_pendingFrags <= getRootFragCount());
  return 0;
} // NdbQueryImpl::sendFetchMore()

int
NdbQueryImpl::closeTcCursor(bool forceSend)
{
  assert (m_queryDef.isScanQuery());

  NdbImpl* const ndb = m_transaction.getNdb()->theImpl;
  NdbImpl* const impl = ndb;
  const Uint32 timeout  = ndb->get_waitfor_timeout();
  const Uint32 nodeId   = m_transaction.getConnectedNodeId();
  const Uint32 seq      = m_transaction.theNodeSequence;

  /* This part needs to be done under mutex due to synchronization with 
   * receiver thread.
   */
  PollGuard poll_guard(*ndb);

  if (unlikely(impl->getNodeSequence(nodeId) != seq))
  {
    setErrorCode(Err_NodeFailCausedAbort);
    return -1;  // Transporter disconnected and reconnected, no need to close
  }

  /* Wait for outstanding scan results from current batch fetch */
  while (m_pendingFrags > 0)
  {
    const FetchResult result = static_cast<FetchResult>
        (poll_guard.wait_scan(3*timeout, nodeId, forceSend));

    if (unlikely(impl->getNodeSequence(nodeId) != seq))
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
  m_fullFrags.clear();                         // Throw any unhandled results
  m_fullFrags.m_errorCode = 0;                 // Clear errors caused by previous fetching
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

      if (unlikely(impl->getNodeSequence(nodeId) != seq))
        setFetchTerminated(Err_NodeFailCausedAbort,false);
      if (unlikely(result != FetchResult_ok))
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
  tSignal.setSignal(GSN_SCAN_NEXTREQ);
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
 * NdbQueryImpl::SharedFragStack methods.
 ***************/

NdbQueryImpl::SharedFragStack::SharedFragStack():
  m_errorCode(0),
  m_capacity(0),
  m_current(-1),
  m_array(NULL)
{}

int
NdbQueryImpl::SharedFragStack::prepare(int capacity)
{
  assert(m_array==NULL);
  assert(m_capacity==0);
  if (capacity > 0) 
  { m_capacity = capacity;
    m_array = new NdbRootFragment*[capacity];
    if (unlikely(m_array==NULL))
      return Err_MemoryAlloc;
  }
  return 0;
}

void
NdbQueryImpl::SharedFragStack::push(NdbRootFragment& frag)
{
  m_current++;
  assert(m_current<m_capacity);
  m_array[m_current] = &frag; 
}

/****************
 * NdbQueryImpl::OrderedFragSet methods.
 ***************/

NdbQueryImpl::OrderedFragSet::OrderedFragSet():
  m_capacity(0),
  m_activeFragCount(0),
  m_emptiedFragCount(0),
  m_finalFragCount(0),
  m_ordering(NdbQueryOptions::ScanOrdering_void),
  m_keyRecord(NULL),
  m_resultRecord(NULL),
  m_activeFrags(NULL),
  m_emptiedFrags(NULL)
{
}

NdbQueryImpl::OrderedFragSet::~OrderedFragSet() 
{ 
  delete[] m_activeFrags;
  m_activeFrags = NULL;
  delete[] m_emptiedFrags;
  m_emptiedFrags= NULL;
}


int
NdbQueryImpl::OrderedFragSet::prepare(NdbQueryOptions::ScanOrdering ordering, 
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
    m_activeFrags = new NdbRootFragment*[capacity];
    if (unlikely(m_activeFrags==NULL))
      return Err_MemoryAlloc;
    bzero(m_activeFrags, capacity * sizeof(NdbRootFragment*));
    m_emptiedFrags = new NdbRootFragment*[capacity];
    if (unlikely(m_emptiedFrags==NULL))
      return Err_MemoryAlloc;
    bzero(m_emptiedFrags, capacity * sizeof(NdbRootFragment*));
  }
  m_ordering = ordering;
  m_keyRecord = keyRecord;
  m_resultRecord = resultRecord;
  return 0;
}


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
    // Results should be ordered.
    assert(verifySortOrder());
    /** 
     * Must have tuples for each (non-completed) fragment when doing ordered
     * scan.
     */
    if (unlikely(m_activeFragCount+m_finalFragCount < m_capacity))
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
}


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
  // Remove the current fragment if the batch has been emptied.
  if (m_activeFragCount>0 && m_activeFrags[m_activeFragCount-1]->isEmpty())
  {
    if (m_activeFrags[m_activeFragCount-1]->finalBatchReceived())
    {
      m_finalFragCount++;
    }
    else
    {
      m_emptiedFrags[m_emptiedFragCount++] = m_activeFrags[m_activeFragCount-1];
    }
    m_activeFragCount--;
    assert(m_activeFragCount==0 || 
           !m_activeFrags[m_activeFragCount-1]->isEmpty());
    assert(m_activeFragCount + m_emptiedFragCount + m_finalFragCount 
           <= m_capacity);
  }

  // Reorder fragments if this is a sorted scan.
  if (m_ordering!=NdbQueryOptions::ScanOrdering_unordered && 
      m_activeFragCount+m_finalFragCount == m_capacity)
  {
    /** 
     * This is a sorted scan. There are more data to be read from 
     * m_activeFrags[m_activeFragCount-1]. Move it to its proper place.
     */
    int first = 0;
    int last = m_activeFragCount-1;
    /* Use binary search to find the largest record that is smaller than or
     * equal to m_activeFrags[m_activeFragCount-1] */
    int middle = (first+last)/2;
    while(first<last)
    {
      assert(middle<m_activeFragCount);
      switch(compare(*m_activeFrags[m_activeFragCount-1], 
                     *m_activeFrags[middle]))
      {
      case -1:
        first = middle + 1;
        break;
      case 0:
        last = first = middle;
        break;
      case 1:
        last = middle;
        break;
      }
      middle = (first+last)/2;
    }

    assert(m_activeFragCount == 0 ||
           compare(*m_activeFrags[m_activeFragCount-1], 
                   *m_activeFrags[middle]) >= 0);

    if(middle < m_activeFragCount-1)
    {
      NdbRootFragment* const oldTop = m_activeFrags[m_activeFragCount-1];
      memmove(m_activeFrags+middle+1, 
              m_activeFrags+middle, 
              (m_activeFragCount - middle - 1) * sizeof(NdbRootFragment*));
      m_activeFrags[middle] = oldTop;
    }
    assert(verifySortOrder());
  }
}

void 
NdbQueryImpl::OrderedFragSet::add(NdbRootFragment& frag)
{
  assert(&frag!=NULL);

  if (frag.isEmpty())
  {
    if (frag.finalBatchReceived())
    {
      m_finalFragCount++;
    }
    else
    {
      m_emptiedFrags[m_emptiedFragCount++] = &frag;
    }
  }
  else
  {
    assert(m_activeFragCount+m_finalFragCount < m_capacity);
    if(m_ordering==NdbQueryOptions::ScanOrdering_unordered)
    {
      m_activeFrags[m_activeFragCount++] = &frag;
    }
    else
    {
      int current = 0;
      // Insert the new frag such that the array remains sorted.
      while(current<m_activeFragCount && 
            compare(frag, *m_activeFrags[current])==-1)
      {
        current++;
      }
      memmove(m_activeFrags+current+1,
              m_activeFrags+current,
              (m_activeFragCount - current) * sizeof(NdbRootFragment*));
      m_activeFrags[current] = &frag;
      m_activeFragCount++;
      assert(verifySortOrder());
    }
  }
  assert(m_activeFragCount==0 || 
         !m_activeFrags[m_activeFragCount-1]->isEmpty());
  assert(m_activeFragCount + m_emptiedFragCount + m_finalFragCount 
         <= m_capacity);
}

void NdbQueryImpl::OrderedFragSet::clear() 
{ 
  m_activeFragCount = 0;
  m_emptiedFragCount = 0; 
  m_finalFragCount = 0;
}

NdbRootFragment* 
NdbQueryImpl::OrderedFragSet::getEmpty()
{
  if (m_emptiedFragCount > 0)
  {
    assert(m_emptiedFrags[m_emptiedFragCount-1]->isEmpty());
    return m_emptiedFrags[--m_emptiedFragCount];
  }
  else
  {
    return NULL;
  }
}

bool 
NdbQueryImpl::OrderedFragSet::verifySortOrder() const
{
  for(int i = 0; i<m_activeFragCount-2; i++)
  {
    if(compare(*m_activeFrags[i], *m_activeFrags[i+1])==-1)
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
 * @return -1 if frag1<frag2, 0 if frag1 == frag2, otherwise 1.
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
  m_children(def.getNoOfChildOperations()),
  m_maxBatchRows(0),   // >0: User specified prefered value, ==0: Use default CFG values
  m_resultStreams(NULL),
  m_params(),
  m_bufferSize(0),
  m_batchBuffer(NULL),
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
  m_parallelism(0)
{ 
  if (errno == ENOMEM)
  {
    // Memory allocation in Vector() (for m_children) assumed to have failed.
    queryImpl.setErrorCode(Err_MemoryAlloc);
    return;
  }
  // Fill in operations parent refs, and append it as child of its parent
  const NdbQueryOperationDefImpl* parent = def.getParentOperation();
  if (parent != NULL)
  { 
    const Uint32 ix = parent->getQueryOperationIx();
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
  // We expect ::postFetchRelease to have deleted fetch related structures when fetch completed.
  // Either by fetching through last row, or calling ::close() which forcefully terminates fetch
  assert (m_batchBuffer == NULL);
  assert (m_resultStreams == NULL);
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
  if (m_batchBuffer) {
#ifndef NDEBUG // Buffer overrun check activated.
    { const Uint32 bufLen = m_bufferSize*m_queryImpl.getRootFragCount();
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
    for(Uint32 i = 0; i<getQuery().getRootFragCount(); i ++){
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
    NdbResultStream& resultStream = *m_resultStreams[rootFrag->getFragNo()];
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
      NdbResultStream& resultStream = *m_resultStreams[rootFrag->getFragNo()];
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
  const char* buff = resultStream.getReceiver().get_row();
  assert(buff!=NULL || (m_firstRecAttr==NULL && m_ndbRecord==NULL));

  m_isRowNull = false;
  if (m_firstRecAttr != NULL)
  {
    NdbRecAttr* recAttr = m_firstRecAttr;
    Uint32 posInRow = 0;
    while (recAttr != NULL)
    {
      const char *attrData = NULL;
      Uint32 attrSize = 0;
      const int retVal1 = resultStream.getReceiver()
        .getScanAttrData(attrData, attrSize, posInRow);
      UNUSED(retVal1);
      assert(retVal1==0);
      assert(attrData!=NULL);
      const bool retVal2 = recAttr
        ->receive_data(reinterpret_cast<const Uint32*>(attrData), attrSize);
      UNUSED(retVal2);
      assert(retVal2);
      recAttr = recAttr->next();
    }
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
      memcpy(m_resultBuffer, buff, 
             resultStream.getReceiver().m_record.m_ndb_record->m_row_size);
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
::calculateBatchedRows(NdbQueryOperationImpl* closestScan)
{
  NdbQueryOperationImpl* myClosestScan;
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

#ifdef TEST_SCANREQ
    m_maxBatchRows = 4;  // To force usage of SCAN_NEXTREQ even for small scans resultsets
#endif

    const Ndb& ndb = *getQuery().getNdbTransaction().getNdb();

    /**
     * For each batch, a lookup operation must be able to receive as many rows
     * as the closest ancestor scan operation. 
     * We must thus make sure that we do not set a batch size for the scan 
     * that exceeds what any of its scan descendants can use.
     *
     * Ignore calculated 'batchByteSize' and 'firstBatchRows' 
     * here - Recalculated when building signal after max-batchRows has been 
     * determined.
     */
    Uint32 batchByteSize, firstBatchRows;
    /**
     * myClosestScan->m_maxBatchRows may be zero to indicate that we
     * should use default values, or non-zero if the application had an 
     * explicit preference.
     */
    maxBatchRows = myClosestScan->m_maxBatchRows;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      m_ndbRecord,
                                      m_firstRecAttr,
                                      0, // Key size.
                                      m_queryImpl.getRootFragCount(),
                                      maxBatchRows,
                                      batchByteSize,
                                      firstBatchRows);
    assert(maxBatchRows > 0);
    assert(firstBatchRows == maxBatchRows);
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
NdbQueryOperationImpl::prepareReceiver()
{
  const Uint32 rowSize = 
    NdbReceiver::ndbrecord_rowsize(m_ndbRecord, m_firstRecAttr,0,false);
  m_bufferSize = rowSize * getMaxBatchRows();
//ndbout "m_bufferSize=" << m_bufferSize << endl;

  if (m_bufferSize > 0) { // 0 bytes in batch if no result requested
    Uint32 bufLen = m_bufferSize*m_queryImpl.getRootFragCount();
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
  assert(m_queryImpl.getRootFragCount() > 0);
  m_resultStreams = new NdbResultStream*[m_queryImpl.getRootFragCount()];
  if (unlikely(m_resultStreams == NULL)) {
    return Err_MemoryAlloc;
  }
  for(Uint32 i = 0; i<m_queryImpl.getRootFragCount(); i++) {
    m_resultStreams[i] = NULL;  // Init to legal contents for d'tor
  }
  for(Uint32 i = 0; i<m_queryImpl.getRootFragCount(); i++) {
    m_resultStreams[i] = new NdbResultStream(*this, i);
    if (unlikely(m_resultStreams[i] == NULL)) {
      return Err_MemoryAlloc;
    }
    const int error = m_resultStreams[i]->prepare();
    if (unlikely(error)) {
      return error;
    }

    m_resultStreams[i]->getReceiver().init(NdbReceiver::NDB_QUERY_OPERATION, 
                                        false, this);
    m_resultStreams[i]->getReceiver()
      .do_setup_ndbrecord(m_ndbRecord,
                          getMaxBatchRows(), 
                          0 /*key_size*/, 
                          0 /*read_range_no*/, 
                          rowSize,
                          &m_batchBuffer[m_bufferSize*i],
                          0);
    m_resultStreams[i]->getReceiver().prepareSend();
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
  Uint32 startPos = attrInfo.getSize();
  Uint32 requestInfo = 0;
  bool isRoot = (def.getQueryOperationIx()==0);

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

  if (m_ndbRecord!=NULL || m_firstRecAttr!=NULL)
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
    Uint32 batchByteSize, firstBatchRows;
    NdbReceiver::calculate_batch_size(* ndb.theImpl,
                                      m_ndbRecord,
                                      m_firstRecAttr,
                                      0, // Key size.
                                      m_queryImpl.getRootFragCount(),
                                      batchRows,
                                      batchByteSize,
                                      firstBatchRows);
    assert(batchRows==getMaxBatchRows());
    assert(batchRows==firstBatchRows);
    requestInfo |= QN_ScanIndexParameters::SIP_PARALLEL; // FIXME: SPJ always assume. SIP_PARALLEL
    param->requestInfo = requestInfo; 
    param->batchSize = ((Uint16)batchByteSize << 16) | (Uint16)firstBatchRows;
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
      if (unlikely(len > 0xFF))
        return QRY_CHAR_OPERAND_TRUNCATED;
      shortLen[0] = (unsigned char)len;
      buffer.appendBytes(shortLen, 1);
      buffer.appendBytes(constOp.getAddr(), len);
      len+=1;
      break;

    case NdbDictionary::Column::ArrayTypeMediumVar:
      if (unlikely(len > 0xFFFF))
        return QRY_CHAR_OPERAND_TRUNCATED;
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
  NdbRootFragment* rootFrag = m_queryImpl.m_rootFrags;
  Uint32 rootFragNo = 0;
  if (getQueryDef().isScanQuery())
  {
    const Uint32 receiverId = CorrelationData(ptr, len).getRootReceiverId();
    
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
    rootFragNo = rootFrag->getFragNo();
  }
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTRANSID_AI()" 
           << ", operation no: " << getQueryOperationDef().getQueryOperationIx()
           << ", fragment no: " << rootFragNo
           << endl;
  }

  // Process result values.
  m_resultStreams[rootFragNo]->execTRANSID_AI(ptr, len);

  rootFrag->incrOutstandingResults(-1);

  bool ret = false;
  if (rootFrag->isFragBatchComplete())
  {
    ret = m_queryImpl.handleBatchComplete(rootFragNo);
  }

  if (traceSignals) {
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
    getQuery().setErrorCode(ref->errorCode);
    if (aSignal->getLength() == TcKeyRef::SignalLength)
    {
      // Signal may contain additional error data
      getQuery().m_error.details = (char *)ref->errorData;
    }
  }

  Uint32 rootFragNo = 0;
  NdbRootFragment& rootFrag = getQuery().m_rootFrags[0];

  if (ref->errorCode != DbspjErr::NodeFailure)
  {
    // Compensate for children results not produced.
    // (doSend() assumed all child results to be materialized)
    Uint32 cnt = 0;
    cnt += 1; // self
    cnt += getNoOfDescendantOperations();
    if (getNoOfChildOperations() > 0)
    {
      cnt += getNoOfLeafOperations();
    }
    rootFrag.incrOutstandingResults(- Int32(cnt));
  }
  else
  {
    // consider frag-batch complete
    rootFrag.clearOutstandingResults();
  }

  bool ret = false;
  if (rootFrag.isFragBatchComplete())
  { 
    ret = m_queryImpl.handleBatchComplete(rootFragNo);
  } 

  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execTCKEYREF(): returns:" << ret
           << ", *getRoot().m_resultStreams[0] {" 
           << *getRoot().m_resultStreams[0] << "}"
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
  if (traceSignals) {
    ndbout << "NdbQueryOperationImpl::execSCAN_TABCONF(rows: " << rowCount
           << " nodeMask: H'" << hex << nodeMask << ")" << endl;
  }
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
  rootFrag->setConfReceived();
  rootFrag->incrOutstandingResults(rowCount);

  // Handle for SCAN_NEXTREQ, RNIL -> EOF
  NdbResultStream& resultStream = *m_resultStreams[rootFrag->getFragNo()];
  resultStream.getReceiver().m_tcPtrI = tcPtrI;  

  if(traceSignals){
    ndbout << "  resultStream(root) {" << resultStream << "} fragNo" 
           << rootFrag->getFragNo() << endl;
  }

  const NdbQueryDefImpl& queryDef = m_queryImpl.getQueryDef();
  /* Mark each scan node to indicate if the current batch is the last in the
   * current sub-scan or not.
   */
  for (Uint32 opNo = 0; opNo < queryDef.getNoOfOperations(); opNo++)
  {
    const NdbQueryOperationImpl& op = m_queryImpl.getQueryOperation(opNo);
    /* Find the node number seen by the SPJ block. Since a unique index
     * operation will have two distincts nodes in the tree used by the
     * SPJ block, this number may be different from 'opNo'.*/
    const Uint32 internalOpNo = op.getQueryOperationDef().getQueryOperationId();
    assert(internalOpNo >= opNo);
    const bool maskSet = ((nodeMask >> internalOpNo) & 1) == 1;

    if (op.getQueryOperationDef().isScanOperation())
    {
      rootFrag->getResultStream(opNo).setSubScanComplete(!maskSet);
    }
    else
    {
      assert(!maskSet);
    }
  }
  // Check that nodeMask does not have more bits than we have operations. 
  assert(nodeMask >> 
         (1+queryDef.getQueryOperation(queryDef.getNoOfOperations() - 1)
          .getQueryOperationId()) == 0);

  bool ret = false;
  if (rootFrag->isFragBatchComplete())
  {
    /* This fragment is now complete */
    ret = m_queryImpl.handleBatchComplete(rootFrag->getFragNo());
  }
  if (traceSignals) {
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

  if (m_parallelism != 0)
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
  
  /* Check if query is sorted and has multiple scan operations. This 
   * combination is not implemented.
   */
  if (ordering != NdbQueryOptions::ScanOrdering_unordered)
  {
    for (Uint32 i = 1; i < getQuery().getNoOfOperations(); i++)
    {
      if (getQuery().getQueryOperation(i).getQueryOperationDef()
          .isScanOperation())
      {
        getQuery().setErrorCode(QRY_MULTIPLE_SCAN_SORTED);
        return -1;
      }
    }
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
  else if (getQueryOperationDef().getQueryOperationIx() > 0)
  {
    getQuery().setErrorCode(Err_FunctionNotImplemented);
    return -1;
  }
  m_parallelism = parallelism;
  return 0;
}

int NdbQueryOperationImpl::setBatchSize(Uint32 batchSize){
  if (!getQueryOperationDef().isScanOperation())
  {
    getQuery().setErrorCode(QRY_WRONG_OPERATION_TYPE);
    return -1;
  }

  m_maxBatchRows = batchSize;
  return 0;
}

NdbResultStream& 
NdbQueryOperationImpl::getResultStream(Uint32 rootFragNo) const
{
  assert(rootFragNo < getQuery().getRootFragCount());
  return *m_resultStreams[rootFragNo];
}

bool
NdbQueryOperationImpl::hasInterpretedCode() const
{
  return m_interpretedCode && m_interpretedCode->m_instructions_length > 0;
} // NdbQueryOperationImpl::hasInterpretedCode

int
NdbQueryOperationImpl::prepareInterpretedCode(Uint32Buffer& attrInfo) const
{
  // There should be no subroutines in a filter.
  assert(m_interpretedCode->m_first_sub_instruction_pos==0);
  assert(m_interpretedCode->m_instructions_length > 0);
  assert(m_interpretedCode->m_instructions_length <= 0xffff);

  // Allocate space for program and length field.
  Uint32* const buffer = 
    attrInfo.alloc(1+m_interpretedCode->m_instructions_length);
  if(unlikely(buffer==NULL))
  {
    return Err_MemoryAlloc;
  }

  buffer[0] = m_interpretedCode->m_instructions_length;
  memcpy(buffer+1, 
         m_interpretedCode->m_buffer, 
         m_interpretedCode->m_instructions_length * sizeof(Uint32));
  return 0;
} // NdbQueryOperationImpl::prepareInterpretedCode


Uint32 
NdbQueryOperationImpl::getIdOfReceiver() const {
  return m_resultStreams[0]->getReceiver().getId();
}


const NdbReceiver& 
NdbQueryOperationImpl::getReceiver(Uint32 recNo) const {
  assert(recNo<getQuery().getRootFragCount());
  assert(m_resultStreams!=NULL);
  return m_resultStreams[recNo]->getReceiver();
}


/** For debugging.*/
NdbOut& operator<<(NdbOut& out, const NdbQueryOperationImpl& op){
  out << "[ this: " << &op
      << "  m_magic: " << op.m_magic;
  out << " op.operationDef.getQueryOperationIx()" 
      << op.m_operationDef.getQueryOperationIx();
  if (op.getParentOperation()){
    out << "  m_parent: " << op.getParentOperation(); 
  }
  for(unsigned int i = 0; i<op.getNoOfChildOperations(); i++){
    out << "  m_children[" << i << "]: " << &op.getChildOperation(i); 
  }
  out << "  m_queryImpl: " << &op.m_queryImpl;
  out << "  m_operationDef: " << &op.m_operationDef;
  for(Uint32 i = 0; i<op.m_queryImpl.getRootFragCount(); i++){
    out << "  m_resultStream[" << i << "]{" << *op.m_resultStreams[i] << "}";
  }
  out << " m_isRowNull " << op.m_isRowNull;
  out << " ]";
  return out;
}

NdbOut& operator<<(NdbOut& out, const NdbResultStream& stream){
  out << " m_rowCount: " << stream.m_rowCount;
  return out;
}

 
// Compiler settings require explicit instantiation.
template class Vector<NdbQueryOperationImpl*>;
