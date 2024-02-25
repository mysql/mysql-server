/*
   Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <iostream>
#include <cstdlib>
#include <string>
#include <iterator>

#include <NdbApi.hpp>

class BasicRead
{
  public:
    BasicRead(const char *connectstring)
      : m_connection(connectstring), m_ndb(&m_connection, "ndbapi_examples") {}

    bool init();
    bool do_read();

  private:
    Ndb_cluster_connection m_connection;
    Ndb m_ndb;

    struct BasicRow
    {
      int attr1, attr2;
    };

    inline bool on_error(const struct NdbError &error,
                         const std::string &explanation)
    {
      // prints error in format:
      // ERROR <NdbErrorCode>: <NdbError message>
      //    explanation what went wrong on higher level (in the example code)
      std::cout << "ERROR "<< error.code << ": " << error.message << std::endl;
      std::cout << explanation << std::endl;
      return false;
    }
};

/*
 * Before running this example ensure that you have created
 * the database and the table:
 * mysql> CREATE DATABASE ndbapi_examples;
 * mysql> CREATE TABLE ndbapi_examples.basic (
 *          ATTR1 INT NOT NULL PRIMARY KEY,
 *          ATTR2 INT NOT NULL
 *        ) ENGINE=NDB;
 *
 * Also make sure that your table contains a data to read
 * you can use ndb_ndbapi_basic_insert:
 * $ ./ndb_ndbapi_basic_insert <connectstring> 1 1
 *
 * or INSERT statement in mysql:
 * mysql> INSERT INTO ndbapi_examples.basic VALUES (1, 1);
 *
 */

int main(int argc, char **argv)
{
  if (argc != 2)
  {
    std::cout << "Usage: ndb_ndbapi_basic_read <connectstring>" << std::endl;
    return EXIT_FAILURE;
  }
  const char *connectstring = argv[1];

  ndb_init();
  {
    BasicRead example(connectstring);
    if (!example.init())
      return EXIT_FAILURE;

    // Let's verify reads
    if (!example.do_read()) return EXIT_FAILURE;
  }
  ndb_end(0);
  return EXIT_SUCCESS;
}

bool BasicRead::do_read()
{
  NdbDictionary::Dictionary *dict = m_ndb.getDictionary();
  const NdbDictionary::Table *table = dict->getTable("basic");
  if (table == nullptr)
    return on_error(dict->getNdbError(),
                    "Cannot access table 'ndbapi_examples.basic'");

  // Prepare record specification,
  // this will allow us later to access rows in the table
  // using our structure BasicRow
  NdbRecord* record;
  NdbDictionary::RecordSpecification record_spec[] = {
    { table->getColumn("ATTR1"), offsetof(BasicRow, attr1), 0, 0, 0 },
    { table->getColumn("ATTR2"), offsetof(BasicRow, attr2), 0, 0, 0 }
  };

  record = dict->createRecord(table,
                              record_spec,
                              std::size(record_spec),
                              sizeof(record_spec[0]));
  if (record == nullptr)
    return on_error(dict->getNdbError(), "Failed to create record");

  // All reads will be performed within single transaction
  NdbTransaction *transaction = m_ndb.startTransaction(table);
  if(transaction == nullptr)
    return on_error(m_ndb.getNdbError(), "Failed to start transaction");

  // Note the usage of NdbScanOperation instead of regular NdbOperation
  NdbScanOperation *operation = transaction->scanTable(record);
  if(operation == nullptr)
    return on_error(transaction->getNdbError(),
                    "Failed to start scanTable operation");

  // Note the usage of NoCommit flag, as we are only reading the tuples
  if (transaction->execute(NdbTransaction::NoCommit) != 0)
    return on_error(transaction->getNdbError(),
                    "Failed to execute transaction");

  const BasicRow *row_ptr;
  int rc;
  std::cout << "ATTR1" << "\t" << "ATTR2" << std::endl;
  // Loop over all read results to print them
  while ((rc = operation->nextResult(reinterpret_cast<const char **>(&row_ptr),
                                     true, false)) == 0)
    std::cout << row_ptr->attr1 << "\t" << row_ptr->attr2
              << std::endl;
  if (rc == -1)
    return on_error(transaction->getNdbError(), "Failed to read tuple");

  operation->close();
  m_ndb.closeTransaction(transaction);
  dict->releaseRecord(record);

  return true;
}

bool BasicRead::init()
{
  if (m_connection.connect() != 0)
  {
    std::cout << "Cannot connect to cluster management server" << std::endl;
    return false;
  }

  if (m_connection.wait_until_ready(30, 0) != 0)
  {
    std::cout << "Cluster was not ready within 30 secs" << std::endl;
    return false;
  }

  if (m_ndb.init() != 0)
    return on_error(m_ndb.getNdbError(), "Failed to initialize ndb object");

  return true;
}

