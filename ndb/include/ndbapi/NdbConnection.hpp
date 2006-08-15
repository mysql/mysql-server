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

#ifndef NdbConnection_H
#define NdbConnection_H

#include <ndb_types.h>
#include "NdbError.hpp"
#include "NdbDictionary.hpp"
#include "Ndb.hpp"

class NdbConnection;
class NdbOperation;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbIndexOperation;
class NdbApiSignal;
class Ndb;
class NdbBlob;


/**
 * NdbAsynchCallback functions are used when executing asynchronous 
 * transactions (using NdbConnection::executeAsynchPrepare, or 
 * NdbConnection::executeAsynch).  
 * The functions are called when the execute has finished.
 * See @ref secAsync for more information.
 */
typedef void (* NdbAsynchCallback)(int, NdbConnection*, void*);

/**
 * Commit type of transaction
 */
enum AbortOption {      
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  CommitIfFailFree = 0,         
  CommitAsMuchAsPossible = 2,   ///< Commit transaction with as many 
  TryCommit = 0,                ///< <i>Missing explanation</i>
#endif
  AbortOnError = 0,             ///< Abort transaction on failed operation
  AO_IgnoreError = 2               ///< Transaction continues on failed operation
};
  
typedef AbortOption CommitType;


/**
 * Execution type of transaction
 */
enum ExecType { 
  NoExecTypeDef = -1,           ///< Erroneous type (Used for debugging only)
  Prepare,                      ///< <i>Missing explanation</i>
  NoCommit,                     ///< Execute the transaction as far as it has
                                ///< been defined, but do not yet commit it
  Commit,                       ///< Execute and try to commit the transaction
  Rollback                      ///< Rollback transaction
};


/**
 * @class NdbConnection
 * @brief Represents a transaction.
 *
 * A transaction (represented by an NdbConnection object) 
 * belongs to an Ndb object and is typically created using 
 * Ndb::startTransaction.
 * A transaction consists of a list of operations 
 * (represented by NdbOperation objects). 
 * Each operation access exactly one table.
 *
 * After getting the NdbConnection object, 
 * the first step is to get (allocate) an operation given the table name. 
 * Then the operation is defined. 
 * Several operations can be defined in parallel on the same 
 * NdbConnection object. 
 * When all operations are defined, the NdbConnection::execute
 * method sends them to the NDB kernel for execution. 
 *
 * The NdbConnection::execute method returns when the NDB kernel has 
 * completed execution of all operations defined before the call to 
 * NdbConnection::execute. 
 * All allocated operations should be properly defined 
 * before calling NdbConnection::execute.
 *
 * A call to NdbConnection::execute uses one out of three types of execution:
 *  -# ExecType::NoCommit  Executes operations without committing them.
 *  -# ExecType::Commit	   Executes remaining operation and commits the 
 *        	           complete transaction
 *  -# ExecType::Rollback  Rollbacks the entire transaction.
 *
 * NdbConnection::execute is equipped with an extra error handling parameter 
 * There are two alternatives:
 * -# AbortOption::AbortOnError (default).
 *    The transaction is aborted if there are any error during the
 *    execution
 * -# AbortOption::IgnoreError
 *    Continue execution of transaction even if operation fails
 *
 * NdbConnection::execute can sometimes indicate an error 
 * (return with -1) while the error code on the NdbConnection is 0. 
 * This is an indication that one of the operations found a record 
 * problem. The transaction is still ok and can continue as usual.
 * The NdbConnection::execute returns -1 together with error code 
 * on NdbConnection object equal to 0 always means that an 
 * operation was not successful but that the total transaction was OK. 
 * By checking error codes on the individual operations it is possible 
 * to find out which operation was not successful. 
 *
 * NdbConnection::executeScan is used to setup a scan in the NDB kernel
 * after it has been defined.
 * NdbConnection::nextScanResult is used to iterate through the 
 * scanned tuples. 
 * After each call to NdbConnection::nextScanResult, the pointers
 * of NdbRecAttr objects defined in the NdbOperation::getValue 
 * operations are updated with the values of the new the scanned tuple. 
 */

