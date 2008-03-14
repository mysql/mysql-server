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

#ifndef NdbTransaction_H
#define NdbTransaction_H

#include <ndb_types.h>
#include "NdbError.hpp"
#include "NdbDictionary.hpp"
#include "Ndb.hpp"
#include "NdbOperation.hpp"
#include <NdbIndexScanOperation.hpp>

class NdbTransaction;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbIndexOperation;
class NdbApiSignal;
class Ndb;
class NdbBlob;
class NdbInterpretedCode;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
// to be documented later
/**
 * NdbAsynchCallback functions are used when executing asynchronous 
 * transactions (using NdbTransaction::executeAsynchPrepare, or 
 * NdbTransaction::executeAsynch).
 * The functions are called when the execute has finished.
 * See @ref secAsync for more information.
 */
typedef void (* NdbAsynchCallback)(int, NdbTransaction*, void*);
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
enum AbortOption {
  DefaultAbortOption = NdbOperation::DefaultAbortOption,
  CommitIfFailFree = NdbOperation::AbortOnError,         
  TryCommit = NdbOperation::AbortOnError,
  AbortOnError= NdbOperation::AbortOnError,
  CommitAsMuchAsPossible = NdbOperation::AO_IgnoreError,
  AO_IgnoreError= NdbOperation::AO_IgnoreError
};
enum ExecType { 
  NoExecTypeDef = -1,
  Prepare,
  NoCommit,
  Commit,
  Rollback
};
#endif

/**
 * @class NdbTransaction
 * @brief Represents a transaction.
 *
 * A transaction (represented by an NdbTransaction object) 
 * belongs to an Ndb object and is created using 
 * Ndb::startTransaction().
 * A transaction consists of a list of operations 
 * (represented by NdbOperation, NdbScanOperation, NdbIndexOperation,
 *  and NdbIndexScanOperation objects). 
 * Each operation access exactly one table.
 *
 * After getting the NdbTransaction object, 
 * the first step is to get (allocate) an operation given the table name using
 * one of the methods getNdbOperation(), getNdbScanOperation(),
 * getNdbIndexOperation(), or getNdbIndexScanOperation().
 * Then the operation is defined. 
 * Several operations can be defined on the same 
 * NdbTransaction object, they will in that case be executed in parallell.
 * When all operations are defined, the execute()
 * method sends them to the NDB kernel for execution.
 *
 * The execute() method returns when the NDB kernel has 
 * completed execution of all operations defined before the call to 
 * execute(). All allocated operations should be properly defined 
 * before calling execute().
 *
 * A call to execute() uses one out of three types of execution:
 *  -# NdbTransaction::NoCommit  Executes operations without committing them.
 *  -# NdbTransaction::Commit    Executes remaining operation and commits the 
 *        	           complete transaction
 *  -# NdbTransaction::Rollback  Rollbacks the entire transaction.
 *
 * execute() is equipped with an extra error handling parameter. 
 * There are two alternatives:
 * -# NdbTransaction::AbortOnError (default).
 *    The transaction is aborted if there are any error during the
 *    execution
 * -# NdbTransaction::AO_IgnoreError
 *    Continue execution of transaction even if operation fails
 *
 */

/* FUTURE IMPLEMENTATION:
 * Later a prepare mode will be added when Ndb supports Prepare-To-Commit
 * The NdbTransaction can deliver the Transaction Id of the transaction.
 * After committing a transaction it is also possible to retrieve the 
 * global transaction checkpoint which the transaction was put in.
 *
 * FUTURE IMPLEMENTATION:
 * There are three methods for acquiring the NdbOperation. 
 * -# The first method is the normal where a table name is
 *    provided. In this case the primary key must be supplied through
 *    the use of the NdbOperation::equal methods on the NdbOperation object.
 * -# The second method provides the tuple identity of the tuple to be
 *    read.  The tuple identity contains a table identifier and will
 *    thus be possible to use to ensure the attribute names provided
 *    are correct.  If an object-oriented layer is put on top of NDB
 *    Cluster it is essential that all tables derived from a base
 *    class has the same attributes with the same type and the same
 *    name. Thus the application can use the tuple identity and need
 *    not known the table of the tuple.  As long as the table is
 *    derived from the known base class everything is ok.
 *    It is not possible to provide any primary key since it is 
 *    already supplied with the call to NdbTransaction::getNdbOperation. 
 * -# The third method is used when a scanned tuple is to be transferred to 
 *    another transaction. In this case it is not possible to define the 
 *    primary key since it came along from the scanned tuple.
 *
 */

class NdbRecord;

class NdbTransaction
{
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexOperation;
  friend class NdbIndexScanOperation;
  friend class NdbBlob;
  friend class ha_ndbcluster;
#endif

public:
#ifdef NDBAPI_50_COMPAT
  enum AbortOption {
    DefaultAbortOption = NdbOperation::DefaultAbortOption,
    CommitIfFailFree = NdbOperation::AbortOnError,         
    TryCommit = NdbOperation::AbortOnError,
    AbortOnError= NdbOperation::AbortOnError,
    CommitAsMuchAsPossible = NdbOperation::AO_IgnoreError,
    AO_IgnoreError= NdbOperation::AO_IgnoreError
  };
#endif


