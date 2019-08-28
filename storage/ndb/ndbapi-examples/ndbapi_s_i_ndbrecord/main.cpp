/*
   Copyright (c) 2007, 2014, Oracle and/or its affiliates. All rights reserved.

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
//  ndbapi_simple_index_ndbrecord.cpp: Using secondary unique hash indexes 
//  in NDB API, utilising the NdbRecord interface.
//
//  Correct output from this program is (from a two-node cluster):
//
// ATTR1 ATTR2
//   0     0   (frag=0)
//   1     1   (frag=1)
//   2     2   (frag=1)
//   3     3   (frag=0)
//   4     4   (frag=1)
//   5     5   (frag=1)
//   6     6   (frag=0)
//   7     7   (frag=0)
//   8     8   (frag=1)
//   9     9   (frag=0)
// ATTR1 ATTR2
//   0    10
//   1     1
//   2    12
// Detected that deleted tuple doesn't exist!
//   4    14
//   5     5
//   6    16
//   7     7
//   8    18
//   9     9

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <NdbApi.hpp>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
// Used for cout
#include <iostream>

#define PRINT_ERROR(code,msg) \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code \
            << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql) { \
  PRINT_ERROR(mysql_errno(&mysql),mysql_error(&mysql)); \
  exit(1); }
#define APIERROR(error) { \
  PRINT_ERROR(error.code,error.message); \
  exit(1); }

/* C struct representing layout of data from table
 * api_s_i_ndbrecord in memory
 * This can make it easier to work with rows in the application,
 * but is not necessary - NdbRecord can map columns to any 
 * pattern of offsets.
 * In this program, the same row offsets are used for columns
 * specified as part of a key, and as part of an attribute or
 * result.  This makes the example simpler, but is not 
 * essential.
 */
struct MyTableRow
{
  unsigned int attr1;
  unsigned int attr2;
};

