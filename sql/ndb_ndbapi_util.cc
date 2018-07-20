/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_ndbapi_util.h"

#include <string.h>           // memcpy

#include "my_byteorder.h"


void ndb_pack_varchar(const NdbDictionary::Table *ndbtab, unsigned column_index,
                      char (&buf)[512], const char *str, size_t sz)
{
  // Get the column, cast to int to help compiler choose
  // the "const int" overload rather than "const char*"
  const NdbDictionary::Column* col =
      ndbtab->getColumn(static_cast<int>(column_index));

  assert(col->getLength() <= (int)sizeof(buf));

  switch (col->getArrayType())
  {
    case NdbDictionary::Column::ArrayTypeFixed:
      memcpy(buf, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeShortVar:
      *(uchar*)buf= (uchar)sz;
      memcpy(buf + 1, str, sz);
      break;
    case NdbDictionary::Column::ArrayTypeMediumVar:
      int2store(buf, (uint16)sz);
      memcpy(buf + 2, str, sz);
      break;
  }
}


Uint32
ndb_get_extra_metadata_version(const NdbDictionary::Table *ndbtab)
{
  DBUG_ENTER("ndb_get_extra_metadata_version");

  Uint32 version;
  void* unpacked_data;
  Uint32 unpacked_length;
  const int get_result =
      ndbtab->getExtraMetadata(version,
                               &unpacked_data, &unpacked_length);
  if (get_result != 0)
  {
    // Could not get extra metadata, return 0
    DBUG_RETURN(0);
  }

  free(unpacked_data);

  DBUG_RETURN(version);

}


bool
ndb_table_has_blobs(const NdbDictionary::Table *ndbtab)
{
  const int num_columns = ndbtab->getNoOfColumns();
  for (int i = 0; i < num_columns; i++)
  {
    const NdbDictionary::Column::Type column_type =
        ndbtab->getColumn(i)->getType();
    if (column_type == NdbDictionary::Column::Blob ||
        column_type == NdbDictionary::Column::Text)
    {
      // Found at least one blob column, the table has blobs
      return true;
    }
  }
  return false;
}


bool
ndb_table_has_hidden_pk(const NdbDictionary::Table *ndbtab)
{
  const char* hidden_pk_name = "$PK";
  if (ndbtab->getNoOfPrimaryKeys() == 1)
  {
    const NdbDictionary::Column* ndbcol = ndbtab->getColumn(hidden_pk_name);
    if (ndbcol &&
        ndbcol->getType() == NdbDictionary::Column::Bigunsigned &&
        ndbcol->getLength() == 1 &&
        ndbcol->getNullable() == false &&
        ndbcol->getPrimaryKey() == true &&
        ndbcol->getAutoIncrement() == true &&
        ndbcol->getDefaultValue() == nullptr)
    {
      return true;
    }
  }
  return false;
}



bool
ndb_table_has_tablespace(const NdbDictionary::Table* ndbtab)
{
  // NOTE! There is a slight ambiguity in the NdbDictionary::Table.
  // Depending on wheter it has been retrieved from NDB or created
  // by user as part of defining a new table in NDB, different methods
  // need to be used for determining if table has tablespace

  if (ndb_table_tablespace_name(ndbtab) != nullptr)
  {
    // Has tablespace
    return true;
  }

  if (ndbtab->getTablespace())
  {
    // Retrieved from NDB, the tablespace id and version
    // are avaliable in the table definition -> has tablespace.
    // NOTE! Fetching the name would require another roundtrip to NDB
    return true;
  }

  // Neither name or id of tablespace is set -> no tablespace
  return false;

}

const char*
ndb_table_tablespace_name(const NdbDictionary::Table* ndbtab)
{
  // NOTE! The getTablespaceName() returns zero length string
  // to indicate no tablespace
  const char* tablespace_name = ndbtab->getTablespaceName();
  if (strlen(tablespace_name) == 0)
  {
    // Just the zero length name, no tablespace name
    return nullptr;
  }
  return tablespace_name;
}


bool
ndb_dict_check_NDB_error(NdbDictionary::Dictionary* dict)
{
  return (dict->getNdbError().code != 0);
}
