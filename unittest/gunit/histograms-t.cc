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

#include <gtest/gtest.h>
#include <limits>  // std::numeric_limits
#include <map>     // std::map
#include <string>  // std::string

#include "lex_string.h"
#include "m_ctype.h"  // my_charset_latin1, my_charset_bin
#include "my_inttypes.h"
#include "my_systime.h"                  // my_micro_time()
#include "my_time.h"                     // MYSQL_TIME
#include "sql-common/json_dom.h"         // Json_object
#include "sql/field.h"                   // my_charset_numeric
#include "sql/histograms/equi_height.h"  // Equi_height
#include "sql/histograms/histogram.h"    // Histogram, Histogram_comparator
#include "sql/histograms/singleton.h"    // Singleton
#include "sql/histograms/value_map.h"    // Value_map<T>
#include "sql/my_decimal.h"              // my_decimal
#include "sql/sql_time.h"                // my_time_compare
#include "sql/tztime.h"                  // my_tz_UTC
#include "template_utils.h"              // down_cast

namespace histograms_unittest {

using namespace histograms;

class HistogramsTest : public ::testing::Test {
 protected:
  MEM_ROOT m_mem_root;

  Value_map<double> double_values;
  Value_map<String> string_values;
  Value_map<ulonglong> uint_values;
  Value_map<longlong> int_values;
  Value_map<my_decimal> decimal_values;
  Value_map<MYSQL_TIME> datetime_values;
  Value_map<MYSQL_TIME> date_values;
  Value_map<MYSQL_TIME> time_values;
  Value_map<String> blob_values;

  /*
    Declare these arrays here, so that they survive the lifetime of the unit
    tests.

    Do not use negative char values, since these will be cast to an uchar
    pointer in the function sortcmp.
  */
  const char blob_buf1[4] = {0, 0, 0, 0};
  const char blob_buf2[4] = {127, 127, 127, 127};

