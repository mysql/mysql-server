/*
   Copyright (c) 2017, 2020, Oracle and/or its affiliates. All rights reserved.

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

class THD;

/*
  Translate metadata from NDB dictionary to Data Dictionary(DD)
*/
class Ndb_metadata {
  bool m_compare_tablespace_id{true};
  const NdbDictionary::Table *m_ndbtab;

  Ndb_metadata(const NdbDictionary::Table *ndbtab) : m_ndbtab(ndbtab) {}

  /*

    @brief Return the partition expression

    @return String containing the partition expression for the table
  */
  dd::String_type partition_expression() const;

  /*
    @brief Create DD table definition

    @table_def[out]     DD table definition

    @return ?

  */
  bool create_table_def(dd::Table *table_def);

  /*
    @brief Lookup tablespace_id from DD

    @param thd                Thread context
    @param dd_table_def [out] DD table definition

    @return true if the id could be successfully looked up, false if not
  */
  bool lookup_tablespace_id(THD *thd, dd::Table *dd_table_def);

  /*
    @brief Compare two DD table definitions

    @param t1    DD table definition created from the NdbApi table
    @param t2    DD table definition sent by the caller

    @return true if the two table definitions are identical

    @note Only compares the properties which can be stored in NDB Dictionary
  */
  bool compare_table_def(const dd::Table *t1, const dd::Table *t2) const;

  /*
    @brief Check partition information in DD table definition

    @param dd_table_def  DD table definition

    @return true if the table's partition information is as expected
  */
  bool check_partition_info(const dd::Table *dd_table_def) const;

 public:
  /*
    @brief Compare the NdbApi table with the DD table definition

    @param thd           Thread context
    @param ndbtab        NdbApi table
    @param dd_table_def  DD table definition

    @return true if the NdbApi table is identical to the DD table def.
  */
  static bool compare(THD *thd, const NdbDictionary::Table *ndbtab,
                      const dd::Table *dd_table_def);

  /*
    @brief Compare the NdbApi table with the DD table definition to see if the
           number of indexes match

    @param dict          NDB Dictionary object
    @param ndbtab        NdbApi table
    @param dd_table_def  DD table definition

    @return true if the number of indexes in both definitions match, false if
            not
  */
  static bool compare_indexes(const NdbDictionary::Dictionary *dict,
                              const NdbDictionary::Table *ndbtab,
                              const dd::Table *dd_table_def);
};

#endif
