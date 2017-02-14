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
  @file sql/histograms/singleton.cc
  Singleton histogram (implementation).
*/

#include "singleton.h"

#include <new>
#include <utility>          // std::make_pair

#include "binary_log_types.h"
#include "float_compare.h"
#include "json_dom.h"       // Json_*
#include "my_base.h"        // ha_rows
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql_time.h"

namespace histograms {

template <class T>
Singleton<T>::Singleton(MEM_ROOT *mem_root, std::string db_name,
                        std::string tbl_name, std::string col_name)
  :Histogram(mem_root, db_name, tbl_name, col_name,
             enum_histogram_type::SINGLETON),
   m_buckets(Histogram_comparator(), value_map_allocator<T>(mem_root))
{}


template <class T>
bool Singleton<T>::build_histogram(const value_map_type<T> &value_map,
                                   ha_rows num_null_values)
{
  // Clear any existing data.
  m_buckets.clear();
  m_null_values_fraction= INVALID_NULL_VALUES_FRACTION;

  // Get total frequency count.
  ha_rows num_non_null_values= 0;
  for (const auto& node : value_map)
    num_non_null_values+= node.second;

  // No values, nothing to do.
  if (num_non_null_values == 0)
  {
    if (num_null_values > 0)
      m_null_values_fraction= 1.0;
    else
      m_null_values_fraction= 0.0;

    return false;
  }

  const ha_rows total_count= num_null_values + num_non_null_values;

  // Set the fractions of NULL values.
  m_null_values_fraction= num_null_values / static_cast<double>(total_count);

  // Create buckets with relative frequency, and not absolute frequency.
  double cumulative_frequency= 0.0;

  /*
    Since we are using a std::map with Memroot_allocator, we are forced to wrap
    the following section in a try-catch. The Memroot_allocator will throw an
    exception of class std::bad_alloc when it runs out of memory.
  */
  try
  {
    for (const auto& node : value_map)
    {
      const double frequency= node.second / static_cast<double>(total_count);
      cumulative_frequency+= frequency;

      m_buckets.emplace(node.first, cumulative_frequency);
    }
  }
  catch (const std::bad_alloc&)
  {
    // Out of memory.
    return true;
  }

  DBUG_ASSERT(Float_compare::almost_equal(
    cumulative_frequency + get_null_values_fraction(), 1.0));

  return false;
}


template <class T>
bool Singleton<T>::histogram_to_json(Json_object *json_object) const
{
  /*
    Call the base class implementation first. This will add the properties that
    are common among different histogram types, such as "last-updated" and
    "histogram-type".
  */
  if (Histogram::histogram_to_json(json_object))
    return true;                              /* purecov: inspected */

  // Add the Singleton buckets.
  Json_array json_buckets;
  for (const auto& bucket : m_buckets)
  {
    Json_array json_bucket;
    if (create_json_bucket(bucket, &json_bucket))
      return true;                            /* purecov: inspected */
    if (json_buckets.append_clone(&json_bucket))
      return true;                            /* purecov: inspected */
  }

  if (json_object->add_clone(buckets_str(), &json_buckets))
    return true;                              /* purecov: inspected */
  return false;
}


template <class T>
bool Singleton<T>::create_json_bucket(const std::pair<T, double> &bucket,
                                      Json_array *json_bucket)
{
  // Value
  if (add_value_json_bucket(bucket.first, json_bucket))
    return true;                              /* purecov: inspected */

  // Cumulative frequency
  const Json_double frequency(bucket.second);
  if (json_bucket->append_clone(&frequency))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<double>::add_value_json_bucket(const double &value,
                                              Json_array *json_bucket)
{
  const Json_double json_value(value);
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<String>::add_value_json_bucket(const String &value,
                                              Json_array *json_bucket)
{
  const Json_opaque json_value(enum_field_types::MYSQL_TYPE_STRING, value.ptr(),
                               value.length());
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<ulonglong>::add_value_json_bucket(const ulonglong &value,
                                                 Json_array *json_bucket)
{
  const Json_uint json_value(value);
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<longlong>::add_value_json_bucket(const longlong &value,
                                                Json_array *json_bucket)
{
  const Json_int json_value(value);
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<MYSQL_TIME>::add_value_json_bucket(const MYSQL_TIME &value,
                                                  Json_array *json_bucket)
{
  enum_field_types field_type;
  switch (value.time_type)
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

  const Json_datetime json_value(value, field_type);
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <>
bool Singleton<my_decimal>::add_value_json_bucket(const my_decimal &value,
                                                  Json_array *json_bucket)
{
  const Json_decimal json_value(value);
  if (json_bucket->append_clone(&json_value))
    return true;                              /* purecov: inspected */
  return false;
}


template <class T>
std::string Singleton<T>::histogram_type_to_str() const
{
  return singleton_str();
}


// Explicit template instantiations.
template class Singleton<double>;
template class Singleton<String>;
template class Singleton<ulonglong>;
template class Singleton<longlong>;
template class Singleton<MYSQL_TIME>;
template class Singleton<my_decimal>;

} // namespace histograms
