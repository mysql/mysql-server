/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <cstring>
#include <NdbTick.h>
#include <NdbOut.hpp>
#include "API.hpp"

#include <AttributeHeader.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/TcCommit.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/TcHbRep.hpp>
#include <signaldata/TcRollbackRep.hpp>

static const Uint64 InvalidTransactionId = ~Uint64(0);

/**
 * Blob operation prep checks
 * Check that a batch of blob operations can be
 * safely prepared together in a single batch.
 */
class BlobBatchChecker
{
  /* Type of ops defined in the batch so far */
  Uint32 m_batchOpTypes;

  /**
   * Keep track of how tables are accessed
   */
  struct AccessMethod
  {
    Uint32 m_tableId;
    Uint32 m_indexId;
  };

  static const Uint32 NoIndexId = 0;
  static const Uint32 MultiIndexId = ~Uint32(0);

  Vector<AccessMethod> m_tables;

  /**
   * findIndexForTable
   *
   * Check which index(es) (if any) have been used
   * to access the given table in this batch
   */
  Uint32 findIndexForTable(const Uint32 tableId) const
  {
    DBUG_ENTER("BlobBatchChecker::findIndexForTable");
    assert(tableId != 0);
    /* Linear list */
    for (Uint32 t=0; t < m_tables.size(); t++)
    {
      if (m_tables[t].m_tableId == tableId)
      {
        assert(m_tables[t].m_indexId != NoIndexId);
        DBUG_PRINT("info", ("found tableId %u indexId %u",
                            m_tables[t].m_tableId,
                            m_tables[t].m_indexId));
        DBUG_RETURN(m_tables[t].m_indexId);
      }
    }
    DBUG_PRINT("info", ("Table %u not found", tableId));
    DBUG_RETURN(NoIndexId); /* Not found */
  }

  /**
   * Add table to batch checker, with access via given index
   */
  void addIndexForTable(const Uint32 tableId, const Uint32 indexId)
  {
    DBUG_ENTER("BlobBatchChecker::addIndexForTable");
    assert(tableId != 0);
    assert(indexId != 0);
#ifdef VM_TRACE
    // Must not already be in the table list
    for (Uint32 i=0; i < m_tables.size(); i++)
    {
      assert(m_tables[i].m_tableId != tableId);
    }
#endif

    AccessMethod am;
    am.m_tableId = tableId;
    am.m_indexId = indexId;

    m_tables.push_back(am);
    DBUG_PRINT("info", ("Added tableId %u indexId %u as entry %u",
                        tableId,
                        indexId,
                        m_tables.size()-1));
    DBUG_VOID_RETURN;
  }

  /**
   * Indicate that table is accessed via multiple indices
   */
  void setMultiIndexForTable(const Uint32 tableId)
  {
    DBUG_ENTER("BlobBatchChecker::setMultiIndexForTable");
    for (Uint32 t=0; t < m_tables.size(); t++)
    {
      if (m_tables[t].m_tableId == tableId)
      {
        DBUG_PRINT("info", ("Setting tableId %u index to Multi",
                            tableId));
        m_tables[t].m_indexId = MultiIndexId;
        DBUG_VOID_RETURN;
      }
    }
    abort();
    DBUG_VOID_RETURN;
  }

  /**
   * Keep track of keys used for blob operations
   */
  static const Uint32 NumBuckets = 50;
  NdbBlob* m_buckets[NumBuckets];

  /**
   * Check whether the batch contains another blob
   * operation with the same table, index + key
   *
   * Returns:
   * 0 - found
   * 1 - not found
   * -1 - error
   */
  int findKey(NdbBlob* blob) const
  {
    DBUG_ENTER("BlobBatchChecker::findKey");
    const Uint32 hash = blob->getBlobKeyHash();
    const Uint32 bucketIdx = hash % NumBuckets;

    NdbBlob* candidate = m_buckets[bucketIdx];
    while (candidate != nullptr)
    {
      if (candidate->getBlobKeyHash() == hash)
      {
        int ret = candidate->getBlobKeysEqual(blob);
        if (ret <= 0)
        {
          /* Found or error */
          DBUG_RETURN(ret);
        }
      }

      candidate = candidate->getBlobHashNext();
    }

    /* Not found */
    DBUG_RETURN(1);
  }

  /**
   * Add Blob to hash of table, index, key
   */
  void addKey(NdbBlob* blob)
  {
    DBUG_ENTER("BlobBatchChecker::addKey");
    const Uint32 hash = blob->getBlobKeyHash();
    const Uint32 bucketIdx = hash % NumBuckets;

    DBUG_PRINT("info", ("Adding blob %p to bucket idx %u current blob %p",
                        blob,
                        bucketIdx,
                        m_buckets[bucketIdx]));

    /* Add to head of bucket */
    blob->setBlobHashNext(m_buckets[bucketIdx]);
    m_buckets[bucketIdx] = blob;
    DBUG_VOID_RETURN;
  }


public:
  BlobBatchChecker():
    m_batchOpTypes(0),
    m_tables(10)
  {
    DBUG_ENTER("BlobBatchChecker::BlobBatchChecker");
    /* TODO : Avoid dynamic allocation
     *        Allow hash bucket count growth
     */
    reset();
    DBUG_VOID_RETURN;
  }

  ~BlobBatchChecker()
  {
  }

  /**
   * Reset, ready for next (sub) batch
   */
  void reset()
  {
    DBUG_ENTER("BlobBatchChecker::reset");
    m_batchOpTypes = 0;
    m_tables.clear();
    for (Uint32 b=0; b < NumBuckets; b++)
    {
      m_buckets[b] = nullptr;
    }
    DBUG_VOID_RETURN;
  }

  /**
   * Determine whether the nextOp can safely be added
   * to the current batch
   */
  bool includeNextOpInBatch(NdbOperation* nextOp)
  {
    DBUG_ENTER("BlobBatchChecker::includeNextOpInBatch");

    /**
     * Prefer to use cheaper checks (type safety, first op on table
     * etc) to avoid full key comparison.
     */
    bool include = false;
    if (!nextOp->theBlobList)
    {
      DBUG_PRINT("info", ("Non-blob op, including"));
      include = true;
    }
    else
    {
      DBUG_PRINT("info", ("Blob op, checking"));
      NdbBlob* firstBlob = nextOp->theBlobList;
      const Uint32 opType = firstBlob->getOpType();
      const Uint32 tableId = firstBlob->theTable->m_id;
      const Uint32 indexId = (Uint32) firstBlob->theAccessTable->m_id;

      /**
       * Lookup index used for any previous ops on this table in
       * this batch
       */
      const Uint32 existingIndexId = findIndexForTable(tableId);

      const bool firstAccess = (existingIndexId == NoIndexId);
      const bool singleIndex = (existingIndexId == indexId);

      DBUG_PRINT("info", ("opType %u tableId %u indexId %u typeSafe %u "
                          "firstAccess %u singleIndex %u",
                          opType,
                          tableId,
                          indexId,
                          NdbBlob::isOpTypeSafeWithBatch(m_batchOpTypes,
                                                         opType),
                          firstAccess,
                          singleIndex));

      if (firstAccess)
      {
        /* First blob op on this table in this batch : ok */
        DBUG_PRINT("info", ("Including as this is the first op"));
        include = true;
      }
      else
      {
        /**
         * Check type safety - in some cases this is
         * enough to proceed
         */
        if (NdbBlob::isOpTypeSafeWithBatch(m_batchOpTypes,
                                           opType))
        {
          DBUG_PRINT("info", ("Including as op is type safe"));
          include = true;
        }
        else
        {
          if (singleIndex)
          {
            /* Check whether key has been seen before */
            include = (findKey(firstBlob) == 1);  // Not found, ok to include
            DBUG_PRINT("info", ("Checked key : include : %u", include));
          }
        }
      }

      if (include)
      {
        m_batchOpTypes |= opType;
        /* Update tables list */
        if (firstAccess)
        {
          addIndexForTable(tableId,
                           indexId);
        }
        else if (!singleIndex)
        {
          /* Record multi-index access */
          setMultiIndexForTable(tableId);
        }
        /* else index already set */

        /* Potential optimisation :
         * if opTypeSafe then defer adding the key to the hash
         * until someone tries to lookup the hash (!opTypeSafe)
         */
        /* Add key */
        addKey(firstBlob);
      }
    }

    DBUG_PRINT("info", ("include = %u", include));
    DBUG_RETURN(include);
  }
};

/*****************************************************************************
NdbTransaction( Ndb* aNdb );

Return Value:  None
Parameters:    aNdb: Pointers to the Ndb object 
Remark:        Creates a connection object. 
*****************************************************************************/
NdbTransaction::NdbTransaction( Ndb* aNdb ) :
  theSendStatus(NotInit),
  theCallbackFunction(nullptr),
  theCallbackObject(nullptr),
  theTransArrayIndex(0),
  theStartTransTime(0),
  theErrorLine(0),
  theErrorOperation(nullptr),
  theNdb(aNdb),
  theNext(nullptr),
  theFirstOpInList(nullptr),
  theLastOpInList(nullptr),
  theFirstExecOpInList(nullptr),
  theLastExecOpInList(nullptr),
  theCompletedFirstOp(nullptr),
  theCompletedLastOp(nullptr),
  theNoOfOpSent(0),
  theNoOfOpCompleted(0),
  theMyRef(0),
  theTCConPtr(0),
  theTransactionId(0),
  theGlobalCheckpointId(0),
  p_latest_trans_gci(nullptr),
  theStatus(NotConnected),
  theCompletionStatus(NotCompleted), 
  theCommitStatus(NotStarted),
  theMagicNumber(0xFE11DC),
  theTransactionIsStarted(false),
  theDBnode(0),
  theReleaseOnClose(false),
  // Scan operations
  m_waitForReply(true),
  m_theFirstScanOperation(nullptr),
  m_theLastScanOperation(nullptr),
  m_firstExecutedScanOp(nullptr),
  // Scan operations
  theScanningOp(nullptr),
  theBuddyConPtr(0xFFFFFFFF),
  theBlobFlag(false),
  m_userDefinedBlobOps(false),
  thePendingBlobOps(0),
  maxPendingBlobReadBytes(~Uint32(0)),
  maxPendingBlobWriteBytes(~Uint32(0)),
  pendingBlobReadBytes(0),
  pendingBlobWriteBytes(0),
  // Lock handle
  m_theFirstLockHandle(nullptr),
  m_theLastLockHandle(nullptr),
  // Composite query operations
  m_firstQuery(nullptr),
  m_firstExecQuery(nullptr),
  m_firstActiveQuery(nullptr),
  m_scanningQuery(nullptr),
  //
  m_tcRef(numberToRef(DBTC, 0)),
  m_enable_schema_obj_owner_check(false)
{
  theListState = NotInList;
  theError.code = 0;
  //theId = NdbObjectIdMap::InvalidId;
  theId = theNdb->theImpl->mapRecipient(this);

#define CHECK_SZ(mask, sz) assert((sizeof(mask)/sizeof(mask[0])) == sz)

  CHECK_SZ(m_db_nodes, NdbNodeBitmask::Size);
  CHECK_SZ(m_failed_db_nodes, NdbNodeBitmask::Size);
}//NdbTransaction::NdbTransaction()

/*****************************************************************************
~NdbTransaction();

Remark:        Deletes the connection object. 
*****************************************************************************/
NdbTransaction::~NdbTransaction()
{
  DBUG_ENTER("NdbTransaction::~NdbTransaction");
  theNdb->theImpl->unmapRecipient(theId, this);
  DBUG_VOID_RETURN;
}//NdbTransaction::~NdbTransaction()

/*****************************************************************************
void init();

Remark:         Initialise connection object for new transaction. 
*****************************************************************************/
int
NdbTransaction::init()
{
  theListState            = NotInList;
  theInUseState           = true;
  theTransactionIsStarted = false;
  theNext		  = nullptr;

  theFirstOpInList	  = nullptr;
  theLastOpInList	  = nullptr;

  theScanningOp           = nullptr;
  m_scanningQuery         = nullptr;

  theFirstExecOpInList	  = nullptr;
  theLastExecOpInList	  = nullptr;

  theCompletedFirstOp	  = nullptr;
  theCompletedLastOp	  = nullptr;

  theGlobalCheckpointId   = 0;
  p_latest_trans_gci      =
    theNdb->theImpl->m_ndb_cluster_connection.get_latest_trans_gci();
  theCommitStatus         = Started;
  theCompletionStatus     = NotCompleted;

  theError.code		  = 0;
  theErrorLine		  = 0;
  theErrorOperation	  = nullptr;

  theReleaseOnClose       = false;
  theSimpleState          = true;
  theSendStatus           = InitState;
  theMagicNumber          = getMagicNumber();

  // Query operations
  m_firstQuery            = nullptr;
  m_firstExecQuery        = nullptr;
  m_firstActiveQuery      = nullptr;

  // Scan operations
  m_waitForReply          = true;
  m_theFirstScanOperation = nullptr;
  m_theLastScanOperation  = nullptr;
  m_firstExecutedScanOp   = nullptr;
  theBuddyConPtr          = 0xFFFFFFFF;
  //
  theBlobFlag = false;
  m_userDefinedBlobOps = false;
  thePendingBlobOps = 0;
  m_theFirstLockHandle    = nullptr;
  m_theLastLockHandle     = nullptr;
  pendingBlobReadBytes = 0;
  pendingBlobWriteBytes = 0;
  if (theId == NdbObjectIdMap::InvalidId)
  {
    theId = theNdb->theImpl->mapRecipient(this);
    if (theId == NdbObjectIdMap::InvalidId)
    {
      theError.code = 4000;
      return -1;
    }
  }
  return 0;

}//NdbTransaction::init()

/*****************************************************************************
setOperationErrorCode(int error);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void
NdbTransaction::setOperationErrorCode(int error)
{
  DBUG_ENTER("NdbTransaction::setOperationErrorCode");
  setErrorCode(error);
  DBUG_VOID_RETURN;
}

/*****************************************************************************
setOperationErrorCodeAbort(int error);

Remark:        Sets an error code on the connection object from an 
               operation object. 
*****************************************************************************/
void NdbTransaction::setOperationErrorCodeAbort(int error)
{
  DBUG_ENTER("NdbTransaction::setOperationErrorCodeAbort");
  if (theTransactionIsStarted == false) {
    theCommitStatus = Aborted;
  } else if ((theCommitStatus != Committed) &&
             (theCommitStatus != Aborted)) {
    theCommitStatus = NeedAbort;
  }//if
  setErrorCode(error);
  DBUG_VOID_RETURN;
}

