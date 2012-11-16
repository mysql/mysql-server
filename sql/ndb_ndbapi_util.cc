/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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


int
cmp_frm(const NdbDictionary::Table* ndbtab, const void* pack_data,
        size_t pack_length)
{
  DBUG_ENTER("cmp_frm");
  /*
    Compare the NDB tables FrmData with frm file blob in pack_data.
  */
  if ((pack_length != ndbtab->getFrmLength()) ||
      (memcmp(pack_data, ndbtab->getFrmData(), pack_length)))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


#ifndef MYSQL_SERVER
#define MYSQL_SERVER
#endif

#include <sql_class.h> // TABLE, Field etc.

/*
  This routine is shared by injector.  There is no common blobs buffer
  so the buffer and length are passed by reference.  Injector also
  passes a record pointer diff.
 */
int get_ndb_blobs_value(TABLE* table, NdbValue* value_array,
                        uchar*& buffer, uint& buffer_size,
                        my_ptrdiff_t ptrdiff)
{
  DBUG_ENTER("get_ndb_blobs_value");

  // Field has no field number so cannot use TABLE blob_field
  // Loop twice, first only counting total buffer size
  for (int loop= 0; loop <= 1; loop++)
  {
    uint32 offset= 0;
    for (uint i= 0; i < table->s->fields; i++)
    {
      Field *field= table->field[i];
      NdbValue value= value_array[i];
      if (! (field->flags & BLOB_FLAG))
        continue;
      if (value.blob == NULL)
      {
        DBUG_PRINT("info",("[%u] skipped", i));
        continue;
      }
      Field_blob *field_blob= (Field_blob *)field;
      NdbBlob *ndb_blob= value.blob;
      int isNull;
      if (ndb_blob->getNull(isNull) != 0)
        DBUG_RETURN(-1);
      if (isNull == 0) {
        Uint64 len64= 0;
        if (ndb_blob->getLength(len64) != 0)
          DBUG_RETURN(-1);
        // Align to Uint64
        uint32 size= Uint32(len64);
        if (size % 8 != 0)
          size+= 8 - size % 8;
        if (loop == 1)
        {
          uchar *buf= buffer + offset;
          uint32 len= 0xffffffff;  // Max uint32
          if (ndb_blob->readData(buf, len) != 0)
            DBUG_RETURN(-1);
          DBUG_PRINT("info", ("[%u] offset: %u  buf: 0x%lx  len=%u  [ptrdiff=%d]",
                              i, offset, (long) buf, len, (int)ptrdiff));
          DBUG_ASSERT(len == len64);
          // Ugly hack assumes only ptr needs to be changed
          field_blob->set_ptr_offset(ptrdiff, len, buf);
        }
        offset+= size;
      }
      else if (loop == 1) // undefined or null
      {
        // have to set length even in this case
        uchar *buf= buffer + offset; // or maybe NULL
        uint32 len= 0;
        field_blob->set_ptr_offset(ptrdiff, len, buf);
        DBUG_PRINT("info", ("[%u] isNull=%d", i, isNull));
        }
    }
    if (loop == 0 && offset > buffer_size)
    {
      my_free(buffer);
      buffer_size= 0;
      DBUG_PRINT("info", ("allocate blobs buffer size %u", offset));
      buffer= (uchar*) my_malloc(offset, MYF(MY_WME));
      if (buffer == NULL)
      {
        sql_print_error("ha_ndbcluster::get_ndb_blobs_value: "
                        "my_malloc(%u) failed", offset);
        DBUG_RETURN(-1);
      }
      buffer_size= offset;
    }
  }
  DBUG_RETURN(0);
}
