/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

//
//  ndbapi_simple_index_ndbrecord.cpp: Using secondary hash indexes in NDB API,
//  utilising the NdbRecord interface.
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

#include <mysql.h>
#include <NdbApi.hpp>

// Used for cout
#include <stdio.h>
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

    mysql_query(&mysql, "CREATE DATABASE TEST_DB_1");
    if (mysql_query(&mysql, "USE TEST_DB_1") != 0)
      MYSQLERROR(mysql);

    mysql_query(&mysql, "DROP TABLE MYTABLENAME");
    if (mysql_query(&mysql,
                    "CREATE TABLE"
                    "  MYTABLENAME"
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
                        "TEST_DB_1" );  // Object representing the database
  if (myNdb->init() == -1) {
    APIERROR(myNdb->getNdbError());
    exit(1);
  }

  NdbDictionary::Dictionary* myDict= myNdb->getDictionary();
  const NdbDictionary::Table *myTable= myDict->getTable("MYTABLENAME");
  if (myTable == NULL)
    APIERROR(myDict->getNdbError());
  const NdbDictionary::Index *myIndex= myDict->getIndex("MYINDEXNAME$unique","MYTABLENAME");
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
  spec[0].offset= 0;
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  const NdbRecord *pk_record=
    myDict->createRecord(myTable, spec, 1, sizeof(spec[0]));
  if (pk_record == NULL)
    APIERROR(myDict->getNdbError());

  /* NdbRecord for all table attributes (insert/read). */
  spec[0].column= col1;
  spec[0].offset= 0;
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  spec[1].column= col2;
  spec[1].offset= 4;
  spec[1].nullbit_byte_offset= 0;
  spec[1].nullbit_bit_in_byte= 0;
  const NdbRecord *attr_record=
    myDict->createRecord(myTable, spec, 2, sizeof(spec[0]));
  if (attr_record == NULL)
    APIERROR(myDict->getNdbError());

  /* NdbRecord for unique key lookup. */
  spec[0].column= col2;
  spec[0].offset= 4;
  spec[0].nullbit_byte_offset= 0;
  spec[0].nullbit_bit_in_byte= 0;
  const NdbRecord *key_record=
    myDict->createRecord(myIndex, spec, 1, sizeof(spec[0]));
  if (key_record == NULL)
    APIERROR(myDict->getNdbError());
  char row[2][8];

  /**************************************************************************
   * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
   **************************************************************************/
  for (int i = 0; i < 5; i++) {
    NdbTransaction *myTransaction= myNdb->startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb->getNdbError());

    /*
      Fill in rows with data. We need two rows, as the data must remain valid
      until NdbTransaction::execute() returns.
    */
    memcpy(&row[0][0], &i, 4);
    memcpy(&row[0][4], &i, 4);
    int value= i+5;
    memcpy(&row[1][0], &value, 4);
    memcpy(&row[1][4], &value, 4);

    NdbOperation *myOperation=
      myTransaction->insertTuple(attr_record, &row[0][0]);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());
    myOperation= 
      myTransaction->insertTuple(attr_record, &row[1][0]);
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

    memcpy(&row[0][4], &i, 4);
    unsigned char mask[1]= { 0x01 };            // Only read ATTR1
    NdbOperation *myOperation=
      myTransaction->readTuple(key_record, &row[0][0],
                               attr_record, &row[1][0],
                               NdbOperation::LM_Read, mask);
    if (myOperation == NULL)
      APIERROR(myTransaction->getNdbError());

    /* Demonstrate the posibility to use getValue() for the odd extra read. */
    Uint32 frag;
    if (myOperation->getValue(NdbDictionary::Column::FRAGMENT,
                              (char *)(&frag)) == 0)
      APIERROR(myOperation->getNdbError());

    if (myTransaction->execute( NdbTransaction::Commit,
                                NdbOperation::AbortOnError ) != -1)
    {
      int value;
      memcpy(&value, &row[1][0], 4);
      printf(" %2d    %2d   (frag=%u)\n", value, i, frag);
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

    memcpy(&row[0][4], &i, 4);
    int value= i+10;
    memcpy(&row[1][4], &value, 4);
    unsigned char mask[1]= { 0x02 };            // Only update ATTR2
    NdbOperation *myOperation=
      myTransaction->updateTuple(key_record, &row[0][0],
                                 attr_record, &row[1][0], mask);
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

    int value= 3;
    memcpy(&row[0][4], &value, 4);
    NdbOperation *myOperation=
      myTransaction->deleteTuple(key_record, &row[0][0]);
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

      memcpy(&row[0][0], &i, 4);
      NdbOperation *myOperation=
        myTransaction->readTuple(pk_record, &row[0][0],
                                 attr_record, &row[1][0]);
      if (myOperation == NULL)
        APIERROR(myTransaction->getNdbError());

      if (myTransaction->execute( NdbTransaction::Commit,
                                  NdbOperation::AbortOnError ) == -1)
        if (i == 3) {
          std::cout << "Detected that deleted tuple doesn't exist!\n";
        } else {
          APIERROR(myTransaction->getNdbError());
        }

      if (i != 3) {
        int value1, value2;
        memcpy(&value1, &row[1][0], 4);
        memcpy(&value2, &row[1][4], 4);
        printf(" %2d    %2d\n", value1, value2);
      }
      myNdb->closeTransaction(myTransaction);
    }
  }

  /**************
   * Drop table *
   **************/
  if (mysql_query(&mysql, "DROP TABLE MYTABLENAME"))
    MYSQLERROR(mysql);

  delete myNdb;
  delete cluster_connection;

  ndb_end(0);
  return 0;
}
