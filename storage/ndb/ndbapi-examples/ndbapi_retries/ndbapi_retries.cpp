/*
   Copyright (c) 2005, 2014, Oracle and/or its affiliates. All rights reserved.

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

// 
//  ndbapi_retries.cpp: Error handling and transaction retries
//
//  There are many ways to program using the NDB API.  In this example
//  we execute two inserts in the same transaction using 
//  NdbConnection::execute(NoCommit).
// 
//  Transaction failing is handled by re-executing the transaction
//  in case of non-permanent transaction errors.
//  Application errors (i.e. errors at points marked with APIERROR) 
//  should be handled by the application programmer.

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>

#include <stdlib.h>
// Used for cout
#include <iostream>  

#ifdef _WIN32
#include <windows.h>
// Emulate sleep with Sleep
static inline
void sleep(unsigned int seconds)
{
  Sleep(seconds/1000);
}
#else
// Used for sleep (use your own version of sleep)
#include <unistd.h>
#endif
#define TIME_TO_SLEEP_BETWEEN_TRANSACTION_RETRIES 1

#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }

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
//  TRANSERROR prints all error info regarding an NdbTransaction
//
#define TRANSERROR(ndbTransaction) \
  { NdbError error = ndbTransaction->getNdbError(); \
    std::cout << "TRANS ERROR: " << error.code << " " << error.message \
              << std::endl \
              << "           " << "Status: " << error.status \
              << ", Classification: " << error.classification << std::endl \
              << "           " << "File: " << __FILE__ \
              << " (Line: " << __LINE__ << ")" << std::endl \
              ; \
    printTransactionError(ndbTransaction); \
  }

void printTransactionError(NdbTransaction *ndbTransaction) {
  const NdbOperation *ndbOp = NULL;
  int i=0;

  /****************************************************************
   * Print NdbError object of every operations in the transaction *
   ****************************************************************/
  while ((ndbOp = ndbTransaction->getNextCompletedOperation(ndbOp)) != NULL) {
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
//  @param myNdb          Ndb object representing NDB Cluster
//  @param myTransaction  NdbTransaction used for transaction
//  @param myTable        Table to insert into
//  @param error          NdbError object returned in case of errors
//  @return -1 in case of failures, 0 otherwise
//
int insert(int transactionId, NdbTransaction* myTransaction,
	   const NdbDictionary::Table *myTable) {
  NdbOperation	 *myOperation;          // For other operations

  myOperation = myTransaction->getNdbOperation(myTable);
  if (myOperation == NULL) return -1;
  
  if (myOperation->insertTuple() ||  
      myOperation->equal("ATTR1", transactionId) ||
      myOperation->setValue("ATTR2", transactionId)) {
    APIERROR(myOperation->getNdbError());
    exit(-1);
  }

  return myTransaction->execute(NdbTransaction::NoCommit);
}


//
//  Execute function which re-executes (tries 10 times) the transaction 
//  if there are temporary errors (e.g. the NDB Cluster is overloaded).
//  @return -1 failure, 1 success
//
int executeInsertTransaction(int transactionId, Ndb* myNdb,
			     const NdbDictionary::Table *myTable) {
  int result = 0;                       // No result yet
  int noOfRetriesLeft = 10;
  NdbTransaction	 *myTransaction;         // For other transactions
  NdbError ndberror;
  
  while (noOfRetriesLeft > 0 && !result) {
    
    /*********************************
     * Start and execute transaction *
     *********************************/
    myTransaction = myNdb->startTransaction();
    if (myTransaction == NULL) {
      APIERROR(myNdb->getNdbError());
      ndberror = myNdb->getNdbError();
      result = -1;  // Failure
    } else if (insert(transactionId, myTransaction, myTable) || 
	       insert(10000+transactionId, myTransaction, myTable) ||
	       myTransaction->execute(NdbTransaction::Commit)) {
      TRANSERROR(myTransaction);
      ndberror = myTransaction->getNdbError();
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
    if (myTransaction != NULL) {
      myNdb->closeTransaction(myTransaction);
    }
  }

  if (result != 1) exit(-1);
  return result;
}

/*********************************************************
 * Create a table named api_retries if it does not exist *
 *********************************************************/
static void create_table(MYSQL &mysql)
{
  while(mysql_query(&mysql, 
		  "CREATE TABLE "
		  "  api_retries"
		  "    (ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,"
		  "     ATTR2 INT UNSIGNED NOT NULL)"
		  "  ENGINE=NDB"))
  {
    if (mysql_errno(&mysql) == ER_TABLE_EXISTS_ERROR)
    {
      std::cout << "MySQL Cluster already has example table: api_scan. "
	     << "Dropping it..." << std::endl; 
        mysql_query(&mysql, "DROP TABLE api_retries");
    }
    else MYSQLERROR(mysql);
  }
}


int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cout << "Arguments are <socket mysqld> <connect_string cluster>.\n";
    exit(-1);
  }
  char * mysqld_sock  = argv[1];
  const char *connectstring = argv[2];
  ndb_init();

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(connectstring); // Object representing the cluster

  int r= cluster_connection->connect(5 /* retries               */,
				     3 /* delay between retries */,
				     1 /* verbose               */);
  if (r > 0)
  {
    std::cout
      << "Cluster connect failed, possibly resolved with more retries.\n";
    exit(-1);
  }
  else if (r < 0)
  {
    std::cout
      << "Cluster connect failed.\n";
    exit(-1);
  }
					   
  if (cluster_connection->wait_until_ready(30,30))
  {
    std::cout << "Cluster was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  // connect to mysql server
  MYSQL mysql;
  if ( !mysql_init(&mysql) ) {
    std::cout << "mysql_init failed\n";
    exit(-1);
  }
  if ( !mysql_real_connect(&mysql, "localhost", "root", "", "",
			   0, mysqld_sock, 0) )
    MYSQLERROR(mysql);
  
  /********************************************
   * Connect to database via mysql-c          *
   ********************************************/
  mysql_query(&mysql, "CREATE DATABASE ndb_examples");
  if (mysql_query(&mysql, "USE ndb_examples") != 0) MYSQLERROR(mysql);
  create_table(mysql);

  Ndb* myNdb= new Ndb( cluster_connection,
		       "ndb_examples" );  // Object representing the database
  
  if (myNdb->init() == -1) {
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_retries");
  if (myTable == NULL)
  {
    APIERROR(myDict->getNdbError());
    return -1;
  }
  /************************************
   * Execute some insert transactions *
   ************************************/
   
  std::cout << "Ready to insert rows.  You will see notices for temporary "
    "errors, permenant errors, and retries. \n";
  for (int i = 10000; i < 20000; i++) {
    executeInsertTransaction(i, myNdb, myTable);
  }
  std::cout << "Done.\n";
  
  delete myNdb;
  delete cluster_connection;
    
  ndb_end(0);
  return 0;
}
