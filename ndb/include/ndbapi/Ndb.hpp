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

/**
   @mainpage                            NDB API Programmers' Guide

   This guide assumes a basic familiarity with NDB Cluster concepts.
   Some of the fundamental ones are described in section @ref secConcepts.
   
   The <em>NDB API</em> is an NDB Cluster application interface 
   that implements both synchronous and asynchronous transactions.
   The NDB API consists of the following fundamental classes:
   - Ndb is the main class representing the database, 
   - NdbConnection represents a transaction, 
   - NdbOperation represents a transaction operation using primary key,
   - NdbIndexOperation represents a transaction operation using a secondary
     index,
   - NdbRecAttr represents the value of an attribute, and
   - NdbDictionary represents meta information about tables and attributes.
   - NdbError represents an error condition
   There are also some auxiliary classes.
     
   The main structure of an application program is as follows:
   -# Construct and initialize Ndb object(s).
   -# Define and execute (synchronous or asynchronous) transactions.
   -# Delete Ndb objects

   The main structure of a transaction is as follows:
   -# Start transaction
   -# Add and define operations (associated with the transaction)
   -# Execute transaction

   The execute can be of two different types, 
   <em>Commit</em> or <em>NoCommit</em>.
   (The execute can also be divided into three 
   steps: prepare, send, and poll to get asynchronous
   transactions.  More about this later.)
   
   If the execute is of type NoCommit, 
   then the application program executes part of a transaction,
   but without committing the transaction.
   After a NoCommit type of execute, the program can continue 
   to add and define more operations to the transaction
   for later execution.

   If the execute is of type Commit, then the transaction is
   committed and no further adding and defining of operations 
   is allowed.


   @section secSync                     Synchronous Transactions
  
   Synchronous transactions are defined and executed in the following way.
  
    -# Start (create) transaction (the transaction will be 
       referred to by an NdbConnection object, 
       typically created by Ndb::startTransaction).
       At this step the transaction is being defined.
       It is not yet sent to the NDB kernel.
    -# Add and define operations to the transaction 
       (using NdbConnection::getNdbOperation and
       methods from class NdbOperation).
       The transaction is still not sent to the NDB kernel.
    -# Execute the transaction (using NdbConnection::execute).
    -# Close the transaction (using Ndb::closeTransaction).
  
   See example program in section @ref ndbapi_example1.cpp.

   To execute several parallel synchronous transactions, one can either 
   use multiple Ndb objects in several threads or start multiple 
   applications programs.  
   Another way to execute several parallel transactions is to use
   asynchronous transactions.
  
  
   @section secNdbOperations            Operations

   Each transaction (NdbConnection object) consist of a list of 
   operations (NdbOperation or NdbIndexOperation objects.  
   NdbIndexOperation is used for accessing tables through secondary indexes).   
   Operations are of two different kinds:
   -# standard operations, and
   -# interpreted program operations.

   <h3>Standard Operations</h3>
   After the operation is created using NdbConnection::getNdbOperation
   (or NdbConnection::getNdbIndexOperation),
   it is defined in the following three steps:
   -# Defining standard operation type
      (e.g. using NdbOperation::readTuple)
   -# Specifying search conditions 
      (e.g. using NdbOperation::equal)
   -# Specify attribute actions 
      (e.g. using NdbOperation::getValue)

   Example code (using an NdbOperation):
   @code
     MyOperation = MyConnection->getNdbOperation("MYTABLENAME"); // 1. Create
     if (MyOperation == NULL) APIERROR(MyConnection->getNdbError());
    
     MyOperation->readTuple();                  // 2. Define type of operation
     MyOperation->equal("ATTR1", i);            // 3. Specify Search Conditions
    
     MyRecAttr = MyOperation->getValue("ATTR2", NULL);   // 4. Attribute Actions
     if (MyRecAttr == NULL) APIERROR(MyConnection->getNdbError());
   @endcode
   For more examples, see @ref ndbapi_example1.cpp and @ref ndbapi_example2.cpp.

   Example code using an NdbIndexOperation:
   @code
     MyOperation =                              // 1. Create
     	MyConnection->getNdbIndexOperation("MYINDEX", "MYTABLENAME");
     if (MyOperation == NULL) APIERROR(MyConnection->getNdbError());

     MyOperation->readTuple();                  // 2. Define type of operation
     MyOperation->equal("ATTR1", i);            // 3. Specify Search Conditions

     MyRecAttr = MyOperation->getValue("ATTR2", NULL); // 4. Attribute Actions 
     if (MyRecAttr == NULL) APIERROR(MyConnection->getNdbError());
   @endcode
   For more examples, see @ref ndbapi_example4.cpp.


   <h4>Step 1: Define Standard Operation Type</h4>
   The following types of standard operations exist:
    -# NdbOperation::insertTuple : 
       inserts a non-existing tuple
    -# NdbOperation::writeTuple : 
       updates an existing tuple if is exists,
       otherwise inserts a new tuple
    -# NdbOperation::updateTuple : 
       updates an existing tuple
    -# NdbOperation::deleteTuple : 
       deletes an existing tuple
    -# NdbOperation::readTuple : 
       reads an existing tuple
    -# NdbOperation::readTupleExclusive : 
       reads an existing tuple using an exclusive lock
    -# NdbOperation::simpleRead : 
       reads an existing tuple (using shared read lock),
       but releases lock immediately after read
    -# NdbOperation::committedRead : 
       reads committed tuple
    -# NdbOperation::dirtyUpdate : 
       updates an existing tuple, but releases lock immediately
       after read (uses dirty lock)
    -# NdbOperation::dirtyWrite : 
       updates or writes a tuple, but releases lock immediately
       after read (uses dirty lock)

   All of these operations operate on the unique tuple key.
   (When NdbIndexOperation is used then all of these operations 
   operate on a defined secondary index.)


   Some comments:
   - NdbOperation::simpleRead and 
     NdbOperation::committedRead can execute on the same transaction
     as the above operations but will release its locks immediately
     after reading the tuple. 
     NdbOperation::simpleRead will always read the latest version 
     of the tuple. 
     Thus it will wait until it can acquire a shared read lock on 
     the tuple. 
     NdbOperation::committedRead will read the latest committed 
     version of the tuple. 
     <br>
     Both NdbOperation::simpleRead and NdbOperation::committedRead 
     are examples of consistent reads which are not repeatable.
     All reads read the latest version if updates were made by the same
     transaction.
     Errors on simple read are only reported by the NdbOperation object.
     These error codes are not transferred to the NdbConnection object.
   - NdbOperation::dirtyUpdate and NdbOperation::dirtyWrite 
     will execute in the same transaction
     but will release the lock immediately after updating the
     tuple.
     It will wait on the lock until it can acquire an exclusive
     write lock.
     In a replicated version of NDB Cluster NdbOperation::dirtyUpdate 
     can lead to inconsistency between the replicas.
     Examples of when it could be used is
     to update statistical counters on tuples which are "hot-spots".

   @note If you want to define multiple operations within the same transaction,
         then you need to call NdbConnection::getNdbOperation 
	 (or NdbConnection::getNdbIndexOperation) for each
         operation.


   <h4>Step 2: Specify Search Conditions</h4>
   The search condition is used to select tuples.
   (In the current NdbIndexOperation implementation 
   this means setting the value of
   the secondary index attributes of the wanted tuple.)

   If a tuple identity is used, then NdbOperation::setTupleId 
   is used to define the search key when inserting new tuples.
   Otherwise, NdbOperation::equal is used.
   
   For NdbOperation::insertTuple it is also allowed to define the 
   search key by using NdbOperation::setValue.
   The NDB API will automatically detect that it is
   supposed to use NdbOperation::equal instead. 
   For NdbOperation::insertTuple it is not necessary to use
   NdbOperation::setValue on key attributes before other attributes.


   <h4>Step 3: Specify Attribute Actions</h4>
   Now it is time to define which attributes should be read or updated.
   Deletes can neither read nor set values, read can only read values and
   updates can only set values.
   Normally the attribute is defined by its name but it is
   also possible to use the attribute identity to define the
   attribute.
   The mapping from name to identity is performed by the Table object.

   NdbIndexOperation::getValue returns an NdbRecAttr object
   containing the read value.
   To get the value, there is actually two methods.
   The application can either
   - use its own memory (passed through a pointer aValue) to
     NdbIndexOperation::getValue, or
   - receive the attribute value in an NdbRecAttr object allocated
     by the NDB API.

   The NdbRecAttr object is released when Ndb::closeTransaction
   is called.
   Thus, the application can not reference this object after
   Ndb::closeTransaction have been called.
   The result of reading data from an NdbRecAttr object before
   calling NdbConnection::execute is undefined.



   <h3>Interpreted Program Operations</h3>
   The following types of interpreted program operations exist:
    -# NdbOperation::interpretedUpdateTuple :
       updates a tuple using an interpreted program
    -# NdbOperation::interpretedDeleteTuple :
       delete a tuple using an interpreted program
    -# NdbOperation::openScanRead : 
       scans a table with read lock on each tuple
    -# NdbOperation::openScanExclusive : 
       scans a table with exclusive update lock on each tuple

   The operations interpretedUpdateTuple and interpretedDeleteTuple both
   work using the unique tuple key.

   These <em>interpreted programs</em> 
   make it possible to perform computations
   inside the NDB Cluster Kernel instead of in the application
   program.
   This is sometimes very effective, since no intermediate results
   are sent to the application, only the final result.


  <h3>Interpreted Update and Delete</h3>

   Operations for interpreted updates and deletes must follow a
   certain order when defining operations on a tuple.
   As for read and write operations,
   one must first define the operation type and then the search key.
   -# The first step is to define the initial readings.
      In this phase it is only allowed to use the
      NdbOperation::getValue method.
      This part might be empty.
   -# The second step is to define the interpreted part.
      The methods supported are the methods listed below except
      NdbOperation::def_subroutine and NdbOperation::ret_sub
      which can only be used in a subroutine.
      NdbOperation::incValue and NdbOperation::subValue
      increment and decrement attributes
      (currently only unsigned integers supported).
      This part can also be empty since interpreted updates
      can be used for reading and updating the same tuple.
      <p>
      Even though getValue and setValue are not really interpreted
      program instructions, it is still allowed to use them as
      the last instruction of the program.
      (If a getValue or setValue is found when an interpret_exit_ok
      could have been issued then the interpreted_exit_ok
      will be inserted.
      A interpret_exit_ok should be viewed as a jump to the first
      instruction after the interpreted instructions.)
   -# The third step is to define all updates without any
      interpreted program instructions.
      Here a set of NdbOperation::setValue methods are called.
      There might be zero such calls.
   -# The fourth step is the final readings.
      The initial readings reads the initial value of attributes
      and the final readings reads them after their updates.
      There might be zero NdbOperation::getValue calls.
   -# The fifth step is possible subroutine definitions using
      NdbOperation::def_subroutine and NdbOperation::ret_sub.


   @subsection secScan              Scanning 
   The most common use of interpreted programs is for scanning
   tables.  Scanning is a search of all tuples in a table.  
   Tuples which satisfy conditions (a search filter) 
   stated in the interpreted program 
   are sent to the application.

   Reasons for using scan transactions include
   need to use a search key different from the primary key
   and any secondary index.
   Or that the query needs to access so many tuples so that 
   it is more efficient to scan the entire table.

   Scanning can also be used to update information.  
   The scanning transaction itself is however 
   not allowed to update any tuples.
   To do updates via scanning transactions, the tuples 
   need to be handed over to another transaction which is 
   executing the actual update.

   Even though a scan operation is part of a transaction, 
   the scan transaction is not a normal transaction.
   The locks are <em>not</em> kept throughout the entire 
   scan transaction, since this would imply non-optimal performance.  
   <em>
   A transaction containing a scan operation can only 
   contain that operation.  
   No other operations are allowed in the same transaction.
   </em>

   The NdbOperation::openScanRead operation 
   only sets a temporary read lock while
   reading the tuple. 
   The tuple lock is released already when the
   result of the read reaches the application. 
   The NdbOperation::openScanExclusive operation sets an 
   exclusive lock on the tuple 
   and sends the result to the application. 
   Thus when the application reads the data it is still
   locked with the exclusive lock. 

   If the application desires to update the tuple it may transfer
   the tuple to another transaction which updates the tuple.
   The updating transaction can consist of a combination of tuples 
   received from the scan and normal operations. 

   For transferred operations it is not necessary to provide the
   primary key.  It is part of the transfer. 
   You only need to give the operation type and the 
   actions to perform on the tuple.

   The scan transaction starts like a usual transaction,
   but is of the following form:
    -# Start transaction
    -# Get NdbOperation for the table to be scanned
    -# Set the operation type using NdbOperation::openScanRead or 
       NdbOperation::openScanExclusive
    -# Search conditions are defined by an interpreted program
       (setValue and write_attr are not allowed, since scan transactions
       are only allowed to read information).
       The instruction interpret_exit_nok does in this case
       not abort the transaction, it only skips the tuple and 
       proceeds with the next.  
       The skipped tuple will not be reported to the application.
    -# Call NdbConnection::executeScan to define (and start) the scan.
    -# Call NdbConnection::nextScanResult to proceed with next tuple.  
       When calling NdbConnection::nextScanResult, the lock on any 
       previous tuples are released.
       <br>
       If the tuple should be updated then it must be transferred over
       to another updating transaction.  
       This is performed by calling
       NdbOperation::takeOverForUpdate or takeOverForDelete on 
       the scanning transactions NdbOperation object with the updating 
       transactions NdbConnection object as parameter.  
       <p>
       If NdbOperation::takeOverFor* returns NULL then the 
       operation was not successful, otherwise it returns a reference
       to the NdbOperation which the updating transaction has received
    -# Use Ndb::closeTransaction as usual to close the transaction. 
       This can be performed even if there are more tuples to scan.

   See also example program in section @ref select_all.cpp.

   However, a new scan api is under development, using NdbScanOperation
   and NdbScanFilter. NdbScanFilter makes it easier to define a search
   criteria and is recommended instead of using Interpreted Programs.

   The scan transaction starts like a usual transaction,
   but is of the following form:
    -# Start transaction
    -# Get NdbScanOperation for the table to be scanned
    -# NdbScanOperation::readTuplesExclusive returns a handle to a 
       NdbResultSet. 
    -# Search conditions are defined by NdbScanFilter
    -# Call NdbConnection::execute(NoCommit) to start the scan.
    -# Call NdbResultSet::nextResult to proceed with next tuple.  
       When calling NdbResultSet::nextResult(false), the lock on any 
       previous tuples are released and the next tuple cached in the API
       is fetched. 
       <br>
       If the tuple should be updated then define a new update operation 
       (NdbOperation) using NdbResultSet::updateTuple().
       The new update operation can the be used to modify the tuple.
       When nextResult(false) returns != 0, then no more tuples 
       are cached in the API. Updated tuples is now commit using 
       NdbConnection::execute(Commit).
       After the commit, more tuples are fetched from NDB using 
       nextResult(true).
    -# Use Ndb::closeTransaction as usual to close the transaction. 
       This can be performed even if there are more tuples to scan.

       See the scan example program in @ref ndbapi_scan.cppn for example
       usage of the new scan api.


   <h3>Interpreted Programs</h3>
   Interpretation programs are executed in a
   register-based virtual machine.
   The virtual machine has eight 64 bit registers numbered 0-7.
   Each register contains type information which is used both
   for type conversion and for type checking.

   @note Arrays are currently <b>not</b> supported in the virtual machine.
         Currently only unsigned integers are supported and of size
         maximum 64 bits.

   All errors in the interpretation program will cause a
   transaction abort, but will not affect any other transactions.

   The following are legal interpreted program instructions:
   -# incValue        : Add to an attribute
   -# subValue        : Subtract from an attribute
   -# def_label       : Define a label in the interpreted program
   -# add_reg         : Add two registers
   -# sub_reg         : Subtract one register from another
   -# load_const_u32  : Load an unsigned 32 bit value into a register
   -# load_const_u64  : Load an unsigned 64 bit value into a register
   -# load_const_null : Load a NULL value into a register
   -# read_attr       : Read attribute value into a register
   -# write_attr      : Write a register value into an attribute
   -# branch_ge       : Compares registers and possibly jumps to specified label
   -# branch_gt       : Compares registers and possibly jumps to specified label
   -# branch_le       : Compares registers and possibly jumps to specified label
   -# branch_lt       : Compares registers and possibly jumps to specified label
   -# branch_eq       : Compares registers and possibly jumps to specified label
   -# branch_ne       : Compares registers and possibly jumps to specified label
   -# branch_ne_null  : Jumps if register does not contain NULL value
   -# branch_eq_null  : Jumps if register contains NULL value
   -# branch_label    : Unconditional jump to label
   -# interpret_exit_ok  : Exit interpreted program
                           (approving tuple if used in scan)
   -# interpret_exit_nok : Exit interpreted program
                           (disqualifying tuple if used in scan)

   There are also three instructions for subroutines, which
   are described in the next section.

   @subsection subsubSub                Interpreted Programs: Subroutines

   The following are legal interpreted program instructions for
   subroutines:
   -# NdbOperation::def_subroutine : 
      Defines start of subroutine in interpreted program code
   -# NdbOperation::call_sub : 
      Calls a subroutine
   -# NdbOperation::ret_sub : 
      Return from subroutine

   The virtual machine executes subroutines using a stack for
   its operation.
   The stack allows for up to 24 subroutine calls in succession.
   Deeper subroutine nesting will cause an abort of the transaction.

   All subroutines starts with the instruction
   NdbOperation::def_subroutine and ends with the instruction
   NdbOperation::ret_sub.
   If it is necessary to return earlier in the subroutine
   it has to be done using a branch_label instruction
   to a label defined right before the 
   NdbOperation::ret_sub instruction.

   @note The subroutines are automatically numbered starting with 0.
         The parameter used by NdbOperation::def_subroutine 
	 should match the automatic numbering to make it easier to 
	 debug the interpreted program.


   @section secAsync                    Asynchronous Transactions
   The asynchronous interface is used to increase the speed of
   transaction executing by better utilizing the connection
   between the application and the NDB Kernel.
   The interface is used to send many transactions 
   at the same time to the NDB kernel.  
   This is often much more efficient than using synchronous transactions.
   The main reason for using this method is to ensure that 
   Sending many transactions at the same time ensures that bigger 
   chunks of data are sent when actually sending and thus decreasing 
   the operating system overhead.

   The synchronous call to NdbConnection::execute 
   normally performs three main steps:<br>
   -# <b>Prepare</b> 
      Check transaction status
      - if problems, abort the transaction
      - if ok, proceed
   -# <b>Send</b> 
      Send the defined operations since last execute
      or since start of transaction.
   -# <b>Poll</b>
      Wait for response from NDB kernel.

   The asynchronous method NdbConnection::executeAsynchPrepare 
   only perform step 1.
   (The abort part in step 1 is only prepared for.  The actual 
   aborting of the transaction is performed in a later step.)

   Asynchronous transactions are defined and executed 
   in the following way.
   -# Start (create) transactions (same way as for the 
       synchronous transactions)
   -# Add and define operations (also as in the synchronous case)
   -# <b>Prepare</b> transactions 
       (using NdbConnection::executeAsynchPrepare or 
       NdbConnection::executeAsynch)
   -# <b>Send</b> transactions to NDB Kernel
       (using Ndb::sendPreparedTransactions, 
       NdbConnection::executeAsynch, or Ndb::sendPollNdb)
   -# <b>Poll</b> NDB kernel to find completed transactions 
       (using Ndb::pollNdb or Ndb::sendPollNdb)
   -# Close transactions (same way as for the synchronous transactions)

   See example program in section @ref ndbapi_example2.cpp.
   
   This prepare-send-poll protocol actually exists in four variants:
   - (Prepare-Send-Poll).  This is the one-step variant provided
     by synchronous transactions.
   - (Prepare-Send)-Poll.  This is the two-step variant using
     NdbConnection::executeAsynch and Ndb::pollNdb.
   - Prepare-(Send-Poll).  This is the two-step variant using
     NdbConnection::executeAsynchPrepare and Ndb::sendPollNdb.
   - Prepare-Send-Poll.  This is the three-step variant using
     NdbConnection::executeAsynchPrepare, Ndb::sendPreparedTransactions, and
     Ndb::pollNdb.
  
   Transactions first has to be prepared by using method
   NdbConnection::executeAsynchPrepare or NdbConnection::executeAsynch.
   The difference between these is that 
   NdbConnection::executeAsynch also sends the transaction to 
   the NDB kernel.
   One of the arguments to these methods is a callback method.
   The callback method is executed during polling (item 5 above).
  
   Note that NdbConnection::executeAsynchPrepare does not 
   send the transaction to the NDB kernel.  When using 
   NdbConnection::executeAsynchPrepare, you either have to call 
   Ndb::sendPreparedTransactions or Ndb::sendPollNdb to send the 
   database operations.
   (Ndb::sendPollNdb also polls Ndb for completed transactions.)
  
   The methods Ndb::pollNdb and Ndb::sendPollNdb checks if any 
   sent transactions are completed.  The method Ndb::sendPollNdb 
   also send all prepared transactions before polling NDB.
   Transactions still in the definition phase (i.e. items 1-3 above, 
   transactions which has not yet been sent to the NDB kernel) are not 
   affected by poll-calls.
   The poll method invoked (either Ndb::pollNdb or Ndb::sendPollNdb)
   will return when:
    -# at least 'minNoOfEventsToWakeup' of the transactions
       are finished processing, and
    -# all of these transactions have executed their 
       callback methods.
  
   The poll method returns the number of transactions that 
   have finished processing and executed their callback methods.
  
   @note When an asynchronous transaction has been started and sent to
         the NDB kernel, it is not allowed to execute any methods on
         objects belonging to this transaction until the transaction
         callback method have been executed.
         (The transaction is stated and sent by either
	 NdbConnection::executeAsynch or through the combination of
         NdbConnection::executeAsynchPrepare and either
         Ndb::sendPreparedTransactions or Ndb::sendPollNdb).

   More about how transactions are send the NDB Kernel is 
   available in section @ref secAdapt.


   @section secError                    Error Handling

   Errors can occur when
   -# operations are being defined, or when the
   -# transaction is being executed.

   One recommended way to handle a transaction failure 
   (i.e. an error is reported) is to:
   -# Rollback transaction (NdbConnection::execute with a special parameter)
   -# Close transaction
   -# Restart transaction (if the error was temporary)

   @note Transaction are not automatically closed when an error occur.

   Several errors can occur when a transaction holds multiple 
   operations which are simultaneously executed.
   In this case the application has to go through the operation
   objects and query for their NdbError objects to find out what really
   happened.

   NdbConnection::getNdbErrorOperation returns a reference to the 
   operation causing the latest error.
   NdbConnection::getNdbErrorLine delivers the method number of the 
   erroneous method in the operation.

   @code
     theConnection = theNdb->startTransaction();
     theOperation = theConnection->getNdbOperation("TEST_TABLE");
     if (theOperation == NULL) goto error;
     theOperation->readTuple();
     theOperation->setValue("ATTR_1", at1);
     theOperation->setValue("ATTR_2", at1); //Here an error occurs
     theOperation->setValue("ATTR_3", at1);
     theOperation->setValue("ATTR_4", at1);
    
     if (theConnection->execute(Commit) == -1) {
       errorLine = theConnection->getNdbErrorLine();
       errorOperation = theConnection->getNdbErrorOperation();
   @endcode

   Here errorLine will be 3 as the error occurred in the third method 
   on the operation object.
   Getting errorLine == 0 means that the error occurred when executing the 
   operations.
   Here errorOperation will be a pointer to the theOperation object.
   NdbConnection::getNdbError will return the NdbError object 
   including holding information about the error.

   Since errors could have occurred even when a commit was reported,
   there is also a special method, NdbConnection::commitStatus,
   to check the commit status of the transaction.

*******************************************************************************/

