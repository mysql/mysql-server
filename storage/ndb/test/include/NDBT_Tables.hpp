/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBT_TABLES_HPP
#define NDBT_TABLES_HPP


#include <NDBT.hpp>
#include <Ndb.hpp>
#include <NdbDictionary.hpp>
#include <NDBT_Table.hpp>

typedef int (* NDBT_CreateTableHook)(Ndb*, NdbDictionary::Table&, int when);

class NDBT_Tables {
public:

  static int createTable(Ndb* pNdb, const char* _name, bool _temp = false, 
			 bool existsOK = false, NDBT_CreateTableHook = 0);
  static int createAllTables(Ndb* pNdb, bool _temp, bool existsOK = false);
  static int createAllTables(Ndb* pNdb);

  static int dropAllTables(Ndb* pNdb);

  static int print(const char * name);
  static int printAll();
  
  static const NdbDictionary::Table* getTable(const char* _nam);
  static const NdbDictionary::Table* getTable(int _num);
  static int getNumTables();

private:
  static const NdbDictionary::Table* tableWithPkSize(const char* _nam, Uint32 pkSize);
};
#endif


