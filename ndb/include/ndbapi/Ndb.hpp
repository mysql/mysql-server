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

   This guide assumes a basic familiarity with MySQL Cluster concepts.
   Some of the fundamental ones are described in section @ref secConcepts.
   
   The <em>NDB API</em> is a MySQL Cluster application interface 
   that implements transactions.
   The NDB API consists of the following fundamental classes:
   - <code>Ndb_cluster_connection</code>, representing a connection to a cluster, 
   - <code>Ndb</code> is the main class, representing the database, 
   - <code>NdbTransaction</code> represents a transaction, 
   - <code>NdbOperation</code> represents an operation using a primary key,
   - <code>NdbScanOperation</code> represents an operation performing a full table scan.
   - <code>NdbIndexOperation</code> represents an operation using a unique hash index,
   - <code>NdbIndexScanOperation</code> represents an operation performing a scan using
     an ordered index,
   - <code>NdbRecAttr</code> represents an attribute value
   - <code>NdbDictionary</code> represents meta information about tables and attributes.
   - <code>NdbError</code> contains the specification for an error.
   There are also some auxiliary classes.
     
   The main structure of an application program is as follows:
   -# Construct and connect to a cluster using the <code>Ndb_cluster_connection</code>
      object.
   -# Construct and initialize <code>Ndb</code> object(s).
   -# Define and execute transactions using <code>NdbTransaction</code> and <code>Ndb*Operation</code>.
   -# Delete <code>Ndb</code> objects
   -# Delete cluster connection

   The main structure of a transaction is as follows:
   -# Start transaction (an <code>NdbTransaction</code>)
   -# Add and define operations associated with the transaction using
      <code>Ndb*Operation</code>
   -# Execute transaction

   The execution can be of two different types, 
   <var>Commit</var> or <var>NoCommit</var>.
   If the execution is of type <var>NoCommit</var>, 
   then the application program executes part of a transaction,
   but without committing the transaction.
   After executing a <var>NoCommit</var> transaction, the program can continue 
   to add and define more operations to the transaction
   for later execution.

   If the execute is of type <var>Commit</var>, then the transaction is
   committed, and no further addition or definition of operations 
   is allowed.


   @section secSync                     Synchronous Transactions
  
   Synchronous transactions are defined and executed as follows:
  
    -# Start (create) the transaction, which is
       referenced by an <code>NdbTransaction</code> object 
       (typically created using <code>Ndb::startTransaction()</code>).
       At this point, the transaction is only being defined,
       and is not yet sent to the NDB kernel.
    -# Define operations and add them to the transaction, 
       using <code>NdbTransaction::getNdb*Operation()</code> and
       methods of the <code>Ndb*Operation</code> class.
       Note that the transaction has still not yet been sent to the NDB kernel.
    -# Execute the transaction, using the <code>NdbTransaction::execute()</code> method.
    -# Close the transaction (using <code>Ndb::closeTransaction()</code>).
  
   For an example of this process, see the program listing in @ref ndbapi_example1.cpp.

   To execute several parallel synchronous transactions, one can either 
   use multiple <code>Ndb</code> objects in several threads, or start multiple 
   applications programs.  

   @section secNdbOperations            Operations

   Each <code>NdbTransaction</code>
   consists of a list of operations which are represented by instances 
   of <code>Ndb*Operation</code>.

   <h3>Single row operations</h3>
   After the operation is created using <code>NdbTransaction::getNdbOperation()</code>
   (or <code>NdbTransaction::getNdbIndexOperation()</code>), it is defined in the following 
   three steps:
   -# Define the standard operation type, using <code>NdbOperation::readTuple()</code>
   -# Specify search conditions, using <code>NdbOperation::equal()</code>
   -# Specify attribute actions, using <code>NdbOperation::getValue()</code>

   Here are two brief examples illustrating this process. For the sake of brevity, 
   we omit error-handling.
   
   This first example uses an <code>NdbOperation</code>:
   @code
     // 1. Create
     MyOperation= MyTransaction->getNdbOperation("MYTABLENAME");
    
     // 2. Define type of operation and lock mode
     MyOperation->readTuple(NdbOperation::LM_Read);

     // 3. Specify Search Conditions
     MyOperation->equal("ATTR1", i);
    
     // 4. Attribute Actions
     MyRecAttr= MyOperation->getValue("ATTR2", NULL);
   @endcode
   For additional examples of this sort, see @ref ndbapi_example1.cpp and 
   @ref ndbapi_example2.cpp.

   The second example uses an <code>NdbIndexOperation</code>:
   @code
     // 1. Create
     MyOperation= MyTransaction->getNdbIndexOperation("MYINDEX", "MYTABLENAME");

     // 2. Define type of operation and lock mode
     MyOperation->readTuple(NdbOperation::LM_Read);

     // 3. Specify Search Conditions
     MyOperation->equal("ATTR1", i);

     // 4. Attribute Actions 
     MyRecAttr = MyOperation->getValue("ATTR2", NULL);
   @endcode
   Another example of this second type can be found in @ref ndbapi_example4.cpp.

   We will now discuss in somewhat greater detail each step involved in the creation 
   and use of synchronous transactions.

  //  Edit stop point - JS, 20041228 0425+1000

   <h4>Step 1: Define single row operation type</h4>
   The following types of operations exist:
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
       reads an existing tuple with specified lock mode

   All of these operations operate on the unique tuple key.
   (When NdbIndexOperation is used then all of these operations 
   operate on a defined unique hash index.)

   @note If you want to define multiple operations within the same transaction,
         then you need to call NdbTransaction::getNdb*Operation for each
         operation.

   <h4>Step 2: Specify Search Conditions</h4>
   The search condition is used to select tuples.

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

   NdbOperation::getValue returns an NdbRecAttr object
   containing the read value.
   To get the value, there is actually two methods.
   The application can either
   - use its own memory (passed through a pointer aValue) to
     NdbOperation::getValue, or
   - receive the attribute value in an NdbRecAttr object allocated
     by the NDB API.

   The NdbRecAttr object is released when Ndb::closeTransaction
   is called.
   Thus, the application can not reference this object after
   Ndb::closeTransaction have been called.
   The result of reading data from an NdbRecAttr object before
   calling NdbTransaction::execute is undefined.


   @subsection secScan              Scan Operations 
   
   Scans are roughly the equivalent of SQL cursors.

   Scans can either be performed on a table (@ref NdbScanOperation) or 
   on an ordered index (@ref NdbIndexScanOperation).

   Scan operation are characteriesed by the following:
   - They can only perform reads (shared, exclusive or dirty)
   - They can potentially work with multiple rows
   - They can be used to update or delete multiple rows
   - They can operate on several nodes in parallell

   After the operation is created using <code>NdbTransaction::getNdbScanOperation()</code>
   (or <code>NdbTransaction::getNdbIndexScanOperation()</code>), it is defined in the following 
   three steps:
   -# Define the standard operation type, using <code>NdbScanOperation::readTuples()</code>
   -# Specify search conditions, using @ref NdbScanFilter and/or @ref NdbIndexScanOperation::setBound
   -# Specify attribute actions, using <code>NdbOperation::getValue()</code>
   -# Executing the transaction, using <code>NdbTransaction::execute()</code>
   -# Iterating through the result set using <code>NdbScanOperation::nextResult</code>

   Here are two brief examples illustrating this process. For the sake of brevity, 
   we omit error-handling.
   
   This first example uses an <code>NdbScanOperation</code>:
   @code
     // 1. Create
     MyOperation= MyTransaction->getNdbScanOperation("MYTABLENAME");
    
     // 2. Define type of operation and lock mode
     MyOperation->readTuples(NdbOperation::LM_Read);

     // 3. Specify Search Conditions
     NdbScanFilter sf(MyOperation);
     sf.begin(NdbScanFilter::OR);
     sf.eq(0, i);   // Return rows with column 0 equal to i or
     sf.eq(1, i+1); // column 1 equal to (i+1)
     sf.end();

     // 4. Attribute Actions
     MyRecAttr= MyOperation->getValue("ATTR2", NULL);
   @endcode

   The second example uses an <code>NdbIndexScanOperation</code>:
   @code
     // 1. Create
     MyOperation= MyTransaction->getNdbIndexScanOperation("MYORDEREDINDEX", "MYTABLENAME");

     // 2. Define type of operation and lock mode
     MyOperation->readTuples(NdbOperation::LM_Read);

     // 3. Specify Search Conditions
     // All rows with ATTR1 between i and (i+1)
     MyOperation->setBound("ATTR1", NdbIndexScanOperation::BoundGE, i);
     MyOperation->setBound("ATTR1", NdbIndexScanOperation::BoundLE, i+1);    

     // 4. Attribute Actions 
     MyRecAttr = MyOperation->getValue("ATTR2", NULL);
   @endcode

   <h4>Step 1: Define scan operation operation type</h4>
   Scan operations only support 1 operation, @ref NdbScanOperation::readTuples or @ref NdbIndexScanOperation::readTuples

   @note If you want to define multiple scan operations within the same transaction,
         then you need to call NdbTransaction::getNdb*ScanOperation for each
         operation.

   <h4>Step 2: Specify Search Conditions</h4>
   The search condition is used to select tuples.
   If no search condition is specified, the scan will return all rows
   in the table.

   Search condition can be @ref NdbScanFilter which can be used on both
   @ref NdbScanOperation and @ref NdbIndexScanOperation or bounds which
   can only be used on index scans, @ref NdbIndexScanOperation::setBound.
   An index scan can have both NdbScanFilter and bounds

   @note When NdbScanFilter is used each row is examined but maybe not 
   returned. But when using bounds, only rows within bounds will be examined.

   <h4>Step 3: Specify Attribute Actions</h4>

   Now it is time to define which attributes should be read.
   Normally the attribute is defined by its name but it is
   also possible to use the attribute identity to define the
   attribute.

   NdbOperation::getValue returns an NdbRecAttr object
   containing the read value.
   To get the value, there is actually two methods.
   The application can either
   - use its own memory (passed through a pointer aValue) to
     NdbOperation::getValue, or
   - receive the attribute value in an NdbRecAttr object allocated
     by the NDB API.

   The NdbRecAttr object is released when Ndb::closeTransaction
   is called.
   Thus, the application can not reference this object after
   Ndb::closeTransaction have been called.
   The result of reading data from an NdbRecAttr object before
   calling NdbTransaction::execute is undefined.

   <h3> Using Scan to update/delete </h3>
   Scanning can also be used to update/delete rows.
   This is performed by
   -# Scan using exclusive locks, NdbOperation::LM_Exclusive
   -# When iterating through the result set, for each row optionally call 
      either NdbScanOperation::updateCurrentTuple or 
      NdbScanOperation::deleteCurrentTuple
   -# If performing <code>NdbScanOperation::updateCurrentTuple</code>, 
      set new values on record using ordinary @ref NdbOperation::setValue.
      NdbOperation::equal should _not_ be called as the primary key is 
      retreived from the scan.

   @note that the actual update/delete will not be performed until next 
   NdbTransaction::execute (as with single row operations), 
   NdbTransaction::execute needs to be called before locks are released,
     see @ref secScanLocks

   <h4> Index scans specific features </h4> 
   The following features are available when performing an index scan
   - Scan subset of table using @ref NdbIndexScanOperation::setBound
   - Ordering result set ascending or descending, @ref NdbIndexScanOperation::readTuples
   - When using NdbIndexScanOperation::BoundEQ on distribution key 
     only fragment containing rows will be scanned.
   
   Rows are returned unordered unless sorted is set to true.
   @note When performing sorted scan, parameter parallelism to readTuples will
   be ignored and max parallelism will be used instead. 

   @subsection secScanLocks Lock handling with scans

   When scanning a table or an index potentially 
   a lot of records will be returned.

   But Ndb will only lock a batch of rows per fragment at a time.
   How many rows will be locked per fragment is controlled by the 
   <code>batch</code> parameter to @ref NdbScanOperation::readTuples.

   To let the application handle how locks are released 
   @ref NdbScanOperation::nextResult have a parameter <code>fetch_allow</code>.
   If NdbScanOperation::nextResult is called with fetch_allow = false, no
   locks may be released as result of the function call. Otherwise the locks
   for the current batch may be released.

   This example shows scan delete, handling locks in an efficient manner.
   For the sake of brevity, we omit error-handling.
   @code
     int check;

     // Outer loop for each batch of rows
     while((check = MyScanOperation->nextResult(true)) == 0)
     {
       do
       {
         // Inner loop for each row within batch
         MyScanOperation->deleteCurrentTuple();
       } while((check = MyScanOperation->nextResult(false) == 0));

       // When no more rows in batch, exeute all defined deletes       
       MyTransaction->execute(NoCommit);
     }
   @endcode

   See @ref ndbapi_scan.cpp for full example of scan.

   @section secError                    Error Handling

   Errors can occur when
   -# operations are being defined, or when the
   -# transaction is being executed.

   One recommended way to handle a transaction failure 
   (i.e. an error is reported) is to:
   -# Rollback transaction (NdbTransaction::execute with a special parameter)
   -# Close transaction
   -# Restart transaction (if the error was temporary)

   @note Transaction are not automatically closed when an error occur.

   Several errors can occur when a transaction holds multiple 
   operations which are simultaneously executed.
   In this case the application has to go through the operation
   objects and query for their NdbError objects to find out what really
   happened.

   NdbTransaction::getNdbErrorOperation returns a reference to the 
   operation causing the latest error.
   NdbTransaction::getNdbErrorLine delivers the method number of the 
   erroneous method in the operation.

   @code
     theTransaction = theNdb->startTransaction();
     theOperation = theTransaction->getNdbOperation("TEST_TABLE");
     if (theOperation == NULL) goto error;
     theOperation->readTuple(NdbOperation::LM_Read);
     theOperation->setValue("ATTR_1", at1);
     theOperation->setValue("ATTR_2", at1); //Here an error occurs
     theOperation->setValue("ATTR_3", at1);
     theOperation->setValue("ATTR_4", at1);
    
     if (theTransaction->execute(Commit) == -1) {
       errorLine = theTransaction->getNdbErrorLine();
       errorOperation = theTransaction->getNdbErrorOperation();
   @endcode

   Here errorLine will be 3 as the error occurred in the third method 
   on the operation object.
   Getting errorLine == 0 means that the error occurred when executing the 
   operations.
   Here errorOperation will be a pointer to the theOperation object.
   NdbTransaction::getNdbError will return the NdbError object 
   including holding information about the error.

   Since errors could have occurred even when a commit was reported,
   there is also a special method, NdbTransaction::commitStatus,
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
 * @page ndbapi_scan.cpp ndbapi_scan.cpp
 * @include ndbapi_scan.cpp
 */


/**
   @page secAdapt  Adaptive Send Algorithm

   At the time of "sending" the transaction 
   (using NdbTransaction::execute), the transactions 
   are in reality <em>not</em> immediately transfered to the NDB Kernel.  
   Instead, the "sent" transactions are only kept in a 
   special send list (buffer) in the Ndb object to which they belong.
   The adaptive send algorithm decides when transactions should
   be transfered to the NDB kernel.
  
   For each of these "sent" transactions, there are three 
   possible states:
   -# Waiting to be transferred to NDB Kernel.
   -# Has been transferred to the NDB Kernel and is currently 
      being processed.
   -# Has been transferred to the NDB Kernel and has 
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
   -# When calling NdbTransaction::execute synchronously or calling any 
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

   The default method is to select the transaction coordinator (TC) as being
   the "closest" DB node.  There is a heuristics for closeness based on
   the type of transporter connection. In order of closest first, we have
   SCI, SHM, TCP/IP (localhost), and TCP/IP (remote host). If there are several
   connections available with the same "closeness", they will each be 
   selected in a round robin fashion for every transaction. Optionally
   one may set the methos for  TC selection round robin over all available
   connections, where each new set of transactions
   is placed on the next DB node.
   
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
   

   @subsection secKeys               Primary Keys
   Each record has from 1 up to 32 attributes which belong
   to the primary key of the table.
   
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
   applications)
   sometimes deadlock when they try to access the same information.
   Applications need to be programmed so that timeout errors
   occurring due to deadlocks are handled.  This generally
   means that the transaction encountering timeout
   should be rolled back and restarted.


   @section secHint                 Hints and performance

   Placing the transaction coordinator close
   to the actual data used in the transaction can in many cases
   improve performance significantly. This is particularly true for
   systems using TCP/IP. A system using Solaris and a 500 MHz processor
   has a cost model for TCP/IP communication which is:

     30 microseconds + (100 nanoseconds * no of Bytes)

   This means that if we can ensure that we use "popular" links we increase
   buffering and thus drastically reduce the communication cost.
   Systems using SCI has a different cost model which is:

     5 microseconds + (10 nanoseconds *  no of Bytes)

   Thus SCI systems are much less dependent on selection of 
   transaction coordinators. 
   Typically TCP/IP systems spend 30-60% of the time during communication,
   whereas SCI systems typically spend 5-10% of the time during
   communication. 
   Thus SCI means that less care from the NDB API programmer is
   needed and great scalability can be achieved even for applications using
   data from many parts of the database.

   A simple example is an application that uses many simple updates where
   a transaction needs to update one record. 
   This record has a 32 bit primary key, 
   which is also the distribution key. 
   Then the keyData will be the address of the integer 
   of the primary key and keyLen will be 4.
*/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
   (A transaction's execution can also be divided into three 
   steps: prepare, send, and poll. This allows us to perform asynchronous
   transactions.  More about this later.)
*/
#endif
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
   Another way to execute several parallel transactions is to use
   asynchronous transactions.
*/
#endif  
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
   Operations are of two different kinds:
   -# standard operations, and
   -# interpreted program operations.
*/
#endif
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
   <h3>Interpreted Program Operations</h3>
   The following types of interpreted program operations exist:
    -# NdbOperation::interpretedUpdateTuple :
       updates a tuple using an interpreted program
    -# NdbOperation::interpretedDeleteTuple :
       delete a tuple using an interpreted program

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
*/
#endif
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
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
*/
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
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

   The synchronous call to NdbTransaction::execute 
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

   The asynchronous method NdbTransaction::executeAsynchPrepare 
   only perform step 1.
   (The abort part in step 1 is only prepared for.  The actual 
   aborting of the transaction is performed in a later step.)

   Asynchronous transactions are defined and executed 
   in the following way.
   -# Start (create) transactions (same way as for the 
       synchronous transactions)
   -# Add and define operations (also as in the synchronous case)
   -# <b>Prepare</b> transactions 
       (using NdbTransaction::executeAsynchPrepare or 
       NdbTransaction::executeAsynch)
   -# <b>Send</b> transactions to NDB Kernel
       (using Ndb::sendPreparedTransactions, 
       NdbTransaction::executeAsynch, or Ndb::sendPollNdb)
   -# <b>Poll</b> NDB kernel to find completed transactions 
       (using Ndb::pollNdb or Ndb::sendPollNdb)
   -# Close transactions (same way as for the synchronous transactions)

   See example program in section @ref ndbapi_example2.cpp.
   
   This prepare-send-poll protocol actually exists in four variants:
   - (Prepare-Send-Poll).  This is the one-step variant provided
     by synchronous transactions.
   - (Prepare-Send)-Poll.  This is the two-step variant using
     NdbTransaction::executeAsynch and Ndb::pollNdb.
   - Prepare-(Send-Poll).  This is the two-step variant using
     NdbTransaction::executeAsynchPrepare and Ndb::sendPollNdb.
   - Prepare-Send-Poll.  This is the three-step variant using
     NdbTransaction::executeAsynchPrepare, Ndb::sendPreparedTransactions, and
     Ndb::pollNdb.
  
   Transactions first has to be prepared by using method
   NdbTransaction::executeAsynchPrepare or NdbTransaction::executeAsynch.
   The difference between these is that 
   NdbTransaction::executeAsynch also sends the transaction to 
   the NDB kernel.
   One of the arguments to these methods is a callback method.
   The callback method is executed during polling (item 5 above).
  
   Note that NdbTransaction::executeAsynchPrepare does not 
   send the transaction to the NDB kernel.  When using 
   NdbTransaction::executeAsynchPrepare, you either have to call 
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
	 NdbTransaction::executeAsynch or through the combination of
         NdbTransaction::executeAsynchPrepare and either
         Ndb::sendPreparedTransactions or Ndb::sendPollNdb).

   More about how transactions are sent the NDB Kernel is 
   available in section @ref secAdapt.