 public:
  HistogramsTest()
      : m_mem_root(PSI_NOT_INSTRUMENTED, 256),
        double_values(&my_charset_numeric, Value_map_type::DOUBLE),
        string_values(&my_charset_latin1, Value_map_type::STRING),
        uint_values(&my_charset_numeric, Value_map_type::UINT),
        int_values(&my_charset_numeric, Value_map_type::INT),
        decimal_values(&my_charset_numeric, Value_map_type::DECIMAL),
        datetime_values(&my_charset_numeric, Value_map_type::DATETIME),
        date_values(&my_charset_numeric, Value_map_type::DATE),
        time_values(&my_charset_numeric, Value_map_type::TIME),
        blob_values(&my_charset_bin, Value_map_type::STRING) {
    // Double values.
    double_values.add_values(std::numeric_limits<double>::lowest(), 10);
    double_values.add_values(std::numeric_limits<double>::max(), 10);
    double_values.add_values(std::numeric_limits<double>::epsilon(), 10);
    double_values.add_values(0.0, 10);
    double_values.add_values(42.0, 10);
    double_values.add_values(43.0, 10);

    // String values.
    string_values.add_values(String("", &my_charset_latin1), 10);
    string_values.add_values(String("string4", &my_charset_latin1), 10);
    string_values.add_values(String("string3", &my_charset_latin1), 10);
    string_values.add_values(String("string1", &my_charset_latin1), 10);
    string_values.add_values(String("string2", &my_charset_latin1), 10);

    // Unsigned integer values (ulonglong).
    uint_values.add_values(std::numeric_limits<ulonglong>::lowest(), 10);
    uint_values.add_values(std::numeric_limits<ulonglong>::max(), 10);
    uint_values.add_values(42ULL, 10);
    uint_values.add_values(43ULL, 10);
    uint_values.add_values(10000ULL, 10);

    // Signed integer values (longlong).
    int_values.add_values(std::numeric_limits<longlong>::lowest(), 10);
    int_values.add_values(std::numeric_limits<longlong>::max(), 10);
    int_values.add_values(0LL, 10);
    int_values.add_values(-1LL, 10);
    int_values.add_values(1LL, 10);
    int_values.add_values(42LL, 10);
    int_values.add_values(10000LL, 10);

    // Decimal values (my_decimal).
    my_decimal decimal1;
    int2my_decimal(E_DEC_FATAL_ERROR, 0LL, false, &decimal1);
    decimal_values.add_values(decimal1, 10);

    my_decimal decimal2;
    int2my_decimal(E_DEC_FATAL_ERROR, -1000LL, false, &decimal2);
    decimal_values.add_values(decimal2, 10);

    my_decimal decimal3;
    int2my_decimal(E_DEC_FATAL_ERROR, 1000LL, false, &decimal3);
    decimal_values.add_values(decimal3, 10);

    my_decimal decimal4;
    int2my_decimal(E_DEC_FATAL_ERROR, 42LL, false, &decimal4);
    decimal_values.add_values(decimal4, 10);

    my_decimal decimal5;
    int2my_decimal(E_DEC_FATAL_ERROR, 1LL, false, &decimal5);
    decimal_values.add_values(decimal5, 10);

    /*
      Datetime values (MYSQL_TIME).

      We are using these packed values for testing:

      914866242077065216  => 1000-01-01 00:00:00.000000
      914866242077065217  => 1000-01-01 00:00:00.000001
      1845541820734373888 => 2017-05-23 08:08:03.000000
      9147936188962652735 => 9999-12-31 23:59:59.999999
      9147936188962652734 => 9999-12-31 23:59:59.999998
    */
    MYSQL_TIME datetime1;
    TIME_from_longlong_datetime_packed(&datetime1, 9147936188962652734);
    datetime_values.add_values(datetime1, 10);

    MYSQL_TIME datetime2;
    TIME_from_longlong_datetime_packed(&datetime2, 914866242077065217);
    datetime_values.add_values(datetime2, 10);

    MYSQL_TIME datetime3;
    TIME_from_longlong_datetime_packed(&datetime3, 914866242077065216);
    datetime_values.add_values(datetime3, 10);

    MYSQL_TIME datetime4;
    TIME_from_longlong_datetime_packed(&datetime4, 1845541820734373888);
    datetime_values.add_values(datetime4, 10);

    MYSQL_TIME datetime5;
    TIME_from_longlong_datetime_packed(&datetime5, 9147936188962652735);
    datetime_values.add_values(datetime5, 10);

    /*
      Date values (MYSQL_TIME).

      Do not test negative values, since negative DATETIME is not supported by
      MySQL. We also call "set_zero_time", to initialize the entire MYSQL_TIME
      structure. If we don't, valgrind will complain on uninitialised values.
    */
    MYSQL_TIME date1;
    set_zero_time(&date1, MYSQL_TIMESTAMP_DATE);
    set_max_hhmmss(&date1);
    date_values.add_values(date1, 10);

    MYSQL_TIME date2;
    set_zero_time(&date2, MYSQL_TIMESTAMP_DATE);
    TIME_from_longlong_date_packed(&date2, 10000);
    date_values.add_values(date2, 10);

    MYSQL_TIME date3;
    set_zero_time(&date3, MYSQL_TIMESTAMP_DATE);
    TIME_from_longlong_date_packed(&date3, 0);
    date_values.add_values(date3, 10);

    MYSQL_TIME date4;
    set_zero_time(&date4, MYSQL_TIMESTAMP_DATE);
    TIME_from_longlong_date_packed(&date4, 100);
    date_values.add_values(date4, 10);

    MYSQL_TIME date5;
    set_zero_time(&date5, MYSQL_TIMESTAMP_DATE);
    TIME_from_longlong_date_packed(&date5, 100000);
    date_values.add_values(date5, 10);

    /*
      Time values (MYSQL_TIME).

      Do not test negative values, since negative DATETIME is not supported by
      MySQL.
    */
    MYSQL_TIME time1;
    set_zero_time(&time1, MYSQL_TIMESTAMP_TIME);
    set_max_time(&time1, false);
    time_values.add_values(time1, 10);

    MYSQL_TIME time2;
    set_zero_time(&time2, MYSQL_TIMESTAMP_TIME);
    TIME_from_longlong_time_packed(&time2, 12);
    time_values.add_values(time2, 10);

    MYSQL_TIME time3;
    set_zero_time(&time3, MYSQL_TIMESTAMP_TIME);
    TIME_from_longlong_time_packed(&time3, 0);
    time_values.add_values(time3, 10);

    MYSQL_TIME time4;
    set_zero_time(&time4, MYSQL_TIMESTAMP_TIME);
    TIME_from_longlong_time_packed(&time4, 42);
    time_values.add_values(time4, 10);

    MYSQL_TIME time5;
    set_zero_time(&time5, MYSQL_TIMESTAMP_TIME);
    TIME_from_longlong_time_packed(&time5, 100000);
    time_values.add_values(time5, 10);

    // Blob values.
    blob_values.add_values(String(blob_buf1, 4, &my_charset_bin), 10);
    blob_values.add_values(String(blob_buf2, 4, &my_charset_bin), 10);
    blob_values.add_values(String("foo", &my_charset_bin), 10);
    blob_values.add_values(String("bar", &my_charset_bin), 10);
    blob_values.add_values(String("foobar", &my_charset_bin), 10);
  }
};

/*
  Utility function that verify the following properties for a histogram that is
  converted to JSON:
    - All histogram types must have the field "last-updated" of type J_DATETIME.
    - All histogram types must have the field "histogram-type" of type J_STRING.
      * Check that the printed histogram type actually is the correct one.
    - All histogram types must have the field "buckets" of type J_ARRAY.
      * Check that the number of buckets in the JSON array is the same as the
        amount of buckets in the original histogram.
    - All histogram types must have the field "null-values" of type J_DOUBLE.
    - All histogram types must have the field "collation-id" of type J_UINT.
*/
void VerifyCommonJSONFields(Json_object *json_histogram,
                            const Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  // Last updated field.
  Json_dom *last_updated_dom = json_histogram->get("last-updated");
  EXPECT_NE(last_updated_dom, nullptr);
  EXPECT_EQ(last_updated_dom->json_type(), enum_json_type::J_DATETIME);

  // Histogram type field.
  Json_dom *histogram_type_dom = json_histogram->get("histogram-type");
  EXPECT_NE(histogram_type_dom, nullptr);
  EXPECT_EQ(histogram_type_dom->json_type(), enum_json_type::J_STRING);

  Json_string *json_histogram_type =
      static_cast<Json_string *>(histogram_type_dom);

  switch (histogram->get_histogram_type()) {
    case Histogram::enum_histogram_type::EQUI_HEIGHT:
      EXPECT_STREQ(json_histogram_type->value().c_str(), "equi-height");
      break;
    case Histogram::enum_histogram_type::SINGLETON:
      EXPECT_STREQ(json_histogram_type->value().c_str(), "singleton");
      break;
  }

  // Buckets field.
  Json_dom *buckets_dom = json_histogram->get("buckets");
  EXPECT_NE(buckets_dom, nullptr);
  EXPECT_EQ(buckets_dom->json_type(), enum_json_type::J_ARRAY);

  // Fraction of null values.
  Json_dom *null_values_dom = json_histogram->get("null-values");
  EXPECT_NE(null_values_dom, nullptr);
  EXPECT_EQ(null_values_dom->json_type(), enum_json_type::J_DOUBLE);

  // Collation ID
  Json_dom *collation_id_dom = json_histogram->get("collation-id");
  EXPECT_NE(collation_id_dom, nullptr);
  EXPECT_EQ(collation_id_dom->json_type(), enum_json_type::J_UINT);

  Json_array *buckets = static_cast<Json_array *>(buckets_dom);
  EXPECT_EQ(buckets->size(), histogram->get_num_buckets());
}

/*
  Utility function that verifies the following constraints for a singleton
  histogram that is converted to JSON:
    - The value in a singleton bucket is greater than or equal to the value in
      the previous bucket.
    - The cumulative frequency is in the range (0.0, 1.0] (lower exclusive,
      upper inclusive).
    - The cumulative frequency is greater than the cumulative frequency in the
      previous bucket.
*/
void VerifySingletonBucketConstraintsDouble(const Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  double previous_value = 0.0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_double *json_double = down_cast<Json_double *>((*bucket)[0]);
    double current_value = json_double->value();
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(previous_value, current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifySingletonBucketConstraintsInt(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  longlong previous_value = 0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_int *json_int = down_cast<Json_int *>((*bucket)[0]);
    longlong current_value = json_int->value();
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(previous_value, current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifySingletonBucketConstraintsUInt(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  ulonglong previous_value = 0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_uint = down_cast<Json_uint *>((*bucket)[0]);
    ulonglong current_value = json_uint->value();
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(previous_value, current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifySingletonBucketConstraintsString(Histogram *histogram,
                                            const CHARSET_INFO *charset) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  String previous_value;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_opaque *json_opaque = down_cast<Json_opaque *>((*bucket)[0]);
    String current_value(json_opaque->value(), json_opaque->size(), charset);
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(previous_value, current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifySingletonBucketConstraintsDecimal(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  const my_decimal *previous_value = nullptr;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_decimal *json_decimal = down_cast<Json_decimal *>((*bucket)[0]);
    const my_decimal *current_value = json_decimal->value();
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(*previous_value, *current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifySingletonBucketConstraintsTemporal(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  const MYSQL_TIME *previous_value = nullptr;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[1]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_datetime *json_datetime = down_cast<Json_datetime *>((*bucket)[0]);
    const MYSQL_TIME *current_value = json_datetime->value();
    if (i > 0) {
      EXPECT_TRUE(Histogram_comparator()(*previous_value, *current_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }
    previous_value = current_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

/*
  Utility function that verifies the following constraints for an equi-height
  histogram that is converted to JSON:
    - The lower inclusive value in an equi-height bucket is less than or equal
      to the upper inclusive value.
    - The lower inclusive value in an equi-height bucket is greater than the
      upper inclusive value of the previous bucket.
    - The cumulative frequency is in the range (0.0, 1.0] (lower exclusive,
      upper inclusive).
    - The cumulative frequency is greater than the cumulative frequency in the
      previous bucket.
    - The number of distinct values in a bucket is equal to or greater than 1.
*/
void VerifyEquiHeightBucketConstraintsDouble(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  double previous_upper_value = 0.0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_double *json_double_lower = down_cast<Json_double *>((*bucket)[0]);
    Json_double *json_double_upper = down_cast<Json_double *>((*bucket)[1]);

    double current_lower_value = json_double_lower->value();
    double current_upper_value = json_double_upper->value();
    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(previous_upper_value, current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(current_upper_value, current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifyEquiHeightBucketConstraintsInt(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  longlong previous_upper_value = 0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_int *json_int_lower = down_cast<Json_int *>((*bucket)[0]);
    Json_int *json_int_upper = down_cast<Json_int *>((*bucket)[1]);

    longlong current_lower_value = json_int_lower->value();
    longlong current_upper_value = json_int_upper->value();
    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(previous_upper_value, current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(current_upper_value, current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifyEquiHeightBucketConstraintsUInt(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  ulonglong previous_upper_value = 0;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_uint *json_uint_lower = down_cast<Json_uint *>((*bucket)[0]);
    Json_uint *json_uint_upper = down_cast<Json_uint *>((*bucket)[1]);

    ulonglong current_lower_value = json_uint_lower->value();
    ulonglong current_upper_value = json_uint_upper->value();
    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(previous_upper_value, current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(current_upper_value, current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifyEquiHeightBucketConstraintsString(Histogram *histogram,
                                             const CHARSET_INFO *charset) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  String previous_upper_value;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_opaque *json_opaque_lower = down_cast<Json_opaque *>((*bucket)[0]);
    Json_opaque *json_opaque_upper = down_cast<Json_opaque *>((*bucket)[1]);

    String current_lower_value(json_opaque_lower->value(),
                               json_opaque_lower->size(), charset);
    String current_upper_value(json_opaque_upper->value(),
                               json_opaque_upper->size(), charset);

    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(previous_upper_value, current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(current_upper_value, current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifyEquiHeightBucketConstraintsDecimal(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  const my_decimal *previous_upper_value = nullptr;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_decimal *json_decimal_lower = down_cast<Json_decimal *>((*bucket)[0]);
    Json_decimal *json_decimal_upper = down_cast<Json_decimal *>((*bucket)[1]);

    const my_decimal *current_lower_value(json_decimal_lower->value());
    const my_decimal *current_upper_value(json_decimal_upper->value());

    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(*previous_upper_value, *current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(*current_upper_value, *current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

void VerifyEquiHeightBucketConstraintsTemporal(Histogram *histogram) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = down_cast<Json_array *>(buckets_dom);

  const MYSQL_TIME *previous_upper_value = nullptr;
  double previous_cumulative_frequency = 0.0;
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    Json_array *bucket = static_cast<Json_array *>(bucket_dom);

    Json_double *json_frequency = down_cast<Json_double *>((*bucket)[2]);
    double current_cumulative_frequency = json_frequency->value();
    EXPECT_GT(current_cumulative_frequency, 0.0);
    EXPECT_LE(current_cumulative_frequency, 1.0);

    Json_uint *json_num_distinct = down_cast<Json_uint *>((*bucket)[3]);
    EXPECT_GE(json_num_distinct->value(), 1ULL);

    /*
      Index 1 should be lower inclusive value, and index 2 should be upper
      inclusive value.
    */
    Json_datetime *json_datetime_lower =
        down_cast<Json_datetime *>((*bucket)[0]);
    Json_datetime *json_datetime_upper =
        down_cast<Json_datetime *>((*bucket)[1]);

    const MYSQL_TIME *current_lower_value(json_datetime_lower->value());
    const MYSQL_TIME *current_upper_value(json_datetime_upper->value());

    if (i > 0) {
      EXPECT_TRUE(
          Histogram_comparator()(*previous_upper_value, *current_lower_value));
      EXPECT_LT(previous_cumulative_frequency, current_cumulative_frequency);
    }

    EXPECT_FALSE(
        Histogram_comparator()(*current_upper_value, *current_lower_value));

    previous_upper_value = current_upper_value;
    previous_cumulative_frequency = current_cumulative_frequency;
  }
}

/*
  Utility function that verify the following properties for an equi-height
  histogram that is converted to JSON:
    - The histogram has all the "common" JSON fields
      (see VerifyCommonJSONFields).
    - All equi-height buckets have the following types in each index:
      0: J_DOUBLE
      1: Depends on the data type stored in the histogram
      2: Depends on the data type stored in the histogram
      3: J_UINT

  The function does not check that the values are correct, but rather that they
  are present with the expected type.
*/
void VerifyEquiHeightJSONStructure(Histogram *histogram,
                                   enum_json_type expected_json_type) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));
  VerifyCommonJSONFields(&json_object, histogram);

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = static_cast<Json_array *>(buckets_dom);

  // Verify that all the buckets have the expected structure.
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    EXPECT_EQ(bucket_dom->json_type(), enum_json_type::J_ARRAY);

    Json_array *bucket = static_cast<Json_array *>(bucket_dom);
    EXPECT_EQ(bucket->size(), 4U);

    // Index 0 should be lower inclusive value.
    EXPECT_EQ((*bucket)[0]->json_type(), expected_json_type);

    // Index 1 should be upper inclusive value.
    EXPECT_EQ((*bucket)[1]->json_type(), expected_json_type);

    // Index 2 should be cumulative frequency.
    EXPECT_EQ((*bucket)[2]->json_type(), enum_json_type::J_DOUBLE);

    // Index 3 should be numer of distinct values.
    EXPECT_EQ((*bucket)[3]->json_type(), enum_json_type::J_UINT);
  }
}

/*
  Utility function that verify the following properties for a singleton
  histogram that is converted to JSON:
    - The histogram has all the "common" JSON fields
      (see VerifyCommonJSONFields).
    - All equi-height buckets have the following types in each index:
      0: J_DOUBLE
      1: Depends on the data type stored in the histogram

  The function does not check that the values are correct, but rather that they
  are present with the expected type.
*/
void VerifySingletonJSONStructure(Histogram *histogram,
                                  enum_json_type expected_json_type) {
  ASSERT_TRUE(histogram != nullptr);

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));
  VerifyCommonJSONFields(&json_object, histogram);

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *buckets = static_cast<Json_array *>(buckets_dom);

  // Verify that all the buckets have the expected structure.
  for (size_t i = 0; i < buckets->size(); ++i) {
    Json_dom *bucket_dom = (*buckets)[i];
    EXPECT_EQ(bucket_dom->json_type(), enum_json_type::J_ARRAY);

    Json_array *bucket = static_cast<Json_array *>(bucket_dom);
    EXPECT_EQ(bucket->size(), 2U);

    // Index 0 should be the value.
    EXPECT_EQ((*bucket)[0]->json_type(), expected_json_type);

    // Index 1 should be cumulative frequency.
    EXPECT_EQ((*bucket)[1]->json_type(), enum_json_type::J_DOUBLE);
  }
}

/*
  Check that a singleton histogram can be built and converted to JSON for all
  supported data types:

    - Double
    - String
    - Uint
    - Int
    - Decimal
    - Datetime (MYSQL_TIME)
    - Date (MYSQL_TIME)
    - Time (MYSQL_TIME)
    - Blob/binary
*/
TEST_F(HistogramsTest, DoubleSingletonToJSON) {
  Singleton<double> *histogram = Singleton<double>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DOUBLE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(double_values, double_values.size()));
  EXPECT_EQ(double_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(double_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_DOUBLE);
  VerifySingletonBucketConstraintsDouble(histogram);
}

TEST_F(HistogramsTest, StringSingletonToJSON) {
  Singleton<String> *histogram = Singleton<String>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::STRING);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(string_values, string_values.size()));
  EXPECT_EQ(string_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(string_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_OPAQUE);
  VerifySingletonBucketConstraintsString(histogram, &my_charset_latin1);
}

TEST_F(HistogramsTest, UintSingletonToJSON) {
  Singleton<ulonglong> *histogram = Singleton<ulonglong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::UINT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(uint_values, uint_values.size()));
  EXPECT_EQ(uint_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(uint_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_UINT);
  VerifySingletonBucketConstraintsUInt(histogram);
}

TEST_F(HistogramsTest, IntSingletonToJSON) {
  Singleton<longlong> *histogram = Singleton<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(int_values, int_values.size()));
  EXPECT_EQ(int_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(int_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_INT);
  VerifySingletonBucketConstraintsInt(histogram);
}

TEST_F(HistogramsTest, DecimalSingletonToJSON) {
  Singleton<my_decimal> *histogram = Singleton<my_decimal>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DECIMAL);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(
      histogram->build_histogram(decimal_values, decimal_values.size()));
  EXPECT_EQ(decimal_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(decimal_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_DECIMAL);
  VerifySingletonBucketConstraintsDecimal(histogram);
}

TEST_F(HistogramsTest, DatetimeSingletonToJSON) {
  Singleton<MYSQL_TIME> *histogram = Singleton<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATETIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(
      histogram->build_histogram(datetime_values, datetime_values.size()));
  EXPECT_EQ(datetime_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(datetime_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_DATETIME);
  VerifySingletonBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, DateSingletonToJSON) {
  Singleton<MYSQL_TIME> *histogram = Singleton<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(date_values, date_values.size()));
  EXPECT_EQ(date_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(date_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_DATE);
  VerifySingletonBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, TimeSingletonToJSON) {
  Singleton<MYSQL_TIME> *histogram = Singleton<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::TIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(time_values, time_values.size()));
  EXPECT_EQ(time_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(time_values.size(), histogram->get_num_distinct_values());

  VerifySingletonJSONStructure(histogram, enum_json_type::J_TIME);
  VerifySingletonBucketConstraintsTemporal(histogram);
}

/*
  Check that an equi-height histogram can be built and converted to JSON for all
  supported data types:

    - Double
    - String
    - Uint
    - Int
    - Decimal
    - Datetime (MYSQL_TIME)
    - Date (MYSQL_TIME)
    - Time (MYSQL_TIME)
    - Blob/binary

  Create equi-height histograms with the same number of buckets as the number
  of distinct values in the data set. This will lead to every histogram bucket
  having lower_inclusive_value == upper_inclusive value.

  We check that the resulting JSON has the expected structure, as well as every
  bucket having lower_inclusive_value <= upper_inclusive.
*/
TEST_F(HistogramsTest, DoubleEquiHeightToJSON) {
  Equi_height<double> *histogram = Equi_height<double>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DOUBLE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(double_values, double_values.size()));
  EXPECT_EQ(double_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(double_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DOUBLE);
  VerifyEquiHeightBucketConstraintsDouble(histogram);
}

TEST_F(HistogramsTest, StringEquiHeightToJSON) {
  Equi_height<String> *histogram = Equi_height<String>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::STRING);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(string_values, string_values.size()));
  EXPECT_EQ(string_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(string_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_OPAQUE);
  VerifyEquiHeightBucketConstraintsString(histogram, &my_charset_latin1);
}

TEST_F(HistogramsTest, UintEquiHeightToJSON) {
  Equi_height<ulonglong> *histogram = Equi_height<ulonglong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::UINT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(uint_values, uint_values.size()));
  EXPECT_EQ(uint_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(uint_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_UINT);
  VerifyEquiHeightBucketConstraintsUInt(histogram);
}

TEST_F(HistogramsTest, IntEquiHeightToJSON) {
  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(int_values, int_values.size()));
  EXPECT_EQ(int_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(int_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_INT);
  VerifyEquiHeightBucketConstraintsInt(histogram);
}

TEST_F(HistogramsTest, DecimalEquiHeightToJSON) {
  Equi_height<my_decimal> *histogram = Equi_height<my_decimal>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DECIMAL);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(
      histogram->build_histogram(decimal_values, decimal_values.size()));
  EXPECT_EQ(decimal_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(decimal_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DECIMAL);
  VerifyEquiHeightBucketConstraintsDecimal(histogram);
}

TEST_F(HistogramsTest, DatetimeEquiHeightToJSON) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATETIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(
      histogram->build_histogram(datetime_values, datetime_values.size()));
  EXPECT_EQ(datetime_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(datetime_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DATETIME);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, DateEquiHeightToJSON) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(date_values, date_values.size()));
  EXPECT_EQ(date_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(date_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DATE);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, TimeEquiHeightToJSON) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::TIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(time_values, time_values.size()));
  EXPECT_EQ(time_values.size(), histogram->get_num_buckets());
  EXPECT_EQ(time_values.size(), histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_TIME);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

/*
  Create Equi-height histograms with fewer buckets than the distinct number
  of values. This will force at least one of the buckets to have
  lower_inclusive_value != upper_inclusive_value.

  We check that the resulting JSON has the expected structure, as well as every
  bucket having lower_inclusive_value <= upper_inclusive.
*/
TEST_F(HistogramsTest, DoubleEquiHeightFewBuckets) {
  Equi_height<double> *histogram = Equi_height<double>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DOUBLE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(double_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DOUBLE);
  VerifyEquiHeightBucketConstraintsDouble(histogram);
}

TEST_F(HistogramsTest, StringEquiHeightFewBuckets) {
  Equi_height<String> *histogram = Equi_height<String>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::STRING);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(string_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_OPAQUE);
  VerifyEquiHeightBucketConstraintsString(histogram, &my_charset_latin1);
}

TEST_F(HistogramsTest, UintEquiHeightFewBuckets) {
  Equi_height<ulonglong> *histogram = Equi_height<ulonglong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::UINT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(uint_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_UINT);
  VerifyEquiHeightBucketConstraintsUInt(histogram);
}

TEST_F(HistogramsTest, IntEquiHeightFewBuckets) {
  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(int_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_INT);
  VerifyEquiHeightBucketConstraintsInt(histogram);
}

TEST_F(HistogramsTest, DecimalEquiHeightFewBuckets) {
  Equi_height<my_decimal> *histogram = Equi_height<my_decimal>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DECIMAL);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(decimal_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DECIMAL);
  VerifyEquiHeightBucketConstraintsDecimal(histogram);
}

TEST_F(HistogramsTest, DatetimeEquiHeightFewBuckets) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATETIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(datetime_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DATETIME);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, DateEquiHeightFewBuckets) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(date_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_DATE);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

TEST_F(HistogramsTest, TimeEquiHeightFewBuckets) {
  Equi_height<MYSQL_TIME> *histogram = Equi_height<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::TIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_FALSE(histogram->build_histogram(time_values, 2U));
  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_TIME);
  VerifyEquiHeightBucketConstraintsTemporal(histogram);
}

/*
  Verify that the "auto-select histogram"-mechanism works as expected. That is,
  it should select a singleton histogram when we have less or equal amount of
  distinct values as the specified amount of buckets. In all other cases it
  should create an equi-height histogram.
*/
TEST_F(HistogramsTest, AutoSelectHistogramType) {
  /*
    Case 1: Less buckets than the number of distinct values. We should end up
    with an equi-height histogram.
  */
  size_t num_buckets = double_values.size() - 1;
  Histogram *histogram1 = build_histogram(&m_mem_root, double_values,
                                          num_buckets, "db1", "tbl1", "col1");

  EXPECT_EQ(Histogram::enum_histogram_type::EQUI_HEIGHT,
            histogram1->get_histogram_type());
  EXPECT_LE(histogram1->get_num_buckets(), num_buckets);
  EXPECT_EQ(histogram1->get_num_distinct_values(), double_values.size());

  /*
    Case 2: Same number of buckets as the number of distinct values. We should
    end up with a singleton histogram.
  */
  num_buckets = double_values.size();
  Histogram *histogram2 = build_histogram(&m_mem_root, double_values,
                                          num_buckets, "db1", "tbl1", "col1");

  EXPECT_EQ(Histogram::enum_histogram_type::SINGLETON,
            histogram2->get_histogram_type());
  EXPECT_EQ(histogram2->get_num_buckets(), double_values.size());
  EXPECT_EQ(histogram2->get_num_distinct_values(), double_values.size());

  /*
    Case 3: More buckets than the number of distinct values. We should end up
    with a singleton histogram.
  */
  num_buckets = std::numeric_limits<std::size_t>::max();
  Histogram *histogram3 = build_histogram(&m_mem_root, double_values,
                                          num_buckets, "db1", "tbl1", "col1");

  EXPECT_EQ(Histogram::enum_histogram_type::SINGLETON,
            histogram3->get_histogram_type());
  EXPECT_LE(histogram3->get_num_buckets(), double_values.size());
  EXPECT_EQ(histogram3->get_num_distinct_values(), double_values.size());
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsInt(Json_array *singleton_buckets,
                                      int bucket_index,
                                      double cumulative_frequency,
                                      longlong value) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_int *json_value = down_cast<Json_int *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(value, json_value->value());
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsUInt(Json_array *singleton_buckets,
                                       int bucket_index,
                                       double cumulative_frequency,
                                       ulonglong value) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_uint *json_value = down_cast<Json_uint *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(value, json_value->value());
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsString(Json_array *singleton_buckets,
                                         int bucket_index,
                                         double cumulative_frequency,
                                         String value,
                                         const CHARSET_INFO *charset) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_opaque *json_value_dom = down_cast<Json_opaque *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  String json_value(json_value_dom->value(), json_value_dom->size(), charset);

  EXPECT_EQ(json_value.charset()->number, value.charset()->number);
  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(sortcmp(&value, &json_value, charset), 0);
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsDouble(Json_array *singleton_buckets,
                                         int bucket_index,
                                         double cumulative_frequency,
                                         double value) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_double *json_value = down_cast<Json_double *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(value, json_value->value());
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsDecimal(Json_array *singleton_buckets,
                                          int bucket_index,
                                          double cumulative_frequency,
                                          my_decimal value) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_decimal *json_value = down_cast<Json_decimal *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(my_decimal_cmp(json_value->value(), &value), 0);
}

/*
  An utility function that verifies the actual values in the singleton JSON
  bucket.
*/
void VerifySingletonBucketContentsTemporal(Json_array *singleton_buckets,
                                           int bucket_index,
                                           double cumulative_frequency,
                                           MYSQL_TIME value) {
  Json_array *json_bucket =
      down_cast<Json_array *>((*singleton_buckets)[bucket_index]);

  Json_datetime *json_value = down_cast<Json_datetime *>((*json_bucket)[0]);
  Json_double *json_cumulative_frequency =
      down_cast<Json_double *>((*json_bucket)[1]);

  EXPECT_DOUBLE_EQ(cumulative_frequency, json_cumulative_frequency->value());
  EXPECT_EQ(my_time_compare(*json_value->value(), value), 0);
}

template <typename T>
Equi_height<T> *BuildEquiHeightAndVerifyBasicProperties(
    MEM_ROOT *mem_root, const Value_map<T> &value_map, size_t num_buckets) {
  Equi_height<T> *histogram = Equi_height<T>::create(
      mem_root, "db1", "tbl1", "col1", value_map.get_data_type());
  EXPECT_TRUE(histogram != nullptr);

  EXPECT_EQ(histogram->get_num_buckets(), 0U);
  EXPECT_EQ(histogram->get_num_buckets_specified(), 0U);
  EXPECT_EQ(histogram->get_num_distinct_values(), 0U);
  EXPECT_EQ(histogram->get_data_type(), value_map.get_data_type());

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  EXPECT_FALSE(histogram->build_histogram(value_map, num_buckets));
  EXPECT_EQ(histogram->get_num_buckets(), num_buckets);
  EXPECT_EQ(histogram->get_num_buckets_specified(), num_buckets);
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());
  EXPECT_EQ(histogram->get_character_set()->number,
            value_map.get_character_set()->number);

  return histogram;
}

template <typename T>
void EquiHeightBucketsAreEqual(const equi_height::Bucket<T> &b1,
                               const equi_height::Bucket<T> &b2) {
  EXPECT_EQ(b1.get_cumulative_frequency(), b2.get_cumulative_frequency());
  EXPECT_EQ(b1.get_num_distinct(), b2.get_num_distinct());

  // Equality check: Neither value is less than the other.
  EXPECT_FALSE(Histogram_comparator()(b1.get_lower_inclusive(),
                                      b2.get_lower_inclusive()));
  EXPECT_FALSE(Histogram_comparator()(b2.get_lower_inclusive(),
                                      b1.get_lower_inclusive()));

  EXPECT_FALSE(Histogram_comparator()(b1.get_upper_inclusive(),
                                      b2.get_upper_inclusive()));
  EXPECT_FALSE(Histogram_comparator()(b2.get_upper_inclusive(),
                                      b1.get_upper_inclusive()));
}

template <typename T>
void EquiHeightHistogramsAreEqual(const Equi_height<T> *h1,
                                  const Equi_height<T> *h2) {
  EXPECT_STREQ(h1->get_database_name().str, h2->get_database_name().str);
  EXPECT_STREQ(h1->get_table_name().str, h2->get_table_name().str);
  EXPECT_STREQ(h1->get_column_name().str, h2->get_column_name().str);

  EXPECT_EQ(h1->get_histogram_type(), h2->get_histogram_type());
  EXPECT_EQ(h1->get_data_type(), h2->get_data_type());
  EXPECT_EQ(h1->get_num_buckets(), h2->get_num_buckets());
  EXPECT_EQ(h1->get_num_buckets_specified(), h2->get_num_buckets_specified());
  EXPECT_EQ(h1->get_num_distinct_values(), h2->get_num_distinct_values());
  EXPECT_EQ(h1->get_non_null_values_fraction(),
            h2->get_non_null_values_fraction());
  EXPECT_EQ(h1->get_null_values_fraction(), h2->get_null_values_fraction());
  EXPECT_EQ(h1->get_character_set()->number, h2->get_character_set()->number);
  EXPECT_EQ(h1->get_sampling_rate(), h2->get_sampling_rate());

  ASSERT_EQ(h1->get_num_buckets(), h2->get_num_buckets());
  for (size_t i = 0; i < h1->get_num_buckets(); ++i) {
    EquiHeightBucketsAreEqual(h1->get_buckets()[i], h2->get_buckets()[i]);
  }
}

/*
  Serialize and deserialize the given histogram.
  Verify that the deserialized histogram matches the original.
*/
template <typename T>
void VerifyEquiHeightSerialization(MEM_ROOT *mem_root,
                                   const Equi_height<T> *histogram) {
  // Serialization.
  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  // Deserialization.
  Error_context ctx;
  Histogram *deserialized_histogram = Histogram::json_to_histogram(
      mem_root, "db1", "tbl1", "col1", json_object, &ctx);
  ASSERT_TRUE(deserialized_histogram != nullptr);
  Equi_height<T> *deserialized_equi_height =
      dynamic_cast<Equi_height<T> *>(deserialized_histogram);
  ASSERT_TRUE(deserialized_equi_height != nullptr);

  EquiHeightHistogramsAreEqual(histogram, deserialized_equi_height);
}

/*
  Verify that histogram selectivity estimates for the values in the value_map
  are within max_abs_error of the actual selectivities.
*/
template <typename T>
void VerifyEquiHeightSelectivities(const Value_map<T> &value_map,
                                   const Equi_height<T> *histogram,
                                   double max_error_factor) {
  double max_abs_error =
      max_error_factor /
      static_cast<double>(histogram->get_num_buckets_specified());

  ha_rows non_null_values = 0;
  for (const auto &[value, count] : value_map) non_null_values += count;
  ha_rows total_values = non_null_values + value_map.get_num_null_values();

  ha_rows cumulative_values = 0;
  for (const auto &[value, count] : value_map) {
    double less_than_selectivity =
        static_cast<double>(cumulative_values) / total_values;
    EXPECT_NEAR(less_than_selectivity,
                histogram->get_less_than_selectivity(value), max_abs_error);

    double equal_to_selectivity = static_cast<double>(count) / total_values;
    EXPECT_NEAR(equal_to_selectivity,
                histogram->get_equal_to_selectivity(value), max_abs_error);

    double greater_than_selectivity =
        1.0 - (less_than_selectivity + equal_to_selectivity);
    EXPECT_NEAR(greater_than_selectivity,
                histogram->get_greater_than_selectivity(value), max_abs_error);

    cumulative_values += count;
  }

  double null_fraction =
      static_cast<double>(value_map.get_num_null_values()) / total_values;
  double non_null_fraction =
      static_cast<double>(non_null_values) / total_values;

  const double null_fraction_max_error = 1.0e-9;
  EXPECT_NEAR(histogram->get_null_values_fraction(), null_fraction,
              null_fraction_max_error);
  EXPECT_NEAR(histogram->get_non_null_values_fraction(), non_null_fraction,
              null_fraction_max_error);
}

/*
  The json type that we expect histogram values (bucket endpoints) of a given
  type to be serialized into.
*/
enum_json_type ValueMapTypeToJsonType(Value_map_type value_type) {
  switch (value_type) {
    case Value_map_type::INVALID:
      return enum_json_type::J_ERROR;
    case Value_map_type::STRING:
      return enum_json_type::J_OPAQUE;
    case Value_map_type::INT:
      return enum_json_type::J_INT;
    case Value_map_type::UINT:
      return enum_json_type::J_UINT;
    case Value_map_type::DOUBLE:
      return enum_json_type::J_DOUBLE;
    case Value_map_type::DECIMAL:
      return enum_json_type::J_DECIMAL;
    case Value_map_type::DATE:
      return enum_json_type::J_DATE;
    case Value_map_type::TIME:
      return enum_json_type::J_TIME;
    case Value_map_type::DATETIME:
      return enum_json_type::J_DATETIME;
    case Value_map_type::ENUM:
      return enum_json_type::J_UINT;
    case Value_map_type::SET:
      return enum_json_type::J_UINT;
  }
  return enum_json_type::J_ERROR;
}

template <typename T>
void VerifyEquiHeight(MEM_ROOT *mem_root, const Value_map<T> &value_map,
                      size_t num_buckets, double max_error_factor = 1.0) {
  Equi_height<T> *histogram =
      BuildEquiHeightAndVerifyBasicProperties(mem_root, value_map, num_buckets);
  enum_json_type expected_json_value_type =
      ValueMapTypeToJsonType(histogram->get_data_type());
  VerifyEquiHeightJSONStructure(histogram, expected_json_value_type);
  VerifyEquiHeightSerialization(mem_root, histogram);
  VerifyEquiHeightSelectivities(value_map, histogram, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsInt1) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, int_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsInt2) {
  /*
    Create a value map with the following key/value pairs;
      [NULL, 10000]
      [0,    10000]
      [1,     9999]
      [2,     9998]
      ...
      [9998,     2]
      [9999,     1]
  */
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  values.add_null_values(10000);
  for (longlong i = 0; i < 10000; i++) {
    size_t frequency = static_cast<size_t>(10000 - i);
    values.add_values(i, frequency);
  }

  size_t num_buckets = 10;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsDouble) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, double_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsString) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, string_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsUint) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, uint_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsDecimal) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, decimal_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsDatetime) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, datetime_values, num_buckets, max_error_factor);
}

TEST_F(HistogramsTest, VerifyEquiHeightContentsBlob) {
  size_t num_buckets = 3;
  double max_error_factor = 1.0;
  VerifyEquiHeight(&m_mem_root, blob_values, num_buckets, max_error_factor);
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsDouble) {
  Singleton<double> *histogram = Singleton<double>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DOUBLE);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<double> value_map(&my_charset_numeric, Value_map_type::DOUBLE);
  value_map.add_null_values(10);
  value_map.insert(double_values.begin(), double_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  VerifySingletonBucketContentsDouble(json_buckets, 0, (10.0 / 70.0),
                                      std::numeric_limits<double>::lowest());

  VerifySingletonBucketContentsDouble(json_buckets, 1, (20.0 / 70.0), 0.0);

  VerifySingletonBucketContentsDouble(json_buckets, 2, (30.0 / 70.0),
                                      std::numeric_limits<double>::epsilon());

  VerifySingletonBucketContentsDouble(json_buckets, 3, (40.0 / 70.0), 42.0);

  VerifySingletonBucketContentsDouble(json_buckets, 4, (50.0 / 70.0), 43.0);

  VerifySingletonBucketContentsDouble(json_buckets, 5, (60.0 / 70.0),
                                      std::numeric_limits<double>::max());
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsInt) {
  Singleton<longlong> *histogram = Singleton<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<longlong> value_map(&my_charset_numeric, Value_map_type::INT);
  value_map.add_null_values(10);
  value_map.insert(int_values.begin(), int_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  VerifySingletonBucketContentsInt(json_buckets, 0, (10.0 / 80.0),
                                   std::numeric_limits<longlong>::lowest());

  VerifySingletonBucketContentsInt(json_buckets, 1, (20.0 / 80.0), -1LL);

  VerifySingletonBucketContentsInt(json_buckets, 2, (30.0 / 80.0), 0LL);

  VerifySingletonBucketContentsInt(json_buckets, 3, (40.0 / 80.0), 1LL);

  VerifySingletonBucketContentsInt(json_buckets, 4, (50.0 / 80.0), 42LL);

  VerifySingletonBucketContentsInt(json_buckets, 5, (60.0 / 80.0), 10000LL);

  VerifySingletonBucketContentsInt(json_buckets, 6, (70.0 / 80.0),
                                   std::numeric_limits<longlong>::max());
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsUInt) {
  Singleton<ulonglong> *histogram = Singleton<ulonglong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::UINT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<ulonglong> value_map(&my_charset_numeric, Value_map_type::UINT);
  value_map.add_null_values(10);
  value_map.insert(uint_values.begin(), uint_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  VerifySingletonBucketContentsUInt(json_buckets, 0, (10.0 / 60.0),
                                    std::numeric_limits<ulonglong>::lowest());

  VerifySingletonBucketContentsUInt(json_buckets, 1, (20.0 / 60.0), 42ULL);

  VerifySingletonBucketContentsUInt(json_buckets, 2, (30.0 / 60.0), 43ULL);

  VerifySingletonBucketContentsUInt(json_buckets, 3, (40.0 / 60.0), 10000ULL);

  VerifySingletonBucketContentsUInt(json_buckets, 4, (50.0 / 60.0),
                                    std::numeric_limits<ulonglong>::max());
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsString) {
  Singleton<String> *histogram = Singleton<String>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::STRING);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<String> value_map(&my_charset_latin1, Value_map_type::STRING);
  value_map.add_null_values(10);
  value_map.insert(string_values.begin(), string_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  String string1("", &my_charset_latin1);
  String string2("string1", &my_charset_latin1);
  String string3("string2", &my_charset_latin1);
  String string4("string3", &my_charset_latin1);
  String string5("string4", &my_charset_latin1);

  VerifySingletonBucketContentsString(json_buckets, 0, (10.0 / 60.0), string1,
                                      &my_charset_latin1);
  VerifySingletonBucketContentsString(json_buckets, 1, (20.0 / 60.0), string2,
                                      &my_charset_latin1);
  VerifySingletonBucketContentsString(json_buckets, 2, (30.0 / 60.0), string3,
                                      &my_charset_latin1);
  VerifySingletonBucketContentsString(json_buckets, 3, (40.0 / 60.0), string4,
                                      &my_charset_latin1);
  VerifySingletonBucketContentsString(json_buckets, 4, (50.0 / 60.0), string5,
                                      &my_charset_latin1);
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsDecimal) {
  Singleton<my_decimal> *histogram = Singleton<my_decimal>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DECIMAL);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<my_decimal> value_map(&my_charset_latin1, Value_map_type::DECIMAL);
  value_map.add_null_values(10);
  value_map.insert(decimal_values.begin(), decimal_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  my_decimal decimal1;
  int2my_decimal(E_DEC_FATAL_ERROR, -1000LL, false, &decimal1);

  my_decimal decimal2;
  int2my_decimal(E_DEC_FATAL_ERROR, 0LL, false, &decimal2);

  my_decimal decimal3;
  int2my_decimal(E_DEC_FATAL_ERROR, 1LL, false, &decimal3);

  my_decimal decimal4;
  int2my_decimal(E_DEC_FATAL_ERROR, 42LL, false, &decimal4);

  my_decimal decimal5;
  int2my_decimal(E_DEC_FATAL_ERROR, 1000LL, false, &decimal5);

  VerifySingletonBucketContentsDecimal(json_buckets, 0, (10.0 / 60.0),
                                       decimal1);
  VerifySingletonBucketContentsDecimal(json_buckets, 1, (20.0 / 60.0),
                                       decimal2);
  VerifySingletonBucketContentsDecimal(json_buckets, 2, (30.0 / 60.0),
                                       decimal3);
  VerifySingletonBucketContentsDecimal(json_buckets, 3, (40.0 / 60.0),
                                       decimal4);
  VerifySingletonBucketContentsDecimal(json_buckets, 4, (50.0 / 60.0),
                                       decimal5);
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsDateTime) {
  Singleton<MYSQL_TIME> *histogram = Singleton<MYSQL_TIME>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::DATETIME);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<MYSQL_TIME> value_map(&my_charset_latin1, Value_map_type::DATETIME);
  value_map.add_null_values(10);
  value_map.insert(datetime_values.begin(), datetime_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  MYSQL_TIME time1;
  TIME_from_longlong_datetime_packed(&time1, 914866242077065216);

  MYSQL_TIME time2;
  TIME_from_longlong_datetime_packed(&time2, 914866242077065217);

  MYSQL_TIME time3;
  TIME_from_longlong_datetime_packed(&time3, 1845541820734373888);

  MYSQL_TIME time4;
  TIME_from_longlong_datetime_packed(&time4, 9147936188962652734);

  MYSQL_TIME time5;
  TIME_from_longlong_datetime_packed(&time5, 9147936188962652735);

  VerifySingletonBucketContentsTemporal(json_buckets, 0, (10.0 / 60.0), time1);
  VerifySingletonBucketContentsTemporal(json_buckets, 1, (20.0 / 60.0), time2);
  VerifySingletonBucketContentsTemporal(json_buckets, 2, (30.0 / 60.0), time3);
  VerifySingletonBucketContentsTemporal(json_buckets, 3, (40.0 / 60.0), time4);
  VerifySingletonBucketContentsTemporal(json_buckets, 4, (50.0 / 60.0), time5);
}

/*
  Create a singleton histogram, where we manually verify the value for every
  property in every bucket.
*/
TEST_F(HistogramsTest, VerifySingletonContentsBlob) {
  Singleton<String> *histogram = Singleton<String>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::STRING);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_STREQ(histogram->get_database_name().str, "db1");
  EXPECT_STREQ(histogram->get_table_name().str, "tbl1");
  EXPECT_STREQ(histogram->get_column_name().str, "col1");

  Value_map<String> value_map(&my_charset_bin, Value_map_type::STRING);
  value_map.add_null_values(10);
  value_map.insert(blob_values.begin(), blob_values.end());

  EXPECT_FALSE(histogram->build_histogram(value_map, value_map.size()));
  EXPECT_EQ(histogram->get_num_buckets(), value_map.size());
  EXPECT_EQ(histogram->get_num_distinct_values(), value_map.size());

  Json_object json_object;
  EXPECT_FALSE(histogram->histogram_to_json(&json_object));

  Json_dom *buckets_dom = json_object.get("buckets");
  Json_array *json_buckets = static_cast<Json_array *>(buckets_dom);

  String blob1(blob_buf1, 4, &my_charset_bin);
  String blob2("bar", &my_charset_bin);
  String blob3("foo", &my_charset_bin);
  String blob4("foobar", &my_charset_bin);
  String blob5(blob_buf2, 4, &my_charset_bin);

  VerifySingletonBucketContentsString(json_buckets, 0, (10.0 / 60.0), blob1,
                                      &my_charset_bin);
  VerifySingletonBucketContentsString(json_buckets, 1, (20.0 / 60.0), blob2,
                                      &my_charset_bin);
  VerifySingletonBucketContentsString(json_buckets, 2, (30.0 / 60.0), blob3,
                                      &my_charset_bin);
  VerifySingletonBucketContentsString(json_buckets, 3, (40.0 / 60.0), blob4,
                                      &my_charset_bin);
  VerifySingletonBucketContentsString(json_buckets, 4, (50.0 / 60.0), blob5,
                                      &my_charset_bin);
}

/*
  Create an equi-height histogram with zero buckets sepcified. Ensure that the
  resulting histogram actually have zero buckets.
*/
TEST_F(HistogramsTest, EmptyEquiHeightHistogram) {
  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  Value_map<longlong> empty_value_map(&my_charset_numeric, Value_map_type::INT);

  // Empty map, no null values, but several buckets specified.
  EXPECT_FALSE(histogram->build_histogram(empty_value_map, 10U));
  EXPECT_EQ(histogram->get_num_buckets(), 0U);
  EXPECT_EQ(histogram->get_num_distinct_values(), 0U);

  // Empty map, multiple null values and several buckets specified.
  empty_value_map.add_null_values(500);
  EXPECT_FALSE(histogram->build_histogram(empty_value_map, 10U));
  EXPECT_EQ(histogram->get_num_buckets(), 0U);
  EXPECT_EQ(histogram->get_num_distinct_values(), 0U);
}

/*
  Create a singleton histogram from an empty value map. Ensure that the
  resulting histogram actually have zero buckets.
*/
TEST_F(HistogramsTest, EmptySingletonHistogram) {
  Singleton<longlong> *histogram = Singleton<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  Value_map<longlong> empty_value_map(&my_charset_numeric, Value_map_type::INT);

  // Empty map, no null values,
  EXPECT_FALSE(histogram->build_histogram(empty_value_map, 10U));
  EXPECT_EQ(histogram->get_num_buckets(), 0U);
  EXPECT_EQ(histogram->get_num_distinct_values(), 0U);
}

/*
  Create an equi-height histogram from an empty value map, but with several NULL
  values. Check that the resulting histogram has a fraction of NULL values equal
  to 1.0.
*/
TEST_F(HistogramsTest, EquiHeightNullValues) {
  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  Value_map<longlong> empty_value_map(&my_charset_numeric, Value_map_type::INT);
  empty_value_map.add_null_values(10);

  EXPECT_FALSE(histogram->build_histogram(empty_value_map, 1U));
  EXPECT_DOUBLE_EQ(histogram->get_null_values_fraction(), 1.0);
}

/*
  Create a singleton histogram from an empty value map, but with several NULL
  values. Check that the resulting histogram has a fraction of NULL values equal
  to 1.0.
*/
TEST_F(HistogramsTest, SingletonNullValues) {
  Singleton<longlong> *histogram = Singleton<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  Value_map<longlong> empty_value_map(&my_charset_numeric, Value_map_type::INT);
  empty_value_map.add_null_values(10);

  EXPECT_FALSE(histogram->build_histogram(empty_value_map, 10U));
  EXPECT_DOUBLE_EQ(histogram->get_null_values_fraction(), 1.0);
}

/*
  Check that the histogram comparator only checks the 42 first characters of
  long string values. If the strings differ at any character after the 42nd
  character, the strings should be considered equal.

  This does not test any histogram per se, but the histogram comparator.
*/
TEST_F(HistogramsTest, LongStringValues) {
  /*
    Ensure that HISTOGRAM_MAX_COMPARE_LENGTH is set to the value we have assumed
    throughout this test.
  */
  EXPECT_EQ(42U, HISTOGRAM_MAX_COMPARE_LENGTH);

  Value_map<String> long_strings(&my_charset_latin1, Value_map_type::STRING);

  /*
    The following three strings should be considered equal, since the 42 first
    characters are equal.
  */
  String string1("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop0000",
                 &my_charset_latin1);

  String string2("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop2222",
                 &my_charset_latin1);

  String string3("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop1111",
                 &my_charset_latin1);

  /*
    The following three strings should be considered different, since they
    differ at the 42nd character
  */
  String string4("abcdefghijklmnopqrstuvwxyzabcdefghijklmno2222",
                 &my_charset_latin1);

  String string5("abcdefghijklmnopqrstuvwxyzabcdefghijklmno1111",
                 &my_charset_latin1);

  String string6("abcdefghijklmnopqrstuvwxyzabcdefghijklmno0000",
                 &my_charset_latin1);

  long_strings.add_values(string1, 10);
  long_strings.add_values(string2, 10);
  long_strings.add_values(string3, 10);
  long_strings.add_values(string4, 10);
  long_strings.add_values(string5, 10);
  long_strings.add_values(string6, 10);

  EXPECT_EQ(4U, long_strings.size());
}

/*
  Check that the histogram comparator only checks the 42 first bytes of long
  binary values. If the values differ at any byte after the 42nd byte, the
  binary values should be considered equal.

  This does not test any histogram per se, but the histogram comparator.
*/
TEST_F(HistogramsTest, LongBlobValues) {
  /*
    Ensure that HISTOGRAM_MAX_COMPARE_LENGTH is set to the value we have assume
    throughout this test.
  */
  EXPECT_EQ(42U, HISTOGRAM_MAX_COMPARE_LENGTH);

  Value_map<String> long_blobs(&my_charset_bin, Value_map_type::STRING);

  /*
    The following three blobs should be considered equal, since the 42 first
    bytes are equal.
  */
  const char buf1[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 42, 2,  2,  2,  2};

  const char buf2[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 42, 1,  1,  1,  1};

  const char buf3[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 42, 0,  0,  0,  0};

  /*
    The following three blobs should be considered different, since they differ
    at the 42nd byte
  */
  const char buf4[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 2,  2,  2,  2,  2};

  const char buf5[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 1,  1,  1,  1,  1};

  const char buf6[46] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12,
                         13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
                         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36,
                         37, 38, 39, 40, 41, 0,  0,  0,  0,  0};

  long_blobs.add_values(String(buf1, 46, &my_charset_bin), 10);
  long_blobs.add_values(String(buf2, 46, &my_charset_bin), 10);
  long_blobs.add_values(String(buf3, 46, &my_charset_bin), 10);
  long_blobs.add_values(String(buf4, 46, &my_charset_bin), 10);
  long_blobs.add_values(String(buf5, 46, &my_charset_bin), 10);
  long_blobs.add_values(String(buf6, 46, &my_charset_bin), 10);

  EXPECT_EQ(4U, long_blobs.size());
}

/*
  Check that the histogram comparator only checks the 42 first characters of
  long string values, where the strings are multi-byte strings. If the strings
  differ at any character after the 42nd character, the strings should be
  considered equal.

  This does not test any histogram per se, but the histogram comparator.
*/
TEST_F(HistogramsTest, MultiByteStrings) {
  /*
    Ensure that HISTOGRAM_MAX_COMPARE_LENGTH is set to the value we have assumed
    throughout this test.
  */
  EXPECT_EQ(42U, HISTOGRAM_MAX_COMPARE_LENGTH);

  /*
    Declare the strings to have UCS2 character set, which is fixed 2 byte per
    character.
  */
  MY_CHARSET_LOADER loader;
  CHARSET_INFO *cs =
      my_collation_get_by_name(&loader, "ucs2_general_ci", MYF(0));

  Value_map<String> long_strings(cs, Value_map_type::STRING);

  String string1("", cs);
  String string2("", cs);
  String string3("", cs);
  String string4("", cs);
  String string5("", cs);
  String string6("", cs);

  /*
    The following three strings should be considered equal, since the 42 first
    characters are equal.
  */
  string1.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop2222", 46,
                 &my_charset_latin1);
  string2.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop1111", 46,
                 &my_charset_latin1);
  string3.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmnop0000", 46,
                 &my_charset_latin1);
  /*
    The following three strings should be considered different, since they
    differ at the 42nd character
  */
  string4.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmno22222", 46,
                 &my_charset_latin1);
  string5.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmno11111", 46,
                 &my_charset_latin1);
  string6.append("abcdefghijklmnopqrstuvwxyzabcdefghijklmno00000", 46,
                 &my_charset_latin1);

