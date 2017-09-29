/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/dd/impl/types/table_stat_impl.h" // Table_stat_impl

#include <ostream>
#include <string>

#include "my_sys.h"
#include "mysqld_error.h"
#include "sql/dd/impl/raw/object_keys.h"
#include "sql/dd/impl/raw/raw_record.h"    // Raw_record
#include "sql/dd/impl/tables/table_stats.h" // Table_stats
#include "sql/dd/impl/transaction_impl.h"  // Open_dictionary_tables_ctx

namespace dd {
class Object_key;
}  // namespace dd


using dd::tables::Table_stats;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Table_stat implementation.
///////////////////////////////////////////////////////////////////////////

const Entity_object_table &Table_stat::OBJECT_TABLE()
{
  return Table_stats::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Table_stat::TYPE()
{
  static Table_stat_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Table_stat_impl implementation.
///////////////////////////////////////////////////////////////////////////

bool Table_stat_impl::validate() const
{
  if (schema_name().empty() || table_name().empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Table_stat_impl::OBJECT_TABLE().name().c_str(),
             "schema name or table name not supplied.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_stat_impl::restore_attributes(const Raw_record &r)
{
  m_schema_name=    r.read_str(Table_stats::FIELD_SCHEMA_NAME);
  m_table_name=     r.read_str(Table_stats::FIELD_TABLE_NAME);

  m_table_rows=     r.read_int(Table_stats::FIELD_TABLE_ROWS);
  m_avg_row_length= r.read_int(Table_stats::FIELD_AVG_ROW_LENGTH);
  m_data_length=    r.read_int(Table_stats::FIELD_DATA_LENGTH);
  m_max_data_length=r.read_int(Table_stats::FIELD_MAX_DATA_LENGTH);
  m_index_length=   r.read_int(Table_stats::FIELD_INDEX_LENGTH);
  m_data_free=      r.read_int(Table_stats::FIELD_DATA_FREE);
  m_auto_increment= r.read_int(Table_stats::FIELD_AUTO_INCREMENT);
  m_checksum=       r.read_int(Table_stats::FIELD_CHECKSUM);
  m_update_time=    r.read_int(Table_stats::FIELD_UPDATE_TIME);
  m_check_time=     r.read_int(Table_stats::FIELD_CHECK_TIME);
  m_cached_time=    r.read_int(Table_stats::FIELD_CACHED_TIME);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_stat_impl::store_attributes(Raw_record *r)
{
  return r->store(Table_stats::FIELD_SCHEMA_NAME, m_schema_name) ||
           r->store(Table_stats::FIELD_TABLE_NAME, m_table_name) ||
           r->store(Table_stats::FIELD_TABLE_ROWS, m_table_rows) ||
           r->store(Table_stats::FIELD_AVG_ROW_LENGTH, m_avg_row_length) ||
           r->store(Table_stats::FIELD_DATA_LENGTH, m_data_length) ||
           r->store(Table_stats::FIELD_MAX_DATA_LENGTH, m_max_data_length) ||
           r->store(Table_stats::FIELD_INDEX_LENGTH, m_index_length) ||
           r->store(Table_stats::FIELD_DATA_FREE, m_data_free) ||
           r->store(Table_stats::FIELD_AUTO_INCREMENT, m_auto_increment,
                    m_auto_increment == (ulonglong) -1) ||
           r->store(Table_stats::FIELD_CHECKSUM,
                    m_checksum, m_checksum == 0) ||
           r->store(Table_stats::FIELD_UPDATE_TIME,
                    m_update_time,
                    m_update_time == 0) ||
           r->store(Table_stats::FIELD_CHECK_TIME,
                    m_check_time,
                    m_check_time == 0) ||
           r->store(Table_stats::FIELD_CACHED_TIME,
                    m_cached_time);
}

///////////////////////////////////////////////////////////////////////////

void Table_stat_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
  ss
    << "TABLE STAT OBJECT: { "
    << "m_schema_name: " <<  m_schema_name << "; "
    << "m_table_name: " <<  m_table_name << "; "
    << "m_table_rows: " <<  m_table_rows << "; "
    << "m_avg_row_length: " <<  m_avg_row_length << "; "
    << "m_data_length: " <<  m_data_length << "; "
    << "m_max_data_length: " <<  m_max_data_length << "; "
    << "m_index_length: " <<  m_index_length << "; "
    << "m_data_free: " <<  m_data_free << "; "
    << "m_auto_increment: " <<  m_auto_increment << "; "
    << "m_checksum: " <<  m_checksum << "; "
    << "m_update_time: " <<  m_update_time << "; "
    << "m_check_time: " <<  m_check_time << "; "
    << "m_cached_time: " <<  m_cached_time;

  ss << " }";
  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Object_key *Table_stat_impl::create_primary_key() const
{
  return Table_stats::create_object_key(m_schema_name, m_table_name);
}

///////////////////////////////////////////////////////////////////////////

bool Table_stat_impl::has_new_primary_key() const
{
  /*
    There is no OBJECT_ID for Table_stat/Index_stat DD object.
    So deciding if a object exists or not is not possible based
    on just schema and table name, we would need to add a new
    numeric object id for the same. Adding this new column to
    these objects would be un-necessary and serve only purpose to
    update or insert the objects. Additionaly would cost little
    more disk space too.

    These DD objects are only updated. I.e., if row exists we
    just update or else insert a new row. Returning 'false' here
    enables expected behavior. Even if we have added a new
    numeric primary key, that would require to first fetch the DD
    object and then call store(). Instead returning false here
    will end-up doing the same, we would first try to find the
    object and then insert if not found.
  */

  return false;
}

///////////////////////////////////////////////////////////////////////////
// Table_stat_type implementation.
///////////////////////////////////////////////////////////////////////////

void Table_stat_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  /**
    The requirement is that we should be able to update
    Table_stats and Index_stats DD tables even when someone holds
    global read lock, when we execute ANALYZE TABLE.
  */
  otx->mark_ignore_global_read_lock();
  otx->add_table<Table_stats>();
}

///////////////////////////////////////////////////////////////////////////

}
