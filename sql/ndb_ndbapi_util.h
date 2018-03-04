/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_NDBAPI_UTIL_H
#define NDB_NDBAPI_UTIL_H

#include <stddef.h>

#include "storage/ndb/include/ndbapi/NdbBlob.hpp"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/include/ndbapi/NdbRecAttr.hpp"

union NdbValue
{
  const NdbRecAttr *rec;
  NdbBlob *blob;
  void *ptr;
};

char *ndb_pack_varchar(const NdbDictionary::Column *col,
                       char *buf, const char *str, int sz);

/**
   @brief ndb_get_extra_metadata_version, returns the version of the
          extra metadata attached to the table in NDB.
   @param ndbtab
   @return version of extra metadata or 0 if none
 */
Uint32 ndb_get_extra_metadata_version(const NdbDictionary::Table* ndbtab);


/**
 * @brief ndb_table_has_blobs, check if the NDB table has blobs
 * @param ndbtab
 * @return true if the table have blobs
 */
bool ndb_table_has_blobs(const NdbDictionary::Table* ndbtab);


/**
 * @brief ndb_table_has_hidden_pk, check if the NDB table has a hidden
 *        primary key(as created by ndbcluster to support having table
 *        without primary key in NDB)
 * @param ndbtab
 * @return true if the table has a hidden primary key
 */
bool ndb_table_has_hidden_pk(const NdbDictionary::Table* ndbtab);

#endif