  /**
   * Execution type of transaction
   */
  enum ExecType {
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    NoExecTypeDef=
    ::NoExecTypeDef,            ///< Erroneous type (Used for debugging only)
    Prepare= ::Prepare,         ///< <i>Missing explanation</i>
#endif
    NoCommit=                   ///< Execute the transaction as far as it has
                                ///< been defined, but do not yet commit it
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    ::NoCommit
#endif
    ,Commit=                    ///< Execute and try to commit the transaction
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    ::Commit
#endif
    ,Rollback                   ///< Rollback transaction
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    = ::Rollback
#endif
  };

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Convenience method to fetch this transaction's Ndb* object 
   */
  Ndb * getNdb() { 
    return theNdb; 
  }
#endif

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Get an NdbOperation for a table.
   * Note that the operation has to be defined before it is executed.
   *
   * @note All operations within the same transaction need to 
   *       be initialized with this method.
   * 
   * @param  aTableName   The table name.
   * @return  Pointer to an NdbOperation object if successful, otherwise NULL.
   */
  NdbOperation* getNdbOperation(const char* aTableName);
#endif

  /**
   * Get an NdbOperation for a table.
   * Note that the operation has to be defined before it is executed.
   *
   * @note All operations within the same transaction need to 
   *       be initialized with this method.
   * 
   * @param  aTable  
   *         A table object (fetched by NdbDictionary::Dictionary::getTable)
   * @return  Pointer to an NdbOperation object if successful, otherwise NULL.
   */
  NdbOperation* getNdbOperation(const NdbDictionary::Table * aTable);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Get an operation from NdbScanOperation idlelist and 
   * get the NdbTransaction object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  aTableName  The table name.
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbScanOperation* getNdbScanOperation(const char* aTableName);
#endif

  /**
   * Get an operation from NdbScanOperation idlelist and 
   * get the NdbTransaction object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  aTable  
   *         A table object (fetched by NdbDictionary::Dictionary::getTable)
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbScanOperation* getNdbScanOperation(const NdbDictionary::Table * aTable);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Get an operation from NdbIndexScanOperation idlelist and 
   * get the NdbTransaction object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  anIndexName  The name of the index to use for scanning
   * @param  aTableName  The name of the table to scan
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbIndexScanOperation* getNdbIndexScanOperation(const char* anIndexName,
						  const char* aTableName);
  NdbIndexScanOperation* getNdbIndexScanOperation
  (const NdbDictionary::Index *anIndex, const NdbDictionary::Table *aTable);
#endif
  
  /**
   * Get an operation from NdbIndexScanOperation idlelist and 
   * get the NdbTransaction object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  anIndex  
             An index object (fetched by NdbDictionary::Dictionary::getIndex).
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbIndexScanOperation* getNdbIndexScanOperation
  (const NdbDictionary::Index *anIndex);
  
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Get an operation from NdbIndexOperation idlelist and 
   * get the NdbTransaction object that
   * was fetched by startTransaction pointing to this operation.
   *
   * @param   anIndexName   The index name (as created by createIndex).
   * @param   aTableName    The table name.
   * @return                Pointer to an NdbIndexOperation object if 
   *                        successful, otherwise NULL
   */
  NdbIndexOperation* getNdbIndexOperation(const char*  anIndexName,
                                          const char*  aTableName);
  NdbIndexOperation* getNdbIndexOperation(const NdbDictionary::Index *anIndex,
					  const NdbDictionary::Table *aTable);
#endif

  /**
   * Get an operation from NdbIndexOperation idlelist and 
   * get the NdbTransaction object that
   * was fetched by startTransaction pointing to this operation.
   *
   * @param   anIndex
   *          An index object (fetched by NdbDictionary::Dictionary::getIndex).
   * @return              Pointer to an NdbIndexOperation object if 
   *                      successful, otherwise NULL
   */
  NdbIndexOperation* getNdbIndexOperation(const NdbDictionary::Index *anIndex);

  /** 
   * @name Execute Transaction
   * @{
   */

  /**
   * Executes transaction.
   *
   * @param execType     Execution type:<br>
   *                     ExecType::NoCommit executes operations without 
   *                                        committing them.<br>
   *                     ExecType::Commit  executes remaining operations and 
   *                                       commits the complete transaction.<br>
   *                     ExecType::Rollback rollbacks the entire transaction.
   * @param abortOption  Handling of error while excuting
   *                     AbortOnError - Abort transaction if an operation fail
   *                     AO_IgnoreError  - Accept failing operations
   *                     DefaultAbortOption - Use per-operation abort option
   * @param force        When operations should be sent to NDB Kernel.
   *                     (See @ref secAdapt.)
   *                     - 0: non-force, adaptive algorithm notices it 
   *                          (default); 
   *                     - 1: force send, adaptive algorithm notices it; 
   *                     - 2: non-force, adaptive algorithm do not notice 
   *                          the send.
   * @return 0 if successful otherwise -1.
   */
#ifndef NDBAPI_50_COMPAT
  int execute(ExecType execType,
	      NdbOperation::AbortOption = NdbOperation::DefaultAbortOption,
	      int force = 0 );
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int execute(::ExecType execType,
	      ::AbortOption abortOption = ::DefaultAbortOption,
	      int force = 0 ) {
    return execute ((ExecType)execType,
		    (NdbOperation::AbortOption)abortOption,
		    force); }
#endif
#else
  /**
   * 50 compability layer
   *   Check 50-docs for sematics
   */

