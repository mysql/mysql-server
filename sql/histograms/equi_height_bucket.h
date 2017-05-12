#ifndef HISTOGRAMS_EQUI_HEIGHT_BUCKET_INCLUDED
#define HISTOGRAMS_EQUI_HEIGHT_BUCKET_INCLUDED

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
  @file sql/histograms/equi_height_bucket.h
  Equi-height bucket.

  This file defines the class representing an equi-height bucket. A bucket holds
  four different values:
    - Lower inclusive value.
    - Upper inclusive value.
    - The cumulative frequency (between 0.0 and 1.0).
    - Number of distinct values in this bucket.
*/

#include "my_base.h"      // ha_rows
#include "my_decimal.h"
#include "sql_string.h"

class Json_array;

namespace histograms {
namespace equi_height {

/**
  Equi-height bucket.
*/
template <class T>
class Bucket
{
private:
  /// Lower inclusive value contained in this bucket.
  const T m_lower_inclusive;

  /// Upper inclusive value contained in this bucket.
  const T m_upper_inclusive;

  /// The cumulative frequency. 0.0 <= m_cumulative_frequency <= 1.0
  const double m_cumulative_frequency;

  /// Number of distinct values in this bucket. m_num_distinct >= 1
  const ha_rows m_num_distinct;

  /**
    Add values to a JSON bucket

    This function adds the lower and upper inclusive value to the supplied
    JSON array. The lower value is added first.

    @param      lower_value the lower inclusive value to add
    @param      upper_value the upper inclusive value to add
    @param[out] json_array  a JSON array where the bucket data is stored

    @return     true on error, false otherwise.
  */
  static bool add_values_json_bucket(const T &lower_value,
                                     const T &upper_value,
                                     Json_array *json_array);
public:
  /**
    Equi-height bucket constructor.

    Does nothing more than setting the member variables.

    @param lower         lower inclusive value
    @param upper         upper inclusive value
    @param freq          the cumulative frequency
    @param num_distinct  number of distinct/unique values in this bucket
  */
  Bucket(T lower, T upper, double freq, ha_rows num_distinct);

  /**
    @return lower inclusive value
  */
  const T& get_lower_inclusive() const { return m_lower_inclusive; }

  /**
    @return upper inclusive value
  */
  const T& get_upper_inclusive() const { return m_upper_inclusive; }

  /**
    @return cumulative frequency
  */
  double get_cumulative_frequency() const { return m_cumulative_frequency; }

  /**
    @return number of distinct values
  */
  ha_rows get_num_distinct() const { return m_num_distinct; }

  /**
    Convert this equi-height bucket to a JSON array.

    This function will take the contents of the current equi-height bucket
    and put it in the output parameter "json_array". The result is an array
    with the following contents:
      Index 0: Lower inclusive value.
      Index 1: Upper inclusive value.
      Index 2: Cumulative frequency.
      Index 3: Number of distinct values.

    @param[out] json_array output where the bucket content is to be stored

    @return     true on error, false otherwise
  */
  bool bucket_to_json(Json_array *json_array) const;
};

} // namespace equi_height
} // namespace histograms

#endif
