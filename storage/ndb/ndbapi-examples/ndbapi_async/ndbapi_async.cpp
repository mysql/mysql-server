/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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
 * ndbapi_async.cpp: 
 * Illustrates how to use callbacks and error handling using the asynchronous
 * part of the NDBAPI.
 *
 * Classes and methods in NDBAPI used in this example:
 *
 *  Ndb_cluster_connection
 *       connect()
 *       wait_until_ready()
 *
 *  Ndb
 *       init()
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
 *  NdbOperation
 *       insertTuple()
 *       equal()
 *       setValue()
 *       
 */

#include "config.h"

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>

#include <stdlib.h>
#include <string.h>
#include <iostream> // Used for cout
#include <config.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

/**
 * Helper sleep function
 */
static void
milliSleep(int milliseconds){
  struct timeval sleeptime;
  sleeptime.tv_sec = milliseconds / 1000;
  sleeptime.tv_usec = (milliseconds - (sleeptime.tv_sec * 1000)) * 1000000;
  select(0, 0, 0, 0, &sleeptime);
}


/**
 * error printout macro
 */
#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }
#define APIERROR(error) { \
  PRINT_ERROR(error.code,error.message); \
  exit(-1); }

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
 * callback struct.
 * transaction :  index of the transaction in transaction[] array below
 * data : the data that the transaction was modifying.
 * retries : counter for how many times the trans. has been retried
 */
typedef struct async_callback_t {
  Ndb* const ndb;
  int    transaction;  
  int    data;
  int    retries;

  async_callback_t(Ndb* _ndb) : ndb(_ndb) {}
} async_callback_t;

/**
 * Structure used in "free list" to a NdbTransaction
 */
typedef struct transaction_t {
  NdbTransaction*  conn;   
  int used; 

  transaction_t() : conn(NULL), used(0) {}
} transaction_t;

/**
 * Free list holding transactions
 */
transaction_t   transaction[1024];  //1024 - max number of outstanding
                                    //transaction in one Ndb object

#endif 


static int nPreparedTransactions = 0; //Prepared + asynch executing txn
static int MAX_RETRIES = 10;
static int parallelism = 100;

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
bool asynchErrorHandler(NdbTransaction * trans, Ndb* ndb);

/**
 * Exit function
 */
void asynchExitHandler(Ndb * m_ndb) ;

/**
 * Helper function used in callback(...)
 */
void closeTransaction(async_callback_t * cb);

/**
 * stat. variables
 */
int tempErrors = 0;
int permErrors = 0;

void
closeTransaction(async_callback_t * cb)
{
  cb->ndb->closeTransaction(transaction[cb->transaction].conn);
  transaction[cb->transaction].conn = 0;
  transaction[cb->transaction].used = 0;
}

/**
 * Callback executed when transaction has return from NDB
 */
static void
callback(int result, NdbTransaction* trans, void* aObject)
{
  async_callback_t * cbData = (async_callback_t *)aObject;
  if (result<0)
  {
    /**
     * Error: Temporary or permanent?
     */
    const bool retryable = asynchErrorHandler(trans, cbData->ndb);
    closeTransaction(cbData);

    if (retryable && cbData->retries++ >= MAX_RETRIES) 
    {
      while(populate(cbData->ndb, cbData->data, cbData) < 0)
        milliSleep(10);
    }
    else
    {
      std::cout << "Unrecoverable error. Exiting..." << std::endl;
      Ndb* ndb = cbData->ndb;
      delete cbData;
      asynchExitHandler(ndb);
    }
  } 
  else 
  {
    /**
     * OK! close transaction
     */
    closeTransaction(cbData);
    delete cbData;
  }
}

void asynchExitHandler(Ndb * m_ndb) 
{
  delete m_ndb;
  exit(-1);
}

/* returns true if is recoverable (temporary),
 *  false if it is an  error that is permanent.
 */
bool asynchErrorHandler(NdbTransaction * trans, Ndb* ndb) 
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


/************************************************************************
 * populate()
 * 1. Prepare 'parallelism' number of insert transactions. 
 * 2. Send transactions to NDB and wait for callbacks to execute
 */
