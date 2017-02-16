/*
   Copyright (c) 2006, 2010, Oracle and/or its affiliates.

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

#include "sql_priv.h"
#include "m_string.h"                           /* bzero, memcpy */
#ifdef MYSQL_SERVER
#include "table.h"                              /* TABLE_LIST */
#endif
#include "mysql_com.h"

class Relay_log_info;
class Log_event;

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
    Constructor.

    @param types Array of types, each stored as a byte
    @param size  Number of elements in array 'types'
    @param field_metadata Array of extra information about fields
    @param metadata_size Size of the field_metadata array
    @param null_bitmap The bitmap of fields that can be null
   */
  table_def(unsigned char *types, ulong size, uchar *field_metadata,
            int metadata_size, uchar *null_bitmap, uint16 flags);

  ~table_def();

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
  enum_field_types type(ulong index) const
  {
    DBUG_ASSERT(index < m_size);
    /*
      If the source type is MYSQL_TYPE_STRING, it can in reality be
      either MYSQL_TYPE_STRING, MYSQL_TYPE_ENUM, or MYSQL_TYPE_SET, so
      we might need to modify the type to get the real type.
    */
    enum_field_types source_type= static_cast<enum_field_types>(m_type[index]);
    uint16 source_metadata= m_field_metadata[index];
    switch (source_type)
    {
    case MYSQL_TYPE_STRING:
    {
      int real_type= source_metadata >> 8;
      if (real_type == MYSQL_TYPE_ENUM || real_type == MYSQL_TYPE_SET)
        source_type= static_cast<enum_field_types>(real_type);
      break;
    }

    /*
      This type has not been used since before row-based replication,
      so we can safely assume that it really is MYSQL_TYPE_NEWDATE.
    */
    case MYSQL_TYPE_DATE:
      source_type= MYSQL_TYPE_NEWDATE;
      break;

    default:
      /* Do nothing */
      break;
    }

    return source_type;
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
      - The columns types of the table definition is a (not
        necessarily proper) prefix of the column type of the table.

      - The other way around.

      - Each column on the master that also exists on the slave can be
        converted according to the current settings of @c
        SLAVE_TYPE_CONVERSIONS.

    @param thd
    @param rli   Pointer to relay log info
    @param table Pointer to table to compare with.

    @param[out] tmp_table_var Pointer to temporary table for holding
    conversion table.

    @retval 1  if the table definition is not compatible with @c table
    @retval 0  if the table definition is compatible with @c table
  */
#ifndef MYSQL_CLIENT
  bool compatible_with(THD *thd, Relay_log_info *rli, TABLE *table,
                      TABLE **conv_table_var) const;

  /**
   Create a virtual in-memory temporary table structure.

   The table structure has records and field array so that a row can
   be unpacked into the record for further processing.

   In the virtual table, each field that requires conversion will
   have a non-NULL value, while fields that do not require
   conversion will have a NULL value.

   Some information that is missing in the events, such as the
   character set for string types, are taken from the table that the
   field is going to be pushed into, so the target table that the data
   eventually need to be pushed into need to be supplied.

   @param thd Thread to allocate memory from.
   @param rli Relay log info structure, for error reporting.
   @param target_table Target table for fields.

   @return A pointer to a temporary table with memory allocated in the
   thread's memroot, NULL if the table could not be created
   */
  TABLE *create_conversion_table(THD *thd, Relay_log_info *rli, TABLE *target_table) const;
#endif


private:
  ulong m_size;           // Number of elements in the types array
  unsigned char *m_type;  // Array of type descriptors
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
  TABLE *m_conv_table;
};


/* Anonymous namespace for template functions/classes */
CPP_UNNAMED_NS_START

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

CPP_UNNAMED_NS_END

class Deferred_log_events
{
private:
  DYNAMIC_ARRAY array;
  Log_event *last_added;

public:
  Deferred_log_events(Relay_log_info *rli);
  ~Deferred_log_events();
  /* queue for exection at Query-log-event time prior the Query */
  int add(Log_event *ev);
  bool is_empty();
  bool execute(Relay_log_info *rli);
  void rewind();
  bool is_last(Log_event *ev) { return ev == last_added; };
};

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