  /*
    Since we are using UCS-2, we should have twice the amount of bytes as we
    have characters.
  */
  EXPECT_EQ(string6.numchars(), 46U);
  EXPECT_EQ(string6.length(), 92U);

  long_strings.add_values(string1, 10);
  long_strings.add_values(string2, 10);
  long_strings.add_values(string3, 10);
  long_strings.add_values(string4, 10);
  long_strings.add_values(string5, 10);
  long_strings.add_values(string6, 10);

  EXPECT_EQ(4U, long_strings.size());
}

/*
  Build an equi-height histogram with a significant amount of distinct values.
*/
TEST_F(HistogramsTest, BigEquiHeight) {
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  values.add_null_values(514);
  for (longlong i = 0; i < 100000; i++) {
    size_t frequency = static_cast<size_t>((rand() % 10000) + 1);
    values.add_values(i, frequency);
  }

  Equi_height<longlong> *histogram = Equi_height<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_EQ(0U, histogram->get_num_buckets());
  EXPECT_EQ(0U, histogram->get_num_distinct_values());

  // Build a histogram with 200 buckets.
  size_t num_buckets = 200;
  EXPECT_FALSE(histogram->build_histogram(values, num_buckets));
  EXPECT_LE(histogram->get_num_buckets(), num_buckets);
  EXPECT_EQ(100000U, histogram->get_num_distinct_values());

  VerifyEquiHeightJSONStructure(histogram, enum_json_type::J_INT);
  VerifyEquiHeightBucketConstraintsInt(histogram);
}