/*****************************************************************************
setErrorCode(int anErrorCode);

Remark:        Sets an error indication on the connection object. 
*****************************************************************************/
void
NdbTransaction::setErrorCode(int error)
{
  DBUG_ENTER("NdbTransaction::setErrorCode");
  DBUG_PRINT("enter", ("error: %d, theError.code: %d", error, theError.code));

  if (theError.code == 0)
    theError.code = error;

  DBUG_VOID_RETURN;
}//NdbTransaction::setErrorCode()

int
NdbTransaction::restart(){
  DBUG_ENTER("NdbTransaction::restart");
  if(theCompletionStatus == CompletedSuccess){
    releaseCompletedOperations();
    releaseCompletedQueries();

    theTransactionId = theNdb->allocate_transaction_id();

    theCommitStatus = Started;
    theCompletionStatus = NotCompleted;
    theTransactionIsStarted = false;
    DBUG_RETURN(0);
  }
  DBUG_PRINT("error",("theCompletionStatus != CompletedSuccess"));
  DBUG_RETURN(-1);
}

/*****************************************************************************
void handleExecuteCompletion(void);

Remark:        Handle time-out on a transaction object. 
*****************************************************************************/
void
NdbTransaction::handleExecuteCompletion()
{
  /***************************************************************************
   *	  Move the NdbOperation objects from the list of executing 
   *      operations to list of completed
   **************************************************************************/
  NdbOperation* tFirstExecOp = theFirstExecOpInList;
  NdbOperation* tLastExecOp = theLastExecOpInList;
  if (tLastExecOp != nullptr) {
    tLastExecOp->next(theCompletedFirstOp);
    theCompletedFirstOp = tFirstExecOp;
    if (theCompletedLastOp == nullptr)
      theCompletedLastOp = tLastExecOp;
    theFirstExecOpInList = nullptr;
    theLastExecOpInList = nullptr;
  }//if

  theSendStatus = InitState;
  return;
}//NdbTransaction::handleExecuteCompletion()

/**
 * OpList
 *
 * Class using RAII to more safely / clearly do
 * operation list manipulations
 * Potential improvement :  implement as template parameterised by
 * NdbTransaction* list first/last vars
 */
class OpList
{
  NdbOperation*& m_listFirst;
  NdbOperation*& m_listLast;

  NdbOperation* m_savedFirst;
  NdbOperation* m_savedLast;

  // Restore saved list items after or
  // before main list items
  bool m_savedAfter;

public:
#ifdef VM_TRACE
  void checkOpInList(const NdbOperation* op)
  {
    const NdbOperation* lop = m_listFirst;
    do
    {
      if (op == lop)
      {
        return;
      }
    } while (lop != nullptr &&
             lop != m_listLast &&
             (lop = lop->next()) != nullptr);
    abort();
  }
#endif
  /**
   * OpList
   *
   * Create OpList with pointers to list source
   */
  OpList(NdbOperation*& listFirst,
         NdbOperation*& listLast):
    m_listFirst(listFirst),
    m_listLast(listLast),
    m_savedFirst(nullptr),
    m_savedLast(nullptr),
    m_savedAfter(true)
  {
#ifdef VM_TRACE
    checkOpInList(m_listLast);
#endif
  }

  /**
   * ~OpList
   *
   * On going out of scope return the saved operations to the
   * list that they came from.
   */
  ~OpList()
  {
    /* Restore list */
    if (m_savedFirst)
    {
      assert(m_savedLast);

      if (m_listFirst != nullptr)
      {
        assert(m_listLast != nullptr);
        if (m_savedAfter)
        {
          /* Add saved to end of list */
          m_listLast->next(m_savedFirst);
          m_listLast = m_savedLast;
        }
        else
        {
          /* Add saved to start of list */
          m_savedLast->next(m_listFirst);
          m_listFirst = m_savedFirst;
        }
      }
      else
      {
        /* Replace Whole list */
        assert(m_listLast == nullptr);
        m_listFirst = m_savedFirst;
        m_listLast = m_savedLast;
      }
    }
  }

  /**
   * Anything saved attm
   */
  bool isEmpty() const
  {
    return (m_savedFirst == nullptr);
  }


  /**
   * Save everything after passed op
   * Will be restored to end of list
   * on going out of scope
   */
  void saveAfter(NdbOperation* op)
  {
    assert(m_savedFirst == nullptr);
    assert(m_savedLast == nullptr);
#ifdef VM_TRACE
    checkOpInList(op);
#endif
    m_savedFirst = op->next();
    m_savedLast = m_listLast;

    op->next(nullptr);
    m_listLast = op;
    m_savedAfter = true;
  }

  /**
   * Save everything up to and including
   * passed op.
   * Will be restored to start of list
   * on going out of scope
   */
  void saveBeforeAndIncluding(NdbOperation* op)
  {
    assert(m_savedFirst == nullptr);
    assert(m_savedLast == nullptr);
#ifdef VM_TRACE
    checkOpInList(op);
#endif
    m_savedFirst = m_listFirst;
    m_savedLast = op;

    m_listFirst = op->next();
    op->next(nullptr);
    m_savedAfter = false;
  }

  /**
   * Save everything in the list
   * Will be restored to end of list
   * on going out of scope
   */
  void save()
  {
    if (m_savedFirst != nullptr)
    {
      assert(m_savedLast != nullptr);
      m_savedLast->next(m_listFirst);
      m_savedLast = m_listLast;
    }
    else
    {
      assert(m_savedLast == nullptr);
      m_savedFirst = m_listFirst;
      m_savedLast = m_listLast;
    }
    m_listFirst = m_listLast = nullptr;
    m_savedAfter = true;
  }

  /**
   * allOpsHaveFlagsSet
   *
   * Checks that all operations in the supplied list
   * have all of the given flags set
   */
  static bool allOpsHaveFlagsSet(const NdbOperation* opFirst,
                                 const NdbOperation* opLast,
                                 Uint8 flags)
  {
    if (opFirst)
    {
      assert(opLast);

      const NdbOperation* op = opFirst;

      do
      {
        if ((op->m_flags & flags) != flags)
        {
          break;
        }

        if (op == opLast)
        {
          return true;
        }

        op = op->next();
      } while (true);
    }

    return false;
  }
};

/*****************************************************************************
int execute(ExecType aTypeOfExec, CommitType aTypeOfCommit, int forceSend);

Return Value:  Return 0 : execute was successful.
               Return -1: In all other case.  
Parameters :   aTypeOfExec: Type of execute.
Remark:        Initialise connection object for new transaction. 
*****************************************************************************/
int 
NdbTransaction::execute(ExecType aTypeOfExec, 
			NdbOperation::AbortOption abortOption,
			int forceSend)
{
  NdbError existingTransError = theError;
  NdbError firstTransError;
  DBUG_ENTER("NdbTransaction::execute");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, abortOption: %d", 
		       aTypeOfExec, abortOption));

  if (! theBlobFlag)
    DBUG_RETURN(executeNoBlobs(aTypeOfExec, abortOption, forceSend));

  /* Blob specific exec path taken */
  m_userDefinedBlobOps = false;

  /*
   * execute prepared ops in batches, as requested by blobs
   * - blob error does not terminate execution
   * - blob error sets error on operation
   * - if error on operation skip blob calls
   *
   * In the call to preExecute(), each operation involving blobs can 
   * add (and execute) extra operations before (reads) and after 
   * (writes) the operation on the main row.
   * In the call to postExecute(), each blob can add extra read and
   * write operations to be executed immediately
   * It is assumed that all operations added in preExecute() are
   * defined 'before' operations added in postExecute().
   * To facilitate this, the transaction's list of operations is 
   * pre-emptively split when a Blob operation is encountered.
   * preExecute can add operations before and after the operation being
   * processed, and if no batch execute is required, the list is rejoined.
   * If batch execute is required, then execute() is performed, and then
   * the postExecute() actions (which can add operations) are called before
   * the list is rejoined.  See NdbBlob::preExecute() and 
   * NdbBlob::postExecute() for more info.
   */

  NdbOperation* tPrepOp;

  if (abortOption != NdbOperation::DefaultAbortOption)
  {
    DBUG_PRINT("info", ("Forcing operations to take execute() abortOption %d",
                        abortOption));
    /* For Blobs, we have to execute with DefaultAbortOption
     * If the user supplied a non default AbortOption to execute()
     * then we need to make sure that all of the operations in their
     * batch are set to use the supplied AbortOption so that the 
     * expected behaviour is obtained when executing below
     */
    tPrepOp= theFirstOpInList;
    while(tPrepOp != nullptr)
    {
      DBUG_PRINT("info", ("Changing abortOption from %d", 
                          tPrepOp->m_abortOption));
      tPrepOp->m_abortOption= abortOption;
      tPrepOp= tPrepOp->next();
    }
  }


  ExecType tExecType;
  OpList completedOps(theCompletedFirstOp,
                      theCompletedLastOp);

  BlobBatchChecker batchChecker;

  int ret = 0;
  do {
    OpList nextBatchOps(theFirstOpInList,
                        theLastOpInList);
    NdbOperation* prevOp = nullptr;

    tExecType = aTypeOfExec;
    tPrepOp = theFirstOpInList;
    while (tPrepOp != nullptr) {
      if (tPrepOp->theError.code == 0) {
        NdbBlob* tBlob = tPrepOp->theBlobList;
        if (tBlob !=nullptr) {
          if (!batchChecker.includeNextOpInBatch(tPrepOp))
          {
            DBUG_PRINT("info", ("Next op cannot be included in batch - "
                                "split list after %p and before %p",
                                prevOp,
                                tPrepOp));
            assert(prevOp != nullptr);
            /* Execute what we have prepared so far */
            /* Save any remainder for later */
            assert(nextBatchOps.isEmpty());
            nextBatchOps.saveAfter(prevOp);

            tExecType = NoCommit;
            /* Break out of loop to execute now... */
            break;
          }

          /* Prepare this operation + blob ops now */
          {
            /* Remove already defined ops from consideration for now
             * for more efficient operation reordering in
             * Blob preExecute
             */
            OpList precedingOps(theFirstOpInList,
                                theLastOpInList);
            if (prevOp)
            {
              precedingOps.saveBeforeAndIncluding(prevOp);
            }
            /**
             * Hide following user defined ops for now so that
             * internal blob operations are logically placed
             * before them
             */
            OpList followingOps(theFirstOpInList,
                                theLastOpInList);
            followingOps.saveAfter(tPrepOp);

            while (tBlob != nullptr) {
              NdbBlob::BlobAction ba = tBlob->preExecute(tExecType);

              if (ba == NdbBlob::BA_ERROR)
              {
                ret = -1;
                if (firstTransError.code==0)
                  firstTransError= theError;
              }
              else if (ba == NdbBlob::BA_EXEC)
              {
                DBUG_PRINT("info",
                           ("Op needs execution, setting execType from %u to %u",
                            tExecType,
                            NoCommit));
                tExecType = NoCommit;
              }
              else
              {
                assert(ba == NdbBlob::BA_DONE);
              }
              tBlob = tBlob->theNext;
            } // while tBlob
          } // ops lists
        }
      }

      prevOp = tPrepOp;
      tPrepOp = tPrepOp->next();
    }

    /* Will execute now with what we have so far */
    batchChecker.reset();

    if (tExecType == Commit) {
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != nullptr) {
        if (tOp->theError.code == 0) {
          NdbBlob* tBlob = tOp->theBlobList;
          while (tBlob != nullptr) {
            if (tBlob->preCommit() == -1)
	    {
	      ret = -1;
	      if (firstTransError.code==0)
		firstTransError= theError;
	    }
            tBlob = tBlob->theNext;
          }
        }
        tOp = tOp->next();
      }
    }

    // Save completed ops so far...
    completedOps.save();

    {
      const NdbOperation* execOpFirst = theFirstOpInList;
      const NdbOperation* execOpLast = theLastOpInList;

      if (executeNoBlobs(tExecType,
                         NdbOperation::DefaultAbortOption,
                         forceSend) == -1)
      {
        if (theError.code == 626)
        {
          DBUG_PRINT("info", ("Got error 626 in main exec, "
                              "checking for part reads "
                              "indicating Blob corruption."));

          if (OpList::allOpsHaveFlagsSet(execOpFirst,
                                         execOpLast,
                                         NdbOperation::OF_BLOB_PART_READ))
          {
            DBUG_PRINT("info", ("Blob corruption"));
            theError.code = NdbBlobImpl::ErrCorrupt;
          }
        }

        /* Capture any trans error left by the execute() in case it gets trampled */
        if (firstTransError.code != 0)
          theError = firstTransError;
        DBUG_RETURN(-1);
      }
    }

    /* Capture any trans error left by the execute() in case it gets trampled */
    if (firstTransError.code==0)
      firstTransError= theError;

#ifdef ndb_api_crash_on_complex_blob_abort
    assert(theFirstOpInList == NULL && theLastOpInList == NULL);
#else
    theFirstOpInList = theLastOpInList = nullptr;