  int execute(ExecType execType, NdbOperation::AbortOption, int force);
  
  int execute(NdbTransaction::ExecType execType,
              NdbTransaction::AbortOption abortOption = AbortOnError,
              int force = 0) 
    {
      int ret = execute ((ExecType)execType,
                         (NdbOperation::AbortOption)abortOption,
                         force); 
      if (ret || (abortOption != AO_IgnoreError && theError.code))
        return -1;
      return 0;
    }
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  // to be documented later
  /**
   * Prepare an asynchronous transaction.
   *
   * See @ref secAsync for more information on
   * how to use this method.
   *
   * @param execType   Execution type:<br>
   *        ExecType::NoCommit executes operations without committing them.<br>
   *        ExecType::Commit   executes remaining operations and commits the 
   *                           complete transaction.<br>
   *        ExecType::Rollback rollbacks the entire transaction.
   * @param callback       A callback method.  This method gets 
   *                        called when the transaction has been 
   *                        executed.  See @ref ndbapi_async1.cpp 
   *                        for an example on how to specify and use 
   *                        a callback method.
   * @param anyObject       A void pointer.  This pointer is forwarded to the 
   *                        callback method and can be used to give 
   *                        the callback method some data to work on.
   *                        It is up to the application programmer 
   *                        to decide on the use of this pointer.
   * @param abortOption     see @ref execute
   */
#ifndef NDBAPI_50_COMPAT
  void executeAsynchPrepare(ExecType          execType,
			    NdbAsynchCallback callback,
			    void*             anyObject,
			    NdbOperation::AbortOption = NdbOperation::DefaultAbortOption);
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  void executeAsynchPrepare(::ExecType       execType,
			    NdbAsynchCallback callback,
			    void*             anyObject,
			    ::AbortOption ao = ::DefaultAbortOption) {
    executeAsynchPrepare((ExecType)execType, callback, anyObject,
			 (NdbOperation::AbortOption)ao); }
#endif
#else
  /**
   * 50 compability layer
   *   Check 50-docs for sematics
   */
  void executeAsynchPrepare(ExecType          execType,
			    NdbAsynchCallback callback,
			    void*             anyObject,
			    NdbOperation::AbortOption);

  void executeAsynchPrepare(NdbTransaction::ExecType execType,
			    NdbAsynchCallback callback,
			    void *anyObject,
			    NdbTransaction::AbortOption abortOption = NdbTransaction::AbortOnError)
    {
      executeAsynchPrepare((ExecType)execType, callback, anyObject,
                           (NdbOperation::AbortOption)abortOption);
    }
#endif

  /**
   * Prepare and send an asynchronous transaction.
   *
   * This method perform the same action as 
   * NdbTransaction::executeAsynchPrepare
   * but also sends the operations to the NDB kernel.
   *
   * See NdbTransaction::executeAsynchPrepare for information
   * about the parameters of this method.
   *
   * See @ref secAsync for more information on
   * how to use this method.
   */
#ifndef NDBAPI_50_COMPAT
  void executeAsynch(ExecType            aTypeOfExec,
		     NdbAsynchCallback   aCallback,
		     void*               anyObject,
		     NdbOperation::AbortOption = NdbOperation::DefaultAbortOption,
                     int forceSend= 0);
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  void executeAsynch(::ExecType         aTypeOfExec,
		     NdbAsynchCallback   aCallback,
		     void*               anyObject,
		     ::AbortOption abortOption= ::DefaultAbortOption,
                     int forceSend= 0)
  { executeAsynch((ExecType)aTypeOfExec, aCallback, anyObject,
		  (NdbOperation::AbortOption)abortOption, forceSend); }
#endif
#else
  /**
   * 50 compability layer
   *   Check 50-docs for sematics
   */
  void executeAsynch(ExecType            aTypeOfExec,
		     NdbAsynchCallback   aCallback,
		     void*               anyObject,
		     NdbOperation::AbortOption = NdbOperation::DefaultAbortOption,
                     int forceSend= 0);
  void executeAsynch(NdbTransaction::ExecType aTypeOfExec,
		     NdbAsynchCallback aCallback,
		     void* anyObject,
		     NdbTransaction::AbortOption abortOption = AbortOnError)
    {
      executeAsynch((ExecType)aTypeOfExec, aCallback, anyObject,
                    (NdbOperation::AbortOption)abortOption, 0);
    }
#endif
#endif

