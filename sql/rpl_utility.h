/* Copyright (c) 2006, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_UTILITY_H
#define RPL_UTILITY_H

#ifndef __cplusplus
#error "Don't include this C++ header file from a non-C++ file!"
#endif

#include <sys/types.h>
#include <unordered_map>

#include "binary_log_types.h"   // enum_field_types
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_macros.h"
#include "mysql/udf_registration_types.h"
#include "sql/psi_memory_key.h"

#ifdef MYSQL_SERVER
#include <memory>

#include "map_helpers.h"
#include "prealloced_array.h"   // Prealloced_array
#include "sql/handler.h"
#include "sql/table.h"          // TABLE_LIST

class Log_event;
class Relay_log_info;
class THD;

/**
   Hash table used when applying row events on the slave and there is
   no index on the slave's table.
 */

typedef struct hash_row_pos_st
{
  /** 
      Points at the position where the row starts in the
      event buffer (ie, area in memory before unpacking takes
      place).
  */
  const uchar *bi_start;
  const uchar *bi_ends;

} HASH_ROW_POS;

struct HASH_ROW_ENTRY;

struct hash_slave_rows_free_entry
{
  void operator() (HASH_ROW_ENTRY *entry) const;
};


/**
   Internal structure that acts as a preamble for HASH_ROW_POS
   in memory structure. 
   
   Allocation is done in Hash_slave_rows::make_entry as part of 
   the entry allocation.
 */
typedef struct hash_row_preamble_st
{
  hash_row_preamble_st()= default;
  /*
    The actual key.
   */
  uint hash_value;

  /**  
    The search state used to iterate over multiple entries for a
    given key.
   */
  malloc_unordered_multimap
    <uint,
     std::unique_ptr<HASH_ROW_ENTRY,
                     hash_slave_rows_free_entry>>::const_iterator
       search_state;

  /**  
    Wether this search_state is usable or not.
   */
  bool is_search_state_inited;

} HASH_ROW_PREAMBLE;

struct HASH_ROW_ENTRY
{
  HASH_ROW_PREAMBLE *preamble;
  HASH_ROW_POS *positions;
};

class Hash_slave_rows 
{
public:

  /**
     Allocates an empty entry to be added to the hash table.
     It should be called before calling member function @c put.

     @returns NULL if a problem occured, a valid pointer otherwise.
  */
  HASH_ROW_ENTRY* make_entry();

  /**
     Allocates an entry to be added to the hash table. It should be
     called before calling member function @c put.
     
     @param bi_start the position to where in the rows buffer the
                     before image begins.
     @param bi_ends  the position to where in the rows buffer the
                     before image ends.
     @returns NULL if a problem occured, a valid pointer otherwise.
   */
  HASH_ROW_ENTRY* make_entry(const uchar *bi_start, const uchar *bi_ends);


  /**
     Puts data into the hash table. It calculates the key taking 
     the data on @c TABLE::record as the input for hash computation.

     @param table   The table holding the buffer used to calculate the
                    key, ie, table->record[0].
     @param cols    The read_set bitmap signaling which columns are used.
     @param entry   The entry with the values to store.

     @returns true if something went wrong, false otherwise.
   */
  bool put(TABLE* table, MY_BITMAP *cols, HASH_ROW_ENTRY* entry);

  /**
     Gets the entry, from the hash table, that matches the data in
     table->record[0] and signaled using cols.
     
     @param table   The table holding the buffer containing data used to
                    make the entry lookup.
     @param cols    Bitmap signaling which columns, from
                    table->record[0], should be used.

     @returns a pointer that will hold a reference to the entry
              found. If the entry is not found then NULL shall be
              returned.
   */
  HASH_ROW_ENTRY* get(TABLE *table, MY_BITMAP *cols);

  /**
     Gets the entry that stands next to the one pointed to by
     *entry. Before calling this member function, the entry that one
     uses as parameter must have: 1. been obtained through get() or
     next() invocations; and 2. must have not been used before in a
     next() operation.

     @param[in,out] entry contains a pointer to an entry that we can
                          use to search for another adjacent entry
                          (ie, that shares the same key).

     @returns true if something went wrong, false otherwise. In the
              case that this entry was already used in a next()
              operation this member function returns true and does not
              update the pointer.
   */
  bool next(HASH_ROW_ENTRY** entry);