/**
 * @page ndbapi_example1.cpp ndbapi_example1.cpp
 * @include ndbapi_example1.cpp 
 */

/**
 * @page ndbapi_example2.cpp ndbapi_example2.cpp
 * @include ndbapi_example2.cpp 
 */

/**
 * @page ndbapi_example3.cpp ndbapi_example3.cpp
 * @include ndbapi_example3.cpp 
 */

/**
 * @page ndbapi_example4.cpp ndbapi_example4.cpp
 * @include ndbapi_example4.cpp 
 */

/**
 * @page select_all.cpp select_all.cpp
 * @include select_all.cpp 
 */

/**
 * @page ndbapi_async.cpp ndbapi_async.cpp
 * @include ndbapi_async.cpp
 */

/**
 * @page ndbapi_scan.cpp ndbapi_scan.cpp
 * @include ndbapi_scan.cpp
 */


/**
   @page secAdapt  Adaptive Send Algorithm

   At the time of "sending" the transaction 
   (using NdbConnection::execute, NdbConnection::executeAsynch, 
   Ndb::sendPreparedTransactions, or Ndb::sendPollNdb), the transactions 
   are in reality <em>not</em> immediately transfered to the NDB Kernel.  
   Instead, the "sent" transactions are only kept in a 
   special send list (buffer) in the Ndb object to which they belong.
   The adaptive send algorithm decides when transactions should
   be transfered to the NDB kernel.
  
   For each of these "sent" transactions, there are three 
   possible states:
   -# Waiting to be transfered to NDB Kernel.
   -# Has been transfered to the NDB Kernel and is currently 
      being processed.
   -# Has been transfered to the NDB Kernel and has 
      finished processing.
      Now it is waiting for a call to a poll method.  
      (When the poll method is invoked, 
      then the transaction callback method will be executed.)
      
   The poll method invoked (either Ndb::pollNdb or Ndb::sendPollNdb)
   will return when:
   -# at least 'minNoOfEventsToWakeup' of the transactions
      in the send list have transitioned to state 3 as described above, and 
   -# all of these transactions have executed their callback methods.
  
  
   Since the NDB API is designed as a multi-threaded interface, 
   it is desirable to transfer database operations from more than 
   one thread at a time. 
   The NDB API keeps track of which Ndb objects are active in transfering
   information to the NDB kernel and the expected amount of threads to 
   interact with the NDB kernel.
   Note that an Ndb object should be used in at most one thread. 
   Two different threads should <em>not</em> use the same Ndb object.
  
   There are four reasons leading to transfering of database 
   operations:
   -# The NDB Transporter (TCP/IP, OSE, SCI or shared memory)
      decides that a buffer is full and sends it off. 
      The buffer size is implementation dependent and
      might change between NDB Cluster releases.
      On TCP/IP the buffer size is usually around 64 kByte and 
      on OSE/Delta it is usually less than 2000 bytes. 
      In each Ndb object there is one buffer per DB node, 
      so this criteria of a full buffer is only 
      local to the connection to one DB node.
   -# Statistical information on the transfered information
      may force sending of buffers to all DB nodes.
   -# Every 10 ms a special send-thread checks whether 
      any send activity has occurred.  If not, then the thread will 
      force sending to all nodes. 
      This means that 20 ms is the maximum time database operations 
      are waiting before being sent off. The 10 millisecond limit 
      is likely to become a configuration parameter in
      later releases of NDB Cluster.
      However, to support faster than 10 ms checks, 
      there has to be support from the operating system.
   -# When calling NdbConnection::execute synchronously or calling any 
      of the poll-methods, there is a force parameter that overrides the 
      adaptive algorithm and forces the send to all nodes.

   @note The times mentioned above are examples.  These might 
         change in later releases of NDB Cluster.
*/

