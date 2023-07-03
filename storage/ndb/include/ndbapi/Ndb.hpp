/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

/**
   @mainpage                            NDB API Programmers' Guide

   This guide assumes a basic familiarity with MySQL Cluster concepts found
   in the MySQL Cluster documentation.
   Some of the fundamental ones are also described in section @ref secConcepts.

   The NDB API is a MySQL Cluster application interface
   that implements transactions.
   The NDB API consists of the following fundamental classes:
   - Ndb_cluster_connection, representing a connection to a cluster,
   - Ndb is the main class, representing a connection to a database,
   - NdbTransaction represents a transaction,
   - NdbOperation represents an operation using a primary key,
   - NdbScanOperation represents an operation performing a full table scan.
   - NdbIndexOperation represents an operation using a unique hash index,
   - NdbIndexScanOperation represents an operation performing a scan using
     an ordered index,
   - NdbRecAttr represents an attribute value
   - NdbRecord represents a memory layout of a row data for a particular table
   - NdbDictionary represents meta information about tables and attributes.

   In addition, the NDB API defines a structure NdbError, which contains the
   specification for an error.

   It is also possible to receive "events" triggered when data in the database in changed.
   This is done through the NdbEventOperation class.

   There are also some auxiliary classes, which are listed in the class hierarchy.

   The main structure of an application program is as follows:
   -# Connect to a cluster using the Ndb_cluster_connection
      object.
   -# Initiate a database connection by constructing and initialising one or more Ndb objects.
   -# Define and execute transactions using the NdbTransaction class.
   -# Delete Ndb objects.
   -# Terminate the connection to the cluster (terminate instance of Ndb_cluster_connection).

   The procedure for using transactions is as follows:
   -# Start transaction (instantiate an NdbTransaction object)
   -# Add and define operations associated with the transaction using instances of one or more of the
      NdbOperation, NdbScanOperation, NdbIndexOperation, and NdbIndexScanOperation classes
   -# Execute transaction (call NdbTransaction::execute())

   The operation can be of two different types,
   <var>Commit</var> or <var>NoCommit</var>.
   If the operation is of type <var>NoCommit</var>,
   then the application program executes the operation part of a transaction,
   but without actually committing the transaction.
   After executing a <var>NoCommit</var> operation, the program can continue
   to add and define more operations to the transaction
   for later execution.

   If the operation is of type <var>Commit</var>, then the transaction is
   immediately committed. The transaction <em>must</em> be closed after it has been
   committed (even if commit fails), and no further addition or definition of
   operations for this transaction is allowed.

   @section secSync                     Synchronous Transactions

   Synchronous transactions are defined and executed as follows:

    -# Start (create) the transaction, which is
       referenced by an NdbTransaction object
       (typically created using Ndb::startTransaction()).
       At this point, the transaction is only being defined,
       and is not yet sent to the NDB kernel.
    -# Define operations and add them to the transaction, using one or more of
       - NdbTransaction::getNdbOperation()
       - NdbTransaction::getNdbScanOperation()
       - NdbTransaction::getNdbIndexOperation()
       - NdbTransaction::getNdbIndexScanOperation()
       - NdbTransaction::readTuple()
       - NdbTransaction::insertTuple()
       - NdbTransaction::updateTuple()
       - NdbTransaction::writeTuple()
       - NdbTransaction::deleteTuple()
       - NdbTransaction::scanTable()
       - NdbTransaction::scanIndex()
       along with the appropriate methods of the respective NdbOperation class
       (or possibly one or more of its subclasses).
       Note that the transaction has still not yet been sent to the NDB kernel.
    -# Execute the transaction, using the NdbTransaction::execute() method.
    -# Close the transaction (call Ndb::closeTransaction()).

   For an example of this process, see the program listing in
   @ref ndbapi_simple.cpp.

   To execute several parallel synchronous transactions, one can either
   use multiple Ndb objects in several threads, or start multiple
   application programs.

   @section secNdbOperations            Operations

   A NdbTransaction consists of a list of operations, each of which is represented
   by an instance of NdbOperation, NdbScanOperation, NdbIndexOperation, or
   NdbIndexScanOperation.

   <h3>Single row operations</h3>
   After the operation is created using NdbTransaction::getNdbOperation()
   (or NdbTransaction::getNdbIndexOperation()), it is defined in the following
   three steps:
   -# Define the standard operation type, using NdbOperation::readTuple()
   -# Specify search conditions, using NdbOperation::equal()
   -# Specify attribute actions, using NdbOperation::getValue()

   Here are two brief examples illustrating this process. For the sake of
   brevity, we omit error handling.

   This first example uses an NdbOperation:
   @code
     // 1. Retrieve table object
     myTable= myDict->getTable("MYTABLENAME");

     // 2. Create
     myOperation= myTransaction->getNdbOperation(myTable);

     // 3. Define type of operation and lock mode
     myOperation->readTuple(NdbOperation::LM_Read);

     // 4. Specify Search Conditions
     myOperation->equal("ATTR1", i);

     // 5. Attribute Actions
     myRecAttr= myOperation->getValue("ATTR2", NULL);
   @endcode
   For additional examples of this sort, see @ref ndbapi_simple.cpp.

   The second example uses an NdbIndexOperation:
   @code
     // 1. Retrieve index object
     myIndex= myDict->getIndex("MYINDEX", "MYTABLENAME");

     // 2. Create
     myOperation= myTransaction->getNdbIndexOperation(myIndex);

     // 3. Define type of operation and lock mode
     myOperation->readTuple(NdbOperation::LM_Read);

     // 4. Specify Search Conditions
     myOperation->equal("ATTR1", i);

     // 5. Attribute Actions
     myRecAttr = myOperation->getValue("ATTR2", NULL);
   @endcode
   Another example of this second type can be found in
   @ref ndbapi_simple_index.cpp.

   We will now discuss in somewhat greater detail each step involved in the
   creation and use of synchronous transactions.

   <h4>Step 1: Define single row operation type</h4>
   The following operation types are supported:
    -# NdbOperation::insertTuple() :
       inserts a non-existing tuple
    -# NdbOperation::writeTuple() :
       updates an existing tuple if is exists,
       otherwise inserts a new tuple
    -# NdbOperation::updateTuple() :
       updates an existing tuple
    -# NdbOperation::deleteTuple() :
       deletes an existing tuple
    -# NdbOperation::readTuple() :
       reads an existing tuple with specified lock mode

   All of these operations operate on the unique tuple key.
   (When NdbIndexOperation is used then all of these operations
   operate on a defined unique hash index.)

   @note If you want to define multiple operations within the same transaction,
         then you need to call NdbTransaction::getNdbOperation() or
	 NdbTransaction::getNdbIndexOperation() for each operation.

   <h4>Step 2: Specify Search Conditions</h4>
   The search condition is used to select tuples. Search conditions are set using NdbOperation::equal().

   <h4>Step 3: Specify Attribute Actions</h4>
   Next, it is necessary to determine which attributes should be read or updated.
   It is important to remember that:
   - Deletes can neither read nor set values, but only delete them
   - Reads can only read values
   - Updates can only set values
   Normally the attribute is identified by name, but it is
   also possible to use the attribute's identity to determine the
   attribute.

   NdbOperation::getValue() returns an NdbRecAttr object
   containing the read value.
   To obtain the actual value, one of two methods can be used;
   the application can either
   - use its own memory (passed through a pointer aValue) to
     NdbOperation::getValue(), or
   - receive the attribute value in an NdbRecAttr object allocated
     by the NDB API.

   The NdbRecAttr object is released when Ndb::closeTransaction()
   is called.
   Thus, the application cannot reference this object following
   any subsequent call to Ndb::closeTransaction().
   Attempting to read data from an NdbRecAttr object before
   calling NdbTransaction::execute() yields an undefined result.


   @subsection secScan              Scan Operations

   Scans are roughly the equivalent of SQL cursors, providing a means to
   perform high-speed row processing. A scan can be performed
   on either a table (using @ref NdbScanOperation) or
   an ordered index (by means of an @ref NdbIndexScanOperation).

   Scan operations are characterised by the following:
   - They can perform only reads (shared, exclusive or dirty)
   - They can potentially work with multiple rows
   - They can be used to update or delete multiple rows
   - They can operate on several nodes in parallel

   After the operation is created using NdbTransaction::getNdbScanOperation()
   (or NdbTransaction::getNdbIndexScanOperation()),
   it is carried out in the following three steps:
   -# Define the standard operation type, using NdbScanOperation::readTuples()
   -# Specify search conditions, using @ref NdbScanFilter and/or
      @ref NdbIndexScanOperation::setBound()
   -# Specify attribute actions, using NdbOperation::getValue()
   -# Executing the transaction, using NdbTransaction::execute()
   -# Traversing the result set by means of succssive calls to
      NdbScanOperation::nextResult()

   Here are two brief examples illustrating this process. Once again, in order
   to keep things relatively short and simple, we will forego any error handling.

   This first example performs a table scan, using an NdbScanOperation:
   @code
     // 1. Retrieve table object
     myTable= myDict->getTable("MYTABLENAME");

     // 2. Create
     myOperation= myTransaction->getNdbScanOperation(myTable);

     // 3. Define type of operation and lock mode
     myOperation->readTuples(NdbOperation::LM_Read);

     // 4. Specify Search Conditions
     NdbScanFilter sf(myOperation);
     sf.begin(NdbScanFilter::OR);
     sf.eq(0, i);   // Return rows with column 0 equal to i or
     sf.eq(1, i+1); // column 1 equal to (i+1)
     sf.end();

     // 5. Attribute Actions
     myRecAttr= myOperation->getValue("ATTR2", NULL);
   @endcode

   Our second example uses an NdbIndexScanOperation to perform an index scan:
   @code
     // 1. Retrieve index object
     myIndex= myDict->getIndex("MYORDEREDINDEX", "MYTABLENAME");

     // 2. Create
     myOperation= myTransaction->getNdbIndexScanOperation(myIndex);

     // 3. Define type of operation and lock mode
     myOperation->readTuples(NdbOperation::LM_Read);

     // 4. Specify Search Conditions
     // All rows with ATTR1 between i and (i+1)
     myOperation->setBound("ATTR1", NdbIndexScanOperation::BoundGE, i);
     myOperation->setBound("ATTR1", NdbIndexScanOperation::BoundLE, i+1);

     // 5. Attribute Actions
     myRecAttr = MyOperation->getValue("ATTR2", NULL);
   @endcode

   Some additional discussion of each step required to perform a scan follows:

   <h4>Step 1: Define Scan Operation Type</h4>
   It is important to remember that only a single operation is supported for each scan operation
   (@ref NdbScanOperation::readTuples() or @ref NdbIndexScanOperation::readTuples()).

   @note If you want to define multiple scan operations within the same
         transaction, then you need to call
	 NdbTransaction::getNdbScanOperation() or
	 NdbTransaction::getNdbIndexScanOperation() separately for <b>each</b> operation.

   <h4>Step 2: Specify Search Conditions</h4>
   The search condition is used to select tuples.
   If no search condition is specified, the scan will return all rows
   in the table.

   The search condition can be an @ref NdbScanFilter (which can be used on both
   @ref NdbScanOperation and @ref NdbIndexScanOperation) or bounds which
   can only be used on index scans (@ref NdbIndexScanOperation::setBound()).
   An index scan can use both NdbScanFilter and bounds.

   @note When NdbScanFilter is used, each row is examined, whether or not it is
   actually returned. However, when using bounds, only rows within the bounds will be examined.

   <h4>Step 3: Specify Attribute Actions</h4>

   Next, it is necessary to define which attributes should be read.
   As with transaction attributes, scan attributes are defined by name but it is
   also possible to use the attributes' identities to define attributes.

   As previously discussed (see @ref secSync), the value read is returned as
   an NdbRecAttr object by the NdbOperation::getValue() method.

   <h3>Using Scan to Update/Delete</h3>
   Scanning can also be used to update or delete rows.
   This is performed by
   -# Scanning using exclusive locks (using NdbOperation::LM_Exclusive)
   -# When iterating through the result set, for each row optionally calling
      either NdbScanOperation::updateCurrentTuple() or
      NdbScanOperation::deleteCurrentTuple()
   -# (If performing NdbScanOperation::updateCurrentTuple():)
      Setting new values for records simply by using @ref NdbOperation::setValue()
      (on the new NdbOperation object returned from updateCurrentTuple()).
      NdbOperation::equal() should <em>not</em> be called in such cases, as the primary
      key is retrieved from the scan.

   @note The actual update or delete will not be performed until the next
   call to NdbTransaction::execute(), just as with single row operations.
   NdbTransaction::execute() also must be called before any locks are released;
   see @ref secScanLocks for more information.

   <h4>Features Specific to Index Scans</h4>

   When performing an index scan, it is possible to
   scan only a subset of a table using @ref NdbIndexScanOperation::setBound().
   In addition, result sets can be sorted in either ascending or descending order, using
   @ref NdbIndexScanOperation::readTuples(). Note that rows are returned unordered
   by default, that is, unless <var>sorted</var> is set to <b>true</b>.
   It is also important to note that, when using NdbIndexScanOperation::BoundEQ
   on a partition key, only fragments containing rows will actually be scanned.

   @note When performing a sorted scan, any value passed as the
   NdbIndexScanOperation::readTuples() method's <code>parallel</code> argument
   will be ignored and maximum parallelism will be used instead. In other words, all
   fragments which it is possible to scan will be scanned simultaneously and in parallel
   in such cases.

   @subsection secScanLocks Lock handling with scans

   Performing scans on either a table or an index has the potential  to
   return a great many records; however, Ndb will lock only a predetermined
   number of rows per fragment at a time.
   How many rows will be locked per fragment is controlled by the
   <var>batch</var> parameter passed to NdbScanOperation::readTuples().

   In order to allow the application to handle how locks are released,
   NdbScanOperation::nextResult() has a Boolean parameter <var>fetch_allow</var>.
   If NdbScanOperation::nextResult() is called with <var>fetch_allow</var> equal to
   <b>false</b>, then no locks may be released as result of the function call.
   Otherwise the locks for the current batch may be released.

   This next example shows a scan delete that handle locks in an efficient manner.
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
       } while((check = MyScanOperation->nextResult(false)) == 0);

       // When no more rows in batch, exeute all defined deletes
       MyTransaction->execute(NoCommit);
     }
   @endcode

   See @ref ndbapi_scan.cpp for a more complete example of a scan.

   @section secError                    Error Handling

   Errors can occur either when operations making up a transaction are being
   defined, or when the transaction is actually being executed. Catching and
   handling either sort of error requires testing the value returned by
   NdbTransaction::execute(), and then, if an error is indicated (that is,
   if this value is equal to -1), using the following two methods in order to
   identify the error's type and location:

   - NdbTransaction::getNdbErrorOperation() returns a reference to the
     operation causing the most recent error.
   - NdbTransaction::getNdbErrorLine() yields the method number of the
     erroneous method in the operation.

   This short example illustrates how to detect an error and to use these
   two methods to identify it:

   @code
     theTransaction = theNdb->startTransaction();
     theOperation = theTransaction->getNdbOperation("TEST_TABLE");
     if (theOperation == NULL) goto error;
     theOperation->readTuple(NdbOperation::LM_Read);
     theOperation->setValue("ATTR_1", at1);
     theOperation->setValue("ATTR_2", at1);  //  Error occurs here
     theOperation->setValue("ATTR_3", at1);
     theOperation->setValue("ATTR_4", at1);

     if (theTransaction->execute(Commit) == -1) {
       errorLine = theTransaction->getNdbErrorLine();
       errorOperation = theTransaction->getNdbErrorOperation();
     }
   @endcode

   Here <code>errorLine</code> will be 3, as the error occurred in the
   third method called on the NdbOperation object (in this case,
   <code>theOperation</code>); if the result of
   NdbTransaction::getNdbErrorLine() is 0, this means that the error
   occurred when the operations were executed. In this example,
   <code>errorOperation</code> will be a pointer to the <code>theOperation</code>
   object. The NdbTransaction::getNdbError() method returns an NdbError
   object providing information about the error.

   @note Transactions are <b>not</b> automatically closed when an error occurs. Call
   Ndb::closeTransaction() to close the transaction.

   One recommended way to handle a transaction failure
   (i.e. an error is reported) is to:
   -# Rollback transaction (call NdbTransaction::execute() with a special parameter)
   -# Close transaction (call NdbTransaction::closeTransaction())
   -# If the error was temporary, attempt to restart the transaction

   Several errors can occur when a transaction contains multiple
   operations which are simultaneously executed.
   In this case the application has to go through all operations
   and query their NdbError objects to find out what really happened.

   It is also important to note that errors can occur even when a commit is
   reported as successful. In order to handle such situations, the NDB API
   provides an additional NdbTransaction::commitStatus() method to check the
   transactions's commit status.

******************************************************************************/

