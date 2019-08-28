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
//  ndbapi_async1.cpp: Using asynchronous transactions in NDB API
//
// 
//  Correct output from this program is:
//
//  Successful insert.
//  Successful insert.

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>

#include <stdlib.h>
// Used for cout
#include <iostream>


#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(-1); }
#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

static void create_table(MYSQL &);
static void drop_table(MYSQL &);
static void callback(int result, NdbTransaction* NdbObject, void* aObject);

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

  if (cluster_connection->wait_until_ready(30,0) < 0)
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

  Ndb* myNdb = new Ndb( cluster_connection,
			"ndb_examples" );  // Object representing the database

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
  const NdbDictionary::Table *myTable= myDict->getTable("api_async1");
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

/*********************************************************
 * Create a table named api_async1 if it does not exist *
 *********************************************************/
static void create_table(MYSQL &mysql)
{
  while(mysql_query(&mysql, 
		  "CREATE TABLE api_async1"
		  "    (ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,"
		  "     ATTR2 INT UNSIGNED NOT NULL)"
		  "  ENGINE=NDB"))
  {
      if (mysql_errno(&mysql) == ER_TABLE_EXISTS_ERROR)
      {
          std::cout << "MySQL Cluster already has example table: api_scan. "
          << "Dropping it..." << std::endl; 
          drop_table(mysql);
      }
      else MYSQLERROR(mysql);
  }
}

/***********************************
 * Drop a table named api_async1 
 ***********************************/
static void drop_table(MYSQL &mysql)
{
  if (mysql_query(&mysql, 
		  "DROP TABLE"
		  "  api_async1"))
    MYSQLERROR(mysql);
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
    std::cout << "Poll error: " << std::endl; 
    APIERROR(myTrans->getNdbError());
  } else {
    std::cout << "Successful insert." << std::endl;
  }
}