int populate(Ndb * myNdb, int data, async_callback_t * cbData)
{

  NdbOperation*   myNdbOperation;       // For operations
  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_async");
  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  async_callback_t * cb = NULL;
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
	cb = new async_callback_t(myNdb);
      }
      else 
      { 
       /**
        * We already have a callback
        */
	cb =cbData;
      }
      /**
       * Set data used by the callback
       */
      cb->retries = 0;
      cb->data =  data; //this is the data we want to insert
      cb->transaction = current; //This is the number (id)  of this transaction
      transaction[current].used = 1 ; //Mark the transaction as used
      break;
    }
  }
  if(cb == NULL)
    return -1;

  int retries = 0;
  while(retries < MAX_RETRIES) 
    {
      transaction[current].conn = myNdb->startTransaction();
      if (transaction[current].conn == NULL) {
	/**
	 * no transaction to close since conn == null
	 */
	milliSleep(10);
	retries++;
	continue;
      }
      myNdbOperation = transaction[current].conn->getNdbOperation(myTable);
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
      char mercedes[22];
      char blue[22];
      memset(mercedes, 0, sizeof(mercedes));
      memset(blue, 0, sizeof(blue));
      strcpy(mercedes, "mercedes");
      strcpy(blue, "blue");
      if(myNdbOperation->insertTuple() < 0  ||
	 myNdbOperation->equal("REG_NO", data) < 0 ||
	 myNdbOperation->setValue("BRAND", mercedes) <0 ||
	 myNdbOperation->setValue("COLOR", blue) < 0)
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
      transaction[current].conn->executeAsynchPrepare(NdbTransaction::Commit, 
						       &callback,
						       cb);
      /**
       * When we have prepared parallelism number of transactions ->
       * send the transaction to ndb. 
       * Next time we will deal with the transactions are in the 
       * callback. There we will see which ones that were successful
       * and which ones to retry.
       */
      nPreparedTransactions++;
      if (nPreparedTransactions >= parallelism)
      {
        //-------------------------------------------------------
        // Send-poll all transactions
        // Now we have defined a set of operations, it is now time
        // to execute all of them. Wait for at least 50% to complete.
        // Close transaction is done in callback
        //-------------------------------------------------------
        const int min_execs = nPreparedTransactions/2;
        const int nCompleted = myNdb->sendPollNdb(3000, min_execs);
        nPreparedTransactions -= nCompleted;
      } 
      return 1;
    }
    std::cout << "Unable to recover from errors. Exiting..." << std::endl;
    asynchExitHandler(myNdb);
    return -1;
}

/**************************************************************
 * Connect to mysql server and create table                   *
 **************************************************************/
void mysql_connect_and_create(const char * socket) {
  MYSQL mysql;
  bool ok;

  mysql_init(&mysql);

  ok = mysql_real_connect(&mysql, "localhost", "root", "", "", 0, socket, 0);
  if(ok) {
    mysql_query(&mysql, "CREATE DATABASE ndb_examples");
    ok = ! mysql_select_db(&mysql, "ndb_examples");
  }
  if(ok) {
    mysql_query(&mysql, "DROP TABLE IF EXISTS api_async");
    ok = ! mysql_query(&mysql,
      "CREATE TABLE"
		  "  api_async"
		  "    (REG_NO INT UNSIGNED NOT NULL,"
		  "     BRAND CHAR(20) NOT NULL,"
		  "     COLOR CHAR(20) NOT NULL,"
		  "     PRIMARY KEY USING HASH (REG_NO))"
		  "  ENGINE=NDB CHARSET=latin1"
    );
  }
  mysql_close(&mysql);

  if(! ok) MYSQLERROR(mysql);
}


void ndb_run_async_inserts(const char * connectstring)
{
  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/
  Ndb_cluster_connection cluster_connection(connectstring);
  if (cluster_connection.connect(4, 5, 1))
  {
    std::cout << "Unable to connect to cluster within 30 secs." << std::endl;
    exit(-1);
  }
  // Optionally connect and wait for the storage nodes (ndbd's)
  if (cluster_connection.wait_until_ready(30,0) < 0)
  {
    std::cout << "Cluster was not ready within 30 secs.\n";
    exit(-1);
  }

  Ndb *myNdb = new Ndb( &cluster_connection, "ndb_examples" );
  if (myNdb->init(1024) == -1) {      // Set max 1024 parallel transactions
    APIERROR(myNdb->getNdbError());
  }

  /**
   * Do some insert transactions.
   */
  for(int i = 0 ; i < 1234 ; i++) 
  {
    while(populate(myNdb, i, 0) < 0)  // <0, no space on free list. Sleep and try again.
      milliSleep(10);
  }
  /**
   * If there are prepared async transactions not yet completed,
   * we send them now as part of cleanup.
   */
  while (nPreparedTransactions > 0)
  {
    const int nCompleted = myNdb->sendPollNdb(3000, nPreparedTransactions);
    nPreparedTransactions -= nCompleted;
  }
  delete myNdb;
  std::cout << "Number of temporary errors: " << tempErrors << std::endl;
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cout << "Arguments are <socket mysqld> <connect_string cluster>.\n";
    exit(-1);
  }
  const char *mysqld_sock   = argv[1];
  const char *connectstring = argv[2];

  mysql_connect_and_create(mysqld_sock);

  ndb_init();
  ndb_run_async_inserts(connectstring);
  ndb_end(0);
  return 0;
}
