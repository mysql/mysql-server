/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NdbTransaction_H
#define NdbTransaction_H

#include <ndb_types.h>
#include "NdbError.hpp"
#include "NdbDictionary.hpp"
#include "Ndb.hpp"
#include "NdbOperation.hpp"
#include "NdbIndexScanOperation.hpp"

class NdbTransaction;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbIndexOperation;
class NdbApiSignal;
class Ndb;
class NdbBlob;
class NdbInterpretedCode;
class NdbQueryImpl;
class NdbQueryDef;
class NdbQuery;
class NdbQueryParamValue;
class NdbLockHandle;

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
  friend class NdbQueryImpl;
  friend class NdbQueryOperationImpl;
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
   * @deprecated Use the const NdbOperation returning variant.
   */
  NdbOperation*	getNdbErrorOperation();

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
  const NdbOperation* getNdbErrorOperation() const;

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
   * NdbRecord primary key and unique key operations.
   *
   * If the key_rec passed in is for a table, the operation will be a primary
   * key operation. If it is for an index, it will be a unique key operation
   * using that index.
   *
   * The key_row passed in defines the primary or unique key of the affected
   * tuple, and must remain valid until execute() is called. The key_rec must
   * include all columns of the key.
   *
   * The mask, if != NULL, defines a subset of attributes to read, update, or
   * insert. Only if (mask[attrId >> 3] & (1<<(attrId & 7))) is set is the
   * column affected. The mask is copied by the methods, so need not remain
   * valid after the call returns.
   *
   * For unique index operations, the attr_rec must refer to the underlying
   * table of the index.
   *
   * OperationOptions can be used to give finer-grained control of operation
   * definition.  An OperationOptions structure is passed with flags
   * indicating which operation definition options are present.  Not all
   * operation types support all operation options.  See the definition of
   * the OperationOptions structure for more information on individual options.
   *
   *   Operation type        Supported OperationOptions flags
   *   --------------        --------------------------------
   *   readTuple             OO_ABORTOPTION, OO_GETVALUE,
   *                         OO_PARTITION_ID, OO_INTERPRETED
   *   insertTuple           OO_ABORTOPTION, OO_SETVALUE, 
   *                         OO_PARTITION_ID, OO_ANYVALUE
   *   updateTuple           OO_ABORTOPTION, OO_SETVALUE,
   *                         OO_PARTITION_ID, OO_INTERPRETED,
   *                         OO_ANYVALUE
   *   writeTuple            OO_ABORTOPTION, OO_SETVALUE,
   *                         OO_PARTITION_ID, OO_ANYVALUE
   *   deleteTuple           OO_ABORTOPTION, OO_GETVALUE,
   *                         OO_PARTITION_ID, OO_INTERPRETED,
   *                         OO_ANYVALUE
   *
   * The sizeOfOptions optional parameter is used to allow this interface
   * to be backwards compatible with previous definitions of the OperationOptions
   * structure.  If an unusual size is detected by the interface implementation, 
   * it can use this to determine how to interpret the passed OperationOptions 
   * structure.  To enable this functionality, the caller should pass 
   * sizeof(NdbOperation::OperationOptions) for this argument.
   */
  const NdbOperation *readTuple(const NdbRecord *key_rec, const char *key_row,
                                const NdbRecord *result_rec, char *result_row,
                                NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
                                const unsigned char *result_mask= 0,
                                const NdbOperation::OperationOptions *opts = 0,
                                Uint32 sizeOfOptions = 0);
  const NdbOperation *insertTuple(const NdbRecord *key_rec, const char *key_row,
                                  const NdbRecord *attr_rec, const char *attr_row,
                                  const unsigned char *mask= 0,
                                  const NdbOperation::OperationOptions *opts = 0,
                                  Uint32 sizeOfOptions = 0);
  const NdbOperation *insertTuple(const NdbRecord *combined_rec, const char *combined_row,
                                  const unsigned char *mask = 0,
                                  const NdbOperation::OperationOptions *opts = 0,
                                  Uint32 sizeOfOptions = 0);
  const NdbOperation *updateTuple(const NdbRecord *key_rec, const char *key_row,
                                  const NdbRecord *attr_rec, const char *attr_row,
                                  const unsigned char *mask= 0,
                                  const NdbOperation::OperationOptions *opts = 0,
                                  Uint32 sizeOfOptions = 0);
  const NdbOperation *writeTuple(const NdbRecord *key_rec, const char *key_row,
                                 const NdbRecord *attr_rec, const char *attr_row,
                                 const unsigned char *mask= 0,
                                 const NdbOperation::OperationOptions *opts = 0,
                                 Uint32 sizeOfOptions = 0);
  const NdbOperation *deleteTuple(const NdbRecord *key_rec, const char *key_row,
                                  const NdbRecord *result_rec, char *result_row = 0,
                                  const unsigned char *result_mask = 0,
                                  const NdbOperation::OperationOptions *opts = 0,
                                  Uint32 sizeOfOptions = 0);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  const NdbOperation *refreshTuple(const NdbRecord *key_rec, const char *key_row,
                                   const NdbOperation::OperationOptions *opts = 0,
                                   Uint32 sizeOfOptions = 0);
