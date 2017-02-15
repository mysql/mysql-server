/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_INFO_SCHEMA_INCLUDED
#define DD_INFO_SCHEMA_INCLUDED

#include <sys/types.h>
#include <string>

#include "dd/object_id.h"
#include "dd/string_type.h"                 // dd::String_type
#include "handler.h"                        // ha_statistics
#include "my_inttypes.h"
#include "sql_string.h"                     // String

class THD;
struct TABLE_LIST;

namespace dd {
namespace info_schema {


/**
  Get dynamic table statistics of a table and store them into
  mysql.table_stats.

  @param thd   Thread.
  @param table TABLE_LIST pointing to table info.

  @returns false on success.
           true on failure.
*/
bool update_table_stats(THD *thd, TABLE_LIST *table);


/**
  Get dynamic index statistics of a table and store them into
  mysql.index_stats.

  @param thd   Thread.
  @param table TABLE_LIST pointing to table info.

  @returns false on success.
           true on failure.
*/
bool update_index_stats(THD *thd, TABLE_LIST *table);


/**
  If the db is 'information_schema' then convert 'db' to
  lowercase and 'table_name' to upper case. Mainly because all
  information schema tables are stored in upper case in server.

  @param db          Database name
  @param table_name  Table name.

  @returns true if the conversion was done.
           false if not.
*/
bool convert_table_name_case(char *db, char *table_name);


// Statistics that are cached.
enum class enum_statistics_type
{
  TABLE_ROWS,
  TABLE_AVG_ROW_LENGTH,
  DATA_LENGTH,
  MAX_DATA_LENGTH,
  INDEX_LENGTH,
  DATA_FREE,
  AUTO_INCREMENT,
  CHECKSUM,
  TABLE_UPDATE_TIME,
  CHECK_TIME,
  INDEX_COLUMN_CARDINALITY
};


// This enum is used to set the SESSION variable 'information_schema_stats'
enum class enum_stats
{
  LATEST= 0,
  CACHED
};


/**
  The class hold dynamic table statistics for a table.
  This cache is used by internal UDF's defined for the purpose
  of INFORMATION_SCHEMA queries which retrieve dynamic table
  statistics. The class caches statistics for just one table.

  Overall aim of introducing this cache is to avoid making
  multiple calls to same SE API to retrieve the statistics.
*/

class Statistics_cache
{
public:
  Statistics_cache()
    : m_checksum(0)
  {}

  /**
    @brief
    Check if the stats are cached for given db.table_name.

    @param db_name          - Schema name.
    @param table_name       - Table name.

    @return true if stats are cached, else false.
  */
  bool is_stat_cached(const String &db_name, const String &table_name)
  { return (m_key == form_key(db_name, table_name)); }


  /**
    @brief
    Store the statistics form the given handler

    @param db_name          - Schema name.
    @param table_name       - Table name.
    @param file             - Handler object for the table.

    @return void
  */
  void cache_stats(const String &db_name,
                   const String &table_name,
                   handler *file)
  {
    m_stats= file->stats;
    m_checksum= file->checksum();
    m_error.clear();
    set_stat_cached(db_name, table_name);
  }


  /**
    @brief
    Store the statistics

    @param db_name          - Schema name.
    @param table_name       - Table name.
    @param stats            - ha_statistics of the table.

    @return void
  */
  void cache_stats(const String &db_name,
                   const String &table_name,
                   ha_statistics &stats)
  {
    m_stats= stats;
    m_checksum= 0;
    m_error.clear();
    set_stat_cached(db_name, table_name);
  }


  /**
    @brief
    Read dynamic table/index statistics from SE by opening the user table
    provided OR by reading cached statistics from SELECT_LEX.

    @param thd                     - Current thread.
    @param schema_name_ptr         - Schema name of table.
    @param table_name_ptr          - Table name of which we need stats.
    @param index_name_ptr          - Index name of which we need stats.
    @param index_ordinal_position  - Ordinal position of index in table.
    @param column_ordinal_position - Ordinal position of column in table.
    @param engine_name_ptr         - Engine of the table.
    @param se_private_id           - se_private_id of the table.
    @param stype                   - Enum specifying the stat we are
                                     interested to read.

    @return ulonglong representing value for the status being read.
  */
  ulonglong read_stat(THD *thd,
                      const String &schema_name_ptr,
                      const String &table_name_ptr,
                      const String &index_name_ptr,
                      uint index_ordinal_position,
                      uint column_ordinal_position,
                      const String &engine_name_ptr,
                      dd::Object_id se_private_id,
                      enum_statistics_type stype);


