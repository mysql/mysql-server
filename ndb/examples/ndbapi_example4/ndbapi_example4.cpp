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
//  ndbapi_example4.cpp: Using secondary indexes in NDB API
//
//  Correct output from this program is:
//
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

#include <NdbApi.hpp>

// Used for cout
#include <stdio.h>
#include <iostream>

#define APIERROR(error) \
  { std::cout << "Error in " << __FILE__ << ", line:" << __LINE__ << ", code:" \
              << error.code << ", msg: " << error.message << "." << std::endl; \
    exit(-1); }

int main()
{
  ndb_init();

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(); // Object representing the cluster

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

  Ndb* myNdb = new Ndb( cluster_connection,
			"TEST_DB_1" );  // Object representing the database
  NdbDictionary::Table myTable;
  NdbDictionary::Column myColumn;
  NdbDictionary::Index myIndex;

  NdbTransaction	*myTransaction;     // For transactions
  NdbOperation	 	*myOperation;      // For primary key operations
  NdbIndexOperation	*myIndexOperation; // For index operations
  NdbRecAttr     	*myRecAttr;        // Result of reading attribute value
  
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  /*********************************************************
   * Create a table named MYTABLENAME if it does not exist *
   *********************************************************/
  NdbDictionary::Dictionary* myDict = myNdb->getDictionary();
  if (myDict->getTable("MYTABLENAME") != NULL) {
    std::cout << "NDB already has example table: MYTABLENAME." << std::endl; 
    exit(-1);
  } 

  myTable.setName("MYTABLENAME");
  
  myColumn.setName("ATTR1");
  myColumn.setType(NdbDictionary::Column::Unsigned);
  myColumn.setLength(1);
  myColumn.setPrimaryKey(true);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  myColumn.setName("ATTR2");
  myColumn.setType(NdbDictionary::Column::Unsigned);
  myColumn.setLength(1);
  myColumn.setPrimaryKey(false);
  myColumn.setNullable(false);
  myTable.addColumn(myColumn);

  if (myDict->createTable(myTable) == -1) 
      APIERROR(myDict->getNdbError());


  /**********************************************************
   * Create an index named MYINDEXNAME if it does not exist *
   **********************************************************/
  if (myDict->getIndex("MYINDEXNAME", "MYTABLENAME") != NULL) {
    std::cout << "NDB already has example index: MYINDEXNAME." << std::endl; 
    exit(-1);
  } 

  myIndex.setName("MYINDEXNAME");
  myIndex.setTable("MYTABLENAME");
  myIndex.setType(NdbDictionary::Index::UniqueHashIndex);
  const char* attr_arr[] = {"ATTR2"};
  myIndex.addIndexColumns(1, attr_arr);

  if (myDict->createIndex(myIndex) == -1) 
      APIERROR(myDict->getNdbError());


  /**************************************************************************
   * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
   **************************************************************************/
  for (int i = 0; i < 5; i++) {
    myTransaction = myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    myOperation = myTransaction->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myOperation->insertTuple();
    myOperation->equal("ATTR1", i);
    myOperation->setValue("ATTR2", i);

    myOperation = myTransaction->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->insertTuple();
    myOperation->equal("ATTR1", i+5);
    myOperation->setValue("ATTR2", i+5);
    
    if (myTransaction->execute( Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    myNdb->closeTransaction(myTransaction);
  }
  
  /*****************************************
   * Read and print all tuples using index *
   *****************************************/
  std::cout << "ATTR1 ATTR2" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    myTransaction = myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    myIndexOperation = myTransaction->getNdbIndexOperation("MYINDEXNAME",
							   "MYTABLENAME");
    if (myIndexOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myIndexOperation->readTuple();
    myIndexOperation->equal("ATTR2", i);
    
    myRecAttr = myIndexOperation->getValue("ATTR1", NULL);
    if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());

    if(myTransaction->execute( Commit ) != -1)
      printf(" %2d    %2d\n", myRecAttr->u_32_value(), i);
    }
    myNdb->closeTransaction(myTransaction);

  /*****************************************************************
   * Update the second attribute in half of the tuples (adding 10) *
   *****************************************************************/
  for (int i = 0; i < 10; i+=2) {
    myTransaction = myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    myIndexOperation = myTransaction->getNdbIndexOperation("MYINDEXNAME",
							   "MYTABLENAME");
    if (myIndexOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myIndexOperation->updateTuple();
    myIndexOperation->equal( "ATTR2", i );
    myIndexOperation->setValue( "ATTR2", i+10);
    
    if( myTransaction->execute( Commit ) == -1 ) 
      APIERROR(myTransaction->getNdbError());
    
    myNdb->closeTransaction(myTransaction);
  }
  
  /*************************************************
   * Delete one tuple (the one with primary key 3) *
   *************************************************/
  myTransaction = myNdb->startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
  
  myIndexOperation = myTransaction->getNdbIndexOperation("MYINDEXNAME",
							 "MYTABLENAME");
  if (myIndexOperation == NULL) 
    APIERROR(myTransaction->getNdbError());
  
  myIndexOperation->deleteTuple();
  myIndexOperation->equal( "ATTR2", 3 );
  
  if (myTransaction->execute(Commit) == -1) 
    APIERROR(myTransaction->getNdbError());
  
  myNdb->closeTransaction(myTransaction);
  
  /*****************************
   * Read and print all tuples *
   *****************************/
  std::cout << "ATTR1 ATTR2" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    myTransaction = myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());
    
    myOperation = myTransaction->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
    myOperation->readTuple();
    myOperation->equal("ATTR1", i);
    
    myRecAttr = myOperation->getValue("ATTR2", NULL);
    if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());
    
    if(myTransaction->execute( Commit ) == -1)
      if (i == 3) {
	std::cout << "Detected that deleted tuple doesn't exist!" << std::endl;
      } else {
	APIERROR(myTransaction->getNdbError());
      }
    
    if (i != 3) {
      printf(" %2d    %2d\n", i, myRecAttr->u_32_value());
    }
    myNdb->closeTransaction(myTransaction);
  }

  /**************
   * Drop index *
   **************/
  if (myDict->dropIndex("MYINDEXNAME", "MYTABLENAME") == -1) 
    APIERROR(myDict->getNdbError());

  /**************
   * Drop table *
   **************/
  if (myDict->dropTable("MYTABLENAME") == -1) 
    APIERROR(myDict->getNdbError());

  delete myNdb;
  delete cluster_connection;

  ndb_end(0);
  return 0;
}