#endif

  /**
   * Scan a table, using NdbRecord to read out column data.
   *
   * The NdbRecord pointed to by result_record must remain valid until 
   * the scan operation is closed.
   *
   * The result_mask pointer is optional, if present only columns for
   * which the corresponding bit (by attribute id order) in result_mask
   * is set will be retrieved in the scan. The result_mask is copied
   * internally, so in contrast to result_record need not be valid at
   * execute().
   * 
   * A ScanOptions structure can be passed, specifying extra options.  See
   * the definition of the NdbScanOperation::ScanOptions structure for 
   * more information.
   *
   * To enable backwards compatability of this interface, a sizeOfOptions
   * parameter can be passed.  This parameter indicates the size of the
   * ScanOptions structure at the time the client was compiled, and enables
   * detection of the use of an old ScanOptions structure.  If this 
   * functionality is not required, it can be left set to zero.
   */
  NdbScanOperation *
  scanTable(const NdbRecord *result_record,
            NdbOperation::LockMode lock_mode= NdbOperation::LM_Read,
            const unsigned char *result_mask= 0,
            const NdbScanOperation::ScanOptions *options = 0,
            Uint32 sizeOfOptions = 0);

  /**
   * Do an index range scan (optionally ordered) of a table.
   *
   * The key_record describes the index to be scanned. It must be a key record
   * for the index, ie. it must specify (at least) all the key columns of the
   * index. And it must be created from the index to be scanned (not from the
   * underlying table).
   *
   * The result_record describes the rows to be returned from the scan. For an
   * ordered index scan, result_record must be a key record for the index to
   * be scanned, that is it must include at least all of the columns in the
   * index (the reason is that the full index key is needed by NDBAPI for merge 
   * sorting the ordered rows returned from each fragment).  The result_record
   * must be created from the underlying table, not from the index to be scanned.
   *
   * Both the key_record and result_record NdbRecord structures must stay
   * in-place until the scan operation is closed.
   *
   * A single IndexBound can either be specified in this call or in a separate
   * call to NdbIndexScanOperation::setBound().  To perform a multi range read, 
   * the scan_flags in the ScanOptions structure must include SF_MULTIRANGE.  
   * Additional bounds can then be added using multiple calls to 
   * NdbIndexScanOperation::setBound().
   * 
   * To specify an equals bound, use the same row pointer for the low_key and
   * high_key with the low and high inclusive bits set.
   *
   * A ScanOptions structure can be passed, specifying extra options.  See
   * the definition of the ScanOptions structure for more information.
   *
   * To enable backwards compatability of this interface, a sizeOfOptions
   * parameter can be passed.  This parameter indicates the size of the
   * ScanOptions structure at the time the client was compiled, and enables
   * detection of the use of an old ScanOptions structure.  If this functionality
   * is not required, it can be left set to zero.
   * 
   */
  NdbIndexScanOperation *
  scanIndex(const NdbRecord *key_record,
            const NdbRecord *result_record,
            NdbOperation::LockMode lock_mode = NdbOperation::LM_Read,
            const unsigned char *result_mask = 0,
            const NdbIndexScanOperation::IndexBound *bound = 0,
            const NdbScanOperation::ScanOptions *options = 0,
            Uint32 sizeOfOptions = 0);

  /**
   * Add a prepared NdbQueryDef to transaction for execution.
   *
   * If the NdbQueryDef contains parameters,
   * (built with NdbQueryBilder::paramValue()) the value of these
   * parameters are specified in the 'paramValue' array. Parameter values
   * Should be supplied in the same order as the related paramValue's
   * was defined.
   */
  NdbQuery*
  createQuery(const NdbQueryDef* query,
              const NdbQueryParamValue paramValue[]= 0,
              NdbOperation::LockMode lock_mode= NdbOperation::LM_Read);

  /* LockHandle methods */
  /*
   * Shared or Exclusive locks taken by read operations in a transaction
   * are normally held until the transaction commits or aborts.
   * Shared or Exclusive *read* locks can be released before transaction
   * commit or abort time by requesting a LockHandle when defining the
   * read operation.  Any time after the read operation has been executed,
   * the LockHandle can be used to create a new Unlock operation.  When
   * the Unlock operation is executed, the row lock placed by the read
   * operation will be released.
   * 
   * The steps are :
   *  1) Define the primary key read operation in the normal way
   *     with lockmode LM_Read or LM_Exclusive
   *
   *  2) Call NdbOperation::getLockHandle() during operation definition
   *     (Or set the OO_LOCKHANDLE operation option when calling
   *      NdbTransaction::readTuple() for NdbRecord)
   *  
   *  3) Call NdbTransaction::execute()
   *                         (Row will be locked from here as normal)
   *  
   *  4) Use the read data, make zero or more calls to 
   *     NdbTransaction::execute() etc.
   *  
   *  5) Call NdbTransaction::unlock(NdbLockHandle*), passing in the
   *     const LockHandle* from 2) to create an Unlock operation.
   *  
   *  6) Call NdbTransaction::execute()
   *                         (Row will be unlocked from here)
   * 
   * Notes
   * - As with other operation types, Unlock operations can be batched.
   * - Each LockHandle object refers to a lock placed on a row by a single 
   *   primary key read operation.  A single row in the database may have 
   *   concurrent multiple lock holders (of mode LM_Read) and may have 
   *   multiple lock holders pending (LM_Exclusive), so releasing the
   *   claim of one lock holder may not result in a change to the 
   *   observable lock status of the row.
   * - LockHandles are supported for Scan lock takeover operations - the
   *   lockhandle must be requested before the locktakeover is executed.
   * - LockHandles and Unlock operations are not supported for Unique Index
   *   read operations.
   */

  /* unlock
   * 
   * This method creates an Unlock operation on the current transaction.
   * When executed, the Unlock operation will remove the lock referenced
   * by the passed LockHandle.
   * 
   * The unlock operation can fail, for example due to the row being 
   * unlocked already.  In this scenario, the AbortOption specifies how 
   * this will be handled.
   * The default is that errors will cause transaction abort.
   */
  const NdbOperation* unlock(const NdbLockHandle* lockHandle,
                             NdbOperation::AbortOption ao = NdbOperation::DefaultAbortOption);
  
  /* releaseLockHandle
   * This method is used to release a LockHandle object once it
   * is no longer required.
   * For NdbRecord primary key read operations, this cannot be
   * called until the associated read operation has executed.
   * All LockHandles associated with a transaction are released
   * when it is closed.
   */
  int releaseLockHandle(const NdbLockHandle* lockHandle);

  /* Get maximum number of pending Blob read/write bytes before
   * an automatic execute() occurs 
   */
  Uint32 getMaxPendingBlobReadBytes() const;
  Uint32 getMaxPendingBlobWriteBytes() const;

  /* Set maximum number of pending Blob read/write bytes before
   * an automatic execute() occurs
   */
  void setMaxPendingBlobReadBytes(Uint32 bytes);
  void setMaxPendingBlobWriteBytes(Uint32 bytes);

