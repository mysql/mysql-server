/*
   Copyright (c) 2006, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

/*
 *  ndbapi_simple_dual.cpp: Using synchronous transactions in NDB API
 *
 *  Correct output from this program is:
 *
 *  ATTR1 ATTR2
 *    0    10
 *    1     1
 *    2    12
 *  Detected that deleted tuple doesn't exist!
 *    4    14
 *    5     5
 *    6    16
 *    7     7
 *    8    18
 *    9     9
 *  ATTR1 ATTR2
 *    0    10
 *    1     1
 *    2    12
 *  Detected that deleted tuple doesn't exist!
 *    4    14
 *    5     5
 *    6    16
 *    7     7
 *    8    18
 *    9     9
 *
 */

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <mysql.h>
#include <stdlib.h>
#include <NdbApi.hpp>
// Used for cout
#include <stdio.h>
#include <iostream>

static void run_application(MYSQL &, Ndb_cluster_connection &,
                            const char *table, const char *db);

#define PRINT_ERROR(code, msg)                                   \
  std::cout << "Error in " << __FILE__ << ", line: " << __LINE__ \
            << ", code: " << code << ", msg: " << msg << "." << std::endl
#define MYSQLERROR(mysql)                                  \
  {                                                        \
    PRINT_ERROR(mysql_errno(&mysql), mysql_error(&mysql)); \
    exit(-1);                                              \
  }
#define APIERROR(error)                     \
  {                                         \
    PRINT_ERROR(error.code, error.message); \
    exit(-1);                               \
  }

int main(int argc, char **argv) {
  if (argc != 5) {
    std::cout << "Arguments are <socket mysqld1> <connect_string cluster 1> "
                 "<socket mysqld2> <connect_string cluster 2>.\n";
    exit(-1);
  }
  // ndb_init must be called first
  ndb_init();
  {
    char *mysqld1_sock = argv[1];
    const char *connectstring1 = argv[2];
    char *mysqld2_sock = argv[3];
    const char *connectstring2 = argv[4];

    // Object representing the cluster 1
    Ndb_cluster_connection cluster1_connection(connectstring1);
    MYSQL mysql1;
    // Object representing the cluster 2
    Ndb_cluster_connection cluster2_connection(connectstring2);
    MYSQL mysql2;

    // connect to mysql server and cluster 1 and run application
    // Connect to cluster 1  management server (ndb_mgmd)
    if (cluster1_connection.connect(4 /* retries               */,
                                    5 /* delay between retries */,
                                    1 /* verbose               */)) {
      std::cout
          << "Cluster 1 management server was not ready within 30 secs.\n";
      exit(-1);
    }
    // Optionally connect and wait for the storage nodes (ndbd's)
    if (cluster1_connection.wait_until_ready(30, 0) < 0) {
      std::cout << "Cluster 1 was not ready within 30 secs.\n";
      exit(-1);
    }
    // connect to mysql server in cluster 1
    if (!mysql_init(&mysql1)) {
      std::cout << "mysql_init failed\n";
      exit(-1);
    }
    if (!mysql_real_connect(&mysql1, "localhost", "root", "", "", 0,
                            mysqld1_sock, 0))
      MYSQLERROR(mysql1);

    // connect to mysql server and cluster 2 and run application

    // Connect to cluster management server (ndb_mgmd)
    if (cluster2_connection.connect(4 /* retries               */,
                                    5 /* delay between retries */,
                                    1 /* verbose               */)) {
      std::cout
          << "Cluster 2 management server was not ready within 30 secs.\n";
      exit(-1);
    }
    // Optionally connect and wait for the storage nodes (ndbd's)
    if (cluster2_connection.wait_until_ready(30, 0) < 0) {
      std::cout << "Cluster 2 was not ready within 30 secs.\n";
      exit(-1);
    }
    // connect to mysql server in cluster 2
    if (!mysql_init(&mysql2)) {
      std::cout << "mysql_init failed\n";
      exit(-1);
    }
    if (!mysql_real_connect(&mysql2, "localhost", "root", "", "", 0,
                            mysqld2_sock, 0))
      MYSQLERROR(mysql2);

    // run the application code
    run_application(mysql1, cluster1_connection, "api_simple_dual_1",
                    "ndb_examples");
    run_application(mysql2, cluster2_connection, "api_simple_dual_2",
                    "ndb_examples");

    mysql_close(&mysql1);
    mysql_close(&mysql2);
  }
  // Note: all connections must have been destroyed before calling ndb_end()
  ndb_end(0);

  return 0;
}

static void create_table(MYSQL &, const char *table);
static void do_insert(Ndb &, const char *table);
static void do_update(Ndb &, const char *table);
static void do_delete(Ndb &, const char *table);
static void do_read(Ndb &, const char *table);
static void drop_table(MYSQL &, const char *table);

static void run_application(MYSQL &mysql,
                            Ndb_cluster_connection &cluster_connection,
                            const char *table, const char *db) {
  /********************************************
   * Connect to database via mysql-c          *
   ********************************************/
  char db_stmt[256];
  sprintf(db_stmt, "CREATE DATABASE %s\n", db);
  mysql_query(&mysql, db_stmt);
  sprintf(db_stmt, "USE %s", db);
  if (mysql_query(&mysql, db_stmt) != 0) MYSQLERROR(mysql);
  create_table(mysql, table);

  /********************************************
   * Connect to database via NdbApi           *
   ********************************************/
  // Object representing the database
  Ndb myNdb(&cluster_connection, db);
  if (myNdb.init()) APIERROR(myNdb.getNdbError());

  /*
   * Do different operations on database
   */
  do_insert(myNdb, table);
  do_update(myNdb, table);
  do_delete(myNdb, table);
  do_read(myNdb, table);
  /*
   * Drop the table
   */
  drop_table(mysql, table);
}