/**
 * @page ndbapi_simple.cpp ndbapi_simple.cpp
 * @include ndbapi_simple.cpp
 */

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
 * @page ndbapi_async.cpp ndbapi_async.cpp
 * @include ndbapi_async.cpp
 */
/**
 * @page ndbapi_async1.cpp ndbapi_async1.cpp
 * @include ndbapi_async1.cpp
 */
#endif

/**
 * @page ndbapi_retries.cpp ndbapi_retries.cpp
 * @include ndbapi_retries.cpp
 */

/**
 * @page ndbapi_simple_index.cpp ndbapi_simple_index.cpp
 * @include ndbapi_simple_index.cpp
 */

/**
 * @page ndbapi_scan.cpp ndbapi_scan.cpp
 * @include ndbapi_scan.cpp
 */

/**
 * @page ndbapi_event.cpp ndbapi_event.cpp
 * @include ndbapi_event.cpp
 */


/**
   @page secAdapt  Adaptive Send Algorithm

   At the time of "sending" a transaction
   (using NdbTransaction::execute()), the transactions
   are in reality <em>not</em> immediately transferred to the NDB Kernel.
   Instead, the "sent" transactions are only kept in a
   special send list (buffer) in the Ndb object to which they belong.
   The adaptive send algorithm decides when transactions should
   actually be transferred to the NDB kernel.

   The NDB API is designed as a multi-threaded interface and so
   it is often desirable to transfer database operations from more than
   one thread at a time.
   The NDB API keeps track of which Ndb objects are active in transferring
   information to the NDB kernel and the expected amount of threads to
   interact with the NDB kernel.
   Note that a given instance of Ndb should be used in at most one thread;
   different threads should <em>not</em> use the same Ndb object.

   There are four conditions leading to the transfer of database
   operations from Ndb object buffers to the NDB kernel:
   -# The NDB Transporter (TCP/IP or shared memory)
      decides that a buffer is full and sends it off.
      The buffer size is implementation-dependent and
      may change between MySQL Cluster releases.
      On TCP/IP the buffer size is usually around 64 KB;
      Since each Ndb object provides a single buffer per storage node,
      the notion of a "full" buffer is local to this storage node.
   -# The accumulation of statistical data on transferred information
      may force sending of buffers to all storage nodes.
   -# Every 10 ms, a special transmission thread checks whether or not
      any send activity has occurred. If not, then the thread will
      force transmission to all nodes.
      This means that 20 ms is the maximum time database operations
      are kept waiting before being sent off. The 10-millisecond limit
      is likely to become a configuration parameter in
      future releases of MySQL Cluster; however, for checks that
      are more frequent than each 10 ms,
      additional support from the operating system is required.
   -# For methods that are affected by the adaptive send algorithm
      (such as NdbTransaction::execute()), there is a <var>force</var>
      parameter
      that overrides its default behaviour in this regard and forces
      immediate transmission to all nodes. See the individual NDB API class
      listings for more information.

   @note The conditions listed above are subject to change in future releases
   of MySQL Cluster.
*/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**

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

   The poll method invoked (either Ndb::pollNdb() or Ndb::sendPollNdb())
   will return when:
   -# at least 'minNoOfEventsToWakeup' of the transactions
      in the send list have transitioned to state 3 as described above, and
   -# all of these transactions have executed their callback methods.