#endif

    bool postexec_again;
    do
    {
      postexec_again = false;
      NdbOperation* tOp = theCompletedFirstOp;
      while (tOp != nullptr) {
        if (tOp->theError.code == 0) {
          NdbBlob* tBlob = tOp->theBlobList;
          while (tBlob != nullptr) {
            // may add new operations if batch
            NdbBlob::BlobAction ba = tBlob->postExecute(tExecType);
            if (ba == NdbBlob::BA_ERROR)
	    {
              ret = -1;
	      if (firstTransError.code==0)
		firstTransError= theError;
	    }
            else if (ba == NdbBlob::BA_EXEC)
            {
              DBUG_PRINT("info", ("postExec requested further exec"));
              postexec_again = true;
              /**
               * Potential improvements :
               * - Resume postExec from first op requesting it?
               * - Early exit if batch is full
               */
            }
            else
            {
              assert(ba == NdbBlob::BA_DONE);
            }
            tBlob = tBlob->theNext;
          }
        }
        tOp = tOp->next();
      }

      const bool have_dependent_ops = !nextBatchOps.isEmpty();
      const bool have_pending_ops = (theFirstOpInList != nullptr);
      const bool exec_now =
        postexec_again ||                           // More to do
        (have_dependent_ops && have_pending_ops);   // Flush

      if (exec_now)
      {
        const NdbOperation* execOpFirst = theFirstOpInList;
        const NdbOperation* execOpLast = theLastOpInList;

        if (executeNoBlobs(NdbTransaction::NoCommit,
                           NdbOperation::DefaultAbortOption,
                           forceSend) == -1)
        {
          /* executeNoBlobs will have set transaction error */
          if (theError.code == 626)
          {
            DBUG_PRINT("info", ("Got error 626 in postExec, "
                                "checking for part reads "
                                "indicating Blob corruption."));
            if (OpList::allOpsHaveFlagsSet(execOpFirst,
                                           execOpLast,
                                           NdbOperation::OF_BLOB_PART_READ))
            {
              DBUG_PRINT("info", ("Blob corruption"));
              theError.code = NdbBlobImpl::ErrCorrupt;
            }
          }
          DBUG_RETURN(-1);
        }
      }
    } while (postexec_again);
    assert((theFirstOpInList == nullptr && nextBatchOps.isEmpty()) ||
           tExecType == NoCommit);
  } while (theFirstOpInList != nullptr || tExecType != aTypeOfExec);

  /* Sometimes the original error is trampled by 'Trans already aborted',
   * detect this case and attempt to restore the original error
   */
  if (theError.code == 4350) // Trans already aborted
  {
    DBUG_PRINT("info", ("Trans already aborted, existingTransError.code %u, "
                        "firstTransError.code %u",
                        existingTransError.code,
                        firstTransError.code));
    if (existingTransError.code != 0)
    {
      theError = existingTransError;
    }
    else if (firstTransError.code != 0)
    {
      theError = firstTransError;
    }
  }

  /* Generally return the first error which we encountered as
   * the Trans error.  Caller can traverse the op list to
   * get the full picture
   */
  if (firstTransError.code != 0)
  {
    DBUG_PRINT("info", ("Setting error to first error.  firstTransError.code = %u, "
                        "theError.code = %u",
                        firstTransError.code,
                        theError.code));
    theError = firstTransError;
  }

  DBUG_RETURN(ret);
}

int 
NdbTransaction::executeNoBlobs(NdbTransaction::ExecType aTypeOfExec, 
			       NdbOperation::AbortOption abortOption,
			       int forceSend)
{
  DBUG_ENTER("NdbTransaction::executeNoBlobs");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, abortOption: %d", 
		       aTypeOfExec, abortOption));

  if (unlikely(m_userDefinedBlobOps))
  {
    // "Pending Blob Operations must be executed before this call"
    theError.code = 4558;
    DBUG_RETURN(-1);
  }

//------------------------------------------------------------------------
// We will start by preparing all operations in the transaction defined
// since last execute or since beginning. If this works ok we will continue
// by calling the poll with wait method. This method will return when
// the NDB kernel has completed its task or when 10 seconds have passed.
// The NdbTransactionCallBack-method will receive the return code of the
// transaction. The normal methods of reading error codes still apply.
//------------------------------------------------------------------------
  Ndb* tNdb = theNdb;

  Uint32 timeout = theNdb->theImpl->get_waitfor_timeout();
  m_waitForReply = false;
  executeAsynchPrepare(aTypeOfExec, nullptr, nullptr, abortOption);
  if (m_waitForReply){
    while (1) {
      int noOfComp = tNdb->sendPollNdb(3 * timeout, 1, forceSend);
      if (unlikely(noOfComp == 0)) {
        /*
         * Just for fun, this is only one of two places where
         * we could hit this error... It's quite possible we
         * hit it in Ndbif.cpp in Ndb::check_send_timeout()
         *
         * We behave rather similarly in both places.
         * Hitting this is certainly a bug though...
         */
        g_eventLogger->error("WARNING: Timeout in executeNoBlobs() waiting for "
                             "response from NDB data nodes. This should NEVER "
                             "occur. You have likely hit a NDB Bug. Please "
                             "file a bug.");
        DBUG_PRINT("error",("This timeout should never occure, execute()"));
        g_eventLogger->error("Forcibly trying to rollback txn (%p"
                             ") to try to clean up data node resources.",
                             this);
        executeNoBlobs(NdbTransaction::Rollback);
        theError.code = 4012;
        theError.status= NdbError::PermanentError;
        theError.classification= NdbError::TimeoutExpired;
        setOperationErrorCodeAbort(4012); // ndbd timeout
        DBUG_RETURN(-1);
      }//if

      /*
       * Check that the completed transactions include this one.  There
       * could be another thread running asynchronously.  Even in pure
       * async case rollback is done synchronously.
       */
      if (theListState != NotInList)
        continue;
#ifdef VM_TRACE
      unsigned anyway = 0;
      for (unsigned i = 0; i < theNdb->theNoOfPreparedTransactions; i++)
        anyway += theNdb->thePreparedTransactionsArray[i] == this;
      for (unsigned i = 0; i < theNdb->theNoOfSentTransactions; i++)
        anyway += theNdb->theSentTransactionsArray[i] == this;
      for (unsigned i = 0; i < theNdb->theNoOfCompletedTransactions; i++)
        anyway += theNdb->theCompletedTransactionsArray[i] == this;
      if (anyway) {
        theNdb->printState("execute %p", this);
        abort();
      }
#endif
      if (theReturnStatus == ReturnFailure) {
        DBUG_RETURN(-1);
      }//if
      break;
    }
  }
  thePendingBlobOps = 0;
  pendingBlobReadBytes = 0;
  pendingBlobWriteBytes = 0;
  DBUG_RETURN(0);
}//NdbTransaction::executeNoBlobs()

/** 
 * Get the first query in the current transaction that has a lookup operation
 * as its root.
 */
static NdbQueryImpl* getFirstLookupQuery(NdbQueryImpl* firstQuery)
{
  NdbQueryImpl* current = firstQuery;
  while (current != nullptr && current->getQueryDef().isScanQuery()) {
    current = current->getNext();
  }
  return current;
}

/** 
 * Get the last query in the current transaction that has a lookup operation
 * as its root.
 */
static NdbQueryImpl* getLastLookupQuery(NdbQueryImpl* firstQuery)
{
  NdbQueryImpl* current = firstQuery;
  NdbQueryImpl* last = nullptr;
  while (current != nullptr) {
    if (!current->getQueryDef().isScanQuery()) {
      last = current;
    }
    current = current->getNext();
  }
  return last;
}

/*****************************************************************************
void executeAsynchPrepare(ExecType           aTypeOfExec,
                          NdbAsynchCallback  callBack,
                          void*              anyObject,
                          CommitType         aTypeOfCommit);

Return Value:  No return value
Parameters :   aTypeOfExec:   Type of execute.
               anyObject:     An object provided in the callback method
               callBack:      The callback method
               aTypeOfCommit: What to do when read/updated/deleted records 
                              are missing or inserted records already exist.

Remark:        Prepare a part of a transaction in an asynchronous manner. 
*****************************************************************************/
void 
NdbTransaction::executeAsynchPrepare(NdbTransaction::ExecType aTypeOfExec,
                                     NdbAsynchCallback  aCallback,
                                     void*              anyObject,
                                     NdbOperation::AbortOption abortOption)
{
  DBUG_ENTER("NdbTransaction::executeAsynchPrepare");
  DBUG_PRINT("enter", ("aTypeOfExec: %d, aCallback: %p, anyObject: %p",
		       aTypeOfExec, aCallback, anyObject));

  /**
   * Reset error.code on execute
   */
#ifndef NDEBUG
  if (theError.code != 0)
    DBUG_PRINT("enter", ("Resetting error %d on execute", theError.code));
#endif
  {
    switch (aTypeOfExec)
    {
    case NdbTransaction::Commit:
      theNdb->theImpl->incClientStat(Ndb::TransCommitCount, 1);
      break;
    case NdbTransaction::Rollback:
      theNdb->theImpl->incClientStat(Ndb::TransAbortCount, 1);
      break;
    default:
      break;
    }
  }
  /**
   * for timeout (4012) we want sendROLLBACK to behave differently.
   * Else, normal behaviour of reset errcode
   */
  if (theError.code != 4012)
    theError.code = 0;

  /***************************************************************************
   * Eager garbage collect queries which has completed execution 
   * w/ all its results made available to client.
   * TODO: Add a member 'doEagerRelease' to check below.
   **************************************************************************/
  if (false) {
    releaseCompletedQueries();
  }

  bool tTransactionIsStarted = theTransactionIsStarted;
  NdbOperation*	tLastOp = theLastOpInList;
  Ndb* tNdb = theNdb;
  CommitStatusType tCommitStatus = theCommitStatus;
  Uint32 tnoOfPreparedTransactions = tNdb->theNoOfPreparedTransactions;

  theReturnStatus     = ReturnSuccess;
  theCallbackFunction = aCallback;
  theCallbackObject   = anyObject;
  m_waitForReply = true;
  tNdb->thePreparedTransactionsArray[tnoOfPreparedTransactions] = this;
  theTransArrayIndex = tnoOfPreparedTransactions;
  theListState = InPreparedList;
  tNdb->theNoOfPreparedTransactions = tnoOfPreparedTransactions + 1;

  theNoOfOpSent		= 0;
  theNoOfOpCompleted	= 0;
  NdbNodeBitmask::clear(m_db_nodes);
  NdbNodeBitmask::clear(m_failed_db_nodes);

  if ((tCommitStatus != Started) ||
      (aTypeOfExec == Rollback)) {
/*****************************************************************************
 *	Rollback have been ordered on a started transaction. Call rollback.
 *      Could also be state problem or previous problem which leads to the 
 *      same action.
 ****************************************************************************/
    if (aTypeOfExec == Rollback) {
      if (theTransactionIsStarted == false || theSimpleState) {
	theCommitStatus = Aborted;
	theSendStatus = sendCompleted;
      } else {
	theSendStatus = sendABORT;
      }
    } else {
      theSendStatus = sendABORTfail;
    }//if
    if (theCommitStatus == Aborted){
      DBUG_PRINT("exit", ("theCommitStatus: Aborted"));
      setErrorCode(4350);
    }
    DBUG_VOID_RETURN;
  }//if

  /**
   * Perform scan finalisation here
   */
  NdbScanOperation* tScanOp = m_theFirstScanOperation;
  while (tScanOp)
  {
    tScanOp->finaliseScan();
    tScanOp = (NdbScanOperation*)tScanOp->next();
  }

  NdbQueryImpl* const lastLookupQuery = getLastLookupQuery(m_firstQuery);

  if (tTransactionIsStarted == true) {
    if (tLastOp != nullptr) {
      if (aTypeOfExec == Commit) {
/*****************************************************************************
 *	Set commit indicator on last operation when commit has been ordered
 *      and also a number of operations.
******************************************************************************/
        tLastOp->theCommitIndicator = 1;
      }//if
    } else if (lastLookupQuery != nullptr) {
      if (aTypeOfExec == Commit) {
        lastLookupQuery->setCommitIndicator();
      }
    } else if (m_firstQuery == nullptr) {
      if (aTypeOfExec == Commit && !theSimpleState) {
	/**********************************************************************
	 *   A Transaction have been started and no more operations exist. 
	 *   We will use the commit method.
	 *********************************************************************/
        theSendStatus = sendCOMMITstate;
	DBUG_VOID_RETURN;
      } else {
	/**********************************************************************
	 * We need to put it into the array of completed transactions to 
	 * ensure that we report the completion in a proper way. 
	 * We cannot do this here since that would endanger the completed
	 * transaction array since that is also updated from the receiver 
	 * thread and thus we need to do it under mutex lock and thus we 
	 * set the sendStatus to ensure that the send method will
	 * put it into the completed array.
	 **********************************************************************/
        theSendStatus = sendCompleted;
	DBUG_VOID_RETURN; // No Commit with no operations is OK
      }//if
    }//if
  } else if (tTransactionIsStarted == false) {
    NdbOperation* tFirstOp = theFirstOpInList;

    /* 
     * Lookups that are roots of queries are sent before non-linked lookups.
     * If both types are present, then the start indicator should be set
     * on a query root lookup, and the commit indicator on a non-linked 
     * lookup.
     */
    if (lastLookupQuery != nullptr) {
      getFirstLookupQuery(m_firstQuery)->setStartIndicator();
    } else if (tFirstOp != nullptr) {
      tFirstOp->setStartIndicator();
    }

    if (tFirstOp != nullptr) {
      if (aTypeOfExec == Commit) {
        tLastOp->theCommitIndicator = 1;
      }//if
    } else if (lastLookupQuery != nullptr) {
      if (aTypeOfExec == Commit) {
        lastLookupQuery->setCommitIndicator();
      }//if
    } else if (m_firstQuery == nullptr) {
      /***********************************************************************
       *    No operations are defined and we have not started yet. 
       *    Simply return OK. Set commit status if Commit.
       ***********************************************************************/
      if (aTypeOfExec == Commit) {
        theCommitStatus = Committed;
      }//if
      /***********************************************************************
       * We need to put it into the array of completed transactions to
       * ensure that we report the completion in a proper way. We
       * cannot do this here since that would endanger the completed
       * transaction array since that is also updated from the
       * receiver thread and thus we need to do it under mutex lock
       * and thus we set the sendStatus to ensure that the send method
       * will put it into the completed array.
       ***********************************************************************/
      theSendStatus = sendCompleted;
      DBUG_VOID_RETURN;
    }//if
  }

  theCompletionStatus = NotCompleted;

  // Prepare sending of all pending NdbQuery's
  if (m_firstQuery) {
    NdbQueryImpl* query = m_firstQuery;
    NdbQueryImpl* last = nullptr;
    while (query!=nullptr) {
      const int tReturnCode = query->prepareSend();
      if (unlikely(tReturnCode != 0)) {
        theSendStatus = sendABORTfail;
        DBUG_VOID_RETURN;
      }//if
      last  = query;
      query = query->getNext();
    }
    assert (m_firstExecQuery==nullptr);
    last->setNext(m_firstExecQuery);
    m_firstExecQuery = m_firstQuery;
    m_firstQuery = nullptr;
  }

  // Prepare sending of all pending (non-scan) NdbOperations's
  NdbOperation* tOp = theFirstOpInList;
  Uint32 pkOpCount = 0;
  Uint32 ukOpCount = 0;
  while (tOp) {
    int tReturnCode;
    NdbOperation* tNextOp = tOp->next();

    /* Count operation */
    if (tOp->theTCREQ->theVerId_signalNumber == GSN_TCINDXREQ)
      ukOpCount++;
    else
      pkOpCount++;

    if (tOp->Status() == NdbOperation::UseNdbRecord)
      tReturnCode = tOp->prepareSendNdbRecord(abortOption);
    else
      tReturnCode= tOp->prepareSend(theTCConPtr, theTransactionId, abortOption);

    if (tReturnCode == -1) {
      theSendStatus = sendABORTfail;
      DBUG_VOID_RETURN;
    }//if

    /*************************************************************************
     * Now that we have successfully prepared the send of this operation we 
     * move it to the list of executing operations and remove it from the 
     * list of defined operations.
     ************************************************************************/
    tOp = tNextOp;
  } 

  theNdb->theImpl->incClientStat(Ndb::PkOpCount, pkOpCount);
  theNdb->theImpl->incClientStat(Ndb::UkOpCount, ukOpCount);

  NdbOperation* tLastOpInList = theLastOpInList;
  NdbOperation* tFirstOpInList = theFirstOpInList;

  theFirstOpInList = nullptr;
  theLastOpInList = nullptr;
  theFirstExecOpInList = tFirstOpInList;
  theLastExecOpInList = tLastOpInList;

  theCompletionStatus = CompletedSuccess;
  theSendStatus = sendOperations;
  DBUG_VOID_RETURN;
}//NdbTransaction::executeAsynchPrepare()

