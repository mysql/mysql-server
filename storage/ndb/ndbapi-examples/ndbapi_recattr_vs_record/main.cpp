/*
   Copyright (c) 2008, 2014, Oracle and/or its affiliates. All rights reserved.

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

/* 
 *  ndbapi_recattr_vs_record.cpp: Kitchen-sink example showing usage of
 *  NdbRecAttr based and NdbRecord interfaces to NDBAPI.
 *
 *  A number of different aspects of the two APIs are exercised, with
 *  parallel implementations to show how the same tasks are accomplished
 *  in each.  Some tasks cannot be accomplished via both APIs and so are
 *  missing from one or another.
 *
 *  A simple schema is used, but the mechanisms generally extend to use
 *  with different types.
 *
 */

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <NdbApi.hpp>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Used for cout
#include <iostream>


// Do we use old-style (NdbRecAttr?) or new style (NdbRecord?)
enum ApiType {api_attr, api_record};

static void run_application(MYSQL &, Ndb_cluster_connection &, ApiType);

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
  if (argc != 4)
  {
    std::cout << "Arguments are <socket mysqld> <connect_string cluster> <attr|record>.\n";
    exit(-1);
  }
  // ndb_init must be called first
  ndb_init();

  // connect to mysql server and cluster and run application
  {
    char * mysqld_sock  = argv[1];
    const char *connectstring = argv[2];
    ApiType accessType=api_attr;

    // Object representing the cluster
    Ndb_cluster_connection cluster_connection(connectstring);

    // Connect to cluster management server (ndb_mgmd)
    if (cluster_connection.connect(4 /* retries               */,
				   5 /* delay between retries */,
				   1 /* verbose               */))
    {
      std::cout << "Cluster management server was not ready within 30 secs.\n";
      exit(-1);
    }

    // Optionally connect and wait for the storage nodes (ndbd's)
    if (cluster_connection.wait_until_ready(30,0) < 0)
    {
      std::cout << "Cluster was not ready within 30 secs.\n";
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
    
    if (0==strncmp("attr", argv[3], 4))
    {
      accessType=api_attr;
    }
    else if (0==strncmp("record", argv[3], 6))
    {
      accessType=api_record;
    }
    else
    {    
      std::cout << "Bad access type argument : "<< argv[3] << "\n";
      exit(-1);
    }

    // run the application code
    run_application(mysql, cluster_connection, accessType);
  }

  ndb_end(0);

  return 0;
}

static void init_ndbrecord_info(Ndb &);
static void create_table(MYSQL &);
static void do_insert(Ndb &, ApiType);
static void do_update(Ndb &, ApiType);
static void do_delete(Ndb &, ApiType);
static void do_read(Ndb &, ApiType);
static void do_mixed_read(Ndb &);
static void do_mixed_update(Ndb &);
static void do_scan(Ndb &, ApiType);
static void do_mixed_scan(Ndb &);
static void do_indexScan(Ndb &, ApiType);
static void do_mixed_indexScan(Ndb&);
static void do_read_and_delete(Ndb &);
static void do_scan_update(Ndb&, ApiType);
static void do_scan_delete(Ndb&, ApiType);
static void do_scan_lock_reread(Ndb&, ApiType);
static void do_all_extras_read(Ndb &myNdb);
static void do_secondary_indexScan(Ndb &myNdb, ApiType accessType);
static void do_secondary_indexScanEqual(Ndb &myNdb, ApiType accessType);
static void do_interpreted_update(Ndb &myNdb, ApiType accessType);
static void do_interpreted_scan(Ndb &myNdb, ApiType accessType);
static void do_read_using_default(Ndb &myNdb);

/* This structure is used describe how we want data read using
 * NDBRecord to be placed into memory.  This can make it easier
 * to work with data, but is not essential.
 */
struct RowData
{
  int attr1;
  int attr2;
  int attr3;
};


/* Handy struct for representing the data in the
 * secondary index
 */
struct IndexRow
{
  unsigned int attr3;
  unsigned int attr2;
};

static void run_application(MYSQL &mysql,
			    Ndb_cluster_connection &cluster_connection,
                            ApiType accessType)
{
  /********************************************
   * Connect to database via mysql-c          *
   ********************************************/
  mysql_query(&mysql, "CREATE DATABASE ndb_examples");
  if (mysql_query(&mysql, "USE ndb_examples") != 0) MYSQLERROR(mysql);
  create_table(mysql);

  /********************************************
   * Connect to database via NdbApi           *
   ********************************************/
  // Object representing the database
  Ndb myNdb( &cluster_connection, "ndb_examples" );
  if (myNdb.init()) APIERROR(myNdb.getNdbError());

  init_ndbrecord_info(myNdb);
  /*
   * Do different operations on database
   */
  do_insert(myNdb, accessType);
  do_update(myNdb, accessType);
  do_delete(myNdb, accessType);
  do_read(myNdb, accessType);
  do_mixed_read(myNdb);
  do_mixed_update(myNdb);
  do_read(myNdb, accessType);
  do_scan(myNdb, accessType);
  do_mixed_scan(myNdb);
  do_indexScan(myNdb, accessType);
  do_mixed_indexScan(myNdb);
  do_read_and_delete(myNdb);
  do_scan_update(myNdb, accessType);
  do_scan_delete(myNdb, accessType);
  do_scan_lock_reread(myNdb, accessType);
  do_all_extras_read(myNdb);
  do_secondary_indexScan(myNdb, accessType);
  do_secondary_indexScanEqual(myNdb, accessType);
  do_scan(myNdb, accessType);
  do_interpreted_update(myNdb, accessType);
  do_interpreted_scan(myNdb, accessType);
  do_read_using_default(myNdb);
  do_scan(myNdb, accessType);
}

/*********************************************************
 * Create a table named api_recattr_vs_record if it does not exist *
 *********************************************************/
static void create_table(MYSQL &mysql)
{
  if (mysql_query(&mysql, 
		  "DROP TABLE IF EXISTS"
		  "  api_recattr_vs_record"))
    MYSQLERROR(mysql);

  if (mysql_query(&mysql, 
		  "CREATE TABLE"
		  "  api_recattr_vs_record"
		  "    (ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,"
		  "     ATTR2 INT UNSIGNED NOT NULL,"
                  "     ATTR3 INT UNSIGNED NOT NULL)"
		  "  ENGINE=NDB"))
    MYSQLERROR(mysql);

  /* Add ordered secondary index on 2 attributes, in reverse order */
  if (mysql_query(&mysql,
                  "CREATE INDEX"
                  "  MYINDEXNAME"
                  "  ON api_recattr_vs_record"
                  "  (ATTR3, ATTR2)"))
    MYSQLERROR(mysql);
}


/* Clunky statics for shared NdbRecord stuff */
static const NdbDictionary::Column *pattr1Col;
static const NdbDictionary::Column *pattr2Col;
static const NdbDictionary::Column *pattr3Col;

static const NdbRecord *pkeyColumnRecord;
static const NdbRecord *pallColsRecord;
static const NdbRecord *pkeyIndexRecord;
static const NdbRecord *psecondaryIndexRecord;

static int attr1ColNum;
static int attr2ColNum;
static int attr3ColNum;

/**************************************************************
 * Initialise NdbRecord structures for table and index access *
 **************************************************************/
static void init_ndbrecord_info(Ndb &myNdb)
{
  /* Here we create various NdbRecord structures for accessing
   * data using the tables and indexes on api_recattr_vs_record
   * We could use the default NdbRecord structures, but then
   * we wouldn't have the nice ability to read and write rows
   * to and from the RowData and IndexRow structs
   */
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");
  
  NdbDictionary::RecordSpecification recordSpec[3];

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  pattr1Col = myTable->getColumn("ATTR1");
  if (pattr1Col == NULL) APIERROR(myDict->getNdbError());
  pattr2Col = myTable->getColumn("ATTR2");
  if (pattr2Col == NULL) APIERROR(myDict->getNdbError());
  pattr3Col = myTable->getColumn("ATTR3");
  if (pattr3Col == NULL) APIERROR(myDict->getNdbError());
  
  attr1ColNum = pattr1Col->getColumnNo();
  attr2ColNum = pattr2Col->getColumnNo();
  attr3ColNum = pattr3Col->getColumnNo();

  // ATTR 1
  recordSpec[0].column = pattr1Col;
  recordSpec[0].offset = offsetof(RowData, attr1);
  recordSpec[0].nullbit_byte_offset = 0; // Not nullable 
  recordSpec[0].nullbit_bit_in_byte = 0;  
        
  // ATTR 2
  recordSpec[1].column = pattr2Col;
  recordSpec[1].offset = offsetof(RowData, attr2);
  recordSpec[1].nullbit_byte_offset = 0;   // Not nullable
  recordSpec[1].nullbit_bit_in_byte = 0;   

  // ATTR 3
  recordSpec[2].column = pattr3Col;
  recordSpec[2].offset = offsetof(RowData, attr3);
  recordSpec[2].nullbit_byte_offset = 0;   // Not nullable
  recordSpec[2].nullbit_bit_in_byte = 0;

  /* Create table record with just the primary key column */
  pkeyColumnRecord = 
    myDict->createRecord(myTable, recordSpec, 1, sizeof(recordSpec[0]));

  if (pkeyColumnRecord == NULL) APIERROR(myDict->getNdbError());

  /* Create table record with all the columns */
  pallColsRecord = 
    myDict->createRecord(myTable, recordSpec, 3, sizeof(recordSpec[0]));
        
  if (pallColsRecord == NULL) APIERROR(myDict->getNdbError());

  /* Create NdbRecord for primary index access */
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "api_recattr_vs_record");

  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  pkeyIndexRecord = 
    myDict->createRecord(myPIndex, recordSpec, 1, sizeof(recordSpec[0]));

  if (pkeyIndexRecord == NULL) APIERROR(myDict->getNdbError());

  /* Create Index NdbRecord for secondary index access
   * Note that we use the columns from the table to define the index
   * access record
   */
  const NdbDictionary::Index *mySIndex= myDict->getIndex("MYINDEXNAME", "api_recattr_vs_record");

  recordSpec[0].column= pattr3Col;
  recordSpec[0].offset= offsetof(IndexRow, attr3);
  recordSpec[0].nullbit_byte_offset=0;
  recordSpec[0].nullbit_bit_in_byte=0;

  recordSpec[1].column= pattr2Col;
  recordSpec[1].offset= offsetof(IndexRow, attr2);
  recordSpec[1].nullbit_byte_offset=0;
  recordSpec[1].nullbit_bit_in_byte=1;
  
  /* Create NdbRecord for accessing via secondary index */
  psecondaryIndexRecord = 
    myDict->createRecord(mySIndex, 
                         recordSpec, 
                         2, 
                         sizeof(recordSpec[0]));


  if (psecondaryIndexRecord == NULL) 
    APIERROR(myDict->getNdbError());

}


