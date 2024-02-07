/*
   Copyright (c) 2014, 2024, Oracle and/or its affiliates.

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

#include <NdbApi.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

#include "../common/array_adapter.hpp"
#include "../common/error_handling.hpp"
#include "../common/ndb_util.hpp"
#include "../common/util.hpp"

/**
This program inserts [VAR]CHAR/BINARY column data into the table
by constructing aRefs using array adapters and then reads those
columns back and extracts the data using array adapters.

schema used
CREATE TABLE api_array_using_adapter(
  ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,
  ATTR2 CHAR(20) NOT NULL,
  ATTR3 VARCHAR(20) NOT NULL,
  ATTR4 VARCHAR(500) NOT NULL,
  ATTR5 BINARY(20) NOT NULL,
  ATTR6 VARBINARY(20) NOT NULL,
  ATTR7 VARBINARY(500) NOT NULL
) engine ndb charset latin1;
 */

// Do a cleanup of all inserted rows
static void do_cleanup(Ndb &ndb) {
  const NdbDictionary::Dictionary *dict = ndb.getDictionary();

  const NdbDictionary::Table *table = dict->getTable("api_array_using_adapter");
  if (table == nullptr) APIERROR(dict->getNdbError());

  NdbTransaction *transaction = ndb.startTransaction();
  if (transaction == nullptr) APIERROR(ndb.getNdbError());

  // Delete all 21 rows using a single transaction
  for (int i = 0; i <= 20; i++) {
    NdbOperation *myOperation = transaction->getNdbOperation(table);
    if (myOperation == nullptr) APIERROR(transaction->getNdbError());
    myOperation->deleteTuple();
    myOperation->equal("ATTR1", i);
  }

  if (transaction->execute(NdbTransaction::Commit) != 0) {
    APIERROR(transaction->getNdbError());
  }
  ndb.closeTransaction(transaction);
}

// Use one transaction and insert 21 rows in one batch.
static void do_insert(Ndb &ndb) {
  const NdbDictionary::Dictionary *dict = ndb.getDictionary();
  const NdbDictionary::Table *table = dict->getTable("api_array_using_adapter");

  if (table == NULL) {
    APIERROR(dict->getNdbError());
  }

  // Get a column object for each CHAR/VARCHAR/BINARY/VARBINARY column
  // to insert into.
  const NdbDictionary::Column *column2 = table->getColumn("ATTR2");
  if (column2 == NULL) {
    APIERROR(dict->getNdbError());
  }

  const NdbDictionary::Column *column3 = table->getColumn("ATTR3");
  if (column3 == NULL) {
    APIERROR(dict->getNdbError());
  }

  const NdbDictionary::Column *column4 = table->getColumn("ATTR4");
  if (column4 == NULL) {
    APIERROR(dict->getNdbError());
  }

  const NdbDictionary::Column *column5 = table->getColumn("ATTR5");
  if (column5 == NULL) {
    APIERROR(dict->getNdbError());
  }

  const NdbDictionary::Column *column6 = table->getColumn("ATTR6");
  if (column6 == NULL) {
    APIERROR(dict->getNdbError());
  }

  const NdbDictionary::Column *column7 = table->getColumn("ATTR7");
  if (column7 == NULL) {
    APIERROR(dict->getNdbError());
  }

  // Create a read/write attribute adapter to be used for all
  // CHAR/VARCHAR/BINARY/VARBINARY columns.
  ReadWriteArrayAdapter attr_adapter;

  // Create and initialize sample data.
  const string meter = 50 * string("''''-,,,,|");
  unsigned char binary_meter[500];
  for (unsigned i = 0; i < 500; i++) {
    binary_meter[i] = (unsigned char)(i % 256);
  }

  NdbTransaction *transaction = ndb.startTransaction();
  if (transaction == NULL) APIERROR(ndb.getNdbError());

  // Create 21 operations and put a reference to them in a vector to
  // be able to find failing operations.
  vector<NdbOperation *> operations;
  for (int i = 0; i <= 20; i++) {
    NdbOperation *operation = transaction->getNdbOperation(table);
    if (operation == NULL) APIERROR(transaction->getNdbError());
    operation->insertTuple();

    operation->equal("ATTR1", i);

    /* use ReadWrite Adapter to convert string to aRefs */
    ReadWriteArrayAdapter::ErrorType error;

    char *attr2_aRef;
    attr2_aRef = attr_adapter.make_aRef(column2, meter.substr(0, i), error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "make_aRef failed for ATTR2");
    operation->setValue("ATTR2", attr2_aRef);

    char *attr3_aRef;
    attr3_aRef = attr_adapter.make_aRef(column3, meter.substr(0, i), error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "make_aRef failed for ATTR3");
    operation->setValue("ATTR3", attr3_aRef);

    char *attr4_aRef;
    attr4_aRef =
        attr_adapter.make_aRef(column4, meter.substr(0, 20 * i), error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "make_aRef failed for ATTR4");
    operation->setValue("ATTR4", attr4_aRef);

    char *attr5_aRef;
    char *attr5_first;
    attr_adapter.allocate_in_bytes(column5, attr5_aRef, attr5_first, i, error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "allocate_in_bytes failed for ATTR5");
    memcpy(attr5_first, binary_meter, i);
    operation->setValue("ATTR5", attr5_aRef);

    char *attr6_aRef;
    char *attr6_first;
    attr_adapter.allocate_in_bytes(column6, attr6_aRef, attr6_first, i, error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "allocate_in_bytes failed for ATTR6");
    memcpy(attr6_first, binary_meter, i);
    operation->setValue("ATTR6", attr6_aRef);

    char *attr7_aRef;
    char *attr7_first;
    attr_adapter.allocate_in_bytes(column7, attr7_aRef, attr7_first, 20 * i,
                                   error);
    PRINT_IF_NOT_EQUAL(error, ReadWriteArrayAdapter::Success,
                       "allocate_in_bytes failed for ATTR7");
    memcpy(attr7_first, binary_meter, 20 * i);
    operation->setValue("ATTR7", attr7_aRef);

    operations.push_back(operation);
  }

  // Now execute all operations in one batch, and check for errors.
  if (transaction->execute(NdbTransaction::Commit) != 0) {
    for (size_t i = 0; i < operations.size(); i++) {
      const NdbError err = operations[i]->getNdbError();
      if (err.code != NdbError::Success) {
        cout << "Error inserting Row : " << i << endl;
        PRINT_ERROR(err.code, err.message);
      }
    }
    APIERROR(transaction->getNdbError());
  }
  ndb.closeTransaction(transaction);
}

