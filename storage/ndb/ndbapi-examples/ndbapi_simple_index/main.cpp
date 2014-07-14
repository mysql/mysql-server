/*
   Copyright (c) 2005, 2014, Oracle and/or its affiliates. All rights reserved.

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
//  ndbapi_simple_index.cpp: Using secondary indexes in NDB API
//
//  Correct output from this program is:
//
//  ATTR1 ATTR2
//    0     0
//    1     1
//    2     2
//    3     3
//    4     4
//    5     5
//    6     6
//    7     7
//    8     8
//    9     9
//  ATTR1 ATTR2
//    0    10
//    1     1
//    2    12
//  Detected that deleted tuple doesn't exist!
//    4    14
//    5     5
//    6    16
//    7     7
//    8    18
//    9     9

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <NdbApi.hpp>

#include <stdlib.h>
// Used for cout
#include <stdio.h>
#include <iostream>

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
  MYSQL mysql;

  /**************************************************************
   * Connect to mysql server and create table                   *
   **************************************************************/
  {
    if ( !mysql_init(&mysql) ) {
      std::cout << "mysql_init failed\n";
      exit(-1);
    }
    if ( !mysql_real_connect(&mysql, "localhost", "root", "", "",
			     0, mysqld_sock, 0) )
      MYSQLERROR(mysql);

    mysql_query(&mysql, "CREATE DATABASE ndb_examples");
    if (mysql_query(&mysql, "USE ndb_examples") != 0) MYSQLERROR(mysql);

    while (mysql_query(&mysql, 
		    "CREATE TABLE"
		    "  api_simple_index"
		    "    (ATTR1 INT UNSIGNED,"
		    "     ATTR2 INT UNSIGNED NOT NULL,"
		    "     PRIMARY KEY USING HASH (ATTR1),"
		    "     UNIQUE MYINDEXNAME USING HASH (ATTR2))"
		    "  ENGINE=NDB"))
    {
      if (mysql_errno(&mysql) == ER_TABLE_EXISTS_ERROR)
      {
        std::cout << "MySQL Cluster already has example table: api_scan. "
        << "Dropping it..." << std::endl; 
        mysql_query(&mysql, "DROP TABLE api_simple_index");
      }
      else MYSQLERROR(mysql);
    }
  }
 
  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(connectstring); // Object representing the cluster

  if (cluster_connection->connect(5,3,1))
  {
    std::cout << "Connect to cluster management server failed.\n";
    exit(-1);
  }

  if (cluster_connection->wait_until_ready(30,30))
  {
    std::cout << "Cluster was not ready within 30 secs.\n";
    exit(-1);
  }

  Ndb* myNdb = new Ndb( cluster_connection,
			"ndb_examples" );  // Object representing the database
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  const NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_simple_index");
  if (myTable == NULL)
    APIERROR(myDict->getNdbError());
  const NdbDictionary::Index *myIndex= myDict->getIndex("MYINDEXNAME$unique","api_simple_index");
  if (myIndex == NULL)
    APIERROR(myDict->getNdbError());

  /**************************************************************************
   * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
   **************************************************************************/
  for (int i = 0; i < 5; i++) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myOperation->insertTuple();
    myOperation->equal("ATTR1", i);
    myOperation->setValue("ATTR2", i);

    myOperation = myTransaction->getNdbOperation(myTable);	
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->insertTuple();
    myOperation->equal("ATTR1", i+5);
    myOperation->setValue("ATTR2", i+5);
    
    if (myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    myNdb->closeTransaction(myTransaction);
  }
  
  /*****************************************
   * Read and print all tuples using index *
   *****************************************/
  std::cout << "ATTR1 ATTR2" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    NdbIndexOperation *myIndexOperation=
      myTransaction->getNdbIndexOperation(myIndex);
    if (myIndexOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myIndexOperation->readTuple(NdbOperation::LM_Read);
    myIndexOperation->equal("ATTR2", i);
    
    NdbRecAttr *myRecAttr= myIndexOperation->getValue("ATTR1", NULL);
    if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());

    if(myTransaction->execute( NdbTransaction::Commit,
                               NdbOperation::AbortOnError ) != -1)
      printf(" %2d    %2d\n", myRecAttr->u_32_value(), i);

    myNdb->closeTransaction(myTransaction);
  }

  /*****************************************************************
   * Update the second attribute in half of the tuples (adding 10) *
   *****************************************************************/
  for (int i = 0; i < 10; i+=2) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    NdbIndexOperation *myIndexOperation=
      myTransaction->getNdbIndexOperation(myIndex);
    if (myIndexOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myIndexOperation->updateTuple();
    myIndexOperation->equal( "ATTR2", i );
    myIndexOperation->setValue( "ATTR2", i+10);
    
    if( myTransaction->execute( NdbTransaction::Commit ) == -1 ) 
      APIERROR(myTransaction->getNdbError());
    
    myNdb->closeTransaction(myTransaction);
  }
  
  /*************************************************
   * Delete one tuple (the one with primary key 3) *
   *************************************************/
  {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
  
    NdbIndexOperation *myIndexOperation=
      myTransaction->getNdbIndexOperation(myIndex);
    if (myIndexOperation == NULL) APIERROR(myTransaction->getNdbError());
  
    myIndexOperation->deleteTuple();
    myIndexOperation->equal( "ATTR2", 3 );
  
    if (myTransaction->execute(NdbTransaction::Commit) == -1) 
      APIERROR(myTransaction->getNdbError());
  
    myNdb->closeTransaction(myTransaction);
  }

  /*****************************
   * Read and print all tuples *
   *****************************/
  {
    std::cout << "ATTR1 ATTR2" << std::endl;
  
    for (int i = 0; i < 10; i++) {
      NdbTransaction *myTransaction= myNdb->startTransaction();
      if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
      
      NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
      if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
      myOperation->readTuple(NdbOperation::LM_Read);
      myOperation->equal("ATTR1", i);
    
      NdbRecAttr *myRecAttr= myOperation->getValue("ATTR2", NULL);
      if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());
    
      if(myTransaction->execute( NdbTransaction::Commit,
                                 NdbOperation::AbortOnError ) == -1)
      {
	if (i == 3) {
	  std::cout << "Detected that deleted tuple doesn't exist!\n";
	} else {
	  APIERROR(myTransaction->getNdbError());
	}
      }
    
      if (i != 3) {
	printf(" %2d    %2d\n", i, myRecAttr->u_32_value());
      }
      myNdb->closeTransaction(myTransaction);
    }
  }

  delete myNdb;
  delete cluster_connection;

  ndb_end(0);
  return 0;
}