*/
#endif


/**
   
   Put this back when real array ops are supported
   i.e. get/setValue("kalle[3]");

   @subsection secArrays             Array Attributes
   A table attribute in NDB Cluster can be of <em>array type</em>.
   This means that the attribute consists of an array of 
   <em>elements</em>.  The <em>attribute size</em> is the size
   of one element of the array (expressed in bits) and the 
   <em>array size</em> is the number of elements of the array.

*/

#ifndef Ndb_H
#define Ndb_H

#include <ndb_types.h>
#include <ndbapi_limits.h>
#include <ndb_cluster_connection.hpp>
#include <NdbError.hpp>
#include <NdbDictionary.hpp>

class NdbObjectIdMap;
class NdbOperation;
class NdbEventOperationImpl;
class NdbScanOperation;
class NdbIndexScanOperation;
class NdbIndexOperation;
class NdbTransaction;
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
class NdbReceiver;

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
 */

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
// to be documented later
/*
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
#endif

class Ndb
{
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class NdbReceiver;
  friend class NdbOperation;
  friend class NdbEventOperationImpl;
  friend class NdbTransaction;
  friend class Table;
  friend class NdbApiSignal;
  friend class NdbIndexOperation;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class NdbDictionaryImpl;
  friend class NdbDictInterface;
  friend class NdbBlob;
#endif

public:
  /** 
   * @name General 
   * @{
   */
  /**
   * The Ndb object represents a connection to a database.
   *
   * @note the init() method must be called before it may be used
   *
   * @param ndb_cluster_connection is a connection to a cluster containing
   *        the database to be used
   * @param aCatalogName is the name of the catalog you want to use.
   * @note The catalog name provides a name space for the tables and
   *       indexes created in any connection from the Ndb object.
   * @param aSchemaName is the name of the schema you 
   *        want to use.
   * @note The schema name provides an additional name space 
   *       for the tables and indexes created in a given catalog.
   */
  Ndb(Ndb_cluster_connection *ndb_cluster_connection,
      const char* aCatalogName = "", const char* aSchemaName = "def");

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  Ndb(const char* aCatalogName = "", const char* aSchemaName = "def");
#endif
  ~Ndb();

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
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
#endif

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
   * Initializes the Ndb object
   *
   * @param  maxNoOfTransactions 
   *         Maximum number of parallel 
   *         NdbTransaction objects that can be handled by the Ndb object.
   *         Maximum value is 1024.
   *
   * @note each scan or index scan operation uses one extra
   *       NdbTransaction object
   *
   * @return 0 if successful, -1 otherwise.
   */
  int init(int maxNoOfTransactions = 4);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
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
#endif

  /** @} *********************************************************************/

  /** 
   * @name Meta Information
   * @{
   */

  /**
   * Get an object for retrieving or manipulating database schema information 
   *
   * @note this object operates outside any transaction
   *
   * @return Object containing meta information about all tables 
   *         in NDB Cluster.
   */
  class NdbDictionary::Dictionary* getDictionary() const;
  

  /** @} *********************************************************************/

  /** 
   * @name Event subscriptions
   * @{
   */

  /**
   * Create a subcription to an event defined in the database
   *
   * @param eventName
   *        unique identifier of the event
   * @param bufferLength
   *        buffer size for storing event data
   *
   * @return Object representing an event, NULL on failure
   */
  NdbEventOperation* createEventOperation(const char* eventName,
					  const int bufferLength);
  /**
   * Drop a subscription to an event
   *
   * @param eventName
   *        unique identifier of the event
   *
   * @return 0 on success
   */
  int dropEventOperation(NdbEventOperation* eventName);

  /**
   * Wait for an event to occur. Will return as soon as an event
   * is detected on any of the created events.
   *
   * @param aMillisecondNumber
   *        maximum time to wait
   *
   * @return the number of events that has occured, -1 on failure
   */
  int pollEvents(int aMillisecondNumber);

  /** @} *********************************************************************/

  /** 
   * @name Starting and Closing Transactions
   * @{
   */

  /**
   * Start a transaction
   *
   * @note When the transaction is completed it must be closed using
   *       Ndb::closeTransaction or NdbTransaction::close. 
   *       The transaction must be closed independent of its outcome, i.e.
   *       even if there is an error.
   *
   * @param  prio     Not implemented
   * @param  keyData  Pointer to partition key to be used for deciding
   *                  which node to run the Transaction Coordinator on
   * @param  keyLen   Length of partition key expressed in bytes
   * 
   * @return NdbTransaction object, or NULL on failure.
   */
  NdbTransaction* startTransaction(Uint32        prio = 0, 
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
   * @return NdbTransaction, or NULL if it failed.
   */
  NdbTransaction* startTransactionDGroup(Uint32 aPrio, 
					const char * keyData, int type);
#endif

  /**
   * Close a transaction.
   *
   * @note should be called after the transaction has completed, irrespective
   *       of success or failure
   */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * @note It is not allowed to call Ndb::closeTransaction after sending the
   *       transaction asynchronously with either 
   *       Ndb::sendPreparedTransactions or
   *       Ndb::sendPollNdb before the callback method has been called.
   *       (The application should keep track of the number of 
   *       outstanding transactions and wait until all of them 
   *       has completed before calling Ndb::closeTransaction).
   *       If the transaction is not committed it will be aborted.
   */
#endif
  void closeTransaction(NdbTransaction*);

  /** @} *********************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  // to be documented later
  /** 
   * @name Asynchronous Transactions
   * @{
   */

  /**
   * Wait for prepared transactions.
   * Will return as soon as at least 'minNoOfEventsToWakeUp' 
   * of them have completed, or the maximum time given as timeout has passed.
   *
   * @param aMillisecondNumber 
   *        Maximum time to wait for transactions to complete. Polling 
   *        without wait is achieved by setting the timer to zero.
   *        Time is expressed in milliseconds.
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
#endif
  
  /** @} *********************************************************************/

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get the NdbError object
   *
   * @note The NdbError object is valid until a new NDB API method is called.
   */
  const NdbError & getNdbError() const;
  
  /**
   * Get a NdbError object for a specific error code
   *
   * The NdbError object is valid until you call a new NDB API method.
   */
  const NdbError & getNdbError(int errorCode);


  /** @} *********************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Get the application node identity.  
   *
   * @return Node id of this application.
   */
  int getNodeId();

  /**
   * setConnectString
   *
   * @param connectString - see MySQL ref manual for format
   */
  static void setConnectString(const char * connectString);

  bool usingFullyQualifiedNames();

  /**
   * Different types of tampering with the NDB Cluster.
   * <b>Only for debugging purposes only.</b>
   */
  enum TamperType	{ 
    LockGlbChp = 1,           ///< Lock GCP
    UnlockGlbChp,             ///< Unlock GCP
    CrashNode,                ///< Crash an NDB node
    ReadRestartGCI,           ///< Request the restart GCI id from NDB Cluster
    InsertError               ///< Execute an error in NDB Cluster 
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
  Uint64 getAutoIncrementValue(const NdbDictionary::Table * aTable, 
			       Uint32 cacheSize = 1);
  Uint64 readAutoIncrementValue(const char* aTableName);
  Uint64 readAutoIncrementValue(const NdbDictionary::Table * aTable);
  bool setAutoIncrementValue(const char* aTableName, Uint64 val, 
			     bool increase = false);
  bool setAutoIncrementValue(const NdbDictionary::Table * aTable, Uint64 val, 
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

  /**
   */
  NdbTransaction* hupp( NdbTransaction* );
  Uint32 getReference() const { return theMyRef;}
#endif

/*****************************************************************************
 *	These are service routines used by the other classes in the NDBAPI.
 ****************************************************************************/
private:
  
  void setup(Ndb_cluster_connection *ndb_cluster_connection,
	     const char* aCatalogName, const char* aSchemaName);

  void connected(Uint32 block_reference);
 

  NdbTransaction*  startTransactionLocal(Uint32 aPrio, Uint32 aFragmentId); 

// Connect the connection object to the Database.
  int NDB_connect(Uint32 tNode);
  NdbTransaction* doConnect(Uint32 nodeId); 
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
  Uint32                insert_completed_list(NdbTransaction*);
  Uint32                insert_sent_list(NdbTransaction*);

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

  NdbTransaction* getNdbCon();	// Get a connection from idle list
  
  /**
   * Get a connected NdbTransaction to nodeId
   *   Returns NULL if none found
   */
  NdbTransaction* getConnectedNdbTransaction(Uint32 nodeId);

  // Release and disconnect from DBTC a connection
  // and seize it to theConIdleList
  void	releaseConnectToNdb (NdbTransaction*);

  // Release a connection to idle list
  void 	releaseNdbCon (NdbTransaction*);
  
  int	checkInitState();		// Check that we are initialized
  void	report_node_failure(Uint32 node_id);           // Report Failed node
  void	report_node_failure_completed(Uint32 node_id); // Report Failed node(NF comp.)

  void	checkFailedNode();		// Check for failed nodes

  int   NDB_connect();     // Perform connect towards NDB Kernel

  // Release arrays of NdbTransaction pointers
  void  releaseTransactionArrays();     

  Uint32  pollCompleted(NdbTransaction** aCopyArray);
  void    sendPrepTrans(int forceSend);
  void    reportCallback(NdbTransaction** aCopyArray, Uint32 aNoOfComplTrans);
  void    waitCompletedTransactions(int milliSecs, int noOfEventsToWaitFor);
  void    completedTransaction(NdbTransaction* aTransaction);
  void    completedScanTransaction(NdbTransaction* aTransaction);

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
  NdbTransaction*     void2con     (void* val);
  NdbOperation*      void2rec_op  (void* val);
  NdbIndexOperation* void2rec_iop (void* val);

/******************************************************************************
 *	These are the private variables in this class.	
 *****************************************************************************/
  NdbObjectIdMap*       theNdbObjectIdMap;
  Ndb_cluster_connection   *m_ndb_cluster_connection;

  NdbTransaction**       thePreparedTransactionsArray;
  NdbTransaction**       theSentTransactionsArray;
  NdbTransaction**       theCompletedTransactionsArray;

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

  class NdbImpl * theImpl;
  class NdbDictionaryImpl* theDictionary;
  class NdbGlobalEventBufferHandle* theGlobalEventBufferHandle;

  NdbTransaction*	theConIdleList;	// First connection in idle list.

  NdbOperation*		theOpIdleList;	// First operation in the idle list. 

  NdbIndexScanOperation* theScanOpIdleList;	// First scan operation in the idle list. 
  NdbIndexOperation*	theIndexOpIdleList;	// First index operation in the idle list. 
  NdbTransaction*	theTransactionList;
  NdbTransaction**      theConnectionArray;
  NdbRecAttr*		theRecAttrIdleList;  
  NdbApiSignal*		theSignalIdleList;   // First signal in idlelist.
  NdbLabel*		theLabelList;	     // First label descriptor in list
  NdbBranch*		theBranchList;	     // First branch descriptor in list
  NdbSubroutine*	theSubroutineList;   // First subroutine descriptor in
  NdbCall*		theCallList;	     // First call descriptor in list
  NdbReceiver*          theScanList;
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


#ifdef POORMANSPURIFY
  int cfreeSignals;
  int cnewSignals;
  int cgetSignals;
  int creleaseSignals;
#endif

  static void executeMessage(void*, NdbApiSignal *, 
			     struct LinearSectionPtr ptr[3]);
  static void statusMessage(void*, Uint32, bool, bool);
#ifdef VM_TRACE
  void printState(const char* fmt, ...);
#endif
};

#endif
