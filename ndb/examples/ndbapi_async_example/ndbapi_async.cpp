

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
 * ndbapi_async.cpp: 
 * Illustrates how to use callbacks and error handling using the asynchronous
 * part of the NDBAPI.
 *
 * Classes and methods in NDBAPI used in this example:
 *
 *  Ndb
 *       init()
 *       waitUntilRead()
 *       getDictionary()
 *       startTransaction()
 *       closeTransaction()
 *       sendPollNdb()
 *       getNdbError()
 *
 *  NdbConnection
 *       getNdbOperation()
 *       executeAsynchPrepare()
 *       getNdbError()
 *
 *  NdbDictionary::Dictionary
 *       getTable()
 *       dropTable()
 *       createTable()
 *       getNdbError()
 *
 *  NdbDictionary::Column
 *       setName()
 *       setPrimaryKey()
 *       setType()
 *       setLength()
 *       setNullable()
 *
 *  NdbDictionary::Table
 *       setName()
 *       addColumn()
 *
 *  NdbOperation
 *       insertTuple()
 *       equal()
 *       setValue()
 *       
 */


#include <ndb_global.h>

#include <NdbApi.hpp>
#include <NdbScanFilter.hpp>
#include <iostream> // Used for cout

/**
 * Helper sleep function
 */
int
milliSleep(int milliseconds){
  int result = 0;
  struct timespec sleeptime;
  sleeptime.tv_sec = milliseconds / 1000;
  sleeptime.tv_nsec = (milliseconds - (sleeptime.tv_sec * 1000)) * 1000000;
  result = nanosleep(&sleeptime, NULL);
  return result;
}

/**
 * error printout macro
 */
#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }


#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
 * callback struct.
 * transaction :  index of the transaction in transaction[] array below
 * data : the data that the transaction was modifying.
 * retries : counter for how many times the trans. has been retried
 */
typedef struct  {
  Ndb * ndb;
  int    transaction;  
  int    data;
  int    retries;
} async_callback_t;

/**
 * Structure used in "free list" to a NdbConnection
 */
typedef struct  {
  NdbConnection*  conn;   
  int used; 
} transaction_t;

/**
 * Free list holding transactions
 */
transaction_t   transaction[1024];  //1024 - max number of outstanding
                                    //transaction in one Ndb object

#endif 
/**
 * prototypes
 */

/**
 * Prepare and send transaction
 */
int  populate(Ndb * myNdb, int data, async_callback_t * cbData);

/**
 * Error handler.
 */
bool asynchErrorHandler(NdbConnection * trans, Ndb* ndb);

/**
 * Exit function
 */
void asynchExitHandler(Ndb * m_ndb) ;

/**
 * Helper function used in callback(...)
 */
void closeTransaction(Ndb * ndb , async_callback_t * cb);

/**
 * Function to create table
 */
int create_table(Ndb * myNdb);

/**
 * stat. variables
 */
int tempErrors = 0;
int permErrors = 0;

/**
 * Helper function for callback(...)
 */
void
closeTransaction(Ndb * ndb , async_callback_t * cb)
{
  ndb->closeTransaction(transaction[cb->transaction].conn);
  transaction[cb->transaction].conn = 0;
  transaction[cb->transaction].used = 0;
  cb->retries++;  
}

/**
 * Callback executed when transaction has return from NDB
 */
static void
callback(int result, NdbConnection* trans, void* aObject)
{
  async_callback_t * cbData = (async_callback_t *)aObject;
  if (result<0)
  {
    /**
     * Error: Temporary or permanent?
     */
    if (asynchErrorHandler(trans,  (Ndb*)cbData->ndb)) 
    {
      closeTransaction((Ndb*)cbData->ndb, cbData);
      while(populate((Ndb*)cbData->ndb, cbData->data, cbData) < 0)
	milliSleep(10);
    }
    else
    {
      std::cout << "Restore: Failed to restore data " 
		<< "due to a unrecoverable error. Exiting..." << std::endl;
      delete cbData;
      asynchExitHandler((Ndb*)cbData->ndb);
    }
  } 
  else 
  {
    /**
     * OK! close transaction
     */
    closeTransaction((Ndb*)cbData->ndb, cbData);
    delete cbData;
  }
}