void
NdbTransaction::executeAsynch(ExecType aTypeOfExec,
                              NdbAsynchCallback aCallback,
                              void* anyObject,
                              NdbOperation::AbortOption abortOption,
                              int forceSend)
{
  executeAsynchPrepare(aTypeOfExec, aCallback, anyObject, abortOption);
  theNdb->sendPreparedTransactions(forceSend);
}

void NdbTransaction::close()
{
  theNdb->closeTransaction(this);
}

int NdbTransaction::refresh()
{
  for(NdbIndexScanOperation* scan_op = m_firstExecutedScanOp;
      scan_op != nullptr; scan_op = (NdbIndexScanOperation *) scan_op->theNext)
  {
    NdbTransaction* scan_trans = scan_op->theNdbCon;
    if (scan_trans)
    {
      scan_trans->sendTC_HBREP();
    }
  }
  return sendTC_HBREP();
}

/*****************************************************************************
int sendTC_HBREP();

Return Value:  No return value.  
Parameters :   None.
Remark:        Order NDB to refresh the timeout counter of the transaction. 
******************************************************************************/
int 	
NdbTransaction::sendTC_HBREP()		// Send a TC_HBREP signal;
{
  NdbApiSignal* tSignal;
  Ndb* tNdb = theNdb;
  Uint32 tTransId1, tTransId2;

  tSignal = tNdb->getSignal();
  if (tSignal == nullptr) {
    return -1;
  }

  if (tSignal->setSignal(GSN_TC_HBREP, refToBlock(m_tcRef)) == -1) {
    tNdb->releaseSignal(tSignal);
    return -1;
  }

  TcHbRep * const tcHbRep = CAST_PTR(TcHbRep, tSignal->getDataPtrSend());
  
  tcHbRep->apiConnectPtr = theTCConPtr;    
  
  tTransId1 = (Uint32) theTransactionId;
  tTransId2 = (Uint32) (theTransactionId >> 32);
  tcHbRep->transId1      = tTransId1;
  tcHbRep->transId2      = tTransId2;
 
  tNdb->theImpl->lock();
  const int res = tNdb->theImpl->sendSignal(tSignal,theDBnode);
  tNdb->theImpl->flush_send_buffers();
  tNdb->theImpl->unlock();
  tNdb->releaseSignal(tSignal);

  if (res == -1){
    return -1;
  }    
  
  return 0;
}//NdbTransaction::sendTC_HBREP()

/*****************************************************************************
int doSend();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Remark:        Send all operations and queries belonging to this connection.
               The caller of this method has the responsibility to remove the
               object from the prepared transactions array on the Ndb-object.
*****************************************************************************/
int
NdbTransaction::doSend()
{
  DBUG_ENTER("NdbTransaction::doSend");
  /*
  This method assumes that at least one operation or query have been defined.
  This is ensured by the caller of this routine (=execute).
  */
  NdbScanOperation* tcOp = m_theFirstScanOperation;
  if (tcOp != nullptr){
    // Execute any cursor operations
    while (tcOp != nullptr) {
      int tReturnCode;
      tReturnCode = tcOp->executeCursor(theDBnode);
      /**
        Previously this code executed in executeAsynchPrepare and we
        had no way of reporting an error. Thus this particular failure
        won't immediately cause a transaction failure in the old code.
        To retain this old behaviour we don't cause a transaction
        failure here either. We avoid postExecuteRelease in the
        failure case just in case we want to analyse the error a
        bit more.
      */
      if (tReturnCode != -1) {
        tcOp->postExecuteRelease(); // Release unneeded resources
                                    // outside TP mutex
      }//if
      tcOp = (NdbScanOperation*)tcOp->next();
    } // while
    m_theLastScanOperation->next(m_firstExecutedScanOp);
    m_firstExecutedScanOp = m_theFirstScanOperation;
    // Discard cursor operations, since these are also
    // in the complete operations list we do not need
    // to release them.
    m_theFirstScanOperation = m_theLastScanOperation = nullptr;
  }

  switch(theSendStatus){
  case sendOperations: {
    assert (m_firstExecQuery!=nullptr || theFirstExecOpInList!=nullptr);

    const NdbQueryImpl* const lastLookupQuery 
      = getLastLookupQuery(m_firstExecQuery);
    if (m_firstExecQuery!=nullptr) {
      NdbQueryImpl* query = m_firstExecQuery;
      NdbQueryImpl* last  = nullptr;
      while (query!=nullptr) {
        const bool lastFlag = 
          query == lastLookupQuery && theFirstExecOpInList == nullptr;
        const int tReturnCode = query->doSend(theDBnode, lastFlag);
        if (tReturnCode == -1) {
          goto fail;
        }
        last = query;
        query = query->getNext();
      } // while

      // Append to list of active queries
      last->setNext(m_firstActiveQuery);
      m_firstActiveQuery = m_firstExecQuery;
      m_firstExecQuery = nullptr;
    }

    NdbOperation * tOp = theFirstExecOpInList;
    while (tOp != nullptr) {
      NdbOperation* tNext = tOp->next();
      const Uint32 lastFlag = ((tNext == nullptr) ? 1 : 0);
      const int tReturnCode = tOp->doSend(theDBnode, lastFlag);
      if (tReturnCode == -1) {
        goto fail;
      }//if
      tOp = tNext;
    }

    if (theFirstExecOpInList || lastLookupQuery != nullptr) {
      theSendStatus = sendTC_OP;
      theTransactionIsStarted = true;
      theNdb->insert_sent_list(this);      // Lookup: completes with KEYCONF/REF
    } else {
      theSendStatus = sendCompleted;
      theNdb->insert_completed_list(this); // Scans query completes after send
    }
    DBUG_RETURN(0);
  }//case
  case sendABORT:
  case sendABORTfail:{
  /***********************************************************************
   * Rollback have been ordered on a not started transaction. 
   * Simply return OK and set abort status.
   ***********************************************************************/
    if (theSendStatus == sendABORTfail) {
      theReturnStatus = ReturnFailure;
    }//if
    if (sendROLLBACK() == 0) {
      DBUG_RETURN(0);
    }//if
    break;
  }//case
  case sendCOMMITstate:
    if (sendCOMMIT() == 0) {
      DBUG_RETURN(0);
    }//if
    break;
  case sendCompleted:
    theNdb->insert_completed_list(this); 
    DBUG_RETURN(0);
  default:
    g_eventLogger->info("Inconsistent theSendStatus = %d",
                        (Uint32)theSendStatus);
    abort();
    break;
  }//switch

  theReleaseOnClose = true;
  theTransactionIsStarted = false;
  theCommitStatus = Aborted;
fail:
  setOperationErrorCodeAbort(4002);
  DBUG_RETURN(-1);
}//NdbTransaction::doSend()

/**************************************************************************
int sendROLLBACK();

Return Value:  Return -1 if send unsuccessful.  
Parameters :   None.
Remark:        Order NDB to rollback the transaction. 
**************************************************************************/
int 	
NdbTransaction::sendROLLBACK()      // Send a TCROLLBACKREQ signal;
{
  Ndb* tNdb = theNdb;
  if ((theTransactionIsStarted == true) &&
      (theCommitStatus != Committed) &&
      (theCommitStatus != Aborted)) {
/**************************************************************************
 *	The user did not perform any rollback but simply closed the
 *      transaction. We must rollback Ndb since Ndb have been contacted.
 *************************************************************************/
    NdbApiSignal tSignal(tNdb->theMyRef);
    Uint32 tTransId1, tTransId2;
    NdbImpl * impl = theNdb->theImpl;
    int	  tReturnCode;

    tTransId1 = (Uint32) theTransactionId;
    tTransId2 = (Uint32) (theTransactionId >> 32);
    tSignal.setSignal(GSN_TCROLLBACKREQ, refToBlock(m_tcRef));
    tSignal.setData(theTCConPtr, 1);
    tSignal.setData(tTransId1, 2);
    tSignal.setData(tTransId2, 3);
    if(theError.code == 4012)
    {
      g_eventLogger->error("Sending TCROLLBACKREQ with Bad flag");
      tSignal.setLength(tSignal.getLength() + 1); // + flags
      tSignal.setData(0x1, 4); // potentially bad data
    }
    tReturnCode = impl->sendSignal(&tSignal,theDBnode);
    if (tReturnCode != -1) {
      theSendStatus = sendTC_ROLLBACK;
      tNdb->insert_sent_list(this);
      return 0;
    }//if
   /*********************************************************************
    * It was not possible to abort the transaction towards the NDB kernel
    * and thus we put it into the array of completed transactions that
    * are ready for reporting to the application.
    *********************************************************************/
    return -1;
  } else {
    /*
     It is not necessary to abort the transaction towards the NDB kernel and
     thus we put it into the array of completed transactions that are ready
     for reporting to the application.
     */
    theSendStatus = sendCompleted;
    tNdb->insert_completed_list(this);
    return 0;
    ;
  }//if
}//NdbTransaction::sendROLLBACK()

/***************************************************************************
int sendCOMMIT();

Return Value:  Return 0 : send was successful.
               Return -1: In all other case.  
Parameters :   None.
Remark:        Order NDB to commit the transaction. 
***************************************************************************/
int 	
NdbTransaction::sendCOMMIT()    // Send a TC_COMMITREQ signal;
{
  NdbApiSignal tSignal(theNdb->theMyRef);
  Uint32 tTransId1, tTransId2;
  NdbImpl * impl = theNdb->theImpl;
  int	  tReturnCode;

  tTransId1 = (Uint32) theTransactionId;
  tTransId2 = (Uint32) (theTransactionId >> 32);
  tSignal.setSignal(GSN_TC_COMMITREQ, refToBlock(m_tcRef));
  tSignal.setData(theTCConPtr, 1);
  tSignal.setData(tTransId1, 2);
  tSignal.setData(tTransId2, 3);
      
  tReturnCode = impl->sendSignal(&tSignal,theDBnode);
  if (tReturnCode != -1) {
    theSendStatus = sendTC_COMMIT;
    theNdb->insert_sent_list(this);
    return 0;
  } else {
    return -1;
  }//if
}//NdbTransaction::sendCOMMIT()

/******************************************************************************
void release();

Remark:         Release all operations.
******************************************************************************/
void 
NdbTransaction::release(){
  releaseOperations();
  releaseLockHandles();
  if ( (theTransactionIsStarted == true) &&
       ((theCommitStatus != Committed) &&
	(theCommitStatus != Aborted))) {
    /************************************************************************
     *	The user did not perform any rollback but simply closed the
     *      transaction. We must rollback Ndb since Ndb have been contacted.
     ************************************************************************/
    if (!theSimpleState)
    {
      execute(Rollback);
    }
  }//if
  theMagicNumber = 0xFE11DC;
  theInUseState = false;
#ifdef VM_TRACE
  if (theListState != NotInList && theListState != InPreparedList) {
    theNdb->printState("release %p", this);
    abort();
  }
#endif
}//NdbTransaction::release()

void
NdbTransaction::releaseOps(NdbOperation* tOp){
  while (tOp != nullptr) {
    NdbOperation* tmp = tOp;
    tOp->release();
    tOp = tOp->next();
    theNdb->releaseOperation(tmp);
  }//while
}

/******************************************************************************
void releaseOperations();

Remark:         Release all operations.
******************************************************************************/
void 
NdbTransaction::releaseOperations()
{
  // Release any open scans
  releaseScanOperations(m_theFirstScanOperation);
  releaseScanOperations(m_firstExecutedScanOp);
  
  releaseQueries(m_firstQuery);
  releaseQueries(m_firstExecQuery);
  releaseQueries(m_firstActiveQuery);
  releaseOps(theCompletedFirstOp);
  releaseOps(theFirstOpInList);
  releaseOps(theFirstExecOpInList);

  theCompletedFirstOp = nullptr;
  theCompletedLastOp = nullptr;
  theFirstOpInList = nullptr;
  theFirstExecOpInList = nullptr;
  theLastOpInList = nullptr;
  theLastExecOpInList = nullptr;
  m_theFirstScanOperation = nullptr;
  m_theLastScanOperation = nullptr;
  m_firstExecutedScanOp = nullptr;
  m_firstQuery = nullptr;
  m_firstExecQuery = nullptr;
  m_firstActiveQuery = nullptr;

}//NdbTransaction::releaseOperations()

void 
NdbTransaction::releaseCompletedOperations()
{
  releaseOps(theCompletedFirstOp);
  theCompletedFirstOp = nullptr;
  theCompletedLastOp = nullptr;
  theErrorLine = 0;
  theErrorOperation = nullptr;
}//NdbTransaction::releaseCompletedOperations()