int main(int argc, char** argv)
{
  if (argc != 3)
    {
    std::cout << "Arguments are <socket mysqld> <connect_string cluster>.\n";
    exit(1);
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
      exit(1);
    }
    if ( !mysql_real_connect(&mysql, "localhost", "root", "", "",
                             0, mysqld_sock, 0) )
      MYSQLERROR(mysql);

    mysql_query(&mysql, "CREATE DATABASE ndb_examples");
    if (mysql_query(&mysql, "USE ndb_examples") != 0)
      MYSQLERROR(mysql);

    mysql_query(&mysql, "DROP TABLE api_s_i_ndbrecord");
    if (mysql_query(&mysql,
                    "CREATE TABLE"
                    "  api_s_i_ndbrecord"
                    "    (ATTR1 INT UNSIGNED,"
                    "     ATTR2 INT UNSIGNED NOT NULL,"
                    "     PRIMARY KEY USING HASH (ATTR1),"
                    "     UNIQUE MYINDEXNAME USING HASH (ATTR2))"
                    "  ENGINE=NDB"))
      MYSQLERROR(mysql);
  }

  /**************************************************************
   * Connect to ndb cluster                                     *
   **************************************************************/

  Ndb_cluster_connection *cluster_connection=
    new Ndb_cluster_connection(connectstring); // Object representing the cluster

  if (cluster_connection->connect(5,3,1))
  {
    std::cout << "Connect to cluster management server failed.\n";
    exit(1);
  }

  if (cluster_connection->wait_until_ready(30,30))
  {
    std::cout << "Cluster was not ready within 30 secs.\n";
    exit(1);
  }

  Ndb* myNdb = new Ndb( cluster_connection,
                        "ndb_examples" );  // Object representing the database
  if (myNdb->init() == -1) {
    APIERROR(myNdb->getNdbError());
    exit(1);
  }

  NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("api_s_i_ndbrecord");
  if (myTable == NULL)
    APIERROR(myDict->getNdbError());
  const NdbDictionary::Index *myIndex= myDict->getIndex("MYINDEXNAME$unique","api_s_i_ndbrecord");
  if (myIndex == NULL)
    APIERROR(myDict->getNdbError());

  /* Create NdbRecord descriptors. */
  const NdbDictionary::Column *col1= myTable->getColumn("ATTR1");
  if (col1 == NULL)
    APIERROR(myDict->getNdbError());
  const NdbDictionary::Column *col2= myTable->getColumn("ATTR2");
  if (col2 == NULL)
    APIERROR(myDict->getNdbError());

  /* NdbRecord for primary key lookup. */
  NdbDictionary::RecordSpecification spec[2];
  spec[0].column= col1;
  spec[0].offset= offsetof(MyTableRow, attr1); 
    // So that it goes nicely into the struct
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  const NdbRecord *pk_record=
    myDict->createRecord(myTable, spec, 1, sizeof(spec[0]));
  if (pk_record == NULL)
    APIERROR(myDict->getNdbError());

  /* NdbRecord for all table attributes (insert/read). */
  spec[0].column= col1;
  spec[0].offset= offsetof(MyTableRow, attr1);
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  spec[1].column= col2;
  spec[1].offset= offsetof(MyTableRow, attr2);
  spec[1].nullbit_byte_offset= 0;
  spec[1].nullbit_bit_in_byte= 0;
  const NdbRecord *attr_record=
    myDict->createRecord(myTable, spec, 2, sizeof(spec[0]));
  if (attr_record == NULL)
    APIERROR(myDict->getNdbError());

  /* NdbRecord for unique key lookup. */
  spec[0].column= col2;
  spec[0].offset= offsetof(MyTableRow, attr2);
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  const NdbRecord *key_record=
    myDict->createRecord(myIndex, spec, 1, sizeof(spec[0]));
  if (key_record == NULL)
    APIERROR(myDict->getNdbError());

  MyTableRow row;

  /**************************************************************************
   * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
   **************************************************************************/
  for (int i = 0; i < 5; i++) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());

    /*
      We initialise the row data and pass to each insertTuple operation
      The data is copied in the call to insertTuple and so the original
      row object can be reused for the two operations.
    */
    row.attr1= row.attr2= i;

    const NdbOperation *myOperation=
      myTransaction->insertTuple(attr_record, (const char*)&row);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

    row.attr1= row.attr2= i+5;
    myOperation= 
      myTransaction->insertTuple(attr_record, (const char*)&row);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

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
    if (myTransaction == NULL)
      APIERROR(myNdb->getNdbError());

    /* The optional OperationOptions parameter to NdbRecord methods
     * can be used to specify extra reads of columns which are not in
     * the NdbRecord specification, which need to be stored somewhere
     * other than specified in the NdbRecord specification, or
     * which cannot be specified as part of an NdbRecord (pseudo
     * columns)
     */
    Uint32 frag;
    NdbOperation::GetValueSpec getSpec[1];
    getSpec[0].column=NdbDictionary::Column::FRAGMENT;
    getSpec[0].appStorage=&frag;

    NdbOperation::OperationOptions options;
    options.optionsPresent = NdbOperation::OperationOptions::OO_GETVALUE;
    options.extraGetValues = &getSpec[0];
    options.numExtraGetValues = 1;

    /* We're going to read using the secondary unique hash index
     * Set the value of its column
     */
    row.attr2= i;

    MyTableRow resultRow;

    unsigned char mask[1]= { 0x01 };            // Only read ATTR1 into resultRow
    const NdbOperation *myOperation=
      myTransaction->readTuple(key_record, (const char*) &row,
                               attr_record, (char*) &resultRow,
                               NdbOperation::LM_Read, mask,
                               &options, 
                               sizeof(NdbOperation::OperationOptions));
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

    if (myTransaction->execute( NdbTransaction::Commit,
                                NdbOperation::AbortOnError ) != -1)
    {
      printf(" %2d    %2d   (frag=%u)\n", resultRow.attr1, i, frag);
    }

    myNdb->closeTransaction(myTransaction);
  }

  /*****************************************************************
   * Update the second attribute in half of the tuples (adding 10) *
   *****************************************************************/
  for (int i = 0; i < 10; i+=2) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL)
      APIERROR(myNdb->getNdbError());

    /* Specify key column to lookup in secondary index */
    row.attr2= i;

    /* Specify new column value to set */
    MyTableRow newRowData;
    newRowData.attr2= i+10;
    unsigned char mask[1]= { 0x02 };            // Only update ATTR2

    const NdbOperation *myOperation=
      myTransaction->updateTuple(key_record, (const char*)&row,
                                 attr_record,(char*) &newRowData, mask);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

    if ( myTransaction->execute( NdbTransaction::Commit ) == -1 )
      APIERROR(myTransaction->getNdbError());

    myNdb->closeTransaction(myTransaction);
  }

  /*************************************************
   * Delete one tuple (the one with unique key 3) *
   *************************************************/
  {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL)
      APIERROR(myNdb->getNdbError());

    row.attr2= 3;
    const NdbOperation *myOperation=
      myTransaction->deleteTuple(key_record, (const char*) &row,
                                 attr_record);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

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
      if (myTransaction == NULL)
        APIERROR(myNdb->getNdbError());

      row.attr1= i;
      
      /* Read using pk.  Note the same row space is used as 
       * key and result storage space
       */
      const NdbOperation *myOperation=
        myTransaction->readTuple(pk_record, (const char*) &row,
                                 attr_record, (char*) &row);
      if (myOperation == NULL)
        APIERROR(myTransaction->getNdbError());

      if (myTransaction->execute( NdbTransaction::Commit,
                                  NdbOperation::AbortOnError ) == -1)
      {
        if (i == 3) {
          std::cout << "Detected that deleted tuple doesn't exist!\n";
        } else {
          APIERROR(myTransaction->getNdbError());
        }
      }
      
      if (i != 3) 
        printf(" %2d    %2d\n", row.attr1, row.attr2);

      myNdb->closeTransaction(myTransaction);
    }
  }
  
  delete myNdb;
  delete cluster_connection;

  ndb_end(0);
  return 0;
}