/* FUTURE IMPLEMENTATION:
 * Later a prepare mode will be added when Ndb supports Prepare-To-Commit
 * The NdbConnection can deliver the Transaction Id of the transaction.
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
 *    already supplied with the call to NdbConnection::getNdbOperation. 
 * -# The third method is used when a scanned tuple is to be transferred to 
 *    another transaction. In this case it is not possible to define the 
 *    primary key since it came along from the scanned tuple.
 *
 */
class NdbConnection
{
  friend class Ndb;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexOperation;
  friend class NdbIndexScanOperation;
  friend class NdbBlob;
  friend class ha_ndbcluster;

public:

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

  /**
   * Get an operation from NdbScanOperation idlelist and 
   * get the NdbConnection object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  aTableName a table name.
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbScanOperation* getNdbScanOperation(const char* aTableName);

  /**
   * Get an operation from NdbScanOperation idlelist and 
   * get the NdbConnection object which
   * was fetched by startTransaction pointing to this operation.
   *
   * @param  anIndexName  The index name.
   * @param  aTableName a table name.
   * @return pointer to an NdbOperation object if successful, otherwise NULL
   */
  NdbIndexScanOperation* getNdbIndexScanOperation(const char* anIndexName,
						  const char* aTableName);
  
  /**
   * Get an operation from NdbIndexOperation idlelist and 
   * get the NdbConnection object that
   * was fetched by startTransaction pointing to this operation.
   *
   * @param   indexName   An index name (as created by createIndex).
   * @param   tableName    A table name.
   * @return                Pointer to an NdbIndexOperation object if 
   *                        successful, otherwise NULL
   */
  NdbIndexOperation* getNdbIndexOperation(const char*  indexName,
                                          const char*  tableName);

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
   *                     IgnoreError  - Accept failing operations
   * @param force        When operations should be sent to NDB Kernel.
   *                     (See @ref secAdapt.)
   *                     - 0: non-force, adaptive algorithm notices it 
   *                          (default); 
   *                     - 1: force send, adaptive algorithm notices it; 
   *                     - 2: non-force, adaptive algorithm do not notice 
   *                          the send.
   * @return 0 if successful otherwise -1.
   */
  int execute(ExecType execType, 
	      AbortOption abortOption = AbortOnError,
	      int force = 0 );

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
   *                        executed.  See @ref ndbapi_example2.cpp 
   *                        for an example on how to specify and use 
   *                        a callback method.
   * @param anyObject       A void pointer.  This pointer is forwarded to the 
   *                        callback method and can be used to give 
   *                        the callback method some data to work on.
   *                        It is up to the application programmer 
   *                        to decide on the use of this pointer.
   * @param abortOption     see @ref execute
   */
  void executeAsynchPrepare(ExecType          execType,
			    NdbAsynchCallback callback,
			    void*             anyObject,
			    AbortOption abortOption = AbortOnError);

  /**
   * Prepare and send an asynchronous transaction.
   *
   * This method perform the same action as 
   * NdbConnection::executeAsynchPrepare
   * but also sends the operations to the NDB kernel.
   *
   * See NdbConnection::executeAsynchPrepare for information
   * about the parameters of this method.
   *
   * See @ref secAsync for more information on
   * how to use this method.
   */
  void executeAsynch(ExecType            aTypeOfExec,
		     NdbAsynchCallback   aCallback,
		     void*               anyObject,
		     AbortOption abortOption = AbortOnError);

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
   * @note It is not allowed to call NdbConnection::close after sending the
   *       transaction asynchronously before the callback method has 
   * 	   been called.
   *       (The application should keep track of the number of 
   *       outstanding transactions and wait until all of them 
   *       has completed before calling NdbConnection::close).
   *       If the transaction is not committed it will be aborted.
   */
  void close();