/*********************************************************
 * Create a table named by table if it does not exist *
 *********************************************************/
static void create_table(MYSQL &mysql, const char *table) {
  char create_stmt[256];

  sprintf(create_stmt,
          "CREATE TABLE %s \
		         (ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,\
		          ATTR2 INT UNSIGNED NOT NULL)\
		         ENGINE=NDB",
          table);
  if (mysql_query(&mysql, create_stmt)) MYSQLERROR(mysql);
}

/**************************************************************************
 * Using 5 transactions, insert 10 tuples in table: (0,0),(1,1),...,(9,9) *
 **************************************************************************/
static void do_insert(Ndb &myNdb, const char *table) {
  const NdbDictionary::Dictionary *myDict = myNdb.getDictionary();
  const NdbDictionary::Table *myTable = myDict->getTable(table);

  if (myTable == NULL) APIERROR(myDict->getNdbError());

  for (int i = 0; i < 5; i++) {
    NdbTransaction *myTransaction = myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    NdbOperation *myOperation = myTransaction->getNdbOperation(myTable);
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->insertTuple();
    myOperation->equal("ATTR1", i);
    myOperation->setValue("ATTR2", i);

    myOperation = myTransaction->getNdbOperation(myTable);
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->insertTuple();
    myOperation->equal("ATTR1", i + 5);
    myOperation->setValue("ATTR2", i + 5);

    if (myTransaction->execute(NdbTransaction::Commit) == -1)
      APIERROR(myTransaction->getNdbError());

    myNdb.closeTransaction(myTransaction);
  }
}

/*****************************************************************
 * Update the second attribute in half of the tuples (adding 10) *
 *****************************************************************/
static void do_update(Ndb &myNdb, const char *table) {
  const NdbDictionary::Dictionary *myDict = myNdb.getDictionary();
  const NdbDictionary::Table *myTable = myDict->getTable(table);

  if (myTable == NULL) APIERROR(myDict->getNdbError());

  for (int i = 0; i < 10; i += 2) {
    NdbTransaction *myTransaction = myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    NdbOperation *myOperation = myTransaction->getNdbOperation(myTable);
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->updateTuple();
    myOperation->equal("ATTR1", i);
    myOperation->setValue("ATTR2", i + 10);

    if (myTransaction->execute(NdbTransaction::Commit) == -1)
      APIERROR(myTransaction->getNdbError());

    myNdb.closeTransaction(myTransaction);
  }
}

/*************************************************
 * Delete one tuple (the one with primary key 3) *
 *************************************************/
static void do_delete(Ndb &myNdb, const char *table) {
  const NdbDictionary::Dictionary *myDict = myNdb.getDictionary();
  const NdbDictionary::Table *myTable = myDict->getTable(table);

  if (myTable == NULL) APIERROR(myDict->getNdbError());

  NdbTransaction *myTransaction = myNdb.startTransaction();
  if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

  NdbOperation *myOperation = myTransaction->getNdbOperation(myTable);
  if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

  myOperation->deleteTuple();
  myOperation->equal("ATTR1", 3);

  if (myTransaction->execute(NdbTransaction::Commit) == -1)
    APIERROR(myTransaction->getNdbError());

  myNdb.closeTransaction(myTransaction);
}

/*****************************
 * Read and print all tuples *
 *****************************/
static void do_read(Ndb &myNdb, const char *table) {
  const NdbDictionary::Dictionary *myDict = myNdb.getDictionary();
  const NdbDictionary::Table *myTable = myDict->getTable(table);

  if (myTable == NULL) APIERROR(myDict->getNdbError());

  std::cout << "ATTR1 ATTR2" << std::endl;

  for (int i = 0; i < 10; i++) {
    NdbTransaction *myTransaction = myNdb.startTransaction();
    if (myTransaction == NULL) APIERROR(myNdb.getNdbError());

    NdbOperation *myOperation = myTransaction->getNdbOperation(myTable);
    if (myOperation == NULL) APIERROR(myTransaction->getNdbError());

    myOperation->readTuple(NdbOperation::LM_Read);
    myOperation->equal("ATTR1", i);

    NdbRecAttr *myRecAttr = myOperation->getValue("ATTR2", NULL);
    if (myRecAttr == NULL) APIERROR(myTransaction->getNdbError());

    if (myTransaction->execute(NdbTransaction::Commit) == -1) {
      if (i == 3) {
        std::cout << "Detected that deleted tuple doesn't exist!" << std::endl;
      } else {
        APIERROR(myTransaction->getNdbError());
      }
    }

    if (i != 3) {
      printf(" %2d    %2d\n", i, myRecAttr->u_32_value());
    }
    myNdb.closeTransaction(myTransaction);
  }
}

/**************************
 * Drop table after usage *
 **************************/
static void drop_table(MYSQL &mysql, const char *table) {
  char drop_stmt[75];
  sprintf(drop_stmt, "DROP TABLE %s", table);
  if (mysql_query(&mysql, drop_stmt)) MYSQLERROR(mysql);
}
