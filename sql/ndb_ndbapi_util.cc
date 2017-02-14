/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ndb_ndbapi_util.h"

#include <string.h> // memcpy

#include "my_byteorder.h"


/*
  helper function to pack a ndb varchar
*/
char *ndb_pack_varchar(const NdbDictionary::Column *col, char *buf,
                       const char *str, int sz)
{
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
      int2store(buf, sz);
      memcpy(buf + 2, str, sz);
      break;
  }
  return buf;
}


/*
  Compare the extra metadata in the table with the data provided
  by the arguments
*/

int
cmp_unpacked_frm(const NdbDictionary::Table* ndbtab, const void* data,
                 size_t data_length)
{
  DBUG_ENTER("cmp_unpacked_frm");


  // Get the extra metadata of the table(it's returned unpacked)
  Uint32 version;
  void* unpacked_data;
  Uint32 unpacked_length;
  const int get_result =
      ndbtab->getExtraMetadata(version,
                               &unpacked_data, &unpacked_length);
  if (get_result != 0)
  {
    // Could not get extra metadata, assume not equal
    DBUG_RETURN(1);
  }

  DBUG_ASSERT(version == 1); // Only extra metadata with frm now

  if (data_length != unpacked_length)
  {
    free(unpacked_data);
    // Different length, can't be equal
    DBUG_RETURN(1);
  }

  if (memcmp(data, unpacked_data, unpacked_length))
  {
    DBUG_PRINT("info", ("Different extra metadata for table %s",
                        ndbtab->getMysqlName()));
    DBUG_DUMP("frm", (uchar*) unpacked_data,
                      unpacked_length);

    free(unpacked_data);
    DBUG_RETURN(1);
  }

  free(unpacked_data);
  DBUG_RETURN(0);
}