  /**
   * Restart transaction
   *
   *   Once a transaction has been completed successfully
   *     it can be started again wo/ calling closeTransaction/startTransaction
   *
   *   Note this method also releases completed operations
   */
  int restart();

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
   * @return GCI of transaction or -1 if GCI is not available.
   *         (Note that there has to be an NdbConnection::execute call 
   *         with Ndb::Commit for the GCI to be available.)
   */
  int		getGCI();	
			
  /**
   * Get transaction identity.
   *
   * @return  Transaction id.
   */
  Uint64	getTransactionId();			

  /**
   * Returns the commit status of the transaction.
   *
   * @return  The commit status of the transaction, i.e. one of
   *          { NotStarted, Started, TimeOut, Committed, Aborted, NeedAbort } 
   */
  enum CommitStatusType { 
    NotStarted,                   ///< Transaction not yet started
    Started,                      ///< <i>Missing explanation</i>
    Committed,                    ///< Transaction has been committed
    Aborted,                      ///< Transaction has been aborted
    NeedAbort                     ///< <i>Missing explanation</i>
  };

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
   * This method is used on the NdbConnection object to find the
   * NdbOperation causing an error.  
   * To find more information about the
   * actual error, use method NdbOperation::getNdbError 
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
   * - NdbConnection::getNextCompletedOperation(NULL) returns the
   *   first NdbOperation object.
   * - NdbConnection::getNextCompletedOperation(op) returns the
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

  /** @} *********************************************************************/

  /**
   * Execute the transaction in NoCommit mode if there are any not-yet
   * executed blob part operations of given types.  Otherwise do
   * nothing.  The flags argument is bitwise OR of (1 << optype) where
   * optype comes from NdbOperation::OperationType.  Only the basic PK
   * ops are used (read, insert, update, delete).
   */
  int executePendingBlobOps(Uint8 flags = 0xFF);

  // Fast path calls for MySQL ha_ndbcluster
  NdbOperation* getNdbOperation(const NdbDictionary::Table * table);
  NdbIndexOperation* getNdbIndexOperation(const NdbDictionary::Index *,
					  const NdbDictionary::Table * table);
  NdbScanOperation* getNdbScanOperation(const NdbDictionary::Table * table);
  NdbIndexScanOperation* getNdbIndexScanOperation(const NdbDictionary::Index * index,
						  const NdbDictionary::Table * table);

  Uint32	getConnectedNodeId();	          // Get Connected node id
  
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
  NdbConnection(Ndb* aNdb); 
  ~NdbConnection();
  NdbConnection* next();			  // Returns the next pointer
  void next(NdbConnection*);		  // Sets the next pointer

  void init();           // Initialize connection object for new transaction

  int executeNoBlobs(ExecType execType, 
	             AbortOption abortOption = AbortOnError,
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
  int  receiveTC_COMMITCONF(const class TcCommitConf *);
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
 
  int	OpCompleteFailure(Uint8 abortoption, bool setFailure = true);
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
                                NdbOperation* aNextOp = 0);
  NdbIndexScanOperation* getNdbScanOperation(const class NdbTableImpl* aTable);
  NdbIndexOperation* getNdbIndexOperation(const class NdbIndexImpl* anIndex, 
                                          const class NdbTableImpl* aTable,
                                          NdbOperation* aNextOp = 0);
  NdbIndexScanOperation* getNdbIndexScanOperation(const NdbIndexImpl* index,
						  const NdbTableImpl* table);
  
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
  NdbConnection* theNext;	      	     // Next pointer. Used in idle list.

  NdbOperation*	theFirstOpInList;	    // First operation in defining list.
  NdbOperation*	theLastOpInList;	    // Last operation in defining list.

  NdbOperation*	theFirstExecOpInList;	    // First executing operation in list
  NdbOperation*	theLastExecOpInList;	    // Last executing operation in list.


  NdbOperation*	theCompletedFirstOp;	    // First & last operation in completed 
  NdbOperation*	theCompletedLastOp;         // operation list.

