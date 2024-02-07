/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file sql/histograms/equi_height.cc
  Equi-height histogram (implementation).
*/

#include "sql/histograms/equi_height.h"

#include <stdlib.h>
#include <algorithm>  // std::is_sorted
#include <cmath>      // std::lround
#include <iterator>
#include <new>

#include "my_base.h"  // ha_rows
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql_time.h"
#include "sql-common/json_dom.h"  // Json_*
#include "sql/histograms/equi_height_bucket.h"
#include "sql/histograms/histogram_utility.h"  // DeepCopy
#include "sql/histograms/value_map.h"          // Value_map
#include "sql/mem_root_allocator.h"
#include "sql_string.h"
#include "template_utils.h"

class my_decimal;
struct MEM_ROOT;

namespace histograms {

// Private constructor
template <class T>
Equi_height<T>::Equi_height(MEM_ROOT *mem_root, const std::string &db_name,
                            const std::string &tbl_name,
                            const std::string &col_name,
                            Value_map_type data_type, bool *error)
    : Histogram(mem_root, db_name, tbl_name, col_name,
                enum_histogram_type::EQUI_HEIGHT, data_type, error),
      m_buckets(mem_root) {}

// Public factory method
template <class T>
Equi_height<T> *Equi_height<T>::create(MEM_ROOT *mem_root,
                                       const std::string &db_name,
                                       const std::string &tbl_name,
                                       const std::string &col_name,
                                       Value_map_type data_type) {
  bool error = false;
  Equi_height<T> *equi_height = new (mem_root)
      Equi_height<T>(mem_root, db_name, tbl_name, col_name, data_type, &error);
  if (error) return nullptr;
  return equi_height;
}

template <class T>
Equi_height<T>::Equi_height(MEM_ROOT *mem_root, const Equi_height<T> &other,
                            bool *error)
    : Histogram(mem_root, other, error), m_buckets(mem_root) {
  if (m_buckets.reserve(other.m_buckets.size())) {
    *error = true;
    return;
  }
  for (const equi_height::Bucket<T> &other_bucket : other.m_buckets) {
    equi_height::Bucket<T> bucket(
        DeepCopy(other_bucket.get_lower_inclusive(), mem_root, error),
        DeepCopy(other_bucket.get_upper_inclusive(), mem_root, error),
        other_bucket.get_cumulative_frequency(),
        other_bucket.get_num_distinct());
    if (*error) return;
    m_buckets.push_back(bucket);
  }
}

/*
  Returns true if the greedy equi-height histogram construction algorithm can
  successfully fit the provided value_map into a histogram with at most
  max_buckets of size at most max_bucket_values. This function does not actually
  build a histogram, but is used as a step to find the right bucket size.
*/
template <class T>
static bool FitsIntoBuckets(const Value_map<T> &value_map,
                            ha_rows max_bucket_values, size_t max_buckets) {
  assert(value_map.size() > 0);
  size_t used_buckets = 1;
  ha_rows current_bucket_values = 0;

  for (const auto &[value, count] : value_map) {
    assert(count > 0);
    /*
      If the current bucket is not empty and adding the values causes it to
      exceed its max size, add the values to a new bucket instead.
      Note that we allow the size of singleton buckets (buckets with only one
      distinct value) to exceed max_bucket_values.
    */
    if (current_bucket_values > 0 &&
        current_bucket_values + count > max_bucket_values) {
      ++used_buckets;
      current_bucket_values = 0;
    }
    current_bucket_values += count;

    // Terminate early if we have used too many buckets.
    if (used_buckets > max_buckets) return false;
  }
  return true;
}

/*
  Performs a binary search to find the smallest possible bucket size that will
  allow us to greedily construct a histogram with at most max_buckets buckets.

  Important properties of the greedy construction algorithm:

  See the comment above build_histogram() for a description of the algorithm.

  Let M denote the total number of values in the value_map and assume for
  simplicity that max_buckets is an even number. Fractions are rounded up to the
  nearest integer. Buckets are composite if they contain more than one distinct
  value.

  Property (1)
  The histogram fits into N buckets with a composite size of at most K = 2M/N.

  Proof sketch (1)
  Consider the first pair of buckets. If the first bucket contains K - c values,
  then the second bucket is guaranteed to contain at least c values, otherwise
  the greedy construction algorithm would have placed the additional c values in
  the first bucket as well. Thus, every pair of buckets together contain at
  least K = 2M/N rows, and there are N/2 successive pairs of buckets. Therefore,
  the first N buckets contain at least (N/2) * (2M/N) = M values and the
  histogram fits into N buckets.

  Property (2)
  Increasing the maximum allowed composite bucket size can never result in a
  histogram with more buckets. I.e., the number of buckets is non-increasing
  in the max composite bucket size.

  The first property ensure that we have a reasonable upper bound when searching
  for the bucket size. The second property ensures that we can reason about
  ranges of bucket sizes when performing our search. For example, if we cannot
  fit a histogram using a bucket size of K, then it will not work with a bucket
  size of K' < K either.
*/
template <class T>
static ha_rows FindBucketMaxValues(const Value_map<T> &value_map,
                                   size_t max_buckets) {
  ha_rows total_values = 0;
  for (const auto &[value, count] : value_map) total_values += count;
  if (max_buckets == 1) return total_values;

  // Conservative upper bound to avoid dealing with rounding and odd max_buckets
  ha_rows upper_bucket_values = 2 * total_values / (max_buckets - 1) + 1;
  assert(FitsIntoBuckets(value_map, upper_bucket_values, max_buckets));
  ha_rows lower_bucket_values = 0;

  const int max_search_steps = 10;
  int search_step = 0;
  while (upper_bucket_values > lower_bucket_values + 1 &&
         search_step < max_search_steps) {
    ha_rows bucket_values = (upper_bucket_values + lower_bucket_values) / 2;
    if (FitsIntoBuckets(value_map, bucket_values, max_buckets)) {
      upper_bucket_values = bucket_values;
    } else {
      lower_bucket_values = bucket_values;
    }
    ++search_step;
  }

  return upper_bucket_values;
}

/*
  Returns an estimate of the number of distinct values in a histogram bucket
  when the histogram is based on sampling.

  We use the Guaranteed Error Estimator (GEE) from [1]. Let s denote the
  sampling rate, d the number of distinct values in the sample, and u the number
  of distinct values that appear only once in the sample. Then,

                        GEE = sqrt(1/s)*u + d - u.

  The intuition behind the GEE estimator is that we can divide the dataset into
  "high frequency" and "low frequency" values. High frequency values are those
  d - u values that appear at least twice in the sample. The contribution to the
  estimated number of distinct values from the high frequency values will not
  increase, even if increase the sample size. The low frequency values are the
  u values that appeared only once in the sample. The final contribution of the
  low frequency values can be between u and (1/s)*u. In order to minimize the
  worst-case relative error, we use the geometric mean of these two values.

  Important note:

  This estimator was designed for uniform random sampling. We currently use
  page-level sampling for histograms. This can cause us to underestimate the
  number of distinct values by nearly a factor 1/s in the worst case. The
  reason is that we only scale up the number of singleton values.
  With page-level sampling we can have pairs of distinct values occuring
  together so that we will have u=0 in the formula above.

  For now, we opt to keep the formula as it is, since we would rather
  underestimate than overestimate the number of distinct values. Potential
  solutions:

  1) Use a custom estimator for page-level sampling [3]. This requires changes
     to the sampling interface to InnoDB to support counting the number of pages
     a value appears in.

  2) Use the simpler estimate of sqrt(1/s)*d, the geometric mean between the
     lower bound of d and the upper bound of d/s. This has the downside of
     overestimating the number of distinct values by sqrt(1/s) in cases where
     the table only contains heavy hitters.

  3) Simulate uniform random sampling on top of the page-level sampling.
     Postgres does this, but it requires sampling as many pages as the target
     number of rows.

   Further considerations:

  It turns out that estimating the number of distinct values is a difficult
  problem. In [1] it is shown that for any estimator based on random sampling
  with a sampling rate of s there exists a data set such that with probability p
  the estimator is off by a factor at least ((1/s) * ln(1/p))^0.5. For a
  sampling rate of s = 0.01 and an error probability of 1/e this means the
  estimate could be off by a factor 10 about 1/3 of the time.

  We are currently using the distinct values estimates for providing selectivity
  estimates for equality predicates. The selectivity of a value in a composite
  bucket is estimated to be the total selectivity of the bucket divided by the
  number of distinct values in the bucket. So a larger distinct values estimate
  leads to lower selectivity estimates. In future we might also use histograms
  in estimating the size of joins though. In both cases it seems better to
  overestimate rather than underestimate the selectivity.

  The GEE estimator is designed to minimize the ratio between the estimate and
  actual value. The estimator is simple and relatively conservative in that it
  only scales u by sqrt(1/s) rather than 1/s, so it seems suitable for our use.
  In [1] it is furthermore shown that it performs relatively well on real data.

  If we require more accurate estimates we could consider upgrading to the more
  advanced estimators proposed in [1] or [2]. Since estimation distinct values
  by sampling is inherently prone to large errors [1], we could also consider
  streaming/sketching techniques such as HyperLogLog or Count-Min if we need
  more accuracy. These would require updating a sketch on every table update.

  References:

  [1] Charikar, Moses, et al. "Towards estimation error guarantees for distinct
  values." Proceedings of the nineteenth ACM SIGMOD-SIGACT-SIGART symposium on
  Principles of database systems. 2000.

  [2] Haas, Peter J., et al. "Sampling-based estimation of the number of
  distinct values of an attribute." VLDB. Vol. 95. 1995.

  [3] Chaudhuri, Surajit, Gautam Das, and Utkarsh Srivastava. "Effective use of
  block-level sampling in statistics estimation." Proceedings of the 2004 ACM
  SIGMOD international conference on Management of data. 2004.

*/
static ha_rows EstimateDistinctValues(double sampling_rate,
                                      ha_rows bucket_distinct_values,
                                      ha_rows bucket_unary_values) {
  // Singleton buckets can only contain one distinct value.
  if (bucket_distinct_values == 1) return 1;

  // GEE estimate for non-singleton buckets.
  assert(sampling_rate > 0.0);
  assert(bucket_distinct_values >= bucket_unary_values);
  return static_cast<ha_rows>(
             std::round(std::sqrt(1.0 / sampling_rate) * bucket_unary_values)) +
         bucket_distinct_values - bucket_unary_values;
}

/*
  Greedy equi-height histogram construction algorithm:

  Inputs: An ordered collection of [value, count] pairs and a maximum bucket
  size.

  Create an empty bucket. Proceeding in the order of the collection, insert
  values into the bucket while keeping track of its size.

  If the insertion of a value into a non-empty bucket causes the bucket to
  exceed the maximum size, create a new empty bucket and continue.

  ---

  Guarantees:

  Selectivity estimation error of at most ~2 * #values / #buckets, often less.
  Values with relative frequency exceeding this threshold are guaranteed to be
  placed in singleton buckets.

  Longer description:

  The build_histogram() method takes as input the target number of buckets and
  calls FindBucketMaxValues() to search for the smallest maximum bucket size
  that will cause the histogram to fit into the target number of buckets.
  See the comments on find_max_bucket_values() for more details.

  If we disregard sampling error then the remaining error in selectivity
  estimation stems entirely from buckets that contain more than one distinct
  value (composite buckets). To see this, consider estimating the selectivity
  for e.g. "WHERE x < 5". If the value 5 lies inside a composite bucket, the
  selectivity estimation error can be almost as large as the size of the bucket.

  By constructing histograms with the smallest possible composite bucket size
  we minimize the worst case selectivity estimation error. Our algorithm is
  guaranteed to produce a histogram with a maximum composite bucket size of at
  most 2 * #values / #buckets in the worst case. In general it will adapt to the
  data distribution to minimize the size of composite buckets. This property is
  particularly beneficial for distributions that are concentrated on a few
  highly frequent values. The heavy values can be placed in singleton buckets
  and the algorithm will attempt to spread the remaining values evenly across
  the remaining buckets, leading to a lower composite bucket size.

  Note on terminology:

  The term "value" primarily refers to an entry/cell in a column. "value" is
  also used to refer to the actual value of an entry, causing some confusion.
  We try to use the term distinct value to refer the value of an entry.
  The Value_map is an ordered collection of [distinct value, value count] pairs.
  For example, a Value_map<String> could contain the pairs ["a", 1], ["b", 2] to
  represent one "a" value and two "b" values.
*/
template <class T>
bool Equi_height<T>::build_histogram(const Value_map<T> &value_map,
                                     size_t num_buckets) {
  assert(num_buckets > 0);
  if (num_buckets < 1) return true; /* purecov: inspected */

  // Set the number of buckets that was specified/requested by the user.
  m_num_buckets_specified = num_buckets;

  // Clear any existing data.
  m_buckets.clear();
  m_null_values_fraction = INVALID_NULL_VALUES_FRACTION;
  m_sampling_rate = value_map.get_sampling_rate();

  // Set the character set for the histogram contents.
  m_charset = value_map.get_character_set();

  // Get total count of non-null values.
  ha_rows num_non_null_values = 0;
  for (const auto &[value, count] : value_map) num_non_null_values += count;

  // No non-null values, nothing to do.
  if (num_non_null_values == 0) {
    if (value_map.get_num_null_values() > 0)
      m_null_values_fraction = 1.0;
    else
      m_null_values_fraction = 0.0;

    return false;
  }

  // Set the fraction of NULL values.
  const ha_rows total_values =
      value_map.get_num_null_values() + num_non_null_values;

  m_null_values_fraction =
      value_map.get_num_null_values() / static_cast<double>(total_values);

  /*
    Ensure that the capacity is at least num_buckets in order to avoid the
    overhead of additional allocations when inserting buckets.
  */
  if (m_buckets.reserve(num_buckets)) return true;

  const ha_rows bucket_max_values = FindBucketMaxValues(value_map, num_buckets);
  ha_rows cumulative_values = 0;
  ha_rows bucket_values = 0;
  ha_rows bucket_distinct_values = 0;
  ha_rows bucket_unary_values = 0;  // Number of values with a count of one.
  size_t distinct_values_remaining = value_map.size();

  auto freq_it = value_map.begin();
  const T *bucket_lower_value = &freq_it->first;

  for (; freq_it != value_map.end(); ++freq_it) {
    // Add the current distinct value to the current bucket.
    cumulative_values += freq_it->second;
    bucket_values += freq_it->second;
    ++bucket_distinct_values;
    if (freq_it->second == 1) ++bucket_unary_values;
    --distinct_values_remaining;

    /*
      Continue adding the next distinct value to the bucket if:
      (1) We have not reached the last distinct value in the value_map.
      (2) There are more remaining distinct values than empty buckets.
      (3) Adding the value does not cause the bucket to exceed its max size.
    */
    auto next = std::next(freq_it);
    size_t empty_buckets_remaining = num_buckets - m_buckets.size() - 1;
    if (next != value_map.end() &&
        distinct_values_remaining > empty_buckets_remaining &&
        bucket_values + next->second <= bucket_max_values) {
      continue;
    }

    // Finalize the current bucket and add it to our collection of buckets.
    double cumulative_frequency =
        cumulative_values / static_cast<double>(total_values);
    ha_rows bucket_distinct_values_estimate =
        EstimateDistinctValues(value_map.get_sampling_rate(),
                               bucket_distinct_values, bucket_unary_values);

    // Create deep copies of the bucket endpoints to ensure that the values are
    // allocated on the histogram's mem_root.
    bool value_copy_error = false;
    equi_height::Bucket<T> bucket(
        DeepCopy(*bucket_lower_value, get_mem_root(), &value_copy_error),
        DeepCopy(freq_it->first, get_mem_root(), &value_copy_error),
        cumulative_frequency, bucket_distinct_values_estimate);
    if (value_copy_error) return true;

    /*
      In case the histogram construction algorithm unintendedly inserts more
      buckets than we have reserved space for and triggers a reallocation that
      fails, push_back() returns true.
    */
    assert(m_buckets.capacity() > m_buckets.size());
    if (m_buckets.push_back(bucket)) return true;

    /*
      In debug, check that the lower value actually is less than or equal to
      the upper value.
    */
    assert(!Histogram_comparator()(bucket.get_upper_inclusive(),
                                   bucket.get_lower_inclusive()));

    bucket_unary_values = 0;
    bucket_values = 0;
    bucket_distinct_values = 0;
    if (next != value_map.end()) bucket_lower_value = &next->first;
  }

  assert(m_buckets.size() <= num_buckets);
  assert(std::is_sorted(m_buckets.begin(), m_buckets.end(),
                        Histogram_comparator()));
  return false;
}

template <class T>
bool Equi_height<T>::histogram_to_json(Json_object *json_object) const {
  /*
    Call the base class implementation first. This will add the properties that
    are common among different histogram types, such as "last-updated" and
    "histogram-type".
  */
  if (Histogram::histogram_to_json(json_object))
    return true; /* purecov: inspected */

  // Add the equi-height buckets.
  Json_array buckets;
  for (const auto &bucket : m_buckets) {
    Json_array json_bucket;
    if (bucket.bucket_to_json(&json_bucket))
      return true; /* purecov: inspected */
    if (buckets.append_clone(&json_bucket))
      return true; /* purecov: inspected */
  }

  if (json_object->add_clone(buckets_str(), &buckets))
    return true; /* purecov: inspected */

  if (histogram_data_type_to_json(json_object))
    return true; /* purecov: inspected */
  return false;
}

template <class T>
std::string Equi_height<T>::histogram_type_to_str() const {
  return equi_height_str();
}

template <class T>
bool Equi_height<T>::json_to_histogram(const Json_object &json_object,
                                       Error_context *context) {
  if (Histogram::json_to_histogram(json_object, context)) return true;

  // if the histogram is internally persisted, it has already been validated
  // and should never have errors, so assert whenever an error is encountered.
  // If it is not already validated, it is a user-defined histogram and it may
  // have errors, which should be detected and reported.
  bool already_validated [[maybe_unused]] = context->binary();

  const Json_dom *buckets_dom = json_object.get(buckets_str());
  assert(!already_validated ||
         (buckets_dom && buckets_dom->json_type() == enum_json_type::J_ARRAY));
  if (buckets_dom == nullptr) {
    context->report_missing_attribute(buckets_str());
    return true;
  }
  if (buckets_dom->json_type() != enum_json_type::J_ARRAY) {
    context->report_node(buckets_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  const Json_array *buckets = down_cast<const Json_array *>(buckets_dom);
  if (m_buckets.reserve(buckets->size())) return true;
  for (size_t i = 0; i < buckets->size(); ++i) {
    const Json_dom *bucket_dom = (*buckets)[i];
    assert(!already_validated ||
           bucket_dom->json_type() == enum_json_type::J_ARRAY);
    if (bucket_dom->json_type() != enum_json_type::J_ARRAY) {
      context->report_node(bucket_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
      return true;
    }

    const Json_array *bucket = down_cast<const Json_array *>(bucket_dom);
    // Only the first four items are defined, others are simply ignored.
    assert(!already_validated || (bucket->size() == 4));
    if (bucket->size() < 4) {
      context->report_node(bucket_dom, Message::JSON_WRONG_BUCKET_TYPE_4);
      return true;
    }
    if (add_bucket_from_json(bucket, context)) return true;
  }
  assert(std::is_sorted(m_buckets.begin(), m_buckets.end(),
                        Histogram_comparator()));

  // Global post-check
  {
    if (m_buckets.empty()) {
      context->report_global(Message::JSON_IMPOSSIBLE_EMPTY_EQUI_HEIGHT);
      return true;
    } else {
      equi_height::Bucket<T> *last_bucket = &m_buckets[m_buckets.size() - 1];
      float sum =
          last_bucket->get_cumulative_frequency() + get_null_values_fraction();
      if (std::abs(sum - 1.0) > 0) {
        context->report_global(Message::JSON_INVALID_TOTAL_FREQUENCY);
        return true;
      }
    }
  }
  return false;
}

template <class T>
bool Equi_height<T>::add_bucket_from_json(const Json_array *json_bucket,
                                          Error_context *context) {
  const Json_dom *cumulative_frequency_dom = (*json_bucket)[2];
  if (cumulative_frequency_dom->json_type() != enum_json_type::J_DOUBLE) {
    context->report_node(cumulative_frequency_dom,
                         Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  const Json_double *cumulative_frequency =
      down_cast<const Json_double *>(cumulative_frequency_dom);

  const Json_dom *num_distinct_dom = (*json_bucket)[3];
  ulonglong num_distinct_v = 0;
  if (num_distinct_dom->json_type() == enum_json_type::J_UINT) {
    num_distinct_v = down_cast<const Json_uint *>(num_distinct_dom)->value();
  } else if (!context->binary() &&
             num_distinct_dom->json_type() == enum_json_type::J_INT) {
    const Json_int *num_distinct =
        down_cast<const Json_int *>(num_distinct_dom);
    if (num_distinct->value() < 1) {
      context->report_node(num_distinct_dom,
                           Message::JSON_INVALID_NUM_DISTINCT);
      return true;
    }
    num_distinct_v = num_distinct->value();
  } else {
    context->report_node(num_distinct_dom, Message::JSON_WRONG_ATTRIBUTE_TYPE);
    return true;
  }

  const Json_dom *lower_inclusive_dom = (*json_bucket)[0];
  const Json_dom *upper_inclusive_dom = (*json_bucket)[1];

  T upper_value;
  T lower_value;
  if (extract_json_dom_value(upper_inclusive_dom, &upper_value, context))
    return true;
  if (extract_json_dom_value(lower_inclusive_dom, &lower_value, context))
    return true;

  // Bucket-extraction post-check
  {
    // Check items in the bucket
    if ((cumulative_frequency->value() < 0.0) ||
        (cumulative_frequency->value() > 1.0)) {
      context->report_node(cumulative_frequency_dom,
                           Message::JSON_INVALID_FREQUENCY);
      return true;
    }
    if (context->check_value(&upper_value)) {
      context->report_node(upper_inclusive_dom,
                           Message::JSON_VALUE_OUT_OF_RANGE);
      return true;
    }
    if (context->check_value(&lower_value)) {
      context->report_node(lower_inclusive_dom,
                           Message::JSON_VALUE_OUT_OF_RANGE);
      return true;
    }

    // Check endpoint sequence and frequency sequence.
    if (histograms::Histogram_comparator()(upper_value, lower_value)) {
      context->report_node(lower_inclusive_dom,
                           Message::JSON_VALUE_DESCENDING_IN_BUCKET);
      return true;
    }
    if (!m_buckets.empty()) {
      equi_height::Bucket<T> *last_bucket = &m_buckets[m_buckets.size() - 1];
      if (!histograms::Histogram_comparator()(
              last_bucket->get_upper_inclusive(), lower_value)) {
        context->report_node(lower_inclusive_dom,
                             Message::JSON_VALUE_NOT_ASCENDING_2);
        return true;
      }
      if (last_bucket->get_cumulative_frequency() >=
          cumulative_frequency->value()) {
        context->report_node(cumulative_frequency_dom,
                             Message::JSON_CUMULATIVE_FREQUENCY_NOT_ASCENDING);
        return true;
      }
    }
  }
  equi_height::Bucket<T> bucket(lower_value, upper_value,
                                cumulative_frequency->value(), num_distinct_v);

  if (m_buckets.push_back(bucket)) return true;

  return false;
}

template <class T>
Histogram *Equi_height<T>::clone(MEM_ROOT *mem_root) const {
  DBUG_EXECUTE_IF("fail_histogram_clone", return nullptr;);
  bool error = false;
  Histogram *equi_height =
      new (mem_root) Equi_height<T>(mem_root, *this, &error);
  if (error) return nullptr;
  return equi_height;
}

/*
  This produces an estimate for the total number of distinct values by summing
  all the individual bucket estimates. A better estimate could perhaps be
  obtained by computing a single estimate for the entire histogram when it is
  built.
*/
template <class T>
size_t Equi_height<T>::get_num_distinct_values() const {
  size_t distinct_values = 0;
  for (const auto &bucket : m_buckets) {
    distinct_values += bucket.get_num_distinct();
  }
  return distinct_values;
}

template <class T>
double Equi_height<T>::get_equal_to_selectivity(const T &value) const {
  /*
    Find the first bucket where the upper inclusive value is not less than the
    provided value.
  */
  const auto found = std::lower_bound(m_buckets.begin(), m_buckets.end(), value,
                                      Histogram_comparator());

  // Check if we are after the last bucket
  if (found == m_buckets.end()) return 0.0;

  // Check if we are before the first bucket, or between two buckets.
  if (Histogram_comparator()(value, found->get_lower_inclusive())) return 0.0;

  double bucket_frequency;
  if (found == m_buckets.begin()) {
    /*
      If the value we are looking for is in the first bucket, we will end up
      here.
    */
    bucket_frequency = found->get_cumulative_frequency();
  } else {
    /*
      If the value we are looking for is NOT in the first bucket, we will end up
      here.
    */
    const auto previous = std::prev(found, 1);
    bucket_frequency = found->get_cumulative_frequency() -
                       previous->get_cumulative_frequency();

    assert(bucket_frequency >= 0.0);
    assert(bucket_frequency <= get_non_null_values_fraction());
  }

  return (bucket_frequency / found->get_num_distinct());
}

template <class T>
double Equi_height<T>::get_less_than_selectivity(const T &value) const {
  /*
    Find the first bucket with endpoints [a, b] where the upper inclusive value
    b is not less than the provided value, i.e. we have value <= b.
    Buckets that come before the found bucket (previous buckets) have an upper
    inclusive value strictly less than the provided value, and will therefore
    count towards the selectivity.
  */
  const auto found = std::lower_bound(m_buckets.begin(), m_buckets.end(), value,
                                      Histogram_comparator());

  if (found == m_buckets.end()) return get_non_null_values_fraction();

  double previous_bucket_cumulative_frequency;
  double found_bucket_frequency;
  if (found == m_buckets.begin()) {
    previous_bucket_cumulative_frequency = 0.0;
    found_bucket_frequency = found->get_cumulative_frequency();
  } else {
    const auto previous = std::prev(found, 1);
    previous_bucket_cumulative_frequency = previous->get_cumulative_frequency();
    found_bucket_frequency = found->get_cumulative_frequency() -
                             previous->get_cumulative_frequency();
  }

  /*
    We now consider how the found bucket contributes to the selectivity.
    There are two cases:

    1) a < value <= b
    The value lies inside the bucket and we know that the bucket is
    non-singleton since a < b. We include a fraction of the bucket's frequency
    corresponding to the position of the value between a and b.

    2) value <= a <= b
    In this case the found bucket contributes nothing since the lower inclusive
    endpoint a is greater than or equal to the value.
  */
  if (Histogram_comparator()(found->get_lower_inclusive(), value)) {
    const double distance = found->get_distance_from_lower(value);
    assert(distance >= 0.0);
    assert(distance <= 1.0);
    return previous_bucket_cumulative_frequency +
           (found_bucket_frequency * distance);
  } else {
    return previous_bucket_cumulative_frequency;
  }
}

template <class T>
double Equi_height<T>::get_greater_than_selectivity(const T &value) const {
  /*
    Find the first bucket with endpoints [a, b] where the upper inclusive value
    b is greater than the provided value, i.e. we have value < b.
    Buckets that come after the found bucket (next buckets) have a lower
    inclusive value greater than the provided value, and will therefore
    count towards the selectivity.
  */
  const auto found = std::upper_bound(m_buckets.begin(), m_buckets.end(), value,
                                      Histogram_comparator());

  if (found == m_buckets.end()) return 0.0;

  double found_bucket_frequency;
  if (found == m_buckets.begin()) {
    found_bucket_frequency = found->get_cumulative_frequency();
  } else {
    const auto previous = std::prev(found, 1);
    found_bucket_frequency = found->get_cumulative_frequency() -
                             previous->get_cumulative_frequency();
  }
  double next_buckets_frequency =
      get_non_null_values_fraction() - found->get_cumulative_frequency();

  /*
    We now consider how the found bucket contributes to the selectivity.
    There are two cases:

    1) value < a <= b
    The provided value is smaller than the inclusive lower endpoint and the
    entire bucket should be included.

    2) a <= value < b
    The value lies inside the bucket and we know that the bucket is
    non-singleton since a < b. We include a fraction of the bucket's frequency
    corresponding to the position of the value between a and b.
  */
  if (Histogram_comparator()(value, found->get_lower_inclusive())) {
    return found_bucket_frequency + next_buckets_frequency;
  } else {
    const double distance = found->get_distance_from_upper(value);
    assert(distance >= 0.0);
    assert(distance <= 1.0);
    return distance * found_bucket_frequency + next_buckets_frequency;
  }
}

// Explicit template instantiations.
template class Equi_height<double>;
template class Equi_height<String>;
template class Equi_height<ulonglong>;
template class Equi_height<longlong>;
template class Equi_height<MYSQL_TIME>;
template class Equi_height<my_decimal>;

}  // namespace histograms
