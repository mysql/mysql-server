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
//  ndbapi_example1.cpp: Using synchronous transactions in NDB API
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
  Ndb* myNdb = new Ndb( "TEST_DB_1" );  // Object representing the database
  NdbDictionary::Table myTable;
  NdbDictionary::Column myColumn;

  NdbConnection	 *myConnection;         // For other transactions
  NdbOperation	 *myOperation;          // For other operations
  NdbRecAttr     *myRecAttr;            // Result of reading attribute value
  
  /********************************************
   * Initialize NDB and wait until it's ready *
   ********************************************/
  if (myNdb->init() == -1) { 
    APIERROR(myNdb->getNdbError());
    exit(-1);
  }

  if (myNdb->waitUntilReady(30) != 0) {
    std::cout << "NDB was not ready within 30 secs." << std::endl;
    exit(-1);
  }
  
  NdbDictionary::Dictionary* myDict = myNdb->getDictionary();
  
  /*********************************************************
   * Create a table named MYTABLENAME if it does not exist *
   *********************************************************/
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

  /**************************************************************************
   * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
   **************************************************************************/
  for (int i = 0; i < 5; i++) {
    myConnection = myNdb->startTransaction();
    if (myConnection == NULL) APIERROR(myNdb->getNdbError());
    
    myOperation = myConnection->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myConnection->getNdbError());
    
    myOperation->insertTuple();
    myOperation->equal("ATTR1", i);
    myOperation->setValue("ATTR2", i);

    myOperation = myConnection->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myConnection->getNdbError());

    myOperation->insertTuple();
    myOperation->equal("ATTR1", i+5);
    myOperation->setValue("ATTR2", i+5);
    
    if (myConnection->execute( Commit ) == -1)
      APIERROR(myConnection->getNdbError());
    
    myNdb->closeTransaction(myConnection);
  }
  
  /*****************************************************************
   * Update the second attribute in half of the tuples (adding 10) *
   *****************************************************************/
  for (int i = 0; i < 10; i+=2) {
    myConnection = myNdb->startTransaction();
    if (myConnection == NULL) APIERROR(myNdb->getNdbError());
    
    myOperation = myConnection->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myConnection->getNdbError());
    
    myOperation->updateTuple();
    myOperation->equal( "ATTR1", i );
    myOperation->setValue( "ATTR2", i+10);
    
    if( myConnection->execute( Commit ) == -1 ) 
      APIERROR(myConnection->getNdbError());
    
    myNdb->closeTransaction(myConnection);
  }
  
  /*************************************************
   * Delete one tuple (the one with primary key 3) *
   *************************************************/
  myConnection = myNdb->startTransaction();
  if (myConnection == NULL) APIERROR(myNdb->getNdbError());
  
  myOperation = myConnection->getNdbOperation("MYTABLENAME");	
  if (myOperation == NULL) 
    APIERROR(myConnection->getNdbError());
  
  myOperation->deleteTuple();
  myOperation->equal( "ATTR1", 3 );
  
  if (myConnection->execute(Commit) == -1) 
    APIERROR(myConnection->getNdbError());
  
  myNdb->closeTransaction(myConnection);
  
  /*****************************
   * Read and print all tuples *
   *****************************/
  std::cout << "ATTR1 ATTR2" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    myConnection = myNdb->startTransaction();
    if (myConnection == NULL) APIERROR(myNdb->getNdbError());
    
    myOperation = myConnection->getNdbOperation("MYTABLENAME");	
    if (myOperation == NULL) APIERROR(myConnection->getNdbError());
    
    myOperation->readTuple();
    myOperation->equal("ATTR1", i);
    
    myRecAttr = myOperation->getValue("ATTR2", NULL);
    if (myRecAttr == NULL) APIERROR(myConnection->getNdbError());
    
    if(myConnection->execute( Commit ) == -1)
      if (i == 3) {
	std::cout << "Detected that deleted tuple doesn't exist!" << std::endl;
      } else {
	APIERROR(myConnection->getNdbError());
      }
    
    if (i != 3) {
      printf(" %2d    %2d\n", i, myRecAttr->u_32_value());
    }
    myNdb->closeTransaction(myConnection);
  }
  delete myNdb;
}
