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
  /* Some constants for the maximum sizes of keys and attributes
   * in various cases
   */
  static constexpr Uint32 MaxRowBytes = NDB_MAX_TUPLE_SIZE_IN_WORDS * 4;
  static constexpr Uint32 MaxKeyBytes = NDB_MAX_KEYSIZE_IN_WORDS * 4;
  static constexpr Uint32 MinKeyBytes = 4; // Single Unsigned key

  static constexpr Uint32 MaxVarTypeKeyBytes = MaxKeyBytes - 2; // 2 length bytes

  static constexpr Uint32 MaxKeyMaxAttrBytes = MaxRowBytes - MaxKeyBytes;
  static constexpr Uint32 MaxKeyMaxVarTypeAttrBytes = MaxKeyMaxAttrBytes - 2;

  static constexpr Uint32 MinKeyMaxAttrBytes = MaxRowBytes - MinKeyBytes;
  static constexpr Uint32 MinKeyMaxVarTypeAttrBytes = MinKeyMaxAttrBytes - 2;

  static constexpr Uint32 UniqueIndexOverheadBytes = 4; // For FragId

  // Note that since we'll put an unique index on this...it can't be bigger
  // than MaxKeyBytes
  static constexpr Uint32 MaxKeyMaxVarTypeAttrBytesIndex =
               ((MaxKeyMaxVarTypeAttrBytes <= MaxKeyBytes) ?
                MaxKeyMaxVarTypeAttrBytes : MaxKeyBytes) -
                UniqueIndexOverheadBytes;

  /* Hugo requires 2 unsigned int columns somewhere in the table
   * and these also counts towards #attributes relation
   */
  static constexpr Uint32 HugoOverheadBytes = 2 * (4 + 4);

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