/**************************************************************************
 * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
 **************************************************************************/
static void do_insert(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  std::cout << "Running do_insert\n";

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  for (int i = 0; i < 5; i++) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());
    
    switch (accessType)
    {
    case api_attr :
      {
        NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
        if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

        myOperation->insertTuple();
        myOperation->equal("ATTR1", i);
        myOperation->setValue("ATTR2", i);
        myOperation->setValue("ATTR3", i);

        myOperation= myTransaction->getNdbOperation(myTable);
    
        if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
        myOperation->insertTuple();
        myOperation->equal("ATTR1", i+5);
        myOperation->setValue("ATTR2", i+5);
        myOperation->setValue("ATTR3", i+5);
        break;
      }
    case api_record :
      {
        RowData row;
        
        row.attr1= row.attr2= row.attr3= i;

        const NdbOperation *pop1=
          myTransaction->insertTuple(pallColsRecord, (char *) &row);
        if (pop1 == NULL) APIERROR(myTransaction->getNdbError());

        row.attr1= row.attr2= row.attr3= i+5;

        const NdbOperation *pop2=
          myTransaction->insertTuple(pallColsRecord, (char *) &row);
        if (pop2 == NULL) APIERROR(myTransaction->getNdbError());

        break;
      }
    default :
      {
        std::cout << "Bad branch : " << accessType << "\n";
        exit(-1);
      }
    }
    
    if (myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    myNdb.closeTransaction(myTransaction);
  }

  std::cout << "-------\n";
}
 
/*****************************************************************
 * Update the second attribute in half of the tuples (adding 10) *
 *****************************************************************/
static void do_update(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  std::cout << "Running do_update\n";

  for (int i = 0; i < 10; i+=2) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());
    
    switch (accessType)
    {
      case api_attr :
      {
        NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
        if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
        myOperation->updateTuple();
        myOperation->equal( "ATTR1", i );
        myOperation->setValue( "ATTR2", i+10);
        myOperation->setValue( "ATTR3", i+20);
        break;
      }
      case api_record :
      {
        RowData row;
        row.attr1=i;
        row.attr2=i+10;
        row.attr3=i+20;
        
        /* Since we're using an NdbRecord with all columns in it to
         * specify the updated columns, we need to create a mask to 
         * indicate that we are only updating attr2 and attr3.
         */
        unsigned char attrMask=(1<<attr2ColNum) | (1<<attr3ColNum);

        const NdbOperation *pop = 
          myTransaction->updateTuple(pkeyColumnRecord, (char*) &row, 
                                     pallColsRecord, (char*) &row,
                                     &attrMask);
        
        if (pop==NULL) APIERROR(myTransaction->getNdbError());
        break;
      }
    default :
      {
        std::cout << "Bad branch : " << accessType << "\n";
        exit(-1);
      }
    } 

    if( myTransaction->execute( NdbTransaction::Commit ) == -1 ) 
      APIERROR(myTransaction->getNdbError());
    
    myNdb.closeTransaction(myTransaction);
  }

  std::cout << "-------\n";
};
  
/*************************************************
 * Delete one tuple (the one with primary key 3) *
 *************************************************/
static void do_delete(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  std::cout << "Running do_delete\n";

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  NdbTransaction *myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  switch (accessType)
  {
  case api_attr :
    {
      NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
      if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
  
      myOperation->deleteTuple();
      myOperation->equal( "ATTR1", 3 );
      break;
    }
  case api_record :
    {
      RowData keyInfo;
      keyInfo.attr1=3;
      
      const NdbOperation *pop=
        myTransaction->deleteTuple(pkeyColumnRecord, 
                                   (char*) &keyInfo,
                                   pallColsRecord);

      if (pop==NULL) APIERROR(myTransaction->getNdbError());
      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  
  if (myTransaction->execute(NdbTransaction::Commit) == -1) 
    APIERROR(myTransaction->getNdbError());
  
  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}


/*****************************************************************
 * Update the second attribute in half of the tuples (adding 10) *
 *****************************************************************/
static void do_mixed_update(Ndb &myNdb)
{
  /* This method performs an update using a mix of NdbRecord
   * supplied attributes, and extra setvalues provided by 
   * the OperationOptions structure.
   */
  std::cout << "Running do_mixed_update (NdbRecord only)\n";

  for (int i = 0; i < 10; i+=2) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());
    
    RowData row;
    row.attr1=i;
    row.attr2=i+30;

    /* Only attr2 is updated via NdbRecord */
    unsigned char attrMask= (1<<attr2ColNum);

    NdbOperation::SetValueSpec setvalspecs[1];
    
    /* Value to set attr3 to */
    Uint32 dataSource= i + 40;

    setvalspecs[0].column = pattr3Col;
    setvalspecs[0].value = &dataSource;
    
    NdbOperation::OperationOptions opts;
    opts.optionsPresent= NdbOperation::OperationOptions::OO_SETVALUE;
    opts.extraSetValues= &setvalspecs[0];
    opts.numExtraSetValues= 1;
    

    // Define mixed operation in one call to NDBAPI
    const NdbOperation *pop = 
      myTransaction->updateTuple(pkeyColumnRecord, (char*) &row, 
                                 pallColsRecord, (char*) &row,
                                 &attrMask,
                                 &opts);
        
    if (pop==NULL) APIERROR(myTransaction->getNdbError());

    if( myTransaction->execute( NdbTransaction::Commit ) == -1 ) 
      APIERROR(myTransaction->getNdbError());
    
    myNdb.closeTransaction(myTransaction);
  }

  std::cout << "-------\n";
}
  

/*********************************************
 * Read and print all tuples using PK access *
 *********************************************/
static void do_read(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  std::cout << "Running do_read\n";

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    RowData rowData;
    NdbRecAttr *myRecAttr;
    NdbRecAttr *myRecAttr2;
    
    switch (accessType)
    {
      case api_attr :
      {
        NdbOperation *myOperation= myTransaction->getNdbOperation(myTable);
        if (myOperation == NULL) APIERROR(myTransaction->getNdbError());
    
        myOperation->readTuple(NdbOperation::LM_Read);
        myOperation->equal("ATTR1", i);

        myRecAttr= myOperation->getValue("ATTR2", NULL);
        if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());
       
        myRecAttr2=myOperation->getValue("ATTR3", NULL);
        if (myRecAttr2 == NULL) APIERROR(myTransaction->getNdbError());
        
        break;
      }
      case api_record :
      {
        rowData.attr1=i;
        const NdbOperation *pop=
          myTransaction->readTuple(pkeyColumnRecord, 
                                   (char*) &rowData,
                                   pallColsRecord,  // Read PK+ATTR2+ATTR3
                                   (char*) &rowData);
        if (pop==NULL) APIERROR(myTransaction->getNdbError());
        
        break;
      }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
    }

    if(myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    if (myTransaction->getNdbError().classification == NdbError::NoDataFound)
    {
      if (i == 3)
        std::cout << "Detected that deleted tuple doesn't exist!" << std::endl;
      else
	APIERROR(myTransaction->getNdbError());
    }
    
    switch (accessType)
    {
      case api_attr :
      {
        if (i != 3) {
          printf(" %2d    %2d    %2d\n", 
                 i, 
                 myRecAttr->u_32_value(),
                 myRecAttr2->u_32_value());
        }
        break;
      }
      case api_record :
      {
        if (i !=3) {
          printf(" %2d    %2d    %2d\n", 
                 i, 
                 rowData.attr2,
                 rowData.attr3);
        }
        break;
      }
      default :
      {
        std::cout << "Bad branch : " << accessType << "\n";
        exit(-1);
      }
    }

    myNdb.closeTransaction(myTransaction);
  }
  
  std::cout << "-------\n";
}

/*****************************
 * Read and print all tuples *
 *****************************/
