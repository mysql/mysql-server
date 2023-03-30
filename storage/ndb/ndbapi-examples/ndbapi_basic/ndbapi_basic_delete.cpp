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

#include <NdbApi.hpp>

class BasicDelete
{
  public:
    BasicDelete(const char * connectstring)
      : m_connection(connectstring), m_ndb(&m_connection, "ndbapi_examples") {}

    bool init();
    bool do_delete(long long);

  private:
    Ndb_cluster_connection m_connection;
    Ndb m_ndb;

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
 *
 *          ATTR1 INT NOT NULL PRIMARY KEY,
 *          ATTR2 INT NOT NULL
 *        ) ENGINE=NDB;
 *
 * Also make sure that your table contains a data to delete
 * you can use ndb_ndbapi_basic_insert:
 * $ ./ndb_ndbapi_basic_insert <connectstring> 1 1
 *
 * or INSERT statement in mysql:
 * mysql> INSERT INTO ndbapi_examples.basic VALUES (1, 1);
 *
 */

int main(int argc, char **argv)
{
  if (argc != 3)
  {
    std::cout << "Usage: ndb_ndbapi_basic_delete <connectstring> <key: int>"
              << std::endl;
    return EXIT_FAILURE;
  }

  const char *connectstring = argv[1];
  const long long key = std::strtoll(argv[2], nullptr, 10);

  ndb_init();
  {
    BasicDelete example(connectstring);
    if (!example.init()) return EXIT_FAILURE;

    // Let's verify delete
    if (example.do_delete(key))
      std::cout << "Done, check your database:\n"
                << "\t SELECT * FROM ndbapi_examples.basic;\n"
                << "\t or run the example: ndb_ndbapi_basic_read"
                << std::endl;
    else return EXIT_FAILURE;
  }
  ndb_end(0);

  return EXIT_SUCCESS;
}

bool BasicDelete::do_delete(long long key)
{
  const NdbDictionary::Dictionary *dict = m_ndb.getDictionary();
  const NdbDictionary::Table *table = dict->getTable("basic");

  if (table == nullptr)
    return on_error(dict->getNdbError(),
                    "Failed to access 'ndbapi_examples.basic'");

  // The delete operation will be performed within single transaction
  NdbTransaction *transaction = m_ndb.startTransaction(table);
  if(transaction == nullptr)
    return on_error(m_ndb.getNdbError(), "Failed to start transaction");

  NdbOperation *operation = transaction->getNdbOperation(table);
  if(operation == nullptr)
    return on_error(transaction->getNdbError(),
                    "Failed to start delete operation");

  operation->deleteTuple();
  operation->equal("ATTR1", key);

  if (transaction->execute(NdbTransaction::Commit) != 0)
    return on_error(transaction->getNdbError(),
        "Failed to execute transaction");

  m_ndb.closeTransaction(transaction);

  return true;
}

bool BasicDelete::init()
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

