/*
   Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "ndb_fk_util.h"

#include "sql/ndb_table_guard.h"
#include "sql/ndb_thd.h"
#include "sql/ndb_thd_ndb.h"


/**
  Split the given internal ndb object name into usable format.
  The object maybe a table, index or a foreign key.

  @param[out]  dst      Prealloced buffer to copy the names.
                        On a successful return, this will point to the database
                        name of the table.
  @param       src      Pointer to the buffer having the internal name.
  @param       index    Denotes whether the ndb object is an index. True if
                        the object is an index, false if not.

  @return               On success, the actual name of the table, index or
                        the FK is returned.
*/
const char *
fk_split_name(char dst[], const char * src, bool index)
{
  DBUG_PRINT("info", ("fk_split_name: %s index=%d", src, index));

  /**
   * Split a fully qualified (ndb) name into db and name
   *
   * Store result in dst
   */
  char * dstptr = dst;
  const char * save = src;
  while (src[0] != 0 && src[0] != '/')
  {
    * dstptr = * src;
    dstptr++;
    src++;
  }

  if (src[0] == 0)
  {
    /**
     * No '/' found
     *  set db to ''
     *  and return pointer to name
     *
     * This is for compability with create_fk/drop_fk tools...
     */
    dst[0] = 0;
    strcpy(dst + 1, save);
    DBUG_PRINT("info", ("fk_split_name: %s,%s", dst, dst + 1));
    return dst + 1;
  }

  assert(src[0] == '/');
  src++;
  * dstptr = 0;
  dstptr++;

  // Skip over catalog (not implemented)
  while (src[0] != '/')
  {
    src++;
  }

  assert(src[0] == '/');
  src++;

  /**
   * Indexes contains an extra /
   */
  if (index)
  {
    while (src[0] != '/')
    {
      src++;
    }
    assert(src[0] == '/');
    src++;
  }
  strcpy(dstptr, src);
  DBUG_PRINT("info", ("fk_split_name: %s,%s", dst, dstptr));
  return dstptr;
}


/**
  Fetch all tables that are referenced by the given table as a part of a
  foreign key relationship.

  @param       thd                The THD object.
  @param       schema_name        Schema name of the table.
  @param       table_name         Name of the table.
  @param[out]  referenced_tables  Set of pair of strings holding the database
                                  name and the table name of the referenced
                                  tables.

  @return      true               On success
               false              On failure
*/
bool
fetch_referenced_tables_from_ndb_dictionary(
    THD* thd, const char* schema_name, const char* table_name,
    std::set<std::pair<std::string, std::string>> &referenced_tables)
{
  DBUG_ENTER("fetch_referenced_tables_from_ndb_dictionary");
  Thd_ndb* thd_ndb = get_thd_ndb(thd);
  Ndb* ndb = thd_ndb->ndb;

  // save db
  Ndb_db_guard db_guard(ndb);
  if (ndb->setDatabaseName(schema_name) != 0)
  {
    DBUG_PRINT("error", ("Error setting database '%s'. Error : %s",
                         schema_name, ndb->getNdbError().message));
    DBUG_RETURN(false);
  }

  NdbDictionary::Dictionary *dict= ndb->getDictionary();
  Ndb_table_guard tab_guard(dict, table_name);
  const NdbDictionary::Table *table= tab_guard.get_table();
  if (table == NULL)
  {
    DBUG_PRINT("error", ("Unable to load table '%s.%s' from ndb. Error : %s",
                         schema_name, table_name,
                         dict->getNdbError().message));
    DBUG_RETURN(false);
  }

  NdbDictionary::Dictionary::List obj_list;
  if (dict->listDependentObjects(obj_list, *table) != 0)
  {
    DBUG_PRINT("error", ("Unable to list dependents of '%s.%s'. Error : %s",
                         schema_name, table_name,
                         dict->getNdbError().message));
    DBUG_RETURN(false);
  }
  DBUG_PRINT("info", ("found %u dependent objects", obj_list.count));

  for (unsigned i = 0; i < obj_list.count; i++)
  {
    const NdbDictionary::Dictionary::List::Element &element=
        obj_list.elements[i];
    if (element.type != NdbDictionary::Object::ForeignKey)
    {
      DBUG_PRINT("info", ("skip non-FK '%s' type %d",
                          element.name, element.type));
      continue;
    }

    NdbDictionary::ForeignKey fk;
    if (dict->getForeignKey(fk, element.name) != 0)
    {
      DBUG_PRINT("error", ("Unable to fetch foreign key '%s'. Error : %s",
                           element.name,
                           dict->getNdbError().message));
      DBUG_RETURN(false);
    }

    char parent_db[FN_LEN + 1];
    const char *parent_name = fk_split_name(parent_db, fk.getParentTable());

    if (strcmp(parent_db, schema_name) == 0 &&
        strcmp(parent_name, table_name) == 0)
    {
      // Given table is the parent of this FK. Skip adding.
      DBUG_PRINT("info", ("skip FK '%s'", element.name));
      continue;
    }

    DBUG_PRINT("info", ("Adding referenced tables '%s.%s'",
                        parent_db, parent_name));
    referenced_tables.insert(
        std::pair<std::string, std::string>(parent_db, parent_name));
  }

  DBUG_RETURN(true);
}