/**
 * Create table "GARAGE"
 */
int create_table(Ndb * myNdb) 
{
  NdbDictionary::Table myTable;
  NdbDictionary::Column myColumn;
  
  NdbDictionary::Dictionary* myDict = myNdb->getDictionary();
  
  /*********************************************************
   * Create a table named GARAGE if it does not exist *
   *********************************************************/
  if (myDict->getTable("GARAGE") != NULL) 
  {
    std::cout << "NDB already has example table: GARAGE. "
	      << "Dropping it..." << std::endl; 
    if(myDict->dropTable("GARAGE") == -1)
    {
      std::cout << "Failed to drop: GARAGE." << std::endl; 
      exit(1);
    }
  } 

  myTable.setName("GARAGE");
  
/**
 * Column REG_NO
 */
  myColumn.setName("REG_NO");
  myColumn.setPrimaryKey(true);
  myColumn.setType(NdbDictionary::Column::Unsigned);
  myColumn.setLength(1);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

/**
 * Column BRAND
 */
  myColumn.setName("BRAND");
  myColumn.setPrimaryKey(false);
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(20);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

/**
 * Column COLOR
 */
  myColumn.setName("COLOR");
  myColumn.setPrimaryKey(false);
  myColumn.setType(NdbDictionary::Column::Char);
  myColumn.setLength(20);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  if (myDict->createTable(myTable) == -1) {
      APIERROR(myDict->getNdbError());
  }
  return 1;
}

void asynchExitHandler(Ndb * m_ndb) 
{
  if (m_ndb != NULL)
    delete m_ndb;
  exit(-1);
}

/* returns true if is recoverable (temporary),
 *  false if it is an  error that is permanent.
 */
bool asynchErrorHandler(NdbConnection * trans, Ndb* ndb) 
{  
  NdbError error = trans->getNdbError();
  switch(error.status)
  {
  case NdbError::Success:
    return false;
    break;
    
  case NdbError::TemporaryError:
    /**
     * The error code indicates a temporary error.
     * The application should typically retry.
     * (Includes classifications: NdbError::InsufficientSpace, 
     *  NdbError::TemporaryResourceError, NdbError::NodeRecoveryError,
     *  NdbError::OverloadError, NdbError::NodeShutdown 
     *  and NdbError::TimeoutExpired.)
     *     
     * We should sleep for a while and retry, except for insufficient space
     */
    if(error.classification == NdbError::InsufficientSpace)
      return false;
    milliSleep(10);  
    tempErrors++;  
    return true;
    break;    
  case NdbError::UnknownResult:
    std::cout << error.message << std::endl;
    return false;
    break;
  default:
  case NdbError::PermanentError:
    switch (error.code)
    {
    case 499:
    case 250:
      milliSleep(10);    
      return true; // SCAN errors that can be retried. Requires restart of scan.
    default:
      break;
    }
    //ERROR
    std::cout << error.message << std::endl;
    return false;
    break;
  }
  return false;
}

static int nPreparedTransactions = 0;
static int MAX_RETRIES = 10;
static int parallelism = 100;


/************************************************************************
 * populate()
 * 1. Prepare 'parallelism' number of insert transactions. 
 * 2. Send transactions to NDB and wait for callbacks to execute
 */