/*
 Reads the row with id = 17
 Retrieves an prints value of the [VAR]CHAR/BINARY using array_adapter
 */
static void do_read(Ndb &ndb) {
  const NdbDictionary::Dictionary *dict = ndb.getDictionary();
  const NdbDictionary::Table *table = dict->getTable("api_array_using_adapter");

  if (table == NULL) APIERROR(dict->getNdbError());

  NdbTransaction *transaction = ndb.startTransaction();
  if (transaction == NULL) APIERROR(ndb.getNdbError());

  NdbOperation *operation = transaction->getNdbOperation(table);
  if (operation == NULL) APIERROR(transaction->getNdbError());

  operation->readTuple(NdbOperation::LM_Read);
  operation->equal("ATTR1", 17);

  vector<NdbRecAttr *> attr;
  const int column_count = table->getNoOfColumns();
  attr.reserve(column_count);
  attr.push_back(nullptr);
  for (int i = 1; i < column_count; i++) {
    attr.push_back(operation->getValue(i, NULL));
    if (attr[i] == NULL) APIERROR(transaction->getNdbError());
  }

  if (transaction->execute(NdbTransaction::Commit) == -1)
    APIERROR(transaction->getNdbError());

  /* Now use an array adapter to read the data from columns */
  const ReadOnlyArrayAdapter attr_adapter;
  ReadOnlyArrayAdapter::ErrorType error;

  /* print the fetched data */
  cout << "Row ID : 17\n";
  for (int i = 1; i < column_count; i++) {
    if (attr[i] != NULL) {
      NdbDictionary::Column::Type column_type = attr[i]->getType();
      cout << "Column id: " << i
           << ", name: " << attr[i]->getColumn()->getName()
           << ", size: " << attr[i]->get_size_in_bytes()
           << ", type: " << column_type_to_string(attr[i]->getType());
      if (attr_adapter.is_binary_array_type(column_type)) {
        /* if column is [VAR]BINARY, get the byte array and print their sum */
        const char *data_ptr;
        size_t data_length;
        attr_adapter.get_byte_array(attr[i], data_ptr, data_length, error);
        if (error == ReadOnlyArrayAdapter::Success) {
          int sum = 0;
          for (size_t j = 0; j < data_length; j++) sum += (int)(data_ptr[j]);
          cout << ", stored bytes length: " << data_length
               << ", sum of byte array: " << sum << endl;
        } else
          cout << ", error fetching value." << endl;
      } else {
        /* if the column is [VAR]CHAR, retrieve the string and print */
        std::string value = attr_adapter.get_string(attr[i], error);
        if (error == ReadOnlyArrayAdapter::Success) {
          cout << ", stored string length: " << value.length()
               << ", value: " << value << endl;
        } else
          cout << ", error fetching value." << endl;
      }
    }
  }

  ndb.closeTransaction(transaction);
}

static void run_application(Ndb_cluster_connection &cluster_connection,
                            const char *database_name) {
  /********************************************
   * Connect to database via NdbApi           *
   ********************************************/
  // Object representing the database
  Ndb ndb(&cluster_connection, database_name);
  if (ndb.init()) APIERROR(ndb.getNdbError());

  /*
   * Do different operations on database
   */
  do_insert(ndb);
  do_read(ndb);
  do_cleanup(ndb);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cout << "Arguments are <connect_string cluster> <database_name>.\n";
    exit(-1);
  }
  /* ndb_init must be called first */
  ndb_init();
  {
    /* connect to cluster */
    const char *connectstring = argv[1];
    Ndb_cluster_connection cluster_connection(connectstring);
    if (cluster_connection.connect(
            30 /* retries */, 1 /* delay between retries */, 0 /* verbose */)) {
      std::cout << "Cluster management server was not ready within 30 secs.\n";
      exit(-1);
    }

    /* Connect and wait for the storage nodes */
    if (cluster_connection.wait_until_ready(30, 10) < 0) {
      std::cout << "Cluster was not ready within 30 secs.\n";
      exit(-1);
    }

    /* run the application code */
    const char *dbname = argv[2];
    run_application(cluster_connection, dbname);
  }
  ndb_end(0);

  return 0;
}