*/
#endif

/**
   @page secConcepts  MySQL Cluster Concepts

   The <em>NDB Kernel</em> is the collection of storage nodes
   belonging to a MySQL Cluster.
   The application programmer can for most purposes view the
   set of all storage nodes as a single entity.
   Each storage node is made up of three main components:
   - TC : The transaction co-ordinator
   - ACC : Index storage component
   - TUP : Data storage component

   When an application program executes a transaction,
   it connects to one transaction co-ordinator on one storage node.
   Usually, the programmer does not need to specify which TC should be used,
   but in some cases when performance is important, the programmer can
   provide "hints" to use a certain TC.
   (If the node with the desired transaction co-ordinator is down, then another TC will
   automatically take over the work.)

   Every storage node has an ACC and a TUP which store
   the indexes and data portions of the database table fragment.
   Even though one TC is responsible for the transaction,
   several ACCs and TUPs on other storage nodes might be involved in the
   execution of the transaction.


   @section secNdbKernelConnection   Selecting a Transaction Co-ordinator

   The default method is to select the transaction co-ordinator (TC) determined to be
   the "closest" storage node, using a heuristic for proximity based on
   the type of transporter connection. In order of closest to most distant, these are
   - SHM
   - TCP/IP (localhost)
   - TCP/IP (remote host)
   If there are several connections available with the same proximity, they will each be
   selected in a round robin fashion for every transaction. Optionally
   one may set the method for TC selection to round-robin mode, where each new set of
   transactions is placed on the next DB node. The pool of connections from which this
   selection is made consists of all available connections.

   As noted previously, the application programmer can provide hints to the NDB API as to
   which transaction co-ordinator it should use. This is done by
   providing a <em>table</em> and <em>partition key</em>
   (usually the primary key).
   By using the primary key as the partition key,
   the transaction will be placed on the node where the primary replica
   of that record resides.
   Note that this is only a hint; the system can be
   reconfigured at any time, in which case the NDB API will choose a transaction
   co-ordinator without using the hint.
   For more information, see NdbDictionary::Column::getPartitionKey() and
   Ndb::startTransaction(). The application programmer can specify
   the partition key from SQL by using the construct,
   <code>CREATE TABLE ... ENGINE=NDB PARTITION BY KEY (<var>attribute-list</var>);</code>.


   @section secRecordStruct          NDB Record Structure
   The NDB Cluster engine used by MySQL Cluster is a relational database engine
   storing records in tables just as with any other RDBMS.
   Table rows represent records as tuples of relational data.
   When a new table is created, its attribute schema is specified for the table as a whole,
   and thus each record of the table has the same structure. Again, this is typical
   of relational databases, and NDB is no different in this regard.


   @subsection secKeys               Primary Keys
   Each record has from 1 up to 32 attributes which belong
   to the primary key of the table.

   @section secTrans                 Transactions

   Transactions are committed first to main memory,
   and then to disk after a global checkpoint (GCP) is issued.
   Since all data is (in most NDB Cluster configurations)
   synchronously replicated and stored on multiple NDB nodes,
   the system can still handle processor failures without loss
   of data.
   However, in the case of a system failure (e.g. the whole system goes down),
   then all (committed or not) transactions occurring since the latest GCP are lost.


   @subsection secConcur                Concurrency Control
   NDB Cluster uses pessimistic concurrency control based on locking.
   If a requested lock (implicit and depending on database operation)
   cannot be attained within a specified time,
   then a timeout error occurs.

   Concurrent transactions as requested by parallel application programs and
   thread-based applications can sometimes deadlock when they try to access
   the same information simultaneously.
   Thus, applications need to be written in a manner so that timeout errors
   occurring due to such deadlocks are handled gracefully. This generally
   means that the transaction encountering a timeout should be rolled back
   and restarted.


   @section secHint                 Hints and Performance

   Placing the transaction co-ordinator in close proximity
   to the actual data used in the transaction can in many cases
   improve performance significantly. This is particularly true for
   systems using TCP/IP. For example, a Solaris system using a single 500 MHz processor
   has a cost model for TCP/IP communication which can be represented by the formula

     <code>[30 microseconds] + ([100 nanoseconds] * [<var>number of bytes</var>])</code>

   This means that if we can ensure that we use "popular" links we increase
   buffering and thus drastically reduce the communication cost.

   Typically, TCP/IP systems spend 30-60% of their working time on communication.

   A simple example is an application that uses many simple updates where
   a transaction needs to update one record.
   This record has a 32 bit primary key,
   which is also the partition key.
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
   Interpreted programs are executed in a
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
   The stack allows for up to 32 subroutine calls in succession.
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
   A table attribute in NDB Cluster can be of type <var>Array</var>,
   meaning that the attribute consists of an ordered sequence of
   elements. In such cases, <var>attribute size</var> is the size
   (expressed in bits) of any one element making up the array; the
   <var>array size</var> is the number of elements in the array.

*/

