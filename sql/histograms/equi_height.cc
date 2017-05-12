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
  @file sql/histograms/equi_height.cc
  Equi-height histogram (implementation).
*/

#include "sql/histograms/equi_height.h"

#include <cmath>            // std::lround
#include <iterator>
#include <new>

#include "equi_height_bucket.h"
#include "float_compare.h"
#include "json_dom.h"       // Json_*
#include "memroot_allocator.h"
#include "my_base.h"        // ha_rows
#include "my_dbug.h"
#include "my_decimal.h"
#include "my_inttypes.h"
#include "sql_string.h"

namespace histograms {

template <class T>
Equi_height<T>::Equi_height(MEM_ROOT *mem_root, std::string db_name,
                            std::string tbl_name, std::string col_name)
  :Histogram(mem_root, db_name, tbl_name, col_name,
             enum_histogram_type::EQUI_HEIGHT),
  m_buckets(Memroot_allocator<equi_height::Bucket<T> >(mem_root))
{}


/*
  This function will build an equi-height histogram. The algorithm works like
  the following:

  - If the number of buckets specified is euqal to or greater than the number
    of distinct values, a single bucket is created for each value.

  - If we have more distinct values than the number of buckets, we calculate a
    theadshold T for each bucket. The threshold T for bucket number N (counting
    from 1) is calculated as;

      num_non_null_values
      -------------------  * N = T
         num_buckets;

    When adding a value to a bucket, we check if including the next bucket will
    make the accumulated frequency become larger than the threshold. If that is
    the case, check whether only including the current value is closer to the
    threshold than including the next value as well. We select the option that
    is closest to the threshold.
*/
template <class T>
bool Equi_height<T>::build_histogram(const value_map_type<T> &value_map,
                                     ha_rows num_null_values,
                                     size_t num_buckets)
{
  DBUG_ASSERT(num_buckets > 0);
  if (num_buckets < 1)
    return true;                              /* purecov: inspected */

  // Clear any existing data.
  m_buckets.clear();
  m_null_values_fraction= INVALID_NULL_VALUES_FRACTION;

  // Get total frequency count.
  ha_rows num_non_null_values= 0;
  for (const auto& node : value_map)
    num_non_null_values+= node.second;

  // No non-null values, nothing to do.
  if (num_non_null_values == 0)
  {
    if (num_null_values > 0)
      m_null_values_fraction= 1.0;
    else
      m_null_values_fraction= 0.0;

    return false;
  }

  DBUG_ASSERT(num_buckets > 0);

  // Set the fraction of NULL values.
  const ha_rows total_count= num_null_values + num_non_null_values;
  m_null_values_fraction= num_null_values / static_cast<double>(total_count);

  /*
    Divide the frequencies into evenly-ish spaced buckets, and set the bucket
    threadshold accordingly.
  */
  const double avg_bucket_size=
    num_non_null_values / static_cast<double>(num_buckets);
  double current_threadshold= avg_bucket_size;

  ha_rows cumulative_sum= 0;
  ha_rows sum= 0;
  ha_rows num_distinct= 0;
  size_t values_remaining= value_map.size();
  auto freq_it= value_map.begin();
  const T *lowest_value= &freq_it->first;

  for (; freq_it != value_map.end(); ++freq_it)
  {
    sum+= freq_it->second;
    cumulative_sum+= freq_it->second;
    num_distinct++;
    values_remaining--;
    auto next= std::next(freq_it);

    if (next != value_map.end())
    {
      /*
        Check if including the next bucket will make the frequency become
        larger than the threadshold. If that is the case, check whether only
        including the current value is closer to the threadshold than
        including the next value as well.
      */
      if ((cumulative_sum + next->second) > current_threadshold)
      {
        double current_distance=
          std::abs(current_threadshold - cumulative_sum);
        double next_distance=
          std::abs(current_threadshold - (cumulative_sum + next->second));

        if (current_distance >= next_distance)
          continue;
      }
      else if (values_remaining >= (num_buckets - m_buckets.size()))
      {
        /*
          Ensure that we don't end up with more buckets than the maximum
          specified.
        */
        continue;
      }
    }

    // Create a bucket.
    double cumulative_frequency=
      cumulative_sum / static_cast<double>(total_count);

    equi_height::Bucket<T> bucket(*lowest_value, freq_it->first,
                                  cumulative_frequency, num_distinct);

    /*
      Since we are using a std::vector with Memroot_allocator, we are forced to
      wrap the following section in a try-catch. The Memroot_allocator will throw
      an exception of class std::bad_alloc when it runs out of memory.
    */
    try
    {
      m_buckets.push_back(bucket);
    }
    catch (const std::bad_alloc&)
    {
      // Out of memory.
      return true;
    }
    /*
      In debug, check that the lower value actually is less than or equal to
      the upper value.
    */
    DBUG_ASSERT(
      value_map.key_comp().compare(bucket.get_lower_inclusive(),
                                   bucket.get_upper_inclusive()) <= 0);

    /*
      We also check that the lower inclusive value of the current bucket is
      greater than the upper inclusive value of the previous bucket.
    */
    if (m_buckets.size() > 1)
    {
      DBUG_ASSERT(value_map.key_comp().compare(
        std::prev(m_buckets.end(), 2)->get_upper_inclusive(),
        bucket.get_lower_inclusive()) < 0);
    }

    sum= 0;
    num_distinct= 0;
    current_threadshold= avg_bucket_size * (m_buckets.size() + 1);
    if (next != value_map.end())
      lowest_value= &next->first;
  }

  DBUG_ASSERT(m_buckets.size() <= num_buckets);

  if (!m_buckets.empty())
  {
    DBUG_ASSERT(
      Float_compare::almost_equal(m_buckets.back().get_cumulative_frequency() +
                                  get_null_values_fraction(),
                                  1.0));
  }

  return false;
}


template <class T>
bool Equi_height<T>::histogram_to_json(Json_object *json_object) const
{
  /*
    Call the base class implementation first. This will add the properties that
    are common among different histogram types, such as "last-updated" and
    "histogram-type".
  */
  if (Histogram::histogram_to_json(json_object))
    return true;                              /* purecov: inspected */

  // Add the equi-height buckets.
  Json_array buckets;
  for (const auto& bucket : m_buckets)
  {
    Json_array json_bucket;
    if (bucket.bucket_to_json(&json_bucket))
      return true;                            /* purecov: inspected */
    if (buckets.append_clone(&json_bucket))
      return true;                            /* purecov: inspected */
  }

  if (json_object->add_clone(buckets_str(), &buckets))
    return true;                              /* purecov: inspected */
  return false;
}


template <class T>
std::string Equi_height<T>::histogram_type_to_str() const
{
  return equi_height_str();
}

// Explicit template instantiations.
template class Equi_height<double>;
template class Equi_height<String>;
template class Equi_height<ulonglong>;
template class Equi_height<longlong>;
template class Equi_height<MYSQL_TIME>;
template class Equi_height<my_decimal>;

} // namespace histograms
