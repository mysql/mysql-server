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

#include "sql/ndb_ndbapi_util.h"

#include <string.h>           // memcpy

#include "m_string.h"         // my_strtok_r
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
  Print the diff between the extra metadata in the table
  with the data provided by the argument
*/

static void
print_diff(char* data, char* unpacked_data)
{
  DBUG_ENTER("print_diff");
  DBUG_PRINT("info", ("Diff found in extra metadata:\n"));

  // Tokenize both strings and compare each token
  char* data_save_ptr = data;
  char* unpacked_data_save_ptr = unpacked_data;
  char* data_token = my_strtok_r(data_save_ptr, "\n", &data_save_ptr);
  char* unpacked_data_token = my_strtok_r(unpacked_data_save_ptr, "\n",
                                       &unpacked_data_save_ptr);

  while(data_token && unpacked_data_token)
  {
    if(strcmp(data_token, unpacked_data_token))
    {
      DBUG_PRINT("info", ("\n+ %s\n- %s\n", data_token,
          unpacked_data_token));
    }
    data_token = my_strtok_r(data_save_ptr, "\n", &data_save_ptr);
    unpacked_data_token = my_strtok_r(unpacked_data_save_ptr, "\n",
                                   &unpacked_data_save_ptr);
  }

  while(data_token)
  {
    DBUG_PRINT("info", ("\n+ %s\n", data_token));
    data_token = my_strtok_r(data_save_ptr, "\n", &data_save_ptr);
  }

  while(unpacked_data_token)
  {
    DBUG_PRINT("info", ("\n- %s\n", unpacked_data_token));
    unpacked_data_token = my_strtok_r(unpacked_data_save_ptr, "\n",
                                   &unpacked_data_save_ptr);
  }

  DBUG_VOID_RETURN;
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

  if (data_length != unpacked_length)
  {
    DBUG_PRINT("info", ("Different length, data_length: %u, "
                        "unpacked_length: %u", (Uint32)data_length,
                        unpacked_length));
    print_diff((char*)data, (char*)unpacked_data);
    free(unpacked_data);
    // Different length, can't be equal
    DBUG_RETURN(1);
  }

  if (memcmp(data, unpacked_data, unpacked_length))
  {
    DBUG_PRINT("info", ("Different extra metadata for table %s",
                        ndbtab->getMysqlName()));
    print_diff((char*)data, (char*)unpacked_data);
    free(unpacked_data);
    DBUG_RETURN(1);
  }

  free(unpacked_data);
  DBUG_RETURN(0);
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