#ifndef Ndb_H
#define Ndb_H

#include <ndb_types.h>
#include "ndbapi_limits.h"
#include "ndb_cluster_connection.hpp"
#include "NdbError.hpp"
#include "NdbDictionary.hpp"


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
class TransporterFacade;
class PollGuard;
class Ndb_local_table_info;
template <class T> struct Ndb_free_list_t;
class NdbLockHandle;

typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);

#define WAITFOR_RESPONSE_TIMEOUT 120000 // Milliseconds

#define NDB_SYSTEM_DATABASE "sys"
#define NDB_SYSTEM_SCHEMA "def"

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
  friend class NdbEventBuffer;
  friend class NdbTransaction;
  friend class Table;
  friend class NdbApiSignal;
  friend class NdbIndexOperation;
  friend class NdbScanOperation;
  friend class NdbIndexScanOperation;
  friend class NdbDictionary::Dictionary;
  friend class NdbDictionaryImpl;
  friend class NdbDictInterface;
  friend class NdbBlob;
  friend class NdbImpl;
  friend class Ndb_cluster_connection;
  friend class Ndb_cluster_connection_impl;
  friend class Ndb_internal;
  friend class NdbScanFilterImpl;
  friend class PollGuard;
  friend class NdbQueryImpl;
  friend class NdbQueryOperationImpl;
  friend class MultiNdbWakeupHandler;
  friend class NdbWaitGroup;
#endif

public:
  /**
   * @name General
   * @{
   */
  /**
   * The Ndb object represents a connection to a database.
   *
   * @note The init() method must be called before the Ndb object may actually be used.
   *
   * @param ndb_cluster_connection is a connection to the cluster containing
   *        the database to be used
   * @param aCatalogName is the name of the catalog to be used.
   * @note The catalog name provides a namespace for the tables and
   *       indexes created in any connection from the Ndb object.
   * @param aSchemaName is the name of the schema you
   *        want to use.
   * @note The schema name provides an additional namespace
   *       for the tables and indexes created in a given catalog.
   */
  Ndb(Ndb_cluster_connection *ndb_cluster_connection,
      const char* aCatalogName = "", const char* aSchemaName = "def");

  ~Ndb();

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * The current ndb_cluster_connection get_ndb_cluster_connection.
   *
   * @return the current connection
   */
  Ndb_cluster_connection& get_ndb_cluster_connection();
#endif

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
  int setCatalogName(const char * aCatalogName);

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
  int setSchemaName(const char * aSchemaName);