void 
NdbTransaction::releaseCompletedQueries()
{
  /**
   * Find & release all active queries which as completed.
   */
  NdbQueryImpl* prev  = nullptr;
  NdbQueryImpl* query = m_firstActiveQuery;
  while (query != nullptr) {
    NdbQueryImpl* next = query->getNext();

    if (query->hasCompleted()) {
      // Unlink from completed-query list
      if (prev)
        prev->setNext(next);
      else
        m_firstActiveQuery = next;

      query->release();
    } else {
      prev = query;
    }
    query = next;
  } // while
}//NdbTransaction::releaseCompletedQueries()


/******************************************************************************
void releaseQueries();

Remark:         Release all queries 
******************************************************************************/
void
NdbTransaction::releaseQueries(NdbQueryImpl* query)
{
  while (query != nullptr) {
    NdbQueryImpl* next = query->getNext();
    query->release();
    query = next;
  }
}//NdbTransaction::releaseQueries

/******************************************************************************
void releaseScanOperations();

Remark:         Release all cursor operations. 
                (NdbScanOperation and NdbIndexOperation)
******************************************************************************/
void 
NdbTransaction::releaseScanOperations(NdbIndexScanOperation* cursorOp)
{
  while(cursorOp != nullptr){
    NdbIndexScanOperation* next = (NdbIndexScanOperation*)cursorOp->next();
    cursorOp->release();
    theNdb->releaseScanOperation(cursorOp);
    cursorOp = next;
  }
}//NdbTransaction::releaseScanOperations()

bool
NdbTransaction::releaseScanOperation(NdbIndexScanOperation** listhead,
				     NdbIndexScanOperation** listtail,
				     NdbIndexScanOperation* op)
{
  if (* listhead == op)
  {
    * listhead = (NdbIndexScanOperation*)op->theNext;
    if (listtail && *listtail == op)
    {
      assert(* listhead == nullptr);
      * listtail = nullptr;
    }
      
  }
  else
  {
    NdbIndexScanOperation* tmp = * listhead;
    while (tmp != nullptr)
    {
      if (tmp->theNext == op)
      {
	tmp->theNext = (NdbIndexScanOperation*)op->theNext;
	if (listtail && *listtail == op)
	{
	  assert(op->theNext == nullptr);
	  *listtail = tmp;
	}
	break;
      }
      tmp = (NdbIndexScanOperation*)tmp->theNext;
    }
    if (tmp == nullptr)
      op = nullptr;
  }
  
  if (op != nullptr)
  {
    if (unlikely(theErrorOperation == op))
    {
      /* Remove ref to scan op before release */
      theErrorLine = 0;
      theErrorOperation = nullptr;
    }
    op->release();
    theNdb->releaseScanOperation(op);
    return true;
  }
  
  return false;
}

void
NdbTransaction::releaseLockHandles()
{
  NdbLockHandle* lh = m_theFirstLockHandle;

  while (lh)
  {
    NdbLockHandle* next = lh->next();
    lh->next(nullptr);
    
    theNdb->releaseLockHandle(lh);
    lh = next;
  }

  m_theFirstLockHandle = nullptr;
  m_theLastLockHandle = nullptr;
}

/*****************************************************************************
NdbOperation* getNdbOperation(const char* aTableName);

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was successful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbOperation idlelist and get the 
                NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.
                synchronous
******************************************************************************/
NdbOperation*
NdbTransaction::getNdbOperation(const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
    if (table != nullptr){
      return getNdbOperation(table);
    } else {
      setErrorCode(theNdb->theDictionary->getNdbError().code);
      return nullptr;
    }//if
  }

  setOperationErrorCodeAbort(4114);
  
  return nullptr;
}//NdbTransaction::getNdbOperation()

/*****************************************************************************
NdbTransaction::checkSchemaObjects(const NdbTableImpl *tab,
                                   const NdbIndexImpl *idx)
Return value:    true if objects are all valid, false otherwise
Parameters:      table, optional index 
Remark:          If the schema object ownership check is enabled while creating 
                 the Ndb_cluster_connection, check that the connection is not
                 using schema objects which have been acquired by another 
                 connection. 
*****************************************************************************/
bool
NdbTransaction::checkSchemaObjects(const NdbTableImpl *tab,
                                   const NdbIndexImpl *idx)
{
  bool ret = true;
  if(m_enable_schema_obj_owner_check)
  {
    if(tab->m_indexType != NdbDictionary::Object::TypeUndefined)
      return ret; // skip index table passed by getNdbIndexScanOperation

    // check that table and index objects are owned by current connection - get 
    // dict objects from current connection and compare.
    char db[MAX_TAB_NAME_SIZE];
    tab->getDbName(db, sizeof(db));

    const char *old_db= theNdb->getDatabaseName();

    bool change_db= false; 
    if(strcmp(db, old_db) != 0)
      change_db = true;
    if(change_db && (strcmp(db, "") != 0)) // switch to db of current table if not blank
      theNdb->setDatabaseName(db);

    NdbDictionary::Table *dictTab = nullptr;
    NdbDictionary::Index *dictIdx = nullptr;
 
    dictTab = theNdb->theDictionary->getTable(tab->getName());
    if(idx)
      dictIdx = theNdb->theDictionary->getIndex(idx->getName(), tab->getName());
     
    if(change_db && strcmp(old_db, "") != 0) // restore original value of db if not blank
      theNdb->setDatabaseName(old_db);

    if(dictTab && (dictTab->getObjectId() == tab->getObjectId())
               && (dictTab->getObjectVersion() == tab->getObjectVersion())
               && (tab != &(NdbTableImpl::getImpl(*dictTab))))
    {
      g_eventLogger->info(
          "Schema object ownership check failed:"
          " table %s not owned by connection",
          tab->getName());
      ret = false;
    }
    if(idx && dictIdx && (dictTab->getObjectId() == idx->getObjectId())
               && (dictIdx->getObjectVersion() == idx->getObjectVersion())
               && (idx != &(NdbIndexImpl::getImpl(*dictIdx))))
    {
      g_eventLogger->info(
          "Schema object ownership check failed:"
          " index %s not owned by connection",
          idx->getName());
      ret = false;
    }
  }
  return ret;
} //NdbTransaction::checkSchemaObjects


/*****************************************************************************
NdbOperation* getNdbOperation(const NdbTableImpl* tab, NdbOperation* aNextOp)

Return Value    Return a pointer to a NdbOperation object if getNdbOperation 
                was successful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table being deleted.
Remark:         Get an operation from NdbOperation object idlelist and 
                get the NdbTransaction object who was fetch by 
                startTransaction pointing to this operation 
  	        getOperation will set the theTableId in the NdbOperation 
                object, synchronous.
*****************************************************************************/
NdbOperation*
NdbTransaction::getNdbOperation(const NdbTableImpl * tab,
                                NdbOperation* aNextOp)
{ 
  if (theScanningOp != nullptr || m_scanningQuery != nullptr){
    setErrorCode(4607);
    return nullptr;
  }
  if (!checkSchemaObjects(tab))
  {
    setErrorCode(1231);
    return nullptr;
  }
  
  NdbOperation* tOp = theNdb->getOperation();
  if (tOp == nullptr)
    goto getNdbOp_error1;

  if (aNextOp == nullptr) {
    if (theLastOpInList != nullptr) {
       theLastOpInList->next(tOp);
       theLastOpInList = tOp;
    } else {
       theLastOpInList = tOp;
       theFirstOpInList = tOp;
    }//if
    tOp->next(nullptr);
  } else {
    // add before the given op
    if (theFirstOpInList == aNextOp) {
      theFirstOpInList = tOp;
    } else {
      NdbOperation* aLoopOp = theFirstOpInList;
      while (aLoopOp != nullptr && aLoopOp->next() != aNextOp)
        aLoopOp = aLoopOp->next();
      assert(aLoopOp != nullptr);
      aLoopOp->next(tOp);
    }
    tOp->next(aNextOp);
  }
  if (tOp->init(tab, this) != -1) {
    return tOp;
  } else {
    theNdb->releaseOperation(tOp);
  }//if
  return nullptr;
  
 getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return nullptr;
}//NdbTransaction::getNdbOperation()

NdbOperation* NdbTransaction::getNdbOperation(const NdbDictionary::Table * table)
{
  if (table)
    return getNdbOperation(& NdbTableImpl::getImpl(*table));
  else
    return nullptr;
}//NdbTransaction::getNdbOperation()


