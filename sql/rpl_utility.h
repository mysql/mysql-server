/* Copyright (C) 2006 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef RPL_UTILITY_H
#define RPL_UTILITY_H

#ifndef __cplusplus
#error "Don't include this C++ header file from a non-C++ file!"
#endif

#include "mysql_priv.h"

class Relay_log_info;


/**
  A table definition from the master.

  The responsibilities of this class is:
  - Extract and decode table definition data from the table map event
  - Check if table definition in table map is compatible with table
    definition on slave
 */

class table_def
{
public:
  /**
    Convenience declaration of the type of the field type data in a
    table map event.
  */
  typedef unsigned char field_type;

  /**
    Constructor.

    @param types Array of types
    @param size  Number of elements in array 'types'
    @param field_metadata Array of extra information about fields
    @param metadata_size Size of the field_metadata array
    @param null_bitmap The bitmap of fields that can be null
   */
  table_def(field_type *types, ulong size, uchar *field_metadata, 
      int metadata_size, uchar *null_bitmap, uint16 flags)
    : m_size(size), m_type(0), m_field_metadata_size(metadata_size),
    m_field_metadata(0), m_null_bits(0), m_flags(flags), m_memory(NULL)
  {
    m_memory= (uchar *)my_multi_malloc(MYF(MY_WME),
                                       &m_type, size,
                                       &m_field_metadata,
                                       size * sizeof(uint16),
                                       &m_null_bits, (size + 7) / 8,
                                       NULL);

    bzero(m_field_metadata, size * sizeof(uint16));

    if (m_type)
      memcpy(m_type, types, size);
    else
      m_size= 0;
    /*
      Extract the data from the table map into the field metadata array
      iff there is field metadata. The variable metadata_size will be
      0 if we are replicating from an older version server since no field
      metadata was written to the table map. This can also happen if 
      there were no fields in the master that needed extra metadata.
    */
    if (m_size && metadata_size)
    { 
      int index= 0;
      for (unsigned int i= 0; i < m_size; i++)
      {
        switch (m_type[i]) {
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_GEOMETRY:
        {
          /*
            These types store a single byte.
          */
          m_field_metadata[i]= field_metadata[index];
          index++;
          break;
        }
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_STRING:
        {
          uint16 x= field_metadata[index++] << 8U; // real_type
          x+= field_metadata[index++];            // pack or field length
          m_field_metadata[i]= x;
          break;
        }
        case MYSQL_TYPE_BIT:
        {
          uint16 x= field_metadata[index++]; 
          x = x + (field_metadata[index++] << 8U);
          m_field_metadata[i]= x;
          break;
        }
        case MYSQL_TYPE_VARCHAR:
        {
          /*
            These types store two bytes.
          */
          char *ptr= (char *)&field_metadata[index];
          m_field_metadata[i]= uint2korr(ptr);
          index= index + 2;
          break;
        }
        case MYSQL_TYPE_NEWDECIMAL:
        {
          uint16 x= field_metadata[index++] << 8U; // precision
          x+= field_metadata[index++];            // decimals
          m_field_metadata[i]= x;
          break;
        }
        default:
          m_field_metadata[i]= 0;
          break;
        }
      }
    }
    if (m_size && null_bitmap)
       memcpy(m_null_bits, null_bitmap, (m_size + 7) / 8);
  }

  ~table_def() {
    my_free(m_memory, MYF(0));
#ifndef DBUG_OFF
    m_type= 0;
    m_size= 0;
#endif
  }

  /**
    Return the number of fields there is type data for.

    @return The number of fields that there is type data for.
   */
  ulong size() const { return m_size; }


  /*
    Return a representation of the type data for one field.

    @param index Field index to return data for

    @return Will return a representation of the type data for field
    <code>index</code>. Currently, only the type identifier is
    returned.
   */
  field_type type(ulong index) const
  {
    DBUG_ASSERT(index < m_size);
    return m_type[index];
  }


