#ifndef HISTOGRAMS_HISTOGRAM_INCLUDED
#define HISTOGRAMS_HISTOGRAM_INCLUDED

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
  @file sql/histograms/histogram.h
  Histogram base class.

  This file defines the base class for all histogram types. We keep the base
  class itself non-templatized in order to more easily send a histogram as an
  argument, collect multiple histograms in a single collection etc.

  A histogram is stored as a JSON object. This gives the flexibility of storing
  virtually an unlimited number of buckets, data values in its full length and
  easily expanding with new histogram types in the future. They are stored
  persistently in the system table mysql.column_stats.

  We keep all histogram code in the namespace "histograms" in order to avoid
  name conflicts etc.
*/

#include <cstddef>               // size_t
#include <map>                   // std::map
#include <string>                // std::string
#include <utility>               // std::pair

#include "lex_string.h"
#include "m_string.h"            // LEX_CSTRING
#include "memroot_allocator.h"   // Memroot_allocator
#include "my_base.h"             // ha_rows
#include "mysql_time.h"          // MYSQL_TIME
#include "sql_alloc.h"           // Sql_alloc
#include "thr_malloc.h"

class Json_object;
class String;
class my_decimal;

namespace histograms {

/**
  The maximum number of characters to evaluate when building histograms. For
  binary/blob values, this is the number of bytes to consider.
*/
static const size_t HISTOGRAM_MAX_COMPARE_LENGTH= 42;

/// The default (and invalid) value for "m_null_values_fraction".
static const double INVALID_NULL_VALUES_FRACTION= -1.0;

/**
  Histogram comparator.

  Typical usage is in a "value map", where we for instance need to sort based
  on string collation and similar.
*/
struct Histogram_comparator
{
public:
  /**
    Compare two values for equality.

    @param lhs first value to compare
    @param rhs second value to compare

    @retval 0 the two values are considered equal
    @retval <0 lhs is considered to be smaller than rhs
    @retval >0 lhs is considered to be bigger than rhs
  */
  template <class T>
  static int compare(const T &lhs, const T &rhs)
  {
    if (lhs < rhs)
      return -1;
    else if (lhs > rhs)
      return 1;

    return 0;
  }

  /**
    Overload operator(), so that we can use this struct as a custom comparator
    in std::map.

    @param lhs first value to compare
    @param rhs second value to compare

    @return true if lhs is considered to be smaller/less than rhs.
  */
  template <class T>
  bool operator()(const T &lhs, const T &rhs) const
  {
    return compare(lhs, rhs) < 0;
  }
};


// Typedefs.
template<typename T>
using value_map_allocator = Memroot_allocator<std::pair<const T, ha_rows>>;

template<typename T>
using value_map_type = std::map<T, ha_rows, Histogram_comparator,
                                value_map_allocator<T> >;

/**
  Histogram base class.
*/
class Histogram : public Sql_alloc
{
public:
  /// All supported histogram types in MySQL.
  enum class enum_histogram_type
  {
    EQUI_HEIGHT,
    SINGLETON
  };

  /// The different fields in mysql.column_stats.
  enum enum_fields
  {
    FIELD_DATABASE_NAME = 0,
    FIELD_TABLE_NAME = 1,
    FIELD_COLUMN_NAME = 2,
    FIELD_HISTOGRAM = 3
  };

protected:
  /// The fraction of NULL values in the histogram (between 0.0 and 1.0).
  double m_null_values_fraction;

  /// String representation of the JSON field "buckets".
  static constexpr const char *buckets_str() { return "buckets"; }

  /// String representation of the JSON field "last-updated".
  static constexpr const char *last_updated_str() { return "last-updated"; }

  /// String representation of the JSON field "histogram-type".
  static constexpr const char *histogram_type_str() { return "histogram-type"; }

  /// String representation of the JSON field "null-values".
  static constexpr const char *null_values_str() {return "null-values"; }
private:
  /// The MEM_ROOT where the histogram contents will be allocated.
  MEM_ROOT * const m_mem_root;

  /// The type of this histogram.
  const enum_histogram_type m_hist_type;

  /// Name of the database this histogram represents.
  LEX_CSTRING m_database_name;

  /// Name of the table this histogram represents.
  LEX_CSTRING m_table_name;

  /// Name of the column this histogram represents.
  LEX_CSTRING m_column_name;
public:
  /**
    Constructor.

    @param mem_root the mem_root where the histogram contents will be allocated
    @param db_name  name of the database this histogram represents
    @param tbl_name name of the table this histogram represents
    @param col_name name of the column this histogram represents
    @param type     the histogram type
  */
  Histogram(MEM_ROOT *mem_root, std::string db_name, std::string tbl_name,
            std::string col_name, enum_histogram_type type);

  /// Destructor.
  virtual ~Histogram() {}

  /**
    @return name of the database this histogram represents
  */
  const LEX_CSTRING get_database_name() const { return m_database_name; }

  /**
    @return name of the table this histogram represents
  */
  const LEX_CSTRING get_table_name() const { return m_table_name; }

  /**
    @return name of the column this histogram represents
  */
  const LEX_CSTRING get_column_name() const { return m_column_name; }

  /**
    @return type of this histogram
  */
  enum_histogram_type get_histogram_type() const { return m_hist_type; }

  /**
    @return the fraction of NULL values, in the range [0.0, 1.0]
  */
  double get_null_values_fraction() const;

  /**
    Returns the histogram type as a readable string.

    @return a readable string representation of the histogram type
  */
  virtual std::string histogram_type_to_str() const = 0;

  /**
    @return number of buckets in this histogram
  */
  virtual size_t get_num_buckets() const = 0;

  /**
    Converts the histogram to a JSON object.

    @param[in,out] json_object output where the histogram is to be stored. The
                   caller is responsible for allocating/deallocating the JSON
                   object

    @return     true on error, false otherwise
  */
  virtual bool histogram_to_json(Json_object *json_object) const = 0;
};

/**
  Create a histogram from a value map.

  This function will build a histogram from a value map. The histogram type
  depends on both the size of the input data, as well as the number of buckets
  specified. If the number of distinct values is less than or equal to the
  number of buckets, a Singleton histogram will be created. Otherwise, an
  equi-height histogram will be created.

  The histogram will be allocated on the supplied mem_root, and it is the
  callers responsibility to properly clean up when the histogram isn't needed
  anymore.

  @param   mem_root        the MEM_ROOT where the histogram contents will be
                           allocated
  @param   value_map       a value map containing [value, frequency]
  @param   num_null_values the number of NULL values in the data set
  @param   num_buckets     the maximum number of buckets to create
  @param   db_name         name of the database this histogram represents
  @param   tbl_name        name of the table this histogram represents
  @param   col_name        name of the column this histogram represents

  @return  a histogram, using at most "num_buckets" buckets. The histogram
           type depends on the size of the input data, and the number of
           buckets
*/
template <class T>
Histogram *build_histogram(MEM_ROOT *mem_root,
                           const value_map_type<T> &value_map,
                           ha_rows num_null_values, size_t num_buckets,
                           std::string db_name, std::string tbl_name,
                           std::string col_name);

// Explicit template instantiations.
template <>
int Histogram_comparator::compare(const String &, const String &);

template <>
int Histogram_comparator::compare(const MYSQL_TIME &, const MYSQL_TIME &);

template <>
int Histogram_comparator::compare(const my_decimal &, const my_decimal &);

} // namespace histograms

#endif
