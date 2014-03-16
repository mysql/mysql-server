/* Copyright (c) 2003-2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */

#ifndef NDBT_TABLES_HPP
#define NDBT_TABLES_HPP


#include <NDBT.hpp>
#include <Ndb.hpp>
#include <NdbDictionary.hpp>
#include <NDBT_Table.hpp>

typedef int (* NDBT_CreateTableHook)(Ndb*, NdbDictionary::Table&, int when,
                                    void* arg);

class NDBT_Tables {
public:

  static int createTable(Ndb* pNdb, const char* _name, bool _temp = false, 
			 bool existsOK = false, NDBT_CreateTableHook = 0,
                         void* arg = 0);
  static int createAllTables(Ndb* pNdb, bool _temp, bool existsOK = false);
  static int createAllTables(Ndb* pNdb);

  static int dropAllTables(Ndb* pNdb);

  static int print(const char * name);
  static int printAll();
  
  static const NdbDictionary::Table* getTable(const char* _nam);
  static const NdbDictionary::Table* getTable(int _num);
  static int getNumTables();

  static const char** getIndexes(const char* table);

  static int create_default_tablespace(Ndb* pNdb);

private:
  static const NdbDictionary::Table* tableWithPkSize(const char* _nam, Uint32 pkSize);
};
#endif