  Uint32	theNoOfOpSent;				// How many operations have been sent	    
  Uint32	theNoOfOpCompleted;			// How many operations have completed
  Uint32        theNoOfOpFetched;           	        // How many operations was actually fetched
  Uint32	theMyRef;				// Our block reference		
  Uint32	theTCConPtr;				// Transaction Co-ordinator connection pointer.
  Uint64	theTransactionId;			// theTransactionId of the transaction
  Uint32	theGlobalCheckpointId;			// The gloabl checkpoint identity of the transaction
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
  Uint8 m_abortOption;           // Type of commit

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

  static void sendTC_COMMIT_ACK(NdbApiSignal *,
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
  friend struct Ndb_free_list_t<NdbConnection>;
};

inline
Uint32
NdbConnection::get_send_size()
{
  return 0;
}

inline
void
NdbConnection::set_send_size(Uint32 send_size)
{
  return;
}

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

inline
int
NdbConnection::checkMagicNumber()
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
NdbConnection::checkState_TransId(const Uint32 * transId) const {
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
NdbConnection::setTransactionId(Uint64 aTransactionId)
{
  theTransactionId = aTransactionId;
}

inline
void			
NdbConnection::setConnectedNodeId(Uint32 aNode, Uint32 aSequenceNo)
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
NdbConnection::getConnectedNodeId()
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
NdbConnection::setMyBlockReference(int aBlockRef)	
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
NdbConnection::setTC_ConnectPtr(Uint32 aTCConPtr)
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
NdbConnection::getTC_ConnectPtr()
{
  return theTCConPtr;
}

inline
void
NdbConnection::setBuddyConPtr(Uint32 aBuddyConPtr)
{
  theBuddyConPtr = aBuddyConPtr;
}

inline
Uint32 NdbConnection::getBuddyConPtr()
{
  return theBuddyConPtr;
}

/******************************************************************************
NdbConnection* next();

inline
void
NdbConnection::setBuddyConPtr(Uint32 aBuddyConPtr)
{
  theBuddyConPtr = aBuddyConPtr;
}

inline
Uint32 NdbConnection::getBuddyConPtr()
{
  return theBuddyConPtr;
}

Return Value:	Return  next pointer to NdbConnection object.
Remark:         Get the next pointer. 
******************************************************************************/
inline
NdbConnection*
NdbConnection::next()
{
  return theNext;
}

/******************************************************************************
void next(NdbConnection aConnection);

Parameters:     aConnection: The connection object. 
Remark:         Sets the next pointer. 
******************************************************************************/
inline
void
NdbConnection::next(NdbConnection* aConnection)
{
  theNext = aConnection;
}

/******************************************************************************
ConStatusType  Status();

Return Value    Return the ConStatusType.	
Parameters:     aStatus:  The status.
Remark:         Sets Connect status. 
******************************************************************************/
inline
NdbConnection::ConStatusType			
NdbConnection::Status()
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
NdbConnection::Status( ConStatusType aStatus )
{
  theStatus = aStatus;
}


/******************************************************************************
 void    	setGCI();

Remark:		Set global checkpoint identity of the transaction
******************************************************************************/
inline
void
NdbConnection::setGCI(int aGlobalCheckpointId)
{
  theGlobalCheckpointId = aGlobalCheckpointId;
}

/******************************************************************************
void OpSent();

Remark:       An operation was sent with success that expects a response.
******************************************************************************/
inline
void 
NdbConnection::OpSent()
{
  theNoOfOpSent++;
}

/******************************************************************************
void executePendingBlobOps();
******************************************************************************/
#include <stdlib.h>
inline
int
NdbConnection::executePendingBlobOps(Uint8 flags)
{
  if (thePendingBlobOps & flags) {
    // not executeNoBlobs because there can be new ops with blobs
    return execute(NoCommit);
  }
  return 0;
}

inline
Uint32
NdbConnection::ptr2int(){
  return theId;
}

#endif