  /**
   * Refresh
   * Update timeout counter of this transaction 
   * in the database. If you want to keep the transaction 
   * active in the database longer than the
   * transaction abort timeout.
   * @note It's not advised to take a lock on a record and keep it
   *       for a extended time since this can impact other transactions.
   *
   */
  int refresh();

  /**
   * Close transaction
   *
   * @note Equivalent to to calling Ndb::closeTransaction()
   */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * @note It is not allowed to call NdbTransaction::close after sending the
   *       transaction asynchronously before the callback method has 
   * 	   been called.
   *       (The application should keep track of the number of 
   *       outstanding transactions and wait until all of them 
   *       has completed before calling NdbTransaction::close).
   *       If the transaction is not committed it will be aborted.
   */
#endif
  void close();

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Restart transaction
   *
   *   Once a transaction has been completed successfully
   *     it can be started again wo/ calling closeTransaction/startTransaction
   *
   *  @note This method also releases completed operations
   *
   *  @note This method does not close open scans, 
   *        c.f. NdbScanOperation::close()
   *
   *  @note This method can only be called _directly_ after commit
   *        and only if commit is successful
   */
  int restart();
#endif

  /** @} *********************************************************************/

  /** 
   * @name Meta Information
   * @{
   */

  /**
   * Get global checkpoint identity (GCI) of transaction.
   *
   * Each committed transaction belong to a GCI.  
   * The log for the committed transaction is saved on 
   * disk when a global checkpoint occurs.
   * 
   * Whether or not the global checkpoint with this GCI has been 
   * saved on disk or not cannot be determined by this method.
   *
   * By comparing the GCI of a transaction with the value 
   * last GCI restored in a restarted NDB Cluster one can determine
   * whether the transaction was restored or not.
   *
   * @note Global Checkpoint Identity is undefined for scan transactions 
   *       (This is because no updates are performed in scan transactions.)
   *
   * @return 0 if GCI is available, and stored in <em>gciptr</em>
             -1 if GCI is not available.
   *         (Note that there has to be an NdbTransaction::execute call 
   *         with Ndb::Commit for the GCI to be available.)
   */
  int getGCI(Uint64 * gciptr);

  /**
   * Deprecated...in favor of getGCI(Uint64*)
   */
  int getGCI();
			
  /**
   * Get transaction identity.
   *
   * @return  Transaction id.
   */
  Uint64	getTransactionId();

  /**
   * The commit status of the transaction.
   */
  enum CommitStatusType { 
    NotStarted,                   ///< Transaction not yet started
    Started,                      ///< <i>Missing explanation</i>
    Committed,                    ///< Transaction has been committed
    Aborted,                      ///< Transaction has been aborted
    NeedAbort                     ///< <i>Missing explanation</i>
  };

  /**
   * Get the commit status of the transaction.
   *
   * @return  The commit status of the transaction
   */
  CommitStatusType commitStatus();

  /** @} *********************************************************************/

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get error object with information about the latest error.
   *
   * @return An error object with information about the latest error.
   */
  const NdbError & getNdbError() const;

  /**
   * Get the latest NdbOperation which had an error. 
   * This method is used on the NdbTransaction object to find the
   * NdbOperation causing an error.  
   * To find more information about the
   * actual error, use method NdbOperation::getNdbError()
   * on the returned NdbOperation object.
   *
   * @return The NdbOperation causing the latest error.
   */
  NdbOperation*	getNdbErrorOperation();

  /** 
   * Get the method number where the latest error occured.
   * 
   * @return Line number where latest error occured.
   */
  int getNdbErrorLine();

  /**
   * Get completed (i.e. executed) operations of a transaction
   *
   * This method should only be used <em>after</em> a transaction 
   * has been executed.  
   * - NdbTransaction::getNextCompletedOperation(NULL) returns the
   *   first NdbOperation object.
   * - NdbTransaction::getNextCompletedOperation(op) returns the
   *   NdbOperation object defined after the NdbOperation "op".
   * 
   * This method is typically used to fetch all NdbOperation:s of 
   * a transaction to check for errors (use NdbOperation::getNdbError 
   * to fetch the NdbError object of an NdbOperation).
   * 
   * @note This method should only be used after the transaction has been 
   *       executed and before the transaction has been closed.
   * 
   * @param   op Operation, NULL means get first operation
   * @return  Operation "after" op
   */
  const NdbOperation * getNextCompletedOperation(const NdbOperation * op)const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  const NdbOperation* getFirstDefinedOperation()const{return theFirstOpInList;}
  const NdbOperation* getLastDefinedOperation()const{return theLastOpInList;}

  /** @} *********************************************************************/

  /**
   * Execute the transaction in NoCommit mode if there are any not-yet
   * executed blob part operations of given types.  Otherwise do
   * nothing.  The flags argument is bitwise OR of (1 << optype) where
   * optype comes from NdbOperation::OperationType.  Only the basic PK
   * ops are used (read, insert, update, delete).
   */
  int executePendingBlobOps(Uint8 flags = 0xFF);

  /**
   * Get nodeId of TC for this transaction
   */
  Uint32 getConnectedNodeId(); // Get Connected node id
#endif

