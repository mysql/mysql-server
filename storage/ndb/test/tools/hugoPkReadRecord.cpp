/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.
   Use is subject to license terms.

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

#include <ndb_global.h>

#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <NDBT_Tables.hpp>
#include <getarg.h>
#include <NDBT.hpp>
#include <Ndb.hpp>
#include <NdbDictionary.hpp>

//extern NdbOut g_info;

int main(int argc, const char** argv)
{
  ndb_init();
  int _row = 0;
  int _hex = 0;
  int _primaryKey = 0;
  const char* _tableName = NULL;

  struct getargs args[] = {
    { "row", 'r', 
      arg_integer, &_row, "The row number", "row" },
    { "primarykey", 'p', 
      arg_integer, &_primaryKey, "The primary key", "primarykey" },
    { "hex", 'h', 
      arg_flag, &_hex, "Print hex", "hex" }
  };

  int num_args = sizeof(args) / sizeof(args[0]);
  int optind = 0, i;

  if(getarg(args, num_args, argc, argv, &optind) || argv[optind] == NULL) {
    arg_printusage(args, num_args, argv[0], "table name\n");
    return NDBT_WRONGARGS;
  }
  // Check if table name is supplied
  if (argv[optind] != NULL) 
    _tableName = argv[optind];


  const NdbDictionary::Table* table = NDBT_Tables::getTable(_tableName);
  //  const NDBT_Attribute* attribute = table->getAttribute(_column);

  g_info << "Table " << _tableName << endl
	 << "Row: " << _row << ", PrimaryKey: " << _primaryKey
	 << endl;

  Ndb_cluster_connection con;
  if(con.connect(12, 5, 1) != 0)
  {
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  Ndb* ndb = new Ndb(&con, "TEST_DB");
  if (ndb->init() == 0 && ndb->waitUntilReady(30) == 0)
  {
    NdbConnection* conn = ndb->startTransaction();
    if (conn == NULL)
    {
      g_info << "ERROR: " << ndb->getNdbError() << endl;
      delete ndb;
      return -1;
    }
    NdbOperation* op = conn->getNdbOperation(_tableName);
    if (op == NULL)
    {
      g_info << "ERROR: " << conn->getNdbError() << endl;
      delete ndb;
      return -1;
    }
    op->readTuple();
    NdbRecAttr** data = new NdbRecAttr*[table->getNoOfColumns()];
    for (i = 0; i < table->getNoOfColumns(); i++)
    {
      const NdbDictionary::Column* c = table->getColumn(i);
      if (c->getPrimaryKey())
      {
	op->equal(c->getName(), _primaryKey);
	data[i] = op->getValue(c->getName(), NULL);
      }
      else
      {
	data[i] = op->getValue(c->getName(), NULL);
      }      
    }
    if (conn->execute(Commit) == 0)
    {
      // Print column names
      for (i = 0; i < table->getNoOfColumns(); i++)
      {
	const NdbDictionary::Column* c = table->getColumn(i);
	
	g_info 
	  << c->getName()
	  << "[" << c->getType() << "]   ";	  
      }
      g_info << endl;

      if (_hex)
      {
	g_info << hex;
      }
      for (i = 0; i < table->getNoOfColumns(); i++)
      {
	NdbRecAttr* a = data[i];
	ndbout << (* a) << " ";
      } // for   
      g_info << endl;   
    } // if (conn
    else
    {
      g_info << "Failed to commit read transaction... " 
	     << conn->getNdbError()
	     << ", commitStatus = " << conn->commitStatus()
	     << endl;
    }

    delete[] data;
    
    ndb->closeTransaction(conn);
  } // if (ndb.init
  else
  {
    g_info << "ERROR: Unable to connect to NDB, " 
	   << ndb->getNdbError() << endl;
  }
  delete ndb;

  return 0;
}