/*
  Build a singleton histogram, and check if the printed time is within a few
  seconds of the current time.

  We do not add any values to the histogram, since we want it to be built as
  fast as possible.
*/
TEST_F(HistogramsTest, HistogramTimeCreated) {
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);

  Singleton<longlong> *histogram = Singleton<longlong>::create(
      &m_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  ASSERT_TRUE(histogram != nullptr);

  EXPECT_EQ(0U, histogram->get_num_buckets());
  EXPECT_EQ(0U, histogram->get_num_distinct_values());

  EXPECT_FALSE(histogram->build_histogram(values, 10U));

  // Get the current time in GMT timezone.
  MYSQL_TIME current_time;
  ulonglong micro_time = my_micro_time();
  my_tz_UTC->gmt_sec_to_TIME(&current_time,
                             static_cast<my_time_t>(micro_time / 1000000));

  Json_object json_histogram;
  EXPECT_FALSE(histogram->histogram_to_json(&json_histogram));

  Json_dom *last_updated_dom = json_histogram.get("last-updated");
  Json_datetime *last_updated = down_cast<Json_datetime *>(last_updated_dom);

  longlong seconds_diff = 0;
  long microseconds_diff = 0;
  calc_time_diff(*last_updated->value(), current_time, 1, &seconds_diff,
                 &microseconds_diff);

  EXPECT_LE(seconds_diff, 2LL);
}