/**
   @page secConcepts  NDB Cluster Concepts

   The <em>NDB Kernel</em> is the collection of database (DB) nodes
   belonging to an NDB Cluster.
   The application programmer can for most purposes view the
   set of all DB nodes as one entity.
   Each DB node has three main components:
   - TC : The transaction coordinator
   - ACC : The index storage
   - TUP : The data storage

   When the application program executes a transaction,
   it connects to one TC on one DB node.  
   Usually, the programmer does not need to specify which TC to use, 
   but some cases when performance is important,
   transactions can be hinted to use a certain TC.  
   (If the node with the TC is down, then another TC will 
   automatically take over the work.)

   Every DB node has an ACC and a TUP which stores 
   the index and the data part of the database.
   Even though one TC is responsible for the transaction,
   several ACCs and TUPs on other DB nodes might be involved in the 
   execution of the transaction.


   @section secNdbKernelConnection   Selecting Transaction Coordinator 
   The default method is round robin, 
   where each new set of transactions
   is placed on the next DB node.
   The application chooses a TC for a number of transactions
   and then lets the next TC (on the next DB node) carry out
   the next set of transactions.
   
   The application programmer can however hint the NDB API which 
   transaction coordinator to use
   by providing a <em>distribution key</em> (usually the primary key).
   By using the primary key as distribution key, 
   the transaction will be placed on the node where the primary replica
   of that record resides.
   Note that this is only a hint, the system can be 
   reconfigured and then the NDB API will choose a transaction
   coordinator without using the hint.
   For more information, see NdbDictionary::Column::setDistributionKey.


   @section secRecordStruct          Record Structure 
   NDB Cluster is a relational database with tables of records.
   Table rows represent tuples of relational data stored as records.
   When created, the attribute schema of the table is specified,
   and thus each record of the table has the same schema.
   

   @subsection secKeys               Tuple Keys
   Each record has from zero up to four attributes which belong
   to the primary key of the table.
   If no attribute belongs to the primary key, then
   the NDB Cluster creates an attribute named <em>NDB$TID</em>
   which stores a tuple identity.
   The <em>tuple key</em> of a table is thus either 
   the primary key attributes or the special NDB$TID attribute.


   @subsection secArrays             Array Attributes
   A table attribute in NDB Cluster can be of <em>array type</em>.
   This means that the attribute consists of an array of 
   <em>elements</em>.  The <em>attribute size</em> is the size
   of one element of the array (expressed in bits) and the 
   <em>array size</em> is the number of elements of the array.
   

   @section secTrans                 Transactions

   Transactions are committed to main memory, 
   and are committed to disk after a global checkpoint, GCP.
   Since all data is (in most NDB Cluster configurations) 
   synchronously replicated and stored on multiple NDB nodes,
   the system can still handle processor failures without loss 
   of data.
   However, in the case of a system failure (e.g. the whole system goes down), 
   then all (committed or not) transactions after the latest GCP are lost.


   @subsection secConcur                Concurrency Control
   NDB Cluster uses pessimistic concurrency control based on locking.
   If a requested lock (implicit and depending on database operation)
   cannot be attained within a specified time, 
   then a timeout error occurs.

   Concurrent transactions (parallel application programs, thread-based 
   applications, or applications with asynchronous transactions)
   sometimes deadlock when they try to access the same information.
   Applications need to be programmed so that timeout errors
   occurring due to deadlocks are handled.  This generally
   means that the transaction encountering timeout
   should be rolled back and restarted.
*/

