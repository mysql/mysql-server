/*
   Copyright (C) 2007 MySQL AB
    All rights reserved. Use is subject to license terms.

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

//
//  ndbapi_async1.cpp: Using asynchronous transactions in NDB API
//
//  Execute ndbapi_example1 to create the table "MYTABLENAME"
//  before executing this program.
// 
//  Correct output from this program is:
//
//  Successful insert.
//  Successful insert.

#include <mysql.h>
#include <NdbApi.hpp>

// Used for cout
#include <stdio.h>
#include <NdbOut.hpp>

#define APIERROR(error) \
  { ndbout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << endl; \
    exit(-1); }

static void callback(int result, NdbTransaction* NdbObject, void* aObject);

int main()
{
  ndb_init();

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(); // Object representing the cluster

  if (cluster_connection->wait_until_ready(30,30))
  {
    ndbout << "Cluster was not ready within 30 secs." << endl;
    exit(-1);
  }

  int r= cluster_connection->connect(5 /* retries               */,
				     3 /* delay between retries */,
				     1 /* verbose               */);
  if (r > 0)
  {
    ndbout
      << "Cluster connect failed, possibly resolved with more retries.\n";
    exit(-1);
  }
  else if (r < 0)
  {
    ndbout
      << "Cluster connect failed.\n";
    exit(-1);
  }
					   
  if (cluster_connection->wait_until_ready(30,30))
  {
    ndbout << "Cluster was not ready within 30 secs." << endl;
    exit(-1);
  }

  Ndb* myNdb = new Ndb( cluster_connection,
			"TEST_DB_2" );  // Object representing the database

  NdbTransaction*  myNdbTransaction[2];   // For transactions
  NdbOperation*   myNdbOperation;       // For operations
  
  if (myNdb->init(2) == -1) {          // Want two parallel insert transactions
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  /******************************************************
   * Insert (we do two insert transactions in parallel) *
   ******************************************************/
  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("MYTABLENAME");
  if (myTable == NULL)
    APIERROR(myDict->getNdbError());
  for (int i = 0; i < 2; i++) {
    myNdbTransaction[i] = myNdb->startTransaction();
    if (myNdbTransaction[i] == NULL) APIERROR(myNdb->getNdbError());
    
    myNdbOperation = myNdbTransaction[i]->getNdbOperation(myTable);
    if (myNdbOperation == NULL) APIERROR(myNdbTransaction[i]->getNdbError());
    
    myNdbOperation->insertTuple();
    myNdbOperation->equal("ATTR1", 20 + i);
    myNdbOperation->setValue("ATTR2", 20 + i);
    
    // Prepare transaction (the transaction is NOT yet sent to NDB)
    myNdbTransaction[i]->executeAsynchPrepare(NdbTransaction::Commit,
					      &callback, NULL);
  }

  // Send all transactions to NDB 
  myNdb->sendPreparedTransactions(0);
  
  // Poll all transactions
  myNdb->pollNdb(3000, 2);
  
  // Close all transactions
  for (int i = 0; i < 2; i++) 
    myNdb->closeTransaction(myNdbTransaction[i]);

  delete myNdb;
  delete cluster_connection;

  ndb_end(0);
  return 0;
}

/*
 *   callback : This is called when the transaction is polled
 *              
 *   (This function must have three arguments: 
 *   - The result of the transaction, 
 *   - The NdbTransaction object, and 
 *   - A pointer to an arbitrary object.)
 */

static void
callback(int result, NdbTransaction* myTrans, void* aObject)
{
  if (result == -1) {
    ndbout << "Poll error: " << endl; 
    APIERROR(myTrans->getNdbError());
  } else {
    ndbout << "Successful insert." << endl;
  }
}
