/*
   Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_METADATA_H
#define NDB_METADATA_H

#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

namespace dd {
  class Table;
}

/*
  Translate metadata from NDB dictionary to Data Dictionary(DD)
*/
class Ndb_metadata {
  bool m_compare_tablespace_id{true};
  const NdbDictionary::Table* m_ndbtab;

  Ndb_metadata(const NdbDictionary::Table* ndbtab) : m_ndbtab(ndbtab) {}

  /*

    @brief Return the partition expression

    @return string containing the partition expression for the table
  */
  dd::String_type partition_expression();

  /*
    @brief Create DD table definition

    @table_def[out]     DD table definition

    @return ?

  */
  bool create_table_def(dd::Table* table_def);

  /*
    @brief lookup tablespace_id from DD

    @thd                Thread context
    @table_def[out]     DD table definition
  */
  bool lookup_tablespace_id(class THD* thd, dd::Table* table_def);


  /*
    @brief Compare two DD table definitions

    @t1             table definition
    @t2             table definition

    @return true if the two table definitions are identical

    @note Only compares the properties which can be stored in NDB dictionary
  */
  bool compare_table_def(const dd::Table* t1, const dd::Table* t2);


  /*
    @brief Check partition information in DD table definition

    @t1             table definition

    @return true if the table's partition information is as expected

  */
  bool check_partition_info(const dd::Table* t1);

public:
  /*
    @brief Compare the NdbApi table with the DD table definition

    @thd                Thread context
    @ndbtab             NdbApi table
    @table_def          DD table definition

    @return true if the NdbApi table is identical to the DD table def.
  */
  static bool compare(class THD* thd,
                      const NdbDictionary::Table* ndbtab,
                      const dd::Table* table_def);
};

#endif