#endif

  /**
   * Get the name of the Ndb object.
   * If no name is given, it will return 0.
   **/
  const char* getNdbObjectName () const;

  /**
   * Set a human readable name optionally to identify an
   * Ndb object for debugging purpose. Setting should be done
   * after creating the Ndb object, but before the object is
   * initialised with init().
   * 1) Setting the name more than once will fail and return 4014.
   * 2) Setting the name after init will return give error 4015.
   *
   * It is recommended to use the reference (ndbObject->getReference())
   * followed by the name (if given) in printouts
   * of the user applications subscribing events.
   * This will make tracing event handling between a subscribing user app
   * and the ndb engine easier, since the reference correlates the app's
   * ndb object, event buffer and the ndb engine (Suma block).
   */
  int setNdbObjectName(const char *name);

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
  int setDatabaseName(const char * aDatabaseName);

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
  int setDatabaseSchemaName(const char * aDatabaseSchemaName);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /** Set database and schema name to match previously retrieved table
   *
   * Returns non-zero if table internal name does not contain
   * non-empty database and schema names
   */
  int setDatabaseAndSchemaName(const NdbDictionary::Table* t);
#endif

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

  /**
   * Set/get maximum memory size for event buffer
   */
  void set_eventbuf_max_alloc(Uint64 sz);
  Uint64 get_eventbuf_max_alloc();

  /**
   * Set/get free_percent- the % of event buffer memory
   * that should be available before resuming buffering,
   * after the max_alloc limit is hit.
   */
  int set_eventbuffer_free_percent(unsigned sz);
  unsigned get_eventbuffer_free_percent();

  struct EventBufferMemoryUsage
  {
    EventBufferMemoryUsage() :
      allocated_bytes(0),
      used_bytes(0),
      usage_percent(0)
    {}

    Uint64 allocated_bytes;
    Uint64 used_bytes;
    Uint32 usage_percent; // (used_bytes)*100/eventbuf_max_alloc
  };
  /**
   * Get event buffer usauge as a percentage of eventbuf_max_alloc limit.
   * In/out parameter : struct EventBufferMemoryUsage&, which contains
   *  allocated_bytes : total event buffer memory allocated in bytes
   *  used_bytes : total memory used in bytes
   *  usage_percent : event buffer memory usage percent = (100*used/max_alloc).
   * Usage_percent is allowed to go over 100% temporarily
   * for some period of time or permanently if eventbuf_max_alloc
   * and eventbuffer_free_percent are not configured
   * according to the event data load. The latter causes frequent gaps
   * and thus should be avoided.
   */
  void get_event_buffer_memory_usage(EventBufferMemoryUsage&);

  void setEventBufferQueueEmptyEpoch(bool queue_empty_epoch);

  /** Note: The getter function getEventBufferQueueEmptyEpoch() is
   * not provided intentionally to avoid wrong usage, for e.g.,
   * consuming empty epochs based on the getter. Reason:
   * setter option applies to queuing *newer* epochs
   * and the queue may reflect the state before the setting.
   * Therefore, during a transition period, consumption may find
   * an empty epoch in the queue even if the getter shows that
   * the queuing is turned off.
   */

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
   *
   * @return Object representing an event, NULL on failure
   */
  NdbEventOperation* createEventOperation(const char* eventName);
  /**
   * Drop a subscription to an event
   *
   * @param eventOp
   *        Event operation
   *
   * @return 0 on success
   */
  int dropEventOperation(NdbEventOperation* eventOp);

private:
  // Help functions for pollEvents() and nextEvent()

  // Inform event buffer overflow and exit
  void printOverflowErrorAndExit();
public:

  /**
   * Wait for an event to occur. Will return as soon as an event data
   * is available on any of the created events. PollEvents() also moves
   * the complete event data of an epoch to the event queue.
   *
   * @param aMillisecondNumber
   *        maximum time to wait
   * aMillisecondNumber < 0 : returns -1
   *
   * @param[OUT] highestQueuedEpoch if highestQueuedEpoch is non-null and
   * there is some new event data available in the event queue,
   * it will be set to the highest epoch among the available event data.
   *
   * @return > 0 if events available, 0 if no events available, < 0 on failure.
   *
   * pollEvents2 will also return >0 when there is an event data
   * representing empty or error epoch available on the head of the event queue.
   */
  int pollEvents2(int aMillisecondNumber, Uint64 *highestQueuedEpoch= 0);

  /**
   * Check if higher queued epochs have been seen by the last
   * pollEvents2 call or if a TE_CLUSTER_FAILURE event has been
   * detected. If a cluster failure has been detected then the
   * highestQueuedEpoch returned from pollEvents2() might not
   * increase anymore. The correct action is then not to poll
   * for more events, but instead consume events with nextEvent()
   * until a TE_CLUSTER_FAILURE is detected and then reconnect to
   * the cluster when it is available again.
   */
  bool isExpectingHigherQueuedEpochs();

