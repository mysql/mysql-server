/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_DD_FK_H
#define NDB_DD_FK_H

namespace dd {
class Table;
class Foreign_key;
}  // namespace dd

#include "storage/ndb/include/ndbapi/NdbApi.hpp"

/**
  Extract the definition of the given foreign key from NDB and
  update the DD's foreign key object with that.

  @param[out] fk_def          The DD foreign key object.
  @param dd_child_table       The DD child table object on which the
                              foreign key exists.
  @param ndb_fk               The NDB foreign key object
  @param ndb_child_table      The NDB child table object on which the
                              foreign key exists.
  @param ndb_parent_table     The NDB parent table object which is referenced
                              by the foreign key constraint.
  @param parent_schema_name   The parent table schema name.

  @return true        On success.
  @return false       On failure
*/
bool ndb_dd_fk_set_values_from_ndb(dd::Foreign_key *fk_def,
                                   const dd::Table *dd_child_table,
                                   const NdbDictionary::ForeignKey &ndb_fk,
                                   const NdbDictionary::Table *ndb_child_table,
                                   const NdbDictionary::Table *ndb_parent_table,
                                   const char *parent_schema_name);

#endif