#ifndef Ndb_H
#define Ndb_H

#include <ndb_types.h>
#include <ndbapi_limits.h>
#include <NdbError.hpp>
#include <NdbDictionary.hpp>

class NdbObjectIdMap;
class NdbOperation;
class NdbEventOperationImpl;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbIndexOperation;
class NdbConnection;
class NdbApiSignal;
class NdbRecAttr;
class NdbLabel;
class NdbBranch;
class NdbSubroutine;
class NdbCall;
class Table;
class BaseString;
class NdbEventOperation;
class NdbBlob;

typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);


#if defined NDB_OSE
/**
 * Default time to wait for response after request has been sent to 
 * NDB Cluster (Set to 10 seconds usually, but to 100 s in 
 * the OSE operating system)
 */
#define WAITFOR_RESPONSE_TIMEOUT 100000 // Milliseconds
#else
#define WAITFOR_RESPONSE_TIMEOUT 120000 // Milliseconds
#endif

#define NDB_MAX_INTERNAL_TABLE_LENGTH NDB_MAX_DATABASE_NAME_SIZE + \
                                      NDB_MAX_SCHEMA_NAME_SIZE + \
                                      NDB_MAX_TAB_NAME_SIZE*2

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
class NdbWaiter {
public:
  NdbWaiter();
  ~NdbWaiter();

