/*
   Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <NdbApi.hpp>
#include <iostream>
#include <vector>
#include <cstdlib>
#include <cstring>

#include "../common/error_handling.hpp"
#include "../common/ndb_util.hpp"
#include "../common/util.hpp"

using namespace std;

/**
This program inserts [VAR]CHAR/BINARY column data into the table
by constructing aRefs using local functions and then reads those
columns back and extracts the data using local functions.

schema used
create table api_array_simple(
  ATTR1 INT UNSIGNED NOT NULL PRIMARY KEY,
  ATTR2 CHAR(20) NOT NULL,
  ATTR3 VARCHAR(20) NOT NULL,
  ATTR4 VARCHAR(500) NOT NULL,
  ATTR5 BINARY(20) NOT NULL,
  ATTR6 VARBINARY(20) NOT NULL,
  ATTR7 VARBINARY(500) NOT NULL
) engine ndb charset latin1;
 */

/* structure to help in insertion */
struct RowData
{
  /* id */
  int attr1;
  /* CHAR(20)- fixed length, no additional length bytes */
  char attr2[20];
  /* VARCHAR(20) - requires one additional length byte (length < 256 ) */
  char attr3[1 + 20];
  /* VARCHAR(500) - requires two additional length bytes (length > 256 ) */
  char attr4[2 + 500];
  /* BINARY(20) - fixed length, requires no additional length byte */
  char attr5[20];
  /* VARBINARY(20) - requires one additional length byte (length < 256 ) */
  char attr6[1 + 20];
  /* VARBINARY(20) - requires one additional length byte (length > 256 ) */
  char attr7[2 + 500];
};

/* extracts the length and the start byte of the data stored */
static int get_byte_array(const NdbRecAttr* attr,
                          const char*& first_byte,
                          size_t& bytes)
{
  const NdbDictionary::Column::ArrayType array_type =
    attr->getColumn()->getArrayType();
  const size_t attr_bytes = attr->get_size_in_bytes();
  const char* aRef = attr->aRef();
  string result;

  switch (array_type) {
  case NdbDictionary::Column::ArrayTypeFixed:
    /*
     No prefix length is stored in aRef. Data starts from aRef's first byte
     data might be padded with blank or null bytes to fill the whole column
     */
    first_byte = aRef;
    bytes = attr_bytes;
    return 0;
  case NdbDictionary::Column::ArrayTypeShortVar:
    /*
     First byte of aRef has the length of data stored
     Data starts from second byte of aRef
     */
    first_byte = aRef + 1;
    bytes = (size_t)(aRef[0]);
    return 0;
  case NdbDictionary::Column::ArrayTypeMediumVar:
    /*
     First two bytes of aRef has the length of data stored
     Data starts from third byte of aRef
     */
    first_byte = aRef + 2;
    bytes = (size_t)(aRef[1]) * 256 + (size_t)(aRef[0]);
    return 0;
  default:
    first_byte = NULL;
    bytes = 0;
    return -1;
  }
}

/*
 Extracts the string from given NdbRecAttr
 Uses get_byte_array internally
 */
static int get_string(const NdbRecAttr* attr, string& str)
{
  size_t attr_bytes;
  const char* data_start_ptr = NULL;

  /* get stored length and data using get_byte_array */
  if(get_byte_array(attr, data_start_ptr, attr_bytes) == 0)
  {
    /* we have length of the string and start location */
    str= string(data_start_ptr, attr_bytes);
    if(attr->getType() == NdbDictionary::Column::Char)
    {
      /* Fixed Char : remove blank spaces at the end */
      size_t endpos = str.find_last_not_of(" ");
      if( string::npos != endpos )
      {
        str = str.substr(0, endpos+1);
      }
    }
  }
  return 0;
}

