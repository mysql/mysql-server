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
  @file sql/histograms/equi_height_bucket.cc
  Equi-height bucket (implementation).
*/

#include "binary_log_types.h"
#include "equi_height_bucket.h"  // equi_height::Bucket
#include "histogram.h"           // Histogram_comparator
#include "json_dom.h"            // Json_*
#include "my_base.h"             // ha_rows
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql_time.h"

namespace histograms {
namespace equi_height {

template<typename T>
Bucket<T>::Bucket(T lower, T upper, double freq, ha_rows num_distinct)
  :m_lower_inclusive(lower), m_upper_inclusive(upper),
  m_cumulative_frequency(freq), m_num_distinct(num_distinct)
{
  DBUG_ASSERT(m_cumulative_frequency >= 0.0);
  DBUG_ASSERT(m_cumulative_frequency <= 1.0);
  DBUG_ASSERT(m_num_distinct >= 1);
  DBUG_ASSERT(histograms::Histogram_comparator::compare(lower, upper) <= 0);
}


template<typename T>
bool Bucket<T>::bucket_to_json(Json_array *json_array) const
{
  // Lower and upper inclusive value.
  if (add_values_json_bucket(get_lower_inclusive(), get_upper_inclusive(),
                             json_array))
    return true;                              /* purecov: inspected */

  // Cumulative frequency.
  const Json_double frequency(get_cumulative_frequency());
  if (json_array->append_clone(&frequency))
    return true;                              /* purecov: inspected */

  // Number of distinct values.
  const Json_uint num_distinct(get_num_distinct());
  if (json_array->append_clone(&num_distinct))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<double>::add_values_json_bucket(const double &lower_value,
                                            const double &upper_value,
                                            Json_array *json_array)
{
  const Json_double json_lower_value(lower_value);
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_double json_upper_value(upper_value);
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<String>::add_values_json_bucket(const String &lower_value,
                                            const String &upper_value,
                                            Json_array *json_array)
{
  const Json_opaque json_lower_value(enum_field_types::MYSQL_TYPE_STRING,
                                     lower_value.ptr(), lower_value.length());
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_opaque json_upper_value(enum_field_types::MYSQL_TYPE_STRING,
                                     upper_value.ptr(), upper_value.length());
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<ulonglong>::add_values_json_bucket(const ulonglong &lower_value,
                                               const ulonglong &upper_value,
                                               Json_array *json_array)
{
  const Json_uint json_lower_value(lower_value);
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_uint json_upper_value(upper_value);
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<longlong>::add_values_json_bucket(const longlong &lower_value,
                                              const longlong &upper_value,
                                              Json_array *json_array)
{
  const Json_int json_lower_value(lower_value);
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_int json_upper_value(upper_value);
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<MYSQL_TIME>::add_values_json_bucket(const MYSQL_TIME &lower_value,
                                                const MYSQL_TIME &upper_value,
                                                Json_array *json_array)
{
  DBUG_ASSERT(lower_value.time_type == upper_value.time_type);

  enum_field_types field_type;
  switch (lower_value.time_type)
  {
    case MYSQL_TIMESTAMP_DATE:
      field_type= MYSQL_TYPE_DATE;
      break;
    case MYSQL_TIMESTAMP_DATETIME:
      field_type= MYSQL_TYPE_DATETIME;
      break;
    case MYSQL_TIMESTAMP_TIME:
      field_type= MYSQL_TYPE_TIME;
      break;
    default:
      /* purecov: begin deadcode */
      DBUG_ASSERT(false);
      return true;
      /* purecov: end */
  }

  const Json_datetime json_lower_value(lower_value, field_type);
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_datetime json_upper_value(upper_value, field_type);
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}


template<>
bool Bucket<my_decimal>::add_values_json_bucket(const my_decimal &lower_value,
                                                const my_decimal &upper_value,
                                                Json_array *json_array)
{
  const Json_decimal json_lower_value(lower_value);
  if (json_array->append_clone(&json_lower_value))
    return true;                              /* purecov: inspected */

  const Json_decimal json_upper_value(upper_value);
  if (json_array->append_clone(&json_upper_value))
    return true;                              /* purecov: inspected */
  return false;
}

// Explicit template instantiations.
template class Bucket<double>;
template class Bucket<String>;
template class Bucket<ulonglong>;
template class Bucket<longlong>;
template class Bucket<MYSQL_TIME>;
template class Bucket<my_decimal>;

} // namespace equi_height
} // namespace bucket