#define NDB_FAILURE_GCI ~(Uint64)0

  /**
   * Wait for an event to occur. Will return as soon as an event
   * is available on any of the created events.
   *
   * @param aMillisecondNumber
   *        maximum time to wait
   * aMillisecondNumber < 0 will cause a long wait
   * @param latestGCI
   *        if a valid pointer is passed to a 64-bit integer it will be set
   *        to the latest polled GCI. If a cluster failure is detected it
   *        will be set to NDB_FAILURE_GCI.
   *
   * @return > 0 if events available, 0 if no events available, < 0 on failure
   *
   * This is a backward compatibility wrapper to pollEvents2().
   * Returns 1 if a regular data is found,
   * returns 0 otherwise.
   * However it does not maintain the old behaviour when it encounters
   * exceptional event data on the head of the event queue:
   * - returns 1 for event data representing inconsistent epoch.
   *   In this case, the following nextEvent() call will return NULL.
   *   The inconsistency (isConsistent(Uint64& gci)) should be checked
   *   after the following (first) nextEvent() call returning NULL.
   *   Even though the inconsistent event data is removed from the
   *   event queue by this nextEvent() call, the information about
   *   inconsistency will be removed only by the following (second)
   *   nextEvent() call.
   * - returns 1 for event data representing event buffer overflow epoch,
   *   which is added to the event queue when event buffer usage
   *   exceeds eventbuf_max_alloc.
   *   In this case, following call to nextEvent() will exit the process.
   * - removes empty epochs from the event queue head until a regular
   *   event data is found or the whole queue is processed.
   */
  int pollEvents(int aMillisecondNumber, Uint64 *latestGCI= 0);

  /**
   * Returns the event operation associated with the dequeued
   * event data from the event queue. This should be called after
   * pollEvents() populates the queue, and then can be called repeatedly
   * until the event queue becomes empty.
   *
   * @return an event operation that has data or exceptional epoch data,
   * or NULL if the queue is empty.
   *
   * nextEvent2() will return non-null event operation for event data
   * representing exceptional (empty or error) epochs as well.
   * NdbEventOperation::getEpoch2() should be called  after
   * nextEvent2() to find the epoch, then
   * NdbEventOperation::getEventType2() should be called to check the
   * type of the returned event data
   * and proper handling should be performed for the newly introduced
   * exceptional event types:
   * NdbDictionary::Event::TE_EMPTY, TE_INCONSISTENT and TE_OUT_OF_MEMORY.
   * No other methods defined on NdbEventOperation than the above two
   * should be called for exceptional epochs.
   * Returning empty epoch (TE_EMPTY) is new and may overflood the
   * application when ndb data nodes are idling. If this is not desirable,
   * applications should do extra handling to filter out empty epochs.
   */
  NdbEventOperation *nextEvent2();

  /**
   * This is a backward compatibility wrapper to nextEvent2().
   * Returns an event operation that has data after a pollEvents,
   *  NULL if the queue is empty.
   * It maintains the old behaviour :
   * - returns NULL for inconsistent epochs. Therefore, it is important
   *   to call isConsistent(Uint64& gci) to check for inconsistency,
   *   after nextEvent() returns NULL.
   * - will not have empty epochs in the event queue (i.e. remove them),
   * - exits the process when it encounters an event data
   *   representing an event buffer overflow.
   */
  NdbEventOperation *nextEvent();

  /**
   * Check if all events are consistent
   * If node failure occurs during resource exaustion events
   * may be lost and the delivered event data might thus be incomplete.
   *
   * @param[OUT] gci
   *        any inconsistent GCI found
   *
   * @return true if all received events are consistent, false if possible
   * inconsistency
   */
  bool isConsistent(Uint64& gci);

  /**
   * Check if all events in a GCI are consistent
   * If node failure occurs during resource exaustion events
   * may be lost and the delivered event data might thus be incomplete.
   *
  * @param gci
   *        the GCI to check
   *
   * @return true if GCI is consistent, false if possible inconsistency
   */
  bool isConsistentGCI(Uint64 gci);

  /**
   * Iterate over distinct event operations which are part of current
   * GCI.  Valid after nextEvent.  Used to get summary information for
   * the epoch (e.g. list of all tables) before processing event data.
   *
   * Set *iter=0 to start.  Returns NULL when no more.  If event_types
   * is not NULL, it returns bitmask of received event types.
   *
   * This is a wrapper for getNextEventOpInEpoch3, but retains the
   * old name in order to preserve backward compatibility.
   */
  const NdbEventOperation*
    getGCIEventOperations(Uint32* iter,
                          Uint32* event_types);

  /**
   * Iterate over distinct event operations which are part of current
   * GCI.  Valid after nextEvent.  Used to get summary information for
   * the epoch (e.g. list of all tables) before processing event data.
   *
   * Set *iter=0 to start.  Returns NULL when no more.  If event_types
   * is not NULL, it returns bitmask of received event types.
   */

  const NdbEventOperation*
    getNextEventOpInEpoch2(Uint32* iter, Uint32* event_types);

  /**
   * Iterate over distinct event operations which are part of current
   * GCI.  Valid after nextEvent.  Used to get summary information for
   * the epoch (e.g. list of all tables) before processing event data.
   *
   * Set *iter=0 to start.  Returns NULL when no more.  If event_types
   * is not NULL, it returns bitmask of received event types.
   * If cumulative_any_value is not NULL, it returns a merged value of
   * received any_value(s) to show what bits are set for all operations of a
   * specific table.
   */
  const NdbEventOperation*
    getNextEventOpInEpoch3(Uint32* iter,
                           Uint32* event_types,
                           Uint32* cumulative_any_value);

  /** Get the highest epoch that have entered into the event queue.
   * This value can be higher than the epoch returned by the last
   * pollEvents() call, if new epochs have been received and queued later.
   */
  Uint64 getHighestQueuedEpoch();

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  int flushIncompleteEvents(Uint64 gci);
  NdbEventOperation *getEventOperation(NdbEventOperation* eventOp= 0);
  Uint64 getLatestGCI();
  void forceGCP();
  void setReportThreshEventGCISlip(unsigned thresh);
  void setReportThreshEventFreeMem(unsigned thresh);
#endif

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
   * @param  table    Pointer to table object used for deciding
   *                  which node to run the Transaction Coordinator on
   * @param  keyData  Pointer to partition key corresponding to
   *                  <var>table</var>
   * @param  keyLen   Length of partition key expressed in bytes
   *
   * @return NdbTransaction object, or NULL on failure.
   */
  NdbTransaction* startTransaction(const NdbDictionary::Table *table= 0,
				   const char  *keyData = 0,
				   Uint32       keyLen = 0);


  /**
   * Structure for passing in pointers to distribution key values
   * When distribution key has multiple parts, they should be
   * passed as an array, with the last part's ptr == NULL.
   *
   */
  struct Key_part_ptr
  {
    const void * ptr;
    unsigned len;
  };

  /**
   * Structure for describing a table partition in terms of either
   *
   * PS_NONE
   *   No partitioning info provided.
   *
   * PS_USER_DEFINED
   *   A specific partition id for a table with user defined
   *   partitioning
   *
   * PS_DISTR_KEY_PART_PTR
   *   An array of a table's distribution key values for a
   *   table with native partitioning.
   *
   * PS_DISTR_KEY_RECORD
   *   A row in given NdbRecord format containing a natively
   *   partitioned table's distribution key values
   *
   */

  struct PartitionSpec
  {
    /*
      Size of the PartitionSpec structure.
    */
    static inline Uint32 size()
    {
        return sizeof(PartitionSpec);
    }

    enum SpecType
    {
      PS_NONE                = 0,
      PS_USER_DEFINED        = 1,
      PS_DISTR_KEY_PART_PTR  = 2,
      PS_DISTR_KEY_RECORD    = 3
    };

    Uint32 type;

    union
    {
      struct {
        Uint32 partitionId;
      } UserDefined;

      struct {
        const Key_part_ptr* tableKeyParts;
        void* xfrmbuf;
        Uint32 xfrmbuflen;
      } KeyPartPtr;

      struct {
        const NdbRecord* keyRecord;
        const char* keyRow;
        void* xfrmbuf;
        Uint32 xfrmbuflen;
      } KeyRecord;
    };
  };

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /* First version of PartitionSpec, defined here for
   * backwards compatibility reasons
   */
  struct PartitionSpec_v1
  {
    enum SpecType
    {
      PS_NONE                = 0,
      PS_USER_DEFINED        = 1,
      PS_DISTR_KEY_PART_PTR  = 2
    };

    Uint32 type;

    union
    {
      struct {
        Uint32 partitionId;
      } UserDefined;

      struct {
        const Key_part_ptr* tableKeyParts;
        void* xfrmbuf;
        Uint32 xfrmbuflen;
      } KeyPartPtr;
    };
  };
#endif

  /**
   * Start a transaction
   *
   * @note When the transaction is completed it must be closed using
   *       Ndb::closeTransaction or NdbTransaction::close.
   *       The transaction must be closed independent of its outcome, i.e.
   *       even if there is an error.
   *
   * @param  table    Pointer to table object used for deciding
   *                  which node to run the Transaction Coordinator on
   * @param  keyData  Null-terminated array of pointers to keyParts that is
   *                  part of distribution key.
   *                  Length of resp. keyPart will be read from
   *                  metadata and checked against passed value
   * @param  xfrmbuf  Pointer to temporary buffer that will be used
   *                  to calculate hashvalue
   * @param  xfrmbuflen Length of buffer
   *
   * @note if xfrmbuf is null (default) malloc/free will be made
   *       if xfrmbuf is not null but length is too short, method will fail
   *
   * @return NdbTransaction object, or NULL on failure.
   */
  NdbTransaction* startTransaction(const NdbDictionary::Table *table,
				   const struct Key_part_ptr * keyData,
				   void* xfrmbuf = 0, Uint32 xfrmbuflen = 0);
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  NdbTransaction* startTransaction(const NdbRecord *keyRec, const char *keyData,
				   void* xfrmbuf, Uint32 xfrmbuflen);