// NdbScanOperation
/*****************************************************************************
NdbScanOperation* getNdbScanOperation(const char* aTableName);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was successful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbScanOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbOperation object.synchronous
******************************************************************************/
NdbScanOperation*
NdbTransaction::getNdbScanOperation(const char* aTableName)
{
  if (theCommitStatus == Started){
    NdbTableImpl* tab = theNdb->theDictionary->getTable(aTableName);
    if (tab != nullptr){
      return getNdbScanOperation(tab);
    } else {
      setOperationErrorCodeAbort(theNdb->theDictionary->m_error.code);
      return nullptr;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return nullptr;
}//NdbTransaction::getNdbScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbIndexScanOperation(const char* anIndexName, const char* aTableName);

Return Value    Return a pointer to a NdbIndexScanOperation object if getNdbIndexScanOperation was successful.
                Return NULL : In all other case. 	
Parameters:     anIndexName : Name of the index to use. 	
                aTableName : Name of the database table. 	
Remark:         Get an operation from NdbIndexScanOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this  operation  
		getOperation will set the theTableId in the NdbIndexScanOperation object.synchronous
******************************************************************************/
NdbIndexScanOperation*
NdbTransaction::getNdbIndexScanOperation(const char* anIndexName, 
					const char* aTableName)
{
  NdbIndexImpl* index = 
    theNdb->theDictionary->getIndex(anIndexName, aTableName);
  if (index == nullptr)
  {
    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return nullptr;
  }
  NdbTableImpl* table = theNdb->theDictionary->getTable(aTableName);
  if (table == nullptr)
  {
    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return nullptr;
  }

  return getNdbIndexScanOperation(index, table);
}

NdbIndexScanOperation*
NdbTransaction::getNdbIndexScanOperation(const NdbIndexImpl* index,
					const NdbTableImpl* table)
{
  if (theCommitStatus == Started){
    const NdbTableImpl * indexTable = index->getIndexTable();
    if (indexTable != nullptr){
      if (!checkSchemaObjects(table, index))
      {
        setErrorCode(1231);
        return nullptr;
      } 
      NdbIndexScanOperation* tOp = getNdbScanOperation(indexTable);
      if(tOp)
      {
	tOp->m_currentTable = table;
        // Mark that this really is an NdbIndexScanOperation
        tOp->m_type = NdbOperation::OrderedIndexScan;
      }
      return tOp;
    } else {
      setOperationErrorCodeAbort(4271);
      return nullptr;
    }//if
  } 
  
  setOperationErrorCodeAbort(4114);
  return nullptr;
}//NdbTransaction::getNdbIndexScanOperation()

NdbIndexScanOperation* 
NdbTransaction::getNdbIndexScanOperation(const NdbDictionary::Index * index)
{ 
  if (index)
  {
    /* This fetches the underlying table being indexed. */
    const NdbDictionary::Table *table=
      theNdb->theDictionary->getTable(index->getTable());

    if (table)
      return getNdbIndexScanOperation(index, table);

    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return nullptr;
  }
  setOperationErrorCodeAbort(4271);
  return nullptr;
}

NdbIndexScanOperation* 
NdbTransaction::getNdbIndexScanOperation(const NdbDictionary::Index * index,
					const NdbDictionary::Table * table)
{
  if (index && table)
    return getNdbIndexScanOperation(& NdbIndexImpl::getImpl(*index),
				    & NdbTableImpl::getImpl(*table));
  setOperationErrorCodeAbort(4271);
  return nullptr;
}//NdbTransaction::getNdbIndexScanOperation()

/*****************************************************************************
NdbScanOperation* getNdbScanOperation(int aTableId);

Return Value    Return a pointer to a NdbScanOperation object if getNdbScanOperation was successful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table being deleted.
Remark:         Get an operation from NdbScanOperation object idlelist and get the NdbTransaction 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbScanOperation object, synchronous.
*****************************************************************************/
NdbIndexScanOperation*
NdbTransaction::getNdbScanOperation(const NdbTableImpl * tab)
{ 
  if (!checkSchemaObjects(tab))
  {
    setErrorCode(1231);
    return nullptr;
  } 
  
  NdbIndexScanOperation* tOp = theNdb->getScanOperation();
  if (tOp == nullptr)
    goto getNdbOp_error1;

  if (tOp->init(tab, this) != -1) {
    define_scan_op(tOp);
    // Mark that this NdbIndexScanOperation is used as NdbScanOperation
    tOp->m_type = NdbOperation::TableScan; 
    return tOp;
  } else {
    tOp->release();
    theNdb->releaseScanOperation(tOp);
  }//if
  return nullptr;

getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return nullptr;
}//NdbTransaction::getNdbScanOperation()

void
NdbTransaction::remove_list(NdbOperation*& list, NdbOperation* op){
  NdbOperation* tmp= list;
  if(tmp == op)
    list = op->next();
  else {
    while(tmp && tmp->next() != op) tmp = tmp->next();
    if(tmp)
      tmp->next(op->next());
  }
  op->next(nullptr);
}

void
NdbTransaction::define_scan_op(NdbIndexScanOperation * tOp){
  // Link scan operation into list of cursor operations
  if (m_theLastScanOperation == nullptr)
    m_theFirstScanOperation = m_theLastScanOperation = tOp;
  else {
    m_theLastScanOperation->next(tOp);
    m_theLastScanOperation = tOp;
  }
  tOp->next(nullptr);
}

NdbScanOperation* 
NdbTransaction::getNdbScanOperation(const NdbDictionary::Table * table)
{
  if (table)
    return getNdbScanOperation(& NdbTableImpl::getImpl(*table));
  else
    return nullptr;
}//NdbTransaction::getNdbScanOperation()


// IndexOperation
/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(const char* anIndexName,
					const char* aTableName);

Return Value    Return a pointer to an NdbIndexOperation object if
                getNdbIndexOperation was successful.
                Return NULL : In all other case. 	
Parameters:     aTableName : Name of the database table. 	
Remark:         Get an operation from NdbIndexOperation idlelist and get the NdbTransaction object 
		who was fetch by startTransaction pointing to this operation  
		getOperation will set the theTableId in the NdbIndexOperation object.synchronous
******************************************************************************/
NdbIndexOperation*
NdbTransaction::getNdbIndexOperation(const char* anIndexName, 
                                    const char* aTableName)
{
  if (theCommitStatus == Started) {
    NdbTableImpl * table = theNdb->theDictionary->getTable(aTableName);
    NdbIndexImpl * index;

    if (table == nullptr)
    {
      setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
      return nullptr;
    }

    if (table->m_frm.get_data())
    {
      // This unique index is defined from SQL level
      static const char* uniqueSuffix= "$unique";
      BaseString uniqueIndexName(anIndexName);
      uniqueIndexName.append(uniqueSuffix);
      index = theNdb->theDictionary->getIndex(uniqueIndexName.c_str(),
					      aTableName);      
    }
    else
      index = theNdb->theDictionary->getIndex(anIndexName,
					      aTableName);
    if(table != nullptr && index != nullptr){
      return getNdbIndexOperation(index, table);
    }
    
    if(index == nullptr){
      setOperationErrorCodeAbort(4243);
      return nullptr;
    }

    setOperationErrorCodeAbort(4243);
    return nullptr;
  } 
  
  setOperationErrorCodeAbort(4114);
  return nullptr;
}//NdbTransaction::getNdbIndexOperation()

/*****************************************************************************
NdbIndexOperation* getNdbIndexOperation(int anIndexId, int aTableId);

Return Value    Return a pointer to a NdbIndexOperation object if getNdbIndexOperation was successful.
                Return NULL: In all other case. 	
Parameters:     tableId : Id of the database table being deleted.
Remark:         Get an operation from NdbIndexOperation object idlelist and get the NdbTransaction 
                object who was fetch by startTransaction pointing to this  operation 
  	        getOperation will set the theTableId in the NdbIndexOperation object, synchronous.
*****************************************************************************/
NdbIndexOperation*
NdbTransaction::getNdbIndexOperation(const NdbIndexImpl * anIndex, 
                                     const NdbTableImpl * aTable,
                                     NdbOperation* aNextOp)
{ 
  if (!checkSchemaObjects(aTable, anIndex))
  {
    setErrorCode(1231);
    return nullptr;
  } 
  NdbIndexOperation* tOp = theNdb->getIndexOperation();
  if (tOp == nullptr)
    goto getNdbOp_error1;

  if (aNextOp == nullptr) {
    if (theLastOpInList != nullptr) {
       theLastOpInList->next(tOp);
       theLastOpInList = tOp;
    } else {
       theLastOpInList = tOp;
       theFirstOpInList = tOp;
    }//if
    tOp->next(nullptr);
  } else {
    // add before the given op
    if (theFirstOpInList == aNextOp) {
      theFirstOpInList = tOp;
    } else {
      NdbOperation* aLoopOp = theFirstOpInList;
      while (aLoopOp != nullptr && aLoopOp->next() != aNextOp)
        aLoopOp = aLoopOp->next();
      assert(aLoopOp != nullptr);
      aLoopOp->next(tOp);
    }
    tOp->next(aNextOp);
  }
  if (tOp->indxInit(anIndex, aTable, this)!= -1) {
    return tOp;
  } else {
    theNdb->releaseOperation(tOp);
  }//if
  return nullptr;
  
 getNdbOp_error1:
  setOperationErrorCodeAbort(4000);
  return nullptr;
}//NdbTransaction::getNdbIndexOperation()

NdbIndexOperation* 
NdbTransaction::getNdbIndexOperation(const NdbDictionary::Index * index)
{ 
  if (index)
  {
    const NdbDictionary::Table *table=
      theNdb->theDictionary->getTable(index->getTable());

    if (table)
      return getNdbIndexOperation(index, table);

    setOperationErrorCodeAbort(theNdb->theDictionary->getNdbError().code);
    return nullptr;
  }
  setOperationErrorCodeAbort(4271);
  return nullptr;
}

NdbIndexOperation* 
NdbTransaction::getNdbIndexOperation(const NdbDictionary::Index * index,
				    const NdbDictionary::Table * table)
{
  if (index && table)
    return getNdbIndexOperation(& NdbIndexImpl::getImpl(*index),
				& NdbTableImpl::getImpl(*table));
  
  setOperationErrorCodeAbort(4271);
  return nullptr;
}//NdbTransaction::getNdbIndexOperation()


/*******************************************************************************
int  receiveTCSEIZECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer at reception of TCSEIZECONF. 
*******************************************************************************/
int			
NdbTransaction::receiveTCSEIZECONF(const NdbApiSignal* aSignal)
{
  if (theStatus != Connecting)
  {
    return -1;
  } else
  {
    theTCConPtr = (Uint32)aSignal->readData(2);
    if (aSignal->getLength() >= 3)
    {
      m_tcRef = aSignal->readData(3);
    }
    else
    {
      m_tcRef = numberToRef(DBTC, theDBnode);
    }

    assert(m_tcRef == aSignal->theSendersBlockRef);

    theStatus = Connected;
  }
  return 0;
}//NdbTransaction::receiveTCSEIZECONF()

/*******************************************************************************
int  receiveTCSEIZEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCSEIZEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        Sets TC Connect pointer. 
*******************************************************************************/
int			
NdbTransaction::receiveTCSEIZEREF(const NdbApiSignal* aSignal)
{
  DBUG_ENTER("NdbTransaction::receiveTCSEIZEREF");
  if (theStatus != Connecting)
  {
    DBUG_RETURN(-1);
  } else
  {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    DBUG_PRINT("info",("error code %d, %s",
		       theNdb->getNdbError().code,
		       theNdb->getNdbError().message));
    DBUG_RETURN(0);
  }
}//NdbTransaction::receiveTCSEIZEREF()

/*******************************************************************************
int  receiveTCRELEASECONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASECONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:         DisConnect TC Connect pointer to NDBAPI. 
*******************************************************************************/
int NdbTransaction::receiveTCRELEASECONF(const NdbApiSignal* /*aSignal*/)
{
  if (theStatus != DisConnecting)
  {
    return -1;
  } else
  {
    theStatus = NotConnected;
  }
  return 0;
}//NdbTransaction::receiveTCRELEASECONF()

/*******************************************************************************
int  receiveTCRELEASEREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCRELEASEREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        DisConnect TC Connect pointer to NDBAPI Failure. 
*******************************************************************************/
int			
NdbTransaction::receiveTCRELEASEREF(const NdbApiSignal* aSignal)
{
  if (theStatus != DisConnecting) {
    return -1;
  } else {
    theStatus = ConnectFailure;
    theNdb->theError.code = aSignal->readData(2);
    return 0;
  }//if
}//NdbTransaction::receiveTCRELEASEREF()

/******************************************************************************
int  receiveTC_COMMITCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTC_COMMITCONF(const TcCommitConf * commitConf, 
                                     Uint32 len)
{ 
  if(checkState_TransId(&commitConf->transId1)){
    theCommitStatus = Committed;
    theCompletionStatus = CompletedSuccess;
    Uint32 tGCI_hi = commitConf->gci_hi;
    Uint32 tGCI_lo = commitConf->gci_lo;
    if (unlikely(len < TcCommitConf::SignalLength))
    {
      tGCI_lo = 0;
    }
    Uint64 tGCI = Uint64(tGCI_lo) | (Uint64(tGCI_hi) << 32);
    theGlobalCheckpointId = tGCI;
    // theGlobalCheckpointId == 0 if NoOp transaction
    if (tGCI)
      *p_latest_trans_gci = tGCI;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }
  return -1;
}//NdbTransaction::receiveTC_COMMITCONF()

/******************************************************************************
int  receiveTC_COMMITREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTC_COMMITREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTC_COMMITREF(const NdbApiSignal* aSignal)
{
  const TcCommitRef * ref = CAST_CONSTPTR(TcCommitRef, aSignal->getDataPtr());
  if(checkState_TransId(&ref->transId1)){
    setOperationErrorCodeAbort(ref->errorCode);
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedFailure;
    theReturnStatus = ReturnFailure;
    theTransactionId = InvalidTransactionId; /* No further signals please */
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTC_COMMITREF()

/******************************************************************************
int  receiveTCROLLBACKCONF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
******************************************************************************/
int			
NdbTransaction::receiveTCROLLBACKCONF(const NdbApiSignal* aSignal)
{
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedSuccess;
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTCROLLBACKCONF()

/*******************************************************************************
int  receiveTCROLLBACKREF(NdbApiSignal* aSignal);

Return Value:  Return 0 : receiveTCROLLBACKREF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbTransaction::receiveTCROLLBACKREF(const NdbApiSignal* aSignal)
{
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    setOperationErrorCodeAbort(aSignal->readData(4));
    theCommitStatus = Aborted;
    theCompletionStatus = CompletedFailure;
    theReturnStatus = ReturnFailure;
    theTransactionId = InvalidTransactionId; /* No further signals please */
    return 0;
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  return -1;
}//NdbTransaction::receiveTCROLLBACKREF()

/*****************************************************************************
int receiveTCROLLBACKREP( NdbApiSignal* aSignal)

Return Value:   Return 0 : send was successful.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCROLLBACKREP signal from TC.
Remark:         Handles the reception of the ROLLBACKREP signal.
*****************************************************************************/
int
NdbTransaction::receiveTCROLLBACKREP( const NdbApiSignal* aSignal)
{
  DBUG_ENTER("NdbTransaction::receiveTCROLLBACKREP");

  /****************************************************************************
Check that we are expecting signals from this transaction and that it doesn't
belong to a transaction already completed. Simply ignore messages from other 
transactions.
  ****************************************************************************/
  if(checkState_TransId(aSignal->getDataPtr() + 1)){
    theError.code = aSignal->readData(4);// Override any previous errors
    if (aSignal->getLength() == TcRollbackRep::SignalLength)
    {
      // Signal may contain additional error data
      theError.details = (char *)UintPtr(aSignal->readData(5));
    }

    /**********************************************************************/
    /*	A serious error has occurred. This could be due to deadlock or */
    /*	lack of resources or simply a programming error in NDB. This  */
    /*	transaction will be aborted. Actually it has already been     */
    /*	and we only need to report completion and return with the     */
    /*	error code to the application.				      */
    /**********************************************************************/
    theTransactionId = InvalidTransactionId; /* No further signals please */
    theCompletionStatus = CompletedFailure;
    theCommitStatus = Aborted;
    theReturnStatus = ReturnFailure;
    DBUG_RETURN(0);
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }

  DBUG_RETURN(-1);
}//NdbTransaction::receiveTCROLLBACKREP()

/*******************************************************************************
int  receiveTCKEYCONF(NdbApiSignal* aSignal, Uint32 long_short_ind);

Return Value:  Return 0 : receiveTCKEYCONF was successful.
               Return -1: In all other case.
Parameters:    aSignal: The signal object pointer.
Remark:        
*******************************************************************************/
int			
NdbTransaction::receiveTCKEYCONF(const TcKeyConf * keyConf, Uint32 aDataLength)
{
  const Uint32 tTemp = keyConf->confInfo;
  /***************************************************************************
Check that we are expecting signals from this transaction and that it
doesn't belong to a transaction already completed. Simply ignore messages
from other transactions.
  ***************************************************************************/
  if(checkState_TransId(&keyConf->transId1)){

    const Uint32 tNoOfOperations = TcKeyConf::getNoOfOperations(tTemp);
    const Uint32 tCommitFlag = TcKeyConf::getCommitFlag(tTemp);

    const Uint32* tPtr = (const Uint32*)&keyConf->operations[0];
    Uint32 tNoComp = theNoOfOpCompleted;
    for (Uint32 i = 0; i < tNoOfOperations ; i++) {
      NdbReceiver* const tReceiver = 
        NdbImpl::void2rec(theNdb->theImpl->int2void(*tPtr++));
      const Uint32 tAttrInfoLen = *tPtr++;
      if(tReceiver && tReceiver->checkMagicNumber()){
        Uint32 done;
        if(tReceiver->getType()==NdbReceiver::NDB_QUERY_OPERATION){ 
          /* This signal is part of a linked operation.*/
          done = ((NdbQueryOperationImpl*)(tReceiver->m_owner))
            ->getQuery().execTCKEYCONF();
        }else{
          done = tReceiver->execTCOPCONF(tAttrInfoLen);
        }
	if(tAttrInfoLen > TcKeyConf::DirtyReadBit){
	  Uint32 node = tAttrInfoLen & (~TcKeyConf::DirtyReadBit);
          NdbNodeBitmask::set(m_db_nodes, node);
          if(NdbNodeBitmask::get(m_failed_db_nodes, node) && !done)
	  {
            done = 1;
            // 4119 = "Simple/dirty read failed due to node failure"
            tReceiver->setErrorCode(4119);
            theCompletionStatus = CompletedFailure;
            theReturnStatus = NdbTransaction::ReturnFailure;
	  }	    
	}
	tNoComp += done;
      } else { // if(tReceiver && tReceiver->checkMagicNumber())
 	return -1;
      }//if
    }//for
    theNoOfOpCompleted = tNoComp;
    const Uint32 tNoSent = theNoOfOpSent;
    const Uint32 tGCI_hi = keyConf->gci_hi;
    Uint32       tGCI_lo = * tPtr; // After op(s)
    if (unlikely(aDataLength < TcKeyConf::StaticLength+1 + 2*tNoOfOperations))
    {
      tGCI_lo = 0;
    }
    const Uint64 tGCI = Uint64(tGCI_lo) | (Uint64(tGCI_hi) << 32);
    if (tCommitFlag == 1) 
    {
      theCommitStatus = Committed;
      theGlobalCheckpointId = tGCI;
      if (tGCI) // Read(dirty) only transaction doesn't get GCI
      {
	*p_latest_trans_gci = tGCI;
      }
    } 
    else if (theLastExecOpInList &&
             theLastExecOpInList->theCommitIndicator == 1)
    {  
      /**
       * We're waiting for a commit reply...
       */
      return -1;
    }//if
    if (tNoComp >= tNoSent) 
    {
      return 0;	// No more operations to wait for
    }//if
     // Not completed the reception yet.
  } else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
  }
  
  return -1;
}//NdbTransaction::receiveTCKEYCONF()

/*****************************************************************************
int receiveTCKEY_FAILCONF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILCONF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILCONF signal.
*****************************************************************************/
int
NdbTransaction::receiveTCKEY_FAILCONF(const TcKeyFailConf * failConf)
{
  NdbOperation*	tOp;
  /*
    Check that we are expecting signals from this transaction and that it 
    doesn't belong  to a transaction already completed. Simply ignore 
    messages from other transactions.
  */
  if(checkState_TransId(&failConf->transId1)){
    /*
      A node failure of the TC node occurred. The transaction has
      been committed.
    */
    theCommitStatus = Committed;
    theTransactionId = InvalidTransactionId; /* No further signals please */
    tOp = theFirstExecOpInList;
    while (tOp != nullptr) {
      /*
       * Check if the transaction expected read values...
       * If it did some of them might have gotten lost even if we succeeded
       * in committing the transaction.
       */
      switch(tOp->theOperationType){
      case NdbOperation::UpdateRequest:
      case NdbOperation::InsertRequest:
      case NdbOperation::DeleteRequest:
      case NdbOperation::WriteRequest:
      case NdbOperation::UnlockRequest:
      case NdbOperation::RefreshRequest:
	tOp = tOp->next();
	break;
      case NdbOperation::ReadRequest:
      case NdbOperation::ReadExclusive:
      case NdbOperation::OpenScanRequest:
      case NdbOperation::OpenRangeScanRequest:
	theCompletionStatus = CompletedFailure;
	theReturnStatus = NdbTransaction::ReturnFailure;
	setOperationErrorCodeAbort(4115);
	tOp = nullptr;
	break;
      case NdbOperation::NotDefined:
      case NdbOperation::NotDefined2:
	assert(false);
	break;
      }//if
    }//while   
    theReleaseOnClose = true;
    return 0;
  } else {
#ifdef VM_TRACE
    g_eventLogger->info("Recevied TCKEY_FAILCONF wo/ operation");
#endif
  }
  return -1;
}//NdbTransaction::receiveTCKEY_FAILCONF()

/*************************************************************************
int receiveTCKEY_FAILREF( NdbApiSignal* aSignal)

Return Value:   Return 0 : receive was completed.
                Return -1: In all other case.   
Parameters:     aSignal: the signal object that contains the 
                TCKEY_FAILREF signal from TC.
Remark:         Handles the reception of the TCKEY_FAILREF signal.
**************************************************************************/
int
NdbTransaction::receiveTCKEY_FAILREF(const NdbApiSignal* aSignal)
{
  /*
    Check that we are expecting signals from this transaction and
    that it doesn't belong to a transaction already
    completed. Simply ignore messages from other transactions.
  */
  if(checkState_TransId(aSignal->getDataPtr()+1)){
    /*
      We received an indication of that this transaction was aborted due to a
      node failure.
    */
    if (theSendStatus == NdbTransaction::sendTC_ROLLBACK) {
      /*
	We were in the process of sending a rollback anyways. We will
	report it as a success.
      */
      theCompletionStatus = NdbTransaction::CompletedSuccess;
    } else {
      theReturnStatus = NdbTransaction::ReturnFailure;
      theCompletionStatus = NdbTransaction::CompletedFailure;
      theError.code = 4031;
    }//if
    theReleaseOnClose = true;
    theCommitStatus = NdbTransaction::Aborted;
    theTransactionId = InvalidTransactionId; /* No further signals please */
    return 0;
  } else {
#ifdef VM_TRACE
    g_eventLogger->info("Recevied TCKEY_FAILREF wo/ operation");
#endif
  }
  return -1;
}//NdbTransaction::receiveTCKEY_FAILREF()

/*******************************************************************************
int OpCompletedFailure();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.
Remark:        An operation was completed with failure.
*******************************************************************************/
int 
NdbTransaction::OpCompleteFailure()
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;

  tNoComp++;
  theNoOfOpCompleted = tNoComp;
  
  return (tNoComp == tNoSent) ? 0 : -1;
}//NdbTransaction::OpCompleteFailure()