/*
  Check that an out-of-memory situation doesn't crash brutally, but fails
  gracefully.
*/
TEST_F(HistogramsTest, HistogramOOM) {
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  values.add_values(1, 10);
  values.add_values(2, 10);
  values.add_values(3, 10);
  values.add_values(4, 10);

  MEM_ROOT oom_mem_root(PSI_NOT_INSTRUMENTED, 32);

  /*
    Restrict the maximum capacity of the MEM_ROOT so it cannot grow anymore. But
    don't set it to 0, as this means "unlimited".
  */
  oom_mem_root.set_max_capacity(4);

  Histogram *histogram = nullptr;

  // Force an equi-height (num_buckets < num_distinct_values)
  histogram = build_histogram(&oom_mem_root, values, 1U, "db1", "tbl1", "col1");
  EXPECT_EQ(histogram, nullptr);

  // Force a singleton (num_buckets >= num_distinct_values)
  histogram =
      build_histogram(&oom_mem_root, values, 10U, "db1", "tbl1", "col1");
  EXPECT_EQ(histogram, nullptr);
}

/*
  Check that an out-of-memory situation doesn't crash brutally, but fails
  gracefully.
*/
TEST_F(HistogramsTest, EquiHeightOOM) {
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  values.add_values(1, 10);
  values.add_values(2, 10);
  values.add_values(3, 10);
  values.add_values(4, 10);

  MEM_ROOT oom_mem_root(PSI_NOT_INSTRUMENTED, 128);

  {
    /*
      Create the histogram in a new scope so that the underlying structures
      are freed before the MEM_ROOT.
    */
    Equi_height<longlong> *histogram = Equi_height<longlong>::create(
        &oom_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
    ASSERT_TRUE(histogram != nullptr);

    // Restrict the maximum capacity of the MEM_ROOT so it cannot grow anymore.
    oom_mem_root.set_max_capacity(oom_mem_root.allocated_size());
    EXPECT_TRUE(histogram->build_histogram(values, 10U));
  }
}

/*
  Check that the Equi_height factory method returns a nullptr if it runs out of
  memory during construction.
*/
TEST_F(HistogramsTest, EquiHeightCreationOOM) {
  // Successfully allocate a histogram on a MEM_ROOT.
  MEM_ROOT not_oom_mem_root(PSI_NOT_INSTRUMENTED, 128);
  Equi_height<longlong> *not_oom_histogram = Equi_height<longlong>::create(
      &not_oom_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  EXPECT_TRUE(not_oom_histogram != nullptr);

  // Create a new MEM_ROOT and fix its capacity.
  MEM_ROOT fixed_capacity_mem_root(PSI_NOT_INSTRUMENTED, 128);
  fixed_capacity_mem_root.set_max_capacity(not_oom_mem_root.allocated_size());

  // Verify that the same allocation does not fail when we fix the capacity.
  Equi_height<longlong> *not_oom_histogram2 = Equi_height<longlong>::create(
      &fixed_capacity_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  EXPECT_TRUE(not_oom_histogram2 != nullptr);

  // Allocate a histogram with long strings leading to an OOM error during
  // construction (not when allocating space for the histogram itself).
  MEM_ROOT fixed_capacity_mem_root2(PSI_NOT_INSTRUMENTED, 128);
  fixed_capacity_mem_root2.set_max_capacity(not_oom_mem_root.allocated_size());
  std::string long_string(1000, 'x');  // A string of length 1000.
  Equi_height<longlong> *oom_histogram = Equi_height<longlong>::create(
      &fixed_capacity_mem_root2, long_string, long_string, long_string,
      Value_map_type::INT);
  EXPECT_TRUE(oom_histogram == nullptr);
}

/*
  Check that an out-of-memory situation doesn't crash brutally, but fails
  gracefully. We need to add more than a few buckets to the default-initialized
  vector holding the buckets in order to trigger an allocation.
*/
TEST_F(HistogramsTest, SingletonOOM) {
  Value_map<longlong> values(&my_charset_numeric, Value_map_type::INT);
  size_t num_buckets = 100;
  for (longlong i = 0; i < static_cast<longlong>(num_buckets); ++i) {
    values.add_values(i, 10);
  }
  MEM_ROOT oom_mem_root(PSI_NOT_INSTRUMENTED, 128);

  {
    /*
      Create the histogram in a new scope so that the underlying structures
      are freed before the MEM_ROOT.
    */
    Singleton<longlong> *histogram = Singleton<longlong>::create(
        &oom_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
    ASSERT_TRUE(histogram != nullptr);

    // Restrict the maximum capacity of the MEM_ROOT so it cannot grow anymore.
    oom_mem_root.set_max_capacity(oom_mem_root.allocated_size());
    EXPECT_TRUE(histogram->build_histogram(values, num_buckets));
  }
}

/*
  Check that the Singleton histogram factory method returns a nullptr if it runs
  out of memory during construction.
*/
TEST_F(HistogramsTest, SingletonCreationOOM) {
  // Successfully allocate a histogram on a MEM_ROOT.
  MEM_ROOT not_oom_mem_root(PSI_NOT_INSTRUMENTED, 128);
  Singleton<longlong> *not_oom_histogram = Singleton<longlong>::create(
      &not_oom_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  EXPECT_TRUE(not_oom_histogram != nullptr);

  // Create a new MEM_ROOT and fix its capacity.
  MEM_ROOT fixed_capacity_mem_root(PSI_NOT_INSTRUMENTED, 128);
  fixed_capacity_mem_root.set_max_capacity(not_oom_mem_root.allocated_size());

  // Verify that the same allocation does not fail when we fix the capacity.
  Singleton<longlong> *not_oom_histogram2 = Singleton<longlong>::create(
      &fixed_capacity_mem_root, "db1", "tbl1", "col1", Value_map_type::INT);
  EXPECT_TRUE(not_oom_histogram2 != nullptr);

  // Allocate a histogram with long strings leading to an OOM error during
  // construction (not when allocating space for the histogram itself).
  MEM_ROOT fixed_capacity_mem_root2(PSI_NOT_INSTRUMENTED, 128);
  fixed_capacity_mem_root2.set_max_capacity(not_oom_mem_root.allocated_size());
  std::string long_string(1000, 'x');  // A string of length 1000.
  Singleton<longlong> *oom_histogram = Singleton<longlong>::create(
      &fixed_capacity_mem_root2, long_string, long_string, long_string,
      Value_map_type::INT);
  EXPECT_TRUE(oom_histogram == nullptr);
}

}  // namespace histograms_unittest