  /*
    This function allows callers to get the extra field data from the
    table map for a given field. If there is no metadata for that field
    or there is no extra metadata at all, the function returns 0.

    The function returns the value for the field metadata for column at 
    position indicated by index. As mentioned, if the field was a type 
    that stores field metadata, that value is returned else zero (0) is 
    returned. This method is used in the unpack() methods of the 
    corresponding fields to properly extract the data from the binary log 
    in the event that the master's field is smaller than the slave.
  */
  uint16 field_metadata(uint index) const
  {
    DBUG_ASSERT(index < m_size);
    if (m_field_metadata_size)
      return m_field_metadata[index];
    else
      return 0;
  }

  /*
    This function returns whether the field on the master can be null.
    This value is derived from field->maybe_null().
  */
  my_bool maybe_null(uint index) const
  {
    DBUG_ASSERT(index < m_size);
    return ((m_null_bits[(index / 8)] & 
            (1 << (index % 8))) == (1 << (index %8)));
  }

  /*
    This function returns the field size in raw bytes based on the type
    and the encoded field data from the master's raw data. This method can 
    be used for situations where the slave needs to skip a column (e.g., 
    WL#3915) or needs to advance the pointer for the fields in the raw 
    data from the master to a specific column.
  */
  uint32 calc_field_size(uint col, uchar *master_data) const;

  /**
    Decide if the table definition is compatible with a table.

    Compare the definition with a table to see if it is compatible
    with it.

    A table definition is compatible with a table if:
      - the columns types of the table definition is a (not
        necessarily proper) prefix of the column type of the table, or
      - the other way around

    @param rli   Pointer to relay log info
    @param table Pointer to table to compare with.

    @retval 1  if the table definition is not compatible with @c table
    @retval 0  if the table definition is compatible with @c table
  */
#ifndef MYSQL_CLIENT
  int compatible_with(Relay_log_info const *rli, TABLE *table) const;
#endif

private:
  ulong m_size;           // Number of elements in the types array
  field_type *m_type;                     // Array of type descriptors
  uint m_field_metadata_size;
  uint16 *m_field_metadata;
  uchar *m_null_bits;
  uint16 m_flags;         // Table flags
  uchar *m_memory;
};


#ifndef MYSQL_CLIENT
/**
   Extend the normal table list with a few new fields needed by the
   slave thread, but nowhere else.
 */
struct RPL_TABLE_LIST
  : public TABLE_LIST
{
  bool m_tabledef_valid;
  table_def m_tabledef;
};


/* Anonymous namespace for template functions/classes */
namespace {

  /*
    Smart pointer that will automatically call my_afree (a macro) when
    the pointer goes out of scope.  This is used so that I do not have
    to remember to call my_afree() before each return.  There is no
    overhead associated with this, since all functions are inline.

    I (Matz) would prefer to use the free function as a template
    parameter, but that is not possible when the "function" is a
    macro.
  */
  template <class Obj>
  class auto_afree_ptr
  {
    Obj* m_ptr;
  public:
    auto_afree_ptr(Obj* ptr) : m_ptr(ptr) { }
    ~auto_afree_ptr() { if (m_ptr) my_afree(m_ptr); }
    void assign(Obj* ptr) {
      /* Only to be called if it hasn't been given a value before. */
      DBUG_ASSERT(m_ptr == NULL);
      m_ptr= ptr;
    }
    Obj* get() { return m_ptr; }
  };

}
#endif

// NB. number of printed bit values is limited to sizeof(buf) - 1
#define DBUG_PRINT_BITSET(N,FRM,BS)                \
  do {                                             \
    char buf[256];                                 \
    uint i;                                        \
    for (i = 0 ; i < min(sizeof(buf) - 1, (BS)->n_bits) ; i++) \
      buf[i] = bitmap_is_set((BS), i) ? '1' : '0'; \
    buf[i] = '\0';                                 \
    DBUG_PRINT((N), ((FRM), buf));                 \
  } while (0)

#endif /* RPL_UTILITY_H */