/******************************************************************************
int OpCompleteSuccess();

Return Value:  Return 0 : OpCompleteSuccess was successful.
               Return -1: In all other case.  
Remark:        An operation was completed with success.
*******************************************************************************/
int 
NdbTransaction::OpCompleteSuccess()
{
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  tNoComp++;
  theNoOfOpCompleted = tNoComp;
#ifdef JW_TEST
  ndbout << "NdbTransaction::OpCompleteSuccess() tNoComp=" << tNoComp 
	 << " tNoSent=" << tNoSent << endl;
#endif
  if (tNoComp == tNoSent) { // Last operation completed
    return 0;
  } else if (tNoComp < tNoSent) {
    return -1;	// Continue waiting for more signals
  } else {
    setOperationErrorCodeAbort(4113);	// Too many operations, 
                                        // stop waiting for more
    theCompletionStatus = NdbTransaction::CompletedFailure;
    theReturnStatus = NdbTransaction::ReturnFailure;
    return 0;
  }//if
}//NdbTransaction::OpCompleteSuccess()

/******************************************************************************
 int            getGCI();

Remark:		Get global checkpoint identity of the transaction
*******************************************************************************/
int
NdbTransaction::getGCI()
{
  Uint64 val;
  if (getGCI(&val) == 0)
  {
    return (int)(val >> 32);
  }
  return -1;
}

int
NdbTransaction::getGCI(Uint64 * val)
{
  if (theCommitStatus == NdbTransaction::Committed)
  {
    if (val)
    {
      * val = theGlobalCheckpointId;
    }
    return 0;
  }
  return -1;
}

/*******************************************************************************
Uint64 getTransactionId(void);

Remark:        Get the transaction identity. 
*******************************************************************************/
Uint64
NdbTransaction::getTransactionId()
{
  return theTransactionId;
}//NdbTransaction::getTransactionId()

NdbTransaction::CommitStatusType
NdbTransaction::commitStatus()
{
  return theCommitStatus;
}//NdbTransaction::commitStatus()

int 
NdbTransaction::getNdbErrorLine()
{
  return theErrorLine;
}

NdbOperation*
NdbTransaction::getNdbErrorOperation()
{
  return theErrorOperation;
}//NdbTransaction::getNdbErrorOperation()


const NdbOperation*
NdbTransaction::getNdbErrorOperation() const
{
  return theErrorOperation;
}//NdbTransaction::getNdbErrorOperation()


const NdbOperation * 
NdbTransaction::getNextCompletedOperation(const NdbOperation * current) const {
  if(current == nullptr)
    return theCompletedFirstOp;
  return current->theNext;
}

NdbOperation *
NdbTransaction::setupRecordOp(NdbOperation::OperationType type,
                              NdbOperation::LockMode lock_mode,
                              NdbOperation::AbortOption default_ao,
                              const NdbRecord *key_record,
                              const char *key_row,
                              const NdbRecord *attribute_record,
                              const char *attribute_row,
                              const unsigned char *mask,
                              const NdbOperation::OperationOptions *opts,
                              Uint32 sizeOfOptions,
                              const NdbLockHandle* lh)
{
  NdbOperation *op;
  
  /* Check that we've got a base table record for the attribute record */
  if (attribute_record->flags & NdbRecord::RecIsIndex)
  {
    /* Result or attribute record must be a base 
       table ndbrecord, not an index ndbrecord */
    setOperationErrorCodeAbort(4340);
    return nullptr;
  }
  /*
    We are actually passing the table object for the index here, not the table
    object of the underlying table. But we only need it to keep the existing
    NdbOperation code happy, it is not actually used for NdbRecord operation.
    We will eliminate the need for passing table and index completely when
    implementing WL#3707.
  */
  if (key_record->flags & NdbRecord::RecIsIndex)
  {
    op= getNdbIndexOperation(key_record->table->m_index,
                             attribute_record->table, nullptr);
  }
  else
  {
    if (key_record->tableId != attribute_record->tableId)
    {
      setOperationErrorCodeAbort(4287);
      return nullptr;
    }
    op= getNdbOperation(attribute_record->table, nullptr);
  }
  if(!op)
    return nullptr;

  op->theStatus= NdbOperation::UseNdbRecord;
  op->theOperationType= type;
  op->theErrorLine++;
  op->theLockMode= lock_mode;
  op->m_key_record= key_record;
  op->m_key_row= key_row;
  op->m_attribute_record= attribute_record;
  op->m_attribute_row= attribute_row;
  op->m_abortOption=default_ao;
  op->theLockHandle = const_cast<NdbLockHandle*>(lh);
  
  AttributeMask readMask;
  attribute_record->copyMask(readMask.rep.data, mask);
  
  /*
   * Handle options
   */
  if (opts != nullptr)
  {
    /* Delegate to static method in NdbOperation */
    Uint32 result = NdbOperation::handleOperationOptions (type,
                                                          opts,
                                                          sizeOfOptions,
                                                          op);
    if (result !=0)
    {
      setOperationErrorCodeAbort(result);
      return nullptr;
    }
  }

  /* Handle delete + blobs */
  if (type == NdbOperation::DeleteRequest &&
      (attribute_record->flags & NdbRecord::RecTableHasBlob))
  {
    /* Need to link in all the Blob handles for delete 
     * If there is a pre-read, check that no Blobs have
     * been asked for
     */
    if (op->getBlobHandlesNdbRecordDelete(this,
                                          (attribute_row != nullptr),
                                          readMask.rep.data) == -1)
      return nullptr;
  }
  else if (unlikely((attribute_record->flags & NdbRecord::RecHasBlob) &&
                    (type != NdbOperation::UnlockRequest)))
  {
    /* Create blob handles for non-delete, non-unlock operations */
    if (op->getBlobHandlesNdbRecord(this, readMask.rep.data) == -1)
      return nullptr;
  }

  /*
   * Now prepare the signals to be sent...
   *
   */
  int returnCode=op->buildSignalsNdbRecord(theTCConPtr, theTransactionId,
                                           readMask.rep.data);

  if (returnCode)
  {
    // buildSignalsNdbRecord should have set the error status
    // So we can return NULL
    return nullptr;
  }

  return op;
}



const NdbOperation *
NdbTransaction::readTuple(const NdbRecord *key_rec, const char *key_row,
                          const NdbRecord *result_rec, char *result_row,
                          NdbOperation::LockMode lock_mode,
                          const unsigned char *result_mask,
                          const NdbOperation::OperationOptions *opts,
                          Uint32 sizeOfOptions)
{
  bool upgraded_lock = false;
  /* Check that the NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  if (key_rec->flags & NdbRecord::RecIsIndex &&
      lock_mode == NdbOperation::LM_CommittedRead)
  {
    lock_mode= NdbOperation::LM_Read;
    upgraded_lock = true;
  }

  NdbOperation::OperationType opType=
    (lock_mode == NdbOperation::LM_Exclusive ?
       NdbOperation::ReadExclusive : NdbOperation::ReadRequest);
  NdbOperation *op= setupRecordOp(opType, lock_mode, 
                                  NdbOperation::AO_IgnoreError,
                                  key_rec, key_row, 
                                  result_rec, result_row, result_mask,
                                  opts,
                                  sizeOfOptions);
  if (!op)
    return nullptr;

  if (upgraded_lock)
  {
    DBUG_PRINT("info", ("Set ReadCommittedBase true"));
    op->setReadCommittedBase();
  }
  if (op->theLockMode == NdbOperation::LM_CommittedRead)
  {
    op->theDirtyIndicator= 1;
    op->theSimpleIndicator= 1;
  }
  else 
  {
    if (op->theLockMode == NdbOperation::LM_SimpleRead)
    {
      op->theSimpleIndicator = 1;
    }
    
    
    theSimpleState= 0;
  }

  /* Setup the record/row for receiving the results. */
  op->theReceiver.getValues(result_rec, result_row);

  return op;
}

const NdbOperation *
NdbTransaction::insertTuple(const NdbRecord *key_rec, const char *key_row,
                            const NdbRecord *attr_rec, const char *attr_row,
                            const unsigned char *mask,
                            const NdbOperation::OperationOptions *opts,
                            Uint32 sizeOfOptions)
{
  /* Check that the NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  NdbOperation *op= setupRecordOp(NdbOperation::InsertRequest,
                                  NdbOperation::LM_Exclusive, 
                                  NdbOperation::AbortOnError, 
                                  key_rec, key_row,
                                  attr_rec, attr_row, mask,
                                  opts,
                                  sizeOfOptions);
  if (!op)
    return nullptr;

  theSimpleState= 0;

  return op;
}

const NdbOperation *
NdbTransaction::insertTuple(const NdbRecord *combined_rec, const char *combined_row,
                            const unsigned char *mask,
                            const NdbOperation::OperationOptions *opts,
                            Uint32 sizeOfOptions)
{
  return insertTuple(combined_rec, combined_row,
                     combined_rec, combined_row,
                     mask,
                     opts,
                     sizeOfOptions);
}

const NdbOperation *
NdbTransaction::updateTuple(const NdbRecord *key_rec, const char *key_row,
                            const NdbRecord *attr_rec, const char *attr_row,
                            const unsigned char *mask,
                            const NdbOperation::OperationOptions *opts,
                            Uint32 sizeOfOptions)
{
  /* Check that the NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  NdbOperation *op= setupRecordOp(NdbOperation::UpdateRequest,
                                  NdbOperation::LM_Exclusive, 
                                  NdbOperation::AbortOnError, 
                                  key_rec, key_row,
                                  attr_rec, attr_row, mask, 
                                  opts,
                                  sizeOfOptions);
  if(!op)
    return op;

  theSimpleState= 0;

  return op;
}

const NdbOperation *
NdbTransaction::deleteTuple(const NdbRecord *key_rec, 
                            const char *key_row,
                            const NdbRecord *result_rec,
                            char *result_row,
                            const unsigned char *result_mask,
                            const NdbOperation::OperationOptions* opts,
                            Uint32 sizeOfOptions)
{
  /* Check that the key NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  NdbOperation *op= setupRecordOp(NdbOperation::DeleteRequest,
                                  NdbOperation::LM_Exclusive, 
                                  NdbOperation::AbortOnError, 
                                  key_rec, key_row,
                                  result_rec, result_row, result_mask, 
                                  opts,
                                  sizeOfOptions);
  if(!op)
    return op;

  theSimpleState= 0;

  if (result_row != nullptr) // readBeforeDelete
  {
    /* Setup the record/row for receiving the results. */
    op->theReceiver.getValues(result_rec, result_row);
  }

  return op;
}

