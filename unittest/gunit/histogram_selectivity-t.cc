/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <stdint.h>  // uint64_t
#include <stdlib.h>  // atoi

#include <string>  // std::string

#include <gtest/gtest.h>

#include "m_ctype.h"                     // my_charset_utf8mb4_0900_ai_ci
#include "my_alloc.h"                    // MEM_ROOT
#include "my_time.h"                     // MYSQL_TIME
#include "sql/field.h"                   // my_charset_numeric
#include "sql/histograms/equi_height.h"  // Equi_height
#include "sql/histograms/histogram.h"    // Histogram, Histogram_comparator
#include "sql/histograms/value_map.h"    // Value_map<T>
#include "sql/my_decimal.h"              // my_decimal
#include "sql_string.h"                  // String

namespace histogram_selectivity_test {

using namespace histograms;

class HistogramSelectivityTest : public ::testing::Test {
 protected:
  MEM_ROOT m_mem_root;

 public:
  HistogramSelectivityTest() : m_mem_root(PSI_NOT_INSTRUMENTED, 256) {}
};

template <class T>
void set_default(T *key) {
  *key = 1;
}

template <class T>
void increment(T *key) {
  *key += 1;
}

template <class T>
std::string key_to_string(const T &key) {
  using std::to_string;
  return to_string(key);
}

void set_default(my_decimal *key) {
  int2my_decimal(E_DEC_FATAL_ERROR, 0LL, false, key);
}

void increment(my_decimal *key) {
  longlong value;
  my_decimal2int(E_DEC_FATAL_ERROR, key, false, &value);
  int2my_decimal(E_DEC_FATAL_ERROR, value + 1, false, key);
}

std::string key_to_string(const my_decimal &decimal) {
  longlong value;
  my_decimal2int(E_DEC_FATAL_ERROR, &decimal, false, &value);
  return std::to_string(value);
}

void set_default(MYSQL_TIME *datetime) {
  set_zero_time(datetime, MYSQL_TIMESTAMP_DATETIME);
}

void increment(MYSQL_TIME *datetime) {
  datetime->year = (datetime->year + 1) % 10000;
  datetime->month = (datetime->month + 1) % 12;
  datetime->day = (datetime->day + 1) % 28;
  datetime->hour = (datetime->hour + 1) % 12;
  datetime->minute = (datetime->minute + 1) % 60;
  datetime->second = (datetime->second + 1) % 60;
}

std::string key_to_string(const MYSQL_TIME &datetime) {
  char datetime_characters[MAX_DATE_STRING_REP_LENGTH];
  my_datetime_to_str(datetime, datetime_characters, 0);
  return std::string(datetime_characters);
}

void set_default(String *key) {
  key->set_int(0, false, &my_charset_utf8mb4_0900_ai_ci);
}

void increment(String *key) {
  int value = atoi(key->c_ptr_safe());
  value += 1;
  key->set_int(value, false, &my_charset_utf8mb4_0900_ai_ci);
}

enum class FrequencyDistribution {
  Uniform,
  Linear,
  Quadratic,
  Cubic,
  LinearModulo100,
  LinearDecreasing,
  QuadraticDecreasing,
  ExponentiallyDecreasing,
  Pseudorandom,
  SingleHeavyValue,
  ExponentialTail,
};

template <class T>
void fill_value_map(Value_map<T> *map, int number_of_keys,
                    FrequencyDistribution dist) {
  T key;
  set_default(&key);
  switch (dist) {
    case FrequencyDistribution::Uniform: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, 1);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::Linear: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, i);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::Quadratic: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, i * i);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::Cubic: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, i * i * i);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::LinearModulo100: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, (i % 100) + 1);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::LinearDecreasing: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, number_of_keys - i + 1);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::QuadraticDecreasing: {
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, number_of_keys * number_of_keys - i * i + 1);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::ExponentiallyDecreasing: {
      size_t frequency = number_of_keys * number_of_keys;
      size_t one = 1;
      for (int i = 1; i <= number_of_keys; ++i) {
        map->add_values(key, std::max(frequency, one));
        frequency = frequency / 2;
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::Pseudorandom: {
      // We use a random polynomial in a prime field (p = 2^17 - 1) to generate
      // a fixed pseudorandom sequence (also known as universal hashing).
      const uint64_t max_frequency = 10000;
      const uint64_t p = 131071;
      for (int i = 1; i <= number_of_keys; ++i) {
        uint64_t x = static_cast<uint64_t>(i);
        uint64_t frequency =
            1 + (((39618 + 107019 * x + 78986 * x * x) % p) % max_frequency);
        map->add_values(key, frequency);
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::SingleHeavyValue: {
      for (int i = 1; i <= number_of_keys; ++i) {
        if (i == number_of_keys / 2) {
          map->add_values(key, number_of_keys);
        } else {
          map->add_values(key, 1);
        }
        increment(&key);
      }
      break;
    }
    case FrequencyDistribution::ExponentialTail: {
      // Add an exponentially increasing tail to the otherwise uniform data.
      for (int i = 1; i <= number_of_keys; ++i) {
        int remaining_keys = number_of_keys - i + 1;
        int scale = 1;
        if (remaining_keys <= 5) {
          map->add_values(key, scale * number_of_keys);
          scale = 2 * scale;
        } else {
          map->add_values(key, 1);
        }
        increment(&key);
      }
      break;
    }
  }
}

std::string ValueMapTypeToString(Value_map_type type) {
  switch (type) {
    case Value_map_type::INVALID:
      return "INVALID";
    case Value_map_type::STRING:
      return "STRING";
    case Value_map_type::INT:
      return "INT";
    case Value_map_type::UINT:
      return "UINT";
    case Value_map_type::DOUBLE:
      return "DOUBLE";
    case Value_map_type::DECIMAL:
      return "DECIMAL";
    case Value_map_type::DATE:
      return "DATE";
    case Value_map_type::TIME:
      return "TIME";
    case Value_map_type::DATETIME:
      return "DATETIME";
    case Value_map_type::ENUM:
      return "ENUM";
    case Value_map_type::SET:
      return "SET";
  }
  return "Error";
}

std::string FrequencyDistributionToString(FrequencyDistribution distribution) {
  switch (distribution) {
    case FrequencyDistribution::Uniform:
      return "Uniform";
    case FrequencyDistribution::Linear:
      return "Linear";
    case FrequencyDistribution::Quadratic:
      return "Quadratic";
    case FrequencyDistribution::Cubic:
      return "Cubic";
    case FrequencyDistribution::LinearModulo100:
      return "LinearModulo100";
    case FrequencyDistribution::LinearDecreasing:
      return "LinearDecreasing";
    case FrequencyDistribution::QuadraticDecreasing:
      return "QuadraticDecreasing";
    case FrequencyDistribution::ExponentiallyDecreasing:
      return "ExponentiallyDecreasing";
    case FrequencyDistribution::Pseudorandom:
      return "Pseudorandom";
    case FrequencyDistribution::SingleHeavyValue:
      return "SingleHeavyValue";
    case FrequencyDistribution::ExponentialTail:
      return "ExponentialTail";
  }
  return "Error";
}

std::string SelectivityErrorInfo(Value_map_type type,
                                 FrequencyDistribution distribution,
                                 size_t number_of_buckets) {
  return std::string("Histogram type: ") + ValueMapTypeToString(type) +
         ", Frequency distribution: " +
         FrequencyDistributionToString(distribution) +
         ", Buckets: " + std::to_string(number_of_buckets);
}

// Fill a value map according to a given distribution, build a histogram, and
// verify that histogram selectivity estimates do not deviate from the true
// selectivities by too much.
// With the right construction algorithm it is possible to guarantee an absolute
// error of at most 2.0/#buckets. While the the current equi-height construction
// offers no such guarantee, it still passes the test.
template <class T>
void VerifySelectivityEstimates(MEM_ROOT *mem_root, CHARSET_INFO *charset,
                                Value_map_type type,
                                FrequencyDistribution distribution,
                                size_t number_of_buckets) {
  // The number_of_keys cubed should fit into an int, otherwise the Cubic
  // distribution will overflow.
  const int number_of_keys = 1000;

  Value_map<T> key_frequencies(charset, type);
  fill_value_map<T>(&key_frequencies, number_of_keys, distribution);
  Equi_height<T> *histogram =
      Equi_height<T>::create(mem_root, "db1", "tbl1", "col1", type);
  EXPECT_FALSE(histogram->build_histogram(key_frequencies, number_of_buckets));

  ha_rows total_frequency = 0;
  for (const auto &[key, frequency] : key_frequencies)
    total_frequency += frequency;

  const double max_abs_error =
      2.0 / static_cast<double>(number_of_buckets) + 0.00000001;

  std::string error_info =
      SelectivityErrorInfo(type, distribution, number_of_buckets);

  ha_rows cumulative_frequency = 0;
  for (const auto &[key, frequency] : key_frequencies) {
    double less_than_selectivity =
        static_cast<double>(cumulative_frequency) / total_frequency;
    EXPECT_NEAR(less_than_selectivity,
                histogram->get_less_than_selectivity(key), max_abs_error)
        << "less than " << key_to_string(key) << "\n"
        << error_info;

    double equal_to_selectivity =
        static_cast<double>(frequency) / total_frequency;
    EXPECT_NEAR(equal_to_selectivity, histogram->get_equal_to_selectivity(key),
                max_abs_error)
        << "equal to " << key_to_string(key) << "\n"
        << error_info;

    double greater_than_selectivity =
        1.0 - (less_than_selectivity + equal_to_selectivity);
    EXPECT_NEAR(greater_than_selectivity,
                histogram->get_greater_than_selectivity(key), max_abs_error)
        << "greater than " << key_to_string(key) << "\n"
        << error_info;

    cumulative_frequency += frequency;
  }
}

TEST_F(HistogramSelectivityTest, EquiHeightSelectivity) {
  std::vector<Value_map_type> histogram_types = {
      Value_map_type::STRING, Value_map_type::INT, Value_map_type::UINT,
      Value_map_type::DOUBLE, Value_map_type::DECIMAL,
      // Value_map_type::DATE,
      // Value_map_type::TIME,
      Value_map_type::DATETIME,
      // Value_map_type::ENUM,
      // Value_map_type::SET,
  };
  std::vector<FrequencyDistribution> distributions = {
      FrequencyDistribution::Uniform,
      FrequencyDistribution::Linear,
      FrequencyDistribution::Quadratic,
      FrequencyDistribution::Cubic,
      FrequencyDistribution::LinearModulo100,
      FrequencyDistribution::LinearDecreasing,
      FrequencyDistribution::QuadraticDecreasing,
      FrequencyDistribution::ExponentiallyDecreasing,
      FrequencyDistribution::Pseudorandom,
      FrequencyDistribution::SingleHeavyValue,
      FrequencyDistribution::ExponentialTail};
  std::vector<size_t> numbers_of_buckets = {2, 4, 8, 16, 32, 64, 128, 256, 512};
  for (const auto &histogram_type : histogram_types) {
    for (const auto &distribution : distributions) {
      for (const auto &number_of_buckets : numbers_of_buckets) {
        switch (histogram_type) {
          case Value_map_type::INT: {
            VerifySelectivityEstimates<longlong>(
                &m_mem_root, &my_charset_numeric, histogram_type, distribution,
                number_of_buckets);
            break;
          }
          case Value_map_type::STRING:
            VerifySelectivityEstimates<String>(
                &m_mem_root, &my_charset_utf8mb4_0900_ai_ci, histogram_type,
                distribution, number_of_buckets);
            break;
          case Value_map_type::UINT: {
            VerifySelectivityEstimates<ulonglong>(
                &m_mem_root, &my_charset_numeric, histogram_type, distribution,
                number_of_buckets);
            break;
          }
          case Value_map_type::DOUBLE: {
            VerifySelectivityEstimates<double>(&m_mem_root, &my_charset_numeric,
                                               histogram_type, distribution,
                                               number_of_buckets);
            break;
          }
          case Value_map_type::DECIMAL: {
            VerifySelectivityEstimates<my_decimal>(
                &m_mem_root, &my_charset_numeric, histogram_type, distribution,
                number_of_buckets);
            break;
          }
          case Value_map_type::DATE:
          case Value_map_type::TIME:
          case Value_map_type::DATETIME: {
            VerifySelectivityEstimates<MYSQL_TIME>(
                &m_mem_root, &my_charset_numeric, histogram_type, distribution,
                number_of_buckets);
            break;
          }
          case Value_map_type::ENUM:
          case Value_map_type::SET:
            EXPECT_TRUE(false)
                << "Test for Value_map_type::"
                << ValueMapTypeToString(histogram_type) << " not implemented.";
            break;
          case Value_map_type::INVALID:
            EXPECT_TRUE(false) << "Value_map_type::INVALID.";
            break;
        }
      }
    }
  }
}

}  // namespace histogram_selectivity_test