int populate(Ndb * myNdb, int data, async_callback_t * cbData)
{

  NdbOperation*   myNdbOperation;       // For operations

  async_callback_t * cb;
  int retries = 0;
  int current = 0;
  for(int i=0; i<1024; i++)
  {
    if(transaction[i].used == 0)
    {
      current = i;
      if (cbData == 0) 
      {
       /**
        * We already have a callback
	* This is an absolutely new transaction
        */
	cb = new async_callback_t;
	cb->retries = 0;
      }
      else 
      { 
       /**
        * We already have a callback
        */
	cb =cbData;
	retries = cbData->retries;
      }
      /**
       * Set data used by the callback
       */
      cb->ndb = myNdb;  //handle to Ndb object so that we can close transaction
                        // in the callback (alt. make myNdb global).

      cb->data =  data; //this is the data we want to insert
      cb->transaction = current; //This is the number (id)  of this transaction
      transaction[current].used = 1 ; //Mark the transaction as used
      break;
    }
  }
  if(!current)
    return -1;

  while(retries < MAX_RETRIES) 
    {
      transaction[current].conn = myNdb->startTransaction();
      if (transaction[current].conn == NULL) {
	if (asynchErrorHandler(transaction[current].conn, myNdb)) 
	{
          /**
           * no transaction to close since conn == null
           */
	  milliSleep(10);
	  retries++;
	  continue;
	}
	asynchExitHandler(myNdb);	
      }
      // Error check. If error, then maybe table GARAGE is not in database
      myNdbOperation = transaction[current].conn->getNdbOperation("GARAGE");
      if (myNdbOperation == NULL) 
      {
	if (asynchErrorHandler(transaction[current].conn, myNdb)) 
	{
	  myNdb->closeTransaction(transaction[current].conn);
	  transaction[current].conn = 0;
	  milliSleep(10);
	  retries++;
	  continue;
	}
	asynchExitHandler(myNdb);
      } // if
      if(myNdbOperation->insertTuple() < 0  ||
	 myNdbOperation->equal("REG_NO", data) < 0 ||
	 myNdbOperation->setValue("BRAND", "Mercedes") <0 ||
	 myNdbOperation->setValue("COLOR", "Blue") < 0)
      {
	if (asynchErrorHandler(transaction[current].conn, myNdb)) 
	{
	  myNdb->closeTransaction(transaction[current].conn);
	  transaction[current].conn = 0;
	  retries++;
	  milliSleep(10);
	  continue;
	}
	asynchExitHandler(myNdb);
      }     

      /*Prepare transaction (the transaction is NOT yet sent to NDB)*/
      transaction[current].conn->executeAsynchPrepare(Commit, 
						       &callback,
						       cb);
      /**
       * When we have prepared parallelism number of transactions ->
       * send the transaction to ndb. 
       * Next time we will deal with the transactions are in the 
       * callback. There we will see which ones that were successful
       * and which ones to retry.
       */
      if (nPreparedTransactions == parallelism-1) 
      {
	// send-poll all transactions
	// close transaction is done in callback
	myNdb->sendPollNdb(3000, parallelism );
	nPreparedTransactions=0;
      } 
      else
	nPreparedTransactions++;
      return 1;
    }
    std::cout << "Unable to recover from errors. Exiting..." << std::endl;
    asynchExitHandler(myNdb);
    return -1;
}

int main()
{
  ndb_init();
  Ndb* myNdb = new Ndb( "TEST_DB" );  // Object representing the database
  
  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb->init(1024) == -1) {          // Set max 1024  parallel transactions
    APIERROR(myNdb->getNdbError());
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  create_table(myNdb);

  
  /**
   * Initialise transaction array
   */
  for(int i = 0 ; i < 1024 ; i++) 
  {
    transaction[i].used = 0;
    transaction[i].conn = 0;
    
  }
  int i=0;
  /**
   * Do 20000 insert transactions.
   */
  while(i < 20000) 
  {
    while(populate(myNdb,i,0)<0)  // <0, no space on free list. Sleep and try again.
      milliSleep(10);
      
    i++;
  }
  std::cout << "Number of temporary errors: " << tempErrors << std::endl;
  delete myNdb; 
}