const NdbOperation *
NdbTransaction::writeTuple(const NdbRecord *key_rec, const char *key_row,
                           const NdbRecord *attr_rec, const char *attr_row,
                           const unsigned char *mask,
                           const NdbOperation::OperationOptions *opts,
                           Uint32 sizeOfOptions)
{
  /* Check that the NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  NdbOperation *op= setupRecordOp(NdbOperation::WriteRequest,
                                  NdbOperation::LM_Exclusive, 
                                  NdbOperation::AbortOnError, 
                                  key_rec, key_row,
                                  attr_rec, attr_row, mask, 
                                  opts,
                                  sizeOfOptions);
  if(!op)
    return op;

  theSimpleState= 0;

  return op;
}

const NdbOperation *
NdbTransaction::refreshTuple(const NdbRecord *key_rec, const char *key_row,
                             const NdbOperation::OperationOptions *opts,
                             Uint32 sizeOfOptions)
{
  /* Check that the NdbRecord specifies the full primary key. */
  if (!(key_rec->flags & NdbRecord::RecHasAllKeys))
  {
    setOperationErrorCodeAbort(4292);
    return nullptr;
  }

  if (key_rec->flags & NdbRecord::RecTableHasBlob)
  {
    // Table with blobs does not support refreshTuple()
    setOperationErrorCodeAbort(4343);
    return nullptr;
  }

  Uint8 keymask[NDB_MAX_ATTRIBUTES_IN_TABLE/8];
  std::memset(keymask, 0, sizeof(keymask));
  for (Uint32 i = 0; i<key_rec->key_index_length; i++)
  {
    Uint32 id = key_rec->columns[key_rec->key_indexes[i]].attrId;
    keymask[(id / 8)] |= (1 << (id & 7));
  }

  NdbOperation *op= setupRecordOp(NdbOperation::RefreshRequest,
                                  NdbOperation::LM_Exclusive,
                                  NdbOperation::AbortOnError,
                                  key_rec, key_row,
                                  key_rec, key_row,
                                  keymask /* mask */,
                                  opts,
                                  sizeOfOptions);
  if(!op)
    return op;

  theSimpleState= 0;

  return op;
}

NdbScanOperation *
NdbTransaction::scanTable(const NdbRecord *result_record,
                          NdbOperation::LockMode lock_mode,
                          const unsigned char *result_mask,
                          const NdbScanOperation::ScanOptions *options,
                          Uint32 sizeOfOptions)
{
  DBUG_ENTER("NdbTransaction::scanTable");
  DBUG_PRINT("info", ("Options=%p(0x%x)", options,
                      (options ? (unsigned)(options->optionsPresent) : 0)));
  /*
    Normal scan operations are created as NdbIndexScanOperations.
    The reason for this is that they can then share a pool of allocated
    objects.
  */
  NdbIndexScanOperation *op_idx= 
    getNdbScanOperation(result_record->table);

  if (op_idx == nullptr)
  {
    /* Memory allocation error */
    setOperationErrorCodeAbort(4000);
    DBUG_RETURN(NULL);
  }

  op_idx->m_scanUsingOldApi= false;

  /* The real work is done in NdbScanOperation */
  if (op_idx->scanTableImpl(result_record,
                            lock_mode,
                            result_mask,
                            options,
                            sizeOfOptions) == 0)
  {
    DBUG_RETURN(op_idx);
  }

  releaseScanOperation(&m_theFirstScanOperation, &m_theLastScanOperation,
                       op_idx);
  DBUG_RETURN(NULL);
}



NdbIndexScanOperation *
NdbTransaction::scanIndex(const NdbRecord *key_record,
                          const NdbRecord *result_record,
                                NdbOperation::LockMode lock_mode,
                          const unsigned char *result_mask,
                          const NdbIndexScanOperation::IndexBound *bound,
                          const NdbScanOperation::ScanOptions *options,
                          Uint32 sizeOfOptions)
{
  /*
    Normal scan operations are created as NdbIndexScanOperations.
    The reason for this is that they can then share a pool of allocated
    objects.
  */
  NdbIndexScanOperation *op= getNdbScanOperation(key_record->table);
  if (op==nullptr)
  {
    /* Memory allocation error */
    setOperationErrorCodeAbort(4000);
    return nullptr;
  }

  op->m_scanUsingOldApi= false;

  /* Defer the rest of the work to NdbIndexScanOperation */
  if (op->scanIndexImpl(key_record,
                        result_record,
                        lock_mode,
                        result_mask,
                        bound,
                        options,
                        sizeOfOptions) != 0)
  {
    releaseScanOperation(&m_theFirstScanOperation, &m_theLastScanOperation, op);
    return nullptr;
  }
  
  return op;
} // ::scanIndex();

Uint32
NdbTransaction::getMaxPendingBlobReadBytes() const
{
  /* 0 == max */
  return (maxPendingBlobReadBytes == 
          (~Uint32(0)) ? 0 : maxPendingBlobReadBytes);
}

Uint32
NdbTransaction::getMaxPendingBlobWriteBytes() const
{
  /* 0 == max */
  return (maxPendingBlobWriteBytes == 
          (~Uint32(0)) ? 0 : maxPendingBlobWriteBytes);
}

void
NdbTransaction::setMaxPendingBlobReadBytes(Uint32 bytes)
{
  /* 0 == max */
  maxPendingBlobReadBytes = (bytes?bytes : (~ Uint32(0)));
}

void
NdbTransaction::setMaxPendingBlobWriteBytes(Uint32 bytes)
{
  DBUG_PRINT("info", ("Setting Blob max pending bytes %d",
                      bytes));
  /* 0 == max */
  maxPendingBlobWriteBytes = (bytes?bytes : (~ Uint32(0)));
}

#ifdef VM_TRACE
#define CASE(x) case x: ndbout << " " << #x; break
void
NdbTransaction::printState()
{
  ndbout << "con=" << hex << this << dec;
  ndbout << " node=" << getConnectedNodeId();
  switch (theStatus) {
  CASE(NotConnected);
  CASE(Connecting);
  CASE(Connected);
  CASE(DisConnecting);
  CASE(ConnectFailure);
  default: ndbout << (Uint32) theStatus;
  }
  switch (theListState) {
  CASE(NotInList);
  CASE(InPreparedList);
  CASE(InSendList);
  CASE(InCompletedList);
  default: ndbout << (Uint32) theListState;
  }
  switch (theSendStatus) {
  CASE(NotInit);
  CASE(InitState);
  CASE(sendOperations);
  CASE(sendCompleted);
  CASE(sendCOMMITstate);
  CASE(sendABORT);
  CASE(sendABORTfail);
  CASE(sendTC_ROLLBACK);
  CASE(sendTC_COMMIT);
  CASE(sendTC_OP);
  default: ndbout << (Uint32) theSendStatus;
  }
  switch (theCommitStatus) {
  CASE(NotStarted);
  CASE(Started);
  CASE(Committed);
  CASE(Aborted);
  CASE(NeedAbort);
  default: ndbout << (Uint32) theCommitStatus;
  }
  switch (theCompletionStatus) {
  CASE(NotCompleted);
  CASE(CompletedSuccess);
  CASE(CompletedFailure);
  CASE(DefinitionFailure);
  default: ndbout << (Uint32) theCompletionStatus;
  }
  ndbout << endl;
}
#undef CASE
#endif

int
NdbTransaction::report_node_failure(Uint32 id){
  NdbNodeBitmask::set(m_failed_db_nodes, id);
  if(!NdbNodeBitmask::get(m_db_nodes, id))
  {
    return 0;
  }
  
  /**
   *   Arrived
   *   TCKEYCONF   TRANSIDAI
   * 1)   -           -
   * 2)   -           X
   * 3)   X           -
   * 4)   X           X
   */
  NdbOperation* tmp = theFirstExecOpInList;
  const Uint32 len = TcKeyConf::DirtyReadBit | id;
  Uint32 tNoComp = theNoOfOpCompleted;
  Uint32 tNoSent = theNoOfOpSent;
  Uint32 count = 0;
  while(tmp != nullptr)
  {
    if(tmp->theReceiver.m_expected_result_length == len && 
       tmp->theReceiver.m_received_result_length == 0)
    {
      count++;
      tmp->theError.code = 4119;
    }
    tmp = tmp->next();
  }

  /**
   * TODO, only abort ones really needing abort
   */
  NdbQueryImpl* qtmp = m_firstActiveQuery;
  while (qtmp != nullptr)
  {
    if (qtmp->getQueryDef().isScanQuery() == false)
    {
      count++;
      qtmp->setErrorCode(4119);
    }
    qtmp = qtmp->getNext();
  }

  tNoComp += count;
  theNoOfOpCompleted = tNoComp;
  if(count)
  {
    theReturnStatus = NdbTransaction::ReturnFailure;
    if(tNoComp == tNoSent)
    {
      theError.code = 4119;
      theCompletionStatus = NdbTransaction::CompletedFailure;    
      return 1;
    }
  }
  return 0;
}

NdbQuery*
NdbTransaction::createQuery(const NdbQueryDef* def,
                            const NdbQueryParamValue paramValues[],
                            NdbOperation::LockMode)
{
  NdbQueryImpl* query = NdbQueryImpl::buildQuery(*this, def->getImpl());
  if (unlikely(query == nullptr)) {
    return nullptr; // Error code for transaction is already set.
  }

  const int error = query->assignParameters(paramValues);
  if (unlikely(error)) {
    // Error code for transaction is already set.
    query->release();
    return nullptr;
  }

  query->setNext(m_firstQuery);
  m_firstQuery = query;

  return &query->getInterface();
}

NdbLockHandle*
NdbTransaction::getLockHandle()
{
  NdbLockHandle* lh;

  /* Get a LockHandle object from the Ndb pool and
   * link it into our transaction
   */
  lh = theNdb->getLockHandle();

  if (lh)
  {
    lh->thePrev = m_theLastLockHandle;
    if (m_theLastLockHandle == nullptr)
    {
      m_theFirstLockHandle = lh;
      m_theLastLockHandle = lh;
    }
    else
    {
      lh->next(nullptr);
      m_theLastLockHandle->next(lh);
      m_theLastLockHandle = lh;
    }
  }

  return lh;
}

const NdbOperation*
NdbTransaction::unlock(const NdbLockHandle* lockHandle,
                       NdbOperation::AbortOption ao)
{
  switch(lockHandle->m_state)
  {
  case NdbLockHandle::FREE:
    /* LockHandle already released */
    setErrorCode(4551);
    return nullptr;
  case NdbLockHandle::PREPARED:
    if (likely(lockHandle->isLockRefValid()))
    {
      /* Looks ok */
      break;
    }
    [[fallthrough]];
  case NdbLockHandle::ALLOCATED:
    /* NdbLockHandle original operation not executed successfully */
    setErrorCode(4553);
    return nullptr;
  default:
    abort();
    return nullptr;
  }

  if (m_theFirstLockHandle == nullptr)
  {
    /* NdbLockHandle does not belong to transaction */
    setErrorCode(4552);
    return nullptr;
  }

#ifdef VM_TRACE
  /* Check that this transaction 'owns' this lockhandle */
  {
    NdbLockHandle* tmp = m_theLastLockHandle;
    while (tmp && (tmp != lockHandle))
    {
      tmp = tmp->thePrev;
    }
    
    if (tmp != lockHandle)
    {
      /* NdbLockHandle does not belong to transaction */
      setErrorCode(4552);
      return nullptr;
    }
  }
#endif

  assert(theSimpleState == 0);

  /* Use the first work of the Lock reference as the unlock
   * operation's partition id
   * The other two words form the key.
   */
  NdbOperation::OperationOptions opts;

  opts.optionsPresent = NdbOperation::OperationOptions::OO_PARTITION_ID;
  opts.partitionId = lockHandle->getDistKey();

  if (ao != NdbOperation::DefaultAbortOption)
  {
    /* User supplied a preference, pass it on */
    opts.optionsPresent |= NdbOperation::OperationOptions::OO_ABORTOPTION;
    opts.abortOption = ao;
  }

  NdbOperation* unlockOp = setupRecordOp(NdbOperation::UnlockRequest,
                                         NdbOperation::LM_CommittedRead,
                                         NdbOperation::AbortOnError, // Default
                                         lockHandle->m_table->m_ndbrecord,
                                         nullptr, // key_row
                                         lockHandle->m_table->m_ndbrecord,
                                         nullptr,             // attr_row
                                         nullptr,             // mask
                                         &opts,            // opts,
                                         sizeof(opts),     // sizeOfOptions
                                         lockHandle);
  
  return unlockOp;
}

int
NdbTransaction::releaseLockHandle(const NdbLockHandle* lockHandle)
{
  NdbLockHandle* prev = lockHandle->thePrev;
  NdbLockHandle* next = lockHandle->theNext;

  switch(lockHandle->m_state)
  {
  case NdbLockHandle::FREE:
    /* NdbLockHandle already released */
    setErrorCode(4551);
    return -1;
  case NdbLockHandle::PREPARED:
    if (! lockHandle->isLockRefValid())
    {
      /* It's not safe to release the lockHandle after it's
       * defined and before the operation's executed.
       * The lockhandle memory is needed to receive the
       * Lock Reference during execution
       */
      /* Cannot releaseLockHandle until operation executed */
      setErrorCode(4550);
      return -1;
    }
    [[fallthrough]];
  case NdbLockHandle::ALLOCATED:
    /* Ok to release */
    break;
  default:
    /* Bad state */
    abort();
    return -1;
  }

#ifdef VM_TRACE
  /* Check lockhandle is known to this transaction */
  NdbLockHandle* tmp = m_theFirstLockHandle;
  while (tmp &&
         (tmp != lockHandle))
  {
    tmp = tmp->next();
  }

  if (tmp != lockHandle)
  {
    abort();
    return -1;
  }
#endif

  /* Repair list around lock handle */
  if (prev)
    prev->next(next);
  
  if (next)
    next->thePrev = prev;
  
  /* Repair list head and tail ptrs */
  if (lockHandle == m_theFirstLockHandle)
  {
    m_theFirstLockHandle = next;
  }
  if (lockHandle == m_theLastLockHandle)
  {
    m_theLastLockHandle = prev;
  }
  
  /* Now return it to the Ndb's freelist */
  NdbLockHandle* lh = const_cast<NdbLockHandle*>(lockHandle);

  lh->thePrev = nullptr;
  lh->theNext = nullptr;
  
  theNdb->releaseLockHandle(lh);

  return 0;
}