static void do_mixed_read(Ndb &myNdb)
{
  std::cout << "Running do_mixed_read (NdbRecord only)\n";

  std::cout << "ATTR1 ATTR2 ATTR3 COMMIT_COUNT" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());
    
    RowData rowData;
    NdbRecAttr *myRecAttr3, *myRecAttrCC;

    /* Start with NdbRecord read of ATTR2, and then add 
     * getValue NdbRecAttr read of ATTR3 and Commit count 
     */
    NdbOperation::GetValueSpec extraCols[2];

    extraCols[0].column=pattr3Col;
    extraCols[0].appStorage=NULL;
    extraCols[0].recAttr=NULL;
    
    extraCols[1].column=NdbDictionary::Column::COMMIT_COUNT;
    extraCols[1].appStorage=NULL;
    extraCols[1].recAttr=NULL;

    NdbOperation::OperationOptions opts;
    opts.optionsPresent = NdbOperation::OperationOptions::OO_GETVALUE;

    opts.extraGetValues= &extraCols[0];
    opts.numExtraGetValues= 2;

    /* We only read attr2 using the normal NdbRecord access */
    unsigned char attrMask= (1<<attr2ColNum);

    // Set PK search criteria
    rowData.attr1= i;

    const NdbOperation *pop=
      myTransaction->readTuple(pkeyColumnRecord, 
                               (char*) &rowData,
                               pallColsRecord,  // Read all with mask
                               (char*) &rowData,
                               NdbOperation::LM_Read,
                               &attrMask, // result_mask
                               &opts);
    if (pop==NULL) APIERROR(myTransaction->getNdbError());

    myRecAttr3= extraCols[0].recAttr;
    myRecAttrCC= extraCols[1].recAttr;

    if (myRecAttr3 == NULL) APIERROR(myTransaction->getNdbError());
    if (myRecAttrCC == NULL) APIERROR(myTransaction->getNdbError());


    if(myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    if (myTransaction->getNdbError().classification == NdbError::NoDataFound)
    {
      if (i == 3)
        std::cout << "Detected that deleted tuple doesn't exist!" << std::endl;
      else
	APIERROR(myTransaction->getNdbError());
    }

    if (i !=3) {
      printf(" %2d    %2d    %2d    %d\n", 
             rowData.attr1,
             rowData.attr2,
             myRecAttr3->u_32_value(), 
             myRecAttrCC->u_32_value()
             );
    }
  
    myNdb.closeTransaction(myTransaction);
  }

  std::cout << "-------\n";
}

/********************************************
 * Read and print all tuples via table scan *
 ********************************************/
static void do_scan(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");
  
  std::cout << "Running do_scan\n";
  
  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());
  
  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbScanOperation(myTable);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      if (psop->readTuples(NdbOperation::LM_Read) != 0) APIERROR (myTransaction->getNdbError());
      
      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      /* Note that no row ptr is passed to the NdbRecord scan operation
       * The scan will fetch a batch and give the user a series of pointers
       * to rows in the batch in nextResult() below
       */
      psop=myTransaction->scanTable(pallColsRecord, 
                                    NdbOperation::LM_Read);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());
      
      break;
    }
  default :
  {
    std::cout << "Bad branch : " << accessType << "\n";
    exit(-1);
  }
  }
  
  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
  
  switch (accessType)
  {
  case api_attr :
  {
    while (psop->nextResult(true) == 0)
    {
      printf(" %2d    %2d    %2d\n", 
             recAttrAttr1->u_32_value(),
             recAttrAttr2->u_32_value(),
             recAttrAttr3->u_32_value());
    }
    
    psop->close();
    
    break;
  }
  case api_record :
  {
    RowData *prowData; // Ptr to point to our data
    
    int rc=0;
    
    /* Ask nextResult to update out ptr to point to the next 
     * row from the scan
     */
    while ((rc = psop->nextResult((const char**) &prowData,
                                  true,
                                  false)) == 0)
    {
      printf(" %2d    %2d    %2d\n", 
             prowData->attr1,
             prowData->attr2,
             prowData->attr3);
    }
    
    if (rc != 1)  APIERROR(myTransaction->getNdbError());
    
    psop->close(true);
    
    break;
  }
  default :
  {
    std::cout << "Bad branch : " << accessType << "\n";
    exit(-1);
  }
  }
  
  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());
  
  myNdb.closeTransaction(myTransaction);
  
  std::cout << "-------\n";
}

/***********************************************************
 * Read and print all tuples via table scan and mixed read *
 ***********************************************************/
static void do_mixed_scan(Ndb &myNdb)
{
  std::cout << "Running do_mixed_scan(NdbRecord only)\n";

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr3;

  /* Set mask so that NdbRecord scan reads attr1 and attr2 only */
  unsigned char attrMask=((1<<attr1ColNum) | (1<<attr2ColNum));

  /* Define extra get value to get attr3 */
  NdbOperation::GetValueSpec extraGets[1];
  extraGets[0].column = pattr3Col;
  extraGets[0].appStorage= 0;
  extraGets[0].recAttr= 0;

  NdbScanOperation::ScanOptions options;
  options.optionsPresent= NdbScanOperation::ScanOptions::SO_GETVALUE;
  options.extraGetValues= &extraGets[0];
  options.numExtraGetValues= 1;

  psop=myTransaction->scanTable(pallColsRecord, 
                                NdbOperation::LM_Read,
                                &attrMask,
                                &options,
                                sizeof(NdbScanOperation::ScanOptions));
  if (psop == NULL) APIERROR(myTransaction->getNdbError());

  /* RecAttr for the extra get has been set by the operation definition */
  recAttrAttr3 = extraGets[0].recAttr;

  if (recAttrAttr3 == NULL) APIERROR(myTransaction->getNdbError());
      
  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
   
  RowData *prowData; // Ptr to point to our data

  int rc=0;

  while ((rc = psop->nextResult((const char**) &prowData,
                                true,
                                false)) == 0)
  {
    printf(" %2d    %2d    %2d\n", 
           prowData->attr1,
           prowData->attr2,
           recAttrAttr3->u_32_value());
  }

  if (rc != 1)  APIERROR(myTransaction->getNdbError());

  psop->close(true);

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}



/************************************************************
 * Read and print all tuples via primary ordered index scan *
 ************************************************************/
