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

/**
  @file sql/histograms/histogram.cc
  Histogram base class (implementation).
*/

#include "sql/histograms/histogram.h"   // Histogram, Histogram_comparator

#include <new>

#include "binary_log_types.h"
#include "equi_height.h" // Equi_height<T>
#include "json_dom.h"    // Json_*
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_inttypes.h"
#include "my_sys.h"      // my_micro_time
#include "singleton.h"   // Singleton<T>
#include "sql_class.h"   // make_lex_string_root
#include "sql_servers.h"
#include "sql_string.h"  // String
#include "sql_time.h"    // my_time_compare
#include "tztime.h"      // my_tz_UTC

namespace histograms {

// Overloading the Histogram_comparator for various data types.
template <>
int Histogram_comparator::compare(const String &lhs, const String &rhs)
{
  // Ensure that both strings have the same character set/collation.
  DBUG_ASSERT(lhs.charset()->number == rhs.charset()->number);

  String *lhs_nonconst= const_cast<String*>(&lhs);
  String *rhs_nonconst= const_cast<String*>(&rhs);

  // Limit the number of characters we use when comparing strings.
  String lhs_substr= lhs_nonconst->substr(0, HISTOGRAM_MAX_COMPARE_LENGTH);
  String rhs_substr= rhs_nonconst->substr(0, HISTOGRAM_MAX_COMPARE_LENGTH);

  return sortcmp(&lhs_substr, &rhs_substr, lhs.charset());
}


template <>
int Histogram_comparator::compare(const MYSQL_TIME &lhs, const MYSQL_TIME &rhs)
{
  return my_time_compare(&lhs, &rhs);
}


template <>
int Histogram_comparator::compare(const my_decimal &lhs, const my_decimal &rhs)
{
  return my_decimal_cmp(&lhs, &rhs);
}


Histogram::Histogram(MEM_ROOT *mem_root, std::string db_name,
                     std::string tbl_name, std::string col_name,
                     enum_histogram_type type)
  :m_null_values_fraction(INVALID_NULL_VALUES_FRACTION), m_mem_root(mem_root),
  m_hist_type(type)
{
  make_lex_string_root(m_mem_root, &m_database_name, db_name.c_str(),
                       db_name.length(), false);

  make_lex_string_root(m_mem_root, &m_table_name, tbl_name.c_str(),
                       tbl_name.length(), false);

  make_lex_string_root(m_mem_root, &m_column_name, col_name.c_str(),
                       col_name.length(), false);
}


bool Histogram::histogram_to_json(Json_object *json_object) const
{
  // Get the current time in GMT timezone.
  MYSQL_TIME current_time;
  const ulonglong micro_time= my_micro_time();
  my_tz_UTC->gmt_sec_to_TIME(&current_time,
                             static_cast<my_time_t>(micro_time / 1000000));

  // last-updated
  const Json_datetime last_updated(current_time, MYSQL_TYPE_DATETIME);
  if (json_object->add_clone(last_updated_str(), &last_updated))
    return true;                              /* purecov: inspected */

  // histogram-type
  const Json_string histogram_type(histogram_type_to_str());
  if (json_object->add_clone(histogram_type_str(), &histogram_type))
    return true;                              /* purecov: inspected */

  // Fraction of NULL values.
  DBUG_ASSERT(get_null_values_fraction() >= 0.0);
  DBUG_ASSERT(get_null_values_fraction() <= 1.0);
  const Json_double null_values(get_null_values_fraction());
  if (json_object->add_clone(null_values_str(), &null_values))
    return true;                              /* purecov: inspected */
  return false;
}


double Histogram::get_null_values_fraction() const
{
  if (m_null_values_fraction != INVALID_NULL_VALUES_FRACTION)
  {
    DBUG_ASSERT(m_null_values_fraction >= 0.0);
    DBUG_ASSERT(m_null_values_fraction <= 1.0);
  }

  return m_null_values_fraction;
}


template <class T>
Histogram *build_histogram(MEM_ROOT *mem_root,
                           const value_map_type<T> &value_map,
                           ha_rows num_null_values, size_t num_buckets,
                           std::string db_name, std::string tbl_name,
                           std::string col_name)
{
  Histogram *histogram= nullptr;

  /*
    If the number of buckets specified is greater or equal to the number
    of distinct values, we create a Singleton histogram. Otherwise we create
    an equi-height histogram.
  */
  if (num_buckets >= value_map.size())
  {
    Singleton<T> *singleton=
      new(mem_root) Singleton<T>(mem_root, db_name, tbl_name, col_name);

    if (singleton == nullptr)
      return nullptr;

    if (singleton->build_histogram(value_map, num_null_values))
      return nullptr;                         /* purecov: inspected */

    histogram= singleton;
  }
  else
  {
    Equi_height<T> *equi_height=
      new(mem_root) Equi_height<T>(mem_root, db_name, tbl_name, col_name);

    if (equi_height == nullptr)
      return nullptr;

    if (equi_height->build_histogram(value_map, num_null_values, num_buckets))
      return nullptr;                         /* purecov: inspected */

    histogram= equi_height;
  }

  // We should not have a nullptr at this point.
  DBUG_ASSERT(histogram != nullptr);

  // Verify that we haven't created more buckets than requested.
  DBUG_ASSERT(histogram->get_num_buckets() <= num_buckets);

  // Check that the fraction of NULL values has been set properly.
  DBUG_ASSERT(histogram->get_null_values_fraction() >= 0.0);
  DBUG_ASSERT(histogram->get_null_values_fraction() <= 1.0);

  return histogram;
}

// Explicit template instantiations.
template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<double>&, ha_rows, size_t,
                std::string, std::string, std::string);

template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<String>&, ha_rows, size_t,
                std::string, std::string, std::string);

template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<ulonglong>&, ha_rows, size_t,
                std::string, std::string, std::string);

template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<longlong>&, ha_rows, size_t,
                std::string, std::string, std::string);

template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<MYSQL_TIME>&, ha_rows, size_t,
                std::string, std::string, std::string);

template Histogram *
build_histogram(MEM_ROOT *, const value_map_type<my_decimal>&, ha_rows, size_t,
                std::string, std::string, std::string);

} // namespace histograms
