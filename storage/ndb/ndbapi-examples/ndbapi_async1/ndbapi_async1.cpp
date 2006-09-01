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
//  ndbapi_async1.cpp: Using asynchronous transactions in NDB API
//
// 
//  Correct output from this program is:
//
//  Successful insert.
//  Successful insert.

#include <mysql.h>
#include <NdbApi.hpp>

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

  if (cluster_connection->wait_until_ready(30,0))
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
  mysql_query(&mysql, "CREATE DATABASE TEST_DB_1");
  if (mysql_query(&mysql, "USE TEST_DB_1") != 0) MYSQLERROR(mysql);
  create_table(mysql);

  Ndb* myNdb = new Ndb( cluster_connection,
			"TEST_DB_1" );  // Object representing the database

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

  drop_table(mysql);

  ndb_end(0);
  return 0;
}

/*********************************************************
 * Create a table named MYTABLENAME if it does not exist *
 *********************************************************/
static void create_table(MYSQL &mysql)
{
  if (mysql_query(&mysql, 
		  "CREATE TABLE"
		  "  MYTABLENAME"
		  "    (ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,"
		  "     ATTR2 INT UNSIGNED NOT NULL)"
		  "  ENGINE=NDB"))
    MYSQLERROR(mysql);
}

/***********************************
 * Drop a table named MYTABLENAME 
 ***********************************/
static void drop_table(MYSQL &mysql)
{
  if (mysql_query(&mysql, 
		  "DROP TABLE"
		  "  MYTABLENAME"))
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