  /*
    NdbRecord primary key and unique key operations.

    If the key_rec passed in is for a table, the operation will be a primary
    key operation. If it is for an index, it will be a unique key operation
    using that index.

    The key_row passed in defines the primary or unique key of the affected
    tuple, and must remain valid until execute() is called. The key_rec must
    include all columns of the key.

    The mask, if != NULL, defines a subset of attributes to read, update, or
    insert. Only if (mask[attrId >> 3] & (1<<(attrId & 7))) is set is the
    column affected. The mask is copied by the methods, so need not remain
    valid after the call returns.

    For unique index operations, the attr_rec must refer to the underlying
    table of the index.
  */

  NdbOperation *readTuple(const NdbRecord *key_rec, const char *key_row,
                          const NdbRecord *result_rec, char *result_row,
                          NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
                          const unsigned char *result_mask= 0);
  NdbOperation *insertTuple(const NdbRecord *rec, const char *row,
                            const unsigned char *mask= 0);
  NdbOperation *updateTuple(const NdbRecord *key_rec, const char *key_row,
                            const NdbRecord *attr_rec, const char *attr_row,
                            const unsigned char *mask= 0,
                            const Uint32 *setPartitionId = 0,
                            const void *getSetValue = 0,
                            const NdbInterpretedCode *interpreted_code = 0);
  NdbOperation *writeTuple(const NdbRecord *key_rec, const char *key_row,
                           const NdbRecord *attr_rec, const char *attr_row,
                           const unsigned char *mask= 0);
  NdbOperation *deleteTuple(const NdbRecord *key_rec, const char *key_row);

  /*
    Scan a table, using NdbRecord to read out column data.

    The result_record pointer must remain valid until after the call to
    execute().

    The result_mask pointer is optional, if present only columns for
    which the corresponding bit (by attribute id order) in result_mask
    is set will be retrieved in the scan. The result_mask is copied
    internally, so in contrast to result_record need not be valid at
    execute().

    The parallel argument is the desired parallelism, or 0 for maximum
    parallelism (receiving rows from all fragments in parallel).
  */
  NdbScanOperation *
  scanTable(const NdbRecord *result_record,
            NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
            const unsigned char *result_mask= 0,
            Uint32 scan_flags= 0,
            Uint32 parallel= 0,
            Uint32 batch= 0);

//private:
  /*
    Do an index range scan (optionally ordered) of a table.

    The key_record describes the index to be scanned. It must be a key record
    for the index, ie. it must specify (at least) all the key columns of the
    index. And it must be created from the index to be scanned (not from the
    underlying table).

    The result_record describes the rows to be returned from the scan. For an
    ordered index scan, result_record must be a key record for the index to
    be scanned, that is it must include at least all of the column in the
    index (the reason is that the index key is needed for merge sorting the
    scans returned from each fragment).

    The call uses a callback function as a flexible way of specifying multiple
    range bounds. The callback will be called once for each bound to define
    lower and upper key value etc.

    The callback received a private callback_data void *, and the index of the
    bound (0 .. num_key_bounds). However, it is guaranteed that it will be
    called in ordered sequence, so it is permissible to ignore the passed
    bound_index and just return the values for the next bound (for example
    if data is kept in a linked list).

    Note that for multi-range, the IndexBound::low_key and IndexBound::high_key
    pointers must be unique, ie. it is not permissible to re-use the same row
    buffer for several different range bounds within a single scan. It is
    however permissible to use the same row pointer as low_key and high_key (to
    specify an equals bound), and it is also permissible to re-use the rows
    after the scanIndex() method returns (ie. they need not remain valid until
    ececute() time, like the NdbRecord pointers do).

    The callback can return 0 to denote success, and -1 to denote error (the
    latter causing the creation of the NdbIndexScanOperation to fail).

    This multi-range method is only for use in mysqld code.
  */
  NdbIndexScanOperation *
  scanIndex(const NdbRecord *key_record,
            int (*get_bound_callback)(void *callback_data,
                                      Uint32 bound_index,
                                      NdbIndexScanOperation::IndexBound & bound),
            void *callback_data,
            Uint32 num_key_bounds,
            const NdbRecord *result_record,
            NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
            const unsigned char *result_mask= 0,
            Uint32 scan_flags= 0,
            Uint32 parallel= 0,
            Uint32 batch= 0);

public:

  /* A convenience wrapper for simpler specification of a single bound. */
  NdbIndexScanOperation *
  scanIndex(const NdbRecord *key_record,
            const char *low_key,
            Uint32 low_key_count,
            bool low_inclusive,
            const char * high_key,
            Uint32 high_key_count,
            bool high_inclusive,
            const NdbRecord *result_record,
            NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
            const unsigned char *result_mask= 0,
            Uint32 scan_flags= 0,
            Uint32 parallel= 0,
            Uint32 batch= 0);

private:						
  /**
   * Release completed operations
   */
  void releaseCompletedOperations();