  void wait(int waitTime);
  void nodeFail(Uint32 node);
  void signal(Uint32 state);

  Uint32 m_node;
  Uint32 m_state;
  void * m_mutex;
  struct NdbCondition * m_condition;  
};
#endif

/**
 * @class Ndb 
 * @brief Represents the NDB kernel and is the main class of the NDB API.
 *
 * Always start your application program by creating an Ndb object. 
 * By using several Ndb objects it is possible to design 
 * a multi-threaded application, but note that Ndb objects 
 * cannot be shared by several threads. 
 * Different threads should use different Ndb objects. 
 * A thread might however use multiple Ndb objects.
 * Currently there is a limit of maximum 128 Ndb objects 
 * per application process.
 *
 * @note It is not allowed to call methods in the NDB API 
 *       on the same Ndb object in different threads 
 *       simultaneously (without special handling of the 
 *       Ndb object).
 *
 * @note The Ndb object is multi-thread safe in the following manner. 
 *       Each Ndb object can ONLY be handled in one thread. 
 *       If an Ndb object is handed over to another thread then the 
 *       application must ensure that a memory barrier is used to 
 *       ensure that the new thread see all updates performed by 
 *       the previous thread. 
 *       Semaphores, mutexes and so forth are easy ways of issuing memory 
 *       barriers without having to bother about the memory barrier concept.
 *
 * If one Ndb object is used to handle parallel transactions through the 
 * asynchronous programming interface, please read the notes regarding
 * asynchronous transactions (Section @ref secAsync).
 * The asynchronous interface provides much higher performance 
 * in some situations, but is more complicated for the application designer. 
 *
 * @note Each Ndb object should either use the methods for 
 *       asynchronous transaction or the methods for 
 *       synchronous transactions but not both.
 */
class Ndb
{
  friend class NdbReceiver;
  friend class NdbOperation;
  friend class NdbEventOperationImpl;
  friend class NdbConnection;
  friend class Table;
  friend class NdbApiSignal;
  friend class NdbIndexOperation;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class NdbDictionaryImpl;
  friend class NdbDictInterface;
  friend class NdbBlob;

public:
  /** 
   * @name General 
   * @{
   */
  /**
   * The starting point of your application code is to create an 
   * Ndb object.  
   * This object represents the NDB kernel and is the main
   * object used in interaction with the NDB kernel.
   *
   * @param aCatalogName is the name of the catalog you want to use.
   * @note The catalog name provides a name space for the tables and
   *       indexes created in any connection from the Ndb object.
   * @param aSchemaName is the name of the schema you 
   *        want to use. It is optional and defaults to the "def" schema.
   * @note The schema name provides an additional name space 
   *       for the tables and indexes created in a given catalog.
   * @note The methods get/setDatabaseName and get/setDatabaseSchemaName
   *       are equivalent to get/setCatalogName and get/setSchemaName.
   *       The get/setDatabaseName and get/setDatabaseSchemaName are
   *       deprecated.
   */
  Ndb(const char* aCatalogName = "", const char* aSchemaName = "def");

  ~Ndb();

  /**
   * The current catalog name can be fetched by getCatalogName.
   *
   * @return the current catalog name
   */
  const char * getCatalogName() const;

  /**
   * The current catalog name can be set by setCatalogName.
   *
   * @param aCatalogName is the new name of the current catalog
   */
  void setCatalogName(const char * aCatalogName);

  /**
   * The current schema name can be fetched by getSchemaName.
   *
   * @return the current schema name
   */
  const char * getSchemaName() const;

  /**
   * The current schema name can be set by setSchemaName.
   *
   * @param aSchemaName is the new name of the current schema
   */
  void setSchemaName(const char * aSchemaName);


  /**
   * The current database name can be fetched by getDatabaseName.
   *
   * @return the current database name
   */
  const char * getDatabaseName() const;

  /**
   * The current database name can be set by setDatabaseName.
   *
   * @param aDatabaseName is the new name of the current database
   */
  void setDatabaseName(const char * aDatabaseName);

  /**
   * The current database schema name can be fetched by getDatabaseSchemaName.
   *
   * @return the current database schema name
   */
  const char * getDatabaseSchemaName() const;

  /**
   * The current database schema name can be set by setDatabaseSchemaName.
   *
   * @param aDatabaseSchemaName is the new name of the current database schema
   */
  void setDatabaseSchemaName(const char * aDatabaseSchemaName);

  /**
   * Before anything else it is necessary to initialize (start)
   * the Ndb object.
   *
   * @param  maxNoOfTransactions 
   *         Maximum number of parallel 
   *         NdbConnection objects that should be handled by the Ndb object.
   *         A value larger than 1024 will be downgraded to 1024. 
   *         This means that one Ndb object can handle at most 1024 parallel
   *         transactions. 
   * @return 0 if successful, -1 otherwise.
   *
   * @note   The internal implementation multiplies this value 
   *         with 3.
   */
  int init(int maxNoOfTransactions = 4);

  /**
   * Wait for Ndb object to successfully set-up connections to 
   * the NDB kernel. 
   * Starting to use the Ndb object without using this method 
   * gives unspecified behavior. 
   * 
   * @param  timeout  The maximum time we will wait for 
   *                  the initiation process to finish.
   *                  Timeout is expressed in seconds.
   * @return  0: Ndb is ready and timeout has not occurred.<br>
   *          -1: Timeout has expired
   */
  int waitUntilReady(int timeout = 60);
  
  /** @} *********************************************************************/

  /** 
   * @name Meta Information
   * @{
   */

  /**
   * Query the database for schema information 
   * (without performing any transaction).
   *
   * @return Object containing meta information about all tables 
   *         in NDB Cluster.
   */
  class NdbDictionary::Dictionary* getDictionary() const;
  
  NdbEventOperation* createEventOperation(const char* eventName,
					  const int bufferLength);
  int dropEventOperation(NdbEventOperation*);
  void monitorEvent(NdbEventOperation *, NdbEventCallback, void*);
  int pollEvents(int aMillisecondNumber);