  /**
     Deletes the entry pointed by entry. It also frees memory used
     holding entry contents. This is the way to release memeory 
     used for entry, freeing it explicitly with my_free will cause
     undefined behavior.

     @param entry  Pointer to the entry to be deleted.
     @returns true if something went wrong, false otherwise.
   */
  bool del(HASH_ROW_ENTRY* entry);

  /**
     Initializes the hash table.

     @returns true if something went wrong, false otherwise.
   */
  bool init(void);

  /**
     De-initializes the hash table.

     @returns true if something went wrong, false otherwise.
   */
  bool deinit(void);

  /**
     Checks if the hash table is empty or not.

     @returns true if the hash table has zero entries, false otherwise.
   */
  bool is_empty(void);

  /**
     Returns the number of entries in the hash table.

     @returns the number of entries in the hash table.
   */
  int size();
  
private:

  /**
     The hashtable itself.
   */
  malloc_unordered_multimap
    <uint,
     std::unique_ptr<HASH_ROW_ENTRY, hash_slave_rows_free_entry>> m_hash
       {key_memory_HASH_ROW_ENTRY};

  /**
     Auxiliary and internal method used to create an hash key, based on
     the data in table->record[0] buffer and signaled as used in cols.

     @param table  The table that is being scanned
     @param cols   The read_set bitmap signaling which columns are used.

     @returns the hash key created.
   */
  uint make_hash_key(TABLE *table, MY_BITMAP* cols);
};

#endif

/**
  A table definition from the master.

  The responsibilities of this class is:
  - Extract and decode table definition data from the table map event
  - Check if table definition in table map is compatible with table
    definition on slave
  - expose the type information so that it can be used when encoding
    or decoding row event data.
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
    @param flags Table flags
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
    Returns internal binlog type code for one field,
    without translation to real types.
  */
  enum_field_types binlog_type(ulong index) const
  {
    return static_cast<enum_field_types>(m_type[index]);
  }

  /// Return the number of JSON columns in this table.
  int json_column_count() const
  {
    // Cache in member field to make successive calls faster.
    if (m_json_column_count == -1)
    {
      int c= 0;
      for (uint i= 0; i < size(); i++)
        if (type(i) == MYSQL_TYPE_JSON)
          c++;
      m_json_column_count= c;
    }
    return m_json_column_count;
  }

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
    enum_field_types source_type= binlog_type(index);
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
  bool maybe_null(uint index) const
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

#ifdef MYSQL_SERVER
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

    @param thd   Current thread
    @param rli   Pointer to relay log info
    @param table Pointer to table to compare with.

    @param[out] conv_table_var Pointer to temporary table for holding
    conversion table.

    @retval 1  if the table definition is not compatible with @c table
    @retval 0  if the table definition is compatible with @c table
  */
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
  mutable int m_json_column_count;   // Number of JSON columns
};


#ifdef MYSQL_SERVER
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


class Deferred_log_events
{
private:
  Prealloced_array<Log_event*, 32> m_array;

public:
  Deferred_log_events();
  ~Deferred_log_events();
  /* queue for exection at Query-log-event time prior the Query */
  int add(Log_event *ev);
  bool is_empty();
  bool execute(Relay_log_info *rli);
  void rewind();
};

#endif

// NB. number of printed bit values is limited to sizeof(buf) - 1
#define DBUG_PRINT_BITSET(N,FRM,BS)                \
  do {                                             \
    char buf[256];                                 \
    uint i;                                        \
    for (i = 0 ; i < MY_MIN(sizeof(buf) - 1, (BS)->n_bits) ; i++) \
      buf[i] = bitmap_is_set((BS), i) ? '1' : '0'; \
    buf[i] = '\0';                                 \
    DBUG_PRINT((N), ((FRM), buf));                 \
  } while (0)

#endif /* RPL_UTILITY_H */