private:						
  /**
   * Release completed operations
   */
  void releaseCompletedOperations();
  void releaseCompletedQueries();

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
  
  int  receiveTCSEIZECONF(const NdbApiSignal* anApiSignal);
  int  receiveTCSEIZEREF(const NdbApiSignal* anApiSignal);
  int  receiveTCRELEASECONF(const NdbApiSignal* anApiSignal);
  int  receiveTCRELEASEREF(const NdbApiSignal* anApiSignal);
  int  receiveTC_COMMITCONF(const class TcCommitConf *, Uint32 len);
  int  receiveTCKEYCONF(const class TcKeyConf *, Uint32 aDataLength);
  int  receiveTCKEY_FAILCONF(const class TcKeyFailConf *);
  int  receiveTCKEY_FAILREF(const NdbApiSignal* anApiSignal);
  int  receiveTC_COMMITREF(const NdbApiSignal* anApiSignal);
  int  receiveTCROLLBACKCONF(const NdbApiSignal* anApiSignal);
  int  receiveTCROLLBACKREF(const NdbApiSignal* anApiSignal);
  int  receiveTCROLLBACKREP(const NdbApiSignal* anApiSignal);
  int  receiveTCINDXREF(const NdbApiSignal*);
  int  receiveSCAN_TABREF(const NdbApiSignal*);
  int  receiveSCAN_TABCONF(const NdbApiSignal*, const Uint32*, Uint32 len);

  int 	doSend();	                // Send all operations
  int 	sendROLLBACK();	                // Send of an ROLLBACK
  int 	sendTC_HBREP();                 // Send a TCHBREP signal;
  int 	sendCOMMIT();                   // Send a TC_COMMITREQ signal;
  void	setGCI(int GCI);		// Set the global checkpoint identity
 
  int	OpCompleteFailure();
  int	OpCompleteSuccess();
 
  void	OpSent();			// Operation Sent with success
  
  // Free connection related resources and close transaction
  void		release();              

  // Release all operations in connection
  void		releaseOperations();	

  // Release all cursor operations in connection
  void releaseOps(NdbOperation*);	
  void releaseQueries(NdbQueryImpl*);
  void releaseScanOperations(NdbIndexScanOperation*);	
  bool releaseScanOperation(NdbIndexScanOperation** listhead,
			    NdbIndexScanOperation** listtail,
			    NdbIndexScanOperation* op);
  void          releaseLockHandles();
  
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
                              NdbOperation::AbortOption default_ao,
                              const NdbRecord *key_record,
                              const char *key_row,
                              const NdbRecord *attribute_record,
                              const char *attribute_row,
                              const unsigned char *mask,
                              const NdbOperation::OperationOptions *opts,
                              Uint32 sizeOfOptions,
                              const NdbLockHandle* lh = 0);

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
                                                        // Current meanings :
                                                        //   0x00FE11DC : NdbTransaction not in use
                                                        //   0x37412619 : NdbTransaction in use
                                                        //   0x00FE11DF : NdbTransaction for scan operation
                                                        //                scan definition not yet complete
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

  // Scan operations or queries:
  // The operation or query actually performing the scan.
  // (Only one of theScanningOp/m_scanningQuery be non-NULL,
  //  which indirectly indicates the type)
  NdbScanOperation* theScanningOp;

  Uint32 theBuddyConPtr;
  // optim: any blobs
  bool theBlobFlag;
  Uint8 thePendingBlobOps;
  Uint32 maxPendingBlobReadBytes;
  Uint32 maxPendingBlobWriteBytes;
  Uint32 pendingBlobReadBytes;
  Uint32 pendingBlobWriteBytes;
  inline bool hasBlobOperation() { return theBlobFlag; }

  static void sendTC_COMMIT_ACK(class NdbImpl *, NdbApiSignal *,
				Uint32 transId1, Uint32 transId2, 
				Uint32 aBlockRef);

  void completedFail(const char * s);
#ifdef VM_TRACE
  void printState();
#endif
  bool checkState_TransId(const Uint32 * transId) const;

  void remove_list(NdbOperation*& head, NdbOperation*);
  void define_scan_op(NdbIndexScanOperation*);

  NdbLockHandle* m_theFirstLockHandle;
  NdbLockHandle* m_theLastLockHandle;

  NdbLockHandle* getLockHandle();

  friend class HugoOperations;
  friend struct Ndb_free_list_t<NdbTransaction>;

  NdbTransaction(const NdbTransaction&); // Not impl.
  NdbTransaction&operator=(const NdbTransaction&);

  // Query operation (aka multicursor)
  NdbQueryImpl* m_firstQuery;        // First query in defining list.
  NdbQueryImpl* m_firstExecQuery;    // First query to send for execution
  NdbQueryImpl* m_firstActiveQuery;  // First query actively executing, or completed

  // Scan operations or queries:
  // The operation or query actually performing the scan.
  // (Only one of theScanningOp/m_scanningQuery be non-NULL,
  //  which indirectly indicates the type)
  NdbQueryImpl* m_scanningQuery;

  Uint32 m_tcRef;
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
  (void)send_size; //unused
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