  typedef Uint64 TimeMillis_t;
  /**************************************************************************
   *	These methods are service methods to other classes in the NDBAPI.   *
   **************************************************************************/
 
  /**************************************************************************
   *	These are the create and delete methods of this class.              *
   **************************************************************************/
  NdbTransaction(Ndb* aNdb); 
  ~NdbTransaction();

  int init();           // Initialize connection object for new transaction

  int executeNoBlobs(ExecType execType, 
	             NdbOperation::AbortOption = NdbOperation::DefaultAbortOption,
	             int force = 0 );
  
  /**
   * Set Connected node id 
   * and sequence no
   */
  void setConnectedNodeId( Uint32 nodeId, Uint32 sequence); 

  void		setMyBlockReference( int );	  // Set my block refrerence
  void		setTC_ConnectPtr( Uint32 );	  // Sets TC Connect pointer
  int		getTC_ConnectPtr();		  // Gets TC Connect pointer
  void          setBuddyConPtr(Uint32);           // Sets Buddy Con Ptr
  Uint32        getBuddyConPtr();                 // Gets Buddy Con Ptr
  NdbTransaction* next();			  // Returns the next pointer
  void		next(NdbTransaction*);		  // Sets the next pointer

  enum ConStatusType { 
    NotConnected,
    Connecting,
    Connected,
    DisConnecting,
    ConnectFailure
  };
  ConStatusType Status();                 // Read the status information
  void		Status(ConStatusType);	  // Set the status information

  Uint32        get_send_size();                  // Get size to send
  void          set_send_size(Uint32);            // Set size to send;
  
  int  receiveDIHNDBTAMPER(NdbApiSignal* anApiSignal);
  int  receiveTCSEIZECONF(NdbApiSignal* anApiSignal); 
  int  receiveTCSEIZEREF(NdbApiSignal* anApiSignal);	
  int  receiveTCRELEASECONF(NdbApiSignal* anApiSignal);	
  int  receiveTCRELEASEREF(NdbApiSignal* anApiSignal);	
  int  receiveTC_COMMITCONF(const class TcCommitConf *, Uint32 len);
  int  receiveTCKEYCONF(const class TcKeyConf *, Uint32 aDataLength);
  int  receiveTCKEY_FAILCONF(const class TcKeyFailConf *);
  int  receiveTCKEY_FAILREF(NdbApiSignal* anApiSignal);
  int  receiveTC_COMMITREF(NdbApiSignal* anApiSignal);		    	
  int  receiveTCROLLBACKCONF(NdbApiSignal* anApiSignal); // Rec TCPREPARECONF ?
  int  receiveTCROLLBACKREF(NdbApiSignal* anApiSignal);  // Rec TCPREPAREREF ?
  int  receiveTCROLLBACKREP(NdbApiSignal* anApiSignal);
  int  receiveTCINDXCONF(const class TcIndxConf *, Uint32 aDataLength);
  int  receiveTCINDXREF(NdbApiSignal*);
  int  receiveSCAN_TABREF(NdbApiSignal*);
  int  receiveSCAN_TABCONF(NdbApiSignal*, const Uint32*, Uint32 len);

  int 	doSend();	                // Send all operations
  int 	sendROLLBACK();	                // Send of an ROLLBACK
  int 	sendTC_HBREP();                 // Send a TCHBREP signal;
  int 	sendCOMMIT();                   // Send a TC_COMMITREQ signal;
  void	setGCI(int GCI);		// Set the global checkpoint identity
 
  int	OpCompleteFailure(NdbOperation*);
  int	OpCompleteSuccess();
  void	CompletedOperations();	        // Move active ops to list of completed
 
  void	OpSent();			// Operation Sent with success
  
  // Free connection related resources and close transaction
  void		release();              

  // Release all operations in connection
  void		releaseOperations();	

  // Release all cursor operations in connection
  void releaseOps(NdbOperation*);	
  void releaseScanOperations(NdbIndexScanOperation*);	
  bool releaseScanOperation(NdbIndexScanOperation** listhead,
			    NdbIndexScanOperation** listtail,
			    NdbIndexScanOperation* op);
  void releaseExecutedScanOperation(NdbIndexScanOperation*);
  
  // Set the transaction identity of the transaction
  void		setTransactionId(Uint64 aTransactionId);

  // Indicate something went wrong in the definition phase
  void		setErrorCode(int anErrorCode);		

  // Indicate something went wrong in the definition phase
  void		setOperationErrorCode(int anErrorCode);	

  // Indicate something went wrong in the definition phase
  void		setOperationErrorCodeAbort(int anErrorCode, int abortOption = -1);

  int		checkMagicNumber();		       // Verify correct object
  NdbOperation* getNdbOperation(const class NdbTableImpl* aTable,
                                NdbOperation* aNextOp = 0,
                                bool useRec= false);

  NdbIndexScanOperation* getNdbScanOperation(const class NdbTableImpl* aTable);
  NdbIndexOperation* getNdbIndexOperation(const class NdbIndexImpl* anIndex, 
                                          const class NdbTableImpl* aTable,
                                          NdbOperation* aNextOp = 0,
                                          bool useRec= false);
  NdbIndexScanOperation* getNdbIndexScanOperation(const NdbIndexImpl* index,
						  const NdbTableImpl* table);
  