#endif
  /**
   * Start a transaction, specifying table+partition as hint for
   *  TC-selection
   *
   */
  NdbTransaction* startTransaction(const NdbDictionary::Table* table,
                                   Uint32 partitionId);
  /**
   * Start a transaction on a specified node id and instance id.
   * Mostly intended for test cases, but can also be useful on
   * heterogeneous cluster installations.
   *
   * As in all startTransaction variants the nodeId and instanceId is
   * merely a hint and if the node is down another TC will be used
   * instead.
   */
  NdbTransaction* startTransaction(Uint32 nodeId, Uint32 instanceId);

  /**
   * Compute distribution hash value given table/keys
   *
   * @param  hashvalueptr - OUT, is set to hashvalue if return value is 0
   * @param  table    Pointer to table object
   * @param  keyData  Null-terminated array of pointers to keyParts that is
   *                  part of distribution key.
   *                  Length of resp. keyPart will be read from
   *                  metadata and checked against passed value
   * @param  xfrmbuf  Pointer to temporary buffer that will be used
   *                  to calculate hashvalue
   * @param  xfrmbuflen Length of buffer
   *
   * @note if xfrmbuf is null (default) malloc/free will be made
   *       if xfrmbuf is not null but length is too short, method will fail
   *       Only for use with natively partitioned tables.
   *
   * @return 0 - ok - hashvalueptr is set
   *         else - fail, return error code
   */
  static int computeHash(Uint32* hashvalueptr,
                         const NdbDictionary::Table* table,
                         const struct Key_part_ptr * keyData,
                         void* xfrmbuf = 0, Uint32 xfrmbuflen = 0);
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  static int computeHash(Uint32* hashvalueptr,
                         const NdbRecord *keyRec, const char *keyData,
                         void* xfrmbuf, Uint32 xfrmbuflen);
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
  /** @} *********************************************************************/
#endif

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

  /**
   * Get a string containing any extra error details in the supplied
   * buffer
   * Where there is extra detail available a ptr to the start of
   * the supplied buffer will be returned.
   * If the extra detail string is longer than the passed buffer
   * then it will be truncated to fit.
   * Where there is no extra detail, NULL will be returned.
   */
  const char* getNdbErrorDetail(const NdbError& err,
                                char* buff,
                                Uint32 buffLen) const;

  /** @} *********************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * Get the application node identity.
   *
   * @return Node id of this application.
   */
  int getNodeId();

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

  struct TupleIdRange {
    TupleIdRange() {}
    Uint64 m_first_tuple_id;
    Uint64 m_last_tuple_id;
    Uint64 m_highest_seen;
    void reset() {
      m_first_tuple_id = ~(Uint64)0;
      m_last_tuple_id = ~(Uint64)0;
      m_highest_seen = 0;
    }
  };

  int initAutoIncrement();

  /**
   * Return a unique tuple id for a table.  The id sequence is
   * ascending but may contain gaps.  Methods which have no
   * TupleIdRange argument use NDB API dict cache.  They may
   * not be called from mysqld.
   *
   * @param aTableName table name
   *
   * @param cacheSize number of values to cache in this Ndb object
   *
   * @return 0 or -1 on error, and tupleId in out parameter
   */
  int getAutoIncrementValue(const char* aTableName,
                            Uint64 & autoValue, Uint32 cacheSize,
                            Uint64 step = 1, Uint64 start = 1);
  int getAutoIncrementValue(const NdbDictionary::Table * aTable,
                            Uint64 & autoValue, Uint32 cacheSize,
                            Uint64 step = 1, Uint64 start = 1);
  int getAutoIncrementValue(const NdbDictionary::Table * aTable,
                            TupleIdRange & range, Uint64 & autoValue,
                            Uint32 cacheSize,
                            Uint64 step = 1, Uint64 start = 1);
  int readAutoIncrementValue(const char* aTableName,
                             Uint64 & autoValue);
  int readAutoIncrementValue(const NdbDictionary::Table * aTable,
                             Uint64 & autoValue);
  int readAutoIncrementValue(const NdbDictionary::Table * aTable,
                             TupleIdRange & range, Uint64 & autoValue);
  int setAutoIncrementValue(const char* aTableName,
                            Uint64 autoValue, bool modify);
  int setAutoIncrementValue(const NdbDictionary::Table * aTable,
                            Uint64 autoValue, bool modify);
  int setAutoIncrementValue(const NdbDictionary::Table * aTable,
                            TupleIdRange & range, Uint64 autoValue,
                            bool modify);
#ifdef NDBAPI_50_COMPAT
  Uint64 getAutoIncrementValue(const NdbDictionary::Table * aTable,
			       Uint32 cacheSize = 1)
    {
      Uint64 val;
      if (getAutoIncrementValue(aTable, val, cacheSize, 1, 1) == -1)
        return ~(Uint64)0;
      return val;
    }
#endif
  bool checkUpdateAutoIncrementValue(TupleIdRange & range, Uint64 autoValue);
private:
  int getTupleIdFromNdb(const NdbTableImpl* table,
                        TupleIdRange & range, Uint64 & tupleId,
                        Uint32 cacheSize, Uint64 step = 1, Uint64 start = 1);
  int readTupleIdFromNdb(const NdbTableImpl* table,
                         TupleIdRange & range, Uint64 & tupleId);
  int setTupleIdInNdb(const NdbTableImpl* table,
                      TupleIdRange & range, Uint64 tupleId, bool modify);
  int checkTupleIdInNdb(TupleIdRange & range,
                        Uint64 tupleId);
  int opTupleIdOnNdb(const NdbTableImpl* table,
                     TupleIdRange & range, Uint64 & opValue, Uint32 op);