  /**
   * Get the application node identity.  
   *
   * Each node (DB nodes, Applications, and Management Servers) 
   * has its own node identity in the NDB Cluster.
   * See documentation for the management server configuration file.
   *
   * @return Node id of this application.
   */
  int getNodeId();

  /** @} *********************************************************************/

  /** 
   * @name Starting and Closing Transactions
   * @{
   */

  /**
   * This method returns an NdbConnection which caters for the transaction. 
   * When the transaction is completed it must be closed. 
   * The Ndb::closeTransaction also return the NdbConnection object 
   * and all other memory related to the transaction. 
   * Failure to close the transaction will lead to memory leakage. 
   * The transaction must be closed independent of its outcome, i.e.
   * even if there is an error.
   * 
   * NDB API can be hinted to select a particular transaction coordinator.
   * The default method is round robin where each set of new transactions 
   * is placed on the next NDB kernel node. 
   * By providing a distribution key (usually the primary key
   * of the mostly used table of the transaction) for a record
   * the transaction will be placed on the node where the primary replica
   * of that record resides. 
   * Note that this is only a hint, the system can
   * be under reconfiguration and then the NDB API 
   * will use select the transaction coordinator without using 
   * this hint.
   *
   * Placing the transaction coordinator close
   * to the actual data used in the transaction can in many cases
   * improve performance significantly. This is particularly true for
   * systems using TCP/IP. A system using Solaris and a 500 MHz processor
   * has a cost model for TCP/IP communication which is:
   *
   *   30 microseconds + (100 nanoseconds * no of Bytes)
   *
   * This means that if we can ensure that we use "popular" links we increase
   * buffering and thus drastically reduce the communication cost.
   * Systems using SCI has a different cost model which is:
   *
   *   5 microseconds + (10 nanoseconds *  no of Bytes)
   *
   * Thus SCI systems are much less dependent on selection of 
   * transaction coordinators. 
   * Typically TCP/IP systems spend 30-60% of the time during communication,
   * whereas SCI systems typically spend 5-10% of the time during
   * communication. 
   * Thus SCI means that less care from the NDB API programmer is
   * needed and great scalability can be achieved even for applications using
   * data from many parts of the database.
   *
   * A simple example is an application that uses many simple updates where
   * a transaction needs to update one record. 
   * This record has a 32 bit primary key, 
   * which is also the distribution key. 
   * Then the keyData will be the address of the integer 
   * of the primary key and keyLen will be 4.
   *
   * @note Transaction priorities are not yet supported.
   *
   * @param  prio     The priority of the transaction.<br>
   *                  Priority 0 is the highest priority and is used
   *                  for short transactions with requirements on low delay.<br>
   *                  Priority 1 is a medium priority for short transactions.
   *                  <br>
   *                  Priority 2 is a medium priority for long transactions.<br>
   *                  Priority 3 is a low priority for long transactions.<br>
   *                  <em>This parameter is not currently used,
   *                      and can be set to any value</em>
   * @param  keyData  Pointer to distribution key
   * @param  keyLen   Length of distribution key expressed in bytes
   * 
   * @return NdbConnection object, or NULL if method failed.
   */
  NdbConnection* startTransaction(Uint32        prio = 0, 
				  const char *  keyData = 0, 
				  Uint32        keyLen = 0);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * This method is a modification of Ndb::startTransaction, 
   * in which we use only the first two chars of keyData to 
   * select transaction coordinator.
   * This is referred to as a distribution group. 
   * There are two ways to use the method: 
   * - In the first, the two characters are used directly as 
   *   the distribution key, and 
   * - in the second the distribution is calculated as:
   *   (10 * (char[0] - 0x30) + (char[1] - 0x30)). 
   *   Thus, in the second way, the two ASCII digits '78' 
   *   will provide the distribution key = 78.
   *
   * @note Transaction priorities are not yet supported.
   *
   * @param aPrio   Priority of the transaction.<br>
   * 	Priority 0 is the highest priority and is used for short transactions 
   *      with requirements on low delay.<br>
   * 	Priority 1 is a medium priority for short transactions.<br>
   * 	Priority 2 is a medium priority for long transactions.<br>
   * 	Priority 3 is a low priority for long transactions.
   * @param keyData is a string of which the two first characters 
   *        is used to compute which fragement the data is stored in.
   * @param type is the type of distribution group.<br> 
   *        0 means direct usage of the two characters, and<br>
   *        1 means the ASCII digit variant.
   * @return NdbConnection, or NULL if it failed.
   */
  NdbConnection* startTransactionDGroup(Uint32 aPrio, 
					const char * keyData, int type);
#endif

  /**
   * When a transactions is completed, the transaction has to be closed.
   *
   * @note It is not allowed to call Ndb::closeTransaction after sending the
   *       transaction asynchronously with either 
   *       Ndb::sendPreparedTransactions or
   *       Ndb::sendPollNdb before the callback method has been called.
   *       (The application should keep track of the number of 
   *       outstanding transactions and wait until all of them 
   *       has completed before calling Ndb::closeTransaction).
   *       If the transaction is not committed it will be aborted.
   */
  void closeTransaction(NdbConnection* aConnection);
  

  /** @} *********************************************************************/

  /** 
   * @name Asynchronous Transactions
   * @{
   */

  /**
   * Wait for prepared transactions.
   * Will return as soon as at least 'minNoOfEventsToWakeUp' 
   * of them have completed, or the maximum time given as timeout has passed.
   *
   * @param aMillisecondNumber Maximum time to wait for transactions
   *                           to complete.
   *                           Polling without wait is achieved by setting the 
   *                           timer to zero.
   *                           Time is expressed in milliseconds.
   * @param minNoOfEventsToWakeup Minimum number of transactions 
   *            which has to wake up before the poll-call will return.
   *            If minNoOfEventsToWakeup is
   *            set to a value larger than 1 then this is the minimum 
   *            number of transactions that need to complete before the 
   *            poll will return.
   *            Setting it to zero means that one should wait for all
   *            outstanding transactions to return before waking up.
   * @return Number of transactions polled.
   */
  int  pollNdb(int aMillisecondNumber = WAITFOR_RESPONSE_TIMEOUT,
	      int minNoOfEventsToWakeup = 1);

  /**
   * This send method will send all prepared database operations. 
   * The default method is to do it non-force and instead
   * use the adaptive algorithm.  (See Section @ref secAdapt.)
   * The second option is to force the sending and 
   * finally there is the third alternative which is 
   * also non-force but also making sure that the 
   * adaptive algorithm do not notice the send. 
   * In this case the sending will be performed on a 
   * cyclical 10 millisecond event.
   *
   * @param forceSend When operations should be sent to NDB Kernel.
   *                  (See @ref secAdapt.)
   *                  - 0: non-force, adaptive algorithm notices it (default); 
   *                  - 1: force send, adaptive algorithm notices it; 
   *                  - 2: non-force, adaptive algorithm do not notice the send.
   */
  void sendPreparedTransactions(int forceSend = 0);

  /**
   * This is a send-poll variant that first calls 
   * Ndb::sendPreparedTransactions and then Ndb::pollNdb. 
   * It is however somewhat faster than calling the methods 
   * separately, since some mutex-operations are avoided. 
   * See documentation of Ndb::pollNdb and Ndb::sendPreparedTransactions
   * for more details.
   *
   * @param aMillisecondNumber Timeout specifier
   *            Polling without wait is achieved by setting the 
   *            millisecond timer to zero.
   * @param minNoOfEventsToWakeup Minimum number of transactions 
   *            which has to wake up before the poll-call will return.
   *            If minNoOfEventsToWakeup is
   *            set to a value larger than 1 then this is the minimum 
   *            number of transactions that need to complete before the 
   *            poll-call will return.
   *            Setting it to zero means that one should wait for all
   *            outstanding transactions to return before waking up.
   * @param forceSend When operations should be sent to NDB Kernel.
   *                  (See @ref secAdapt.)
   * - 0: non-force, adaptive algorithm notices it (default); 
   * - 1: force send, adaptive algorithm notices it; 
   * - 2: non-force, adaptive algorithm does not notice the send.
   * @return Number of transactions polled.
   */
  int  sendPollNdb(int aMillisecondNumber = WAITFOR_RESPONSE_TIMEOUT,
		   int minNoOfEventsToWakeup = 1,
		   int forceSend = 0);
  