  NdbOperation *setupRecordOp(NdbOperation::OperationType type,
                              NdbOperation::LockMode lock_mode,
                              const NdbRecord *key_record,
                              const char *key_row,
                              const NdbRecord *attribute_record,
                              const char *attribute_row,
                              const unsigned char *mask,
                              const Uint32 *setPartitionId = 0,
                              const void *getSetValue = 0,
                              const NdbInterpretedCode *interpreted_code = 0);

  void		handleExecuteCompletion();
  
  /****************************************************************************
   * These are the private variables of this class.
   ****************************************************************************/

  Uint32 ptr2int();
  Uint32 theId;

  // Keeps track of what the send method should do.
  enum SendStatusType { 
    NotInit,  
    InitState,  
    sendOperations,  
    sendCompleted, 
    sendCOMMITstate, 
    sendABORT, 
    sendABORTfail, 
    sendTC_ROLLBACK,  
    sendTC_COMMIT, 
    sendTC_OP       
  };
  SendStatusType theSendStatus; 
  NdbAsynchCallback  theCallbackFunction;    // Pointer to the callback function
  void*              theCallbackObject;      // The callback object pointer
  Uint32             theTransArrayIndex;     // Current index in a transaction 
                                             // array for this object
  TimeMillis_t       theStartTransTime;      // Start time of the transaction

  NdbError theError;	      	// Errorcode on transaction
  int	   theErrorLine;	// Method number of last error in NdbOperation
  NdbOperation*	theErrorOperation; // The NdbOperation where the error occurred

  Ndb* 		theNdb;			     // Pointer to Ndb object	   
  NdbTransaction* theNext;	      	     // Next pointer. Used in idle list.

  NdbOperation*	theFirstOpInList;	    // First operation in defining list.
  NdbOperation*	theLastOpInList;	    // Last operation in defining list.

  NdbOperation*	theFirstExecOpInList;	    // First executing operation in list
  NdbOperation*	theLastExecOpInList;	    // Last executing operation in list.


  NdbOperation*	theCompletedFirstOp;	    // First & last operation in completed 
  NdbOperation*	theCompletedLastOp;         // operation list.

  Uint32	theNoOfOpSent;				// How many operations have been sent	    
  Uint32	theNoOfOpCompleted;			// How many operations have completed
  Uint32	theMyRef;				// Our block reference		
  Uint32	theTCConPtr;				// Transaction Co-ordinator connection pointer.
  Uint64	theTransactionId;			// theTransactionId of the transaction
  Uint64	theGlobalCheckpointId;			// The gloabl checkpoint identity of the transaction
  Uint64 *p_latest_trans_gci;                           // Reference to latest gci for connection
  ConStatusType	theStatus;				// The status of the connection		
  enum CompletionStatus { 
    NotCompleted,
    CompletedSuccess,
    CompletedFailure,
    DefinitionFailure
  } theCompletionStatus;	  // The Completion status of the transaction
  CommitStatusType theCommitStatus;			// The commit status of the transaction
  Uint32	theMagicNumber;				// Magic Number to verify correct object

  Uint32	thePriority;				// Transaction Priority

  enum ReturnType {  ReturnSuccess,  ReturnFailure };
  ReturnType    theReturnStatus;			// Did we have any read/update/delete failing
							// to find the tuple.
  bool theTransactionIsStarted; 
  bool theInUseState;
  bool theSimpleState;

  enum ListState {  
    NotInList, 
    InPreparedList, 
    InSendList, 
    InCompletedList 
  } theListState;

  Uint32 theDBnode;       // The database node we are connected to  
  Uint32 theNodeSequence; // The sequence no of the db node
  bool theReleaseOnClose;

  /**
   * handle transaction spanning
   *   multiple TC/db nodes
   *
   * 1) Bitmask with used nodes
   * 2) Bitmask with nodes failed during op
   */
  Uint32 m_db_nodes[2];
  Uint32 m_failed_db_nodes[2];
  
  int report_node_failure(Uint32 id);

  // Scan operations
  bool m_waitForReply;     
  NdbIndexScanOperation* m_theFirstScanOperation;
  NdbIndexScanOperation* m_theLastScanOperation;
  
  NdbIndexScanOperation* m_firstExecutedScanOp;

  // Scan operations
  // The operation actually performing the scan
  NdbScanOperation* theScanningOp; 
  Uint32 theBuddyConPtr;
  // optim: any blobs
  bool theBlobFlag;
  Uint8 thePendingBlobOps;
  inline bool hasBlobOperation() { return theBlobFlag; }

  static void sendTC_COMMIT_ACK(class TransporterFacade *, NdbApiSignal *,
				Uint32 transId1, Uint32 transId2, 
				Uint32 aBlockRef);

  void completedFail(const char * s);
#ifdef VM_TRACE
  void printState();
#endif
  bool checkState_TransId(const Uint32 * transId) const;