public:

  /**
   */
  NdbTransaction* hupp( NdbTransaction* );
  Uint32 getReference() const { return theMyRef;}

  struct Free_list_usage
  {
    const char * m_name;
    Uint32 m_created;
    Uint32 m_free;
    Uint32 m_sizeof;
  };

  Free_list_usage * get_free_list_usage(Free_list_usage*);

  /* Get minimum known DB node version */
  Uint32 getMinDbNodeVersion() const;

  /* Get/Set per-Ndb custom data pointer */
  void setCustomData(void*);
  void* getCustomData() const;

  /* Get/Set per-Ndb custom data pointer */
  /* NOTE: shares storage with void*
   * i.e can not be used together with setCustomData
   */
  void setCustomData64(Uint64);
  Uint64 getCustomData64() const;

  /**
   * transid next startTransaction() on this ndb-object will get
   */
  Uint64 getNextTransactionId() const;

  /* Some client behaviour counters to assist
   * optimisation
   */
  enum ClientStatistics
  {
    /* Latency avoidance : */
    /* Number of times user thread blocked waiting for data node response */
    WaitExecCompleteCount    = 0,  /* Waiting for PK/UK/Scan requests to complete */
    WaitScanResultCount      = 1,  /* Waiting for next scan batch(es) to arrive */
    WaitMetaRequestCount     = 2,  /* Waiting for some meta data operation to complete */

    /* Measured latency */
    WaitNanosCount           = 3,  /* Nanoseconds spent waiting for kernel response */

    /* Data transfer */
    BytesSentCount           = 4,  /* Bytes sent to kernel by this object */
    BytesRecvdCount          = 5,  /* Bytes received from kernel by this object */

    /* Work performed */
    TransStartCount          = 6,  /* Transactions started */
    TransCommitCount         = 7,  /* Transactions committed */
    TransAbortCount          = 8,  /* Transactions aborted */
    TransCloseCount          = 9,  /* Transactions closed */

    PkOpCount                = 10, /* Primary key operation count */
    UkOpCount                = 11, /* Unique key operation count */
    TableScanCount           = 12, /* Table scans */
    RangeScanCount           = 13, /* Range scans */

    /* Efficiency */
    PrunedScanCount          = 14, /* Count of scans scanning 1 fragment */
    ScanBatchCount           = 15, /* Count of scan batches received */
    ReadRowCount             = 16, /* Rows returned to API, from PK/UK/Scan */
    TransLocalReadRowCount   = 17, /* Rows returned to API from trans node */

    /* Event Api */
    DataEventsRecvdCount     = 18, /* Number of table data change events received */
    NonDataEventsRecvdCount  = 19, /* Number of non-data events received */
    EventBytesRecvdCount     = 20, /* Number of bytes of event data received */

    /* Adaptive Send */
    ForcedSendsCount         = 21, /* Number of sends with force-send set */
    UnforcedSendsCount       = 22, /* Number of sends without force-send */
    DeferredSendsCount       = 23, /* Number of adaptive send calls not actually sent */

    NumClientStatistics      = 24   /* End marker */
  };

  Uint64 getClientStat(Uint32 id) const;
  const char* getClientStatName(Uint32 id) const;
#endif

private:
/*****************************************************************************
 *     These are service routines used by the other classes in the NDBAPI.
 ****************************************************************************/
  Uint32 _unused;
  void *_unused2;

  Ndb(const Ndb&); // Not impl.
  Ndb&operator=(const Ndb&);

  void setup(Ndb_cluster_connection *ndb_cluster_connection,
	     const char* aCatalogName, const char* aSchemaName);

  void connected(Uint32 block_reference);


  NdbTransaction*  startTransactionLocal(Uint32 aPrio, Uint32 aNode,
                                         Uint32 instance);

// Connect the connection object to the Database.
  int NDB_connect(Uint32 tNode, Uint32 instance);
  NdbTransaction* doConnect(Uint32 nodeId, Uint32 instance);
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

  NdbBlob*              getNdbBlob();// Get a blob handle etc

  NdbLockHandle*        getLockHandle(); // Get a lock handle.

  void			releaseSignal(NdbApiSignal* anApiSignal);
  void                  releaseSignals(Uint32, NdbApiSignal*, NdbApiSignal*);
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
  void                  releaseLockHandle(NdbLockHandle* lh);

  void                  check_send_timeout();
  void                  remove_sent_list(Uint32);
  Uint32                insert_completed_list(NdbTransaction*);
  Uint32                insert_sent_list(NdbTransaction*);

  int			sendRecSignal(Uint16 aNodeId,
				      Uint32 aWaitState,
				      NdbApiSignal* aSignal,
                                      Uint32 nodeSequence,
                                      Uint32 *ret_conn_seq= 0);

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
  NdbTransaction* getConnectedNdbTransaction(Uint32 nodeId, Uint32 instance);
  /**
   * Handle Connection Array lists
   */
  void appendConnectionArray(NdbTransaction *aCon, Uint32 nodeId);
  void prependConnectionArray(NdbTransaction *aCon, Uint32 nodeId);
  void removeConnectionArray(NdbTransaction *first, Uint32 nodeId);

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
  int     poll_trans(int milliSecs, int noOfEventsToWaitFor, PollGuard *pg);
  void    waitCompletedTransactions(int milliSecs, int noOfEventsToWaitFor,
		                    PollGuard *pg);
  void    completedTransaction(NdbTransaction* aTransaction);
  void    completedScanTransaction(NdbTransaction* aTransaction);

  void    abortTransactionsAfterNodeFailure(Uint16 aNodeId);

  static
  const char * externalizeTableName(const char * internalTableName,
                                    bool fullyQualifiedNames);
  const BaseString internalize_table_name(const char * external_name) const;

  static
  const char * externalizeIndexName(const char * internalIndexName,
                                    bool fullyQualifiedNames);

  static
  const BaseString getDatabaseFromInternalName(const char * internalName);
  static
  const BaseString getSchemaFromInternalName(const char * internalName);

  Uint64 allocate_transaction_id();

/******************************************************************************
 *	These are the private variables in this class.
 *****************************************************************************/
  NdbTransaction**       thePreparedTransactionsArray;
  NdbTransaction**       theSentTransactionsArray;
  NdbTransaction**       theCompletedTransactionsArray;

  Uint32                theNoOfPreparedTransactions;
  Uint32                theNoOfSentTransactions;
  Uint32                theNoOfCompletedTransactions;
  Uint32                theRemainingStartTransactions;
  Uint32                theMaxNoOfTransactions;
  Uint32                theMinNoOfEventsToWakeUp;

  Uint32                theNextConnectNode;

  bool deleted_v8_0_24;



  class NdbImpl * theImpl;
  class NdbDictionaryImpl* theDictionary;
  class NdbEventBuffer* theEventBuffer;

  NdbTransaction*	theTransactionList;
  NdbTransaction**      theConnectionArray;
  NdbTransaction**      theConnectionArrayLast;

  Uint32   theMyRef;        // My block reference
  Uint32   theNode;         // The node number of our node

  Uint64               the_last_check_time;
  Uint64               theFirstTransId;
  // The tupleId is retrieved from DB
  const NdbDictionary::Table *m_sys_tab_0;

  Uint32		theRestartGCI;	// the Restart GCI used by DIHNDBTAMPER

  // Allow updating error also from const member functions
  mutable NdbError theError;

  Int32        	        theNdbBlockNumber;

  enum InitType {
    NotConstructed,
    NotInitialised,
    StartingInit,
    Initialised,
    InitConfigError
  } theInitState;

  NdbApiSignal* theCommitAckSignal;

  /* Cached minimum connected Db node version */
  Uint32 theCachedMinDbNodeVersion;


#ifdef POORMANSPURIFY
  int cfreeSignals;
  int cnewSignals;
  int cgetSignals;
  int creleaseSignals;
#endif

#ifdef VM_TRACE
  void printState(const char* fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
#endif
};

#endif