  // Fetch table stats. Invokes the above method.
  ulonglong read_stat(THD *thd,
                      const String &schema_name_ptr,
                      const String &table_name_ptr,
                      const String &engine_name_ptr,
                      dd::Object_id se_private_id,
                      enum_statistics_type stype)
  {
    const String tmp;
    return read_stat(thd,
                     schema_name_ptr,
                     table_name_ptr,
                     tmp, 0, 0,
                     engine_name_ptr,
                     se_private_id,
                     stype);
  }


  // Invalidate the cache.
  void invalidate_cache(void)
  {
    m_key.clear();
    m_error.clear();
  }


  // Get error string. Its empty if a error is not reported.
  inline String_type error()
  { return m_error; }


private:

  /**
    Read dynamic table/index statistics from SE API's OR by reading
    cached statistics from SELECT_LEX.

    @param thd                     - Current thread.
    @param schema_name_ptr         - Schema name of table.
    @param table_name_ptr          - Table name of which we need stats.
    @param index_name_ptr          - Index name of which we need stats.
    @param index_ordinal_position  - Ordinal position of index in table.
    @param column_ordinal_position - Ordinal position of column in table.
    @param engine_name_ptr         - Engine of the table.
    @param se_private_id           - se_private_id of the table.
    @param stype                   - Enum specifying the stat we are
                                     interested to read.

    @return ulonglong representing value for the status being read.
  */
  ulonglong read_stat_from_SE(THD *thd,
                              const String &schema_name_ptr,
                              const String &table_name_ptr,
                              const String &index_name_ptr,
                              uint index_ordinal_position,
                              uint column_ordinal_position,
                              const String &engine_name_ptr,
                              dd::Object_id se_private_id,
                              enum_statistics_type stype);


  /**
    Read dynamic table/index statistics by opening the table OR by reading
    cached statistics from SELECT_LEX.

    @param thd                     - Current thread.
    @param schema_name_ptr         - Schema name of table.
    @param table_name_ptr          - Table name of which we need stats.
    @param index_name_ptr          - Index name of which we need stats.
    @param column_ordinal_position - Ordinal position of column in table.
    @param stype                   - Enum specifying the stat we are
                                     interested to read.

    @return ulonglong representing value for the status being read.
  */
  ulonglong read_stat_by_open_table(THD *thd,
                                    const String &schema_name_ptr,
                                    const String &table_name_ptr,
                                    const String &index_name_ptr,
                                    uint column_ordinal_position,
                                    enum_statistics_type stype);


  /**
    Mark the cache as valid for a given table. This creates a key for the
    cache element. We store just a single table statistics in this cache.

    @param db_name     Database name.
    @param table_name  Table name.

    @returns void.
  */
  void set_stat_cached(const String &db_name, const String &table_name)
  { m_key= form_key(db_name, table_name); }


  /**
    Build a key representating the table for which stats are cached.

    @param db_name     Database name.
    @param table_name  Table name.

    @returns String_type representing the key.
  */
  String_type form_key(const String &db_name,
                       const String &table_name)
  {
    return String_type(db_name.ptr()) + "." +
           String_type(table_name.ptr());
  }


  /**
    Return statistics of the a given type.

    @param stat   ha_statistics for the current cached table.
    @param stype  Type of statistics requested.

    @returns ulonglong statistics value.
  */
  ulonglong get_stat(ha_statistics &stat, enum_statistics_type stype);
  inline ulonglong get_stat(enum_statistics_type stype)
  { return get_stat(m_stats, stype); }


  /**
    Check if we have seen a error.

    @param db_name     Database name.
    @param table_name  Table name.

    @returns true if there is error reported.
             false if not.
  */
  inline bool check_error_for_key(const String &db_name,
                                  const String &table_name)
  {
    if (is_stat_cached(db_name, table_name) && !m_error.empty())
      return true;

    return false;
  }


private:

  // The cache key
  String_type m_key; // Format '<db_name>.<table_name>'

  // Error found when reading statistics.
  String_type m_error;


public:

  // Cached statistics.
  ha_statistics m_stats;

  // Table checksum value retrieved from SE.
  ulonglong m_checksum;
};


} // namespace info_schema
} // namespace dd

#endif // DD_INFO_SCHEMA_INCLUDED