  void remove_list(NdbOperation*& head, NdbOperation*);
  void define_scan_op(NdbIndexScanOperation*);

  friend class HugoOperations;
  friend struct Ndb_free_list_t<NdbTransaction>;
};

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

inline
Uint32
NdbTransaction::get_send_size()
{
  return 0;
}

inline
void
NdbTransaction::set_send_size(Uint32 send_size)
{
  return;
}

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

inline
int
NdbTransaction::checkMagicNumber()
{
  if (theMagicNumber == 0x37412619)
    return 0;
  else {
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
    return -1;
  }
}

inline
bool
NdbTransaction::checkState_TransId(const Uint32 * transId) const {
  const Uint32 tTmp1 = transId[0];
  const Uint32 tTmp2 = transId[1];
  Uint64 tRecTransId = (Uint64)tTmp1 + ((Uint64)tTmp2 << 32);
  bool b = theStatus == Connected && theTransactionId == tRecTransId;
  return b;
}

/************************************************************************************************
void setTransactionId(Uint64 aTransactionId);

Remark:        Set the transaction identity. 
************************************************************************************************/
inline
void
NdbTransaction::setTransactionId(Uint64 aTransactionId)
{
  theTransactionId = aTransactionId;
}

inline
void			
NdbTransaction::setConnectedNodeId(Uint32 aNode, Uint32 aSequenceNo)
{
  theDBnode = aNode;
  theNodeSequence = aSequenceNo;
}	
/******************************************************************************
int getConnectedNodeId();

Return Value:	Return  theDBnode.
Remark:         Get Connected node id. 
******************************************************************************/
inline
Uint32			
NdbTransaction::getConnectedNodeId()
{
  return theDBnode;
}	
/******************************************************************************
void setMyBlockReference(int aBlockRef);

Parameters:     aBlockRef: The block refrerence.
Remark:         Set my block refrerence. 
******************************************************************************/
inline
void			
NdbTransaction::setMyBlockReference(int aBlockRef)	
{
  theMyRef = aBlockRef;
}
/******************************************************************************
void  setTC_ConnectPtr(Uint32 aTCConPtr);

Parameters:     aTCConPtr: The connection pointer.
Remark:         Sets TC Connect pointer. 
******************************************************************************/
inline
void			
NdbTransaction::setTC_ConnectPtr(Uint32 aTCConPtr)
{
  theTCConPtr = aTCConPtr;
}

/******************************************************************************
int  getTC_ConnectPtr();

Return Value:	Return  theTCConPtr.
Remark:         Gets TC Connect pointer. 
******************************************************************************/
inline
int			
NdbTransaction::getTC_ConnectPtr()
{
  return theTCConPtr;
}

inline
void
NdbTransaction::setBuddyConPtr(Uint32 aBuddyConPtr)
{
  theBuddyConPtr = aBuddyConPtr;
}

inline
Uint32 NdbTransaction::getBuddyConPtr()
{
  return theBuddyConPtr;
}

/******************************************************************************
NdbTransaction* next();

inline
void
NdbTransaction::setBuddyConPtr(Uint32 aBuddyConPtr)
{
  theBuddyConPtr = aBuddyConPtr;
}

inline
Uint32 NdbTransaction::getBuddyConPtr()
{
  return theBuddyConPtr;
}

Return Value:	Return  next pointer to NdbTransaction object.
Remark:         Get the next pointer. 
******************************************************************************/
inline
NdbTransaction*
NdbTransaction::next()
{
  return theNext;
}

/******************************************************************************
void next(NdbTransaction aTransaction);

Parameters:     aTransaction: The connection object. 
Remark:         Sets the next pointer. 
******************************************************************************/
inline
void
NdbTransaction::next(NdbTransaction* aTransaction)
{
  theNext = aTransaction;
}

/******************************************************************************
ConStatusType  Status();

Return Value    Return the ConStatusType.	
Parameters:     aStatus:  The status.
Remark:         Sets Connect status. 
******************************************************************************/
inline
NdbTransaction::ConStatusType			
NdbTransaction::Status()
{
  return theStatus;
}

/******************************************************************************
void  Status(ConStatusType aStatus);

Parameters:     aStatus: The status.
Remark:         Sets Connect status. 
******************************************************************************/
inline
void			
NdbTransaction::Status( ConStatusType aStatus )
{
  theStatus = aStatus;
}


/******************************************************************************
void OpSent();

Remark:       An operation was sent with success that expects a response.
******************************************************************************/
inline
void 
NdbTransaction::OpSent()
{
  theNoOfOpSent++;
}

/******************************************************************************
void executePendingBlobOps();
******************************************************************************/
inline
int
NdbTransaction::executePendingBlobOps(Uint8 flags)
{
  if (thePendingBlobOps & flags) {
    // not executeNoBlobs because there can be new ops with blobs
    return execute(NoCommit);
  }
  return 0;
}

inline
Uint32
NdbTransaction::ptr2int(){
  return theId;
}

typedef NdbTransaction NdbConnection;

#endif // ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

#endif
