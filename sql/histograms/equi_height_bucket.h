#ifndef HISTOGRAMS_EQUI_HEIGHT_BUCKET_INCLUDED
#define HISTOGRAMS_EQUI_HEIGHT_BUCKET_INCLUDED

/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "my_base.h"  // ha_rows
#include "my_inttypes.h"
#include "mysql_time.h"
#include "sql/my_decimal.h"
#include "sql_string.h"

class Json_array;

namespace histograms {
namespace equi_height {

/**
  Equi-height bucket.
*/
template <class T>
class Bucket {
 private:
  /// Lower inclusive value contained in this bucket.
  const T m_lower_inclusive;

  /// Upper inclusive value contained in this bucket.
  const T m_upper_inclusive;

  /// The cumulative frequency. 0.0 <= m_cumulative_frequency <= 1.0.
  const double m_cumulative_frequency;

  /// Number of distinct values in this bucket.
  const ha_rows m_num_distinct;

  /**
    Add values to a JSON bucket.

    This function adds the lower and upper inclusive value to the supplied
    JSON array. The lower value is added first.

    @param      lower_value The lower inclusive value to add.
    @param      upper_value The upper inclusive value to add.
    @param[out] json_array  A JSON array where the bucket data is stored.

    @return     True on error, false otherwise.
  */
  static bool add_values_json_bucket(const T &lower_value, const T &upper_value,
                                     Json_array *json_array);

 public:
  /**
    Equi-height bucket constructor.

    Does nothing more than setting the member variables.

    @param lower         Lower inclusive value.
    @param upper         Upper inclusive value.
    @param freq          The cumulative frequency.
    @param num_distinct  Number of distinct values in this bucket.
  */
  Bucket(T lower, T upper, double freq, ha_rows num_distinct);

  /**
    @return Lower inclusive value.
  */
  const T &get_lower_inclusive() const { return m_lower_inclusive; }

  /**
    @return Upper inclusive value.
  */
  const T &get_upper_inclusive() const { return m_upper_inclusive; }

  /**
    @return Cumulative frequency.
  */
  double get_cumulative_frequency() const { return m_cumulative_frequency; }

  /**
    @return Number of distinct values.
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

    @param[out] json_array Output where the bucket content is to be stored.

    @return     True on error, false otherwise.
  */
  bool bucket_to_json(Json_array *json_array) const;

  /**
    Finds the relative location of "value" between bucket endpoints.
    This is used to determine the fraction of a bucket to include in selectivity
    estimates in the case where the query value lies inside a bucket.

    get_distance_from_lower returns the fraction of all elements between bucket
    endpoints [a, b] that lie in the interval [a, value), i.e., strictly less
    than value. For some histogram types the return value is only an estimate.

    @param value The value to calculate the distance for.

    @return The distance between "value" and lower inclusive value.
  */
  double get_distance_from_lower(const T &value) const;

  /**
    Returns the fraction of all elements between bucket endpoints [a, b] that
    are strictly greater than "value". For some histogram types the return value
    is only an estimate.

    @return The distance between "value" and upper inclusive value.
  */
  double get_distance_from_upper(const T &value) const;
};

}  // namespace equi_height
}  // namespace histograms

#endif