// Do a cleanup of all inserted tuples
static void do_cleanup(Ndb& ndb)
{
  const NdbDictionary::Dictionary* dict = ndb.getDictionary();

  const NdbDictionary::Table *table = dict->getTable("api_array_simple");
  if (table == nullptr) APIERROR(dict->getNdbError());

  NdbTransaction *transaction= ndb.startTransaction();
  if (transaction == nullptr) APIERROR(ndb.getNdbError());

  for (int i = 0; i <= 20; i++)
  {
    NdbOperation* myOperation = transaction->getNdbOperation(table);
    if (myOperation == nullptr) APIERROR(transaction->getNdbError());
    myOperation->deleteTuple();
    myOperation->equal("ATTR1", i);
  }

  if (transaction->execute(NdbTransaction::Commit) != 0)
  {
    APIERROR(transaction->getNdbError());
  }
  ndb.closeTransaction(transaction);
}

/*******************************************************
 * Use one transaction and insert 21 rows in one batch *
 *******************************************************/
static void do_insert(Ndb& ndb)
{
  const NdbDictionary::Dictionary* dict = ndb.getDictionary();
  const NdbDictionary::Table *table = dict->getTable("api_array_simple");

  if (table == NULL) APIERROR(dict->getNdbError());

  NdbTransaction *transaction= ndb.startTransaction();
  if (transaction == NULL) APIERROR(ndb.getNdbError());

  /* Create and initialize sample data */
  const string meter = 50 * string("''''-,,,,|");
  const string space = 20 * string(" ");
  unsigned char binary_meter[500];
  for (unsigned i = 0; i < 500; i++)
  {
    binary_meter[i] = (unsigned char)(i % 256);
  }

  vector<NdbOperation*> operations;
  for (int i = 0; i <= 20; i++)
  {
    RowData data;
    NdbOperation* myOperation = transaction->getNdbOperation(table);
    if (myOperation == NULL) APIERROR(transaction->getNdbError());
    data.attr1 = i;

    // Fill CHAR(20) with 'i' chars from meter
    strncpy (data.attr2, meter.c_str(), i);
    // Pad it with space up to 20 chars
    strncpy (data.attr2 + i, space.c_str(), 20 - i);

    // Fill VARCHAR(20) with 'i' chars from meter. First byte is
    // reserved for length field. No padding is needed.
    strncpy (data.attr3 + 1, meter.c_str(), i);
    // Set the length byte
    data.attr3[0] = (char)i;

    // Fill VARCHAR(500) with 20*i chars from meter. First two bytes
    // are reserved for length field. No padding is needed.
    strncpy (data.attr4 + 2, meter.c_str(), 20*i);
    // Set the length bytes
    data.attr4[0] = (char)(20*i % 256);
    data.attr4[1] = (char)(20*i / 256);

    // Fill BINARY(20) with 'i' bytes from binary_meter.
    memcpy(data.attr5, binary_meter, i);
    // Pad with 0 up to 20 bytes.
    memset(data.attr5 + i, 0, 20 - i);

    // Fill VARBINARY(20) with 'i' bytes from binary_meter. First byte
    // is reserved for length field. No padding is needed.
    memcpy(data.attr6 + 1, binary_meter, i);
    // Set the length byte
    data.attr6[0] = (char)i;

    // Fill VARBINARY(500) with 'i' bytes from binary_meter. First two
    // bytes are reserved for length filed. No padding is needed.
    memcpy(data.attr7 + 2, binary_meter, 20*i);
    // Set the length bytes
    data.attr7[0] = (char)(20*i % 256);
    data.attr7[1] = (char)(20*i / 256);

    myOperation->insertTuple();
    myOperation->equal("ATTR1", data.attr1);
    myOperation->setValue("ATTR2", data.attr2);
    myOperation->setValue("ATTR3", data.attr3);
    myOperation->setValue("ATTR4", data.attr4);
    myOperation->setValue("ATTR5", data.attr5);
    myOperation->setValue("ATTR6", data.attr6);
    myOperation->setValue("ATTR7", data.attr7);

    operations.push_back(myOperation);
  }

  // Now execute all operations in one batch, and check for errors.
  if (transaction->execute( NdbTransaction::Commit ) != 0)
  {
    for (size_t i = 0; i < operations.size(); i++)
    {
      const NdbError err= operations[i]->getNdbError();
      if(err.code != NdbError::Success)
      {
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
 Retrieves an prints value of the [VAR]CHAR/BINARY
 */
static void do_read(Ndb& ndb)
{
  const NdbDictionary::Dictionary* dict= ndb.getDictionary();
  const NdbDictionary::Table* table= dict->getTable("api_array_simple");

  if (table == NULL) APIERROR(dict->getNdbError());

  NdbTransaction *transaction= ndb.startTransaction();
  if (transaction == NULL) APIERROR(ndb.getNdbError());

  NdbOperation *operation= transaction->getNdbOperation(table);
  if (operation == NULL) APIERROR(transaction->getNdbError());

  /* create and execute a read operation */
  operation->readTuple(NdbOperation::LM_Read);
  operation->equal("ATTR1", 17);

  vector<NdbRecAttr*> attr;
  const int column_count= table->getNoOfColumns();
  attr.reserve(column_count);

  for (int i= 1; i < column_count; i++)
  {
    attr[i] = operation->getValue(i, NULL);
    if (attr[i] == NULL) APIERROR(transaction->getNdbError());
  }

  if(transaction->execute( NdbTransaction::Commit ) == -1)
    APIERROR(transaction->getNdbError());

  /* print the fetched data */
  cout << "Row ID : 17\n";
  for (int i= 1; i < column_count; i++)
  {
    if (attr[i] != NULL)
    {
      NdbDictionary::Column::Type column_type = attr[i]->getType();
      cout << "Column id: " << i << ", name: " << attr[i]->getColumn()->getName()
           << ", size: " << attr[i]->get_size_in_bytes()
           << ", type: " << column_type_to_string(attr[i]->getType());
      switch (column_type) {
      case NdbDictionary::Column::Char:
      case NdbDictionary::Column::Varchar:
      case NdbDictionary::Column::Longvarchar:
        {
          /* for char columns the actual string is printed */
          string str;
          get_string(attr[i], str);
          cout << ", stored string length: " << str.length()
               << ", value: " << str << endl;
        }
        break;
      case NdbDictionary::Column::Binary:
      case NdbDictionary::Column::Varbinary:
      case NdbDictionary::Column::Longvarbinary:
        {
          /* for binary columns the sum of all stored bytes is printed */
          const char* first;
          size_t count;
          get_byte_array(attr[i], first, count);
          int sum = 0;
          for (const char* byte = first; byte < first + count; byte++)
          {
            sum += (int)(*byte);
          }
          cout << ", stored bytes length: " << count
               << ", sum of byte array: " << sum << endl;
        }
        break;
      default:
        cout << ", column type \"" << column_type_to_string(attr[i]->getType())
             << "\" not covered by this example" << endl;
        break;
      }
    }
  }

  ndb.closeTransaction(transaction);
}

static void run_application(Ndb_cluster_connection &cluster_connection,
                            const char* database_name)
{
  /********************************************
   * Connect to database via NdbApi           *
   ********************************************/
  // Object representing the database
  Ndb ndb( &cluster_connection, database_name);
  if (ndb.init()) APIERROR(ndb.getNdbError());

  /*
   * Do different operations on database
   */
  do_insert(ndb);
  do_read(ndb);
  do_cleanup(ndb);
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    std::cout << "Arguments are <connect_string cluster> <database_name>.\n";
    exit(-1);
  }
  /* ndb_init must be called first */
  ndb_init();
  {
    /* connect to cluster */
    const char *connectstring = argv[1];
    Ndb_cluster_connection cluster_connection(connectstring);
    if (cluster_connection.connect(30 /* retries */,
                                   1  /* delay between retries */,
                                   0  /* verbose */))
    {
      std::cout << "Cluster management server was not ready within 30 secs.\n";
      exit(-1);
    }

    /* Connect and wait for the storage nodes */
    if (cluster_connection.wait_until_ready(30,10) < 0)
    {
      std::cout << "Cluster was not ready within 30 secs.\n";
      exit(-1);
    }

    /* run the application code */
    const char* dbname = argv[2];
    run_application(cluster_connection, dbname);
  }
  ndb_end(0);

  return 0;
}