static void do_indexScan(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "api_recattr_vs_record");

  std::cout << "Running do_indexScan\n";

  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbIndexScanOperation *psop;
  
  /* RecAttrs for NdbRecAttr Api */
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbIndexScanOperation(myPIndex);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Multi read range is not supported for the NdbRecAttr scan
       * API, so we just read one range.
       */
      Uint32 scanFlags= 
        NdbScanOperation::SF_OrderBy |
        NdbScanOperation::SF_MultiRange |
        NdbScanOperation::SF_ReadRangeNo;

      if (psop->readTuples(NdbOperation::LM_Read, 
                           scanFlags, 
                           (Uint32) 0,          // batch 
                           (Uint32) 0) != 0)    // parallel
        APIERROR (myTransaction->getNdbError());

      /* Add a bound
       * Tuples where ATTR1 >=2 and < 4 
       * 2,[3 deleted] 
       */
      Uint32 low=2;
      Uint32 high=4;

      if (psop->setBound("ATTR1", NdbIndexScanOperation::BoundLE, (char*)&low))
        APIERROR(myTransaction->getNdbError());
      if (psop->setBound("ATTR1", NdbIndexScanOperation::BoundGT, (char*)&high))
        APIERROR(myTransaction->getNdbError());
      
      if (psop->end_of_bound(0))
        APIERROR(psop->getNdbError());

      /* Second bound
       * Tuples where ATTR1 > 5 and <=9
       * 6,7,8,9 
       */
      low=5;
      high=9;
      if (psop->setBound("ATTR1", NdbIndexScanOperation::BoundLT, (char*)&low))
        APIERROR(myTransaction->getNdbError());
      if (psop->setBound("ATTR1", NdbIndexScanOperation::BoundGE, (char*)&high))
        APIERROR(myTransaction->getNdbError());

      if (psop->end_of_bound(1))
        APIERROR(psop->getNdbError());

      /* Read all columns */
      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      /* NdbRecord supports scanning multiple ranges using a 
       * single index scan operation
       */
      Uint32 scanFlags = 
        NdbScanOperation::SF_OrderBy |
        NdbScanOperation::SF_MultiRange |
        NdbScanOperation::SF_ReadRangeNo;

      NdbScanOperation::ScanOptions options;
      options.optionsPresent=NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      options.scan_flags=scanFlags;

      psop=myTransaction->scanIndex(pkeyIndexRecord,
                                    pallColsRecord,
                                    NdbOperation::LM_Read,
                                    NULL, // no mask - read all columns in result record
                                    NULL, // bound defined later
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Add a bound
       * Tuples where ATTR1 >=2 and < 4 
       * 2,[3 deleted] 
       */
      Uint32 low=2;
      Uint32 high=4;

      NdbIndexScanOperation::IndexBound bound;
      bound.low_key=(char*)&low;
      bound.low_key_count=1;
      bound.low_inclusive=true;
      bound.high_key=(char*)&high;
      bound.high_key_count=1;
      bound.high_inclusive=false;
      bound.range_no=0;

      if (psop->setBound(pkeyIndexRecord, bound))
        APIERROR(myTransaction->getNdbError());

      /* Second bound
       * Tuples where ATTR1 > 5 and <=9
       * 6,7,8,9 
       */
      low=5;
      high=9;

      bound.low_key=(char*)&low;
      bound.low_key_count=1;
      bound.low_inclusive=false;
      bound.high_key=(char*)&high;
      bound.high_key_count=1;
      bound.high_inclusive=true;
      bound.range_no=1;

      if (psop->setBound(pkeyIndexRecord, bound))
        APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
  
  if (myTransaction->getNdbError().code != 0)
    APIERROR(myTransaction->getNdbError());

  switch (accessType)
  {
    case api_attr :
    {
      while (psop->nextResult(true) == 0)
      {
        printf(" %2d    %2d    %2d    Range no : %2d\n", 
               recAttrAttr1->u_32_value(),
               recAttrAttr2->u_32_value(),
               recAttrAttr3->u_32_value(),
               psop->get_range_no());
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int rc=0;

      while ((rc = psop->nextResult((const char**) &prowData,
                                    true,
                                    false)) == 0)
      {
        // printf(" PTR : %d\n", (int) prowData);
        printf(" %2d    %2d    %2d    Range no : %2d\n", 
               prowData->attr1,
               prowData->attr2,
               prowData->attr3,
               psop->get_range_no());
      }

      if (rc != 1)  APIERROR(myTransaction->getNdbError());

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}



/*************************************************************************
 * Read and print all tuples via index scan using mixed NdbRecord access *
 *************************************************************************/
static void do_mixed_indexScan(Ndb &myNdb)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "api_recattr_vs_record");

  std::cout << "Running do_mixed_indexScan\n";

  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbIndexScanOperation *psop;
  NdbRecAttr *recAttrAttr3;

  Uint32 scanFlags = 
    NdbScanOperation::SF_OrderBy |
    NdbScanOperation::SF_MultiRange |
    NdbScanOperation::SF_ReadRangeNo;

  /* We'll get Attr3 via ScanOptions */
  unsigned char attrMask=((1<<attr1ColNum) | (1<<attr2ColNum));
  
  NdbOperation::GetValueSpec extraGets[1];
  extraGets[0].column= pattr3Col;
  extraGets[0].appStorage= NULL;
  extraGets[0].recAttr= NULL;

  NdbScanOperation::ScanOptions options;
  options.optionsPresent=
    NdbScanOperation::ScanOptions::SO_SCANFLAGS |
    NdbScanOperation::ScanOptions::SO_GETVALUE;
  options.scan_flags= scanFlags;
  options.extraGetValues= &extraGets[0];
  options.numExtraGetValues= 1;

  psop=myTransaction->scanIndex(pkeyIndexRecord,
                                pallColsRecord,
                                NdbOperation::LM_Read,
                                &attrMask, // mask
                                NULL, // bound defined below
                                &options,
                                sizeof(NdbScanOperation::ScanOptions));

  if (psop == NULL) APIERROR(myTransaction->getNdbError());

  /* Grab RecAttr now */
  recAttrAttr3= extraGets[0].recAttr;

  /* Add a bound
   * ATTR1 >= 2, < 4
   * 2,[3 deleted] 
   */
  Uint32 low=2;
  Uint32 high=4;

  NdbIndexScanOperation::IndexBound bound;
  bound.low_key=(char*)&low;
  bound.low_key_count=1;
  bound.low_inclusive=true;
  bound.high_key=(char*)&high;
  bound.high_key_count=1;
  bound.high_inclusive=false;
  bound.range_no=0;
  
  if (psop->setBound(pkeyIndexRecord, bound))
    APIERROR(myTransaction->getNdbError());

  /* Second bound
   * ATTR1 > 5, <= 9
   * 6,7,8,9 
   */
  low=5;
  high=9;

  bound.low_key=(char*)&low;
  bound.low_key_count=1;
  bound.low_inclusive=false;
  bound.high_key=(char*)&high;
  bound.high_key_count=1;
  bound.high_inclusive=true;
  bound.range_no=1;

  if (psop->setBound(pkeyIndexRecord, bound))
    APIERROR(myTransaction->getNdbError());

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    

  RowData *prowData; // Ptr to point to our data

  int rc=0;

  while ((rc = psop->nextResult((const char**) &prowData,
                                true,
                                false)) == 0)
  {
    printf(" %2d    %2d    %2d    Range no : %2d\n", 
           prowData->attr1,
           prowData->attr2,
           recAttrAttr3->u_32_value(),
           psop->get_range_no());
  }

  if (rc != 1)  APIERROR(myTransaction->getNdbError());
  
  psop->close(true);
  
  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}


/********************************************************
 * Read + Delete one tuple (the one with primary key 8) *
 ********************************************************/
static void do_read_and_delete(Ndb &myNdb)
{
  /* This procedure performs a single operation, single round
   * trip read and then delete of a tuple, specified by 
   * primary key
   */
  std::cout << "Running do_read_and_delete (NdbRecord only)\n";

  NdbTransaction *myTransaction= myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  RowData row;
  row.attr1=8;
  row.attr2=0; // Don't care
  row.attr3=0; // Don't care
      
  /* We'll also read some extra columns while we're 
   * reading + deleting
   */
  NdbOperation::OperationOptions options;
  NdbOperation::GetValueSpec extraGets[2];
  extraGets[0].column = pattr3Col;
  extraGets[0].appStorage = NULL;
  extraGets[0].recAttr = NULL;
  extraGets[1].column = NdbDictionary::Column::COMMIT_COUNT;
  extraGets[1].appStorage = NULL;
  extraGets[1].recAttr = NULL;

  options.optionsPresent= NdbOperation::OperationOptions::OO_GETVALUE;
  options.extraGetValues= &extraGets[0];
  options.numExtraGetValues= 2;

  unsigned char attrMask = (1<<attr2ColNum); // Only read Col2 into row

  const NdbOperation *pop=
    myTransaction->deleteTuple(pkeyColumnRecord, // Spec of key used 
                               (char*) &row, // Key information
                               pallColsRecord, // Spec of columns to read
                               (char*) &row, // Row to read values into
                               &attrMask, // Columns to read as part of delete
                               &options,
                               sizeof(NdbOperation::OperationOptions));

  if (pop==NULL) APIERROR(myTransaction->getNdbError());
  
  if (myTransaction->execute(NdbTransaction::Commit) == -1) 
    APIERROR(myTransaction->getNdbError());
  
  std::cout << "ATTR1 ATTR2 ATTR3 COMMITS" << std::endl;
  printf(" %2d    %2d    %2d    %2d\n", 
         row.attr1,
         row.attr2,
         extraGets[0].recAttr->u_32_value(),
         extraGets[1].recAttr->u_32_value());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}

/* Some handy consts for scan control */
static const int GOT_ROW= 0;
static const int NO_MORE_ROWS= 1;
static const int NEED_TO_FETCH_ROWS= 2;

/*********************************************
 * Read and update all tuples via table scan *
 *********************************************/
static void do_scan_update(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  std::cout << "Running do_scan_update\n";
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbScanOperation(myTable);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* When we want to operate on the tuples returned from a
       * scan, we need to request the the tuple's keyinfo is
       * returned, with SF_KeyInfo
       */
      if (psop->readTuples(NdbOperation::LM_Read,
                           NdbScanOperation::SF_KeyInfo) != 0) 
        APIERROR (myTransaction->getNdbError());
      
      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      NdbScanOperation::ScanOptions options;
      options.optionsPresent= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      options.scan_flags= NdbScanOperation::SF_KeyInfo;

      psop=myTransaction->scanTable(pallColsRecord, 
                                    NdbOperation::LM_Read,
                                    NULL,  // mask - read all columns
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());

  switch (accessType)
  {
    case api_attr :
    {


      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed= 0;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch=true;
        while ((result = psop->nextResult(fetch)) == GOT_ROW)
        {
          fetch= false;
          Uint32 col2Value=recAttrAttr2->u_32_value();

          NdbOperation *op=psop->updateCurrentTuple();
          if (op==NULL)
            APIERROR(myTransaction->getNdbError());
          op->setValue("ATTR2", (10*col2Value));

          processed++;
        }
        if (result < 0)
          APIERROR(myTransaction->getNdbError());

        if (processed !=0)
        {
          // Need to execute
          
          if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;
        }
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed=0;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch= true;
        while ((result = psop->nextResult((const char**) &prowData, 
                                          fetch, false)) == GOT_ROW)
        {
          fetch= false;

          /* Copy row into a stack variable */
          RowData r= *prowData;
          
          /* Modify attr2 */
          r.attr2*= 10;

          /* Update it */
          const NdbOperation *op = psop->updateCurrentTuple(myTransaction,
                                                            pallColsRecord,
                                                            (char*) &r);

          if (op==NULL)
            APIERROR(myTransaction->getNdbError());

          processed ++;
        }

        if (result < 0)
          APIERROR(myTransaction->getNdbError());


        if (processed !=0)
        {
          /* To get here, there are no more cached scan results,
           * and some row updates that we've not sent yet.
           * Send them before we try to get another batch, or 
           * finish.
           */
          if (myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;
        }
      }

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}

/**************************************************
 * Read all and delete some tuples via table scan *
 **************************************************/
static void do_scan_delete(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  std::cout << "Running do_scan_delete\n";
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr1;

  /* Scan, retrieving first column.
   * Delete particular records, based on first column
   * Read third column as part of delete
   */
  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbScanOperation(myTable);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Need KeyInfo when performing scanning delete */
      if (psop->readTuples(NdbOperation::LM_Read,
                           NdbScanOperation::SF_KeyInfo) != 0) 
        APIERROR (myTransaction->getNdbError());
      
      recAttrAttr1=psop->getValue("ATTR1");

      break;
    }
    case api_record :
    {
      

      NdbScanOperation::ScanOptions options;
      options.optionsPresent=NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      /* Need KeyInfo when performing scanning delete */
      options.scan_flags=NdbScanOperation::SF_KeyInfo;

      psop=myTransaction->scanTable(pkeyColumnRecord, 
                                    NdbOperation::LM_Read,
                                    NULL,  // mask
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  switch (accessType)
  {
    case api_attr :
    {
      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed=0;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch=true;
        while ((result = psop->nextResult(fetch)) == GOT_ROW)
        {
          fetch= false;
          Uint32 col1Value=recAttrAttr1->u_32_value();
          
          if (col1Value == 2)
          {
            /* Note : We cannot do a delete pre-read via
             * the NdbRecAttr interface.  We can only
             * delete here.
             */
            if (psop->deleteCurrentTuple())
              APIERROR(myTransaction->getNdbError());
            processed++;
          }
        }
        if (result < 0)
          APIERROR(myTransaction->getNdbError());

        if (processed !=0)
        {
          /* To get here, there are no more cached scan results,
           * and some row deletes that we've not sent yet.
           * Send them before we try to get another batch, or 
           * finish.
           */
          if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;
        }
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed=0;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch=true;
        
        const NdbOperation* theDeleteOp;
        RowData readRow;
        NdbRecAttr* attr3;
        NdbRecAttr* commitCount;

        while ((result = psop->nextResult((const char**) &prowData, 
                                          fetch,
                                          false)) == GOT_ROW)
        {
          fetch = false;

          /* Copy latest row to a stack local */
          RowData r;
          r= *prowData;
          
          if (r.attr1 == 2)
          {
            /* We're going to perform a read+delete on this
             * row.  We'll read attr1 and attr2 via NdbRecord
             * and Attr3 and the commit count via extra
             * get values.
             */
            NdbOperation::OperationOptions options;
            NdbOperation::GetValueSpec extraGets[2];
            extraGets[0].column = pattr3Col;
            extraGets[0].appStorage = NULL;
            extraGets[0].recAttr = NULL;
            extraGets[1].column = NdbDictionary::Column::COMMIT_COUNT;
            extraGets[1].appStorage = NULL;
            extraGets[1].recAttr = NULL;

            options.optionsPresent= NdbOperation::OperationOptions::OO_GETVALUE;
            options.extraGetValues= &extraGets[0];
            options.numExtraGetValues= 2;

            // Read cols 1 + 2 via NdbRecord
            unsigned char attrMask = (1<<attr1ColNum) | (1<<attr2ColNum);

            theDeleteOp = psop->deleteCurrentTuple(myTransaction,
                                                   pallColsRecord,
                                                   (char*) &readRow,
                                                   &attrMask,
                                                   &options,
                                                   sizeof(NdbOperation::OperationOptions));

            if (theDeleteOp==NULL)
              APIERROR(myTransaction->getNdbError());

            /* Store extra Get RecAttrs */
            attr3= extraGets[0].recAttr;
            commitCount= extraGets[1].recAttr;

            processed ++;
          }
        }

        if (result < 0)
          APIERROR(myTransaction->getNdbError());


        if (processed !=0)
        {
          /* To get here, there are no more cached scan results,
           * and some row deletes that we've not sent yet.
           * Send them before we try to get another batch, or 
           * finish.
           */
          if (myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;

          // Let's look at the data just read
          printf("Deleted data\n");
          printf("ATTR1  ATTR2  ATTR3 COMMITS\n");
          printf("  %2d    %2d    %2d    %2d\n", 
                 readRow.attr1, 
                 readRow.attr2, 
                 attr3->u_32_value(),
                 commitCount->u_32_value());
        }
      }

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}



/***********************************************************
 * Read all tuples via scan, reread one with lock takeover *
 ***********************************************************/
static void do_scan_lock_reread(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  std::cout << "Running do_scan_lock_reread\n";
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr1;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbScanOperation(myTable);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Need KeyInfo for lock takeover */
      if (psop->readTuples(NdbOperation::LM_Read,
                           NdbScanOperation::SF_KeyInfo) != 0) 
        APIERROR (myTransaction->getNdbError());
      
      recAttrAttr1=psop->getValue("ATTR1");

      break;
    }
    case api_record :
    {
      NdbScanOperation::ScanOptions options;
      options.optionsPresent= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      /* Need KeyInfo for lock takeover */
      options.scan_flags= NdbScanOperation::SF_KeyInfo;

      psop=myTransaction->scanTable(pkeyColumnRecord,
                                    NdbOperation::LM_Read,
                                    NULL,  // mask
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  switch (accessType)
  {
    case api_attr :
    {
      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed=0;
      NdbRecAttr *attr1, *attr2, *attr3, *commitCount;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch=true;
        while ((result = psop->nextResult(fetch)) == GOT_ROW)
        {
          fetch= false;
          Uint32 col1Value=recAttrAttr1->u_32_value();

          if (col1Value == 9)
          {
            /* Let's read the rest of the info for it with
             * a separate operation
             */
            NdbOperation *op= psop->lockCurrentTuple();

            if (op==NULL)
              APIERROR(myTransaction->getNdbError());
            attr1=op->getValue("ATTR1");
            attr2=op->getValue("ATTR2");
            attr3=op->getValue("ATTR3");
            commitCount=op->getValue(NdbDictionary::Column::COMMIT_COUNT);
            processed++;
          }
        }
        if (result < 0)
          APIERROR(myTransaction->getNdbError());

        if (processed !=0)
        {
          // Need to execute
          
          if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;

          // Let's look at the whole row...
          printf("Locked and re-read data:\n");
          printf("ATTR1  ATTR2  ATTR3 COMMITS\n");
          printf("  %2d    %2d    %2d    %2d\n", 
                 attr1->u_32_value(),
                 attr2->u_32_value(),
                 attr3->u_32_value(),
                 commitCount->u_32_value());
        }
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int result= NEED_TO_FETCH_ROWS;
      Uint32 processed=0;
      RowData rereadData;
      NdbRecAttr *attr3, *commitCount;

      while (result == NEED_TO_FETCH_ROWS)
      {
        bool fetch=true;
        while ((result = psop->nextResult((const char**) &prowData, 
                                          fetch,
                                          false)) == GOT_ROW)
        {
          fetch = false;

          /* Copy row to stack local */
          RowData r;
          r=*prowData;

          if (r.attr1 == 9)
          {
            /* Perform extra read of this row via lockCurrentTuple
             * Read all columns using NdbRecord for attr1 + attr2,
             * and extra get values for attr3 and the commit count
             */
            NdbOperation::OperationOptions options;
            NdbOperation::GetValueSpec extraGets[2];
            extraGets[0].column = pattr3Col;
            extraGets[0].appStorage = NULL;
            extraGets[0].recAttr = NULL;
            extraGets[1].column = NdbDictionary::Column::COMMIT_COUNT;
            extraGets[1].appStorage = NULL;
            extraGets[1].recAttr = NULL;

            options.optionsPresent=NdbOperation::OperationOptions::OO_GETVALUE;
            options.extraGetValues=&extraGets[0];
            options.numExtraGetValues=2;

            // Read cols 1 + 2 via NdbRecord
            unsigned char attrMask = (1<<attr1ColNum) | (1<<attr2ColNum);

            const NdbOperation *lockOp = psop->lockCurrentTuple(myTransaction,
                                                                pallColsRecord,
                                                                (char *) &rereadData,
                                                                &attrMask,
                                                                &options,
                                                                sizeof(NdbOperation::OperationOptions));
            if (lockOp == NULL)
              APIERROR(myTransaction->getNdbError());

            attr3= extraGets[0].recAttr;
            commitCount= extraGets[1].recAttr;

            processed++;
          }
        }

        if (result < 0)
          APIERROR(myTransaction->getNdbError());


        if (processed !=0)
        {
          // Need to execute

          if (myTransaction->execute( NdbTransaction::NoCommit ) != 0)
            APIERROR(myTransaction->getNdbError());
          processed=0;

          // Let's look at the whole row...
          printf("Locked and re-read data:\n");
          printf("ATTR1  ATTR2  ATTR3 COMMITS\n");
          printf("  %2d    %2d    %2d    %2d\n", 
                 rereadData.attr1,
                 rereadData.attr2,
                 attr3->u_32_value(),
                 commitCount->u_32_value());
          
        }
      }

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}

/***************************************************************
 * Read all tuples via primary key, using only extra getValues * 
 ***************************************************************/
static void do_all_extras_read(Ndb &myNdb)
{
  std::cout << "Running do_all_extras_read(NdbRecord only)\n";
  std::cout << "ATTR1 ATTR2 ATTR3 COMMIT_COUNT" << std::endl;
  
  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());
    
    RowData rowData;
    NdbRecAttr *myRecAttr1, *myRecAttr2, *myRecAttr3, *myRecAttrCC;

    /* We read nothing via NdbRecord, and everything via
     * 'extra' reads 
     */
    NdbOperation::GetValueSpec extraCols[4];

    extraCols[0].column=pattr1Col;
    extraCols[0].appStorage=NULL;
    extraCols[0].recAttr=NULL;

    extraCols[1].column=pattr2Col;
    extraCols[1].appStorage=NULL;
    extraCols[1].recAttr=NULL;

    extraCols[2].column=pattr3Col;
    extraCols[2].appStorage=NULL;
    extraCols[2].recAttr=NULL;
    
    extraCols[3].column=NdbDictionary::Column::COMMIT_COUNT;
    extraCols[3].appStorage=NULL;
    extraCols[3].recAttr=NULL;

    NdbOperation::OperationOptions opts;
    opts.optionsPresent = NdbOperation::OperationOptions::OO_GETVALUE;

    opts.extraGetValues=&extraCols[0];
    opts.numExtraGetValues=4;

    unsigned char attrMask= 0; // No row results required.

    // Set PK search criteria
    rowData.attr1= i;

    const NdbOperation *pop=
      myTransaction->readTuple(pkeyColumnRecord, 
                               (char*) &rowData,
                               pkeyColumnRecord,
                               NULL, // null result row
                               NdbOperation::LM_Read,
                               &attrMask,
                               &opts);
    if (pop==NULL) APIERROR(myTransaction->getNdbError());

    myRecAttr1=extraCols[0].recAttr;
    myRecAttr2=extraCols[1].recAttr;
    myRecAttr3=extraCols[2].recAttr;
    myRecAttrCC=extraCols[3].recAttr;

    if (myRecAttr1 == NULL) APIERROR(myTransaction->getNdbError());
    if (myRecAttr2 == NULL) APIERROR(myTransaction->getNdbError());
    if (myRecAttr3 == NULL) APIERROR(myTransaction->getNdbError());
    if (myRecAttrCC == NULL) APIERROR(myTransaction->getNdbError());

    if(myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    bool deleted= (myTransaction->getNdbError().classification == 
                   NdbError::NoDataFound);
    if (deleted)
      printf("Detected that deleted tuple %d doesn't exist!\n", i);
    else
    {
      printf(" %2d    %2d    %2d    %d\n", 
             myRecAttr1->u_32_value(),
             myRecAttr2->u_32_value(),
             myRecAttr3->u_32_value(), 
             myRecAttrCC->u_32_value()
             );
    }
  
    myNdb.closeTransaction(myTransaction);
  }

  std::cout << "-------\n";
}


/******************************************************************
 * Read and print some tuples via bounded scan of secondary index *
 ******************************************************************/
static void do_secondary_indexScan(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Index *mySIndex= myDict->getIndex("MYINDEXNAME", "api_recattr_vs_record");

  std::cout << "Running do_secondary_indexScan\n";
  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbIndexScanOperation *psop;
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  Uint32 scanFlags = 
    NdbScanOperation::SF_OrderBy |
    NdbScanOperation::SF_Descending |
    NdbScanOperation::SF_MultiRange |
    NdbScanOperation::SF_ReadRangeNo;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbIndexScanOperation(mySIndex);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      if (psop->readTuples(NdbOperation::LM_Read, 
                           scanFlags, 
                           (Uint32) 0,          // batch 
                           (Uint32) 0) != 0)    // parallel
        APIERROR (myTransaction->getNdbError());

      /* Bounds :
       * > ATTR3=6
       * < ATTR3=42
       */
      Uint32 low=6;
      Uint32 high=42;

      if (psop->setBound("ATTR3", NdbIndexScanOperation::BoundLT, (char*)&low))
        APIERROR(psop->getNdbError());
      if (psop->setBound("ATTR3", NdbIndexScanOperation::BoundGT, (char*)&high))
        APIERROR(psop->getNdbError());     

      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      
      NdbScanOperation::ScanOptions options;
      options.optionsPresent=NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      options.scan_flags=scanFlags;

      psop=myTransaction->scanIndex(psecondaryIndexRecord,
                                    pallColsRecord,
                                    NdbOperation::LM_Read,
                                    NULL, // mask
                                    NULL, // bound
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Bounds :
       * > ATTR3=6
       * < ATTR3=42
       */
      Uint32 low=6;
      Uint32 high=42;

      NdbIndexScanOperation::IndexBound bound;
      bound.low_key=(char*)&low;
      bound.low_key_count=1;
      bound.low_inclusive=false;
      bound.high_key=(char*)&high;
      bound.high_key_count=1;
      bound.high_inclusive=false;
      bound.range_no=0;

      if (psop->setBound(psecondaryIndexRecord, bound))
        APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  // Check rc anyway
  if (myTransaction->getNdbError().status != NdbError::Success)
    APIERROR(myTransaction->getNdbError());

  switch (accessType)
  {
    case api_attr :
    {
      while (psop->nextResult(true) == 0)
      {
        printf(" %2d    %2d    %2d    Range no : %2d\n", 
               recAttrAttr1->u_32_value(),
               recAttrAttr2->u_32_value(),
               recAttrAttr3->u_32_value(),
               psop->get_range_no());
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int rc=0;

      while ((rc = psop->nextResult((const char**) &prowData,
                                    true,
                                    false)) == 0)
      {
        // printf(" PTR : %d\n", (int) prowData);
        printf(" %2d    %2d    %2d    Range no : %2d\n", 
               prowData->attr1,
               prowData->attr2,
               prowData->attr3,
               psop->get_range_no());
      }

      if (rc != 1)  APIERROR(myTransaction->getNdbError());

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}


/***********************************************************************
 * Index scan to read tuples from secondary index using equality bound *
 ***********************************************************************/
static void do_secondary_indexScanEqual(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Index *mySIndex= myDict->getIndex("MYINDEXNAME", "api_recattr_vs_record");

  std::cout << "Running do_secondary_indexScanEqual\n";
  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbIndexScanOperation *psop;
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  Uint32 scanFlags = NdbScanOperation::SF_OrderBy; 

  Uint32 attr3Eq= 44;

  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbIndexScanOperation(mySIndex);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      if (psop->readTuples(NdbOperation::LM_Read, 
                           scanFlags, 
                           (Uint32) 0,          // batch 
                           (Uint32) 0) != 0)    // parallel
        APIERROR (myTransaction->getNdbError());

      if (psop->setBound("ATTR3", NdbIndexScanOperation::BoundEQ, (char*)&attr3Eq))
        APIERROR(myTransaction->getNdbError());     

      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      
      NdbScanOperation::ScanOptions options;
      options.optionsPresent= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      options.scan_flags=scanFlags;

      psop=myTransaction->scanIndex(psecondaryIndexRecord,
                                    pallColsRecord, // Read all table rows back
                                    NdbOperation::LM_Read,
                                    NULL, // mask
                                    NULL, // bound specified below
                                    &options,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      /* Set equality bound via two inclusive bounds */
      NdbIndexScanOperation::IndexBound bound;
      bound.low_key= (char*)&attr3Eq;
      bound.low_key_count= 1;
      bound.low_inclusive= true;
      bound.high_key= (char*)&attr3Eq;
      bound.high_key_count= 1;
      bound.high_inclusive= true;
      bound.range_no= 0;

      if (psop->setBound(psecondaryIndexRecord, bound))
        APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  // Check rc anyway
  if (myTransaction->getNdbError().status != NdbError::Success)
    APIERROR(myTransaction->getNdbError());

  switch (accessType)
  {
    case api_attr :
    {
      int res;

      while ((res= psop->nextResult(true)) == GOT_ROW)
      {
        printf(" %2d    %2d    %2d\n",
               recAttrAttr1->u_32_value(),
               recAttrAttr2->u_32_value(),
               recAttrAttr3->u_32_value());
      }

      if (res != NO_MORE_ROWS)
        APIERROR(psop->getNdbError());

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int rc=0;

      while ((rc = psop->nextResult((const char**) &prowData,
                                    true,   // fetch
                                    false)) // forceSend 
             == GOT_ROW)
      {
        printf(" %2d    %2d    %2d\n",
               prowData->attr1,
               prowData->attr2,
               prowData->attr3);
      }

      if (rc != NO_MORE_ROWS)  
        APIERROR(myTransaction->getNdbError());

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}


/**********************
 * Interpreted update *
 **********************/
static void do_interpreted_update(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");
  const NdbDictionary::Index *myPIndex= myDict->getIndex("PRIMARY", "api_recattr_vs_record");

  std::cout << "Running do_interpreted_update\n";

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());
  if (myPIndex == NULL)
    APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;
  NdbRecAttr *recAttrAttr11;
  NdbRecAttr *recAttrAttr12;
  NdbRecAttr *recAttrAttr13;
  RowData rowData;
  RowData rowData2;

  /* Register aliases */
  const Uint32 R1=1, R2=2, R3=3, R4=4, R5=5, R6=6;

  switch (accessType)
  {
    case api_attr :
    {
      NdbOperation *pop;
      pop=myTransaction->getNdbOperation(myTable);
      
      if (pop == NULL) APIERROR(myTransaction->getNdbError());

      if (pop->interpretedUpdateTuple())
        APIERROR (pop->getNdbError());

      /* Interpreted update on row where ATTR1 == 4 */
      if (pop->equal("ATTR1", 4) != 0)
        APIERROR (pop->getNdbError());

      /* First, read the values of all attributes in the normal way */
      recAttrAttr1=pop->getValue("ATTR1");
      recAttrAttr2=pop->getValue("ATTR2");
      recAttrAttr3=pop->getValue("ATTR3");

      /* Now define interpreted program which will run after the
       * values have been read
       * This program is rather tortuous and doesn't achieve much other
       * than demonstrating control flow, register and some column 
       * operations
       */
      // R5= 3
      if (pop->load_const_u32(R5, 3) != 0)
        APIERROR (pop->getNdbError());
      
      // R1= *ATTR1; R2= *ATTR2; R3= *ATTR3
      if (pop->read_attr("ATTR1", R1) != 0)
        APIERROR (pop->getNdbError());
      if (pop->read_attr("ATTR2", R2) != 0)
        APIERROR (pop->getNdbError());
      if (pop->read_attr("ATTR3", R3) != 0)
        APIERROR (pop->getNdbError());
      
      // R3= R3-R5
      if (pop->sub_reg(R3, R5, R3) != 0)
        APIERROR (pop->getNdbError());
      
      // R2= R1+R2
      if (pop->add_reg(R1, R2, R2) != 0)
        APIERROR (pop->getNdbError());

      // *ATTR2= R2
      if (pop->write_attr("ATTR2", R2) != 0)
        APIERROR (pop->getNdbError());

      // *ATTR3= R3
      if (pop->write_attr("ATTR3", R3) != 0)
        APIERROR (pop->getNdbError());

      // *ATTR3 = *ATTR3 - 30
      if (pop->subValue("ATTR3", (Uint32)30) != 0)
        APIERROR (pop->getNdbError());

      Uint32 comparisonValue= 10;

      // if *ATTR3 > comparisonValue, goto Label 0
      if (pop->branch_col_lt(pattr3Col->getColumnNo(),
                             &comparisonValue,
                             sizeof(Uint32),
                             false,
                             0) != 0)
        APIERROR (pop->getNdbError());

      // assert(false)
      // Fail the operation with error 627 if we get here.
      if (pop->interpret_exit_nok(627) != 0)
        APIERROR (pop->getNdbError());

      // Label 0
      if (pop->def_label(0) != 0)
        APIERROR (pop->getNdbError());

      Uint32 comparisonValue2= 344;

      // if *ATTR2 == comparisonValue, goto Label 1
      if (pop->branch_col_eq(pattr2Col->getColumnNo(),
                             &comparisonValue2,
                             sizeof(Uint32),
                             false,
                             1) != 0)
        APIERROR (pop->getNdbError());
      
      // assert(false)
      // Fail the operation with error 628 if we get here
      if (pop->interpret_exit_nok(628) != 0)
        APIERROR (pop->getNdbError());

      // Label 1
      if (pop->def_label(1) != 1)
        APIERROR (pop->getNdbError());

      // Optional infinite loop
      //if (pop->branch_label(0) != 0)
      //  APIERROR (pop->getNdbError());

      // R1 = 10
      if (pop->load_const_u32(R1, 10) != 0)
        APIERROR (pop->getNdbError());

      // R3 = 2
      if (pop->load_const_u32(R3, 2) != 0)
        APIERROR (pop->getNdbError());

      // Now call subroutine 0
      if (pop->call_sub(0) != 0)
        APIERROR (pop->getNdbError());

      // *ATTR2= R2
      if (pop->write_attr("ATTR2", R2) != 0)
        APIERROR (pop->getNdbError());

      // Return ok, we'll move onto an update.
      if (pop->interpret_exit_ok() != 0)
        APIERROR (pop->getNdbError());

      /* Define a final read of the columns after the update */
      recAttrAttr11= pop->getValue("ATTR1");
      recAttrAttr12= pop->getValue("ATTR2");
      recAttrAttr13= pop->getValue("ATTR3");

      // Define any subroutines called by the 'main' program
      // Subroutine 0
      if (pop->def_subroutine(0) != 0)
        APIERROR (pop->getNdbError());

      // R4= 1
      if (pop->load_const_u32(R4, 1) != 0)
        APIERROR (pop->getNdbError());

      // Label 2
      if (pop->def_label(2) != 2)
        APIERROR (pop->getNdbError());

      // R3= R3-R4
      if (pop->sub_reg(R3, R4, R3) != 0)
        APIERROR (pop->getNdbError());

      // R2= R2 + R1
      if (pop->add_reg(R2, R1, R2) != 0)
        APIERROR (pop->getNdbError());

      // Optional infinite loop
      // if (pop->branch_label(2) != 0)
      //  APIERROR (pop->getNdbError());

      // Loop, subtracting 1 from R4 until R4 < 1
      if (pop->branch_ge(R4, R3, 2) != 0)
        APIERROR (pop->getNdbError());

      // Jump to label 3
      if (pop->branch_label(3) != 0)
        APIERROR (pop->getNdbError());

      // assert(false)
      // Fail operation with error 629
      if (pop->interpret_exit_nok(629) != 0)
        APIERROR (pop->getNdbError());

      // Label 3
      if (pop->def_label(3) != 3)
        APIERROR (pop->getNdbError());

      // Nested subroutine call to sub 2
      if (pop->call_sub(2) != 0)
        APIERROR (pop->getNdbError());

      // Return from subroutine 0
      if (pop->ret_sub() !=0)
        APIERROR (pop->getNdbError());

      // Subroutine 1
      if (pop->def_subroutine(1) != 1)
        APIERROR (pop->getNdbError());
      
      // R6= R1+R2
      if (pop->add_reg(R1, R2, R6) != 0)
        APIERROR (pop->getNdbError());

      // Return from subrouine 1
      if (pop->ret_sub() !=0)
        APIERROR (pop->getNdbError());

      // Subroutine 2 
      if (pop->def_subroutine(2) != 2)
        APIERROR (pop->getNdbError());
      
      // Call backwards to subroutine 1
      if (pop->call_sub(1) != 0)
        APIERROR (pop->getNdbError());

      // Return from subroutine 2
      if (pop->ret_sub() !=0)
        APIERROR (pop->getNdbError());

      break;
    }
    case api_record :
    {
      const NdbOperation *pop;
      rowData.attr1= 4;
      /* NdbRecord does not support an updateTuple pre-read or post-read, so 
       * we use separate operations for these.
       * Note that this assumes that a operations are executed in
       * the order they are defined by NDBAPI, which is not guaranteed.  To ensure
       * execution order, the application should perform a NoCommit execute between
       * operations.
       */
      const NdbOperation *op0= myTransaction->readTuple(pkeyColumnRecord,
                                                        (char*) &rowData,
                                                        pallColsRecord,
                                                        (char*) &rowData);
      if (op0 == NULL)
        APIERROR (myTransaction->getNdbError());
      
      /* Allocate some space to define an Interpreted program */
      const Uint32 numWords= 64;
      Uint32 space[numWords];

      NdbInterpretedCode stackCode(myTable,
                                   &space[0],
                                   numWords);
      
      NdbInterpretedCode *code= &stackCode;

      /* Similar program as above, with tortuous control flow and little
       * purpose.  Note that for NdbInterpretedCode, some instruction
       * arguments are in different orders
       */

      // R5= 3
      if (code->load_const_u32(R5, 3) != 0)
        APIERROR(code->getNdbError());
      
      // R1= *ATTR1; R2= *ATTR2; R3= *ATTR3
      if (code->read_attr(R1, pattr1Col) != 0)
        APIERROR (code->getNdbError());
      if (code->read_attr(R2, pattr2Col) != 0)
        APIERROR (code->getNdbError());
      if (code->read_attr(R3, pattr3Col) != 0)
        APIERROR (code->getNdbError());
      
      // R3= R3-R5
      if (code->sub_reg(R3, R3, R5) != 0)
        APIERROR (code->getNdbError());
      
      // R2= R1+R2
      if (code->add_reg(R2, R1, R2) != 0)
        APIERROR (code->getNdbError());
      
      // *ATTR2= R2
      if (code->write_attr(pattr2Col, R2) != 0)
        APIERROR (code->getNdbError());

      // *ATTR3= R3
      if (code->write_attr(pattr3Col, R3) != 0)
        APIERROR (code->getNdbError());

      // *ATTR3 = *ATTR3 - 30
      if (code->sub_val(pattr3Col->getColumnNo(), (Uint32)30) != 0)
        APIERROR (code->getNdbError());

      Uint32 comparisonValue= 10;

      // if comparisonValue < *ATTR3, goto Label 0
      if (code->branch_col_lt(&comparisonValue,
                              sizeof(Uint32),
                              pattr3Col->getColumnNo(),
                              0) != 0)
        APIERROR (code->getNdbError());

      // assert(false)
      // Fail operation with error 627
      if (code->interpret_exit_nok(627) != 0)
        APIERROR (code->getNdbError());

      // Label 0
      if (code->def_label(0) != 0)
        APIERROR (code->getNdbError());

      Uint32 comparisonValue2= 344;

      // if *ATTR2 == comparisonValue, goto Label 1
      if (code->branch_col_eq(&comparisonValue2,
                              sizeof(Uint32),
                              pattr2Col->getColumnNo(),
                              1) != 0)
        APIERROR (code->getNdbError());
      
      // assert(false)
      // Fail operation with error 628
      if (code->interpret_exit_nok(628) != 0)
        APIERROR (code->getNdbError());

      // Label 1
      if (code->def_label(1) != 0)
        APIERROR (code->getNdbError());

      // R1= 10
      if (code->load_const_u32(R1, 10) != 0)
        APIERROR (code->getNdbError());

      // R3= 2
      if (code->load_const_u32(R3, 2) != 0)
        APIERROR (code->getNdbError());

      // Call subroutine 0 to effect
      // R2 = R2 + (R1*R3)
      if (code->call_sub(0) != 0)
        APIERROR (code->getNdbError());
      
      // *ATTR2= R2
      if (code->write_attr(pattr2Col, R2) != 0)
        APIERROR (code->getNdbError());

      // Return ok
      if (code->interpret_exit_ok() != 0)
        APIERROR (code->getNdbError());

      // Subroutine 0
      if (code->def_sub(0) != 0)
        APIERROR (code->getNdbError());

      // R4= 1
      if (code->load_const_u32(R4, 1) != 0)
        APIERROR (code->getNdbError());

      // Label 2
      if (code->def_label(2) != 0)
        APIERROR (code->getNdbError());

      // R3= R3-R4
      if (code->sub_reg(R3, R3, R4) != 0)
        APIERROR (code->getNdbError());

      // R2= R2+R1
      if (code->add_reg(R2, R2, R1) != 0)
        APIERROR (code->getNdbError());

      // Loop, subtracting 1 from R4 until R4>1
      if (code->branch_ge(R3, R4, 2) != 0)
        APIERROR (code->getNdbError());

      // Jump to label 3
      if (code->branch_label(3) != 0)
        APIERROR (code->getNdbError());

      // Fail operation with error 629
      if (code->interpret_exit_nok(629) != 0)
        APIERROR (code->getNdbError());

      // Label 3
      if (code->def_label(3) != 0)
        APIERROR (code->getNdbError());

      // Call sub 2
      if (code->call_sub(2) != 0)
        APIERROR (code->getNdbError());

      // Return from sub 0
      if (code->ret_sub() != 0)
        APIERROR (code->getNdbError());

      // Subroutine 1
      if (code->def_sub(1) != 0)
        APIERROR (code->getNdbError());
      
      // R6= R1+R2
      if (code->add_reg(R6, R1, R2) != 0)
        APIERROR (code->getNdbError());
      
      // Return from subroutine 1
      if (code->ret_sub() !=0)
        APIERROR (code->getNdbError());

      // Subroutine 2
      if (code->def_sub(2) != 0)
        APIERROR (code->getNdbError());
      
      // Call backwards to subroutine 1
      if (code->call_sub(1) != 0)
        APIERROR (code->getNdbError());

      // Return from subroutine 2
      if (code->ret_sub() !=0)
        APIERROR (code->getNdbError());

      /* Finalise code object
       * This step is essential for NdbInterpretedCode objects 
       * and must be done before they can be used.
       */
      if (code->finalise() !=0)
        APIERROR (code->getNdbError());

      /* Time to define the update operation to use the
       * InterpretedCode object.  The same finalised object 
       * could be used with multiple operations or even
       * multiple threads
       */
      NdbOperation::OperationOptions oo;
      oo.optionsPresent= 
        NdbOperation::OperationOptions::OO_INTERPRETED;
      oo.interpretedCode= code;

      unsigned char mask= 0;

      pop= myTransaction->updateTuple(pkeyColumnRecord,
                                      (char*) &rowData,
                                      pallColsRecord,
                                      (char*) &rowData,
                                      (const unsigned char *) &mask,  // mask - update nothing
                                      &oo,
                                      sizeof(NdbOperation::OperationOptions));
      if (pop == NULL)
        APIERROR (myTransaction->getNdbError());
      
      // NoCommit execute so we can read the 'after' data.
      if (myTransaction->execute( NdbTransaction::NoCommit ) != 0)
        APIERROR(myTransaction->getNdbError());

      /* Second read op as we can't currently do a 'read after 
       * 'interpreted code' read as part of NdbRecord.
       * We are assuming that the order of op definition == order
       * of execution on a single row, which is not guaranteed.
       */
      const NdbOperation *pop2=
        myTransaction->readTuple(pkeyColumnRecord,
                                 (char*) &rowData,
                                 pallColsRecord,
                                 (char*) &rowData2);
      if (pop2 == NULL)
        APIERROR (myTransaction->getNdbError());
      
      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  // Check return code
  if (myTransaction->getNdbError().status != NdbError::Success)
    APIERROR(myTransaction->getNdbError());

  switch (accessType)
  {
    case api_attr :
    {
      printf(" %2d    %2d    %2d  Before\n"
             " %2d    %2d    %2d  After\n",
             recAttrAttr1->u_32_value(),
             recAttrAttr2->u_32_value(),
             recAttrAttr3->u_32_value(),
             recAttrAttr11->u_32_value(),
             recAttrAttr12->u_32_value(),
             recAttrAttr13->u_32_value());
      break;
    }
  
    case api_record :
    {
      printf(" %2d    %2d    %2d  Before\n"
             " %2d    %2d    %2d  After\n", 
             rowData.attr1, 
             rowData.attr2,
             rowData.attr3,
             rowData2.attr1,
             rowData2.attr2,
             rowData2.attr3);
      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}


/******************************************************
 * Read and print selected rows with interpreted code *
 ******************************************************/
static void do_interpreted_scan(Ndb &myNdb, ApiType accessType)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");

  std::cout << "Running do_interpreted_scan\n";

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;
  
  NdbTransaction *myTransaction=myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbScanOperation *psop;
  NdbRecAttr *recAttrAttr1;
  NdbRecAttr *recAttrAttr2;
  NdbRecAttr *recAttrAttr3;

  /* Create some space on the stack for the program */
  const Uint32 numWords= 64;
  Uint32 space[numWords];
  
  NdbInterpretedCode stackCode(myTable,
                               &space[0],
                               numWords);
  
  NdbInterpretedCode *code= &stackCode;
  
  /* RecAttr and NdbRecord scans both use NdbInterpretedCode
   * Let's define a small scan filter of sorts
   */
  Uint32 comparisonValue= 10;

  // Return rows where 10 > ATTR3 (ATTR3 <10)
  if (code->branch_col_gt(&comparisonValue,
                          sizeof(Uint32),
                          pattr3Col->getColumnNo(),
                          0) != 0)
    APIERROR (myTransaction->getNdbError());

  /* If we get here then we don't return this row */
  if (code->interpret_exit_nok() != 0)
    APIERROR (myTransaction->getNdbError());

  /* Label 0 */
  if (code->def_label(0) != 0)
    APIERROR (myTransaction->getNdbError());

  /* Return this row */
  if (code->interpret_exit_ok() != 0)
    APIERROR (myTransaction->getNdbError());

  /* Finalise the Interpreted Program */
  if (code->finalise() != 0)
    APIERROR (myTransaction->getNdbError());
  
  switch (accessType)
  {
    case api_attr :
    {
      psop=myTransaction->getNdbScanOperation(myTable);
      
      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      if (psop->readTuples(NdbOperation::LM_Read) != 0) APIERROR (myTransaction->getNdbError());
      
      if (psop->setInterpretedCode(code) != 0)
        APIERROR (myTransaction->getNdbError());

      recAttrAttr1=psop->getValue("ATTR1");
      recAttrAttr2=psop->getValue("ATTR2");
      recAttrAttr3=psop->getValue("ATTR3");

      break;
    }
    case api_record :
    {
      NdbScanOperation::ScanOptions so;

      so.optionsPresent = NdbScanOperation::ScanOptions::SO_INTERPRETED;
      so.interpretedCode= code;

      psop=myTransaction->scanTable(pallColsRecord, 
                                    NdbOperation::LM_Read,
                                    NULL, // mask
                                    &so,
                                    sizeof(NdbScanOperation::ScanOptions));

      if (psop == NULL) APIERROR(myTransaction->getNdbError());

      break;
    }
  default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::NoCommit ) != 0)
    APIERROR(myTransaction->getNdbError());
    
  switch (accessType)
  {
    case api_attr :
    {
      while (psop->nextResult(true) == 0)
      {
        printf(" %2d    %2d    %2d\n", 
               recAttrAttr1->u_32_value(),
               recAttrAttr2->u_32_value(),
               recAttrAttr3->u_32_value());
      }

      psop->close();

      break;
    }
    case api_record :
    {
      RowData *prowData; // Ptr to point to our data

      int rc=0;

      while ((rc = psop->nextResult((const char**) &prowData,
                                    true,
                                    false)) == GOT_ROW)
      {
        printf(" %2d    %2d    %2d\n", 
               prowData->attr1,
               prowData->attr2,
               prowData->attr3);
      }

      if (rc != NO_MORE_ROWS)  APIERROR(myTransaction->getNdbError());

      psop->close(true);

      break;
    }
    default :
    {
      std::cout << "Bad branch : " << accessType << "\n";
      exit(-1);
    }
  }

  if(myTransaction->execute( NdbTransaction::Commit ) !=0)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);

  std::cout << "-------\n";
}

/******************************************************
 * Read some data using the default NdbRecord objects *
 ******************************************************/
static void do_read_using_default(Ndb &myNdb)
{
  NdbDictionary::Dictionary* myDict= myNdb.getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_recattr_vs_record");
  const NdbRecord* tableRec= myTable->getDefaultRecord();

  if (myTable == NULL) 
    APIERROR(myDict->getNdbError());
  
  std::cout << "Running do_read_using_default_record (NdbRecord only)\n";
  std::cout << "ATTR1 ATTR2 ATTR3" << std::endl;

  /* Allocate some space for the rows to be read into */
  char* buffer= (char*)malloc(NdbDictionary::getRecordRowLength(tableRec));

  if (buffer== NULL)
  {
    printf("Allocation failed\n");
    exit(-1);
  }
  
  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction= myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    char* attr1= NdbDictionary::getValuePtr(tableRec,
                                            buffer,
                                            attr1ColNum);
    *((unsigned int*)attr1)= i;

    const NdbOperation *pop=
      myTransaction->readTuple(tableRec,
                               buffer,
                               tableRec, // Read everything
                               buffer);
    if (pop==NULL) APIERROR(myTransaction->getNdbError());
    
    if(myTransaction->execute( NdbTransaction::Commit ) == -1)
      APIERROR(myTransaction->getNdbError());
    
    NdbError err= myTransaction->getNdbError();
    if (err.code != 0)
    {
      if (err.classification == NdbError::NoDataFound)
        std::cout << "Detected that tuple " << i << " doesn't exist!" << std::endl;
      else
        APIERROR(myTransaction->getNdbError());
    }
    else
    {
      printf(" %2d    %2d    %2d\n", 
             i, 
             *((unsigned int*) NdbDictionary::getValuePtr(tableRec,
                                                          buffer,
                                                          attr2ColNum)),
             *((unsigned int*) NdbDictionary::getValuePtr(tableRec,
                                                          buffer,
                                                          attr3ColNum)));
    }

    myNdb.closeTransaction(myTransaction);
  }
  
  free(buffer);

  std::cout << "-------\n";
}
