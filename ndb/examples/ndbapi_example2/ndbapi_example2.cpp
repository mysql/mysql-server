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
//  ndbapi_example2.cpp: Using asynchronous transactions in NDB API
//
//  Execute ndbapi_example1 to create the table "MYTABLENAME"
//  before executing this program.
// 
//  Correct output from this program is:
//
//  Successful insert.
//  Successful insert.

#include <NdbApi.hpp>

// Used for cout
#include <iostream>

#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

static void callback(int result, NdbConnection* NdbObject, void* aObject);

int main()
{
  ndb_init();
  Ndb* myNdb = new Ndb( "TEST_DB_2" );  // Object representing the database

  NdbConnection*  myNdbConnection[2];   // For transactions
  NdbOperation*   myNdbOperation;       // For operations
  
  /*******************************************
   * Initialize NDB and wait until its ready *
   *******************************************/
  if (myNdb->init(2) == -1) {          // Want two parallel insert transactions
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }

  /******************************************************
   * Insert (we do two insert transactions in parallel) *
   ******************************************************/
  for (int i = 0; i < 2; i++) {
    myNdbConnection[i] = myNdb->startTransaction();
    if (myNdbConnection[i] == NULL) APIERROR(myNdb->getNdbError());
    
    myNdbOperation = myNdbConnection[i]->getNdbOperation("MYTABLENAME");
    // Error check. If error, then maybe table MYTABLENAME is not in database
    if (myNdbOperation == NULL) APIERROR(myNdbConnection[i]->getNdbError());
    
    myNdbOperation->insertTuple();
    myNdbOperation->equal("ATTR1", 20 + i);
    myNdbOperation->setValue("ATTR2", 20 + i);
    
    // Prepare transaction (the transaction is NOT yet sent to NDB)
    myNdbConnection[i]->executeAsynchPrepare(Commit, &callback, NULL);
  }

  // Send all transactions to NDB 
  myNdb->sendPreparedTransactions(0);
  
  // Poll all transactions
  myNdb->pollNdb(3000, 2);
  
  // Close all transactions
  for (int i = 0; i < 2; i++) 
    myNdb->closeTransaction(myNdbConnection[i]);

  delete myNdb;
}

/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbConnection object, and 
 *   - A pointer to an arbitrary object.)
 */

static void
callback(int result, NdbConnection* myTrans, void* aObject)
{
  if (result == -1) {
    std::cout << "Poll error: " << std::endl; 
    APIERROR(myTrans->getNdbError());
  } else {
    std::cout << "Successful insert." << std::endl;
  }
}