  /** @} *********************************************************************/

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get the NdbError object
   *
   * The NdbError object is valid until you call a new NDB API method.
   */
  const NdbError & getNdbError() const;
  
  /**
   * Get a NdbError object for a specific error code
   *
   * The NdbError object is valid until you call a new NDB API method.
   */
  const NdbError & getNdbError(int errorCode);


  /**
   * setConnectString
   * @param connectString - the connectString has the following format:
   * @code
   * "nodeid=<ID>;host=host://<HOSTNAME>:<PORT>;
   *  host=host://<HOSTNAME2>:<PORT>;..."
   * @endcode
   * or
   * @code
   * "nodeid=<ID>;host=<HOSTNAME>:<PORT>;host=<HOSTNAME2>:<PORT>;..."
   * @endcode
   */
  static void setConnectString(const char * connectString);

  /**
   * useFullyQualifiedNames
   * Enables unique name space for different databases and schemas
   * by defining table names as DATABASENAME/SCHEMANAME/TABLENAME and
   * index names as DATABASENAME/SCHEMANAME/TABLENAME/INDEXNAME
   * @param turnNamingOn bool true - turn naming on, false - turn naming off
   */
  void useFullyQualifiedNames(bool turnNamingOn = true);

  bool usingFullyQualifiedNames();

  /** @} *********************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL

  /**
   * Different types of tampering with the NDB Cluster.
   * <b>Only for debugging purposes only.</b>
   */
  enum TamperType	{ 
    LockGlbChp = 1,               ///< Lock GCP
    UnlockGlbChp,                 ///< Unlock GCP
    CrashNode,                    ///< Crash an NDB node
    ReadRestartGCI,               ///< Request the restart GCI id from NDB Cluster
    InsertError                   ///< Execute an error in NDB Cluster 
                                  ///< (may crash system)
  };

  /**
   * For testing purposes it is possible to tamper with the NDB Cluster
   * (i.e. send a special signal to DBDIH, the NDB distribution handler).
   * <b>This feature should only used for debugging purposes.</b>
   * In a release versions of NDB Cluster,
   * this call always return -1 and does nothing.
   * 
   * @param aAction Action to be taken according to TamperType above
   *
   * @param aNode  Which node the action will be taken
   *              -1:   Master DIH.
   *            0-16:   Nodnumber.
   * @return -1 indicates error, other values have meaning dependent 
   *          on type of tampering.
   */
  int NdbTamper(TamperType aAction, int aNode);  
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Return a unique tuple id for a table.  The id sequence is
   * ascending but may contain gaps.
   *
   * @param aTableName table name
   *
   * @param cacheSize number of values to cache in this Ndb object
   *
   * @return tuple id or 0 on error
   */
  Uint64 getAutoIncrementValue(const char* aTableName, 
			       Uint32 cacheSize = 1);
  Uint64 getAutoIncrementValue(NdbDictionary::Table * aTable, 
			       Uint32 cacheSize = 1);
  Uint64 readAutoIncrementValue(const char* aTableName);
  Uint64 readAutoIncrementValue(NdbDictionary::Table * aTable);
  bool setAutoIncrementValue(const char* aTableName, Uint64 val, 
			     bool increase = false);
  bool setAutoIncrementValue(NdbDictionary::Table * aTable, Uint64 val, 
			     bool increase = false);
  Uint64 getTupleIdFromNdb(const char* aTableName, 
			   Uint32 cacheSize = 1000);
  Uint64 getTupleIdFromNdb(Uint32 aTableId, 
			   Uint32 cacheSize = 1000);
  Uint64 readTupleIdFromNdb(Uint32 aTableId);
  bool setTupleIdInNdb(const char* aTableName, Uint64 val, 
		       bool increase);
  bool setTupleIdInNdb(Uint32 aTableId, Uint64 val, bool increase);
  Uint64 opTupleIdOnNdb(Uint32 aTableId, Uint64 opValue, Uint32 op);
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   */
  NdbConnection* hupp( NdbConnection* );
  Uint32 getReference() const { return theMyRef;}
#endif

/*****************************************************************************
 *	These are service routines used by the other classes in the NDBAPI.
 ****************************************************************************/
private:
  
  NdbConnection*  startTransactionLocal(Uint32 aPrio, Uint32 aFragmentId); 

// Connect the connection object to the Database.
  int NDB_connect(Uint32 tNode);
  NdbConnection* doConnect(Uint32 nodeId); 
  void    doDisconnect();	 
  
  NdbReceiver*	        getNdbScanRec();// Get a NdbScanReceiver from idle list
  NdbLabel*		getNdbLabel();	// Get a NdbLabel from idle list
  NdbBranch*            getNdbBranch();	// Get a NdbBranch from idle list
  NdbSubroutine*	getNdbSubroutine();// Get a NdbSubroutine from idle
  NdbCall*		getNdbCall();	// Get a NdbCall from idle list
  NdbApiSignal*	        getSignal();	// Get an operation from idle list
  NdbRecAttr*	        getRecAttr();	// Get a receeive attribute object from
					// idle list of the Ndb object.
  NdbOperation* 	getOperation();	// Get an operation from idle list
  NdbIndexScanOperation* getScanOperation(); // Get a scan operation from idle
  NdbIndexOperation* 	getIndexOperation();// Get an index operation from idle

  class NdbGlobalEventBufferHandle* getGlobalEventBufferHandle();
  NdbBlob*              getNdbBlob();// Get a blob handle etc

  void			releaseSignal(NdbApiSignal* anApiSignal);
  void                  releaseSignalsInList(NdbApiSignal** pList);
  void			releaseNdbScanRec(NdbReceiver* aNdbScanRec);
  void			releaseNdbLabel(NdbLabel* anNdbLabel);
  void			releaseNdbBranch(NdbBranch* anNdbBranch);
  void			releaseNdbSubroutine(NdbSubroutine* anNdbSubroutine);
  void			releaseNdbCall(NdbCall* anNdbCall);
  void			releaseRecAttr (NdbRecAttr* aRecAttr);	
  void		 	releaseOperation(NdbOperation* anOperation);	
  void		 	releaseScanOperation(NdbIndexScanOperation*);
  void                  releaseNdbBlob(NdbBlob* aBlob);

  void                  check_send_timeout();
  void                  remove_sent_list(Uint32);
  Uint32                insert_completed_list(NdbConnection*);
  Uint32                insert_sent_list(NdbConnection*);

  // Handle a received signal. Used by both
  // synchronous and asynchronous interface
  void handleReceivedSignal(NdbApiSignal* anApiSignal, struct LinearSectionPtr ptr[3]);
  
  // Receive response signals
  int			receiveResponse(int waitTime = WAITFOR_RESPONSE_TIMEOUT);

  int			sendRecSignal(Uint16 aNodeId,
				      Uint32 aWaitState,
				      NdbApiSignal* aSignal,
                                      Uint32 nodeSequence);
  
  // Sets Restart GCI in Ndb object
  void			RestartGCI(int aRestartGCI);

  // Get block number of this NDBAPI object
  int			getBlockNumber();
  
  /****************************************************************************
   *	These are local service routines used by this class.	
   ***************************************************************************/
  
  int			createConIdleList(int aNrOfCon);
  int 		createOpIdleList( int nrOfOp );	

