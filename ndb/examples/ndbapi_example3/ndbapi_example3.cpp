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

// 
//  ndbapi_example3.cpp: Error handling and transaction retries
//
//  Execute ndbapi_example1 to create the table "MYTABLENAME"
//  before executing this program.
//
//  There are many ways to program using the NDB API.  In this example
//  we execute two inserts in the same transaction using 
//  NdbConnection::Ndbexecute(NoCommit).
// 
//  Transaction failing is handled by re-executing the transaction
//  in case of non-permanent transaction errors.
//  Application errors (i.e. errors at points marked with APIERROR) 
//  should be handled by the application programmer.

#include <NdbApi.hpp>

// Used for cout
#include <iostream>  

// Used for sleep (use your own version of sleep)
#include <unistd.h>
#define TIME_TO_SLEEP_BETWEEN_TRANSACTION_RETRIES 1

//
//  APIERROR prints an NdbError object
//
#define APIERROR(error) \
  { std::cout << "API ERROR: " << error.code << " " << error.message \
              << std::endl \
              << "           " << "Status: " << error.status \
              << ", Classification: " << error.classification << std::endl\
              << "           " << "File: " << __FILE__ \
              << " (Line: " << __LINE__ << ")" << std::endl \
              ; \
  }

//
//  CONERROR prints all error info regarding an NdbConnection
//
#define CONERROR(ndbConnection) \
  { NdbError error = ndbConnection->getNdbError(); \
    std::cout << "CON ERROR: " << error.code << " " << error.message \
              << std::endl \
              << "           " << "Status: " << error.status \
              << ", Classification: " << error.classification << std::endl \
              << "           " << "File: " << __FILE__ \
              << " (Line: " << __LINE__ << ")" << std::endl \
              ; \
    printTransactionError(ndbConnection); \
  }

void printTransactionError(NdbConnection *ndbConnection) {
  const NdbOperation *ndbOp = NULL;
  int i=0;

  /****************************************************************
   * Print NdbError object of every operations in the transaction *
   ****************************************************************/
  while ((ndbOp = ndbConnection->getNextCompletedOperation(ndbOp)) != NULL) {
    NdbError error = ndbOp->getNdbError();
    std::cout << "           OPERATION " << i+1 << ": " 
	      << error.code << " " << error.message << std::endl
	      << "           Status: " << error.status 
	      << ", Classification: " << error.classification << std::endl;
    i++;
  }
}


//
//  Example insert
//  @param myNdb         Ndb object representing NDB Cluster
//  @param myConnection  NdbConnection used for transaction
//  @param error         NdbError object returned in case of errors
//  @return -1 in case of failures, 0 otherwise
//
int insert(int transactionId, NdbConnection* myConnection) {
  NdbOperation	 *myOperation;          // For other operations

  myOperation = myConnection->getNdbOperation("MYTABLENAME");
  if (myOperation == NULL) return -1;
  
  if (myOperation->insertTuple() ||  
      myOperation->equal("ATTR1", transactionId) ||
      myOperation->setValue("ATTR2", transactionId)) {
    APIERROR(myOperation->getNdbError());
    exit(-1);
  }

  return myConnection->execute(NoCommit);
}


//
//  Execute function which re-executes (tries 10 times) the transaction 
//  if there are temporary errors (e.g. the NDB Cluster is overloaded).
//  @return -1 failure, 1 success
//
int executeInsertTransaction(int transactionId, Ndb* myNdb) {
  int result = 0;                       // No result yet
  int noOfRetriesLeft = 10;
  NdbConnection	 *myConnection;         // For other transactions
  NdbError ndberror;
  
  while (noOfRetriesLeft > 0 && !result) {
    
    /*********************************
     * Start and execute transaction *
     *********************************/
    myConnection = myNdb->startTransaction();
    if (myConnection == NULL) {
      APIERROR(myNdb->getNdbError());
      ndberror = myNdb->getNdbError();
      result = -1;  // Failure
    } else if (insert(transactionId, myConnection) || 
	       insert(10000+transactionId, myConnection) ||
	       myConnection->execute(Commit)) {
      CONERROR(myConnection);
      ndberror = myConnection->getNdbError();
      result = -1;  // Failure
    } else {
      result = 1;   // Success
    }

    /**********************************
     * If failure, then analyze error *
     **********************************/
    if (result == -1) {                 
      switch (ndberror.status) {
      case NdbError::Success:
	break;
      case NdbError::TemporaryError:
	std::cout << "Retrying transaction..." << std::endl;
	sleep(TIME_TO_SLEEP_BETWEEN_TRANSACTION_RETRIES);
	--noOfRetriesLeft;
	result = 0;   // No completed transaction yet
	break;
	
      case NdbError::UnknownResult:
      case NdbError::PermanentError:
	std::cout << "No retry of transaction..." << std::endl;
	result = -1;  // Permanent failure
	break;
      }
    }

    /*********************
     * Close transaction *
     *********************/
    if (myConnection != NULL) {
      myNdb->closeTransaction(myConnection);
    }
  }

  if (result != 1) exit(-1);
  return result;
}


int main()
{
  ndb_init();
  Ndb* myNdb = new Ndb( "TEST_DB_1" );  // Object representing the database
  
  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  
  /************************************
   * Execute some insert transactions *
   ************************************/
  for (int i = 10000; i < 20000; i++) {
    executeInsertTransaction(i, myNdb);
  }
  
  delete myNdb;
}
