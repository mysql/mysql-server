/*
Copyright (c) 2003, 2011, 2013, Oracle and/or its affiliates. All rights
reserved.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; version 2 of
the License.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
02110-1301  USA
*/
#include "field_iterator.h"

//Row_iterator Row_iterator::end() const
//{ return Row_iterator(); }

namespace mysql
{


bool is_null(unsigned char *bitmap, int index)
{
  unsigned char *byte= bitmap + (index / 8);
  unsigned bit= 1 << ((index) & 7);
  return ((*byte) & bit) != 0;
}


uint32_t extract_metadata(const Table_map_event *map, int col_no)
{
  int offset= 0;

  for (int i= 0; i < col_no; ++i)
  {
    unsigned int type= (unsigned int)map->columns[i] & 0xFF;
    offset += lookup_metadata_field_size((enum_field_types)type);
  }

  uint32_t metadata= 0;
  unsigned int type= (unsigned int)map->columns[col_no] & 0xFF;
  switch (lookup_metadata_field_size((enum_field_types)type))
  {
  case 1:
    metadata= map->metadata[offset];
  break;
  case 2:
    {
      unsigned int tmp= ((unsigned int)map->metadata[offset]) & 0xFF;
      metadata=  static_cast<uint32_t >(tmp);
      tmp= (((unsigned int)map->metadata[offset+1]) & 0xFF) << 8;
      metadata+= static_cast<uint32_t >(tmp);
    }
  break;
  }
  return metadata;
}

int lookup_metadata_field_size(enum_field_types field_type)
{
  switch (field_type)
  {
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_BLOB:
    case MYSQL_TYPE_GEOMETRY:
     return 1;
    case MYSQL_TYPE_BIT:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VAR_STRING:
     return 2;
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_SET:
    case MYSQL_TYPE_ENUM:
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_NEWDATE:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    default:
      return 0;
  }
}

} // end namespace mysql