  void	freeOperation();          // Free the first idle operation.
  void	freeScanOperation();      // Free the first idle scan operation.
  void	freeIndexOperation();     // Free the first idle index operation.
  void	freeNdbCon();	// Free the first idle connection.
  void	freeSignal();	// Free the first idle signal	
  void	freeRecAttr();	// Free the first idle receive attr obj	
  void	freeNdbLabel();	// Free the first idle NdbLabel obj
  void	freeNdbBranch();// Free the first idle NdbBranch obj
  void	freeNdbSubroutine();// Free the first idle NdbSubroutine obj
  void	freeNdbCall();	    // Free the first idle NdbCall obj
  void	freeNdbScanRec();   // Free the first idle NdbScanRec obj
  void  freeNdbBlob();      // Free the first etc

  NdbConnection* getNdbCon();	// Get a connection from idle list
  
  /**
   * Get a connected NdbConnection to nodeId
   *   Returns NULL if none found
   */
  NdbConnection* getConnectedNdbConnection(Uint32 nodeId);

  // Release and disconnect from DBTC a connection
  // and seize it to theConIdleList
  void	releaseConnectToNdb (NdbConnection* aConnectConnection);

  // Release a connection to idle list
  void 	releaseNdbCon (NdbConnection* aConnection);
  
  int	checkInitState();		// Check that we are initialized
  void	report_node_failure(Uint32 node_id);           // Report Failed node
  void	report_node_failure_completed(Uint32 node_id); // Report Failed node(NF comp.)

  void	checkFailedNode();		// Check for failed nodes

  int   NDB_connect();     // Perform connect towards NDB Kernel

  // Release arrays of NdbConnection pointers
  void  releaseTransactionArrays();     

  Uint32  pollCompleted(NdbConnection** aCopyArray);
  void    sendPrepTrans(int forceSend);
  void    reportCallback(NdbConnection** aCopyArray, Uint32 aNoOfComplTrans);
  void    waitCompletedTransactions(int milliSecs, int noOfEventsToWaitFor);
  void    completedTransaction(NdbConnection* aTransaction);
  void    completedScanTransaction(NdbConnection* aTransaction);

  void    abortTransactionsAfterNodeFailure(Uint16 aNodeId);

  static
  const char * externalizeTableName(const char * internalTableName, bool fullyQualifiedNames);
  const char * externalizeTableName(const char * internalTableName);
  const char * internalizeTableName(const char * externalTableName);

  static
  const char * externalizeIndexName(const char * internalIndexName, bool fullyQualifiedNames);
  const char * externalizeIndexName(const char * internalIndexName);
  const char * internalizeIndexName(const NdbTableImpl * table,
				    const char * externalIndexName);

  static
  const BaseString getDatabaseFromInternalName(const char * internalName);
  static 
  const BaseString getSchemaFromInternalName(const char * internalName);

  void*              int2void     (Uint32 val);
  NdbReceiver*       void2rec     (void* val);
  NdbConnection*     void2con     (void* val);
  NdbOperation*      void2rec_op  (void* val);
  NdbIndexOperation* void2rec_iop (void* val);

/******************************************************************************
 *	These are the private variables in this class.	
 *****************************************************************************/
  NdbObjectIdMap*       theNdbObjectIdMap;

  NdbConnection**       thePreparedTransactionsArray;
  NdbConnection**       theSentTransactionsArray;
  NdbConnection**       theCompletedTransactionsArray;

  Uint32                theNoOfPreparedTransactions;
  Uint32                theNoOfSentTransactions;
  Uint32                theNoOfCompletedTransactions;
  Uint32                theNoOfAllocatedTransactions;
  Uint32                theMaxNoOfTransactions;
  Uint32                theMinNoOfEventsToWakeUp;

  Uint32                theNextConnectNode;

  NdbWaiter             theWaiter;
  
  bool fullyQualifiedNames;

  // Ndb database name.
  char                  theDataBase[NDB_MAX_DATABASE_NAME_SIZE];
  // Ndb database schema name.  
  char                  theDataBaseSchema[NDB_MAX_SCHEMA_NAME_SIZE];
  char                  prefixName[NDB_MAX_INTERNAL_TABLE_LENGTH];
  char *                prefixEnd;		

  //Table*       		theTable;	// The table object		
  class NdbImpl * theImpl;
  class NdbDictionaryImpl* theDictionary;
  class NdbGlobalEventBufferHandle* theGlobalEventBufferHandle;

  NdbConnection*	theConIdleList;	// First connection in idle list.

  NdbOperation*		theOpIdleList;	// First operation in the idle list. 

  NdbIndexScanOperation* theScanOpIdleList;	// First scan operation in the idle list. 
  NdbIndexOperation*	theIndexOpIdleList;	// First index operation in the idle list. 
  NdbConnection*	theTransactionList;
  NdbConnection**       theConnectionArray;
  NdbRecAttr*		theRecAttrIdleList;  
  NdbApiSignal*		theSignalIdleList;   // First signal in idlelist.
  NdbLabel*		theLabelList;	     // First label descriptor in list
  NdbBranch*		theBranchList;	     // First branch descriptor in list
  NdbSubroutine*	theSubroutineList;   // First subroutine descriptor in
  NdbCall*		theCallList;	     // First call descriptor in list
  NdbReceiver*      theScanList;
  NdbBlob*              theNdbBlobIdleList;

  Uint32   theMyRef;        // My block reference  
  Uint32   theNode;         // The node number of our node
  
  Uint32   theNoOfDBnodes;  // The number of DB nodes  
  Uint32 * theDBnodes;      // The node number of the DB nodes
  Uint8    *the_release_ind;// 1 indicates to release all connections to node 
  
  Uint64               the_last_check_time;
  Uint64               theFirstTransId;
  
  // The tupleId is retreived from DB the 
  // tupleId is unique for each tableid. 
  Uint64               theFirstTupleId[2048]; 
  Uint64               theLastTupleId[2048];           

  Uint32		theRestartGCI;	// the Restart GCI used by DIHNDBTAMPER
  
  NdbError              theError;

  Int32        	        theNdbBlockNumber;

  enum InitType {
    NotConstructed,
    NotInitialised,
    StartingInit,
    Initialised,
    InitConfigError
  } theInitState;

  // Ensure good distribution of connects
  Uint32		theCurrentConnectIndex;
  Uint32		theCurrentConnectCounter;
  
  /**
   * Computes fragement id for primary key
   *
   * Note that keydata has to be "shaped" as it is being sent in KEYINFO
   */
  Uint32 computeFragmentId(const char * keyData, Uint32 keyLen);
  Uint32 getFragmentId(Uint32 hashValue);
  
  /**
   * Make a guess to which node is the primary for the fragment
   */
  Uint32 guessPrimaryNode(Uint32 fragmentId);
  
  /**
   * Structure containing values for guessing primary node
   */
  struct StartTransactionNodeSelectionData {
    StartTransactionNodeSelectionData():
      fragment2PrimaryNodeMap(0) {};
    Uint32 kValue;
    Uint32 hashValueMask;
    Uint32 hashpointerValue;
    Uint32 noOfFragments;
    Uint32 * fragment2PrimaryNodeMap;
    
    void init(Uint32 noOfNodes, Uint32 nodeIds[]);
    void release();
  } startTransactionNodeSelectionData;
  
  NdbApiSignal* theCommitAckSignal;

  int cfreeSignals;
  int cnewSignals;
  int cgetSignals;
  int creleaseSignals;

  static void executeMessage(void*, NdbApiSignal *, 
			     struct LinearSectionPtr ptr[3]);
  static void statusMessage(void*, Uint16, bool, bool);
#ifdef VM_TRACE
  void printState(const char* fmt, ...);
#endif
};

#endif
